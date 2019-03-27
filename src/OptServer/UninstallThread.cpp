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
#include "UninstallThread.h"

UninstallRequestQueueManager::UninstallRequestQueueManager()
{
}

UninstallRequestQueueManager::~UninstallRequestQueueManager()
{
}

void UninstallRequestQueueManager::start(int iThreadNum)
{
    for (int i = 0; i < iThreadNum; i++)
    {
        UninstallThread * t = new UninstallThread(this);
        t->start();

        _uninstallRunners.push_back(t);
    }
}

void UninstallRequestQueueManager::setTarsDbConf(TC_DBConf &dbConf)
{
    _dbConf = dbConf;
    for (size_t i = 0; i < _uninstallRunners.size(); ++i)
    {
        _uninstallRunners[i]->setTarsDbMysql(_dbConf);
    }
}

void UninstallRequestQueueManager::setRelationDbMysql(TC_DBConf &dbConf)
{
    _dbConf = dbConf;
    for (size_t i = 0; i < _uninstallRunners.size(); ++i )
    {
        _uninstallRunners[i]->setRelationDbMysql(_dbConf);
    }
}

void UninstallRequestQueueManager::terminate()
{
    for (size_t i = 0; i < _uninstallRunners.size(); ++i)
    {
        if (_uninstallRunners[i]->isAlive())
        {
            _uninstallRunners[i]->terminate();
        }
    }

    for (size_t i = 0; i < _uninstallRunners.size(); ++i)
    {
        if (_uninstallRunners[i]->isAlive())
        {
            _uninstallRunners[i]->getThreadControl().join();
        }
    }
}

void UninstallRequestQueueManager::push_back(const UninstallRequest & request)
{
    TC_ThreadLock::Lock lock(_requestMutex);

    if (1 == _uninstalling.count(request.requestId))
    {
        std::string sException = "reduplicate uninstall request:" + request.requestId;

        TLOGERROR(FUN_LOG << "reduplicate uninstall request id:" << request.requestId << "|exception:" << sException << endl);
        throw TC_Exception(sException);
    }

    _requestQueue.push_back(request);
    _uninstalling.insert(request.requestId);
}

bool UninstallRequestQueueManager::pop_front(UninstallRequest &request)
{
    TC_ThreadLock::Lock lock(_requestMutex);

    if (_requestQueue.empty())
    {
        return false;
    }

    request = _requestQueue.front();
    _requestQueue.pop_front();
    _uninstalling.erase(request.requestId);

    return true;
}

void UninstallRequestQueueManager::addUninstallRecord(const string &sRequestId)
{
    TC_ThreadLock::Lock lock(_progressMutex);
    map<string, UninstallStatus>::iterator it = _uninstallingProgress.find(sRequestId);
    if (it != _uninstallingProgress.end())
    {
        TLOGERROR(FUN_LOG << "request id:" << sRequestId << " is uninstalling" << endl);
        return;
    }

    UninstallStatus currentStatus;
    currentStatus.sPercent = "0";
    currentStatus.status = UNINSTALLING;
    currentStatus.sError = "";
    _uninstallingProgress[sRequestId] = currentStatus;
}

void UninstallRequestQueueManager::setUninstallRecord(const string &sRequestId, const string &sPercent, UnStatus status, const string &sError)
{
    TC_ThreadLock::Lock lock(_progressMutex);

    map<string, UninstallStatus>::iterator it = _uninstallingProgress.find(sRequestId);
    if (it == _uninstallingProgress.end())
    {
        TLOGERROR(FUN_LOG << "request id:" << sRequestId << " no found" << endl);
        return;
    }

    it->second.sPercent = sPercent;
    it->second.status = status;
    it->second.sError = sError;
}

UninstallStatus UninstallRequestQueueManager::getUninstallRecord(const string & sRequestId)
{
    TC_ThreadLock::Lock lock(_progressMutex);

    map<string, UninstallStatus>::iterator it = _uninstallingProgress.find(sRequestId);
    if (it == _uninstallingProgress.end())
    {
        TLOGERROR(FUN_LOG << "request id:" << sRequestId << " no found" << endl);
        throw runtime_error("request id:" + sRequestId + " no found");
    }

    return it->second;
}

void UninstallRequestQueueManager::deleteUninstallRecord(const string &sRequestId)
{
    TC_ThreadLock::Lock lock(_progressMutex);

    map<string, UninstallStatus>::iterator it = _uninstallingProgress.find(sRequestId);
    if (it == _uninstallingProgress.end())
    {
        TLOGDEBUG(FUN_LOG << "request has been deleted|request id:" << sRequestId << endl);
        return;
    }

    _uninstallingProgress.erase(sRequestId);
}

size_t UninstallRequestQueueManager::getUnistallQueueSize()
{
    TC_ThreadLock::Lock lock(_requestMutex);
    return _requestQueue.size();
}

void UninstallRequestQueueManager::setCacheBackupPath(const string & sCacheBakPath)
{
    _cacheBakPath = sCacheBakPath;
    for (size_t i = 0; i < _uninstallRunners.size(); ++i)
    {
        _uninstallRunners[i]->setCacheBackupPath(_cacheBakPath);
    }
}

UninstallThread::UninstallThread(UninstallRequestQueueManager* urqm)
: _queueManager(urqm), _shutDown(false)
{
    _communicator = Application::getCommunicator();

    TC_Config tcConf;
    tcConf.parseFile(ServerConfig::BasePath + "DCacheOptServer.conf");

    _adminproxy = _communicator->stringToProxy<AdminRegPrx>(tcConf.get("/Main/<AdminRegObj>", "tars.tarsAdminRegistry.AdminRegObj"));
}

UninstallThread::~UninstallThread()
{}

void UninstallThread::run()
{
    while (!_shutDown)
    {
        try
        {
            if (_queueManager->getUnistallQueueSize() == 0)
            {
                sleep(1);
                continue;
            }

            UninstallRequest request;
            if (_queueManager->pop_front(request))
            {
                doUninstallRequest(request);
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
}

void UninstallThread::terminate()
{
    _shutDown = false;
}

void UninstallThread::setTarsDbMysql(TC_DBConf &dbConf)
{
    TLOGDEBUG(FUN_LOG << "db host:" <<  dbConf._host << "|db user:" << dbConf._user << "|db password:"  << dbConf._password << endl);
    _mysqlTarsDb.init(dbConf);
}

void UninstallThread::setRelationDbMysql(TC_DBConf &dbConf)
{
    TLOGDEBUG(FUN_LOG << "db host:" <<  dbConf._host << "|db user:" << dbConf._user << "|db password:"  << dbConf._password << endl);
    _mysqlRelationDb.init(dbConf);
}

void UninstallThread::setCacheBackupPath(const string & sCacheBakPath)
{
    _cacheBakPath = sCacheBakPath;
}

void UninstallThread::doUninstallRequest(UninstallRequest & request)
{
    TLOGDEBUG(FUN_LOG << "request id:" << request.requestId << "|request type:" << request.info.unType<< endl);
    string sError("");
    try
    {
        UninstallInfo & uninstallInfo = request.info;

        TC_Mysql mysqlRouterDb(uninstallInfo.routerDb.ip, uninstallInfo.routerDb.user, uninstallInfo.routerDb.pwd, uninstallInfo.routerDb.dbName, uninstallInfo.routerDb.charset, TC_Common::strto<int>(uninstallInfo.routerDb.port));
        if (DCache::MODULE == uninstallInfo.unType || DCache::GROUP ==  uninstallInfo.unType)
        {
            TC_Mysql::MysqlData cacheData;
            string sQuerySql;

            if (DCache::MODULE == uninstallInfo.unType)
            {
                //按模块
                string sWhere = "where module_name='" + uninstallInfo.moduleName + "'";
                sQuerySql = "select server_name from t_router_group " + sWhere;
            }
            else
            {
                //按组
                sQuerySql = "select server_name from t_router_group where group_name='" + uninstallInfo.groupName + "'";
            }

            cacheData = mysqlRouterDb.queryRecord(sQuerySql);
            int iUnit = 0;
            if (cacheData.size() > 0)
            {
                iUnit = 100 / cacheData.size();
            }
            else
            {
                _queueManager->setUninstallRecord(request.requestId, "100%", FINISH);
            }

            int iPercent = 0;
            for (size_t i = 0; i < cacheData.size(); ++i)
            {
                //调用tarsnode下线服务
                Tool::UninstallCacheServer(mysqlRouterDb, _mysqlTarsDb, _mysqlRelationDb, cacheData[i]["server_name"], _cacheBakPath);
                iPercent += iUnit;
                if (i == (cacheData.size() - 1))
                {
                    _queueManager->setUninstallRecord(request.requestId, "100%", FINISH);
                    break;
                }

                _queueManager->setUninstallRecord(request.requestId, TC_Common::tostr(iPercent) + "%", UNINSTALLING);
            }
        }
        else if (DCache::SERVER == uninstallInfo.unType)
        {
            //按服务
            string moduleName;
            string routerObj;
            int ret = getRouterInfo(_mysqlRelationDb, uninstallInfo.serverName, moduleName, routerObj);
            if (ret < 0)
            {
                sError = "get router info error|cache server name:" + uninstallInfo.serverName;
                TLOGERROR(FUN_LOG << sError<< endl);

                _queueManager->deleteUninstallRecord(request.requestId);
                return;
            }

            //调用tarsnode下线服务
            Tool::UninstallCacheServer(mysqlRouterDb, _mysqlTarsDb, _mysqlRelationDb, uninstallInfo.serverName, _cacheBakPath);

            _queueManager->setUninstallRecord(request.requestId, "100%", FINISH);
            reloadRouterConfByModuleFromDB(moduleName, routerObj);
        }

        return;
    }
    catch(exception &ex)
    {
        sError = string("catch exception|request id:") + request.requestId + "|exception:" + ex.what();
        TLOGERROR(FUN_LOG << sError << endl);
        _queueManager->deleteUninstallRecord(request.requestId);
    }
    catch(...)
    {
        sError = string("catch unknown exception|request id:") + request.requestId;
        TLOGERROR(FUN_LOG << sError << endl);
        _queueManager->deleteUninstallRecord(request.requestId);
    }
}

int UninstallThread::getRouterInfo(TC_Mysql &mysqlRelationDb,const string &sFullCacheServer, string &moduleName, string &routerObj)
{
    try
    {
        string sSql = "select * from t_cache_router where cache_name='" + sFullCacheServer.substr(7) + "'";

        TC_Mysql::MysqlData data = mysqlRelationDb.queryRecord(sSql);
        if (data.size() != 1)
        {
            TLOGERROR(FUN_LOG << "query from t_cache_router no find cache server name:" << sFullCacheServer << endl);
            return -1;
        }

        moduleName = data[0]["module_name"];
        routerObj  = data[0]["router_name"];
    }
    catch(const TarsException& ex)
    {
        TLOGERROR(FUN_LOG << "operate relation db.t_cache_router catch exception:" << ex.what()<< endl);
        return -1;
    }

    return 0;
}

void UninstallThread::reloadRouterConfByModuleFromDB(const string &moduleName, const string &routerObj)
{
    try
    {
        RouterPrx routerPrx;
        routerPrx = _communicator->stringToProxy<RouterPrx>(routerObj);
        vector<TC_Endpoint> vEndPoints = routerPrx->getEndpoint4All();

        set<string> nodeIPset;
        for (vector<TC_Endpoint>::size_type i = 0; i < vEndPoints.size(); i++)
        {
            //获取所有节点ip
            nodeIPset.insert(vEndPoints[i].getHost());
        }

        set<string>::iterator pos;
        vector<string> routerNameVec = TC_Common::sepstr<string>(routerObj, ".");
        for (pos = nodeIPset.begin(); pos != nodeIPset.end(); ++pos)
        {
            //通知重载加载配置
            string sResult;
            _adminproxy->notifyServer("DCache", routerNameVec[1], *pos, "router.reloadRouterByModule " + moduleName, sResult);
        }

        TLOGDEBUG(FUN_LOG << "notify router server reloadRouterByModule success" << endl);
    }
    catch(const TarsException& ex)
    {
        TLOGERROR(FUN_LOG << "catch exception:" << ex.what() << endl);
    }
}

