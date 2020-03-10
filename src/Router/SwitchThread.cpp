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
#include "SwitchThread.h"
#include "RouterServer.h"

using namespace std;
using namespace tars;

extern RouterServer g_app;

map<string, time_t> DoCheckTransThread::_isCheckTransDown;
TC_ThreadLock lockForInsertCheckTrans;

void DoCheckTransThread::init(AdminProxyWrapperPtr adminProxy,
                              const string &moduleName,
                              const string &serverName,
                              const string &reasonStr,
                              std::shared_ptr<DbHandle> dbHandle)
{
    TLOGDEBUG(" [DoCheckTransThread::init]"
              << " begin " << endl);

    try
    {
        _dbHandle = dbHandle;
        _moduleName = moduleName;
        _serverName = serverName;
        _reasonStr = reasonStr;
        _isFinish = false;
        _adminProxy = adminProxy;
        _adminProxy->tars_timeout(10000);
    }
    catch (TarsException &e)
    {
        cerr << "Tars exception: " << e.what() << endl;
        TLOGDEBUG(" [DoCheckTransThread::init]"
                  << " end with TarsException " << e.what() << endl);
        exit(-1);
    }
    catch (...)
    {
        cerr << "unkown error" << endl;
        TLOGDEBUG(" [DoCheckTransThread::init]"
                  << " end with unkown error " << endl);
        exit(-1);
    }

    TLOGDEBUG(" [DoCheckTransThread::init]"
              << " end " << endl);
}

void DoCheckTransThread::run()
{
    FDLOG("doCheck") << "DoCheckTransThread::run() start" << endl;

    doCheckTrans(_moduleName, _serverName, _reasonStr);

    FDLOG("doCheck") << "DoCheckTransThread::run() finish" << endl;

    _isFinish = true;
}

int DoCheckTransThread::doCheckTrans(const string &moduleName,
                                     const string &serverName,
                                     const string &reasonStr)
{
    try
    {
        FDLOG("doCheck") << "DoCheckTransThread::run() start" << endl;
        //用于获取服务名对应的ip
        PackTable tmpPackTable;
        int gRet = _dbHandle->getPackTable(moduleName, tmpPackTable);
        if (gRet != 0)
        {
            string errMsg =
                "[DoCheckTransThread::doCheckTrans]doMigrateCheck not find moduleName "
                "in packTable, moduleName:" +
                moduleName;
            FDLOG("doCheck") << __LINE__ << errMsg << endl;
            return -1;
        }

        ServerInfo tmpServerInfo;
        map<string, ServerInfo>::const_iterator mServerItr =
            tmpPackTable.serverList.find(serverName);
        if (mServerItr == tmpPackTable.serverList.end())
        {
            string errMsg =
                "[DoCheckTransThread::doCheckTrans]doMigrateCheck not find ip in "
                "packTable, serverName:" +
                serverName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("doCheck") << __LINE__ << errMsg << endl;
            return -1;
        }

        {
            TC_ThreadLock::Lock lock(lockForInsertCheckTrans);
            //先判断下服务器是否已经入库
            map<string, time_t>::iterator isCheckTransIter =
                _isCheckTransDown.find(mServerItr->second.ip);

            time_t nowTime = TC_TimeProvider::getInstance()->getNow();

            //已经入库且上次入库时间少于72小时
            if (isCheckTransIter != _isCheckTransDown.end() &&
                isCheckTransIter->second - nowTime <= 300)  // 259200
            {
                FDLOG("doCheck") << __LINE__
                                 << "|[DoCheckTransThread::doCheckTrans] serverName :" << serverName
                                 << " IP " << mServerItr->second.ip
                                 << " is already in DataBase and time is less than 5 min " << endl;
                return -1;
            }
            //已经入库且上次入库时间超过72小时
            else if (isCheckTransIter != _isCheckTransDown.end() &&
                     isCheckTransIter->second - nowTime > 300)
            {
                FDLOG("doCheck") << __LINE__
                                 << "|[DoCheckTransThread::doCheckTrans] serverName:" << serverName
                                 << " IP " << mServerItr->second.ip
                                 << " is already in DataBase and time is more than 5 min " << endl;
                isCheckTransIter->second = nowTime;
            }
            //未入库
            else
            {
                pair<map<string, time_t>::iterator, bool> iret =
                    _isCheckTransDown.insert(pair<string, time_t>(mServerItr->second.ip, nowTime));

                if (!iret.second)
                {
                    TLOGERROR(FILE_FUN << "_isCheckTransDown.insert() fail" << endl);
                    return -1;
                }
                FDLOG("doCheck")
                    << __LINE__
                    << "[DoCheckTransThread::doCheckTrans] _isCheckTransDown.insert() succ:"
                    << endl;
            }
        }

        int iRet = checkMachineDown(mServerItr->second.ip);
        // server down，需要入库
        if (iRet == 1)
        {
            FDLOG("doCheck") << __LINE__ << "|serverName:" << serverName
                             << " status down, start addMirgrateInfo" << endl;
            string strIp = mServerItr->second.ip;
            string strReason = reasonStr;
            int iRetaddMirgrateInfo = _dbHandle->addMirgrateInfo(strIp, strReason);

            //没入库成功
            if (iRetaddMirgrateInfo != 0)
            {
                TLOGDEBUG(FILE_FUN << "|_dbHandle->addMirgrateInfo(" << strIp
                                   << " ) error get ret:" << iRet << endl);

                TC_ThreadLock::Lock lock(lockForInsertCheckTrans);
                map<string, time_t>::iterator isCheckTransIter =
                    _isCheckTransDown.find(mServerItr->second.ip);
                _isCheckTransDown.erase(isCheckTransIter);

                return -1;
            }
            FDLOG("doCheck") << __LINE__ << "|serverName:" << serverName
                             << " status down, addMirgrateInfo success" << endl;
        }
        // 不需入库
        else
        {
            FDLOG("doCheck") << __LINE__ << "|[DoCheckTransThread::doCheckTrans]checkMachineDown("
                             << mServerItr->second.ip << " ) get ret:" << iRet << endl;
            TC_ThreadLock::Lock lock(lockForInsertCheckTrans);
            map<string, time_t>::iterator isCheckTransIter =
                _isCheckTransDown.find(mServerItr->second.ip);
            _isCheckTransDown.erase(isCheckTransIter);
        }
    }
    catch (exception &ex)
    {
        TLOGERROR(FILE_FUN << "[DoCheckTransThread::doCheckTrans] exception:" << ex.what() << endl);
        return -1;
    }

    FDLOG("doCheck") << __LINE__ << "[DoCheckTransThread::doCheckTrans] success and return 0 "
                     << endl;
    return 0;
}

int find_substr(const string &sourcestr, const string &deststr)
{
    string lowerSource = TC_Common::lower(sourcestr);

    string lowerDest = TC_Common::lower(deststr);

    if (lowerSource.find(lowerDest) != string::npos)
        return 1;
    else
        return 0;
}

int DoCheckTransThread::checkMachineDown(const string &serverIp)
{
    /**
     * 判断节点是否死机，首先从relation库中获取该节点上的所有cache服务
     * 如果存在设置状态/当前状态为active/active的服务，则判断为没有死机
     * else如果存在avtive/inactive的服务，则判断为死机
     **/
    try
    {
        vector<string> vAllServerInIp;
        int iRet = _dbHandle->getAllServerInIp(serverIp, vAllServerInIp);
        if (iRet != 0)
        {
            TLOGERROR("[DoCheckTransThread::checkMachineDown] getAllServerInIp error get ret:"
                      << iRet << endl);
            return -1;
        }

        int count = 0;  //记录设置状态active，当前状态inactive的数量
        for (size_t si = 0; si < vAllServerInIp.size(); si++)
        {
            //获取服务状态
            tars::ServerStateDesc state;
            string resultStr;
            int Ret = _adminProxy->getServerState(
                "DCache", vAllServerInIp[si], serverIp, state, resultStr);

            if (Ret != 0)
            {  //获取服务状态出错

                // Find key word "time out" in resultStr
                if (find_substr(resultStr, "TimeOut"))
                {
                    TLOGERROR("[DoCheckTransThread::checkMachineDown] getserverstate TimeOut! ret:"
                              << Ret << endl);
                    count++;
                }
                else
                {
                    TLOGERROR("[DoCheckTransThread::checkMachineDown] getserverstate error! ret:"
                              << Ret << endl);
                    return -1;
                }
            }

            if (state.settingStateInReg == "active" && state.presentStateInReg == "active")
            {
                TLOGDEBUG("[DoCheckTransThread::checkMachineDown]in ip:"
                          << serverIp << ",server:" << vAllServerInIp[si]
                          << " is active/active. 0 returned.\n");
                return 0;  // false
            }
            else if (state.settingStateInReg == "active" && state.presentStateInReg == "inactive")
            {
                count++;
                FDLOG("doCheck") << __LINE__
                                 << "[DoCheckTransThread::checkMachineDown]in ip:" << serverIp
                                 << ",server:" << vAllServerInIp[si] << " is active/inactive. "
                                 << "count " << count << endl;
            }
            else
            {
                FDLOG("doCheck") << __LINE__
                                 << "[DoCheckTransThread::checkMachineDown]in ip:" << serverIp
                                 << ",server:" << vAllServerInIp[si] << " unexceptional："
                                 << "settingState(" << state.settingStateInReg << ") "
                                 << "presentState(" << state.presentStateInReg << ") " << endl;
            }
        }
        if (count > 0)
        {
            TLOGDEBUG("[DoCheckTransThread::checkMachineDown]ip:" << serverIp << " is down \n");
            return 1;
        }
    }
    catch (exception &ex)
    {
        TLOGERROR(FILE_FUN << "exception:" << ex.what() << endl);
        return -2;
    }

    return -1;
}

SwitchThread::SwitchThread()
{
    _enable    = false;
    _terminate = false;
    _lastNotifyTime = 0;
}

SwitchThread::~SwitchThread()
{
    g_app.clearSwitchThreads();
    _doCheckTransThreads.clear();
    if (isAlive())
    {
        terminate();
        getThreadControl().join();
    }
}

void SwitchThread::terminate()
{
    g_app.terminateSwitchThreads();
    TC_ThreadLock::Lock sync(*this);
    _terminate = true;
    notifyAll();
    TLOGDEBUG(FILE_FUN << "DoSwitchThread  terminate() succ" << endl);
}

void SwitchThread::init(AdminProxyWrapperPtr adminProxy, std::shared_ptr<DbHandle> dbHandle)
{
    try
    {
        _terminate = false;
        _enable = g_app.getGlobalConfig().checkEnableSwitch();
        _switchCheckInterval = g_app.getGlobalConfig().getSwitchCheckInterval(10);
        _switchTimeout = g_app.getGlobalConfig().getSwitchTimeOut(300);
        _slaveTimeout = g_app.getGlobalConfig().getSlaveTimeOut(60);
        _dbHandle = dbHandle;
        _adminProxy = adminProxy;
        _adminProxy->tars_timeout(3000);
    }
    catch (TC_Config_Exception &e)
    {
        cerr << "catch config exception: " << e.what() << endl;
        exit(-1);
    }
    catch (TC_ConfigNoParam_Exception &e)
    {
        cerr << "get domain error: " << e.what() << endl;
        exit(-1);
    }
    catch (TarsException &e)
    {
        cerr << "Tars exception:" << e.what() << endl;
        exit(-1);
    }
    catch (...)
    {
        cerr << "unkown error" << endl;
        exit(-1);
    }

    TLOGDEBUG("init SwitchThread succ!" << endl);
}

void SwitchThread::run()
{
    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|"
                    << "SwitchThread::run() start" << endl;

    {
        TC_ThreadLock::Lock sync(*this);
        timedWait(30000);
    }

//    enum RouterType lastType = g_app.getRouterType();
    while (!_terminate && _enable)
    {
        if (!_enable)
        {
            TC_ThreadLock::Lock sync(*this);
            timedWait(1000);
            continue;
        }

//        if (g_app.isEnableEtcd() && g_app.getRouterType() == ROUTER_SLAVE)
//        {
//            if (lastType == ROUTER_MASTER)
//            {
//                TLOGDEBUG(FILE_FUN << "downgrade from master to slave" << endl);
//                downgrade();
//            }
//            lastType = ROUTER_SLAVE;
//            TC_ThreadLock::Lock sync(*this);
//            timedWait(1000);
//            continue;
//        }

        // 以下是master主机执行的逻辑
//        if (g_app.isEnableEtcd() && lastType == ROUTER_SLAVE)
//        {
//            int rc = reloadRouter();
//            if (rc != 0)
//            {
//                TLOGERROR(FILE_FUN << "SwitchThread::run reloadRouter error:" << rc << endl);
//                TC_ThreadLock::Lock sync(*this);
//                timedWait(1000);
//                continue;
//            }
//            TLOGDEBUG(FILE_FUN << "reloadRouter succ, server change to master" << endl);
//            lastType = ROUTER_MASTER;
//        }

        doSwitchCheck();

        doSlaveCheck();

        {
            TC_ThreadLock::Lock sync(*this);
            timedWait(_switchCheckInterval * 1000);
        }

        g_app.removeFinishedSwitchThreads();

        TLOGDEBUG(FILE_FUN << "before _doCheckTransThreads size:" << _doCheckTransThreads.size()
                           << endl);
        vector<DoCheckTransThreadPtr>::iterator it2 = _doCheckTransThreads.begin();
        while (it2 != _doCheckTransThreads.end())
        {
            if ((*it2)->isFinish())
            {
                it2 = _doCheckTransThreads.erase(it2);
            }
            else
                it2++;
        }
        TLOGDEBUG(FILE_FUN << "after _doCheckTransThreads size:" << _doCheckTransThreads.size()
                           << endl);
    }
}

int SwitchThread::reloadRouter()
{
    ostringstream os;
    try
    {
        int rc = _dbHandle->reloadRouter();

        if (rc == 0)
        {
            os << "reload router ok!" << endl;
            TLOGDEBUG(FILE_FUN << "doLoadSwitcInfo() now" << endl);

            if (g_app.switchingModuleNum() == 0)
            {
                // TC_ThreadLock::Lock lock1(lockForHeartBeat);
                _dbHandle->loadSwitchInfo();
                TLOGDEBUG(FILE_FUN << "doLoadSwitcInfo() end" << endl);
            }
            return 0;
        }
        else if (rc == 1)
        {
            os << "transfering or Switching, can not reload router!" << endl;
            TARS_NOTIFY_ERROR(string("SwitchThread::reloadRouter|") + os.str());
        }
        else
        {
            os << "reload router fail!" << endl;
            TARS_NOTIFY_ERROR(string("RouterServer::reloadRouter|") + os.str());
        }

        TLOGDEBUG(FILE_FUN << os.str() << endl);
        return rc;
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
    TLOGDEBUG(FILE_FUN << os.str() << endl);
    return -1;
}

void SwitchThread::doSwitchCheck()
{
    FDLOG("migrate") << __LINE__ << "|doSwitchCheck entered." << endl;
    //持续告警逻辑
    bool isTarsNotify = false;
    time_t nowTime = TC_TimeProvider::getInstance()->getNow();

    if (nowTime - _lastNotifyTime >= 300)
    {
        isTarsNotify = true;
        _lastNotifyTime = nowTime;
    }
    try
    {
        TC_ThreadLock::Lock lock1(g_app.getHeartbeatInfoLock());
        HeartbeatInfo &heartbeat = g_app.getHeartbeatInfo();
        map<string, map<string, GroupHeartBeatInfo>>::iterator itr = heartbeat.begin();
        string errMsg;
        FDLOG("migrate") << __LINE__ << "|" << heartbeat.size() << endl;
        for (; itr != heartbeat.end(); itr++)
        {
            FDLOG("migrate") << __LINE__ << "|" << itr->first << "|" << itr->second.size() << endl;
            if (itr->second.size() == 0) continue;
            int switchType = _dbHandle->getSwitchType(itr->first);
            FDLOG("migrate") << __LINE__ << "|" << switchType << endl;
            if (switchType == -1)
            {
                //告警错误日志
                errMsg = "SwitchThread::doSwitchCheck module no find:" + itr->first;
                TARS_NOTIFY_ERROR(errMsg);
                FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                continue;
            }
            else if (switchType == 2)
            {
                FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|"
                                << "SwitchThread::doSwitchCheck module no suport Switch:"
                                << itr->first << endl;
                continue;
            }
            map<string, GroupHeartBeatInfo>::iterator itrGroupInfo;
            FDLOG("migrate") << __LINE__ << "|" << itr->second.size() << endl;
            for (itrGroupInfo = itr->second.begin(); itrGroupInfo != itr->second.end();
                 itrGroupInfo++)
            {
                FDLOG("migrate") << __LINE__ << "|" << itrGroupInfo->second.status << endl;
                if (itrGroupInfo->second.status != 0)
                {
                    errMsg = "SwitchThread::doSwitchCheck module is Switching:" + itr->first +
                             " groupName:" + itrGroupInfo->first +
                             " status:" + I2S(itrGroupInfo->second.status);
                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                    if (isTarsNotify)
                    {
                        TARS_NOTIFY_ERROR(errMsg);
                    }
                    continue;
                }
                //检查镜像上报超时
                bool mirrorTimeOut = false;
                FDLOG("migrate") << __LINE__ << "|" << itrGroupInfo->second.mirrorInfo.size()
                                 << endl;
                if (itrGroupInfo->second.mirrorInfo.size() != 0)
                {
                    map<string, vector<string>>::const_iterator itrMirrorIdc;
                    for (itrMirrorIdc = itrGroupInfo->second.mirrorIdc.begin();
                         itrMirrorIdc != itrGroupInfo->second.mirrorIdc.end();
                         itrMirrorIdc++)
                    {
                        if (itrMirrorIdc->second.size() != 1)
                        {
                            continue;
                        }

                        FDLOG("mirror")
                            << __LINE__ << "|" << __FUNCTION__ << " mirrorIdc is "
                            << itrMirrorIdc->first << "|" << itrMirrorIdc->second[0] << endl;
                        //检查服务状态是否正确
                        if (itrGroupInfo->second.serverStatus[itrMirrorIdc->second[0]] != 0)
                        {
                            string err =
                                "SwitchThread::doSwitchCheck server status error, please check! "
                                "module:" +
                                itr->first + " groupName:" + itrGroupInfo->first +
                                " serverName:" + itrMirrorIdc->second[0] + " status:" +
                                I2S(itrGroupInfo->second.serverStatus[itrMirrorIdc->second[0]]);
                            FDLOG("switch")
                                << __LINE__ << "|" << __FUNCTION__ << "|" << err << endl;
                            if (isTarsNotify)
                            {
                                TARS_NOTIFY_ERROR(errMsg);
                            }
                            continue;
                        }
                        time_t mirroMasterTime =
                            itrGroupInfo->second.mirrorInfo[itrMirrorIdc->second[0]];
                        if (mirroMasterTime == 0)
                        {
                            errMsg = "SwitchThread::doSwitchCheck find mirror reportTime==0 " +
                                     itrMirrorIdc->second[0] +
                                     " mirrorName:" + itrMirrorIdc->second[0];
                            FDLOG("switch")
                                << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                        }
                        else if ((int(nowTime) - int(mirroMasterTime)) > _switchTimeout)
                        {
                            FDLOG("mirror") << __LINE__ << "|" << __FUNCTION__
                                            << " mirroMasterTimeout " << endl;
                            if (((int(nowTime) - int(itrGroupInfo->second.masterLastReportTime)) >
                                 _switchTimeout) ||
                                ((int(nowTime) - int(itrGroupInfo->second.slaveLastReportTime)) >
                                 _switchTimeout))
                            {
                                errMsg =
                                    "SwitchThread::doSwitchCheck find mirror and (master or "
                                    "slave) TimeOut so not do switch, graoupName:" +
                                    itrGroupInfo->first + " mirrorName:" + itrMirrorIdc->second[0];
                                FDLOG("switch")
                                    << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                                continue;
                            }
                            mirrorTimeOut = true;
                            g_app.addSwitchModule(itr->first);

                            itrGroupInfo->second.status = 2;
                            SwitchWork tmpSwitchWork;
                            tmpSwitchWork.type = 2;  //无镜像备机切换
                            tmpSwitchWork.moduleName = itr->first;
                            tmpSwitchWork.groupName = itrGroupInfo->first;
                            tmpSwitchWork.masterServer = "";
                            tmpSwitchWork.slaveServer = itrGroupInfo->second.slaveServer;
                            tmpSwitchWork.mirrorIdc = itrMirrorIdc->first;
                            tmpSwitchWork.mirrorServer = itrMirrorIdc->second[0];
                            tmpSwitchWork.switchType = 0;
                            tmpSwitchWork.switchBeginTime = time(NULL);

                            DoSwitchThreadPtr threadPtr = new DoSwitchThread(
                                g_app.createAdminRegProxyWrapper(),
                                _dbHandle,
                                g_app.getGlobalConfig().getSwitchTimeOut(300),
                                g_app.getGlobalConfig().getSwitchBinLogDiffLimit(300),
                                g_app.getGlobalConfig().getSwitchMaxTimes(3),
                                g_app.getGlobalConfig().getDowngradeTimeout(30));
                            g_app.addSwitchThread(threadPtr);
                            threadPtr->CreateThread(tmpSwitchWork);

                            string strReason = "mirror heartbeat overtime";
                            FDLOG("mirror") << __LINE__ << "|" << __FUNCTION__
                                            << " mirroMasterTimeout and" << strReason << endl;

                            DoCheckTransThreadPtr doCheckTransThreadPtr = new DoCheckTransThread();
                            doCheckTransThreadPtr->init(g_app.createAdminRegProxyWrapper(),
                                                        itr->first,
                                                        itrMirrorIdc->second[0],
                                                        strReason,
                                                        _dbHandle);
                            _doCheckTransThreads.push_back(doCheckTransThreadPtr);
                            doCheckTransThreadPtr->start();

                            break;
                        }
                    }
                }
                if (mirrorTimeOut) continue;
                //检查主机上报超时
                FDLOG("migrate") << __LINE__ << "|" << itrGroupInfo->second.masterLastReportTime
                                 << "|" << _switchTimeout << "|" << nowTime << endl;
                if (itrGroupInfo->second.masterLastReportTime == 0)
                {
                    errMsg = "SwitchThread::doSwitchCheck find master reportTime==0 " +
                             itrGroupInfo->first +
                             " masterServerName:" + itrGroupInfo->second.masterServer;
                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                }
                else if ((int(nowTime) - int(itrGroupInfo->second.masterLastReportTime)) >
                         _switchTimeout)
                {
                    errMsg = "SwitchThread::doSwitchCheck find TimeOut groupName:" +
                             itrGroupInfo->first +
                             " masterServerName:" + itrGroupInfo->second.masterServer;
                    errMsg += " now:" + I2S(int(nowTime)) +
                              " rePortTime:" + I2S(int(itrGroupInfo->second.masterLastReportTime)) +
                              " Timeout:" + I2S(_switchTimeout);
                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;

                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|"
                                    << itrGroupInfo->second.slaveLastReportTime << "|"
                                    << _switchTimeout << "|" << nowTime << endl;
                    if ((int(nowTime) - int(itrGroupInfo->second.slaveLastReportTime)) >
                        _switchTimeout)
                    {
                        FDLOG("switch")
                            << __LINE__ << "|" << __FUNCTION__ << "|"
                            << "SwitchThread::doSwitchCheck find TimeOut groupName:"
                            << itrGroupInfo->first
                            << " slaveServerName:" << itrGroupInfo->second.slaveServer << endl;
                        continue;
                    }
                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|"
                                    << "moduleName:" << itr->first
                                    << " | itrGroupInfo: " << itrGroupInfo->first << endl;
                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << endl;
                    g_app.addSwitchModule(itr->first);

                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << endl;
                    itrGroupInfo->second.status = 1;
                    SwitchWork tmpSwitchWork;
                    tmpSwitchWork.type = 0;  //主备切换
                    tmpSwitchWork.moduleName = itr->first;
                    tmpSwitchWork.groupName = itrGroupInfo->first;
                    tmpSwitchWork.masterServer = itrGroupInfo->second.masterServer;
                    tmpSwitchWork.slaveServer = itrGroupInfo->second.slaveServer;
                    tmpSwitchWork.switchType = switchType;
                    tmpSwitchWork.switchBeginTime = time(NULL);

                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << endl;
                    DoSwitchThreadPtr threadPtr =
                        new DoSwitchThread(g_app.createAdminRegProxyWrapper(),
                                           _dbHandle,
                                           g_app.getGlobalConfig().getSwitchTimeOut(300),
                                           g_app.getGlobalConfig().getSwitchBinLogDiffLimit(300),
                                           g_app.getGlobalConfig().getSwitchMaxTimes(3),
                                           g_app.getGlobalConfig().getDowngradeTimeout(30));
                    g_app.addSwitchThread(threadPtr);
                    threadPtr->CreateThread(tmpSwitchWork);
                    string strReason = "master heartbeat overtime";
                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << strReason << endl;

                    FDLOG("doCheck")
                        << __LINE__ << "|" << __FUNCTION__ << "|"
                        << "SwitchThread::doSwitchCheck module: ** master **"
                        << " masterServer:" << itrGroupInfo->second.masterServer
                        << " slaveServer: " << itrGroupInfo->second.slaveServer << endl;

                    DoCheckTransThreadPtr doCheckTransThreadPtr = new DoCheckTransThread();
                    doCheckTransThreadPtr->init(g_app.createAdminRegProxyWrapper(),
                                                itr->first,
                                                itrGroupInfo->second.masterServer,
                                                strReason,
                                                _dbHandle);
                    _doCheckTransThreads.push_back(doCheckTransThreadPtr);
                    doCheckTransThreadPtr->start();
                }

                if (itrGroupInfo->second.slaveLastReportTime == 0)
                {
                    errMsg = "SwitchThread::doSwitchCheck find slave reportTime==0 " +
                             itrGroupInfo->first +
                             " slaveServerName:" + itrGroupInfo->second.slaveServer;
                    FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                }
                else if ((int(nowTime) - int(itrGroupInfo->second.slaveLastReportTime)) >
                         _switchTimeout)
                {
                    string strReason = "slave heartbeat overtime";

                    FDLOG("doCheck")
                        << __LINE__ << "SwitchThread::doSwitchCheck module: ** slave **"
                        << " masterServer:" << itrGroupInfo->second.masterServer
                        << " slaveServer: " << itrGroupInfo->second.slaveServer << endl;

                    DoCheckTransThreadPtr doCheckTransThreadPtr1 = new DoCheckTransThread();
                    doCheckTransThreadPtr1->init(g_app.createAdminRegProxyWrapper(),
                                                 itr->first,
                                                 itrGroupInfo->second.slaveServer,
                                                 strReason,
                                                 _dbHandle);
                    _doCheckTransThreads.push_back(doCheckTransThreadPtr1);
                    doCheckTransThreadPtr1->start();
                    itrGroupInfo->second.slaveLastReportTime = 0;
                }
            }
        }
    }
    catch (TarsException &ex)
    {
        FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|"
                        << "SwitchThread::doSwitchCheck Tars exception:" << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("SwitchThread::doSwitchCheck|") + ex.what());
    }
    catch (exception &ex)
    {
        FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|"
                        << "SwitchThread::doSwitchCheck unkown exception " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("SwitchThread::doSwitchCheck|unkown exception ") + ex.what());
    }
}

void SwitchThread::doSlaveCheck()
{
    time_t nowTime = TC_TimeProvider::getInstance()->getNow();
    if (nowTime - _lastNotifyTime >= 300)
    {
        _lastNotifyTime = nowTime;
    }
    try
    {
        TC_ThreadLock::Lock lock1(g_app.getHeartbeatInfoLock());
        HeartbeatInfo &heartbeat = g_app.getHeartbeatInfo();
        map<string, map<string, GroupHeartBeatInfo>>::iterator itr;
        string errMsg;
        for (itr = heartbeat.begin(); itr != heartbeat.end(); itr++)
        {
            if (itr->second.size() == 0) continue;

            map<string, GroupHeartBeatInfo>::iterator itrGroupInfo;
            for (itrGroupInfo = itr->second.begin(); itrGroupInfo != itr->second.end();
                 itrGroupInfo++)
            {
                if (itrGroupInfo->second.status != 0)
                {
                    errMsg = "SwitchThread::doSlaveCheck module is Switching:" + itr->first +
                             " groupName:" + itrGroupInfo->first +
                             " status:" + I2S(itrGroupInfo->second.status);
                    FDLOG("slave") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                    continue;
                }
                //检查镜像上报超时
                if (itrGroupInfo->second.mirrorInfo.size() != 0)
                {
                    map<string, vector<string>>::iterator itrMirrorIdc;
                    for (itrMirrorIdc = itrGroupInfo->second.mirrorIdc.begin();
                         itrMirrorIdc != itrGroupInfo->second.mirrorIdc.end();
                         itrMirrorIdc++)
                    {
                        if (itrMirrorIdc->second.size() == 1)
                        {
                            continue;
                        }
                        else if (itrMirrorIdc->second.size() == 2)
                        {
                            //检查服务状态是否正确
                            if (itrGroupInfo->second.serverStatus[itrMirrorIdc->second[1]] != 0)
                            {
                                errMsg =
                                    "SwitchThread::doSlaveCheck server can't be read, please "
                                    "check! module:" +
                                    itr->first + " groupName:" + itrGroupInfo->first +
                                    " serverName:" + itrMirrorIdc->second[1] + " status:" +
                                    I2S(itrGroupInfo->second.serverStatus[itrMirrorIdc->second[1]]);
                                FDLOG("slave")
                                    << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                                continue;
                            }
                            time_t mirroSlaveTime =
                                itrGroupInfo->second.mirrorInfo[itrMirrorIdc->second[1]];
                            if (mirroSlaveTime == 0)
                            {
                                errMsg = "SwitchThread::doSlaveCheck find mirror reportTime==0" +
                                         itrMirrorIdc->second[1] +
                                         " mirrorName:" + itrMirrorIdc->second[1];
                                FDLOG("slave")
                                    << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                            }
                            else if ((int(nowTime) - int(mirroSlaveTime)) > _slaveTimeout)
                            {
                                errMsg = "SwitchThread::doSlaveCheck find TimeOut groupName:" +
                                         itrGroupInfo->first +
                                         " ServerName:" + itrMirrorIdc->second[1];
                                errMsg += " now:" + I2S(int(nowTime)) +
                                          " rePortTime:" + I2S(int(mirroSlaveTime)) +
                                          " Timeout:" + I2S(_slaveTimeout);
                                FDLOG("slave")
                                    << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;

                                itrGroupInfo->second.serverStatus[itrMirrorIdc->second[1]] = -1;
                                SwitchWork tmpSwitchWork;
                                tmpSwitchWork.type = 3;  //备机不可读
                                tmpSwitchWork.moduleName = itr->first;
                                tmpSwitchWork.groupName = itrGroupInfo->first;
                                tmpSwitchWork.masterServer = "";
                                tmpSwitchWork.slaveServer = itrMirrorIdc->second[1];

                                DoSwitchThreadPtr threadPtr = new DoSwitchThread(
                                    g_app.createAdminRegProxyWrapper(),
                                    _dbHandle,
                                    g_app.getGlobalConfig().getSwitchTimeOut(300),
                                    g_app.getGlobalConfig().getSwitchBinLogDiffLimit(300),
                                    g_app.getGlobalConfig().getSwitchMaxTimes(3),
                                    g_app.getGlobalConfig().getDowngradeTimeout(30));
                                g_app.addSwitchThread(threadPtr);
                                threadPtr->CreateThread(tmpSwitchWork);
                                continue;
                            }
                        }
                    }
                }

                //检查服务状态是否正确
                if (itrGroupInfo->second.serverStatus[itrGroupInfo->second.slaveServer] != 0)
                {
                    string err =
                        "SwitchThread::doSlaveCheck server can't be read, please check! module:" +
                        itr->first + " groupName:" + itrGroupInfo->first +
                        " serverName:" + itrGroupInfo->second.slaveServer + " status:" +
                        I2S(itrGroupInfo->second.serverStatus[itrGroupInfo->second.slaveServer]);
                    FDLOG("slave") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                    continue;
                }
                //检查备机上报超时
                if (itrGroupInfo->second.slaveLastReportTime == 0)
                {
                    errMsg = "SwitchThread::doSlaveCheck find slave reportTime==0 " +
                             itrGroupInfo->first +
                             " slaveServerName:" + itrGroupInfo->second.slaveServer;
                    FDLOG("slave") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
                }
                else if ((int(nowTime) - int(itrGroupInfo->second.slaveLastReportTime)) >
                         _slaveTimeout)
                {
                    errMsg =
                        "SwitchThread::doSlaveCheck find TimeOut groupName:" + itrGroupInfo->first +
                        " ServerName:" + itrGroupInfo->second.slaveServer;
                    errMsg += " now:" + I2S(int(nowTime)) +
                              " rePortTime:" + I2S(int(itrGroupInfo->second.slaveLastReportTime)) +
                              " Timeout:" + I2S(_slaveTimeout);
                    FDLOG("slave") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;

                    SwitchWork tmpSwitchWork;
                    tmpSwitchWork.type = 3;  //备机不可读
                    tmpSwitchWork.moduleName = itr->first;
                    tmpSwitchWork.groupName = itrGroupInfo->first;
                    tmpSwitchWork.masterServer = "";
                    tmpSwitchWork.slaveServer = itrGroupInfo->second.slaveServer;

                    DoSwitchThreadPtr threadPtr =
                        new DoSwitchThread(g_app.createAdminRegProxyWrapper(),
                                           _dbHandle,
                                           g_app.getGlobalConfig().getSwitchTimeOut(300),
                                           g_app.getGlobalConfig().getSwitchBinLogDiffLimit(300),
                                           g_app.getGlobalConfig().getSwitchMaxTimes(3),
                                           g_app.getGlobalConfig().getDowngradeTimeout(30));
                    g_app.addSwitchThread(threadPtr);
                    threadPtr->CreateThread(tmpSwitchWork);
                }
            }
        }
    }
    catch (TarsException &ex)
    {
        FDLOG("slave") << __LINE__ << "|" << __FUNCTION__ << "|"
                       << "SwitchThread::doSlaveCheck Tars exception:" << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("SwitchThread::doSlaveCheck|") + ex.what());
    }
    catch (exception &ex)
    {
        FDLOG("slave") << __LINE__ << "|" << __FUNCTION__ << "|"
                       << "SwitchThread::doSlaveCheck unkown exception " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("SwitchThread::doSlaveCheck|unkown exception ") + ex.what());
    }
}

/////////////////////////////////////////////

int SwitchThread::checkServerSettingState(const string &serverName)
{
    try
    {
        RouterClientPrx prx = Application::getCommunicator()->stringToProxy<RouterClientPrx>(
            serverName + ".RouterClientObj");

        //查看服务的状态
        vector<EndpointInfo> activeEndPoint;
        vector<EndpointInfo> inactiveEndPoint;
        prx->tars_endpointsAll(activeEndPoint, inactiveEndPoint);

        if ((activeEndPoint.size() > 0) && (inactiveEndPoint.size() == 0))
        {
            TLOGERROR("[SwitchThread::checkServerSettingState] server is active! serverName:"
                      << serverName << endl);
            return -1;
        }
        else if ((activeEndPoint.size() == 0) && (inactiveEndPoint.size() > 0))
        {
            string sIp;
            sIp = inactiveEndPoint[0].host();

            //获取服务状态
            tars::ServerStateDesc state;
            string resultStr;
            int Ret =
                _adminProxy->getServerState("DCache", serverName.substr(7), sIp, state, resultStr);

            TLOGERROR("[SwitchThread::checkServerSettingState] state.settingStateInReg:"
                      << state.settingStateInReg << endl);

            if ((Ret == 0) && (state.settingStateInReg == "inactive"))
            {  //服务被停了
                Ret = _adminProxy->getServerState(
                    "DCache", serverName.substr(7), sIp, state, resultStr);
                if (Ret == 0 && (state.settingStateInReg == "inactive"))
                {
                    return 0;
                }
            }
            else if (Ret != 0)
            {  //获取服务状态出错
                TLOGERROR("[SwitchThread::checkServerSettingState] get server state error! ret:"
                          << Ret << endl);
                return -1;
            }
            else
            {  //服务被设置启动
                return 1;
            }
        }
        else
        {
            //主控找不到服务的ip信息
            TLOGERROR(
                "[SwitchThread::checkServerSettingState] some thing wrong! activeEndPoint.size():"
                << activeEndPoint.size() << " inactiveEndPoint.size():" << inactiveEndPoint.size()
                << " serverName:" << serverName << endl);
            return -1;
        }
    }
    catch (exception &ex)
    {
        TLOGERROR(FILE_FUN << "[SwitchThread::checkServerSettingState] exception:" << ex.what()
                           << endl);
        return -1;
    }

    return -1;
}

int SwitchThread::checkMachineDown(const string &serverName, const string &serverIp)
{
    /**
     * 判断节点是否死机，首先从relation库中获取该节点上的所有cache服务
     * 如果存在设置状态/当前状态为active/active的服务，则判断为没有死机
     * else如果存在avtive/inactive的服务，则判断为死机
     **/
    try
    {
        vector<string> vAllServerInIp;
        int iRet = _dbHandle->getAllServerInIp(serverIp, vAllServerInIp);
        if (iRet != 0)
        {
            TLOGERROR("[SwitchThread::checkMachineDown] getAllServerInIp error get ret:" << iRet
                                                                                         << endl);
            FDLOG("migrate") << "[SwitchThread::checkMachineDown] getAllServerInIp error get ret:"
                             << iRet << endl;
            return -1;
        }

        int count = 0;  //记录设置状态active，当前状态inactive的数量
        for (size_t si = 0; si < vAllServerInIp.size(); si++)
        {
            //获取服务状态
            tars::ServerStateDesc state;
            string resultStr;
            try
            {
                int Ret =
                    _adminProxy->getServerState("DCache",
                                                vAllServerInIp[si],
                                                serverIp,
                                                state,
                                                resultStr);  //不用主控调服务状态，直接调用服务
                if (Ret != 0)
                {  //获取服务状态出错
                    TLOGERROR("[SwitchThread::checkServerSettingState] get server state error! ret:"
                              << Ret << endl);
                    return -1;
                }

                if (state.settingStateInReg == "active" && state.presentStateInReg == "active")
                {
                    TLOGDEBUG("[SwitchThread::checkMachineDown]in ip:"
                              << serverIp << ",server:" << vAllServerInIp[si]
                              << " is active/active. 0 returned.\n");
                    FDLOG("migrate")
                        << "[SwitchThread::checkMachineDown]in ip:" << serverIp
                        << ",server:" << vAllServerInIp[si] << " is active/active. 0 returned.\n";
                    return 0;
                }
                if (state.settingStateInReg == "active" && state.presentStateInReg == "inactive")
                {
                    count++;
                }
            }
            catch (exception &ex)
            {
                TLOGERROR(FILE_FUN << "[SwitchThread::checkMachineDown]ip:" << serverIp
                                   << ",server:" << vAllServerInIp[si] << " with exception"
                                   << endl);
                FDLOG("migrate") << "[SwitchThread::checkMachineDown]ip:" << serverIp
                                 << ",server:" << vAllServerInIp[si] << " with exception \n";
                return -2;
            }
        }
        if (count > 0)
        {
            TLOGDEBUG(FILE_FUN << "[SwitchThread::checkMachineDown]ip:" << serverIp << " is down"
                               << endl);
            FDLOG("migrate") << "[SwitchThread::checkMachineDown]ip:" << serverIp << " is down \n";
            return 1;
        }
    }
    catch (exception &ex)
    {
        TLOGERROR(FILE_FUN << "[SwitchThread::checkMachineDown]ip:" << serverIp << " is down"
                           << endl);
        FDLOG("migrate") << "[SwitchThread::checkMachineDown]ip:" << serverIp << " is down \n";
        return -2;
    }
    return -1;
}

int SwitchThread::doCheckTrans(const string &moduleName,
                               const string &serverName,
                               const string &reasonStr)
{
    try
    {
        FDLOG("migrate") << __LINE__ << "|moduleName:" << moduleName << "|serverName:" << serverName
                         << "|reasonStr:" << reasonStr << endl;
        //用于获取服务名对应的ip
        PackTable tmpPackTable;
        int gRet = _dbHandle->getPackTable(moduleName, tmpPackTable);
        if (gRet != 0)
        {
            string errMsg =
                "[SwitchThread::doCheckTrans]doMigrateCheck not find moduleName in "
                "packTable, moduleName:" +
                moduleName;
            FDLOG("migrate") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            return -1;
        }

        ServerInfo tmpServerInfo;
        map<string, ServerInfo>::const_iterator mServerItr =
            tmpPackTable.serverList.find(serverName);
        if (mServerItr == tmpPackTable.serverList.end())
        {
            string errMsg =
                "[SwitchThread::doCheckTrans]doMigrateCheck not find ip in packTable, serverName:" +
                serverName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("migrate") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            return -1;
        }

        int iRet = checkMachineDown(serverName, mServerItr->second.ip);
        if (iRet == 1)
        {
            FDLOG("migrate") << __LINE__ << "|serverName:" << serverName
                             << " status down, start addMirgrateInfo\n";
            string strIp = mServerItr->second.ip;
            string strReason = reasonStr;
            int iRet = _dbHandle->addMirgrateInfo(strIp, strReason);
            if (iRet != 0)
            {
                FDLOG("migrate") << __LINE__ << "|_dbHandle->addMirgrateInfo(" << strIp
                                 << " ) error get ret:" << iRet << endl;
                return -1;
            }
        }
        else if (iRet < 0)
        {
            FDLOG("migrate") << __LINE__ << "|[doCheckTrans]checkMachineDown("
                             << mServerItr->second.ip << " ) error get ret:" << iRet << endl;
            return iRet;
        }
    }
    catch (exception &ex)
    {
        TLOGERROR(FILE_FUN << "[SwitchThread::doCheckTrans] exception:" << ex.what() << endl);
        FDLOG("migrate") << "[SwitchThread::doCheckTrans] exception:" << ex.what() << endl;
        return -1;
    }
    return 0;
}

void SwitchThread::downgrade() { _lastNotifyTime = 0; }

/////////////////////////////////////////////

DoSwitchThread::DoSwitchThread(AdminProxyWrapperPtr adminProxy,
                               std::shared_ptr<DbHandle> dbHandle,
                               int switchTimeOut,
                               int switchBinLogDiffLimit,
                               int switchMaxTimes,
                               int downGradeTimeout)
    : _dbHandle(dbHandle),
//      _switchTimeout(switchTimeOut),
      _switchBlogDifLimit(switchBinLogDiffLimit),
      _switchMaxTimes(switchMaxTimes),
      _downGradeTimeout(downGradeTimeout)
{
    string _locator = Application::getCommunicator()->getProperty("locator");

    if (_locator.find_first_not_of('@') == string::npos)
    {
        TLOGERROR(FILE_FUN << "[Locator is not valid:" << _locator << "]" << endl);
        throw TarsRegistryException("locator is not valid:" + _locator);
    }

    _adminProxy = g_app.createAdminRegProxyWrapper();
    _adminProxy->tars_timeout(3000);
    _isFinish = false;
    _start = false;
}

void DoSwitchThread::CreateThread(SwitchWork task)
{
    //创建线程

    if (!_start)
    {
        _start = true;

        _work = task;

        if (pthread_create(&thread, NULL, run, (void *)this) != 0)
        {
            _start = false;
            RUNTIME_ERROR("Create DeleteThread fail");
        }
    }
}

void DoSwitchThread::doSwitch()
{
    if (_work.type == 0)
    {
        doSwitchMaterSlave(_work);
    }
    else if (_work.type == 3)
    {
        doSlaveDead(_work);
    }
    else
    {
        doSwitchMirror(_work);
    }
}

void DoSwitchThread::doSlaveDead(const SwitchWork &switchWork)
{
    _dbHandle->setServerstatus(
        switchWork.moduleName, switchWork.groupName, switchWork.slaveServer, -1);

    setHeartBeatServerStatus(
        switchWork.moduleName, switchWork.groupName, switchWork.slaveServer, -1);

    _isFinish = true;

    return;
}

void DoSwitchThread::doSwitchMirror(const SwitchWork &switchWork)
{
    // 镜像没有备机，不存在镜像备机切换
    assert(switchWork.type == 2);

    string moduleName = switchWork.moduleName;
    string groupName = switchWork.groupName;
    string mirrorName = switchWork.mirrorServer;
    string mirrorIdc = switchWork.mirrorIdc;
    string mirrorMaster = switchWork.mirrorServer;
    string mirrorSlave;
    string errMsg;
    int switchResult = 0;
    int groupStatus = 0;
    do
    {
        //检查切换次数是否超过限制
        if (checkSwitchTimes(moduleName, groupName, switchWork.type) != 0)
        {
            errMsg = "DoSwitchThread::doSwitchMirror switch times over the SwitchMaxTimes: " +
                     I2S(_switchMaxTimes) + ", so not do switch groupName:" + groupName +
                     ", mirrorServer:" + mirrorName;
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            TARS_NOTIFY_ERROR(errMsg);
            doSwitchMirrorOver(moduleName, groupName);
            switchResult = 2;
            break;
        }

        //先检查服务的设置状态
        //如果镜像被手动关闭，就不需做切换了
        FDLOG("switch") << __LINE__ << "check server setting state." << endl;
        FDLOG("mirror") << __LINE__ << "check server setting state." << endl;
        if ((checkServerSettingState(mirrorMaster) == 0))
        {
            errMsg =
                "SwitchThread::doSwitchCheck server is set inactive mirrorName:" + mirrorMaster;
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchMirrorOver(moduleName, groupName);
            switchResult = 2;
            break;
        }

        FDLOG("switch") << __LINE__ << "heartBeatSend to mirrorServer mirrorName:" << mirrorName
                        << endl;
        int iRet = heartBeatSend(mirrorName);
        if (iRet == 0)
        {
            errMsg = "heartBeatSend to mirrorServer ok:" + mirrorName;
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchMirrorOver(moduleName, groupName);
            switchResult = 2;
            break;
        }

        FDLOG("mirror") << __LINE__ << "switchWork.type==" << switchWork.type << endl;
        iRet = chageMirrorBad(moduleName, groupName, mirrorName);
        if (iRet != 0)
        {
            FDLOG("switch") << __LINE__
                            << "SwitchThread::chageMirrorBad error  mirrorName:" << mirrorName
                            << endl;
            FDLOG("mirror") << __LINE__
                            << "SwitchThread::chageMirrorBad error  mirrorName:" << mirrorName
                            << endl;
            updateSwitchTimes(
                moduleName, groupName, switchWork.type, TC_TimeProvider::getInstance()->getNow());
            doSwitchMirrorOver(moduleName, groupName);
            switchResult = 3;
            break;
        }
        //日志告警
        errMsg = "switch mirror bad succ moudleName:" + moduleName + " groupName:" + groupName +
                 " mirrorName:" + mirrorName;
        TARS_NOTIFY_ERROR(errMsg);
        FDLOG("switch") << __LINE__ << errMsg << endl;
        FDLOG("mirror") << __LINE__ << errMsg << endl;
        updateSwitchTimes(
            moduleName, groupName, switchWork.type, TC_TimeProvider::getInstance()->getNow());
        doSwitchMirrorOver(
            moduleName,
            groupName,
            30,
            true,
            false,
            mirrorIdc);  //保持状态 等待人工处理 人工修改路由组状态后 重启或者reloadRouter
        switchResult = 1;
        groupStatus = 2;
        break;
    } while (0);

    //更新切换结果
    if (switchResult != 2)
    {
        //只有切换成功或者切换失败的才入库, 未切换的不需要
        _dbHandle->insertSwitchInfo(moduleName,
                                    groupName,
                                    "",
                                    "",
                                    mirrorIdc,
                                    mirrorMaster,
                                    mirrorSlave,
                                    "auto",
                                    errMsg,
                                    switchWork.type,
                                    switchResult,
                                    groupStatus,
                                    switchWork.switchBeginTime);
        FDLOG("switch") << __LINE__ << " insertSwitchInfo " << endl;
    }

    _isFinish = true;

    return;
}

int DoSwitchThread::slaveReady(const string &moduleName, const string &serverName, bool &bReady)
{
    bReady = false;
    string ServerObj = serverName + ".RouterClientObj";
    RouterClientPrx pRouterClientPrx =
        Application::getCommunicator()->stringToProxy<RouterClientPrx>(ServerObj);
    pRouterClientPrx->tars_timeout(3000);
    try
    {
        int iRet = pRouterClientPrx->isReadyForSwitch(moduleName, bReady);
        if (iRet != 0)
        {
            TLOGERROR(FILE_FUN << "DoSwitchThread::" << __FUNCTION__
                               << " invoke error:" << serverName << " ret:" << iRet << endl);
            return -1;
        }
        return 0;
    }
    catch (const TarsException &ex)
    {
        TLOGERROR(FILE_FUN << " exception:" << ex.what() << endl);
        try
        {
            int iRet = pRouterClientPrx->isReadyForSwitch(moduleName, bReady);
            if (iRet != 0)
            {
                TLOGERROR(FILE_FUN << " invoke error:" << serverName << " ret:" << iRet << endl);
                return -1;
            }
            return 0;
        }
        catch (const TarsException &ex)
        {
            TLOGERROR(FILE_FUN << " invoke " << serverName << " exception:" << ex.what() << endl);
        }
    }
    return -1;
}

void DoSwitchThread::doSwitchMaterSlave(const SwitchWork &switchWork)
{
    string moduleName = switchWork.moduleName;
    string groupName = switchWork.groupName;
    string masterName = switchWork.masterServer;
    string slaveName = switchWork.slaveServer;
    int switchType = switchWork.switchType;
    string errMsg;
    int switchResult = 0;
    int groupStatus = 0;
    do
    {
        //检查切换次数是否超过限制
        if (checkSwitchTimes(moduleName, groupName, switchWork.type) != 0)
        {
            errMsg = "DoSwitchThread::doSwitchMaterSlave switch times over the SwitchMaxTimes: " +
                     I2S(_switchMaxTimes) + ", so not do switch groupName:" + groupName +
                     ", masterServer:" + masterName + ", slaveServer:" + slaveName;
            FDLOG("slave") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            TARS_NOTIFY_ERROR(errMsg);
            doSwitchOver(moduleName, groupName);
            switchResult = 2;
            break;
        }

        //先检查服务的设置状态
        //如果主备机都设置成关闭，就不需做切换了
        FDLOG("switch") << "check server setting state." << endl;
        if ((checkServerSettingState(masterName) == 0) && (checkServerSettingState(slaveName) == 0))
        {
            errMsg = "SwitchThread::doSwitchCheck server is set inactive masterName:" + masterName;
            FDLOG("switch") << errMsg << endl;
            doSwitchOver(moduleName, groupName);
            switchResult = 2;
            break;
        }

        //向主机发送心跳
        FDLOG("switch") << "heartBeatSend to masterName:" << masterName << endl;
        int iRet = heartBeatSend(masterName);
        if (iRet == 0)
        {
            errMsg = "heartBeatSend to masterName ok:" + masterName;
            FDLOG("switch") << errMsg << endl;
            doSwitchOver(moduleName, groupName);
            switchResult = 2;
            break;
        }

        //向备机发送心跳
        FDLOG("switch") << "heartBeatSend to slaveName:" << slaveName << endl;
        iRet = heartBeatSend(slaveName);
        if (iRet == -1)
        {
            //检查备机是否因为下线导致心跳超时
            bool bOffline = false;
            iRet = _dbHandle->checkServerOffline(slaveName, bOffline);
            if ((iRet == 0) && bOffline)
            {
                errMsg =
                    "SwitchThread::doSwitchMaterSlave checkServerOffline slaveServer is offline:" +
                    slaveName;
                FDLOG("switch") << errMsg << endl;
                doSwitchOver(moduleName, groupName);
                switchResult = 2;
                break;
            }

            errMsg = "SwitchThread:: heartBeatSend error  slaveServer no ok:" + slaveName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << errMsg << endl;
            updateSwitchTimes(
                moduleName, groupName, switchWork.type, TC_TimeProvider::getInstance()->getNow());
            doSwitchOver(moduleName, groupName);
            switchResult = 3;
            break;
        }
        FDLOG("switch") << "heartBeatSend to slaveName  ok:" << slaveName << endl;

        //查询备机binlog差异
        FDLOG("switch") << "query slaveBinlogdif from slaveName:" << slaveName << endl;
        int diffBinlogTime;
        iRet = slaveBinlogdif(slaveName, diffBinlogTime);
        if (iRet == -1)
        {
            errMsg = "SwitchThread:: slaveBinlogdif error  slaveServer no ok:" + slaveName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << errMsg << endl;
            updateSwitchTimes(
                moduleName, groupName, switchWork.type, TC_TimeProvider::getInstance()->getNow());
            doSwitchOver(moduleName, groupName);
            switchResult = 3;
            break;
        }
        if (diffBinlogTime > _switchBlogDifLimit)
        {
            errMsg = "SwitchThread:: slaveBinlogdif diffBinlogTime(" + I2S(diffBinlogTime) +
                     ") > " + I2S(_switchBlogDifLimit) + " " + slaveName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << errMsg << endl;
            updateSwitchTimes(
                moduleName, groupName, switchWork.type, TC_TimeProvider::getInstance()->getNow());
            doSwitchOver(moduleName, groupName);
            switchResult = 3;
            break;
        }

        FDLOG("switch") << "query slaveBinlogdif from slaveName  ok:" << slaveName
                        << " diffBinlogTime:" << I2S(diffBinlogTime) << endl;

        bool bReady = false;
        iRet = slaveReady(moduleName, slaveName, bReady);
        if (iRet == -1)
        {
            errMsg = "SwitchThread:: check salve ready error  slaveServer no ok:" + slaveName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchOver(moduleName, groupName);
            switchResult = 3;
            break;
        }
        if (!bReady)
        {
            errMsg = "SwitchThread:: check salve ready false";
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchOver(moduleName, groupName);
            switchResult = 3;
            break;
        }

        iRet = disconFromMaster(moduleName, slaveName);
        if (iRet == -1)
        {
            errMsg = "SwitchThread:: notify slave disconect from master error  slaveServer no ok:" +
                     slaveName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchOver(moduleName, groupName);
            switchResult = 3;
            break;
        }

        iRet = chageRouterOnlyRead(moduleName, groupName);
        if (iRet != 0)
        {
            errMsg = "SwitchThread::chageRouterOnlyRead error  masterServer:" + masterName;
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchOver(moduleName, groupName);
            switchResult = 3;
            break;
        }

        //通知主机降级，不关心是否成功，
        TLOGDEBUG(FILE_FUN << "notify master downgrade." << endl);
        notifyMasterDowngrade(moduleName, masterName);

        //等待30s，目的是等待主机降级
        sleep(_downGradeTimeout);

        if (switchType == 1)
        {
            //日志告警
            errMsg = "switch readOnly succ moudleName:" + moduleName + " groupName:" + groupName +
                     " masterServer:" + masterName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchOver(moduleName,
                         groupName,
                         30,
                         true);  //一天只能不在进行check 保持状态 等待人工处理 人工修改路由组状态后
                                 //重启或者reloadRouter
            switchResult = 3;
            break;
        }

        //下发新的路由信息给主备机，备机应答成功即可
        iRet = setRouter4Switch(moduleName, groupName, masterName, slaveName);
        if (iRet == -1)
        {
            errMsg =
                "SwitchThread:: setRouter4Switch error, module:" + moduleName + "|" + groupName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchOver(moduleName, groupName, 30, true);
            switchResult = 3;
            break;
        }

        //修改内存、db中的路由信息
        iRet = _dbHandle->switchRWDbAndMem(moduleName, groupName, masterName, slaveName);
        if (iRet == 0)
        {
            groupStatus = 0;
            errMsg = "switch rw succ moudleName:" + moduleName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchOver(moduleName, groupName, 30, false, true);
            _dbHandle->updateStatusToRelationDB(slaveName, "M");
            _dbHandle->updateStatusToRelationDB(masterName, "S");

            switchResult = 1;
            break;
        }
        else
        {
            errMsg = "switch rw error moudleName:" + moduleName;
            TARS_NOTIFY_ERROR(errMsg);
            FDLOG("switch") << __LINE__ << "|" << __FUNCTION__ << "|" << errMsg << endl;
            doSwitchOver(moduleName, groupName, 30, true);
            switchResult = 3;
            break;
        }
    } while (0);

    //更新切换结果
    if (switchResult != 2)
    {
        _dbHandle->insertSwitchInfo(moduleName,
                                    groupName,
                                    masterName,
                                    slaveName,
                                    "",
                                    "",
                                    "",
                                    "auto",
                                    errMsg,
                                    switchWork.type,
                                    switchResult,
                                    groupStatus,
                                    switchWork.switchBeginTime);
    }
    _isFinish = true;

    return;
}

int DoSwitchThread::notifyMasterDowngrade(const string &moduleName, const string &serverName)
{
    string ServerObj = serverName + ".RouterClientObj";
    RouterClientPrx pRouterClientPrx =
        Application::getCommunicator()->stringToProxy<RouterClientPrx>(ServerObj);
    pRouterClientPrx->tars_timeout(3000);
    try
    {
        pRouterClientPrx->async_notifyMasterDowngrade(NULL, moduleName);
    }
    catch (const TarsException &ex)
    {
        TLOGERROR(FILE_FUN << " invoke " << serverName << " exception:" << ex.what() << endl);
        try
        {
            pRouterClientPrx->async_notifyMasterDowngrade(NULL, moduleName);
        }
        catch (const TarsException &ex)
        {
            TLOGERROR(FILE_FUN << " invoke " << serverName << " exception:" << ex.what() << endl);
        }
    }

    return 0;
}

int DoSwitchThread::setRouter4Switch(const string &moduleName,
                                     const string &groupName,
                                     const string &oldMaster,
                                     const string &newMaster)
{
    PackTable packTable;
    int iRet =
        _dbHandle->getPackTable4SwitchRW(moduleName, groupName, oldMaster, newMaster, packTable);
    if (iRet != 0)
    {
        TLOGERROR(FILE_FUN << " getPackTable4SwitchRW error:" << moduleName << " ret:" << iRet
                           << endl);
        return -1;
    }

    //推送路由给新主机
    {
        string serverName = newMaster;
        string ServerObj = serverName + ".RouterClientObj";
        RouterClientPrx pRouterClientPrx =
            Application::getCommunicator()->stringToProxy<RouterClientPrx>(ServerObj);
        pRouterClientPrx->tars_timeout(3000);
        try
        {
            iRet = pRouterClientPrx->setRouterInfoForSwitch(moduleName, packTable);
            if (iRet != 0)
            {
                TLOGERROR(FILE_FUN << " invoke error:" << serverName << " ret:" << iRet << endl);
                return -1;
            }
        }
        catch (exception &ex)
        {
            TLOGERROR(FILE_FUN << " invoke " << serverName << " exception:" << ex.what() << endl);
            try
            {
                iRet = pRouterClientPrx->setRouterInfoForSwitch(moduleName, packTable);
                if (iRet != 0)
                {
                    TLOGERROR(FILE_FUN << " invoke error:" << serverName << " ret:" << iRet
                                       << endl);
                    return -1;
                }
            }
            catch (exception &ex)
            {
                TLOGERROR(FILE_FUN << " invoke " << serverName << " exception:" << ex.what()
                                   << endl);
                return -1;
            }
            catch (...)
            {
                TLOGERROR(FILE_FUN << " invoke " << serverName << " unknow exception" << endl);
                return -1;
            }
        }
        catch (...)
        {
            TLOGDEBUG(FILE_FUN << " invoke " << serverName << " unknow exception" << endl);
            return -1;
        }
    }

    //推送路由给旧主机,不关心返回结果
    {
        string serverName = oldMaster;
        string ServerObj = serverName + ".RouterClientObj";
        RouterClientPrx pRouterClientPrx =
            Application::getCommunicator()->stringToProxy<RouterClientPrx>(ServerObj);
        pRouterClientPrx->tars_timeout(3000);
        try
        {
            iRet = pRouterClientPrx->setRouterInfoForSwitch(moduleName, packTable);
            if (iRet != 0)
            {
                TLOGERROR(FILE_FUN << " invoke error:" << serverName << " ret:" << iRet
                                   << "| doesn't matter." << endl);
            }
        }
        catch (const TarsException &ex)
        {
            TLOGERROR(FILE_FUN << " invoke " << serverName << " exception:" << ex.what()
                               << "| doesn't matter." << endl);
        }
        return 0;
    }
}

int DoSwitchThread::disconFromMaster(const string &moduleName, const string &serverName)
{
    string ServerObj = serverName + ".RouterClientObj";
    RouterClientPrx pRouterClientPrx =
        Application::getCommunicator()->stringToProxy<RouterClientPrx>(ServerObj);
    pRouterClientPrx->tars_timeout(3000);
    try
    {
        int iRet = pRouterClientPrx->disconnectFromMaster(moduleName);
        if (iRet != 0)
        {
            TLOGERROR(FILE_FUN << " invoke error:" << serverName << " ret:" << iRet << endl);
            return -1;
        }
        return 0;
    }
    catch (const TarsException &ex)
    {
        TLOGERROR(FILE_FUN << " invoke " << serverName << " exception:" << ex.what() << endl);
        try
        {
            int iRet = pRouterClientPrx->disconnectFromMaster(moduleName);
            if (iRet != 0)
            {
                TLOGERROR(FILE_FUN << " invoke error:" << serverName << " ret:" << iRet << endl);
                return -1;
            }
            return 0;
        }
        catch (const TarsException &ex)
        {
            TLOGERROR(FILE_FUN << " invoke " << serverName << " exception:" << ex.what() << endl);
        }
    }
    return -1;
}

int DoSwitchThread::checkServerSettingState(const string &serverName)
{
    try
    {
        RouterClientPrx prx = Application::getCommunicator()->stringToProxy<RouterClientPrx>(
            serverName + ".RouterClientObj");

        //查看服务的状态
        vector<EndpointInfo> activeEndPoint;
        vector<EndpointInfo> inactiveEndPoint;
        prx->tars_endpointsAll(activeEndPoint, inactiveEndPoint);

        if ((activeEndPoint.size() > 0) && (inactiveEndPoint.size() == 0))
        {
            TLOGERROR("[SwitchThread::checkServerSettingState] server is active! serverName:"
                      << serverName << endl);
            return -1;
        }
        else if ((activeEndPoint.size() == 0) && (inactiveEndPoint.size() > 0))
        {
            string sIp;
            sIp = inactiveEndPoint[0].host();

            //获取服务状态
            tars::ServerStateDesc state;
            string resultStr;
            int Ret =
                _adminProxy->getServerState("DCache", serverName.substr(7), sIp, state, resultStr);

            TLOGERROR("[SwitchThread::checkServerSettingState] state.settingStateInReg:"
                      << state.settingStateInReg << endl);

            if ((Ret == 0) && (state.settingStateInReg == "inactive"))
            {  //服务被停了
                return 0;
            }
            else if (Ret == 1003)
            {  // 1003:服务下线
                return 0;
            }
            else if (Ret != 0 && Ret != 1003)
            {  //获取服务状态出错
                TLOGERROR("[SwitchThread::checkServerSettingState] get server state error! ret:"
                          << Ret << endl);
                return -1;
            }
            else
            {  //服务被设置启动
                return 1;
            }
        }
        else
        {
            //主控找不到服务的ip信息
            TLOGERROR(
                "[SwitchThread::checkServerSettingState] some thing wrong! activeEndPoint.size():"
                << activeEndPoint.size() << " inactiveEndPoint.size():" << inactiveEndPoint.size()
                << " serverName:" << serverName << endl);
            return -1;
        }
    }
    catch (exception &ex)
    {
        TLOGERROR(FILE_FUN << "[SwitchThread::checkServerSettingState] exception:" << ex.what()
                           << endl);
        return -1;
    }

    return -1;
}

int DoSwitchThread::setHeartBeatServerStatus(const string &moduleName,
                                             const string &groupName,
                                             const string &serverName,
                                             const int status)
{
    g_app.setHeartbeatServerStatus(moduleName, groupName, serverName, status);
    return 0;
}

int DoSwitchThread::chageRouterOnlyRead(const string &moduleName, const string &groupName)
{
    return _dbHandle->switchReadOnlyInDbAndMem(moduleName, groupName);
}

int DoSwitchThread::chageMirrorBad(const string &moduleName,
                                   const string &groupName,
                                   const string &serverName)
{
    return _dbHandle->switchMirrorInDbAndMem(moduleName, groupName, serverName);
}

int DoSwitchThread::chageMirrorBad(const string &moduleName,
                                   const string &groupName,
                                   const string &mirrorIdc,
                                   string &masterImage,
                                   string &slaveImage)
{
    return _dbHandle->switchMirrorInDbAndMemByIdc(
        moduleName, groupName, mirrorIdc, true, masterImage, slaveImage);
}

int DoSwitchThread::heartBeatSend(const string &serverName)
{
    string ServerObj = serverName + ".RouterClientObj";
    RouterClientPrx pRouterClientPrx =
        Application::getCommunicator()->stringToProxy<RouterClientPrx>(ServerObj);
    pRouterClientPrx->tars_timeout(3000);
    try
    {
        pRouterClientPrx->helloBaby();
        FDLOG("switch") << "SwitchThread::doSwitch send heartBeat ok  ServerName:" << serverName
                        << endl;
        return 0;
    }
    catch (const TarsException &ex)
    {
        FDLOG("switch") << "SwitchThread::doSwitch catch exception: " << ex.what() << endl;
        try
        {
            pRouterClientPrx->helloBaby();
            FDLOG("switch") << "SwitchThread::doSwitch send heartBeat ok  ServerName:" << serverName
                            << endl;
            // doSwitchOver(moduleName,groupName);
            return 0;
        }
        catch (const TarsException &ex)
        {
            FDLOG("switch") << "SwitchThread::doSwitch catch exception: " << ex.what() << endl;
        }
    }
    return -1;
}
int DoSwitchThread::slaveBinlogdif(const string &serverName, int &diffBinlogTime)
{
    string ServerObj = serverName + ".RouterClientObj";
    RouterClientPrx pRouterClientPrx =
        Application::getCommunicator()->stringToProxy<RouterClientPrx>(ServerObj);
    pRouterClientPrx->tars_timeout(3000);
    try
    {
        int iRet = pRouterClientPrx->getBinlogdif(diffBinlogTime);
        if (iRet != 0)
        {
            FDLOG("switch") << "DoSwitchThread::slaveBinlogdif getBinlogdif error" << serverName
                            << " ret:" << iRet << endl;
            return -1;
        }
        return 0;
    }
    catch (const TarsException &ex)
    {
        FDLOG("switch") << "DoSwitchThread::slaveBinlogdif catch exception: " << ex.what()
                        << " serverName" << serverName << endl;
        try
        {
            int iRet = pRouterClientPrx->getBinlogdif(diffBinlogTime);
            if (iRet != 0)
            {
                FDLOG("switch") << "DoSwitchThread::slaveBinlogdif getBinlogdif error" << serverName
                                << " ret:" << iRet << endl;
                return -1;
            }
            return 0;
        }
        catch (const TarsException &ex)
        {
            FDLOG("switch") << "DoSwitchThread::slaveBinlogdif catch exception: " << ex.what()
                            << " serverName" << serverName << endl;
        }
    }
    return -1;
}
void DoSwitchThread::doSwitchOver(const string &moduleName,
                                  const string &groupName,
                                  int timeReportSet,
                                  bool readOver,
                                  bool bMasterChange)
{
    g_app.swapMasterSlave(moduleName, groupName, timeReportSet, readOver, bMasterChange);

    //主备切换完成后，如果该组作为源组或者目的组正在做迁移，需要唤醒迁移线程
    if (bMasterChange && _dbHandle->hasTransferingLoc(moduleName, groupName))
    {
        map<string, map<string, TransferMutexCondPtr>> transferMutextCond;
        if (_dbHandle->getTransferMutexCond(transferMutextCond) == 0)
        {
            map<string, map<string, TransferMutexCondPtr>>::iterator _it =
                transferMutextCond.find(moduleName);
            if (_it != transferMutextCond.end())
            {
                TLOGDEBUG(FILE_FUN
                          << "[DoSwitchThread::doSwitchOver] weakup the waiting transfering thread."
                          << endl);
                map<string, TransferMutexCondPtr> &_mMutexCond = _it->second;
                map<string, TransferMutexCondPtr>::iterator _itPtr = _mMutexCond.begin();
                //遍历唤醒所有等待的迁移线程
                for (; _itPtr != _mMutexCond.end(); ++_itPtr)
                {
                    TransferMutexCondPtr &mutexCond = _itPtr->second;
                    pthread_mutex_lock(&(mutexCond->_mutex));
                    pthread_cond_signal(&(mutexCond->_cond));
                    pthread_mutex_unlock(&(mutexCond->_mutex));
                }
            }
        }
    }
}
void DoSwitchThread::doSwitchMirrorOver(const string &moduleName,
                                        const string &groupName,
                                        int timeReportSet,
                                        bool succ,
                                        bool bMasterMirrorChange,
                                        const string &sIdc)
{
    g_app.switchMirrorOver(moduleName, groupName, timeReportSet, succ, bMasterMirrorChange, sIdc);
}

int DoSwitchThread::checkSwitchTimes(const string &moduleName,
                                     const string &groupName,
                                     const int &type)
{
    return g_app.checkSwitchTimes(moduleName, groupName, type, _switchMaxTimes);
}

void DoSwitchThread::updateSwitchTimes(const string &moduleName,
                                       const string &groupName,
                                       const int &type,
                                       const time_t &tNowTime)
{
    g_app.addSwitchTime(moduleName, groupName, type, tNowTime);
}

void DoSwitchThread::resetSwitchTimes() { g_app.resetSwitchTimes(); }
