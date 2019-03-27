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
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <set>

#include "servant/Application.h"
#include "util/tc_common.h"
#include "util/tc_md5.h"

#include "Proxy.h"
#include "Router.h"
#include "DCacheOptImp.h"
#include "DCacheOptServer.h"
#include "Assistance.h"
#include "ReleaseThread.h"
#include "UninstallThread.h"

using namespace std;

#define MAX_ROUTER_PAGE_NUM 429496

extern DCacheOptServer g_app;

inline static int formatCurrentTime(string & formatTime)
{
    // 将当前时间格式化为 YYYY-MM-DD HH:MM:SS
    char buffer[64];
    ::bzero(buffer , sizeof(buffer));
    time_t t = ::time(NULL);
    struct tm result;
    void * ret = ::localtime_r(&t, &result);
    if (NULL == ret)
    {
        TLOGERROR(FUN_LOG << "Get localtime_r error" << endl);
        return -1;
    }
    else
    {
        int real_size = ::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", result.tm_year + 1900, result.tm_mon + 1, result.tm_mday, result.tm_hour, result.tm_min, result.tm_sec);
        formatTime.assign(buffer, real_size);
    }

    return 0;
}

void DCacheOptImp::initialize()
{
    // DCacheOpt initialize
    _tcConf.parseFile(ServerConfig::BasePath + "DCacheOptServer.conf");

    //tars db: save router,proxy,cache server info, include: server conf, adapter conf
    map<string, string> tarsConnInfo = _tcConf.getDomainMap("/Main/DB/tars");
    TC_DBConf tarsDbConf;
    tarsDbConf.loadFromMap(tarsConnInfo);
    _mysqlTarsDb.init(tarsDbConf);

    //relation db: save app-router-proxy-cache relation info
    _relationDBInfo = _tcConf.getDomainMap("/Main/DB/relation");
    TC_DBConf relationDbConf;
    relationDbConf.loadFromMap(_relationDBInfo);
    _mysqlRelationDB.init(relationDbConf);

    /*map<string, string> routerConnInfo = _tcConf.getDomainMap("/Main/RouterConfDb");
    TC_DBConf routerConfDb;
    routerConfDb.loadFromMap(routerConnInfo);
    _mysqlRouterConfDb.init(routerConfDb);*/

    map<string, string> transferExpandDb = _tcConf.getDomainMap("/Main/DB/transferAndExpand");
    TC_DBConf transferExpandDbConf;
    transferExpandDbConf.loadFromMap(transferExpandDb);
    _mysqlTransferExpandDb.init(transferExpandDbConf);

    _communicator = Application::getCommunicator();

    _adminproxy = _communicator->stringToProxy<AdminRegPrx>(_tcConf.get("/Main/<AdminRegObj>", "tars.tarsAdminRegistry.AdminRegObj"));

    _routerDbUser = _tcConf.get("/Main/CreateRouterDb<dbuser>", "dcache");
    _routerDbPwd  = _tcConf.get("/Main/CreateRouterDb<dbpass>", "dcache");

    TLOGDEBUG(FUN_LOG << "initialize succ" << endl);
}

//////////////////////////////////////////////////////
void DCacheOptImp::destroy()
{
    _mysqlTarsDb.disconnect();
    TLOGDEBUG("DCacheOptImp::destroyApp ok" << endl);
}

/*
*  interface function
*/
tars::Int32 DCacheOptImp::installApp(const InstallAppReq & installReq, InstallAppRsp & instalRsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG("begin installApp and app_name=" << installReq.appName << endl);

    try
    {
        const RouterParam & stRouter = installReq.routerParam;
        const ProxyParam & stProxy = installReq.proxyParam;
        string & err = instalRsp.errMsg;

        //创建路由数据库
        if (createRouterDBConf(stRouter, err) != 0)
        {
            err = string("failed to create router DB, errmsg:") + err;
            return -1;
        }

        string::size_type posDot = stRouter.serverName.find_first_of(".");
        if (string::npos == posDot)
        {
            err = "router server name error, eg: DCache.AbcRouterServer";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        //创建router server的配置
        if (createRouterConf(stRouter, false, installReq.replace, err) != 0)
        {
            err = "failed to create router conf";
            return -1;
        }

        posDot = stProxy.serverName.find_first_of(".");
        if (string::npos == posDot)
        {
            err = "proxy server name error, eg: DCache.AbcProxyServer";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        bool bProxyServerExist = hasServer("DCache", stProxy.serverName.substr(posDot + 1));
        //创建proxy的配置 模块名填'';
        if (!bProxyServerExist && createProxyConf("", stProxy, stRouter, installReq.replace, err) != 0)
        {
            err = string("failed to create proxy conf, errmsg:") + err;
            return -1;
        }

        // 这里enableGroup 默认 为true, 采用分组
        if (insertTarsDb(stProxy, stRouter, installReq.version, true, installReq.replace, err) !=0)
        {
            err = string("failed to insert tars server, errmsg:") + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 保存 proxy-app 和 router-app的对应关系
        insertRelationAppTable(installReq.appName, stProxy.serverName, stRouter.serverName, stRouter, installReq.replace, err);

        //把三者关系生成放到最后面 保证配额查询接口的准确性
        try
        {
            RouterDbInfo tmpRouterDbInfo;
            tmpRouterDbInfo.sDbName     = stRouter.dbName;
            tmpRouterDbInfo.sDbIp       = stRouter.dbIp;
            tmpRouterDbInfo.sPort       = stRouter.dbPort;
            tmpRouterDbInfo.sUserName   = stRouter.dbUser;
            tmpRouterDbInfo.sPwd        = stRouter.dbPwd;
            tmpRouterDbInfo.sCharSet    = "utf8";

            insertProxyRouter(stProxy.serverName.substr(7), stRouter.serverName + ".RouterObj", tmpRouterDbInfo.sDbName, tmpRouterDbInfo.sDbIp, "", installReq.replace, err);
        }
        catch (exception &ex)
        {
            //这里捕捉异常以不影响安装结果
            err = string("insert relation info catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << err << endl);

            return 0;
        }

        TLOGDEBUG(FUN_LOG << "install finish" << endl);
    }
    catch (const std::exception &ex)
    {
        instalRsp.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch execption:") + ex.what();
        TLOGERROR(FUN_LOG << instalRsp.errMsg << endl);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::installKVCacheModule(const InstallKVCacheReq & kvCacheReq, InstallKVCacheRsp & kvCacheRsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name: " << kvCacheReq.appName << "|module name: " << kvCacheReq.moduleName << endl);

    try
    {
        const vector<DCache::CacheHostParam> & vtCacheHost = kvCacheReq.kvCacheHost;
        const SingleKeyConfParam & stSingleKeyConf = kvCacheReq.kvCacheConf;
        const string & sTarsVersion = kvCacheReq.version;

        RouterConsistRes &rcRes = kvCacheRsp.rcRes;
        std::string &err = kvCacheRsp.errMsg;

        //初始化out变量
        rcRes.iFlag = 0;
        rcRes.sInfo = "success";
        err = "success";

        //安装前先检查路由数据库与内存是否一致
        RouterDbInfo routerDbInfo;
        string info;
        vector<string> vsProxyName;
        string strRouterName;

        int iRet = getRouterDBFromAppTable(kvCacheReq.appName, routerDbInfo, vsProxyName, strRouterName, info);
        if (-1 == iRet)
        {
            err = info;
            return -1;
        }

        rcRes.iFlag = iRet;
        rcRes.sInfo = info;

        //生成cache router关系,防止多次调用时无关系可查
        if (vtCacheHost.size() == 0 || vtCacheHost[0].serverName.empty())
        {
            err = "install cache server, but cache server name is empty";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        if (createKVCacheConf(kvCacheReq.appName, kvCacheReq.moduleName, strRouterName, vtCacheHost, stSingleKeyConf, kvCacheReq.replace, err) != 0)
        {
            err = "failed to create kvcache server conf";
            return -1;
        }

        if (insertCache2RouterDb(kvCacheReq.moduleName, "", routerDbInfo.sDbIp, routerDbInfo.sDbName, routerDbInfo.sPort, routerDbInfo.sUserName, routerDbInfo.sPwd, vtCacheHost, kvCacheReq.replace, err) != 0)
        {
            if (err.empty())
            {
                err = "failed to insert module and cache info to router db";
            }
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 通知router加载新模块的信息
        reloadRouterConfByModuleFromDB("DCache", kvCacheReq.moduleName, strRouterName, err, current);

        if (!checkRouterLoadModule("DCache", kvCacheReq.moduleName, strRouterName, err))
        {
            err = "router check routing error, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 保存cache信息到tars db中
        iRet = insertCache2TarsDb(routerDbInfo.sDbName, routerDbInfo.sDbIp, routerDbInfo.sPort, routerDbInfo.sUserName, routerDbInfo.sPwd, vtCacheHost, DCache::KVCACHE, sTarsVersion, kvCacheReq.replace, err);
        if (iRet != 0)
        {
            err = "failed to insert catch info to tars db, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        //把三者关系生成放到最后面 保证配额查询接口的准确性
        try
        {
            RouterDbInfo tmpRouterDbInfo;
            tmpRouterDbInfo = routerDbInfo;
            for (size_t i = 0; i < vtCacheHost.size(); ++i)
            {
                for (size_t j = 0; j < vsProxyName.size(); j++)
                {
                    // 保存 proxy server和cache server的对应关系
                    insertProxyCache(vsProxyName[j].substr(7), vtCacheHost[i].serverName, kvCacheReq.replace);
                }

                // 保存cache serve和router server的对应关系
                insertCacheRouter(vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, int(TC_Common::strto<float>(vtCacheHost[i].shmSize)*1024), strRouterName + ".RouterObj", tmpRouterDbInfo, kvCacheReq.moduleName, kvCacheReq.replace, err);
            }
        }
        catch(exception &ex)
        {
            //这里捕捉异常以不影响安装结果
            err = string("option relation db catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << err << endl);
            return 0;
        }

        TLOGDEBUG(FUN_LOG << "finish" << endl);
    }
    catch(const std::exception &ex)
    {
        kvCacheRsp.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << kvCacheRsp.errMsg << endl);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::installMKVCacheModule(const InstallMKVCacheReq & mkvCacheReq, InstallMKVCacheRsp & mkvCacheRsp, tars::TarsCurrentPtr current)
{
    try
    {
        const vector<DCache::CacheHostParam> & vtCacheHost = mkvCacheReq.mkvCacheHost;
        const MultiKeyConfParam & stMultiKeyConf = mkvCacheReq.mkvCacheConf;
        const vector<DCache::RecordParam> & vtRecord = mkvCacheReq.fieldParam;
        const string & sTarsVersion = mkvCacheReq.version;

        RouterConsistRes &rcRes = mkvCacheRsp.rcRes;
        std::string &err = mkvCacheRsp.errMsg;

        //初始化out变量
        rcRes.iFlag = 0;
        rcRes.sInfo = "success";
        err = "success";

        //安装前先检查路由数据库与内存是否一致
        RouterDbInfo routerDbInfo;
        string info;
        vector<string> vtProxyName;
        string strRouterName;
        int iRet = getRouterDBFromAppTable(mkvCacheReq.appName, routerDbInfo, vtProxyName, strRouterName, info);
        if (-1 == iRet)
        {
            err = info;
            return -1;
        }

        rcRes.iFlag = iRet;
        rcRes.sInfo = info;

        //生成cache router关系,防止多次调用时无关系可查
        if (vtCacheHost.size() == 0 || vtCacheHost[0].serverName.empty())
        {
            err = "cache server is empty";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        if (createMKVCacheConf(mkvCacheReq.appName, mkvCacheReq.moduleName, strRouterName, vtCacheHost, stMultiKeyConf, vtRecord, mkvCacheReq.replace, err) != 0)
        {
            err = "failed to create mkvcache server conf";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        if (insertCache2RouterDb(mkvCacheReq.moduleName, "", routerDbInfo.sDbIp, routerDbInfo.sDbName, routerDbInfo.sPort, routerDbInfo.sUserName, routerDbInfo.sPwd, vtCacheHost, mkvCacheReq.replace, err) != 0)
        {
            if (err.empty())
            {
                err = "failed to insert module and cache info to router db";
            }
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 通知router加载新模块的信息
        reloadRouterConfByModuleFromDB("DCache", mkvCacheReq.moduleName, strRouterName, err, current);

        if (!checkRouterLoadModule("DCache", mkvCacheReq.moduleName, strRouterName, err))
        {
            err = "router check routing error, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 保存cache信息到tarsDB中
        iRet = insertCache2TarsDb(routerDbInfo.sDbName, routerDbInfo.sDbIp, routerDbInfo.sPort, routerDbInfo.sUserName, routerDbInfo.sPwd, vtCacheHost, MKVCACHE, sTarsVersion, mkvCacheReq.replace, err);
        if (iRet != 0)
        {
            err = "failed to insert catch info to tars db, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        //把三者关系生成放到最后面 保证配额查询接口的准确性
        try
        {
            RouterDbInfo tmpRouterDbInfo;
            tmpRouterDbInfo = routerDbInfo;
            for (size_t i = 0; i < vtCacheHost.size(); ++i)
            {
                for (unsigned int j = 0; j < vtProxyName.size(); j++)
                {
                    // 保存 proxy server和cache server的对应关系
                    insertProxyCache(vtProxyName[j].substr(7), vtCacheHost[i].serverName, mkvCacheReq.replace);
                }

                // 保存cache serve和router server的对应关系
                insertCacheRouter(vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, int(TC_Common::strto<float>(vtCacheHost[i].shmSize) * 1024), strRouterName + ".RouterObj", tmpRouterDbInfo, mkvCacheReq.moduleName, mkvCacheReq.replace, err);
            }
        }
        catch(exception &ex)
        {
            //这里捕捉异常以不影响安装结果
            err = string("option relation db catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << err << endl);
            return 0;
        }

        TLOGDEBUG(FUN_LOG << "finish" << endl);
    }
    catch(const std::exception &ex)
    {
        mkvCacheRsp.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << mkvCacheRsp.errMsg << endl);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::releaseServer(const vector<DCache::ReleaseInfo> & releaseInfo, ReleaseRsp & releaseRsp, tars::TarsCurrentPtr current)
{
    try
    {
        ReleaseRequest* request = new ReleaseRequest();
        request->requestId      = g_app.getReleaseID();
        request->info           = releaseInfo;
        releaseRsp.releaseId    = request->requestId;
        g_app.releaseRequestQueueManager()->push_back(request);

        releaseRsp.errMsg = "sucess to release server";

        TLOGDEBUG(FUN_LOG << "sucess to releaseServer" << "|release id:" << request->requestId << endl);

        return 0;
    }
    catch (exception &e)
    {
        releaseRsp.errMsg = string("release server catch exception:") + e.what();
    }
    catch (...)
    {
        releaseRsp.errMsg = "release server unkown exception";
    }
    TLOGERROR(FUN_LOG << releaseRsp.errMsg << endl);

    return -1;
}

tars::Int32 DCacheOptImp::getReleaseProgress(const ReleaseProgressReq & progressReq, ReleaseProgressRsp & progressRsp, tars::TarsCurrentPtr current)
{
    try
    {
        vector<DCache::ReleaseInfo> &vtReleaseInfo = progressRsp.releaseInfo;

        ReleaseStatus releaseStatus = g_app.releaseRequestQueueManager()->getProgressRecord(progressReq.releaseId, vtReleaseInfo);
        TLOGDEBUG(FUN_LOG << "release id:" << progressReq.releaseId << "|release status:" << releaseStatus.status << "|release percent:" << releaseStatus.percent << endl);

        if (releaseStatus.status != REERROR)
        {
            progressRsp.releaseId = progressReq.releaseId;
            progressRsp.percent = releaseStatus.percent;
            if (releaseStatus.status == REFINISH && "100" == progressRsp.percent)
            {
                TLOGDEBUG(FUN_LOG << "release finish, delete release progress record id:" << progressReq.releaseId << endl);
                g_app.releaseRequestQueueManager()->deleteProgressRecord(progressReq.releaseId);
            }

            return 0;
        }
        else
        {
            progressRsp.releaseId = progressReq.releaseId;
            progressRsp.errMsg = releaseStatus.error;
            TLOGERROR(FUN_LOG << "release error:" << progressRsp.errMsg << "|release id:" << progressReq.releaseId << endl);
            g_app.releaseRequestQueueManager()->deleteProgressRecord(progressReq.releaseId);
        }
    }
    catch(exception &ex)
    {
        progressRsp.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << progressRsp.errMsg << endl);
    }
    catch (...)
    {
        progressRsp.errMsg = TC_Common::tostr(__FUNCTION__) + " catch unknown exception";
        TLOGERROR(FUN_LOG << progressRsp.errMsg << endl);
    }

    return -1;
}

tars::Int32 DCacheOptImp::uninstall4DCache(const UninstallInfo & uninstallInfo, UninstallRsp & uninstallRsp, tars::TarsCurrentPtr current)
{
    try
    {
        UninstallRequest request;
        request.info = uninstallInfo;

        if (DCache::MODULE == uninstallInfo.unType)
        {
            TC_Mysql mysqlRouterDb(uninstallInfo.routerDb.ip, uninstallInfo.routerDb.user, uninstallInfo.routerDb.pwd, uninstallInfo.routerDb.dbName, uninstallInfo.routerDb.charset, TC_Common::strto<int>(uninstallInfo.routerDb.port));
            //按模块
            string sWhere = "where module_name='" + uninstallInfo.moduleName + "'";
            mysqlRouterDb.deleteRecord("t_router_module", sWhere);
            mysqlRouterDb.deleteRecord("t_router_record", sWhere);

            request.requestId =  uninstallInfo.moduleName;
            TLOGDEBUG(FUN_LOG << "uninstall by module|module name:" << uninstallInfo.moduleName << endl);
        }
        else if (DCache::GROUP ==  uninstallInfo.unType)
        {
            //按组
            request.requestId = uninstallInfo.groupName;
            TLOGDEBUG(FUN_LOG << "uninstall by group|group name:" << uninstallInfo.groupName << endl);
        }
        else if (DCache::SERVER == uninstallInfo.unType)
        {
            //按服务
            request.requestId = uninstallInfo.serverName;
            TLOGDEBUG(FUN_LOG << "uninstall in cache|cache server name:" << uninstallInfo.serverName << endl);
        }
        else if (DCache::QUOTA_SERVER == uninstallInfo.unType)
        {
            //按服务
            request.requestId = uninstallInfo.serverName;
            TLOGDEBUG(FUN_LOG << "uninstall by cache server|cache server name:" << uninstallInfo.serverName << endl);
        }
        else
        {
            throw DCacheOptException("uninstall type invalid: " + TC_Common::tostr(uninstallInfo.unType) + ", should be 0(server), 1(group), 2(module)");
            TLOGERROR(FUN_LOG << "uninstall type invalid:" << uninstallInfo.unType << ", should be 0(server), 1(group), 2(module)" << endl);
        }

        g_app.uninstallRequestQueueManager()->addUninstallRecord(request.requestId);
        g_app.uninstallRequestQueueManager()->push_back(request);

        TLOGDEBUG(FUN_LOG << "sucess to uninstall cache|request id:" << request.requestId << "|uninstall type:" << uninstallInfo.unType << endl);

        return 0;
    }
    catch (exception &e)
    {
        uninstallRsp.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch exception:") + e.what();
    }
    catch (...)
    {
        uninstallRsp.errMsg = TC_Common::tostr(__FUNCTION__) + " catch unkown exception";
    }

    TLOGERROR(FUN_LOG << uninstallRsp.errMsg << endl);

    return -1;
}

/*tars::Int32 DCacheOptImp::uninstall4DCache(const DCacheUninstallInfo & uninstallInfo, std::string &sError, tars::TarsCurrentPtr current)
{
    try
    {
        UninstallRequest request;
        request.info = uninstallInfo;

        if (2 == uninstallInfo.unType)
        {
            TC_Mysql routeDb(uninstallInfo.sRouteDbIP, uninstallInfo.sRouteDbUserName, uninstallInfo.sRouterDbPwd, uninstallInfo.sRouteDbName, uninstallInfo.sRouteDbCharset, TC_Common::strto<int>(uninstallInfo.sRouterDbPort));
            //按模块
            string sWhere = "where module_name='" + uninstallInfo.sModuleName + "'";
            routeDb.deleteRecord("t_router_module", sWhere);
            routeDb.deleteRecord("t_router_record", sWhere);

            request.requestId =  uninstallInfo.sModuleName;
            TLOGDEBUG(FUN_LOG << "uninstall in module|module name:" << uninstallInfo.sModuleName << endl);
        }
        else if (1 ==  uninstallInfo.unType)
        {
            //按组
            request.requestId = uninstallInfo.sCacheGroupName;
            TLOGDEBUG(FUN_LOG << "uninstall in group|group name:" << uninstallInfo.sCacheGroupName << endl);
        }
        else if (0 == uninstallInfo.unType)
        {
            //按服务
            request.requestId = uninstallInfo.sCacheServerName;
            TLOGDEBUG(FUN_LOG << "uninstall in cache|cache server name:" << uninstallInfo.sCacheServerName << endl);
        }
        else if (3 == uninstallInfo.unType)
        {
            //按服务
            request.requestId = uninstallInfo.sCacheServerName;
            TLOGDEBUG(FUN_LOG << "uninstall in cache|cache server name:" << uninstallInfo.sCacheServerName << endl);
        }
        else
        {
            throw DCacheOptException("uninstall type invalid: "+ TC_Common::tostr(uninstallInfo.unType) +", should be 0(server), 1(group), 2(module)");
            TLOGERROR(FUN_LOG << "uninstall type invalid:" << uninstallInfo.unType << ", should be 0(server), 1(group), 2(module)" << endl);
        }

        g_app.uninstallRequestQueueManager()->addUninstallRecord(request.requestId);
        g_app.uninstallRequestQueueManager()->push_back(request);

        TLOGDEBUG(FUN_LOG << "sucess to uninstall cache|request id:" << request.requestId << "|uninstall type:" << uninstallInfo.unType << endl);

        return 0;
    }
    catch(exception &e)
    {
        sError = TC_Common::tostr(__FUNCTION__) + string("catch exception:") + e.what();
    }
    catch (...)
    {
        sError = TC_Common::tostr(__FUNCTION__) + "catch unkown exception";
    }

    TLOGERROR(FUN_LOG << sError << endl);

    return -1;
}*/

tars::Int32 DCacheOptImp::getUninstallPercent(const DCache::UninstallInfo & uninstallInfo, UninstallProgressRsp & progressRsp, tars::TarsCurrentPtr current)
{
    string sRequestId("");
    progressRsp.errMsg = "";

    try
    {
        if (DCache::MODULE == uninstallInfo.unType)
        {
            sRequestId = uninstallInfo.moduleName;
        }
        else if (DCache::GROUP == uninstallInfo.unType)
        {
            sRequestId = uninstallInfo.groupName;
        }
        else if (DCache::SERVER == uninstallInfo.unType || DCache::QUOTA_SERVER == uninstallInfo.unType)
        {
            sRequestId = uninstallInfo.serverName;
        }

        UninstallStatus currentStatus = g_app.uninstallRequestQueueManager()->getUninstallRecord(sRequestId);
        TLOGDEBUG(FUN_LOG << "uninstall status:" << currentStatus.status << "|uninstall percent" << currentStatus.sPercent << "|request id:" << sRequestId << endl);

        if (currentStatus.status != UNERROR)
        {
            progressRsp.percent = currentStatus.sPercent;
            if (currentStatus.status == FINISH && "100%" == progressRsp.percent)
            {
                g_app.uninstallRequestQueueManager()->deleteUninstallRecord(sRequestId);
            }

            return 0;
        }
        else
        {
            progressRsp.errMsg = currentStatus.sError;
            TLOGDEBUG(FUN_LOG << "uninstall error:" << progressRsp.errMsg << "|request id:" << sRequestId << endl);
            g_app.uninstallRequestQueueManager()->deleteUninstallRecord(sRequestId);
        }
    }
    catch(exception &ex)
    {
        progressRsp.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << progressRsp.errMsg << endl);
    }
    catch (...)
    {
        progressRsp.errMsg = TC_Common::tostr(__FUNCTION__) + " catch unknown exception";
        TLOGERROR(FUN_LOG << progressRsp.errMsg << endl);
    }

    return -1;
}

tars::Int32 DCacheOptImp::transferDCache(const TransferReq & transferReq, TransferRsp & transferRsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "appName:"<< transferReq.appName << "|module:"<< transferReq.moduleName<< "|srcGroup:"<< transferReq.srcGroupName << endl);

    string &err = transferRsp.errMsg;
    try
    {
        //往已存在服务迁移
        if (transferReq.hasDestServer)
        {
            string sSql = "select * from t_transferv2_status where module='" + transferReq.moduleName + "' and src_group='" + transferReq.srcGroupName + "' and appName='" + transferReq.appName + "' and dest_group='" + transferReq.cacheHost[0].groupName + "'";
            TC_Mysql::MysqlData data = _mysqlTransferExpandDb.queryRecord(sSql);
            if (data.size() == 0)
            {
                map<string, pair<TC_Mysql::FT, string> > m;

                m["module"]     = make_pair(TC_Mysql::DB_STR, transferReq.moduleName);
                m["appName"]    = make_pair(TC_Mysql::DB_STR, transferReq.appName);
                m["src_group"]  = make_pair(TC_Mysql::DB_STR, transferReq.srcGroupName);
                m["dest_group"] = make_pair(TC_Mysql::DB_STR, transferReq.cacheHost[0].groupName);
                m["status"]     = make_pair(TC_Mysql::DB_INT, "1");  // 1-transfering

                string startTime;
                int iRet = formatCurrentTime(startTime);
                if (iRet == 0)
                {
                    m["transfer_start_time"] = make_pair(TC_Mysql::DB_STR, startTime);
                }

                _mysqlTransferExpandDb.insertRecord("t_transferv2_status", m);
            }
            else if (data[0]["status"] == "4")
            {
                // status:4-done 完成, 以前有成功记录，就覆盖
                map<string, pair<TC_Mysql::FT, string> > m;
                m["status"] = make_pair(TC_Mysql::DB_INT, "1");

                string startTime;
                int iRet = formatCurrentTime(startTime);
                if (iRet == 0)
                {
                    m["transfer_start_time"] = make_pair(TC_Mysql::DB_STR, startTime);
                }

                string cond = "where module='" + transferReq.moduleName + "' and src_group='" + transferReq.srcGroupName + "' and appName='" + transferReq.appName + "' and dest_group='" + transferReq.cacheHost[0].groupName + "'";
                _mysqlTransferExpandDb.updateRecord("t_transferv2_status", m, cond);
            }

            TLOGDEBUG(FUN_LOG << "configure transfer succ for existing dest server, app name:" << transferReq.appName << "|module name:" << transferReq.moduleName << "|src group name:" << transferReq.srcGroupName << endl);
            return 0;
        }
        else
        {
            //往新部署服务迁移
            string sSql = "select * from t_transferv2_status where module='" + transferReq.moduleName + "' and src_group='" + transferReq.srcGroupName + "' and appName='" + transferReq.appName + "' and dest_group='" + transferReq.cacheHost[0].groupName + "'";
            TC_Mysql::MysqlData data = _mysqlTransferExpandDb.queryRecord(sSql);
            if ((data.size() > 0) && (data[0]["status"] != "4"))
            {
                //此阶段的配置已成功
                if (data[0]["status"] != "0") // status: 0 配置失败
                {
                    TLOGDEBUG(FUN_LOG << "configure transfer succ, app name:" << transferReq.appName << "|module name:" << transferReq.moduleName << "|srcGroupName: " << transferReq.srcGroupName << endl);
                    return 0;
                }
                else if (data[0]["status"] == "0")
                {
                    //上次在此阶段失败 需要清除下 再发起迁移
                    err = "configure transfer failed need clean, app name:" + transferReq.appName + "|module name:" + transferReq.moduleName + "|srcGroupName:" + transferReq.srcGroupName;
                    TLOGERROR(FUN_LOG << err << endl);
                }
            }
            else
            {
                //新生成的迁移记录，记录迁移信息并准备开始迁移
                if (data.size() == 0)
                {
                    map<string, pair<TC_Mysql::FT, string> > m;
                    m["module"]     = make_pair(TC_Mysql::DB_STR, transferReq.moduleName);
                    m["appName"]    = make_pair(TC_Mysql::DB_STR, transferReq.appName);
                    m["src_group"]  = make_pair(TC_Mysql::DB_STR, transferReq.srcGroupName);
                    m["dest_group"] = make_pair(TC_Mysql::DB_STR, transferReq.cacheHost[0].groupName);
                    m["status"]     = make_pair(TC_Mysql::DB_INT, "0"); // 0-config stage

                    string startTime;
                    int iRet = formatCurrentTime(startTime);
                    if (iRet == 0)
                    {
                        m["transfer_start_time"] = make_pair(TC_Mysql::DB_STR, startTime);
                    }

                    _mysqlTransferExpandDb.insertRecord("t_transferv2_status", m);
                }
                else
                {
                    //以前有成功记录，就覆盖
                    map<string, pair<TC_Mysql::FT, string> > m;
                    m["status"] = make_pair(TC_Mysql::DB_INT, "0");

                    string startTime;
                    int iRet = formatCurrentTime(startTime);
                    if (iRet == 0)
                    {
                        m["transfer_start_time"] = make_pair(TC_Mysql::DB_STR, startTime);
                    }

                    string cond = "where module='" + transferReq.moduleName + "' and src_group='" + transferReq.srcGroupName + "' and appName='" + transferReq.appName + "' and dest_group='" + transferReq.cacheHost[0].groupName + "'";
                    _mysqlTransferExpandDb.updateRecord("t_transferv2_status", m, cond);
                }

               int iRet = expandDCacheServer(transferReq.appName, transferReq.moduleName, transferReq.cacheHost, transferReq.version, transferReq.cacheType, true, err);
               if (iRet == 0)
               {
                    string sSql = "select * from t_cache_router where app_name='" + transferReq.appName + "'";
                    TC_Mysql::MysqlData cacheRouterData = _mysqlRelationDB.queryRecord(sSql);
                    if (cacheRouterData.size() > 0)
                    {
                        vector<string> tmp = TC_Common::sepstr<string>(cacheRouterData[0]["router_name"], ".");
                        if (reloadRouterConfByModuleFromDB("DCache", transferReq.moduleName, tmp[0] + "." + tmp[1], err, current))
                        {
                            map<string, pair<TC_Mysql::FT, string> > m_update;
                            m_update["status"]  = make_pair(TC_Mysql::DB_INT, "1");
                            string condition = "where module='" + transferReq.moduleName + "' and src_group='" + transferReq.srcGroupName + "' and appName='" + transferReq.appName + "' and dest_group='" + transferReq.cacheHost[0].groupName + "'";
                            _mysqlTransferExpandDb.updateRecord("t_transferv2_status", m_update, condition);

                            TLOGDEBUG(FUN_LOG << "configure transfer record succ and expand cache server succ, module Name:" << transferReq.moduleName << "|srcGroupName: " << transferReq.srcGroupName << endl);

                            return 0;
                        }
                        else
                        {
                            err = "reload router conf from DB failed, router server name:" + tmp[1];
                            TLOGERROR(FUN_LOG << err << endl);
                        }
                    }
                    else
                    {
                        err = "no router obj selected for appName:" + transferReq.appName;
                        TLOGERROR(FUN_LOG << err << endl);
                    }
               }
               else
               {
                   err = "configure transfer record failed and expand cache server failed, module Name:" + transferReq.moduleName + "|srcGroupName:" + transferReq.srcGroupName + "|errmsg:" + err;
                   TLOGERROR(FUN_LOG << err << endl);
               }
            }
        }
    }
    catch(const std::exception &ex)
    {
        err = TC_Common::tostr(__FUNCTION__) + "|configure transfer record catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << err << endl);
    }

    return -1;
}

/*tars::Int32 DCacheOptImp::expandDCache(const ExpandDCacheReq & expandDCacheReq, ExpandDCacheRsp & expandDCacheRsq, tars::TarsCurrentPtr current)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "appName:" << expandDCacheReq.appName << "|moduleName:" << expandDCacheReq.moduleName << "|cacheType:" << expandDCacheReq.cacheType << endl);

        const vector<DCache::CacheHostParam> & vtCacheHost = expandDCacheReq.cacheHost;
        const string & sTarsVersion = expandDCacheReq.version;

        string &err = expandDCacheRsq.errMsg;

        if (expandDCacheReq.cacheType != DCache::KVCACHE && expandDCacheReq.cacheType != MKVCACHE)
        {
            err = "expand type error: KVCACHE-1 or MKVCACHE-2";
            return -1;
        }

        err = "success";

        string info;
        if (vtCacheHost.size() == 0)
        {
            err = "no cache server specified,please make sure params is right";
            return -1;
        }

        //RouterDbInfo routerDbInfo;
        //string sRouterObjName;
        //int iRet = getRouterDBInfoAndObj(expandDCacheReq.appName, routerDbInfo, sRouterObjName, err);
        //if (iRet == -1)
        //{
        //    return -1;
        //}

        RouterDbInfo routerDbInfo;
        string info;
        vector<string> vtProxyName;
        string routerServerName;
        int iRet = getRouterDBFromAppTable(expandDCacheReq.appName, routerDbInfo, vtProxyName, routerServerName, info);
        if (-1 == iRet)
        {
            err = info;
            return -1;
        }

        if (expandCacheConf(expandDCacheReq.appName, expandDCacheReq.moduleName, vtCacheHost, err) != 0)
        {
            err = string("failed to expand cache server conf") + err;
            return -1;
        }

        if (expandCache2RouterDb(expandDCacheReq.moduleName, routerDbInfo.sDbName, routerDbInfo.sDbIp, routerDbInfo.sPort, routerDbInfo.sUserName, routerDbInfo.sPwd, vtCacheHost, err) != 0)
        {
            return -1;
        }

        string sCacheType("");
        expandDCacheReq.expandType == DCache::KVCACHE ? sCacheType = "KV" : sCacheType = "MKV";
        iRet = insertCache2TarsDb(routerDbInfo.sDbName, routerDbInfo.sDbIp, routerDbInfo.sPort, routerDbInfo.sUserName, routerDbInfo.sPwd, vtCacheHost, "S", sTarsVersion, err);
        if (iRet != 0)
        {
            err = "failed to insert catch info to tars db, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        generateRelationDCacheRouter(appName, vtCacheHost,err,true);

        //把三者关系生成放到最后面 保证配额查询接口的准确性
        try
        {
            RouterDbInfo tmpRouterDbInfo;
            tmpRouterDbInfo = routerDbInfo;
            for (size_t i = 0; i < vtCacheHost.size(); ++i)
            {
                for (size_t j = 0; j < vtProxyName.size(); j++)
                {
                    // 保存 proxy server和cache server的对应关系
                    insertProxyCache(vtProxyName[j].substr(7), vtCacheHost[i].serverName);
                }

                // 保存cache serve和router server的对应关系
                insertCacheRouter(vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, int(TC_Common::strto<float>(vtCacheHost[i].shmSize)*1024), routerServerName + ".RouterObj", tmpRouterDbInfo, expandDCacheReq.moduleName, err);
            }
        }
        catch(exception &ex)
        {
            //这里捕捉异常以不影响安装结果
            err = string("option relation db catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << err << endl);
            return 0;
        }
    }
    catch(const std::exception &ex)
    {
        expandDCacheRsq.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << expandDCacheRsq.errMsg << endl);
        return -1;
    }

    return 0;
}*/

tars::Int32 DCacheOptImp::expandDCache(const ExpandReq & expandReq, ExpandRsp & expandRsq, tars::TarsCurrentPtr current)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "appName:" << expandReq.appName << "|moduleName:" << expandReq.moduleName << "|cacheType:" << expandReq.cacheType << endl);

        int iRet = expandDCacheServer(expandReq.appName, expandReq.moduleName, expandReq.cacheHost, expandReq.version, expandReq.cacheType, expandReq.replace, expandRsq.errMsg);

        return iRet;

    }
    catch(const std::exception &ex)
    {
        expandRsq.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << expandRsq.errMsg << endl);
    }

    return -1;
}


/*
* 配置中心操作接口
*/
tars::Int32 DCacheOptImp::addCacheConfigItem(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        string sSql = "select id from t_config_item where item='" + configReq.item + "' and path='" + configReq.path + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            errmsg = string("the record of t_config_item is already exists, item:") + configReq.item + ", path:" + configReq.path;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        map<string, pair<TC_Mysql::FT, string> > m;
        m["remark"] = make_pair(TC_Mysql::DB_STR, configReq.remark);
        m["item"]   = make_pair(TC_Mysql::DB_STR, configReq.item);
        m["path"]   = make_pair(TC_Mysql::DB_STR, configReq.path);
        m["reload"] = make_pair(TC_Mysql::DB_STR, configReq.reload);
        m["period"] = make_pair(TC_Mysql::DB_STR, configReq.period);

        _mysqlRelationDB.insertRecord("t_config_item", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("add config item to table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::updateCacheConfigItem(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m_update;
        m_update["remark"] = make_pair(TC_Mysql::DB_STR, configReq.remark);
        m_update["reload"] = make_pair(TC_Mysql::DB_STR, configReq.reload);
        m_update["period"] = make_pair(TC_Mysql::DB_STR, configReq.period);

        string condition = "where id=" + configReq.id;

        _mysqlRelationDB.updateRecord("t_config_item", m_update, condition);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("update config item on table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::deleteCacheConfigItem(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        string condition = "where id=" + configReq.id;

        _mysqlRelationDB.deleteRecord("t_config_item", condition);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("delete config item from table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::getCacheConfigItemList(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;

    try
    {
        string selectSql = "select * from t_config_item order by path, item";

        TC_Mysql::MysqlData itemData = _mysqlRelationDB.queryRecord(selectSql);
        configRsp.configItemList = itemData.data();
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get config item from table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::addServerConfigItem(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        string configId("");
        string level("");
        string serverName("");
        string nodeName("");
        if (configReq.serverName.size() > 0 && configReq.nodeName.size() > 0)
        {
            // serverName和nodeName不为空，则表示增加单个服务节点的配置项
            // configId 设置为0，level设置为2
            configId = "0";
            level = "2";
            serverName = configReq.serverName;
            nodeName = configReq.nodeName;
        }
        else
        {
            // serverName或nodeName为空，则表示增加模块的公共配置项
            // serverName 设置为模块名，nodeName设置为空，level设置为1
            int appModConfigId = 0;
            if (0 != getAppModConfigId(configReq.appName, configReq.moduleName, appModConfigId, errmsg))
            {
                errmsg = string("get app-mod config id failed, errmsg:") + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }
            configId = TC_Common::tostr(appModConfigId);
            level = "1";
            serverName = configReq.moduleName;
            nodeName = "";
        }

        map<string, pair<TC_Mysql::FT, string> > m;
        m["config_id"]    = make_pair(TC_Mysql::DB_INT, configId);
        m["server_name"]  = make_pair(TC_Mysql::DB_STR, serverName);
        m["host"]         = make_pair(TC_Mysql::DB_STR, nodeName);
        m["item_id"]      = make_pair(TC_Mysql::DB_INT, configReq.itemId);
        m["config_value"] = make_pair(TC_Mysql::DB_STR, configReq.configValue);
        m["level"]        = make_pair(TC_Mysql::DB_INT, level);
        m["posttime"]     = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]     = make_pair(TC_Mysql::DB_STR, configReq.lastUser);
        m["config_flag"]  = make_pair(TC_Mysql::DB_INT, configReq.configFlag); // 这个配置没啥用，建议删除，建表语句把这个字段也删除

        _mysqlRelationDB.insertRecord("t_config_table", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("add module or server config item to table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::updateServerConfigItem(const ServerConfigReq& configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m_update;
        m_update["config_value"] = make_pair(TC_Mysql::DB_STR, configReq.configValue);
        m_update["lastuser"]     = make_pair(TC_Mysql::DB_STR, configReq.lastUser);

        string condition = "where id=" + configReq.indexId;

        _mysqlRelationDB.updateRecord("t_config_table", m_update, condition);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("update config item on table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::updateServerConfigItemBatch(const vector<ServerConfigReq> & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        for (size_t i = 0; i < configReq.size(); ++i)
        {
            if (0 != updateServerConfigItem(configReq[i], configRsp, current))
            {
                return -1;
            }
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("update config item on table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::deleteServerConfigItem(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        string condition = "where id=" + configReq.indexId;

        _mysqlRelationDB.deleteRecord("t_config_table", condition);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("delete config item from table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::deleteServerConfigItemBatch(const vector<ServerConfigReq> & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        for (size_t i = 0; i < configReq.size(); ++i)
        {
            if (0 != deleteServerConfigItem(configReq[i], configRsp, current))
            {
                return -1;
            }
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("delete config item from table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::getServerNodeConfigItemList(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;

    try
    {
        string selectSql = "select ct.*, ci.remark, ci.item, ci.path, ci.reload, ci.period from t_config_table ct join t_config_item ci on ct.item_id = ci.id where server_name='" +  configReq.serverName + "' and host='" + configReq.nodeName + "'";

        TC_Mysql::MysqlData itemListData = _mysqlRelationDB.queryRecord(selectSql);
        configRsp.configItemList = itemListData.data();
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get config item from table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::getServerConfigItemList(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;

    try
    {
        int appModConfigId = 0;
        if (0 != getAppModConfigId(configReq.appName, configReq.moduleName, appModConfigId, errmsg))
        {
            errmsg = string("get app-mod config id failed, errmsg:") + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        // 查询模块的公共配置
        string selectSql = "select ct.*, ci.remark, ci.item, ci.path, ci.reload, ci.period from t_config_table ct join t_config_item ci on ct.item_id = ci.id where ct.config_id=" + TC_Common::tostr(appModConfigId);

        TC_Mysql::MysqlData itemListData = _mysqlRelationDB.queryRecord(selectSql);
        configRsp.configItemList = itemListData.data();

        if (configReq.serverName.size() > 0 && configReq.nodeName.size() > 0)
        {
            // serverName 和 nodeName，则需要查询服务节点的配置项
            ConfigRsp serverConfigRsq;
            int iRet = getServerNodeConfigItemList(configReq, serverConfigRsq, current);
            if (iRet != 0)
            {
                errmsg = string("get server config item failed, errmsg:" + serverConfigRsq.errMsg);
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            // 合并 模块公共配置和服务节点配置
            configRsp.configItemList.insert(configRsp.configItemList.end(), serverConfigRsq.configItemList.begin(), serverConfigRsq.configItemList.end());
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get module or server config item list from table t_config_table and t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}


/*
*------------------------------
*      private function
*------------------------------
*/
bool DCacheOptImp::checkRouterLoadModule(const std::string & sApp, const std::string & sModuleName, const std::string & sRouterServerName, std::string &errmsg)
{
    try
    {
        int times = 0;
        while(times < 2)
        {
            if (sApp.empty() || sRouterServerName.empty())
            {
                errmsg = "app name and router server name can not be empty";
                TLOGERROR(FUN_LOG << errmsg << endl);
                return false;
            }

            RouterPrx routerPrx;
            routerPrx = _communicator->stringToProxy<RouterPrx>(sRouterServerName + string(".RouterObj"));
            vector<TC_Endpoint> vEndPoints = routerPrx->getEndpoint4All();

            set<string> nodeIPset;
            for (vector<TC_Endpoint>::size_type i = 0; i < vEndPoints.size(); i++)
            {
                //获取所有节点ip
                nodeIPset.insert(vEndPoints[i].getHost());
            }

            set<string> reloadNodeIP;
            string info;
            set<string>::iterator pos;
            for (pos = nodeIPset.begin(); pos != nodeIPset.end(); ++pos)
            {
                _adminproxy->notifyServer(sApp, sRouterServerName.substr(7), *pos, "router.checkModule " + sModuleName, info);

                if (info.find("not support") != string::npos)
                {
                    TLOGERROR(FUN_LOG << "module is not support, need reload router|router server name:" << sRouterServerName << "|moduleName:" << sModuleName << "|notify server result:" << info << endl);
                    reloadNodeIP.insert(*pos);
                }
            }

            if (reloadNodeIP.empty())
            {
                break;
            }

            if (times == 0)
            {
                reloadRouterByModule(sApp, sModuleName, sRouterServerName, reloadNodeIP, errmsg);
                times++;
                sleep(10);
                continue;
            }
            else
            {
                errmsg = string("router load module:") + sModuleName + "'s routing failed, please check!";
                TLOGERROR(FUN_LOG << errmsg << endl);
                return false;
            }
        }
    }
    catch(const TarsException& ex)
    {
        errmsg = string("get server endpoint catch exception|server name:") + sRouterServerName + "|exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        return false;
    }

    errmsg = "";
    return true;
}

bool DCacheOptImp::reloadRouterByModule(const std::string & sApp, const std::string & sModuleName, const std::string & sRouterServerName, const set<string>& nodeIPset, std::string &errmsg)
{
    try
    {
        if (sApp.empty() || sRouterServerName.empty())
        {
            errmsg = "app name and router server name can not be empty";
            TLOGERROR(FUN_LOG << errmsg << endl);
            return false;
        }

        set<string>::iterator pos;
        for (pos = nodeIPset.begin(); pos != nodeIPset.end(); ++pos)
        {
            //通知router重新加载配置
            _adminproxy->notifyServer(sApp, sRouterServerName.substr(7), *pos, "router.reloadRouterByModule " + sModuleName, errmsg);
        }
    }
    catch(const TarsException& ex)
    {
        errmsg = string("reload router by module name catch TarsException:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        return false;
    }

    errmsg = "";
    return true;
}

tars::Bool DCacheOptImp::reloadRouterConfByModuleFromDB(const std::string & sApp, const std::string & sModuleName, const std::string & sRouterServerName, std::string &errmsg, tars::TarsCurrentPtr current)
{
    try
    {
        if (sApp.empty() || sRouterServerName.empty())
        {
            errmsg = "app name and router server name can not be empty";
            TLOGERROR(FUN_LOG << errmsg << endl);
            return false;
        }

        RouterPrx routerPrx;
        routerPrx = _communicator->stringToProxy<RouterPrx>(sRouterServerName + string(".RouterObj"));
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
            //通知router重新加载配置
            _adminproxy->notifyServer(sApp, sRouterServerName.substr(7), *pos, "router.reloadRouterByModule " + sModuleName, errmsg);
        }
    }
    catch(const TarsException& ex)
    {
        errmsg = string("reload router by module name catch TarsException:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        return false;
    }

    errmsg = "";
    return true;
}

int DCacheOptImp::createRouterDBConf(const RouterParam &param, string &errmsg)
{
    //string t_sOperation = "replace";

    try
    {
        /*string t_sdbConnectFlag = param.dbIp + string("_") + param.dbName; //"ip_数据库名"
        string t_sPostTime = TC_Common::now2str("%Y-%m-%d %H:%M:%S");

        map<string, pair<TC_Mysql::FT, string> > m;
        TC_Mysql::MysqlData t_tcConnData = _mysqlRouterConfDb.queryRecord(string("select * from t_config_dbconnects where name='") + t_sdbConnectFlag + string("'"));
        if (t_tcConnData.size() == 0)
        {
            m["NAME"]       = make_pair(TC_Mysql::DB_STR, t_sdbConnectFlag);
            m["HOST"]       = make_pair(TC_Mysql::DB_STR, param.dbIp);
            m["PORT"]       = make_pair(TC_Mysql::DB_STR, param.dbPort);
            m["USER"]       = make_pair(TC_Mysql::DB_STR, param.dbUser);
            m["PASSWORD"]   = make_pair(TC_Mysql::DB_STR, param.dbPwd);
            m["CHARSET"]    = make_pair(TC_Mysql::DB_STR, "utf8");
            m["DBNAME"]     = make_pair(TC_Mysql::DB_STR, param.dbName);
            m["ABOUT"]      = make_pair(TC_Mysql::DB_STR, param.remark);
            m["POSTTIME"]   = make_pair(TC_Mysql::DB_STR, t_sPostTime);
            m["LASTUSER"]   = make_pair(TC_Mysql::DB_STR, "system");

            _mysqlRouterConfDb.insertRecord("t_config_dbconnects", m);
        }
        else
        {
            m["NAME"]       = make_pair(TC_Mysql::DB_STR, t_sdbConnectFlag);
            m["HOST"]       = make_pair(TC_Mysql::DB_STR, param.dbIp);
            m["PORT"]       = make_pair(TC_Mysql::DB_STR, param.dbPort);
            m["USER"]       = make_pair(TC_Mysql::DB_STR, param.dbUser);
            m["PASSWORD"]   = make_pair(TC_Mysql::DB_STR, param.dbPwd);
            m["CHARSET"]    = make_pair(TC_Mysql::DB_STR, "utf8");
            m["DBNAME"]     = make_pair(TC_Mysql::DB_STR, param.dbName );
            m["ABOUT"]      = make_pair(TC_Mysql::DB_STR, param.remark);
            m["POSTTIME"]   = make_pair(TC_Mysql::DB_STR, t_sPostTime);
            m["LASTUSER"]   = make_pair(TC_Mysql::DB_STR, "system");

            _mysqlRouterConfDb.replaceRecord("t_config_dbconnects", m);
        }*/

        //创建路由数据库
        createRouterDB(param, errmsg);

        /*string sql = string("select ID from t_config_dbconnects where name='") + t_sdbConnectFlag + "' limit 1 ";
        TC_Mysql::MysqlData data = _mysqlRouterConfDb.queryRecord(sql);

        if (data.size() == 0)
        {
            errmsg = string("select ID from t_config_dbconnects size=0 [t_sdbConnectFlag=") + t_sdbConnectFlag + string("]");
            TLOGERROR(errmsg << endl);
            return -1;
        }
        else
        {
            string ID = data[0]["ID"];
            string t_sqlContent("select * from t_router_group order by id asc ");
            string columns(" OPNAME,QUERYKEY, QUERYPOSTTIME,QUERYLASTUSER,SERVERNAME, DBCONNECTID,PAGESIZE,`EXECUTESQL`,ORDERSQL,TABLENAME,OTHERCALL,POSTTIME,LASTUSER ");
            string t_sLastUser = "system";
            sql = t_sOperation + string(" into t_config_querys (")      + columns + string(")")
                + string(" values('DCache.RouterServer.RouterModule.")  + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'select * from  t_router_module order by id asc ','','t_router_module','','") + t_sPostTime + string("','") + t_sLastUser + string("'),")
                + string(" ('DCache.RouterServer.RouterTransfer.")      + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'select * from t_router_transfer order by id asc ','','t_router_transfer','','") + t_sPostTime + string("','") + t_sLastUser + string("'),")
                + string(" ('DCache.RouterServer.RouterRecord.")        + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'select * from t_router_record order by id asc ','','t_router_record','','") + t_sPostTime + string("','") + t_sLastUser + string("'),")
                + string(" ('DCache.RouterServer.RouterGroup.")         + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'") + t_sqlContent + string("','','t_router_group','','") +t_sPostTime + string("','") + t_sLastUser + string("'),")
                + string(" ('DCache.RouterServer.RouterServer.")        + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'select * from t_router_server order by id asc ','','t_router_server','','") + t_sPostTime + string("','") + t_sLastUser + string("') ");

            string sql4Module("select * from t_config_querys where opname in ('DCache.GetModulesNew')");
            if (_mysqlRouterConfDb.queryRecord(sql4Module).size() == 0) //数据库中没有，插入
            {
                sql += string(" ,('DCache.GetModulesNew','id','POSTTIME','LASTUSER','',0,100,'select concat(\\'DCache.\\',module_name) module_name,server_name from t_router_group ','','t_router_group','','") + t_sPostTime + string("','system')")   ;
            }

            _mysqlRouterConfDb.execute(sql);
            MYSQL *pMysql = _mysqlRouterConfDb.getMysql();
            MYSQL_RES *result = mysql_store_result(pMysql);
            mysql_free_result(result);

            string t_sOpNames = string("'DCache.RouterServer.RouterModule.")  + param.appName+ string("', ")
                              + string("'DCache.RouterServer.RouterTransfer.")+ param.appName + string("', ")
                              + string("'DCache.RouterServer.RouterRecord.")  + param.appName + string("', ")
                              + string("'DCache.RouterServer.RouterGroup.")   + param.appName + string("', ")
                              + string("'DCache.RouterServer.RouterServer.")  + param.appName + string("' ");
            sql = string(" select id, opname from t_config_querys where opname in (") + t_sOpNames+ string(") ") ;

            TC_Mysql::MysqlData data5 = _mysqlRouterConfDb.queryRecord(sql);
            if (5 == data5.size())
            {
                string t_sFields = "QUERYID,NAME,SEARCHNAME,ABOUT,ISMODIFY,ISSEARCH,ISVISIBLE,ISEQUAL,ISNULL,ORDERBY,EDITWIDTH,EDITHEIGHT,SEARCHSIZE,DATATYPE,RAWVALUE,OPTIONS,POSTTIME,LASTUSER";
                for (size_t i=0; i<data5.size(); i++)
                {
                    string t_ID =  data5[i]["id"];
                    string t_OPNAME = data5[i]["opname"];

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterModule"))
                    {
                        sql = t_sOperation + string("  into t_config_fields (") + t_sFields + string(") ")
                            + string(" values (") + t_ID + string(",'module_name','module_name','业务模块名','1','1','1','0','0',1,400,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'version','version','版本号','1','1','1','0','0',2,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'switch_status','status','模块状态','1','1','1','0','0',2,100,0,10,4,'','0|读写自动切换;1|只读自动切换;2|不支持自动切换;3|无效模块','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'remark','remark','备注','1','1','1','0','1',3,400,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','1','0','0','1',4,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterTransfer"))
                    {
                        sql = t_sOperation + string("  into t_config_fields (") + t_sFields + string(") ")
                            + string(" values (") + t_ID +string(",'from_page_no','from_page_no','迁移开始页','1','1','1','0','0',2,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'group_name','group_name','迁移原服务器组名','1','1','1','0','0',4,400,60,10,4,'','select group_name as S1, group_name as S2 from t_router_group','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','1','0','0','1',9,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'startTime','startTime','开始时间','1','1','1','0','1',9,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'endTime','endTime','结束时间','1','1','1','0','1',9,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'module_name','module_name','业务模块名','1','1','1','0','0',1,400,0,10,4,'','select module_name as S1, module_name as S2 from t_router_module','")  + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'remark','remark','迁移结果描述','1','1','1','0','1',7,400,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'state','state','迁移状态','1','1','1','0','0',8,100,0,10,4,'','0|未迁移;1|正在迁移;2|迁移结束;4|停止迁移','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'to_page_no','to_page_no','迁移结束页','1','1','1','0','0',3,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'transfered_page_no','transfered_page_no','已迁移成功页','1','1','1','0','1',6,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'trans_group_name','trans_group_name','迁移目的服务器组名','1','1','1','0','0',5,400,60,10,4,'','select group_name as S1, group_name as S2 from t_router_group','") + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterRecord"))
                    {
                        sql = t_sOperation + string(" into t_config_fields (") + t_sFields + string(") ")
                            + string(" values (") + t_ID + string(",'from_page_no','from_page_no','开始页','1','1','1','0','0',2,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'group_name','group_name','路由目的服务器组名','1','1','1','0','0',4,400,60,10,4,'','select group_name as S1, group_name as S2 from t_router_group','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'module_name','module_name','业务模块名','1','1','1','0','0',1,400,0,10,4,'','select module_name as S1, module_name as S2 from t_router_module','")  + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','1','0','0','1',5,100,0,10,9,'','','" ) + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'to_page_no','to_page_no','结束页','1','1','1','0','0',3,100,0,10,1,'','','")  + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterGroup"))
                    {
                        sql = t_sOperation + string("  into t_config_fields (") + t_sFields + string(") ")
                            + string(" values(") + t_ID + string(",'group_name','group_name','服务器组名','1','1','1','0','0',2,400,0,10,1,'','','")  + t_sPostTime + string("','dcache_init_fields.sh') ,")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','0','0','0','0',5,100,0,10,9,'','','")  + t_sPostTime +string( "','dcache_init_fields.sh') ,")
                            + string(" (") + t_ID + string(",'module_name','module_name','业务模块名','1','1','1','0','0',1,400,60,10,4,'','select module_name as S1, module_name as S2 from t_router_module','")  + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'pri','pri','优先级','1','1','1','0','1',5,150,0,15,1,'','','")  + t_sPostTime + string("','dcache_init_fields.sh' ),")
                            + string(" (") + t_ID + string(",'server_name','server_name','服务器名','1','1','1','0','0',3,400,60,10,4,'','select server_name as S1, server_name as S2 from t_router_server union all select \\'NULL\\' as S1, \\'NULL\\' as S2 from t_router_server','")  + t_sPostTime + string("','dcache_init_fields.sh' ),")
                            + string(" (") + t_ID + string(",'server_status','server_status','服务类型','1','1','1','0','0',4,400,60,10,4,'','M|主机;S|备机;I|镜像主机;B|镜像备机','")  + t_sPostTime + string("','dcache_init_fields.sh' ),")
                            + string(" (") + t_ID + string(",'access_status','access_status','访问状态','1','1','1','0','0',2,100,0,10,4,'','0|正常状态;1|只读状态;2|镜像切换状态','")  + t_sPostTime + string("','dcache_init_fields.sh' ),")
                            + string(" (") + t_ID + string(",'source_server_name','source_server_name','备份源Server','1','1','1','0','1',6,150,0,15,1,'','','")  + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterServer"))
                    {
                        sql = t_sOperation + string(" into t_config_fields (") + t_sFields + string(") ")
                            + string(" values(") + t_ID + string(",'binlog_port','binlog_port','BinLogObj服务端口','1','1','1','0','0',3,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'cache_port','cache_port','cache_port服务端口','1','1','1','0','0',4,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'wcache_port','cache_port','wcache_port服务端口','1','1','1','0','0',4,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'backup_port','backup_port','backup_port服务端口','1','1','1','0','0',4,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'ip','ip','服务器IP','1','1','1','0','0',2,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','1','0','0','1',7,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'remark','remark','备注','1','1','1','0','1',6,400,0,10,1,'','','" )+ t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'routeclient_port','routeclient_port','RouterClientObj服务端口','1','1','1','0','0',5,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'server_name','server_name','服务器名','1','1','1','0','0',1,400,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'idc_area','idc_area','IDC地区','1','1','1','0','0',0,150,0,15,4,'','sz|深圳;bj|北京;sh|上海;nj|南京;hk|香港;cd|成都','") + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    _mysqlRouterConfDb.execute(sql);

                    pMysql = _mysqlRouterConfDb.getMysql();
                    result = mysql_store_result(pMysql);
                    mysql_free_result(result);
                }
            }
        }*/
    }
    catch(const std::exception &ex)
    {
        errmsg = string("create router DB conf catch expcetion:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }
    catch (...)
    {
        errmsg = "create router DB conf catch unknown expcetion";
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::createRouterDB(const RouterParam &param, string &errmsg)
{
    TC_Mysql tcMysql;
    int iRet = 0;

    try
    {
        tcMysql.init(param.dbIp, _routerDbUser, _routerDbPwd, "", "utf8", TC_Common::strto<int>(param.dbPort));

        tcMysql.connect();
        MYSQL *pMysql = tcMysql.getMysql();

        //检查DB是否已经存在
        iRet = checkDb(tcMysql, param.dbName);
        if (iRet == 0)
        {
            // 不存在，则新建
            string sSql = "create database " + param.dbName;
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create database:") + param.dbName + "@" + param.dbIp + " error:" + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        string sSql2 = "use " + param.dbName;
        mysql_real_query(pMysql, sSql2.c_str(), sSql2.length());

        // 检查routerDB中各个表是否存在，不存在则新建
        iRet = checkTable(tcMysql, param.dbName, "t_router_module");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_module` (") +
                                "`id` int(11) NOT NULL auto_increment," +
                                "`module_name` varchar(255) NOT NULL default ''," +
                                "`version` int(11) NOT NULL default '0'," +
                                "`remark` varchar(255) default NULL," +
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP," +
                                "`POSTTIME` datetime default NULL," +
                                "`LASTUSER` varchar(60) default NULL," +
                                "`switch_status` int(11) default '0'," +
                                "PRIMARY KEY  (`id`)," +
                                "UNIQUE KEY `module_name` (`module_name`)" +
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table:") + param.dbName + ".t_router_module error:" + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        iRet = checkTable(tcMysql, param.dbName, "t_router_server");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_server` (")+
                                "`id` int(11) NOT NULL auto_increment,"+
                                "`server_name` varchar(255) NOT NULL default '',"+
                                "`ip` varchar(15) NOT NULL default '',"+
                                "`binlog_port` int(11) NOT NULL default '0',"+
                                "`cache_port` int(11) NOT NULL default '0',"+
                                "`routerclient_port` int(11) NOT NULL default '0',"+
                                "`backup_port` int(11) NOT NULL default '0',"+
                                "`wcache_port` int(11) NOT NULL default '0',"+
                                "`controlack_port` int(11) NOT NULL default '0',"+
                                "`idc_area` varchar(10) NOT NULL default 'sz',"+
                                "`status` int(11) NOT NULL default '0',"+
                                "`remark` varchar(255) default NULL,"+
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,"+
                                "`POSTTIME` datetime default NULL,"+
                                "`LASTUSER` varchar(60) default NULL,"+
                                "PRIMARY KEY  (`id`),"+
                                "UNIQUE KEY `server_name` (`server_name`)"+
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";

            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table: ") + param.dbName + ".t_router_server error: " + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        iRet = checkTable(tcMysql, param.dbName, "t_router_group");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_group` (")+
                                "`id` int(11) NOT NULL auto_increment,"+
                                "`module_name` varchar(255) NOT NULL default '',"+
                                "`group_name` varchar(255) NOT NULL default '',"+
                                "`server_name` varchar(255) NOT NULL default '',"+
                                "`server_status` char(1) NOT NULL default '',"+
                                "`priority` tinyint(4) NOT NULL default '1',"+
                                "`source_server_name` varchar(255) default NULL,"+
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,"+
                                "`POSTTIME` datetime default NULL,"+
                                "`LASTUSER` varchar(60) default NULL,"+
                                "`access_status` int(11) default '0'," +
                                "PRIMARY KEY  (`id`),"+
                                "UNIQUE KEY `server_name` (`server_name`)"+
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table: ") + param.dbName + ".t_router_group error: " + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        iRet = checkTable(tcMysql, param.dbName, "t_router_record");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_record` (")+
                                "`id` int(11) NOT NULL auto_increment,"+
                                "`module_name` varchar(255) NOT NULL default '',"+
                                "`from_page_no` int(11) NOT NULL default '0',"+
                                "`to_page_no` int(11) NOT NULL default '0',"+
                                "`group_name` varchar(255) NOT NULL default '',"+
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,"+
                                "`POSTTIME` datetime default NULL,"+
                                "`LASTUSER` varchar(60) default NULL,"+
                                "PRIMARY KEY  (`id`)"+
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table: ") + param.dbName + ".t_router_record error: " + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        iRet = checkTable(tcMysql, param.dbName, "t_router_transfer");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_transfer` (")+
                                "`id` int(11) NOT NULL auto_increment,"+
                                "`module_name` varchar(255) NOT NULL default '',"+
                                "`from_page_no` int(11) NOT NULL default '0',"+
                                "`to_page_no` int(11) NOT NULL default '0',"+
                                "`group_name` varchar(255) NOT NULL default '',"+
                                "`trans_group_name` varchar(255) NOT NULL default '',"+
                                "`transfered_page_no` int(11) default NULL,"+
                                "`remark` varchar(255) default NULL,"+
                                "`state` tinyint(4) NOT NULL default '0',"+
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,"+
                                "`startTime` datetime NOT NULL default '1970-01-01 00:00:01',"+
                                "`endTime` datetime NOT NULL default '1970-01-01 00:00:01',"+
                                "`POSTTIME` datetime default NULL,"+
                                "`LASTUSER` varchar(60) default NULL,"+
                                "PRIMARY KEY  (`id`)"+
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table: ") + param.dbName + ".t_router_transfer error: " + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }
    }
    catch (exception &ex)
    {
        tcMysql.disconnect();
        errmsg = string("create router db and table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::createRouterConf(const RouterParam &param, bool bRouterServerExist, bool bReplace, string & errmsg)
{
    if (!bRouterServerExist)
    {
        string sRouterConf = string("<Main>\n")+
                                    "   # 应用名称\n" +
                                    "   AppName=${AppName}\n" +
                                    "   # 数据库配置重新装载最小间隔时间\n" +
                                    "   DbReloadTime=300000\n" +
                                    "   # 管理接口的Obj\n"+
                                    "   AdminRegObj=tars.tarsregistry.AdminRegObj\n" +

                                    "   <ETCD>\n" +
                                    "       # 是否开启ETCD为Router集群(Router集群需要利用ETCD来做选举)\n" +
                                    "       enable=N\n" +
                                    "       # 所有ETCD机器的IP地址，以分号分割\n" +
                                    "       host=x.x.x.x:port;x.x.x.x:port\n" +
                                    "       # ETCD通信请求的超时时间(秒)\n" +
                                    "       RequestTimeout=3\n" +
                                    "       # Router主机心跳的维持时间(秒)\n" +
                                    "       EtcdHbTTL=60\n" +
                                    "   </ETCD>\n" +

                                    "   <Transfer>\n" +
                                    "       # 清理代理的最近未访问时间\n" +
                                    "       ProxyMaxSilentTime=1800\n" +
                                    "       # 清理代理的间隔时间\n" +
                                    "       ClearProxyInterval=1800\n" +
                                    "       # 轮询迁移数据库的时间\n" +
                                    "       TransferInterval=3\n" +
                                    "       # 轮询线程数\n" +
                                    "       TimerThreadSize=20\n" +
                                    "       # 等待页迁移的超时时间(毫秒)\n" +
                                    "       TransferTimeOut=3000\n" +
                                    "       # 迁移时隔多少页整理一下数据库记录\n" +
                                    "       TransferDefragDbInterval=50\n" +
                                    "       # 重新执行迁移指令的时间间隔(秒)\n" +
                                    "       RetryTransferInterval=1\n" +
                                    "       # 迁移失败时的最大重试次数\n" +
                                    "       RetryTransferMaxTimes=3\n" +
                                    "       # 一次迁移页数\n" +
                                    "       TransferPagesOnce=10\n" +
                                    "       # 每个组分配的最小迁移线程数\n" +
                                    "       MinTransferThreadEachGroup=5\n" +
                                    "       # 每个组分配的最大迁移线程数\n" +
                                    "       MaxTransferThreadEachGroup=8\n" +
                                    "   </Transfer>\n" +

                                    "   <Switch>\n" +
                                    "       # 自动切换超时的检测间隔(秒)\n" +
                                    "       SwitchCheckInterval= 10\n" +
                                    "       # 自动切换的超时时间(秒)\n" +
                                    "       SwitchTimeOut=60\n" +
                                    "       # 自动切换执行的线程数(默认1个)\n" +
                                    "       SwitchThreadSize=50\n" +
                                    "       # 备机不可读的超时时间(秒)\n" +
                                    "       SlaveTimeOut=60\n" +
                                    "       # 主备切换时，主备机binlog差异的阈值(毫秒)\n" +
                                    "       SwitchBinLogDiffLimit=300\n" +
                                    "       # 一天当中主备切换的最大次数\n" +
                                    "       SwitchMaxTimes=3\n" +
                                    "       # 主备切换时等待主机降级的时间(秒)\n" +
                                    "       DowngradeTimeout=30\n" +
                                    "   </Switch>\n" +

                                    "   <DB>\n" +
                                    "       <conn>\n" +
                                    "           charset=utf8\n" +
                                    "           dbname=${DbName}\n" +
                                    "           dbhost=${DbIp}\n" +
                                    "           dbpass=${DbPwd}\n" +
                                    "           dbport=${DbPort}\n" +
                                    "           dbuser=${DbUser}\n" +
                                    "       </conn>\n" +

                                    "       <relation>\n" +
                                    "           charset=${RelationDBCharset}\n" +
                                    "           dbname=${RelationDBName}\n" +
                                    "           dbhost=${RelationDBIp}\n" +
                                    "           dbpass=${RelationDBPass}\n" +
                                    "           dbport=${RelationDBPort}\n" +
                                    "           dbuser=${RelationDBUser}\n" +
                                    "       </relation>\n" +
                                    "   </DB>\n" +
                                    "</Main>\n";

        sRouterConf = TC_Common::replace(sRouterConf, "${AppName}", param.serverName);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbName}", param.dbName);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbIp}", param.dbIp);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbPort}", param.dbPort);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbUser}", param.dbUser);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbPwd}", param.dbPwd);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBCharset}", _relationDBInfo["charset"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBName}", _relationDBInfo["dbname"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBIp}", _relationDBInfo["dbhost"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBPass}", _relationDBInfo["dbpass"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBPort}", _relationDBInfo["dbport"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBUser}", _relationDBInfo["dbuser"]);

        int iConfigId;
        if (insertTarsConfigFilesTable(param.serverName, param.serverName.substr(7) + ".conf", "", sRouterConf, 2, iConfigId, bReplace, errmsg) != 0)
            return -1;

        if (insertTarsHistoryConfigFilesTable(iConfigId, sRouterConf, bReplace, errmsg) != 0)
            return -1;
    }

    int iHostConfigId;
    for (size_t i = 0; i < param.serverIp.size(); ++i)
    {
        if (insertTarsConfigFilesTable(param.serverName, param.serverName.substr(7) + ".conf", param.serverIp[i], "", 3, iHostConfigId, bReplace, errmsg) != 0)
            return -1;

        if (insertTarsHistoryConfigFilesTable(iHostConfigId, "", bReplace, errmsg) != 0)
            return -1;
    }

    return 0;
}

int DCacheOptImp::createProxyConf(const string &sModuleName, const ProxyParam &stProxyParam, const RouterParam &stRouterParam, bool bReplace, string & errmsg)
{
    string sProxyConf = string("<Main>\n")+
                        "   #同步路由表的时间间隔(秒)\n" +
                        "   SynRouterTableInterval = 1\n" +
                        "   #同步模块变化(模块新增或者下线)的时间间隔(秒)\n" +
                        "   SynRouterTableFactoryInterval = 5\n" +
                        "   #保存在本地的路由表文件名前缀\n" +
                        "   BaseLocalRouterFile = Router.dat\n" +
                        "   #模块路由表每秒钟的最大更新频率\n" +
                        "   RouterTableMaxUpdateFrequency = 3\n" +
                        "   RouterObj = ${RouterServant}\n" +
                        "</Main>\n";

    sProxyConf = TC_Common::replace(sProxyConf, "${RouterServant}", stRouterParam.serverName + ".RouterObj");

    int iConfigId;
    if (insertTarsConfigFilesTable(stProxyParam.serverName, "ProxyServer.conf", "", sProxyConf, 2, iConfigId, bReplace, errmsg) != 0)
        return -1;

    if (insertTarsHistoryConfigFilesTable(iConfigId, sProxyConf, bReplace, errmsg) != 0)
        return -1;

    string sHostProxyConf = string("<Main>\n") +
                            "   #sz-深圳; bj-北京; sh-上海; tj-天津\n" +
                            "   IdcArea = ${IdcArea}\n" +
                            "</Main>\n";
    for (size_t i = 0; i < stProxyParam.serverIp.size(); i++)
    {
        string stmpHostProxyConf = TC_Common::replace(sHostProxyConf, "${IdcArea}", stProxyParam.serverIp[i].idcArea);
        int iHostConfigId;
        if (insertTarsConfigFilesTable(stProxyParam.serverName, "ProxyServer.conf", stProxyParam.serverIp[i].ip, stmpHostProxyConf, 3, iHostConfigId, bReplace, errmsg) != 0)
            return -1;

        if (insertTarsHistoryConfigFilesTable(iHostConfigId, stmpHostProxyConf, bReplace, errmsg) != 0)
            return -1;
    }

    return 0;
}

int DCacheOptImp::matchItemId(const string &item, const string &path, string &item_id)
{
    string sql = "select id from t_config_item where item = '" + item + "' and path = '" + path + "'";
    TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sql);
    if (data.size() != 1)
    {
        string sErr = string("select from table t_config_item result error, the sql is:") + sql;
        TLOGDEBUG(FUN_LOG << sErr << endl);
        throw DCacheOptException(sErr);
    }
    item_id = data[0]["id"];

    return 0;
}

int DCacheOptImp::insertAppModTable(const string &appName, const string &moudleName, int &id, bool bReplace, string & errmsg)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "app name:" << appName << "|moudle name:" << moudleName << endl);
        map<string, pair<TC_Mysql::FT, string> > m;

        m["appName"]    = make_pair(TC_Mysql::DB_STR, appName);
        m["moduleName"] = make_pair(TC_Mysql::DB_STR, moudleName);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_config_appMod", m);
        else
            _mysqlRelationDB.insertRecord("t_config_appMod", m);

        string sSql = "select id from t_config_appMod where appName='" + appName + "' and moduleName='" + moudleName + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() != 1)
        {
            errmsg = string("select id from t_config_appMod result count not equal to 1, app name:") + appName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
        }

        id = TC_Common::strto<int>(data[0]["id"]);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("operate t_config_appMod error, app name:") + appName + "|module name:" + moudleName + "|catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertConfigFilesTable(const string &sFullServerName, const string &sHost, const int config_id,  vector< map<string,string> > &myVec, const int level, bool bReplace, string & errmsg)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "server name:" << sFullServerName << "|host:" << sHost << endl);

        map<string, pair<TC_Mysql::FT, string> > m;
        for (size_t i = 0; i < myVec.size(); i++)
        {
            m["server_name"] = make_pair(TC_Mysql::DB_STR, sFullServerName);
            m["host"]        = make_pair(TC_Mysql::DB_STR, sHost);
            m["posttime"]    = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            m["lastuser"]    = make_pair(TC_Mysql::DB_STR, "system");
            m["level"]       = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(level));
            m["config_flag"] = make_pair(TC_Mysql::DB_INT, "0");
            m["config_id"]   = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(config_id));

            std::string item_id = myVec[i]["item_id"];
            std::string config_value =  myVec[i]["config_value"];
            m["item_id"]      = make_pair(TC_Mysql::DB_STR, item_id);
            m["config_value"] = make_pair(TC_Mysql::DB_STR, config_value);

            if (bReplace)
                _mysqlRelationDB.replaceRecord("t_config_table", m);
            else
                _mysqlRelationDB.insertRecord("t_config_table", m);
        }
    }
    catch (const std::exception &ex)
    {
        errmsg = string("config table t_config_table servevr name:") + sFullServerName + "|catch exception:" + ex.what();
        TLOGDEBUG(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

int DCacheOptImp::insertReferenceTable(const int appConfigID, const string& server_name, const string& host, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["reference_id"]   = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(appConfigID));
        m["server_name"]    = make_pair(TC_Mysql::DB_STR, server_name);
        m["host"]           = make_pair(TC_Mysql::DB_STR, host);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_config_reference", m);
        else
            _mysqlRelationDB.insertRecord("t_config_reference", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("operate table t_config_reference catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

int DCacheOptImp::createKVCacheConf(const string &appName, const string &moduleName, const string &routerName, const vector<DCache::CacheHostParam> & cacheHost, const DCache::SingleKeyConfParam & kvConf, bool bReplace, string & errmsg)
{
    vector < map <string, string> > vtConfig;

    insertConfigItem2Vector("ModuleName", "Main", moduleName, vtConfig);
    insertConfigItem2Vector("CoreSizeLimit", "Main", "-1", vtConfig);
    insertConfigItem2Vector("BackupDayLog", "Main", "dumpAndRecover", vtConfig);
    insertConfigItem2Vector("RouterHeartbeatInterval", "Main", "1000", vtConfig);

    // <Cache Config>
    insertConfigItem2Vector("AvgDataSize", "Main/Cache", kvConf.avgDataSize, vtConfig);
    insertConfigItem2Vector("HashRadio", "Main/Cache", "2", vtConfig);

    insertConfigItem2Vector("EnableErase", "Main/Cache", kvConf.enableErase, vtConfig);
    insertConfigItem2Vector("EraseInterval", "Main/Cache", "5", vtConfig);
    insertConfigItem2Vector("EraseRadio", "Main/Cache", kvConf.eraseRadio, vtConfig);
    insertConfigItem2Vector("EraseThreadCount", "Main/Cache", "2", vtConfig);
    insertConfigItem2Vector("MaxEraseCountOneTime", "Main/Cache", "500", vtConfig);

    insertConfigItem2Vector("StartExpireThread", "Main/Cache", kvConf.startExpireThread, vtConfig);
    insertConfigItem2Vector("ExpireDb", "Main/Cache", kvConf.expireDb, vtConfig);
    insertConfigItem2Vector("ExpireInterval", "Main/Cache", "3600", vtConfig);
    insertConfigItem2Vector("ExpireSpeed", "Main/Cache", "0", vtConfig);

    insertConfigItem2Vector("SyncInterval", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("SyncSpeed", "Main/Cache", "0", vtConfig);
    insertConfigItem2Vector("SyncThreadNum", "Main/Cache", "1", vtConfig);
    insertConfigItem2Vector("SyncTime", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("SyncBlockTime", "Main/Cache", "0000-0000", vtConfig);

    insertConfigItem2Vector("SaveOnlyKey", "Main/Cache", kvConf.saveOnlyKey, vtConfig);
    insertConfigItem2Vector("DowngradeTimeout", "Main/Cache", "30", vtConfig);

    insertConfigItem2Vector("JmemNum", "Main/Cache", "10", vtConfig);
    insertConfigItem2Vector("coldDataCalEnable", "Main/Cache", "Y", vtConfig);
    insertConfigItem2Vector("coldDataCalCycle", "Main/Cache", "7", vtConfig);
    insertConfigItem2Vector("MaxKeyLengthInDB", "Main/Cache", "767", vtConfig);

    // <Log Config>
    insertConfigItem2Vector("DbDayLog", "Main/Log", "db", vtConfig);

    // <DbAccess Config>
    insertConfigItem2Vector("DBFlag", "Main/DbAccess", kvConf.dbFlag, vtConfig);
    insertConfigItem2Vector("ObjName", "Main/DbAccess", kvConf.dbAccessServant, vtConfig);
    if ((kvConf.enableErase == "Y") && (kvConf.dbFlag == "Y"))
    {
        insertConfigItem2Vector("ReadDbFlag", "Main/DbAccess", "Y", vtConfig);
    }
    else
    {
        insertConfigItem2Vector("ReadDbFlag", "Main/DbAccess", kvConf.readDbFlag, vtConfig);
    }

    // <Binlog Config>
    insertConfigItem2Vector("LogFile", "Main/BinLog", "binlog", vtConfig);
    insertConfigItem2Vector("Record", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("KeyRecord", "Main/BinLog", "N", vtConfig);
    insertConfigItem2Vector("MaxLine", "Main/BinLog", "10000", vtConfig);
    insertConfigItem2Vector("SyncCompress", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("IsGzip", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("KeySyncMode", "Main/BinLog", "N", vtConfig);
    insertConfigItem2Vector("BuffSize", "Main/BinLog", "10", vtConfig);
    insertConfigItem2Vector("SaveSyncTimeInterval", "Main/BinLog", "10", vtConfig);
    insertConfigItem2Vector("HeartbeatInterval", "Main/BinLog", "600", vtConfig);

    // <Router Config>
    insertConfigItem2Vector("ObjName", "Main/Router", routerName + ".RouterObj", vtConfig);
    insertConfigItem2Vector("PageSize", "Main/Router", "10000", vtConfig);
    insertConfigItem2Vector("RouteFile", "Main/Router", "Route.dat", vtConfig);
    insertConfigItem2Vector("SyncInterval", "Main/Router", "1", vtConfig);

    // shm size
    string shmSize;
    for (size_t i = 0; i < cacheHost.size(); i++)
    {
        if (cacheHost[i].type == "M")
        {
            shmSize = cacheHost[i].shmSize;
            break;
        }
    }
    insertConfigItem2Vector("ShmSize", "Main/Cache", shmSize + "G", vtConfig);

    // 保存appname和modulename间的关系
    int iAppConfigId;
    if (insertAppModTable(appName, moduleName, iAppConfigId, bReplace, errmsg) != 0)
        return -1;

    // 模块公共配置
    if (insertConfigFilesTable(appName, "", iAppConfigId, vtConfig, 1, bReplace, errmsg) != 0)
        return -1;

    // 节点服务配置
    for (size_t i=0; i < cacheHost.size(); i++)
    {
        vector < map <string, string> > level2Vec;

        string hostShmSize = TC_Common::tostr(cacheHost[i].shmSize);
        insertConfigItem2Vector("ShmSize", "Main/Cache", hostShmSize + "G", level2Vec);

        if (!cacheHost[i].shmKey.empty())
        {
            insertConfigItem2Vector("ShmKey", "Main/Cache", TC_Common::tostr(cacheHost[i].shmKey), level2Vec);
        }

        if (insertConfigFilesTable(cacheHost[i].serverName, cacheHost[i].serverIp, 0, level2Vec, 2, bReplace, errmsg) != 0)
            return -1;

        // 保存节点配置对模块公共配置的引用关系
        if (insertReferenceTable(iAppConfigId, cacheHost[i].serverName, cacheHost[i].serverIp, bReplace, errmsg) != 0)
            return -1;
    }

    return 0;
}

int DCacheOptImp::createMKVCacheConf(const string &appName, const string &moduleName, const string &routerName,const vector<DCache::CacheHostParam> & vtCacheHost,const DCache::MultiKeyConfParam & mkvConf, const vector<RecordParam> &vtRecordParam, bool bReplace, string & errmsg)
{
    TLOGDEBUG(FUN_LOG << "router name:" << routerName  << "|cache type:" << mkvConf.cacheType << endl);

    vector< map<string, string> > vtConfig;

    insertConfigItem2Vector("ModuleName", "Main", moduleName, vtConfig);
    insertConfigItem2Vector("CoreSizeLimit", "Main", "-1", vtConfig);
    insertConfigItem2Vector("MKeyMaxBlockCount", "Main", "20000", vtConfig);
    insertConfigItem2Vector("BackupDayLog", "Main", "dumpAndRecover", vtConfig);
    insertConfigItem2Vector("RouterHeartbeatInterval", "Main", "1000", vtConfig);

    // <Cache Config>
    insertConfigItem2Vector("AvgDataSize", "Main/Cache", mkvConf.avgDataSize, vtConfig);
    insertConfigItem2Vector("HashRadio", "Main/Cache", "2", vtConfig);
    insertConfigItem2Vector("MKHashRadio", "Main/Cache", "1", vtConfig);
    insertConfigItem2Vector("MainKeyType", "Main/Cache", mkvConf.cacheType, vtConfig);

    insertConfigItem2Vector("StartDeleteThread", "Main/Cache", "Y", vtConfig);
    insertConfigItem2Vector("DeleteInterval", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("DeleteSpeed", "Main/Cache", "0", vtConfig);

    insertConfigItem2Vector("EnableErase", "Main/Cache", mkvConf.enableErase, vtConfig);
    insertConfigItem2Vector("EraseInterval", "Main/Cache", "5", vtConfig);
    insertConfigItem2Vector("EraseRadio", "Main/Cache", mkvConf.eraseRadio, vtConfig);
    insertConfigItem2Vector("EraseThreadCount", "Main/Cache", "2", vtConfig);
    insertConfigItem2Vector("MaxEraseCountOneTime", "Main/Cache", "500", vtConfig);

    insertConfigItem2Vector("StartExpireThread", "Main/Cache", mkvConf.startExpireThread, vtConfig);
    insertConfigItem2Vector("ExpireDb", "Main/Cache", mkvConf.expireDb, vtConfig);
    insertConfigItem2Vector("ExpireInterval", "Main/Cache", "3600", vtConfig);
    insertConfigItem2Vector("ExpireSpeed", "Main/Cache", "0", vtConfig);

    insertConfigItem2Vector("SyncInterval", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("SyncSpeed", "Main/Cache", "0", vtConfig);
    insertConfigItem2Vector("SyncThreadNum", "Main/Cache", "1", vtConfig);
    insertConfigItem2Vector("SyncTime", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("SyncBlockTime", "Main/Cache", "0000-0000", vtConfig);
    insertConfigItem2Vector("SyncUNBlockPercent", "Main/Cache", "60", vtConfig);

    insertConfigItem2Vector("InsertOrder", "Main/Cache", "N", vtConfig);
    insertConfigItem2Vector("UpdateOrder", "Main/Cache", "N", vtConfig);
    insertConfigItem2Vector("MkeyMaxDataCount", "Main/Cache", "0", vtConfig);
    insertConfigItem2Vector("SaveOnlyKey", "Main/Cache", mkvConf.saveOnlyKey, vtConfig);
    insertConfigItem2Vector("OrderItem", "Main/Cache", "", vtConfig);
    insertConfigItem2Vector("OrderDesc", "Main/Cache", "Y", vtConfig);

    insertConfigItem2Vector("JmemNum", "Main/Cache", "2", vtConfig);
    insertConfigItem2Vector("DowngradeTimeout", "Main/Cache", "20", vtConfig);
    insertConfigItem2Vector("coldDataCalEnable", "Main/Cache", "Y", vtConfig);
    insertConfigItem2Vector("coldDataCalCycle", "Main/Cache", "7", vtConfig);
    insertConfigItem2Vector("MaxKeyLengthInDB", "Main/Cache", "767", vtConfig);

    insertConfigItem2Vector("transferCompress", "Main/Cache", "Y", vtConfig);

    // <Log Config>
    insertConfigItem2Vector("DbDayLog", "Main/Log", "db", vtConfig);

    // <DbAccess Config>
    insertConfigItem2Vector("DBFlag", "Main/DbAccess", mkvConf.dbFlag, vtConfig);
    insertConfigItem2Vector("ObjName", "Main/DbAccess", mkvConf.dbAccessServant, vtConfig);
    if ((mkvConf.enableErase == "Y") && (mkvConf.dbFlag == "Y"))
    {
        insertConfigItem2Vector("ReadDbFlag", "Main/DbAccess", "Y", vtConfig);
    }
    else
    {
        insertConfigItem2Vector("ReadDbFlag", "Main/DbAccess", mkvConf.readDbFlag, vtConfig);
    }

    // <Binlog Config>
    insertConfigItem2Vector("LogFile", "Main/BinLog", "binlog", vtConfig);
    insertConfigItem2Vector("Record", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("KeyRecord", "Main/BinLog", "N", vtConfig);
    insertConfigItem2Vector("KeySyncMode", "Main/BinLog", "N", vtConfig);
    insertConfigItem2Vector("MaxLine", "Main/BinLog", "10000", vtConfig);
    insertConfigItem2Vector("SyncCompress", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("IsGzip", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("BuffSize", "Main/BinLog", "10", vtConfig);
    insertConfigItem2Vector("SaveSyncTimeInterval", "Main/BinLog", "10", vtConfig);
    insertConfigItem2Vector("HeartbeatInterval", "Main/BinLog", "600", vtConfig);

    // <Router Config>
    insertConfigItem2Vector("ObjName", "Main/Router", routerName + ".RouterObj", vtConfig);
    insertConfigItem2Vector("PageSize", "Main/Router", "10000", vtConfig);
    insertConfigItem2Vector("RouteFile", "Main/Router", "Route.dat", vtConfig);
    insertConfigItem2Vector("SyncInterval", "Main/Router", "1", vtConfig);

    // <Record Config>
    string sMKey, sUKey, sValue, sRecords;
    int iMKeyNum = 0;
    int iTag = 0;
    for (size_t i = 0; i < vtRecordParam.size(); i++)
    {
        if (vtRecordParam[i].keyType == "mkey")
        {
            iMKeyNum++;
            if (iMKeyNum > 1)
            {
                errmsg = string("cache config format error, mkey number:") + TC_Common::tostr(iMKeyNum) + "no equal to 1|module name:" + moduleName;
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
            sRecords += "\t\t\t" + vtRecordParam[i].fieldName + " = " + TC_Common::tostr(iTag) + "|" + vtRecordParam[i].dataType + "|" + vtRecordParam[i].property + "|" + vtRecordParam[i].defaultValue + "|" + TC_Common::tostr(vtRecordParam[i].maxLen) + "\n";
            iTag++;
            sMKey = vtRecordParam[i].fieldName;
        }
    }

    for (size_t i=0; i<vtRecordParam.size(); i++)
    {
        if (vtRecordParam[i].keyType == "ukey")
        {
            sRecords += "\t\t\t" + vtRecordParam[i].fieldName + " = " + TC_Common::tostr(iTag) + "|" + vtRecordParam[i].dataType + "|" + vtRecordParam[i].property + "|" + vtRecordParam[i].defaultValue + "|" + TC_Common::tostr(vtRecordParam[i].maxLen) + "\n";
            iTag++;

            if (sUKey.length() == 0)
                sUKey = vtRecordParam[i].fieldName;
            else
                sUKey = sUKey + "|" + vtRecordParam[i].fieldName;
        }
    }

    if ((mkvConf.cacheType == "hash") && (sUKey.length() <= 0))
    {
        errmsg = string("module:") + moduleName + "'s cache config format error, not exist ukey";
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    for (size_t i = 0; i < vtRecordParam.size(); i++)
    {
        if (vtRecordParam[i].keyType == "value")
        {
            sRecords += "\t\t\t" + vtRecordParam[i].fieldName + " = " + TC_Common::tostr(iTag) + "|" + vtRecordParam[i].dataType + "|" + vtRecordParam[i].property + "|" + vtRecordParam[i].defaultValue + "|" + TC_Common::tostr(vtRecordParam[i].maxLen) + "\n";
            iTag++;
            if (sValue.length() == 0)
                sValue = vtRecordParam[i].fieldName;
            else
                sValue = sValue + "|" + vtRecordParam[i].fieldName;
        }
    }

    insertConfigItem2Vector("Virtualrecord", "Main/Record/Field", sRecords, vtConfig);
    insertConfigItem2Vector("MKey", "Main/Record", sMKey, vtConfig);
    insertConfigItem2Vector("UKey", "Main/Record", sUKey, vtConfig);
    insertConfigItem2Vector("VKey", "Main/Record", sValue, vtConfig);

    // 保存 应用和模块之间的对应关系
    int iAppConfigId;
    if (insertAppModTable(appName, moduleName, iAppConfigId, bReplace, errmsg) != 0)
        return -1;

    if (insertConfigFilesTable(appName, "", iAppConfigId, vtConfig, 1, bReplace, errmsg) != 0)
        return -1;

    for (size_t i = 0; i < vtCacheHost.size(); ++i)
    {

        vector < map <string, string> > level2Vec;
        map<string , string> level3Map;
        string level2_item;
        string level2_path;
        string level2_item_id;

        string hostShmSize = TC_Common::tostr(vtCacheHost[i].shmSize);
        level3Map["config_value"] = hostShmSize + "G";

        level2_item = "ShmSize";
        level2_path = "Main/Cache";
        matchItemId(level2_item, level2_path, level2_item_id);
        level3Map["item_id"] = level2_item_id;

        level2Vec.push_back(level3Map);

        if (!vtCacheHost[i].shmKey.empty())
        {
            insertConfigItem2Vector("ShmKey", "Main/Cache", TC_Common::tostr(vtCacheHost[i].shmKey), level2Vec);
        }

        if (insertConfigFilesTable(vtCacheHost[i].serverName, vtCacheHost[i].serverIp, 0, level2Vec, 2, bReplace, errmsg) != 0)
            return -1;

        if (insertReferenceTable(iAppConfigId, vtCacheHost[i].serverName, vtCacheHost[i].serverIp, bReplace, errmsg) != 0)
            return -1;
    }

    return 0;
}

void DCacheOptImp::insertConfigItem2Vector(const string& sItem, const string &sPath, const string &sConfigValue, vector < map <string, string> > &vtConfig)
{
    map<string , string> mMap;
    string sItemId("");
    matchItemId(sItem, sPath, sItemId);

    mMap["item_id"]      = sItemId;
    mMap["config_value"] = sConfigValue;

    vtConfig.push_back(mMap);
}

int DCacheOptImp::insertCache2RouterDb(const string& sModuleName, const string &sRemark, const string& sRouterDbHost, const string& sRouterDbName, const string &sRouterDbPort, const string &DbUser, const string &DbPassword, const vector<DCache::CacheHostParam> & vtCacheHost, bool bReplace, string& errmsg)
{
    TC_Mysql tcMysql;
    try
    {
        tcMysql.init(sRouterDbHost, DbUser, DbPassword, sRouterDbName, "utf8", TC_Common::strto<int>(sRouterDbPort));

        if (tcMysql.getRecordCount("t_router_module", "where module_name='" + sModuleName + "'") == 0)
        {
            map<string, pair<TC_Mysql::FT, string> > mpModule;
            mpModule["module_name"] = make_pair(TC_Mysql::DB_STR, sModuleName);
            mpModule["version"]     = make_pair(TC_Mysql::DB_INT, "1");
            mpModule["remark"]      = make_pair(TC_Mysql::DB_STR, sRemark);
            mpModule["POSTTIME"]    = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpModule["LASTUSER"]    = make_pair(TC_Mysql::DB_STR, "sys");

            if (bReplace)
                tcMysql.replaceRecord("t_router_module", mpModule);
            else
                tcMysql.insertRecord("t_router_module", mpModule);
        }
        else
        {
            // 已经存在，则更新版本号
            string updateVersion = string("update t_router_module set version = version+1 where module_name='") + sModuleName + "';";
            TLOGDEBUG(FUN_LOG << "module name:" << sModuleName << " already exist, update router version, do SQL:" << updateVersion << endl);
            tcMysql.execute(updateVersion);
        }

        uint16_t iPort = getPort(vtCacheHost[0].serverIp);
        if (0 == iPort)
        {
            errmsg = "failed to get port for cache server ip:" + vtCacheHost[0].serverIp;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        set<string> setAddr;
        for (size_t i = 0; i < vtCacheHost.size(); i++)
        {
            map<string, pair<TC_Mysql::FT, string> > mpServer;
            if (vtCacheHost[i].isContainer == "true")
            {
                mpServer["server_name"]       = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
                mpServer["ip"]                = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverIp);
                mpServer["binlog_port"]       = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].binlogPort);
                mpServer["cache_port"]        = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].cachePort);
                mpServer["routerclient_port"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].routerPort);
                mpServer["backup_port"]       = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].backupPort);
                mpServer["wcache_port"]       = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].wcachePort);
                mpServer["controlack_port"]   = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].controlackPort);
            }
            else
            {
                mpServer["server_name"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
                mpServer["ip"]          = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverIp);

                uint16_t iBinLogPort = iPort;
                while(true)
                {
                    iBinLogPort = getPort(vtCacheHost[i].serverIp, iBinLogPort);
                    if (iBinLogPort == 0)
                    {
                        errmsg = "failed to get port for binlog obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBinLogPort)) != setAddr.end())
                    {
                        iBinLogPort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBinLogPort));
                mpServer["binlog_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iBinLogPort));

                uint16_t iCachePort = iPort + 1;
                while(true)
                {
                    iCachePort = getPort(vtCacheHost[i].serverIp, iCachePort);
                    if (iCachePort == 0)
                    {
                        errmsg = "failed to get port for cache obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iCachePort)) != setAddr.end())
                    {
                        iCachePort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iCachePort));
                mpServer["cache_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iCachePort));

                uint16_t iRouterPort = iPort + 2;
                while(true)
                {
                    iRouterPort = getPort(vtCacheHost[i].serverIp, iRouterPort);
                    if (iRouterPort == 0)
                    {
                        errmsg = "failed to get port for router client obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iRouterPort)) != setAddr.end())
                    {
                        iRouterPort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iRouterPort));
                mpServer["routerclient_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iRouterPort));

                uint16_t iBackUpPort = iPort + 3;
                while(true)
                {
                    iBackUpPort = getPort(vtCacheHost[i].serverIp, iBackUpPort);
                    if (iBackUpPort == 0)
                    {
                        errmsg = "failed to get port for backup obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBackUpPort)) != setAddr.end())
                    {
                        iBackUpPort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBackUpPort));
                mpServer["backup_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iBackUpPort));

                uint16_t iWCachePort = iPort + 4;
                while(true)
                {
                    iWCachePort = getPort(vtCacheHost[i].serverIp, iWCachePort);
                    if (iWCachePort == 0)
                    {
                        errmsg = "failed to get port for wcache obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iWCachePort)) != setAddr.end())
                    {
                        iWCachePort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iWCachePort));
                mpServer["wcache_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iWCachePort));

                uint16_t iControlAckPort = iPort + 5;
                while(true)
                {
                    iControlAckPort = getPort(vtCacheHost[i].serverIp, iControlAckPort);
                    if (iControlAckPort == 0)
                    {
                        errmsg = "failed to get port for control ack obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iControlAckPort)) != setAddr.end())
                    {
                        iControlAckPort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iControlAckPort));
                mpServer["controlack_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iControlAckPort));
            }
            mpServer["idc_area"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].idc);
            mpServer["POSTTIME"] = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpServer["LASTUSER"] = make_pair(TC_Mysql::DB_STR, "sys");

            if (tcMysql.getRecordCount("t_router_server", "where server_name='" + vtCacheHost[i].serverName + "'") == 0)
            {
                tcMysql.insertRecord("t_router_server", mpServer);
            }

            //避免主机重复
            if (!bReplace && (vtCacheHost[i].type == "M") && tcMysql.getRecordCount("t_router_group", "where server_status='M' AND module_name='" + sModuleName + "' AND group_name='" + vtCacheHost[i].groupName + "'") > 0)
            {
                errmsg = string("module_name:") + sModuleName + ", group_name:" + vtCacheHost[i].groupName + " has master cache server|server name:" + vtCacheHost[i].serverName;
                TLOGERROR(FUN_LOG << errmsg << endl);
                continue;
            }

            string groupsql = "SELECT * FROM t_router_group WHERE module_name='" + sModuleName + "' AND group_name='" + vtCacheHost[i].groupName + "' AND server_name='" + vtCacheHost[i].serverName + "'";
            TC_Mysql::MysqlData existRouterGroup = tcMysql.queryRecord(groupsql);
            if (existRouterGroup.size() > 0)
            {
                if (existRouterGroup[0]["source_server_name"] != vtCacheHost[i].bakSrcServerName)
                {
                    errmsg = string("existed record's source_server_name incosistent with param:") + vtCacheHost[i].bakSrcServerName + "|existed record in database is :" + existRouterGroup[0]["source_server_name"];
                    TLOGERROR(FUN_LOG << errmsg<< endl);
                    return -1;
                }
                else
                {
                    continue;
                }
            }

            map<string, pair<TC_Mysql::FT, string> > mpGroup;
            mpGroup["module_name"]        = make_pair(TC_Mysql::DB_STR, sModuleName);
            mpGroup["group_name"]         = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].groupName);
            mpGroup["server_name"]        = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
            mpGroup["server_status"]      = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].type);
            mpGroup["priority"]           = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].priority);
            mpGroup["source_server_name"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].bakSrcServerName);
            mpGroup["POSTTIME"]           = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpGroup["LASTUSER"]           = make_pair(TC_Mysql::DB_STR, "sys");

            if (bReplace)
                tcMysql.replaceRecord("t_router_group", mpGroup);
            else
                tcMysql.insertRecord("t_router_group", mpGroup);
        }

        // 删除可能存在的模块路由信息
        tcMysql.deleteRecord("t_router_record", "where module_name = '" + sModuleName + "'");

        string sSql = "select group_name from t_router_group where module_name = '" + sModuleName + "' group by group_name order by group_name";
        TC_Mysql::MysqlData data = tcMysql.queryRecord(sSql);
        if (data.size() <= 0)
        {
            errmsg = string("select group_name from t_router_group error|module_name:") + sModuleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
        }

        // 根据组的个数均分路由页
        int num    = MAX_ROUTER_PAGE_NUM / data.size();
        int iBegin = 0;
        int iEnd   = MAX_ROUTER_PAGE_NUM;
        for (size_t i = 0; i < data.size(); i++)
        {
            int iToPage;
            if ( (i + 1) == data.size() )
            {
                iToPage = iEnd;
            }
            else
            {
                iToPage = iBegin + num;
            }

            map<string, pair<TC_Mysql::FT, string> > mpRecord;
            mpRecord["module_name"]     = make_pair(TC_Mysql::DB_STR, sModuleName);
            mpRecord["from_page_no"]    = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(iBegin));
            mpRecord["to_page_no"]      = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(iToPage));
            mpRecord["group_name"]      = make_pair(TC_Mysql::DB_STR, data[i]["group_name"]);
            mpRecord["POSTTIME"]        = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpRecord["LASTUSER"]        = make_pair(TC_Mysql::DB_STR, "sys");

            if (bReplace)
                tcMysql.replaceRecord("t_router_record", mpRecord);
            else
                tcMysql.insertRecord("t_router_record", mpRecord);

            iBegin = iToPage + 1;
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("insert module name:") + sModuleName + " to router db's table catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        tcMysql.disconnect();
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertTarsDb(const ProxyParam &stProxyParam, const RouterParam &stRouterParam, const std::string &sTarsVersion, const bool enableGroup, bool bReplace, string & errmsg)
{
    if (stProxyParam.installProxy)
    {
        for (size_t i = 0; i < stProxyParam.serverIp.size(); i++)
        {
            uint16_t iProxyObjPort = getPort(stProxyParam.serverIp[i].ip);
            uint16_t iRouterClientObjPort = getPort(stProxyParam.serverIp[i].ip);
            if (0 == iProxyObjPort || 0 == iRouterClientObjPort)
            {
                //获取PORT失败
                TLOGERROR(FUN_LOG << "failed to get port for " << stProxyParam.serverName << "_" << stProxyParam.serverIp[i].ip << "|" << iProxyObjPort << "|" << iRouterClientObjPort << endl);
            }

            if (insertTarsServerTableWithIdcGroup("DCache", stProxyParam.serverName, stProxyParam.serverIp[i], stProxyParam.serverIp[i].ip, stProxyParam.templateFile, "", sTarsVersion, enableGroup, bReplace, errmsg) != 0)
                return -1;

            string sProxyServant = stProxyParam.serverName + ".ProxyObj";
            string sProxyEndpoint = "tcp -h " + stProxyParam.serverIp[i].ip + " -t 60000 -p "+ TC_Common::tostr(getPort(stProxyParam.serverIp[i].ip, iProxyObjPort));
            if (insertServantTable("DCache", stProxyParam.serverName.substr(7), stProxyParam.serverIp[i].ip, sProxyServant, sProxyEndpoint, "10", bReplace, errmsg) != 0)
                return -1;

            string sRouteClientServant = stProxyParam.serverName + ".RouterClientObj";
            string sRouteClientEndpoint = "tcp -h " + stProxyParam.serverIp[i].ip + " -t 60000 -p " + TC_Common::tostr(getPort(stProxyParam.serverIp[i].ip, iRouterClientObjPort));
            if (insertServantTable("DCache", stProxyParam.serverName.substr(7), stProxyParam.serverIp[i].ip, sRouteClientServant, sRouteClientEndpoint, "10", bReplace, errmsg) != 0)
                return -1;
        }
    }

    if (stRouterParam.installRouter)
    {
        for (size_t i = 0; i < stRouterParam.serverIp.size(); ++i)
        {
            if (insertTarsServerTable("DCache", stRouterParam.serverName, stRouterParam.serverIp[i], stRouterParam.templateFile, "", sTarsVersion, false, bReplace, errmsg) != 0)
                return -1;

            string sRouterServant = stRouterParam.serverName + ".RouterObj";
            uint16_t iRouterObjPort = getPort(stRouterParam.serverIp[i]);
            if (0 == iRouterObjPort)
            {
                TLOGERROR(FUN_LOG << "failed to get port for " << stRouterParam.serverName << "_" << stRouterParam.serverIp[i]<< "|" << iRouterObjPort << endl);
            }

            string sRouterEndpoint = "tcp -h " + stRouterParam.serverIp[i] + " -t 60000 -p " + TC_Common::tostr(iRouterObjPort);
            if (insertServantTable("DCache", stRouterParam.serverName.substr(7),  stRouterParam.serverIp[i], sRouterServant, sRouterEndpoint, "10", bReplace, errmsg) != 0)
                return -1;
        }
    }

    return 0;
}

int DCacheOptImp::insertCache2TarsDb(const string &sRouterDbName, const string& sRouterDbHost, const string &sRouterDbPort,const string &sRouterDbUser, const string &sRouterDbPwd, const vector<DCache::CacheHostParam> & vtCacheHost, DCache::DCacheType eCacheType, const string &sTarsVersion, bool bReplace, std::string &errmsg)
{
    string sExePath;
    if (eCacheType == DCache::KVCACHE)
        sExePath = "KVCacheServer";
    else
        sExePath = "MKVCacheServer";

    TC_Mysql tcMysql;
    try
    {
        tcMysql.init(sRouterDbHost, sRouterDbUser, sRouterDbPwd, sRouterDbName, "utf8", TC_Common::strto<int>(sRouterDbPort));

        for (size_t i = 0; i < vtCacheHost.size(); i++)
        {
            if (insertTarsServerTable("DCache", vtCacheHost[i].serverName, vtCacheHost[i].serverIp, vtCacheHost[i].templateFile, sExePath, sTarsVersion, false, bReplace, errmsg) != 0)
                return -1;

            string sBinLogPort, sCachePort,sRouterPort,sBackUpPort,sWCachePort,sControlAckPort;

            if (selectPort(tcMysql, vtCacheHost[i].serverName, vtCacheHost[i].serverIp, sBinLogPort, sCachePort, sRouterPort, sBackUpPort, sWCachePort, sControlAckPort, errmsg) != 0)
                return -1;

            string sBinLogServant = vtCacheHost[i].serverName + ".BinLogObj";
            string sBinLogEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sBinLogPort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sBinLogServant, sBinLogEndpoint, "3", bReplace, errmsg) != 0)
                return -1;

            string sCacheServant = vtCacheHost[i].serverName + ".CacheObj";
            string sCacheEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sCachePort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sCacheServant, sCacheEndpoint, "8", bReplace, errmsg) != 0)
                return -1;

            string sRouterServant = vtCacheHost[i].serverName + ".RouterClientObj";
            string sRouterEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 180000 -p " + sRouterPort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sRouterServant, sRouterEndpoint, "5", bReplace, errmsg) != 0)
                return -1;

            string sBackUpServant = vtCacheHost[i].serverName + ".BackUpObj";
            string sBackupEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sBackUpPort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sBackUpServant, sBackupEndpoint, "1", bReplace, errmsg) != 0)
                return -1;

            string sWCacheServant = vtCacheHost[i].serverName + ".WCacheObj";
            string sWCacheEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sWCachePort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sWCacheServant, sWCacheEndpoint, "8", bReplace, errmsg) != 0)
                return -1;

            string sControlAckServant = vtCacheHost[i].serverName + ".ControlAckObj";
            string sControlAckEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sControlAckPort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sControlAckServant, sControlAckEndpoint, "1", bReplace, errmsg) != 0)
                return -1;
        }
    }
    catch(exception &ex)
    {
        tcMysql.disconnect();
        errmsg = string("insert cache info to tars db catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }
    catch (...)
    {
        tcMysql.disconnect();
        errmsg = string("insert cache info to tars db catch unknown exception");
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertTarsConfigFilesTable(const string &sFullServerName, const string &sConfName, const string &sHost, const string &sConfig, const int level, int &id, bool bReplace, string & errmsg)
{
    string sSql;
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;

        m["server_name"]    = make_pair(TC_Mysql::DB_STR, sFullServerName);
        m["host"]           = make_pair(TC_Mysql::DB_STR, sHost);
        m["filename"]       = make_pair(TC_Mysql::DB_STR, sConfName);
        m["config"]         = make_pair(TC_Mysql::DB_STR, sConfig);
        m["posttime"]       = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]       = make_pair(TC_Mysql::DB_STR, "system");
        m["level"]          = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(level));
        m["config_flag"]    = make_pair(TC_Mysql::DB_INT, "0");

        if(bReplace)
            _mysqlTarsDb.replaceRecord("t_config_files", m);
        else
            _mysqlTarsDb.insertRecord("t_config_files", m);

        sSql= "select id from t_config_files where server_name = '" + sFullServerName + "' and filename = '" + sConfName + "' and host = '" + sHost + "' and level = " + TC_Common::tostr(level);
        TC_Mysql::MysqlData data = _mysqlTarsDb.queryRecord(sSql);
        if (data.size() != 1)
        {
            errmsg = string("get id from t_config_files error, serverName = ") + sFullServerName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
        }

        id = TC_Common::strto<int>(data[0]["id"]);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("insert server name:") + sFullServerName + " catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertTarsHistoryConfigFilesTable(const int iConfigId, const string &sConfig, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["configid"]       = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(iConfigId));
        m["reason"]         = make_pair(TC_Mysql::DB_STR, "新建立配置文件");
        m["reason_select"]  = make_pair(TC_Mysql::DB_STR, "");
        m["content"]        = make_pair(TC_Mysql::DB_STR, sConfig);
        m["posttime"]       = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]       = make_pair(TC_Mysql::DB_STR, "system");

        if(bReplace)
            _mysqlTarsDb.replaceRecord("t_config_history_files", m);
        else
            _mysqlTarsDb.insertRecord("t_config_history_files", m);
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert server:") + TC_Common::tostr(iConfigId) + " history config error, catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertTarsServerTableWithIdcGroup(const string &sApp, const string& sServerName , const ProxyAddr &stProxyAddr, const string &sIp, const string &sTemplateName, const string &sExePath,const std::string &sTarsVersion, const bool enableGroup, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["application"]    = make_pair(TC_Mysql::DB_STR, sApp);
        m["server_name"]    = make_pair(TC_Mysql::DB_STR, sServerName.substr(7));//去掉前缀DCache.
        m["node_name"]      = make_pair(TC_Mysql::DB_STR, sIp);
        m["exe_path"]       = make_pair(TC_Mysql::DB_STR, sExePath);
        m["template_name"]  = make_pair(TC_Mysql::DB_STR, sTemplateName);
        m["posttime"]       = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]       = make_pair(TC_Mysql::DB_STR, "sys");
        m["tars_version"]   = make_pair(TC_Mysql::DB_STR, sTarsVersion);

        // 这里默认开启IDC分组
        m["enable_group"] = make_pair(TC_Mysql::DB_STR,(enableGroup)?"Y":"N");//是否启用IDC分组

        if (stProxyAddr.idcArea == "sz")
        {
            m["ip_group_name"] = make_pair(TC_Mysql::DB_STR, "sz");
        }
        else if (stProxyAddr.idcArea == "tj")
        {
            m["ip_group_name"] = make_pair(TC_Mysql::DB_STR, "tj");
        }
        else if (stProxyAddr.idcArea == "sh")
        {
            m["ip_group_name"] = make_pair(TC_Mysql::DB_STR, "sh");
        }
        else if (stProxyAddr.idcArea == "cd")
        {
            m["ip_group_name"] = make_pair(TC_Mysql::DB_STR, "cd");
        }
        else
        {
            //启用IDC分组改为No
            m["enable_group"] = make_pair(TC_Mysql::DB_STR, "N");
        }

        if(bReplace)
            _mysqlTarsDb.replaceRecord("t_server_conf", m);
        else
            _mysqlTarsDb.insertRecord("t_server_conf", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("insert appName:") + sApp + ", serverName:" + sServerName + "@" + sIp +"'s tars server config error, " + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertTarsServerTable(const string &sApp, const string &sServerName, const string &sIp, const string &sTemplateName, const string &sExePath,const std::string &sTarsVersion, const bool enableGroup, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["application"]    = make_pair(TC_Mysql::DB_STR, sApp);
        m["server_name"]    = make_pair(TC_Mysql::DB_STR, sServerName.substr(7));
        m["node_name"]      = make_pair(TC_Mysql::DB_STR, sIp);
        m["exe_path"]       = make_pair(TC_Mysql::DB_STR, sExePath);
        m["template_name"]  = make_pair(TC_Mysql::DB_STR, sTemplateName);
        m["posttime"]       = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]       = make_pair(TC_Mysql::DB_STR, "sys");
        m["tars_version"]   = make_pair(TC_Mysql::DB_STR, sTarsVersion);

        m["enable_group"] = make_pair(TC_Mysql::DB_STR,(enableGroup)?"Y":"N");//是否启用IDC分组

        if (bReplace)
            _mysqlTarsDb.replaceRecord("t_server_conf", m);
        else
            _mysqlTarsDb.insertRecord("t_server_conf", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("insert") + sApp + "."+ sServerName + "@" + sIp + "'s tars server config error, catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertServantTable(const string &sApp, const string &sServerName, const string &sIp, const string &sServantName, const string &sEndpoint, const string &sThreadNum, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["application"]        = make_pair(TC_Mysql::DB_STR, sApp);
        m["server_name"]        = make_pair(TC_Mysql::DB_STR, sServerName);
        m["node_name"]          = make_pair(TC_Mysql::DB_STR, sIp);
        m["adapter_name"]       = make_pair(TC_Mysql::DB_STR, sServantName + "Adapter");
        m["thread_num"]         = make_pair(TC_Mysql::DB_INT, sThreadNum);
        m["endpoint"]           = make_pair(TC_Mysql::DB_STR, sEndpoint);
        m["max_connections"]    = make_pair(TC_Mysql::DB_INT, "10240");
        m["servant"]            = make_pair(TC_Mysql::DB_STR, sServantName);
        m["queuecap"]           = make_pair(TC_Mysql::DB_INT, "100000");
        m["queuetimeout"]       = make_pair(TC_Mysql::DB_INT, "60000");
        m["posttime"]           = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]           = make_pair(TC_Mysql::DB_STR, "sys");
        m["protocol"]           = make_pair(TC_Mysql::DB_STR, "tars");

        if (bReplace)
            _mysqlTarsDb.replaceRecord("t_adapter_conf", m);
        else
            _mysqlTarsDb.insertRecord("t_adapter_conf", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("insert ") + sApp + "." + sServerName + "@" + sIp + ":" + sServantName + " config catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

bool DCacheOptImp::hasServer(const string&sApp, const string &sServerName)
{
    string sSql = "select id from t_server_conf where application='" + sApp + "' and server_name='" + sServerName +"'";
    return _mysqlTarsDb.existRecord(sSql);
}

int DCacheOptImp::selectPort(TC_Mysql &tcMysql, const string &sServerName, const string &sIp, string &sBinLogPort, string &sCachePort, string &sRouterPort,string &sBackUpPort,string &sWCachePort,string &sControlAckPort, std::string &errmsg)
{
    try
    {
        string sSql = "select binlog_port,cache_port,routerclient_port,backup_port,wcache_port,controlack_port from t_router_server where server_name = '" + sServerName +"' and ip = '"+ sIp + "'";
        TC_Mysql::MysqlData data = tcMysql.queryRecord(sSql);
        if (data.size() != 1)
        {
            errmsg = string("get port from t_router_server error|server name:") + sServerName + "|ip:" + sIp;
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
        }

        sBinLogPort     = data[0]["binlog_port"];
        sCachePort      = data[0]["cache_port"];
        sRouterPort     = data[0]["routerclient_port"];
        sBackUpPort     = data[0]["backup_port"];
        sWCachePort     = data[0]["wcache_port"];
        sControlAckPort = data[0]["controlack_port"];
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get port from t_router_server catch exception|server name:") + sServerName + "|ip:" + sIp + "|exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::checkDb(TC_Mysql &tcMysql, const string &sDbName)
{
    try
    {
        string sSql = "show databases";

        TC_Mysql::MysqlData data = tcMysql.queryRecord(sSql);
        for (size_t i = 0; i < data.size(); i++)
        {
            if (data[i]["Database"] == sDbName)
            {
                return 1;
            }
        }
    }
    catch(const std::exception &ex)
    {
        string sErr = string("check db: ") + sDbName + " error, catch exception: " + ex.what();
        TLOGERROR(sErr << endl);
        throw DCacheOptException(sErr);
    }

    return 0;
}

int DCacheOptImp::checkTable(TC_Mysql &tcMysql, const string &sDbName, const string &sTableName)
{
    try
    {
        string sSql = "show tables from " + sDbName;
        TC_Mysql::MysqlData data = tcMysql.queryRecord(sSql);

        string sField = "Tables_in_" + sDbName;
        for (size_t i = 0; i < data.size(); i++)
        {
            if (data[i][sField] == sTableName)
            {
                return 1;
            }
        }
    }
    catch(const std::exception &ex)
    {
        string sErr = string("check table: ") + sTableName + " from db:" + sDbName + " error, exception: " + ex.what();
        TLOGERROR(FUN_LOG << sErr << endl);
        throw DCacheOptException(sErr);
    }

    return 0;
}

bool DCacheOptImp::checkIP(const string &sIp)
{
    //检查ip是否合法
    vector<string> vDigit;
    vDigit = TC_Common::sepstr<string>(sIp, ".",false);
    if ( 4 == vDigit.size())
    {
        //检查是否为数字
        if (!(TC_Common::isdigit(vDigit[0])
            && TC_Common::isdigit(vDigit[1])
            && TC_Common::isdigit(vDigit[2])
            && TC_Common::isdigit(vDigit[3])))
        {
            TLOGERROR(FUN_LOG << "ip format error in GetPort(),some nondigit exists|" <<sIp << endl);
            return false;
        }

        //检测每段数据范围是否合法
        unsigned iOne    = TC_Common::strto<unsigned>(vDigit[0]);
        unsigned iTwo    = TC_Common::strto<unsigned>(vDigit[1]);
        unsigned iThree  = TC_Common::strto<unsigned>(vDigit[2]);
        unsigned iFour   = TC_Common::strto<unsigned>(vDigit[3]);
        if (!((iOne <= 255 && iOne >=0)
            && (iTwo <= 255 && iTwo >=0)
            && (iThree <= 255 && iThree >=0)
            && (iFour <= 255 && iFour >=0)))
        {
            TLOGERROR(FUN_LOG << "ip format error in GetPort(), digit scope error" << endl);
            return false;
        }
    }
    else
    {
        TLOGERROR(FUN_LOG << "ip format error in GetPort(), the size of splited ip is  " << vDigit.size() << endl);
        return false;
    }

    return true;
}

uint16_t DCacheOptImp::getPort(const string &sIp, uint16_t iPort)
{
    if (iPort == 0)
    {
        iPort = getRand();
    }

    while(true)
    {
        if (!checkIP(sIp))
        {
            return 0;
        }

        int s,flags,retcode;
        struct sockaddr_in  peer;

        s = socket(AF_INET, SOCK_STREAM, 0);
        if ((flags = fcntl(s, F_GETFL, 0)) < 0)
        {
            continue;
        }

        if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            continue;
        }

        peer.sin_family      = AF_INET;
        peer.sin_port        = htons(iPort);
        peer.sin_addr.s_addr=inet_addr(sIp.c_str());

        retcode = connect(s, (struct sockaddr*)&peer, sizeof(peer));
        if (retcode == 0)
        {
            close(s);
            continue;
        }

        fd_set rdevents,wrevents,exevents;

        FD_ZERO(&rdevents);
        FD_SET(s, &rdevents);

        wrevents = rdevents;
        exevents = rdevents;
        struct timeval tv;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        retcode = select(s+1, &rdevents, &wrevents, &exevents, &tv);

        if (retcode < 0)
        {
            close(s);
            break;
        }
        else if (retcode == 0)
        {
            close(s);
            break;
        }
        else
        {
            int err;
            socklen_t errlen = sizeof(err);
            getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &errlen);
            if (err)
            {
                close(s);
                break;
            }
        }
        close(s);

        iPort = getRand();
    }

    return iPort;
}

uint16_t DCacheOptImp::getRand()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);

    srand(tv.tv_usec);
    return (rand() % 20000) + 10000;
}

void DCacheOptImp::insertProxyRouter(const string &sProxyName, const string &sRouterName, const string &sDbName,const string &sIp, const string& sModuleNameList, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > mpProxyRouter;
        mpProxyRouter["proxy_name"]     = make_pair(TC_Mysql::DB_STR, sProxyName);
        mpProxyRouter["router_name"]    = make_pair(TC_Mysql::DB_STR, sRouterName);
        mpProxyRouter["db_name"]        = make_pair(TC_Mysql::DB_STR, sDbName);
        mpProxyRouter["db_ip"]          = make_pair(TC_Mysql::DB_STR, sIp);

        string sql = "select * from t_proxy_router where proxy_name='" + sProxyName + "' and router_name='" + sRouterName + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sql);
        string mnlist = sModuleNameList;
        if (data.size() == 1)
        {
            vector<string> vtModuleNameList;
            vtModuleNameList = TC_Common::sepstr<string>(data[0]["modulename_list"] + ";" + sModuleNameList, ";");
            mnlist = "";
            for (size_t i = 0; i < vtModuleNameList.size(); ++i)
            {
                string tmp = vtModuleNameList[i];
                mnlist += tmp + string(";");
                vtModuleNameList.erase(vtModuleNameList.begin() + i);

                // 删除相同的模块名
                while(true)
                {
                    vector<string>::iterator it = find(vtModuleNameList.begin(), vtModuleNameList.end(), tmp);
                    if (it == vtModuleNameList.end())
                    {
                        break;
                    }
                    vtModuleNameList.erase(it);
                }
            }
        }

        mpProxyRouter["modulename_list"] = make_pair(TC_Mysql::DB_STR, mnlist);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_proxy_router", mpProxyRouter);
        else
            _mysqlRelationDB.insertRecord("t_proxy_router", mpProxyRouter);
    }
    catch (exception &ex)
    {
        errmsg = string("insert proxy-router info catch exception:") + string(ex.what());
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }
}

int DCacheOptImp::insertCacheRouter(const string &sCacheName, const string &sCacheIp, int memSize, const string &sRouterName, const RouterDbInfo &routerDBInfo, const string& sModuleName, bool bReplace, string & errmsg)
{
    TLOGDEBUG(FUN_LOG << "cache server name:" << sCacheName << "|router server name:" << sRouterName << endl);

    vector<string> vRouterName = TC_Common::sepstr<string>(sRouterName, ".");
    if (vRouterName.size() != 3)
    {
        errmsg = "router server name is invalid, router name:" + sRouterName;
        TLOGERROR(FUN_LOG << errmsg << endl);
        return -1;
    }

    string sSql = "select * from t_router_app where router_name='" + vRouterName[0] + "." + vRouterName[1] + "'";
    TC_Mysql::MysqlData appNameData = _mysqlRelationDB.queryRecord(sSql);
    if (appNameData.size() != 1)
    {
        errmsg = string("select from t_router_app result data size:") + TC_Common::tostr(appNameData.size()) + "no equal to 1.|router name:" + vRouterName[0] + "." + vRouterName[1];
        TLOGERROR(FUN_LOG << errmsg << endl);
        return -1;
    }

    map<string, pair<TC_Mysql::FT, string> > mpProxyRouter;
    try
    {
        TC_Mysql tRouterDb(routerDBInfo.sDbIp, routerDBInfo.sUserName, routerDBInfo.sPwd, routerDBInfo.sDbName, routerDBInfo.sCharSet, TC_Common::strto<int>(routerDBInfo.sPort));
        sSql = "select * from t_router_group where server_name='DCache." + sCacheName + "'";
        TC_Mysql::MysqlData groupInfo = tRouterDb.queryRecord(sSql);
        if (groupInfo.size() != 1)
        {
            errmsg = string("select from t_router_group result data size:") + TC_Common::tostr(groupInfo.size()) + " no equal to 1.|cache server name:" + "DCache." + sCacheName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }


        sSql = "select idc_area from t_router_server where server_name='DCache." + sCacheName + "'";
        TC_Mysql::MysqlData serverInfo = tRouterDb.queryRecord(sSql);
        if (serverInfo.size() != 1)
        {
            errmsg = string("select from t_router_server result data size:") + TC_Common::tostr(serverInfo.size()) + " no equal to 1.|cache server name:" + "DCache." + sCacheName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        mpProxyRouter["cache_name"]     = make_pair(TC_Mysql::DB_STR, sCacheName);
        mpProxyRouter["cache_ip"]       = make_pair(TC_Mysql::DB_STR, sCacheIp);
        mpProxyRouter["router_name"]    = make_pair(TC_Mysql::DB_STR, sRouterName);
        mpProxyRouter["db_name"]        = make_pair(TC_Mysql::DB_STR, routerDBInfo.sDbName);
        mpProxyRouter["db_ip"]          = make_pair(TC_Mysql::DB_STR, routerDBInfo.sDbIp);
        mpProxyRouter["db_port"]        = make_pair(TC_Mysql::DB_STR, routerDBInfo.sPort);
        mpProxyRouter["db_user"]        = make_pair(TC_Mysql::DB_STR, routerDBInfo.sUserName);
        mpProxyRouter["db_pwd"]         = make_pair(TC_Mysql::DB_STR, routerDBInfo.sPwd);
        mpProxyRouter["db_charset"]     = make_pair(TC_Mysql::DB_STR, routerDBInfo.sCharSet);
        mpProxyRouter["module_name"]    = make_pair(TC_Mysql::DB_STR, sModuleName);
        mpProxyRouter["app_name"]       = make_pair(TC_Mysql::DB_STR, appNameData[0]["app_name"]);
        mpProxyRouter["idc_area"]       = make_pair(TC_Mysql::DB_STR, serverInfo[0]["idc_area"]);
        mpProxyRouter["group_name"]     = make_pair(TC_Mysql::DB_STR, groupInfo[0]["group_name"]);
        mpProxyRouter["server_status"]  = make_pair(TC_Mysql::DB_STR, groupInfo[0]["server_status"]);
        mpProxyRouter["priority"]       = make_pair(TC_Mysql::DB_STR, groupInfo[0]["priority"]);
        mpProxyRouter["mem_size"]       = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(memSize));

        _mysqlRelationDB.replaceRecord("t_cache_router", mpProxyRouter);
    }
    catch(exception &ex)
    {
        errmsg = string("query router db catch exception|db host:") + routerDBInfo.sDbIp + "|db name:"+ routerDBInfo.sDbName + "|port:" + routerDBInfo.sPort + "|exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

void DCacheOptImp::insertProxyCache(const string &sProxyObj, const string & sCacheName, bool bReplace)
{
    map<string, pair<TC_Mysql::FT, string> > mpProxyCache;
    mpProxyCache["proxy_name"]     = make_pair(TC_Mysql::DB_STR, sProxyObj);
    mpProxyCache["cache_name"]     = make_pair(TC_Mysql::DB_STR, sCacheName);

    if (bReplace)
        _mysqlRelationDB.replaceRecord("t_proxy_cache", mpProxyCache);
    else
        _mysqlRelationDB.insertRecord("t_proxy_cache", mpProxyCache);
}

/*int DCacheOptImp::getRouterDBFromAppTable(const string &appName, RouterDbInfo &routerDbInfo, string & sProxyName, string & sRouterName, string &errmsg)
{
    errmsg = "";
    string sSql ("");
    sSql = "select * from t_proxy_app where app_name='" + appName + "'";

    TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);

    if (data.size() > 0)
    {
        sProxyName = data[0]["proxy_name"];

        sSql = "select * from t_router_app where app_name='" + appName + "'";
        data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            sRouterName = data[0]["router_name"];
            routerDbInfo.sDbIp= data[0]["host"];
            routerDbInfo.sDbName = data[0]["db_name"];
            routerDbInfo.sPort = data[0]["port"];
            routerDbInfo.sUserName = data[0]["user"];
            routerDbInfo.sPwd = data[0]["password"];
            routerDbInfo.sCharSet = data[0]["charset"];
            return 0;
        }
        else
        {
            errmsg = string("no router selected for app name:") + appName;
            TLOGERROR(FUN_LOG << "error:" << errmsg << "|sql:" << sSql << endl);
        }
    }
    else
    {
        errmsg = string("no proxy selected for app name:") + appName;
        TLOGERROR(FUN_LOG << "error:" << errmsg << endl);
    }

    return -1;
}*/

int DCacheOptImp::getRouterDBFromAppTable(const string &appName, RouterDbInfo &routerDbInfo, vector<string> & sProxyName, string & sRouterName, string &errmsg)
{
    try
    {
        errmsg = "";
        string sSql("");

        sSql = "select * from t_proxy_app where app_name='" + appName + "'";

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);

        if (data.size() > 0)
        {
            for (size_t i = 0; i < data.size(); i++)
            {
                sProxyName.push_back(data[i]["proxy_name"]);
            }

            sSql = "select * from t_router_app where app_name='" + appName + "'";
            data = _mysqlRelationDB.queryRecord(sSql);
            if (data.size() > 0)
            {
                sRouterName             = data[0]["router_name"];
                routerDbInfo.sDbIp      = data[0]["host"];
                routerDbInfo.sDbName    = data[0]["db_name"];
                routerDbInfo.sPort      = data[0]["port"];
                routerDbInfo.sUserName  = data[0]["user"];
                routerDbInfo.sPwd       = data[0]["password"];
                routerDbInfo.sCharSet   = data[0]["charset"];

                return 0;
            }
            else
            {
                errmsg = string("select from table t_router_app no find app name:") + appName;
                TLOGERROR(FUN_LOG << errmsg << endl);
            }
        }
        else
        {
            errmsg = string("select from table t_proxy_app no find app name:") + appName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch(exception &ex)
    {
        errmsg = string("select from relation db catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = "select from relation db catch unknown exception";
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::insertRelationAppTable(const string &appName, const string &proxyServer,const string &routerServer,const DCache::RouterParam & stRouter, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["proxy_name"] = make_pair(TC_Mysql::DB_STR, proxyServer);
        m["app_name"]   = make_pair(TC_Mysql::DB_STR, appName);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_proxy_app", m);
        else
            _mysqlRelationDB.insertRecord("t_proxy_app", m);

        map<string, pair<TC_Mysql::FT, string> > n;
        n["router_name"] = make_pair(TC_Mysql::DB_STR, routerServer);
        n["app_name"]    = make_pair(TC_Mysql::DB_STR, appName);
        n["host"]        = make_pair(TC_Mysql::DB_STR, stRouter.dbIp);
        n["port"]        = make_pair(TC_Mysql::DB_STR, stRouter.dbPort);
        n["user"]        = make_pair(TC_Mysql::DB_STR, stRouter.dbUser);
        n["password"]    = make_pair(TC_Mysql::DB_STR, stRouter.dbPwd);
        n["db_name"]     = make_pair(TC_Mysql::DB_STR, stRouter.dbName);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_router_app", n);
        else
            _mysqlRelationDB.insertRecord("t_router_app", n);
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert relation info catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

/*
*   expand/transfer
*/
int DCacheOptImp::expandCacheConf(const string &appName, const string &moduleName, const vector<DCache::CacheHostParam> & vtCacheHost, bool bReplace, string & errmsg)
{
    int iAppConfigId = 0;

    getAppModConfigId(appName, moduleName, iAppConfigId, errmsg);

    for (size_t i = 0; i < vtCacheHost.size(); ++i)
    {
        vector < map <string, string> > level3Vec;

        string shmSize = TC_Common::tostr(vtCacheHost[i].shmSize);

        insertConfigItem2Vector("ShmSize", "Main/Cache",  shmSize + "G", level3Vec);

        if (!vtCacheHost[i].shmKey.empty())
        {
            insertConfigItem2Vector("ShmKey", "Main/Cache", TC_Common::tostr(vtCacheHost[i].shmKey), level3Vec);
        }

        if (insertConfigFilesTable(vtCacheHost[i].serverName, vtCacheHost[i].serverIp, 0, level3Vec, 2, bReplace, errmsg) != 0)
            return -1;

        if (insertReferenceTable(iAppConfigId, vtCacheHost[i].serverName, vtCacheHost[i].serverIp, bReplace, errmsg) != 0)
            return -1;

    }

    return 0;
}

int DCacheOptImp::getAppModConfigId(const string &appName, const string &moudleName, int &id, string & errmsg)
{
    try
    {
        string sSql = "select id from t_config_appMod where appName = '" + appName + "' and moduleName = '" + moudleName + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() != 1)
        {
            errmsg = string("get id from t_config_appMod error, appName:") + appName + ", moudleName:" + moudleName + ", size:" + TC_Common::tostr<int>(data.size()) + " not equal to 1.";
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
            return -1;
        }

        id = TC_Common::strto<int>(data[0]["id"]);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get id from t_config_appMod appName:") + appName + ", moudleName:" + moudleName + ", catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

int DCacheOptImp::expandCache2RouterDb(const string &sModuleName, const string& sRouterDbName, const string& sRouterDbHost, const string& sRouterDbPort, const string &sRouterDbUser, const string &sRouterDbPwd, const vector<DCache::CacheHostParam> & vtCacheHost, bool bReplace, string& errmsg)
{
    TC_Mysql tcMysql;
    try
    {
        TLOGDEBUG(FUN_LOG << "router db name:" << sRouterDbName << "|host:" << sRouterDbHost << "|port:" << sRouterDbPort << endl);

        tcMysql.init(sRouterDbHost, sRouterDbUser, sRouterDbPwd, sRouterDbName, "utf8", TC_Common::strto<int>(sRouterDbPort));

        uint16_t iPort = getPort(vtCacheHost[0].serverIp);
        if (0 == iPort)
        {
            //获取PORT失败
            errmsg = "failed to get port for cache server ip:" + vtCacheHost[0].serverIp;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        set<string> setAddr;
        for (size_t i = 0; i < vtCacheHost.size(); ++i)
        {
            //save cache server info to routerdb.t_router_server
            map<string, pair<TC_Mysql::FT, string> > mpServer;
            mpServer["server_name"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
            mpServer["ip"]          = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverIp);

            uint16_t iBinLogPort = iPort;
            while(true)
            {
                iBinLogPort = getPort(vtCacheHost[i].serverIp, iBinLogPort);
                if (iBinLogPort == 0)
                {
                    errmsg = "failed to get port for binlog obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBinLogPort)) != setAddr.end())
                {
                    iBinLogPort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBinLogPort));
            mpServer["binlog_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iBinLogPort));

            uint16_t iCachePort = iPort + 1;
            while(true)
            {
                iCachePort = getPort(vtCacheHost[i].serverIp, iCachePort);
                if (iCachePort == 0)
                {
                    errmsg = "failed to get port for cache obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iCachePort)) != setAddr.end())
                {
                    iCachePort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iCachePort));
            mpServer["cache_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iCachePort));

            uint16_t iRouterPort = iPort + 2;
            while(true)
            {
                iRouterPort = getPort(vtCacheHost[i].serverIp, iRouterPort);
                if (iRouterPort == 0)
                {
                    errmsg = "failed to get port for router client obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iRouterPort)) != setAddr.end())
                {
                    iRouterPort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iRouterPort));
            mpServer["routerclient_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iRouterPort));

            uint16_t iBackUpPort = iPort + 3;
            while(true)
            {
                iBackUpPort = getPort(vtCacheHost[i].serverIp, iBackUpPort);
                if (iBackUpPort == 0)
                {
                    errmsg = "failed to get port for backup obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBackUpPort)) != setAddr.end())
                {
                    iBackUpPort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBackUpPort));
            mpServer["backup_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iBackUpPort));

            uint16_t iWCachePort = iPort + 4;
            while(true)
            {
                iWCachePort = getPort(vtCacheHost[i].serverIp, iWCachePort);
                if (iWCachePort == 0)
                {
                    errmsg = "failed to get port for wcache obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iWCachePort)) != setAddr.end())
                {
                    iWCachePort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iWCachePort));
            mpServer["wcache_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iWCachePort));

            uint16_t iControlAckPort = iPort + 5;
            while(true)
            {
                iControlAckPort = getPort(vtCacheHost[i].serverIp, iControlAckPort);
                if (iControlAckPort == 0)
                {
                    errmsg = "failed to get port for control ack obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iControlAckPort)) != setAddr.end())
                {
                    iControlAckPort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iControlAckPort));
            mpServer["controlack_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iControlAckPort));

            mpServer["idc_area"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].idc);
            mpServer["POSTTIME"] = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpServer["LASTUSER"] = make_pair(TC_Mysql::DB_STR, "sys");

            if (tcMysql.getRecordCount("t_router_server", "where server_name='"+vtCacheHost[i].serverName+"'")==0)
            {
                if (bReplace)
                    tcMysql.replaceRecord("t_router_server", mpServer);
                else
                    tcMysql.insertRecord("t_router_server", mpServer);
            }

            //save cache server info to routerdb.t_router_group
            //避免主机重复
            if (!bReplace && vtCacheHost[i].type == "M" && tcMysql.getRecordCount("t_router_group", "where server_status='M' AND module_name='" + sModuleName + "' AND group_name='" + vtCacheHost[i].groupName + "' AND server_status='M'") > 0)
            {
                errmsg = string("module_name:") + sModuleName + ", group_name:" + vtCacheHost[i].groupName + " has master cache server|server name:" + vtCacheHost[i].serverName;
                TLOGERROR(FUN_LOG << errmsg<< endl);
                return -1;
            }

            string sSql = "select * from t_router_group where module_name='" + sModuleName + "' AND group_name='" + vtCacheHost[i].groupName + "' AND server_name='" + vtCacheHost[i].serverName + "'";
            TC_Mysql::MysqlData existRouterGroup = tcMysql.queryRecord(sSql);
            if (existRouterGroup.size() > 0)
            {
                if (existRouterGroup[0]["source_server_name"] != vtCacheHost[i].bakSrcServerName)
                {
                    errmsg = string("source_server_name incosistent-param:") + vtCacheHost[i].bakSrcServerName + "|database:" + existRouterGroup[0]["source_server_name"];
                    TLOGERROR(FUN_LOG << errmsg<< endl);
                    return -1;
                }
                else
                {
                    continue;
                }
            }

            map<string, pair<TC_Mysql::FT, string> > mpGroup;
            mpGroup["module_name"]   = make_pair(TC_Mysql::DB_STR, sModuleName);
            mpGroup["group_name"]    = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].groupName);
            mpGroup["server_name"]   = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
            mpGroup["server_status"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].type);
            mpGroup["priority"]      = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].priority);
            mpGroup["source_server_name"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].bakSrcServerName);
            mpGroup["POSTTIME"]      = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpGroup["LASTUSER"]      = make_pair(TC_Mysql::DB_STR, "sys");

            if (bReplace)
                tcMysql.replaceRecord("t_router_group", mpGroup);
            else
                tcMysql.insertRecord("t_router_group", mpGroup);
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("expand module name:") + sModuleName + " to router db's table catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        tcMysql.disconnect();
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::getRouterDBInfoAndObj(const string &sWhereName, RouterDbInfo &routerDbInfo, string &routerObj, string & errmsg, int type)
{
    errmsg = "";
    string sSql ("");
    if (type == 1)
    {
        sSql = "select * from t_cache_router where app_name='" + sWhereName + "'";
    }
    else
    {
        sSql = "select * from t_cache_router where cache_name='" + sWhereName + "'";
    }
    TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);

    if (data.size() > 0)
    {
        routerDbInfo.sDbIp      = data[0]["db_ip"];
        routerDbInfo.sDbName    = data[0]["db_name"];
        routerDbInfo.sPort      = data[0]["db_port"];
        routerDbInfo.sUserName  = data[0]["db_user"];
        routerDbInfo.sPwd       = data[0]["db_pwd"];
        routerDbInfo.sCharSet   = data[0]["db_charset"];
        routerObj               = data[0]["router_name"];

        return 0;
    }
    else
    {
        errmsg = "not find router info in t_cache_router";
        TLOGERROR(FUN_LOG << errmsg << "|sql:" << sSql << endl);
    }

    return -1;
}

int DCacheOptImp::expandDCacheServer(const std::string & appName,const std::string & moduleName,const vector<DCache::CacheHostParam> & vtCacheHost,const std::string & sTarsVersion,DCache::DCacheType cacheType, bool bReplace,std::string &err)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "appName:" << appName << "|moduleName:" << moduleName << "|cacheType:" << cacheType << "|replace:" << bReplace << endl);

        if (cacheType != DCache::KVCACHE && cacheType != DCache::MKVCACHE)
        {
            err = "expand type error: KVCACHE-1 or MKVCACHE-2";
            return -1;
        }

        //初始化out变量
        err = "success";

        if (vtCacheHost.size() == 0)
        {
            err = "no cache server specified,please make sure params is right";
            return -1;
        }

        /*RouterDbInfo routerDbInfo;
        string sRouterObjName;
        int iRet = getRouterDBInfoAndObj(expandDCacheReq.appName, routerDbInfo, sRouterObjName, err);
        if (iRet == -1)
        {
            return -1;
        }*/

        RouterDbInfo routerDbInfo;
        vector<string> vtProxyName;
        string routerServerName;
        int iRet = getRouterDBFromAppTable(appName, routerDbInfo, vtProxyName, routerServerName, err);
        if (0 != iRet)
        {
            return -1;
        }

        if (expandCacheConf(appName, moduleName, vtCacheHost, bReplace, err) != 0)
        {
            err = string("failed to expand cache server conf") + err;
            return -1;
        }

        if (expandCache2RouterDb(moduleName, routerDbInfo.sDbName, routerDbInfo.sDbIp, routerDbInfo.sPort, routerDbInfo.sUserName, routerDbInfo.sPwd, vtCacheHost, bReplace, err) != 0)
        {
            return -1;
        }

        iRet = insertCache2TarsDb(routerDbInfo.sDbName, routerDbInfo.sDbIp, routerDbInfo.sPort, routerDbInfo.sUserName, routerDbInfo.sPwd, vtCacheHost, cacheType, sTarsVersion, bReplace, err);
        if (iRet != 0)
        {
            err = "failed to insert catch info to tars db, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        try
        {
            RouterDbInfo tmpRouterDbInfo;
            tmpRouterDbInfo = routerDbInfo;
            for (size_t i = 0; i < vtCacheHost.size(); ++i)
            {
                for (size_t j = 0; j < vtProxyName.size(); j++)
                {
                    // 保存 proxy server和cache server的对应关系
                    insertProxyCache(vtProxyName[j].substr(7), vtCacheHost[i].serverName, bReplace);
                }

                // 保存cache serve和router server的对应关系
                insertCacheRouter(vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, int(TC_Common::strto<float>(vtCacheHost[i].shmSize)*1024), routerServerName + ".RouterObj", tmpRouterDbInfo, moduleName, bReplace, err);
            }
        }
        catch(exception &ex)
        {
            //这里捕捉异常以不影响安装结果
            err = string("option relation db catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << err << endl);
            return 0;
        }
    }
    catch(const std::exception &ex)
    {
        err = TC_Common::tostr(__FUNCTION__) + string("catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << err << endl);
        return -1;
    }

    return 0;
}


