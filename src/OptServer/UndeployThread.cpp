/**
* Tencent is pleased to support the open source community by making DCache available.
* Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
* Licensed under the BSD 3-Clause License (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSD-3-Clause
*
* Unless required by applicable law or agreed to in writing, software distributed under
* the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
* either express or implied. See the License for the specific language governing permissions
* and limitations under the License.
*/
#include "UndeployThread.h"
#include "UninstallThread.h"
#include "DCacheOptServer.h"

extern DCacheOptServer g_app;

void UndeployThread::init(const string &sConf)
{
    _tcConf.parseFile(sConf);

    _checkInterval = TC_Common::strto<int>(_tcConf.get("/Main/Undeploy/<CheckInterval>", "3600"));

    // relation db
    map<string, string> relationDBInfo = _tcConf.getDomainMap("/Main/DB/relation");
    TC_DBConf relationDbConf;
    relationDbConf.loadFromMap(relationDBInfo);
    _mysqlRelationDB.init(relationDbConf);

    _communicator = Application::getCommunicator();

    _adminRegPrx = _communicator->stringToProxy<AdminRegPrx>(_tcConf.get("/Main/<AdminRegObj>", "tars.tarsAdminRegistry.AdminRegObj"));

}

void UndeployThread::run()
{
    // pthread_detach(pthread_self());

    time_t tLastCheck = 0;

    while (!_terminate)
    {
        // 查询迁移状态表
        time_t tNow = TC_TimeProvider::getInstance()->getNow();

        if (tLastCheck + _checkInterval < tNow)
        {
            doUndeploy(tLastCheck);

            tLastCheck = tNow;
        }

        {
            TC_ThreadLock::Lock lock(*this);
            timedWait(60 * 1000);
        }
    }
}

void UndeployThread::terminate()
{
    {
        TC_ThreadLock::Lock lock(*this);

        _terminate = true;

        notifyAll();
    }

    getThreadControl().join();

}

void UndeployThread::doUndeploy(time_t tLastCheck)
{
    try
    {
        string sSql = "select * from t_transfer_status where UNIX_TIMESTAMP(auto_updatetime) >= " + TC_Common::tostr(tLastCheck)
                    + " and status=" + TC_Common::tostr(DCache::TRANSFER_FINISH);

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            for (size_t i = 0; i < data.size(); ++i)
            {
                // 根据 appname,modulename,和组名查找服务名，检查是否还存在路由记录，如果不存在，则通知下线(增加判断条件，确保不会误下)
                TC_DBConf routerDbInfo;
                string errmsg("");
                if (getRouterDBInfo(data[i]["app_name"], routerDbInfo, errmsg) != 0)
                {
                   TLOGERROR(FUN_LOG << "get router db info falied|app name:" << data[i]["app_name"] << "|module name:" << data[i]["module_name"] << "|errmsg:" << errmsg << endl);
                   continue;
                }

                TC_Mysql tcMysql(routerDbInfo);
                tcMysql.connect();

                string sql = "select * from t_router_record where module_name='" + data[i]["module_name"] + "' and group_name='" + data[i]["src_group"] + "' limit 1";
                TC_Mysql::MysqlData routerRecordData = tcMysql.queryRecord(sql);
                if (routerRecordData.size() == 0)
                {
                    // 该组已经没有路由记录则可以下线

                    // 查询该组的全部服务名和ip
                    sSql = "select cache_name,cache_ip from t_cache_router where group_name='" + data[i]["src_group"] + "'";
                    TC_Mysql::MysqlData cacheData = _mysqlRelationDB.queryRecord(sSql);
                    if (cacheData.size() > 0)
                    {
                        do
                        {
                            bool bFailed = false;
                            for (size_t j = 0; j < cacheData.size(); ++j)
                            {
                                // 停止服务增加重试逻辑
                                int retryTimes = 3;
                                do
                                {
                                    try
                                    {
                                        string resultStr("");
                                        int iRet = _adminRegPrx->stopServer("DCache", cacheData[j]["cache_name"], cacheData[j]["cache_ip"], resultStr);
                                        if (iRet != 0)
                                        {
                                            TLOGERROR(FUN_LOG << "stop server failed|servername:" << cacheData[j]["cache_name"]
                                                              << "|server ip:" << cacheData[j]["cache_ip"]
                                                              << "|errmsg:" << resultStr
                                                              << endl);

                                            // 停止服务失败，则重试
                                        }
                                        else
                                        {
                                            // stop successfully
                                            break;
                                        }
                                    }
                                    catch (exception& e)
                                    {
                                        TLOGERROR(FUN_LOG << "notice admin stop server catch exception:" << e.what() << endl);
                                    }
                                    catch (...)
                                    {
                                        TLOGERROR(FUN_LOG << "notice admin stop server catch unkown exception" << endl);
                                    }

                                    --retryTimes;

                                } while (retryTimes);

                                if (retryTimes <= 0)
                                {
                                    // 停止服务失败，则不能下线
                                    bFailed = true;
                                    TLOGERROR(FUN_LOG << "stop server tried 3 times and all failed|servername:" << cacheData[j]["cache_name"]
                                                      << "|server ip:" << cacheData[j]["cache_ip"] << endl);
                                    break;
                                }

                                // 等待服务停止完成
                                sleep(3);
                            }

                            // 有服务停止失败，则不进行下线
                            if (bFailed)
                            {
                                break;
                            }

                            // 构造下线服务请求
                            UninstallRequest request;
                            request.info.unType     = DCache::GROUP; // 按组下线
                            request.info.appName    = data[i]["app_name"];
                            request.info.moduleName = data[i]["module_name"];
                            request.info.groupName  = data[i]["src_group"];
                            request.requestId       = data[i]["src_group"];

                            TLOGDEBUG(FUN_LOG << "for transfer|uninstall the group which the router record is zero|app name:" << data[i]["app_name"]
                                              << "|module name:" << data[i]["module_name"]
                                              << "|group name:" << data[i]["src_group"]
                                              << endl);

                            g_app.uninstallRequestQueueManager()->addUninstallRecord(request.requestId);
                            g_app.uninstallRequestQueueManager()->push_back(request);

                        } while (0);
                    }
                }
            }
        }

        // 查询 t_expand_status(缩容: type=2)，然后下线路由页为0的组
        sSql = "select * from t_expand_status where UNIX_TIMESTAMP(auto_updatetime) >= " + TC_Common::tostr(tLastCheck)
             + " and type=" + TC_Common::tostr(DCache::REDUCE_TYPE)
             + " and status=" + TC_Common::tostr(DCache::TRANSFER_FINISH);

        data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            for (size_t i = 0; i < data.size(); ++i)
            {
                TC_DBConf routerDbInfo;
                string errmsg("");
                if (getRouterDBInfo(data[i]["app_name"], routerDbInfo, errmsg) != 0)
                {
                   TLOGERROR(FUN_LOG << "get router db info falied|app name:" << data[i]["app_name"] << "|module name:" << data[i]["module_name"] << endl);
                   continue;
                }

                TC_Mysql tcMysql(routerDbInfo);
                tcMysql.connect();

                vector<string> srcGroupName = TC_Common::sepstr<string>(data[i]["modify_group_name"], "|");
                for (size_t ii = 0; ii < srcGroupName.size(); ++ii)
                {
                    string sql = "select * from t_router_record where module_name='" + data[i]["module_name"] + "' and group_name='" + srcGroupName[ii] + "' limit 1";

                    TC_Mysql::MysqlData routerRecordData = tcMysql.queryRecord(sql);
                    if (routerRecordData.size() == 0)
                    {
                        // 该组已经没有路由记录则可以下线

                        // 查询该组的全部服务名和ip
                        sSql = "select cache_name,cache_ip from t_cache_router where group_name='" + srcGroupName[ii] + "'";

                        TC_Mysql::MysqlData cacheData = _mysqlRelationDB.queryRecord(sSql);
                        if (cacheData.size() > 0)
                        {
                            do
                            {
                                bool bFailed = false;
                                for (size_t j = 0; j < cacheData.size(); ++j)
                                {
                                    // 停止服务增加重试逻辑
                                    int retryTimes = 3;
                                    do
                                    {
                                        try
                                        {
                                            string resultStr("");
                                            int iRet = _adminRegPrx->stopServer("DCache", cacheData[j]["cache_name"], cacheData[j]["cache_ip"], resultStr);
                                            if (iRet != 0)
                                            {
                                                TLOGERROR(FUN_LOG << "stop server failed|servername:" << cacheData[j]["cache_name"]
                                                                  << "|server ip:" << cacheData[j]["cache_ip"]
                                                                  << "|errmsg:" << resultStr
                                                                  << endl);

                                                // 停止服务失败，则重试
                                            }
                                            else
                                            {
                                                break;
                                            }
                                        }
                                        catch (exception& e)
                                        {
                                            TLOGERROR(FUN_LOG << "notice admin stop server catch exception:" << e.what() << endl);
                                        }
                                        catch (...)
                                        {
                                            TLOGERROR(FUN_LOG << "notice admin stop server catch unkown exception" << endl);
                                        }

                                        --retryTimes;

                                    } while (retryTimes);

                                    if (retryTimes <= 0)
                                    {
                                        // 停止服务失败，则不能下线
                                        bFailed = true;
                                        TLOGERROR(FUN_LOG << "stop server tried 3 times and all failed|servername:" << cacheData[j]["cache_name"]
                                                          << "|server ip:" << cacheData[j]["cache_ip"] << endl);
                                        break;
                                    }

                                    // 等待服务停止完成
                                    sleep(3);
                                }

                                // 有服务停止失败，则不进行下线
                                if (bFailed)
                                {
                                    break;
                                }

                                // 构造下线服务请求
                                UninstallRequest request;
                                request.info.unType     = DCache::GROUP; // 按组下线
                                request.info.appName    = data[i]["app_name"];
                                request.info.moduleName = data[i]["module_name"];
                                request.info.groupName  = srcGroupName[ii];
                                request.requestId       = srcGroupName[ii];

                                TLOGDEBUG(FUN_LOG << "for reduce|uninstall the group which the router record is zero|app name:" << data[i]["app_name"]
                                                  << "|module name:" << data[i]["module_name"]
                                                  << "|group name:" << srcGroupName[ii]
                                                  << endl);

                                g_app.uninstallRequestQueueManager()->addUninstallRecord(request.requestId);
                                g_app.uninstallRequestQueueManager()->push_back(request);

                            } while (0);
                        }
                    }
                }
            }
        }
    }
    catch (exception& e)
    {
        TLOGERROR(FUN_LOG << "catch exception:" << e.what() << endl);
    }
    catch (...)
    {
        TLOGERROR(FUN_LOG << "catch unkown exception" << endl);
    }
}

int UndeployThread::getRouterDBInfo(const string &appName, TC_DBConf &routerDbInfo, string& errmsg)
{
    try
    {
        string sSql("");
        sSql = "select * from t_cache_router where app_name='" + appName + "'";

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            routerDbInfo._host      = data[0]["db_ip"];
            routerDbInfo._database  = data[0]["db_name"];
            routerDbInfo._port      = TC_Common::strto<int>(data[0]["db_port"]);
            routerDbInfo._user      = data[0]["db_user"];
            routerDbInfo._password  = data[0]["db_pwd"];
            routerDbInfo._charset   = data[0]["db_charset"];

            return 0;
        }
        else
        {
            errmsg = string("not find router db config in relation db table t_cache_router|app name:") + appName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch(exception &ex)
    {
        errmsg = string("get router db info from relation db t_cache_router table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = "get router db info from relation db t_cache_router table catch unknown exception";
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}


