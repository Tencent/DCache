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
#include "RouterServer.h"
//#include "EtcdHandle.h"
//#include "EtcdHost.h"
#include "OuterProxyFactory.h"
#include "RouterImp.h"
#include "Transfer.h"
#include "util/tc_clientsocket.h"

using namespace tars;

RouterServer g_app;

namespace
{
std::shared_ptr<AdminRegProxyWrapper> defaultAdminProxyCreatePolicy(const std::string &obj)
{
    return std::shared_ptr<AdminRegProxyWrapper>(new AdminRegProxyWrapper(obj));
}
}  // namespace

std::unique_ptr<MySqlHandlerWrapper> RouterServer::createMySqlHandler(
    const std::map<string, string> &dbConfig) const
{
    auto handler = std::unique_ptr<MySqlHandlerWrapper>(new MySqlHandlerWrapper());
    handler->init(dbConfig);
    return handler;
}

void RouterServer::initialize()
{
    ostringstream os;
    try
    {
        addServant<RouterImp>(ServerConfig::Application + "." + ServerConfig::ServerName +
                              ".RouterObj");
        if (!addConfig(ServerConfig::ServerName + ".conf"))
        {
            TLOGERROR(FILE_FUN << "add config error." << endl);
            exit(-1);
        }
        _conf.init(ServerConfig::BasePath + "/" + ServerConfig::ServerName + ".conf");
        _adminCreatePolicy = std::bind(defaultAdminProxyCreatePolicy,
                                       _conf.getAdminRegObj("tars.tarsregistry.AdminRegObj"));
       if (setUpEtcd() != 0)
       {
           TLOGERROR(FILE_FUN << "set up etcd error" << endl);
           exit(-1);
       }

        _dbHandle->init(_conf.getDbReloadTime(100000),
                        createMySqlHandler(_conf.getDbConnInfo()),
                        createMySqlHandler(_conf.getDbRelationInfo()),
                        createMySqlHandler(_conf.getDbRelationInfo()),
                        createDBPropertyReport());
        _dbHandle->initCheck();
        _outerProxy->init(_conf.getProxyMaxSilentTime(1800));
        _transfer->init(_dbHandle);
        _dbHandle->loadSwitchInfo();

        ADD_ADMIN_CMD_NORMAL("router.reloadRouter", RouterServer::reloadRouter);
        ADD_ADMIN_CMD_NORMAL("router.reloadRouterByModule", RouterServer::reloadRouterByModule);
        ADD_ADMIN_CMD_NORMAL("router.reloadConf", RouterServer::reloadConf);
        ADD_ADMIN_CMD_NORMAL("router.getVersions", RouterServer::getVersions);
        ADD_ADMIN_CMD_NORMAL("router.getRouterInfos", RouterServer::getRouterInfos);
        ADD_ADMIN_CMD_NORMAL("router.getTransferInfos", RouterServer::getTransferInfos);
        ADD_ADMIN_CMD_NORMAL("router.getTransferingInfos", RouterServer::getTransferingInfos);
        ADD_ADMIN_CMD_NORMAL("router.clearTransferInfos", RouterServer::clearTransferInfos);
        ADD_ADMIN_CMD_NORMAL("router.notifyCacheServers", RouterServer::notifyCacheServers);
        ADD_ADMIN_CMD_NORMAL("router.defragRouteRecords", RouterServer::defragRouteRecords);
        ADD_ADMIN_CMD_NORMAL("router.deleteAllProxy", RouterServer::deleteAllProxy);
        ADD_ADMIN_CMD_NORMAL("router.switchByGroup", RouterServer::switchByGroup);
        ADD_ADMIN_CMD_NORMAL("router.switchMirrorByGroup", RouterServer::switchMirrorByGroup);
        ADD_ADMIN_CMD_NORMAL("router.heartBeat", RouterServer::showHeartBeatInfo);
        ADD_ADMIN_CMD_NORMAL("router.resetServerStatus", RouterServer::resetServerStatus);
        ADD_ADMIN_CMD_NORMAL("router.checkModule", RouterServer::checkModule);
        ADD_ADMIN_CMD_NORMAL("help", RouterServer::help);

        //启动线程
        _timerThread.init(_conf, _dbHandle, _outerProxy);
        _timerThread.start();
        _switchThread.init(createAdminRegProxyWrapper(), _dbHandle);
        _switchThread.start();

        TARS_NOTIFY_NORMAL("RouterServer::initialize|Succ|");

        return;
    }
    catch (TC_Config_Exception &e)
    {
        os << "initialize ConfigFile error: " << e.what() << endl;
    }
    catch (TC_ConfigNoParam_Exception &e)
    {
        os << "get domain error: " << e.what() << endl;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGERROR(os.str() << endl);

    TARS_NOTIFY_ERROR("RouterServer::initialize|" + os.str());
    exit(-1);
}

bool RouterServer::reloadRouter(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, reload router fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::reloadRouter|") + os.str());
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            int rc = _dbHandle->reloadRouter();
            if (rc == 0)
            {
                os << "reload router ok!" << endl;
                FDLOG("switch") << "RouterServer::reloadRouter doLoadSwitcInfo() now" << endl;
                TC_ThreadLock::Lock lock(_moduleSwitchingLock);
                if (_moduleSwitching.size() == 0)
                {
                    _dbHandle->loadSwitchInfo();
                    FDLOG("switch") << "RouterServer::reloadRouter doLoadSwitcInfo() end" << endl;
                }
            }
            else if (rc == 1)
            {
                os << "transfering or Switching, cant reload router!" << endl;
                TARS_NOTIFY_ERROR(string("RouterServer::reloadRouter|") + os.str());
            }
            else
            {
                os << "reload router fail!" << endl;
                TARS_NOTIFY_ERROR(string("RouterServer::reloadRouter|") + os.str());
            }
        }
        TLOGDEBUG(os.str() << endl);
        result = os.str();
        return true;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TARS_NOTIFY_ERROR(string("RouterServer::reloadRouter|") + os.str());
    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::reloadRouterByModule(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, reload router by module fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::reloadRouterByModule|") + os.str());
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            TLOGDEBUG("RouterServer::reloadRouterByModule moduleName:" << params << endl);
            int rc = _dbHandle->reloadRouter(params);
            if (rc == 0)
            {
                os << "reload router ok!" << endl;
                FDLOG("switch") << "RouterServer::reloadRouter doLoadSwitcInfo() now" << endl;
                TC_ThreadLock::Lock lock(_moduleSwitchingLock);
                if (_moduleSwitching.size() == 0)
                {
                    _dbHandle->loadSwitchInfo(params);
                    FDLOG("switch") << "RouterServer::reloadRouter doLoadSwitcInfo() end" << endl;
                }
            }
            else if (rc == 1)
            {
                os << "transfering or Switching, cant reload router!" << endl;
                TARS_NOTIFY_ERROR(string("RouterServer::reloadRouter|") + os.str());
            }
            else
            {
                os << "reload router fail!" << endl;
                TARS_NOTIFY_ERROR(string("RouterServer::reloadRouter|") + os.str());
            }
        }
        TLOGDEBUG(os.str() << endl);
        result = os.str();
        return true;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TARS_NOTIFY_ERROR("RouterServer::reloadRouterByModule|" + os.str());
    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::reloadConf(const string &command, const string &params, string &result)
{
    ostringstream os;

    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, reload conf fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::reloadConf|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            addConfig(ServerConfig::ServerName + ".conf");
            _conf.init(ServerConfig::BasePath + "/" + ServerConfig::ServerName + ".conf");

            _outerProxy->reloadConf(_conf);
            _dbHandle->reloadConf(_conf);
            _transfer->reloadConf();

            //先停止定时线程
            _timerThread.terminate();
            _timerThread.getThreadControl().join();

            //重启定时线程
            TLOGDEBUG("restart timer thread ..." << endl);
            _timerThread.init(_conf, _dbHandle, _outerProxy);
            _timerThread.start();

            //先停止切换线程
            _switchThread.terminate();
            _switchThread.getThreadControl().join();

            //重启切换线程
            TLOGDEBUG("restart switch thread ..." << endl);
            _switchThread.init(createAdminRegProxyWrapper(), _dbHandle);
            _switchThread.start();
        }
        os << "reload config ok!" << endl;

        TARS_NOTIFY_NORMAL("RouterServer::reloadConf|Succ|");
        TLOGDEBUG(os.str() << endl);
        result = os.str();
        return true;
    }
    catch (TC_Config_Exception &e)
    {
        os << "initialize ConfigFile error: " << e.what() << endl;
    }
    catch (TC_ConfigNoParam_Exception &e)
    {
        os << "get domain error: " << e.what() << endl;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TARS_NOTIFY_ERROR(string("RouterServer::reloadConf|") + os.str());
    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::getVersions(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, get versions fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::getVersions|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            map<string, int> versions;
            vector<string> names = SEPSTR(params, " ");
            vector<string>::const_iterator vit = find(names.begin(), names.end(), "all");
            if (vit != names.end())
            {
                _dbHandle->getVersion(versions);
            }
            else
            {
                for (vector<string>::const_iterator it = names.begin(); it != names.end(); ++it)
                {
                    versions[*it] = _dbHandle->getVersion(*it);
                }
            }

            for (map<string, int>::const_iterator it = versions.begin(); it != versions.end(); ++it)
            {
                os << it->first << " version: " << it->second << endl;
            }
        }
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::getRouterInfos(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, get router infos fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::getRouterInfos|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            os << "list router infos are: " << endl;
            map<string, PackTable> infos;

            vector<string> names = SEPSTR(params, " ");
            vector<string>::const_iterator vit = find(names.begin(), names.end(), "all");
            if (vit != names.end())
            {
                _dbHandle->getRouterInfos(infos);
            }
            else
            {
                for (vector<string>::const_iterator it = names.begin(); it != names.end(); ++it)
                {
                    PackTable packTable;
                    if (_dbHandle->getPackTable(*it, packTable) == 0)
                    {
                        infos[*it] = packTable;
                    }
                    else
                    {
                        os << *it << "\tnot exist!" << endl;
                    }
                }
            }

            for (map<string, PackTable>::const_iterator it = infos.begin(); it != infos.end(); ++it)
            {
                os << it->first << ": ";
                it->second.display(os, 1);
            }
        }
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::getTransferInfos(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, get transfer infos fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::getTransferInfos|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            map<string, map<string, TransferInfo>> infos;

            _dbHandle->getTransferInfos(infos);

            os << "list transfer infos are: " << endl;
            map<string, map<string, TransferInfo>>::iterator it = infos.begin();
            for (; it != infos.end(); ++it)
            {
                map<string, TransferInfo>::iterator it1 = (it->second).begin();
                for (; it1 != (it->second).end(); ++it1)
                {
                    (it1->second).displaySimple(os);
                    os << endl;
                }
            }
        }
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::getTransferingInfos(const string &command, const string &params, string &result)
{
    TLOGDEBUG("Enter RouterServer::getTransferingInfos" << endl);
    string moduleName = params;
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, get transfering infos fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::getTransferingInfos|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            ModuleTransferingInfo info;

            _dbHandle->getTransferingInfos(moduleName, info);

            os << " transfering : " << moduleName << endl;
            for (unsigned int i = 0; i < info.transferingInfoList.size(); ++i)
            {
                info.transferingInfoList[i].displaySimple(os);
                os << endl;
            }
            os << " transfering version: " << info.version << endl;
            map<string, int>::iterator it1 = info.transferingGroup.begin();
            for (; it1 != info.transferingGroup.end(); ++it1)
            {
                os << "groupname:" << it1->first << " num:" << it1->second << endl;
            }
        }
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::clearTransferInfos(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, clear transfer infos fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::clearTransferInfos|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            _dbHandle->clearTransferInfos();
 
            os << "clear transfer infos return: Succ" << endl;
        }
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::notifyCacheServers(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, notify cache servers fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::notifyCacheServers|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            map<string, vector<string>> succServers, failServers;

            _transfer->notifyAllServer(params, succServers, failServers);

            os << "notifyCacheServers status: " << endl;

            map<string, vector<string>>::const_iterator it;
            for (it = succServers.begin(); it != succServers.end(); ++it)
            {
                os << it->first << ": " << endl;
                os << "\tSucc: " << endl;
                for (unsigned i = 0; i < it->second.size(); ++i)
                {
                    os << "\t\t" << it->second[i] << endl;
                }

                os << "\tFail: " << endl;
                for (unsigned i = 0; i < failServers[it->first].size(); ++i)
                {
                    os << "\t\t" << failServers[it->first][i] << endl;
                }
            }
        }
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::defragRouteRecords(const string &command, const string &params, string &result)
{
    ostringstream os;
    string modules = params;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, defrag route records fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::defragRouteRecords|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            map<string, vector<RecordInfo>> mOldRecord, mNewRecord;

            int rc = _dbHandle->defragDbRecord(modules, mOldRecord, mNewRecord);

            if (rc == 1)
            {
                os << "there are transfers processing, cant defrag." << endl;
            }
            else
            {
                if (rc == -1)
                {
                    os << "defrag module failed: " << modules
                        << ", please check the reason!"
                        << endl;

                    TARS_NOTIFY_ERROR(string("RouterServer::defragRouteRecords|") + os.str());
                }
                if (rc == -2)
                {
                    os << "defrag fininsh but reload failed, "
                        "please check the reason!"
                        << endl;
                    TARS_NOTIFY_ERROR(string("RouterServer::defragRouteRecords|") + os.str());
                }

                os << "following defrag succ: " << endl;
                map<string, vector<RecordInfo>>::const_iterator it;
                for (it = mOldRecord.begin(); it != mOldRecord.end(); ++it)
                {
                    os << it->first << ": " << endl;
                    os << "\tbefore defrag: " << endl;
                    for (unsigned i = 0; i < it->second.size(); ++i)
                    {
                        os << "\t\t";
                        it->second[i].displaySimple(os);
                        os << endl;
                    }
                    os << "\tarster defrag: " << endl;
                    for (unsigned i = 0; i < mNewRecord[it->first].size(); ++i)
                    {
                        os << "\t\t";
                        mNewRecord[it->first][i].displaySimple(os);
                        os << endl;
                    }
                }
            }

            if (rc == 0)
            {
                TARS_NOTIFY_NORMAL("RouterServer::defragRouteRecords|Succ|");
            }
        }

        TLOGDEBUG(os.str() << endl);
        result = os.str();
        return true;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TARS_NOTIFY_ERROR(string("RouterServer::defragRouteRecords|") + os.str());
    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::deleteAllProxy(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, delete all proxy fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::deleteAllProxy|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else
        {
            _outerProxy->deleteAllProxy();
        }
        os << "delete all proxy succ!" << endl;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::switchByGroup(const string &command, const string &params, string &result)
{
    ostringstream os;
    if (getRouterType() != ROUTER_MASTER)
    {
        try
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, switch by group fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::switchByGroup|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        catch (TarsException &e)
        {
            os << "Tars exception:" << e.what() << endl;
        }
        catch (exception &e)
        {
            os << "std exception:" << e.what() << endl;
        }
        catch (...)
        {
            os << "unkown error" << endl;
        }

        TLOGERROR(os.str() << endl);
        result = os.str();
        return false;
    }
    
    TLOGDEBUG("swicth start" << endl);
    vector<string> names = SEPSTR(params, " ");
    if (names.size() != 4)
    {
        result = "[swicth_by_group_fail]params num != 4";
        return false;
    }

    TLOGDEBUG("get command [switchByGroup]|" << names[0] << "|" << names[1] << "|" << names[2]
    << "|" << names[3] << endl);

    bool forceSwitch = (TC_Common::lower(names[2]) == "true") ? true : false;
    int maxDifTime   = TC_Common::strto<int>(names[3]);
    bool returnValue;
    
    string curMasterServer;
    string curSlaveServer;

    //切换前，先向数据库插入一条切换记录
    long switchRecordID = -1;
    _dbHandle->insertSwitchInfo(names[0], names[1], "", "", "", "", 0, switchRecordID);
    int switchResult = 0;
    int groupStatus = 0;

    //是否清理模块切换信息
    bool cleanSwitchInfo = false;

    try
    {
        do
        {
            if (_dbHandle->hasTransferingLoc(names[0], names[1]))
            {
                os << "[swicth_by_group_fail] module is transfering: " << names[0]
                   << ", reject grant" << endl;
                TLOGERROR(FILE_FUN << os.str() << endl);
                result = os.str();
                returnValue = false;
                switchResult = 3;
                break;
            }
            {
                TC_ThreadLock::Lock lock(_moduleSwitchingLock);
                TC_ThreadLock::Lock lock1(_heartbeatInfoLock);
                map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
                    _heartbeatInfo.find(names[0]);
                if (itr != _heartbeatInfo.end())
                {
                    map<string, GroupHeartBeatInfo>::iterator itrGroupInfo =
                        itr->second.find(names[1]);
                    if (itrGroupInfo != itr->second.end())
                    {
                        if (itrGroupInfo->second.status != 0)
                        {
                            os << "[swicth_by_group_fail] group is switching! module:" << names[0]
                               << " group:" << names[1] << endl;
                            TLOGERROR(FILE_FUN << os.str() << endl);
                            result = os.str();
                            returnValue = false;
                            switchResult = 3;
                            break;
                        }
                        itrGroupInfo->second.status = 1;
                    }
                    else
                    {
                        os << "[swicth_by_group_fail] can not find module " << names[0] << endl;
                        TLOGERROR(FILE_FUN << os.str() << endl);
                        result = os.str();
                        returnValue = false;
                        switchResult = 3;
                        break;
                    }
                }
                else
                {
                    os << "[swicth_by_group_fail] can not find module " << names[0] << endl;
                    TLOGERROR(FILE_FUN << os.str() << endl);
                    result = os.str();
                    returnValue = false;
                    switchResult = 3;
                    break;
                }
                map<string, int>::iterator itModuleSwitch = _moduleSwitching.find(names[0]);
                if (itModuleSwitch != _moduleSwitching.end())
                {
                    itModuleSwitch->second++;
                }
                else
                {
                    _moduleSwitching[names[0]] = 1;
                }
                cleanSwitchInfo = true;
            }
            PackTable packTable;
            int iRet = _dbHandle->getPackTable(names[0], packTable);
            if (iRet != 0)
            {
                os << "[swicth_by_group_fail]do not support module: " << names[0]
                   << ", reject grant" << endl;
                TLOGERROR(FILE_FUN << os.str() << endl);
                result = os.str();
                returnValue = false;
                switchResult = 3;
                break;
            }
            map<string, GroupInfo>::iterator it = packTable.groupList.find(names[1]);
            if (it == packTable.groupList.end())
            {
                os << "[swicth_by_group_fail]module: " << names[0]
                   << " do not have group: " << names[1] << endl;
                TLOGERROR(FILE_FUN << os.str() << endl);
                result = os.str();
                returnValue = false;
                switchResult = 3;
                break;
            }
            it->second.accessStatus = 0;
            string idc;
            //生成临时的切换后的路由
            map<string, ServerInfo>::iterator it1 = packTable.serverList.begin(),
                                              it1End = packTable.serverList.end();
            for (; it1 != it1End; ++it1)
            {
                if (it1->second.ServerStatus == "M" && it1->second.groupName == names[1])
                {
                    curMasterServer = it1->first;
                    idc = it1->second.idc;
                    break;
                }
            }
            if (it1 == it1End)
            {
                os << "[swicth_by_group_fail]no masterServer find module: " << names[0]
                   << " group: " << names[1] << endl;
                TLOGERROR(FILE_FUN << os.str() << endl);
                result = os.str();
                returnValue = false;
                switchResult = 3;
                break;
            }
            //找备机
            map<string, ServerInfo>::iterator it2 = packTable.serverList.begin(),
                                              it2End = packTable.serverList.end();
            for (; it2 != it2End; ++it2)
            {
                if (it2->second.ServerStatus == "S" && it2->second.groupName == names[1] &&
                    it2->second.idc == idc)
                {
                    curSlaveServer = it2->first;
                    break;
                }
            }
            if (it2 == it2End)
            {
                os << "[swicth_by_group_fail]no slaveServer find module: " << names[0]
                   << " group: " << names[1] << endl;
                TLOGERROR(FILE_FUN << os.str() << endl);
                result = os.str();
                returnValue = false;
                switchResult = 3;
                break;
            }
            TLOGDEBUG("curMasterServer: " << curMasterServer << " "
                                          << "curSlaveServer: " << curSlaveServer << endl);
            // it指向组信息 it1指向主机，it2指向要切合的备机 修改路由
            it1->second.ServerStatus = "S";
            it2->second.ServerStatus = "M";
            it->second.masterServer = curSlaveServer;
            it->second.bakList[curSlaveServer] = "";
            map<string, string>::iterator it3 = it->second.bakList.begin(),
                                          it3End = it->second.bakList.end();
            for (; it3 != it3End; ++it3)
            {
                if (it3->first != curSlaveServer) it3->second = curSlaveServer;
            }
            vector<string> &vIdcList = it->second.idcList[idc];
            vector<string>::iterator vIt = vIdcList.begin(), vItEnd = vIdcList.end();
            while (vIt != vItEnd)
            {
                if (*vIt == curSlaveServer)
                {
                    vIdcList.erase(vIt);
                    break;
                }
                ++vIt;
            }
            vector<string> vIdcListNew;
            vIdcListNew.push_back(curSlaveServer);
            vIdcListNew.insert(vIdcListNew.end(), vIdcList.begin(), vIdcList.end());
            it->second.idcList[idc] = vIdcListNew;
            ++packTable.info.version;
            //生成临时路由表完成
            //验证备机差异
            if (!forceSwitch)
            {
                ServerInfo &serverInfo = packTable.serverList[curSlaveServer];
                string sStringProxy = serverInfo.RouteClientServant;
                RouterClientPrx pRouterClientPrx =
                    Application::getCommunicator()->stringToProxy<RouterClientPrx>(sStringProxy);
                pRouterClientPrx->tars_timeout(100);
                try
                {
                    // tars::Int32 getBinlogdif(tars::Int32 &difBinlogTime,tars::TarsCurrentPtr
                    // current);
                    int difBinlogTime;
                    int iRet = pRouterClientPrx->getBinlogdif(difBinlogTime);
                    if (iRet != 0)
                    {
                        os << "[swicth_by_group_fail]备机可能处于自建状态,不允许切换" << endl;
                        TLOGERROR(FILE_FUN << os.str() << endl);
                        returnValue = false;
                        result = os.str();
                        switchResult = 3;
                        break;
                    }
                    else
                    {
                        if (difBinlogTime > maxDifTime)
                        {
                            os << "[swicth_by_group_fail]备机的binlog差异为" << difBinlogTime
                               << endl;
                            TLOGERROR(FILE_FUN << os.str() << endl);
                            result = os.str();
                            returnValue = false;
                            switchResult = 3;
                            break;
                        }
                    }
                }
                catch (const exception &ex)
                {
                    os << "[swicth_by_group_fail]获取备机binlog差异发生异常:" << ex.what() << endl;
                    TLOGERROR(FILE_FUN << os.str() << endl);
                    result = os.str();
                    returnValue = false;
                    switchResult = 3;
                    break;
                }
            }
            //向备机推送路由
            ServerInfo &serverInfo1 = packTable.serverList[curSlaveServer];
            string sStringProxy1 = serverInfo1.RouteClientServant;
            RouterClientPrx pSalveRouterClientPrx =
                Application::getCommunicator()->stringToProxy<RouterClientPrx>(sStringProxy1);
            pSalveRouterClientPrx->tars_timeout(100);
            try
            {
                int iRet = pSalveRouterClientPrx->setRouterInfoForSwitch(names[0], packTable);
                if (iRet != 0)
                {
                    os << "[swicth_by_group_fail]备机处于自建状态,不允许切换" << endl;
                    TLOGERROR(FILE_FUN << os.str() << endl);
                    result = os.str();
                    returnValue = false;
                    switchResult = 3;
                    break;
                }
            }
            catch (const exception &ex)
            {
                os << "[pRouterClientPrx->setRouterInfoForSwicth]  exception: " << ex.what()
                   << endl;
                TLOGERROR(FILE_FUN << os.str() << endl);
                // result=os.str();
            }
            //如果上面步骤全部成功 修改内存和数据库
            if (_dbHandle->switchMasterAndSlaveInDbAndMem(
                    names[0], names[1], curMasterServer, curSlaveServer, packTable) != 0)
            {
                os << "[swicth_by_group_fail]更新数据库和内存路由表失败,请手工处理" << endl;
                TLOGERROR(FILE_FUN << os.str() << endl);
                result = os.str();
                returnValue = false;
                switchResult = 3;
                break;
            }
            result = "[swicth_by_group_success]";
            returnValue = true;
            _dbHandle->updateStatusToRelationDB(curSlaveServer, "M");
            _dbHandle->updateStatusToRelationDB(curMasterServer, "S");
            switchResult = 1;
        } while (0);
        TLOGDEBUG("swicth end" << endl);
        if (cleanSwitchInfo)
        {
            TC_ThreadLock::Lock lock(_moduleSwitchingLock);
            map<string, int>::iterator it = _moduleSwitching.find(names[0]);
            if (it != _moduleSwitching.end())
            {
                it->second--;
                if (it->second == 0) _moduleSwitching.erase(it);
            }
            cleanSwitchInfo = false;
        }
        //手动切换完成后 再更改心跳信息
        if (returnValue)
        {
            FDLOG("switch") << "switch over now change zhe report" << endl;
            TC_ThreadLock::Lock lock(_heartbeatInfoLock);
            map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
                _heartbeatInfo.find(names[0]);
            if (itr != _heartbeatInfo.end())
            {
                FDLOG("switch") << "switch over now change zhe report find module" << names[0]
                                << endl;
                map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.find(names[1]);
                if (itrGroupInfo != itr->second.end())
                {
                    FDLOG("switch")
                        << "switch over now change zhe report find groupName" << names[1] << endl;
                    time_t nowTime = TC_TimeProvider::getInstance()->getNow();
                    string tmp = itrGroupInfo->second.masterServer;
                    itrGroupInfo->second.status = 0;
                    itrGroupInfo->second.masterServer = itrGroupInfo->second.slaveServer;
                    itrGroupInfo->second.slaveServer = tmp;
                    itrGroupInfo->second.masterLastReportTime = nowTime + 86400;
                    itrGroupInfo->second.slaveLastReportTime = nowTime + 86400;
                }
            }
        }
        else
        {
            FDLOG("switch") << "switch over now change zhe report" << endl;
            TC_ThreadLock::Lock lock(_heartbeatInfoLock);
            map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
                _heartbeatInfo.find(names[0]);
            if (itr != _heartbeatInfo.end())
            {
                FDLOG("switch") << "switch over now change zhe report find module" << names[0]
                                << endl;
                map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.find(names[1]);
                if (itrGroupInfo != itr->second.end())
                {
                    FDLOG("switch")
                        << "switch over now change zhe report find groupName" << names[1] << endl;
                    itrGroupInfo->second.status = 0;
                }
            }
        }

        _dbHandle->updateSwitchInfo(
            switchRecordID, switchResult, os.str(), groupStatus, curMasterServer, curSlaveServer);
        return returnValue;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
        TLOGERROR(FILE_FUN << os.str() << endl);
        result = os.str();
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
        TLOGERROR(FILE_FUN << os.str() << endl);
        result = os.str();
    }
    catch (...)
    {
        os << "unkown error" << endl;
        TLOGERROR(FILE_FUN << os.str() << endl);
        result = os.str();
    }

    if (cleanSwitchInfo)
    {
        TC_ThreadLock::Lock lock(_moduleSwitchingLock);
        map<string, int>::iterator it = _moduleSwitching.find(names[0]);
        if (it != _moduleSwitching.end())
        {
            it->second--;
            if (it->second == 0) _moduleSwitching.erase(it);
        }
    }

    {
        TC_ThreadLock::Lock lock(_heartbeatInfoLock);
        map<string, map<string, GroupHeartBeatInfo>>::iterator itr = _heartbeatInfo.find(names[0]);
        if (itr != _heartbeatInfo.end())
        {
            map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.find(names[1]);
            if (itrGroupInfo != itr->second.end())
            {
                itrGroupInfo->second.status = 0;
            }
        }
    }

    _dbHandle->updateSwitchInfo(switchRecordID, 3, os.str());
    return false;
}

bool RouterServer::switchMirrorByGroup(const string &command, const string &params, string &result)
{
    ostringstream os;
    if (getRouterType() != ROUTER_MASTER)
    {
        try
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, switch mirror by group fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::switchMirrorByGroup|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        catch (TarsException &e)
        {
            os << "Tars exception:" << e.what() << endl;
        }
        catch (exception &e)
        {
            os << "std exception:" << e.what() << endl;
        }
        catch (...)
        {
            os << "unkown error" << endl;
        }

        TLOGERROR(os.str() << endl);
        result = os.str();
        return false;
    }

    TLOGDEBUG("swicthMirror start" << endl);
    vector<string> names = SEPSTR(params, " ");
    if (names.size() != 3)
    {
        result = "[swicth_by_group_fail]params num != 5";
        return false;
    }

    bool returnValue;
    string masterImage;
    string slaveImage;
    

    //切换前，先向数据库插入一条切换记录
    long switchRecordID = -1;
    _dbHandle->insertSwitchInfo(names[0], names[1], "", "", names[2], "", 1, switchRecordID);
    int switchResult = 0;
    int groupStatus = 0;

    //是否清理模块切换信息
    bool cleanSwitchInfo = false;

    try
    {
        do
        {
            if (_dbHandle->hasTransferingLoc(names[0], names[1]))
            {
                os << "[swicth_mirror_by_group_fail] module is transfering: " << names[0]
                   << ", reject grant" << endl;
                TLOGERROR(FILE_FUN << os.str() << endl);
                result = os.str();
                returnValue = false;
                switchResult = 3;
                break;
            }
            {
                TC_ThreadLock::Lock lock(_moduleSwitchingLock);
                TC_ThreadLock::Lock lock1(_heartbeatInfoLock);
                map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
                    _heartbeatInfo.find(names[0]);
                if (itr != _heartbeatInfo.end())
                {
                    map<string, GroupHeartBeatInfo>::iterator itrGroupInfo =
                        itr->second.find(names[1]);
                    if (itrGroupInfo != itr->second.end())
                    {
                        if (itrGroupInfo->second.status != 0)
                        {
                            os << "[swicth_by_group_fail] group is switching! module:" << names[0]
                               << " group:" << names[1] << endl;
                            TLOGERROR(FILE_FUN << os.str() << endl);
                            result = os.str();
                            returnValue = false;
                            switchResult = 3;
                            break;
                        }
                        itrGroupInfo->second.status = 2;
                    }
                    else
                    {
                        os << "[swicth_by_group_fail] can not find module " << names[0] << endl;
                        TLOGERROR(FILE_FUN << os.str() << endl);
                        result = os.str();
                        returnValue = false;
                        switchResult = 3;
                        break;
                    }
                }
                else
                {
                    os << "[swicth_by_group_fail] can not find module " << names[0] << endl;
                    TLOGERROR(FILE_FUN << os.str() << endl);
                    result = os.str();
                    returnValue = false;
                    switchResult = 3;
                    break;
                }
                map<string, int>::iterator itModuleSwitch = _moduleSwitching.find(names[0]);
                if (itModuleSwitch != _moduleSwitching.end())
                {
                    itModuleSwitch->second++;
                }
                else
                {
                    _moduleSwitching[names[0]] = 1;
                }
                cleanSwitchInfo = true;
            }
            //如果上面步骤全部成功 修改内存和数据库
            if (_dbHandle->switchMirrorInDbAndMemByIdc(
                    names[0], names[1], names[2], false, masterImage, slaveImage) != 0)
            {
                os << "[swicth_by_group_fail]更新数据库和内存路由表失败,请手工处理" << endl;
                TLOGERROR(FILE_FUN << os.str() << endl);
                result = os.str();
                returnValue = false;
                switchResult = 3;
                break;
            }
            result = "[swicth_mirror_by_group_success]";
            returnValue = true;
            _dbHandle->updateStatusToRelationDB(masterImage, "I");
            _dbHandle->updateStatusToRelationDB(slaveImage, "B");
            switchResult = 1;
        } while (0);
        TLOGDEBUG("swicth_mirror end" << endl);
        if (cleanSwitchInfo)
        {
            TC_ThreadLock::Lock lock(_moduleSwitchingLock);
            map<string, int>::iterator it = _moduleSwitching.find(names[0]);
            if (it != _moduleSwitching.end())
            {
                it->second--;
                if (it->second == 0) _moduleSwitching.erase(it);
            }
        }
        //手动切换完成后 再更改心跳信息
        if (returnValue)
        {
            FDLOG("switch") << "switch over now change zhe report find groupName" << names[1]
                            << endl;

            TC_ThreadLock::Lock lock(_heartbeatInfoLock);
            map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
                _heartbeatInfo.find(names[0]);
            if (itr != _heartbeatInfo.end())
            {
                FDLOG("switch") << "switch over now change the report find module" << names[0]
                                << endl;
                map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.find(names[1]);
                if (itrGroupInfo != itr->second.end())
                {
                    FDLOG("switch")
                        << "switch over now change the report find groupName" << names[1] << endl;
                    time_t nowTime = TC_TimeProvider::getInstance()->getNow();
                    itrGroupInfo->second.status = 0;
                    //读写切换成功，交互主备身份
                    string tmp = itrGroupInfo->second.mirrorIdc[names[2]][0];
                    itrGroupInfo->second.mirrorIdc[names[2]][0] =
                        itrGroupInfo->second.mirrorIdc[names[2]][1];
                    itrGroupInfo->second.mirrorIdc[names[2]][1] = tmp;
                    itrGroupInfo->second.mirrorInfo[itrGroupInfo->second.mirrorIdc[names[2]][0]] =
                        nowTime + 86400;
                }
            }
        }

        _dbHandle->updateSwitchMirrorInfo(
            switchRecordID, switchResult, os.str(), groupStatus, masterImage, slaveImage);
        return returnValue;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
        TLOGERROR(FILE_FUN << os.str() << endl);
        result = os.str();
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
        TLOGERROR(FILE_FUN << os.str() << endl);
        result = os.str();
    }
    catch (...)
    {
        os << "unkown error" << endl;
        TLOGERROR(FILE_FUN << os.str() << endl);
        result = os.str();
    }
    if (cleanSwitchInfo)
    {
        TC_ThreadLock::Lock lock(_moduleSwitchingLock);
        map<string, int>::iterator it = _moduleSwitching.find(names[0]);
        if (it != _moduleSwitching.end())
        {
            it->second--;
            if (it->second == 0) _moduleSwitching.erase(it);
        }
    }

    _dbHandle->updateSwitchInfo(switchRecordID, 3, os.str());
    return false;
}

bool RouterServer::showHeartBeatInfo(const string &command, const string &params, string &result)
{
    ostringstream os;
    HeartbeatInfo info;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, show heart beat info fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::showHeartBeatInfo|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else    
        {
            TC_ThreadLock::Lock lock(_heartbeatInfoLock);
            info = _heartbeatInfo;
        }

        os << "当前时间:" << TC_TimeProvider::getInstance()->getNow() << endl;
        HeartbeatInfo::iterator itr = info.begin();
        while (itr != info.end())
        {
            os << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
            os << "模块名:" << itr->first << endl;
            map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.begin();
            while (itrGroupInfo != itr->second.end())
            {
                os << "服务组:" << itrGroupInfo->first << endl;
                os << "主机:" << itrGroupInfo->second.masterServer << " "
                   << itrGroupInfo->second.masterLastReportTime << endl;
                os << "备机:" << itrGroupInfo->second.slaveServer << " "
                   << itrGroupInfo->second.slaveLastReportTime << endl;
                os << "镜像心跳:" << endl;
                map<string, time_t>::iterator mirroItr = itrGroupInfo->second.mirrorInfo.begin();
                while (mirroItr != itrGroupInfo->second.mirrorInfo.end())
                {
                    os << mirroItr->first << " " << mirroItr->second << endl;
                    ++mirroItr;
                }
                ++itrGroupInfo;
            }
            ++itr;
        }
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::resetServerStatus(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, reset server status fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::resetServerStatus|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else    
        {
            vector<string> names = SEPSTR(params, " ");
            if (names.size() < 3)
            {
                result = "[reset_server_status]params num != 3";
                return false;
            }

            int rc = _dbHandle->setServerstatus(names[0], names[1], names[2], 0);

            TC_ThreadLock::Lock lock(_heartbeatInfoLock);
            map<string, map<string, GroupHeartBeatInfo>>::iterator itr = _heartbeatInfo.find(names[0]);
            if (itr != _heartbeatInfo.end())
            {
                map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.find(names[1]);
                if (itrGroupInfo != itr->second.end())
                {
                    itrGroupInfo->second.serverStatus[names[2]] = 0;
                    itrGroupInfo->second.mirrorInfo[names[2]] = 0;
                }
            }

            os << "reset " << params << " status result: " << (rc == 0 ? "Succ" : "Fail");
        }
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::checkModule(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        if (getRouterType() != ROUTER_MASTER)
        {
            RouterPrx prx;
            string masterRouterObj;

            if ((command == "reproxy") || (getMasterRouterPrx(prx, masterRouterObj) != 0))
            {
                os << "master still not set, check module fail!";
                TARS_NOTIFY_ERROR(string("RouterServer::checkModule|") + os.str());
                TLOGDEBUG(os.str() << endl);
                result = os.str();
                return true;
            }
            else
            {
                TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master - " << masterRouterObj << endl);
                return prx->procAdminCommand(command, params, result);
            }
        }
        else    
        {
            if (params.empty())
            {
                result = "useage: module modulename\n";
                result += "example: module TestModule";
                return true;
            }

            bool exist = _dbHandle->checkModule(params);
            os << (exist ? "support " : "not support ") << params;
        }
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

bool RouterServer::procAdminCommand(const string &command, const string &params, string &result)
{
    map<string, TAdminFunc>::iterator it;

    it =  _procAdminCommandFunctors.find(command);

    if (it != _procAdminCommandFunctors.end())
    {
        return (it->second)("reproxy", params, result);
    }

    assert(false);

    return false;
}

bool RouterServer::help(const string &command, const string &params, string &result)
{
    ostringstream os;
    try
    {
        os << "router.reloadRouter                          重新从数据库加载路由" << endl;
        os << "router.reloadRouterByModule modulename       按模块名重新从数据库加载路由" << endl;
        os << "router.reloadConf                            重新加载配置文件及数据库" << endl;
        os << "router.getVersions modulename(all)           列出模块版本" << endl;
        os << "router.getRouterInfos modulename(all)        列出模块信息" << endl;
        os << "router.getTransferInfos modulename(all)      列出模块正在迁移信息" << endl;
        os << "router.clearTransferInfos modulename(all)    清除模块正在迁移信息" << endl;
        os << "router.notifyCacheServers modulename(all)    通知模块相关master服务器路由" << endl;
        os << "router.defragRouteRecords modulename(all)    整理模块路由记录" << endl;
        os << "router.deleteAllProxy                        清除内存中的所有代理缓存" << endl;
        os << "router.heartBeat                             显示心态上报信息" << endl;
        os << "router.switchMirrorByGroup modulename groupname idc 切换制定idc下面的镜像" << endl;
        os << "router.switchByGroup modulename groupname isForceSwitch difBinlogTime 主备切换"
           << endl;
        os << "router.resetServerStatus moduleName groupName serverName 重置服务状态" << endl;
        os << "router.checkModule moduleName                检查模块路由加载结果" << endl;
        os << "help succ!" << endl;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGDEBUG(os.str() << endl);
    result = os.str();
    return true;
}

void RouterServer::destroyApp()
{
//    _etcdThread.terminate();

    _timerThread.terminate();

    _switchThread.terminate();

//    while (_etcdThread.isAlive() || _timerThread.isAlive() || _switchThread.isAlive())
	while ( _timerThread.isAlive() || _switchThread.isAlive())
    {
        usleep(1000);
    }

//    if (_etcdHandle)
//    {
//        _etcdHandle->destroy();
//    }
}

int RouterServer::setUpEtcd()
{
//    setEnableEtcd(_conf.checkEnableEtcd());

//    if (!isEnableEtcd())
//    {
       TLOGDEBUG("RouterServer::setUpEtcd ETCD is not enable" << endl);
       setRouterType(ROUTER_MASTER);
       return 0;
//    }

//    _etcdHandle = std::make_shared<EtcdHandle>();
//    auto etcdHost = std::make_shared<EtcdHost>();
//    if (_etcdHandle->init(_conf, etcdHost) != 0)
//    {
//        TLOGERROR("RouterServer::setUpEtcd init etcd handle error" << endl);
//        return -1;
//    }

//    // RouterServer启动时均配置为slave，待后续抢主
//    setRouterType(ROUTER_SLAVE);

//    _etcdThread.init(_conf, _etcdHandle);
//    _etcdThread.start();

   return 0;
}

void RouterServer::clearSwitchThreads()
{
    TC_ThreadLock::Lock lock(_doSwitchThreadsLock);
    _doSwitchThreads.clear();
}

void RouterServer::terminateSwitchThreads()
{
    TC_ThreadLock::Lock lock(_doSwitchThreadsLock);
    vector<DoSwitchThreadPtr>::iterator itr = _doSwitchThreads.begin();
    while (itr != _doSwitchThreads.end())
    {
        (*itr)->join();
    }
}

void RouterServer::addSwitchThread(DoSwitchThreadPtr threadPtr)
{
    TC_ThreadLock::Lock lock(_doSwitchThreadsLock);
    _doSwitchThreads.push_back(threadPtr);
}

void RouterServer::removeFinishedSwitchThreads()
{
    TLOGDEBUG(FILE_FUN << "before removeFinishedSwitchThreads size:" << _doSwitchThreads.size()
                       << endl);
    {
        TC_ThreadLock::Lock lock(_doSwitchThreadsLock);
        vector<DoSwitchThreadPtr>::iterator it = _doSwitchThreads.begin();
        while (it != _doSwitchThreads.end())
        {
            if ((*it)->isFinish())
            {
                it = _doSwitchThreads.erase(it);
            }
            else
                it++;
        }
    }
    TLOGDEBUG(FILE_FUN << "after removeFinishedSwitchThreads size:" << _doSwitchThreads.size()
                       << endl);
}

int RouterServer::checkModuleSwitching(const TransferInfo &transferInfo) const
{
    {
        TC_ThreadLock::Lock lock(_moduleSwitchingLock);
        map<string, int>::const_iterator it = _moduleSwitching.find(transferInfo.moduleName);
        if (it == _moduleSwitching.end())
        {
            return 0;
        }
    }

    {
        TC_ThreadLock::Lock lock(_doSwitchThreadsLock);
        vector<DoSwitchThreadPtr>::const_iterator it = _doSwitchThreads.begin();
        for (; it != _doSwitchThreads.end(); ++it)
        {
            FDLOG("TransferSwitch2")
                << __LINE__ << "|" << __FUNCTION__ << "|"
                << "tranid:" << transferInfo.id << "|"
                << " pthread_id: " << pthread_self()
                << "_doSwitchThreadsModuleSize: " << _doSwitchThreads.size() << "|"
                << (*it)->getWork().moduleName << " group: " << (*it)->getWork().groupName << endl;
            if ((*it)->getWork().moduleName == transferInfo.moduleName)
            {
                FDLOG("TransferSwitch2") << __LINE__ << "|" << __FUNCTION__ << "|"
                                         << "tranid:" << transferInfo.id << "|"
                                         << " pthread_id: " << pthread_self() << endl;
                if ((*it)->getWork().groupName == transferInfo.groupName ||
                    (*it)->getWork().groupName == transferInfo.transGroupName)
                {
                    string errMsg =
                        "DbHandle::addMemTransferRecord error module is switching moduleName:" +
                        transferInfo.moduleName +
                        " | Switch groupName: " + (*it)->getWork().groupName +
                        " | Transfer groupName: " + transferInfo.groupName +
                        " | Transfer transGroupName: " + transferInfo.transGroupName +
                        "with collision";
                    TARS_NOTIFY_ERROR(errMsg);
                    FDLOG("TransferSwitch2") << __LINE__ << "|" << __FUNCTION__ << "|"
                                             << "tranid:" << transferInfo.id << "|"
                                             << " pthread_id: " << pthread_self() << errMsg << endl;
                    return -1;
                }
            }
        }
    }
    return 0;
}

int RouterServer::switchingModuleNum() const
{
    TC_ThreadLock::Lock lock(_moduleSwitchingLock);
    return _moduleSwitching.size();
}

bool RouterServer::isModuleSwitching(const std::string &moduleName) const
{
    TC_ThreadLock::Lock lock(_moduleSwitchingLock);
    map<string, int>::const_iterator it = _moduleSwitching.find(moduleName);
    return it != _moduleSwitching.end();
}

void RouterServer::addSwitchModule(const std::string &moduleName)
{
    TC_ThreadLock::Lock lock(_moduleSwitchingLock);
    map<string, int>::iterator it = _moduleSwitching.find(moduleName);
    if (it != _moduleSwitching.end())
    {
        it->second++;
        FDLOG("switch") << "addSwitchModule moduleName : " << moduleName
                        << "| switch tasks : " << it->second << endl;
    }
    else
    {
        _moduleSwitching[moduleName] = 1;
    }
}

void RouterServer::removeSwitchModule(const std::string &moduleName)
{
    TC_ThreadLock::Lock lock(_moduleSwitchingLock);
    map<string, int>::iterator it = _moduleSwitching.find(moduleName);
    if (it != _moduleSwitching.end())
    {
        it->second--;
        FDLOG("switch") << "removeSwitchModule moduleName : " << moduleName
                        << "| switch tasks : " << it->second << endl;
        if (it->second == 0) _moduleSwitching.erase(it);
    }
}

int RouterServer::checkSwitchTimes(const std::string &moduleName,
                                   const std::string &groupName,
                                   int type,
                                   int maxTimes) const
{
    TC_ThreadLock::Lock lock(_switchTimesLock);
    map<string, map<string, map<int, SwitchTimes>>>::const_iterator itModule =
        _switchTimes.find(moduleName);
    if (itModule != _switchTimes.end())
    {
        map<string, map<int, SwitchTimes>>::const_iterator itGroup =
            itModule->second.find(groupName);
        if (itGroup != itModule->second.end())
        {
            map<int, SwitchTimes>::const_iterator itType = itGroup->second.find(type);
            if (itType != itGroup->second.end())
            {
                if (itType->second.switchTimes >= maxTimes)
                {
                    return 1;
                }
            }
        }
    }

    return 0;
}

void RouterServer::addSwitchTime(const string &moduleName,
                                 const string &groupName,
                                 const int &type,
                                 time_t now)
{
    TC_ThreadLock::Lock lock(_switchTimesLock);
    map<string, map<string, map<int, SwitchTimes>>>::iterator itModule =
        _switchTimes.find(moduleName);
    if (itModule != _switchTimes.end())
    {
        map<string, map<int, SwitchTimes>>::iterator itGroup = itModule->second.find(groupName);
        if (itGroup != itModule->second.end())
        {
            map<int, SwitchTimes>::iterator itType = itGroup->second.find(type);
            if (itType != itGroup->second.end())
            {
                itType->second.switchTimes++;
            }
            else
            {
                SwitchTimes switchTimesTmp;
                switchTimesTmp.switchTimes = 1;
                switchTimesTmp.firstTime = now;
                itGroup->second.insert(pair<int, SwitchTimes>(type, switchTimesTmp));
            }
        }
        else
        {
            SwitchTimes switchTimesTmp;
            switchTimesTmp.switchTimes = 1;
            switchTimesTmp.firstTime = now;

            map<int, SwitchTimes> s;
            s.insert(pair<int, SwitchTimes>(type, switchTimesTmp));
            itModule->second.insert(pair<string, map<int, SwitchTimes>>(groupName, s));
        }
    }
    else
    {
        SwitchTimes switchTimesTmp;
        switchTimesTmp.switchTimes = 1;
        switchTimesTmp.firstTime = now;

        map<int, SwitchTimes> mTypeTmp;
        mTypeTmp.insert(pair<int, SwitchTimes>(type, switchTimesTmp));

        map<string, map<int, SwitchTimes>> mGroupTmp;
        mGroupTmp.insert(pair<string, map<int, SwitchTimes>>(groupName, mTypeTmp));
        _switchTimes.insert(
            pair<string, map<string, map<int, SwitchTimes>>>(moduleName, mGroupTmp));
    }
}

void RouterServer::resetSwitchTimes()
{
    TC_ThreadLock::Lock lock(_switchTimesLock);
    time_t tNowTime = TC_TimeProvider::getInstance()->getNow();

    map<string, map<string, map<int, SwitchTimes>>>::iterator itModule = _switchTimes.begin();
    while (itModule != _switchTimes.end())
    {
        map<string, map<int, SwitchTimes>>::iterator itGroup = itModule->second.begin();
        while (itGroup != itModule->second.end())
        {
            map<int, SwitchTimes>::iterator itType = itGroup->second.begin();
            while (itType != itGroup->second.end())
            {
                //如果第一次切换的时间，距离这次切换超过24小时，则需要将原次数清零
                if (itType->second.firstTime + 86400 < tNowTime)
                {
                    itGroup->second.erase(itType++);
                }
                else
                {
                    ++itType;
                }
            }

            if (itGroup->second.size() == 0)
            {
                itModule->second.erase(itGroup++);
            }
            else
            {
                itGroup++;
            }
        }

        if (itModule->second.size() == 0)
        {
            _switchTimes.erase(itModule++);
        }
        else
        {
            itModule++;
        }
    }
}

void RouterServer::loadSwitchInfo(std::map<string, PackTable> *packTables, const bool isUpgrade)
{
    _heartbeatInfo.clear();
    //上报心跳时间在刚开始加载的时候, 设置为0而不设置为当前时间，防止重启过程中的误切换，但如果是从SLAVE状态升级为MASTER时的重新加载，
    //则设置为当前时间，防止不切换
    time_t nowTime = (isUpgrade ? TNOW : 0);
    map<string, PackTable>::iterator it = packTables->begin();
    TC_ThreadLock::Lock lock(_heartbeatInfoLock);
    while (it != packTables->end())
    {
        map<string, map<string, GroupHeartBeatInfo>>::iterator itr = _heartbeatInfo.find(it->first);
        if (itr == _heartbeatInfo.end())
        {
            map<string, GroupHeartBeatInfo> tmpHeartBeatInfo;
            _heartbeatInfo.insert(
                pair<string, map<string, GroupHeartBeatInfo>>(it->first, tmpHeartBeatInfo));
        }
        map<string, GroupInfo>::iterator itrGroupInfo = it->second.groupList.begin();
        while (itrGroupInfo != it->second.groupList.end())
        {
            GroupHeartBeatInfo groupHeartBeatInfo;
            groupHeartBeatInfo.masterServer = itrGroupInfo->second.masterServer;
            groupHeartBeatInfo.masterLastReportTime = nowTime;
            groupHeartBeatInfo.slaveLastReportTime = nowTime;
            groupHeartBeatInfo.status = itrGroupInfo->second.accessStatus;
            if (itrGroupInfo->second.accessStatus != 0)
            {
                string errMsg =
                    "RouterServer::loadSwitchInfo find groupStatus!=0 please check!! status:" +
                    I2S(itrGroupInfo->second.accessStatus) + " module:" + it->first +
                    " group:" + itrGroupInfo->first;
                FDLOG("switch") << errMsg << endl;
                TARS_NOTIFY_ERROR(errMsg);
            }
            map<string, ServerInfo>::iterator itrServerInfo =
                it->second.serverList.find(groupHeartBeatInfo.masterServer);
            if (itrServerInfo != it->second.serverList.end())
            {
                bool bNeedInsert = false;
                string MasterIdc = itrServerInfo->second.idc;
                map<string, vector<string>>::const_iterator itrIdclist;
                for (itrIdclist = itrGroupInfo->second.idcList.begin();
                     itrIdclist != itrGroupInfo->second.idcList.end();
                     itrIdclist++)
                {
                    if (itrIdclist->first == MasterIdc)
                    {
                        vector<string> serverlist = itrIdclist->second;
                        for (size_t i = 0; i < serverlist.size(); i++)
                        {
                            groupHeartBeatInfo.serverStatus[serverlist[i]] =
                                it->second.serverList[serverlist[i]].status;
                            if (serverlist[i] != groupHeartBeatInfo.masterServer)
                            {
                                groupHeartBeatInfo.slaveServer = serverlist[i];
                                groupHeartBeatInfo.slaveLastReportTime = nowTime;
                                bNeedInsert = true;
                            }
                        }
                    }
                    else
                    {
                        vector<string> serverlist = itrIdclist->second;
                        for (size_t i = 0; i < serverlist.size(); i++)
                        {
                            groupHeartBeatInfo.serverStatus[serverlist[i]] =
                                it->second.serverList[serverlist[i]].status;
                            groupHeartBeatInfo.mirrorInfo.insert(
                                pair<string, time_t>(serverlist[i], nowTime));
                            bNeedInsert = true;
                        }
                        groupHeartBeatInfo.mirrorIdc.insert(
                            pair<string, vector<string>>(itrIdclist->first, serverlist));
                    }
                }
                if (bNeedInsert)
                {
                    _heartbeatInfo[it->first].insert(pair<string, GroupHeartBeatInfo>(
                        itrGroupInfo->second.groupName, groupHeartBeatInfo));
                }
            }
            itrGroupInfo++;
        }
        it++;
    }
}

void RouterServer::loadSwitchInfo(std::map<string, PackTable> *packTables,
                                  const std::string &moduleName)
{
    time_t nowTime = 0;
    map<string, PackTable>::iterator it = packTables->find(moduleName);
    TC_ThreadLock::Lock lock(_heartbeatInfoLock);
    if (it != packTables->end())
    {
        map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
            _heartbeatInfo.find(moduleName);
        if (itr == _heartbeatInfo.end())
        {
            map<string, GroupHeartBeatInfo> tmpHeartBeatInfo;
            _heartbeatInfo.insert(
                pair<string, map<string, GroupHeartBeatInfo>>(moduleName, tmpHeartBeatInfo));
        }
        else
        {
            itr->second.clear();
        }
        map<string, GroupInfo>::iterator itrGroupInfo = it->second.groupList.begin();
        while (itrGroupInfo != it->second.groupList.end())
        {
            GroupHeartBeatInfo groupHeartBeatInfo;
            groupHeartBeatInfo.masterServer = itrGroupInfo->second.masterServer;
            groupHeartBeatInfo.masterLastReportTime = nowTime;
            groupHeartBeatInfo.status = itrGroupInfo->second.accessStatus;
            if (itrGroupInfo->second.accessStatus != 0)
            {
                string errMsg =
                    "DbHandle::loadSwitchInfo find groupStatus!=0 please check!! status:" +
                    I2S(itrGroupInfo->second.accessStatus) + " module:" + it->first +
                    " group:" + itrGroupInfo->first;
                FDLOG("switch") << errMsg << endl;
                TARS_NOTIFY_ERROR(errMsg);
            }
            map<string, ServerInfo>::iterator itrServerInfo =
                it->second.serverList.find(groupHeartBeatInfo.masterServer);
            if (itrServerInfo != it->second.serverList.end())
            {
                bool bNeedInsert = false;
                string MasterIdc = itrServerInfo->second.idc;
                map<string, vector<string>>::iterator itrIdclist;
                for (itrIdclist = itrGroupInfo->second.idcList.begin();
                     itrIdclist != itrGroupInfo->second.idcList.end();
                     itrIdclist++)
                {
                    if (itrIdclist->first == MasterIdc)
                    {
                        vector<string> serverlist = itrIdclist->second;
                        for (size_t i = 0; i < serverlist.size(); i++)
                        {
                            groupHeartBeatInfo.serverStatus[serverlist[i]] =
                                it->second.serverList[serverlist[i]].status;
                            if (serverlist[i] != groupHeartBeatInfo.masterServer)
                            {
                                groupHeartBeatInfo.slaveServer = serverlist[i];
                                groupHeartBeatInfo.slaveLastReportTime = nowTime;
                                bNeedInsert = true;
                            }
                        }
                    }
                    else
                    {
                        vector<string> serverlist = itrIdclist->second;
                        for (size_t i = 0; i < serverlist.size(); i++)
                        {
                            groupHeartBeatInfo.serverStatus[serverlist[i]] =
                                it->second.serverList[serverlist[i]].status;
                            groupHeartBeatInfo.mirrorInfo.insert(
                                pair<string, time_t>(serverlist[i], nowTime));
                            bNeedInsert = true;
                        }
                        groupHeartBeatInfo.mirrorIdc.insert(
                            pair<string, vector<string>>(itrIdclist->first, serverlist));
                    }
                }
                if (bNeedInsert)
                {
                    _heartbeatInfo[it->first].insert(pair<string, GroupHeartBeatInfo>(
                        itrGroupInfo->second.groupName, groupHeartBeatInfo));
                }
            }
            itrGroupInfo++;
        }
    }
    else
    {  //如果找不到这个模块，并且路由信息还是有的，说明只是这个模块被下线了
        map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
            _heartbeatInfo.find(moduleName);
        if (itr != _heartbeatInfo.end()) _heartbeatInfo.erase(itr);
    }
}

void RouterServer::setHeartbeatServerStatus(const string &moduleName,
                                            const string &groupName,
                                            const string &serverName,
                                            int status)
{
    TC_ThreadLock::Lock lock(_heartbeatInfoLock);
    map<string, map<string, GroupHeartBeatInfo>>::iterator itr = _heartbeatInfo.find(moduleName);
    if (itr != _heartbeatInfo.end())
    {
        map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.find(groupName);
        if (itrGroupInfo != itr->second.end())
        {
            itrGroupInfo->second.serverStatus[serverName] = status;
        }
    }
}

void RouterServer::swapMasterSlave(const string &moduleName,
                                   const string &groupName,
                                   int timeReportSet,
                                   bool readOver,
                                   bool bMasterChange)
{
    time_t nowTime = TC_TimeProvider::getInstance()->getNow();
    g_app.removeSwitchModule(moduleName);
    TC_ThreadLock::Lock lock(_heartbeatInfoLock);
    map<string, map<string, GroupHeartBeatInfo>>::iterator itr = _heartbeatInfo.find(moduleName);
    if (itr != _heartbeatInfo.end())
    {
        map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.find(groupName);
        if (itrGroupInfo != itr->second.end())
        {
            itrGroupInfo->second.masterLastReportTime = nowTime + timeReportSet;
            if (!readOver)
            {
                itrGroupInfo->second.status = 0;
            }
            if (bMasterChange)
            {
                //读写切换成功，交互主备身份
                string tmp = itrGroupInfo->second.masterServer;
                itrGroupInfo->second.masterServer = itrGroupInfo->second.slaveServer;
                itrGroupInfo->second.slaveServer = tmp;

                itrGroupInfo->second.serverStatus[itrGroupInfo->second.slaveServer] = -1;
            }
        }
    }
}

void RouterServer::switchMirrorOver(const string &moduleName,
                                    const string &groupName,
                                    int timeReportSet,
                                    bool succ,
                                    bool masterMirrorChange,
                                    const string &idc)
{
    time_t nowTime = TC_TimeProvider::getInstance()->getNow();
    g_app.removeSwitchModule(moduleName);
    if (!succ)
    {
        TC_ThreadLock::Lock lock(_heartbeatInfoLock);
        map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
            _heartbeatInfo.find(moduleName);
        if (itr != _heartbeatInfo.end())
        {
            map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.find(groupName);
            if (itrGroupInfo != itr->second.end())
            {
                itrGroupInfo->second.status = 0;

                map<string, vector<string>>::iterator itrMirrorIdc =
                    itrGroupInfo->second.mirrorIdc.find(idc);
                if (itrMirrorIdc != itrGroupInfo->second.mirrorIdc.end())
                {
                    time_t &mirroMasterTime =
                        itrGroupInfo->second.mirrorInfo[itrMirrorIdc->second[0]];
                    mirroMasterTime = nowTime + timeReportSet;

                    if (masterMirrorChange == true)
                    {
                        //交换镜像主备身份
                        string tmp = itrMirrorIdc->second[0];
                        itrMirrorIdc->second[0] = itrMirrorIdc->second[1];
                        itrMirrorIdc->second[1] = tmp;

                        itrGroupInfo->second.serverStatus[itrMirrorIdc->second[1]] = -1;
                    }
                }
            }
        }
    }
}

void RouterServer::downgrade()
{
    assert(getRouterType() == ROUTER_SLAVE);

    {
        TC_ThreadLock::Lock lock(_heartbeatInfoLock);
        _heartbeatInfo.clear();
    }

    {
        TC_ThreadLock::Lock lock(_switchTimesLock);
        _switchTimes.clear();
    }

    {
        TC_ThreadLock::Lock lock(_moduleSwitchingLock);
        _moduleSwitching.clear();
    }

    {
        TC_ThreadLock::Lock lock(_objLock);
        _masterRouterObj = "";
    }

    terminateSwitchThreads();
    clearSwitchThreads();
    _dbHandle->downgrade();
}

void RouterServer::upgrade()
{
    assert(getRouterType() == ROUTER_SLAVE);

    ostringstream os;
    try
    {
        _dbHandle->upgrade();
        
        _dbHandle->loadSwitchInfo(true);

        return;
    }
    catch (TarsException &e)
    {
        os << "Tars exception:" << e.what() << endl;
    }
    catch (exception &e)
    {
        os << "std exception:" << e.what() << endl;
    }
    catch (...)
    {
        os << "unkown error" << endl;
    }

    TLOGERROR(os.str() << endl);

    TARS_NOTIFY_ERROR("RouterServer::upgrade|" + os.str());

    assert(false);
}

int RouterServer::getMasterRouterPrx(RouterPrx &prx, string &masterRouterObj) const
{
    masterRouterObj = getMasterRouterObj();

    if (masterRouterObj == "")
    {
        // 全局对象中的RouterObj还未被设置
        TLOGERROR(FILE_FUN << "master router obj not set" << endl);
        return -1;
    }

    prx = Application::getCommunicator()->stringToProxy<RouterPrx>(masterRouterObj);

    return 0;
}

void RouterServer::addAdminCommand(const string& command, TAdminFunc func)
{
    _procAdminCommandFunctors.insert(std::make_pair(command, func));
}
