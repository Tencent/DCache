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
#include "ExpandThread.h"
#include "DCacheOptImp.h"

using namespace std;

TC_ThreadLock ExpandThread::_lock;

void ExpandThread::init(const string &sConf)
{
    _tcConf.parseFile(sConf);

    _checkInterval = TC_Common::strto<int>(_tcConf.get("/Main/Expand/<CheckInterval>", "10"));

    // relation db
    map<string, string> relationDBInfo = _tcConf.getDomainMap("/Main/DB/relation");
    TC_DBConf relationDbConf;
    relationDbConf.loadFromMap(relationDBInfo);
    _mysqlRelationDB.init(relationDbConf);

	_communicator = Application::getCommunicator();

	_adminRegPrx = _communicator->stringToProxy<AdminRegPrx>(_tcConf.get("/Main/<AdminRegObj>", "tars.tarsAdminRegistry.AdminRegObj"));
}

void ExpandThread::createThread()
{
    pthread_t thread;
    if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
    {
        throw runtime_error("create expand test thread failed");
    }
}

void* ExpandThread::Run(void* arg)
{
    pthread_detach(pthread_self());

    ExpandThread* pthis = (ExpandThread*) arg;
    time_t tLastCheck = 0;

    while (!pthis->_terminate)
    {
        try
        {
            time_t tNow = TC_TimeProvider::getInstance()->getNow();

            if (tNow - tLastCheck >= pthis->_checkInterval)
            {
                TC_ThreadLock::Lock lock(_lock);

                string sSql = "select * from t_expand_status where status=" + TC_Common::tostr(TRANSFERRING);
                TC_Mysql::MysqlData data = pthis->_mysqlRelationDB.queryRecord(sSql);
                if (data.size() > 0)
                {
                    for (size_t i = 0; i < data.size(); i++)
                    {
                        TC_DBConf routerDbInfo;
                        if (pthis->getRouterDBInfo(data[i]["app_name"], routerDbInfo) != 0)
                        {
                            TLOGERROR(FUN_LOG << "get router db info failed|app name:" << data[i]["app_name"] << endl);
                            continue;
                        }

                        bool finished = true;

                        TC_Mysql tcMysql;
                        tcMysql.init(routerDbInfo);
                        tcMysql.connect();

                        // router_transfer_id: 迁移任务 id
                        vector<string> tmp = TC_Common::sepstr<string>(data[i]["router_transfer_id"], "|");
                        for (size_t j = 0; j < tmp.size(); j++)
                        {
                            sSql = "select * from t_router_transfer where id=" + tmp[j];
                            TC_Mysql::MysqlData transferData = tcMysql.queryRecord(sSql);
                            if (transferData.size() == 0)
                            {
                               TLOGERROR(FUN_LOG << "not find record in t_router_transfer|sql:" << sSql << endl);
                               continue;
                            }

                            if (transferData[0]["state"] == "0" || transferData[0]["state"] == "1")
                            {
                                // 未开始或者迁移中
                               finished = false;
                            }
                            else if (transferData[0]["state"] == "2" && transferData[0]["remark"] != "e_Succ")
                            {
                                // status 2: 迁移结束，但是 迁移失败，则修改开始页为上一次的成功页的下一页，重置为 未迁移状态
                                map<string, pair<TC_Mysql::FT, string> > m_update;
                                m_update["state"]  = make_pair(TC_Mysql::DB_INT, "0");
                                m_update["remark"] = make_pair(TC_Mysql::DB_STR, "");

                                if (TC_Common::strto<int>(transferData[0]["transfered_page_no"]) != 0)
                                {
                                    if (TC_Common::strto<int>(transferData[0]["transfered_page_no"]) < TC_Common::strto<int>(transferData[0]["to_page_no"]))
                                    {
                                        m_update["from_page_no"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr<int>(TC_Common::strto<int>(transferData[0]["transfered_page_no"]) + 1));
                                    }
                                }

                                string condition = "where id=" + tmp[j];
                                tcMysql.updateRecord("t_router_transfer", m_update, condition);

                                map<string, int>::iterator it = pthis->_mFailCounts.find(transferData[0]["module_name"]);
                                if (it != pthis->_mFailCounts.end())
                                {
                                    it->second++;
                                    if (it->second > 10)
                                    {
                                        string error_str = "too much expand error|moduleName:" + it->first + "|info:" + transferData[0]["remark"];
                                        TLOGERROR(FUN_LOG << error_str << endl);
                                        TARS_NOTIFY_ERROR(error_str);

                                        it->second = 0;
                                    }
                                }
                                else
                                {
                                    pthis->_mFailCounts.insert(make_pair(transferData[0]["module_name"], 0));
                                }

                                finished = false;
                            }
                            else if (transferData[0]["remark"] == "e_Succ")
                            {
                                finished = true;
                            }
                            else if(transferData[0]["state"] == "3")
                            {
                               // state 3: 设置迁移页(新建迁移任务时，设置为 3)
                               finished = false;
                            }
                        }

                        if (finished)
                        {
                            //更新状态为完成
                            string condition = "where id=" + data[i]["id"];

                            map<string, pair<TC_Mysql::FT, string> > m_update;
                            m_update["status"]              = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(TRANSFER_FINISH)); // 扩容完成
                            m_update["router_transfer_id"]  = make_pair(TC_Mysql::DB_STR, "");

                            pthis->_mysqlRelationDB.updateRecord("t_expand_status", m_update, condition);

                            //整理路由
                            string routerObj;
                            string errmsg;
                            int iRet = pthis->getRouterObj(data[i]["app_name"], routerObj, errmsg);
                            if (iRet == 0)
                            {
                            	vector<string> tmp = TC_Common::sepstr<string>(routerObj, ".");
                            	pthis->defragRouterRecord("DCache", tmp[0] + "." + tmp[1], errmsg);
                            }
                        }
                    }
                }

                tLastCheck = TC_TimeProvider::getInstance()->getNow();
            }
            else
            {
                sleep(1);
            }

        }
        catch (const std::exception &ex)
        {
            TLOGERROR(FUN_LOG << "check expand status catch exception:" << ex.what() << endl);
            sleep(1);
        }
        catch (...)
        {
            TLOGERROR(FUN_LOG << "check expand status catch unkown exception" << endl);
            sleep(1);
        }
    }

    return NULL;
}

void ExpandThread::terminate()
{
    _terminate = true;
}

int ExpandThread::getRouterObj(const string & appName, string & routerObj, string & errmsg)
{
    try
    {
        errmsg = "";

        string sSql = "select * from t_cache_router where app_name='" + appName + "' limit 1";

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            routerObj = data[0]["router_name"];

            return 0;
        }
        else
        {
            errmsg = "not find router info in t_cache_router";
            TLOGERROR(FUN_LOG << errmsg << "|sql:" << sSql << endl);
        }
    }
    catch(exception &ex)
    {
        errmsg = string("get router obj from relation db t_cache_router table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = "get router obj from relation db t_cache_router table catch unknown exception";
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int ExpandThread::getRouterDBInfo(const string &appName, TC_DBConf &routerDbInfo)
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
        TLOGERROR(FUN_LOG << "not find router db config in relation db table t_cache_router|app name:" << appName << endl);
    }

    return -1;
}

int ExpandThread::defragRouterRecord(const string & app, const std::string & routerName, std::string & errmsg)
{
    try
    {
        if (app.empty() || routerName.empty())
        {
            errmsg = "defrag router record|app or router server name can not be empty";
			TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        RouterPrx routerPrx;
        routerPrx = _communicator->stringToProxy<RouterPrx>(routerName + string(".RouterObj"));
        vector<TC_Endpoint> vEndPoints = routerPrx->getEndpoint4All();

        set<string> nodeIPset;
		for (vector<TC_Endpoint>::size_type i = 0; i < vEndPoints.size(); i++)
		{
			//获取所有节点ip
			nodeIPset.insert(vEndPoints[i].getHost());
		}

        set<string>::iterator pos;
        for (pos = nodeIPset.begin(); pos != nodeIPset.end(); ++pos)
        {
        	//通知重载加载配置
            _adminRegPrx->notifyServer(app, routerName.substr(7), *pos, "router.defragRouteRecords all", errmsg);
        }

        return 0;
    }
    catch (const TarsException& ex)
    {
        errmsg = string("defrag router record all catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

