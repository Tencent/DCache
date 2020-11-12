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
// Router集群化之后，所有发到备机上的请求要转发给主机处理并通过备机返回结果给调用者。
// 备机向主机转发请求前要更新主机的proxy，因为主机可能切换。
#include "RouterImp.h"
#include "DbHandle.h"
//#include "EtcdHandle.h"
#include "Transfer.h"

extern RouterServer g_app;

// 一天中的秒数
const int SECONDS_IN_DAY = 86400;

// 从全局的heartBeatInfo中查找指定的GroupHeartBeatInfo。
// 使用时注意要在外部对lockForHeartBeat进行加锁。
int RouterImp::queryGroupHeartBeatInfo(const string &moduleName,
                                       const string &groupName,
                                       string &errMsg,
                                       GroupHeartBeatInfo **info)
{
    HeartbeatInfo &heartbeatInfo = g_app.getHeartbeatInfo();
    map<string, map<string, GroupHeartBeatInfo>>::iterator it1 = heartbeatInfo.find(moduleName);
    if (it1 == heartbeatInfo.end())
    {
        errMsg = "queryGroupHeartBeatInfo : Module not found : " + moduleName;
        return -1;
    }

    map<string, GroupHeartBeatInfo>::iterator it2 = it1->second.find(groupName);
    if (it2 == it1->second.end())
    {
        errMsg = "group not found in " + moduleName + " | " + groupName;
        return -1;
    }

    *info = &(it2->second);
    TLOGDEBUG(FILE_FUN << "it2->second.masterServer = " << it2->second.masterServer << endl);
    return 0;
}

// 从全局的heartBeatInfo中查找指定的MirrorInfo。
// 使用时注意要在外部对lockForHeartBeat进行加锁。
int RouterImp::queryMirrorInfo(const string &moduleName,
                               const string &groupName,
                               const string &mirrorIdc,
                               string &errMsg,
                               vector<string> **idcList)
{
    GroupHeartBeatInfo *groupInfo = NULL;
    if (queryGroupHeartBeatInfo(moduleName, groupName, errMsg, &groupInfo) != 0) return -1;

    map<string, vector<string>>::iterator itrMirrorIdc = groupInfo->mirrorIdc.find(mirrorIdc);
    if (itrMirrorIdc == groupInfo->mirrorIdc.end())
    {
        errMsg = "idc not found in " + moduleName + " | " + groupName + " | " + mirrorIdc;
        return -1;
    }

    *idcList = &(itrMirrorIdc->second);
    return 0;
}

void RouterImp::initialize()
{
    _dbHandle = g_app.getDbHandle();

    int ret = _dbHandle->getIdcMap(_cityToIDC);
    if (ret != 0)
    {
        TLOGERROR(FILE_FUN << "getIdcMap error:" << ret << "| try aggin." << endl);
        sleep(1);
        ret = _dbHandle->getIdcMap(_cityToIDC);
        if (ret != 0)
        {
            TLOGERROR(FILE_FUN << "getIdcMap error:" << ret << endl);
        }
    }

    std::string obj = ServerConfig::Application + "." + ServerConfig::ServerName + ".RouterObj";
    std::string adapter = obj + "Adapter";
    TC_Endpoint ep = g_app.getEpollServer()->getBindAdapter(adapter)->getEndpoint();
    _selfObj = obj + "@" + ep.toString();
}

tars::Int32 RouterImp::getRouterInfo(const string &moduleName,
                                     PackTable &packTable,
                                     tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->getRouterInfo(moduleName, packTable);
    }

    int rc = _dbHandle->getPackTable(moduleName, packTable);
    TLOGDEBUG(current->getIp() << "|" << current->getPort() << "|" << __FUNCTION__ << "|"
                               << moduleName << "|" << rc << endl);

    return rc == 0 ? ROUTER_SUCC : ROUTER_INFO_ERR;
}

tars::Int32 RouterImp::getRouterInfoFromCache(const string &moduleName,
                                              PackTable &packTable,
                                              tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->getRouterInfoFromCache(moduleName, packTable);
    }

    if (g_app.isModuleSwitching(moduleName))
    {
        TLOGDEBUG(current->getIp()
                  << "|" << current->getPort() << "|" << __FUNCTION__ << "|" << moduleName << "|"
                  << "SWICTHING" << endl);
        return ROUTER_INFO_ERR;
    }

    int rc = _dbHandle->getPackTable(moduleName, packTable);
    TLOGDEBUG(current->getIp() << "|" << current->getPort() << "|" << __FUNCTION__ << "|"
                               << moduleName << "|" << rc << endl);

    return rc == 0 ? ROUTER_SUCC : ROUTER_INFO_ERR;
}

tars::Int32 RouterImp::getTransRouterInfo(const string &moduleName,
                                          tars::Int32 &transInfoListVer,
                                          vector<TransferInfo> &transferingInfoList,
                                          PackTable &packTable,
                                          tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->getTransRouterInfo(
            moduleName, transInfoListVer, transferingInfoList, packTable);
    }

    int rc = _dbHandle->getRouterInfo(moduleName, transInfoListVer, transferingInfoList, packTable);
    TLOGDEBUG(current->getIp() << "|" << current->getPort() << "|" << __FUNCTION__ << "|"
                               << moduleName << "|" << rc << endl);

    return rc == 0 ? ROUTER_SUCC : ROUTER_INFO_ERR;
}

tars::Int32 RouterImp::getVersion(const std::string &moduleName, tars::TarsCurrentPtr current)
{
    int version = _dbHandle->getVersion(moduleName);
    TLOGDEBUG(FILE_FUN << current->getIp() << "|" << current->getPort() << "|" << __FUNCTION__
                       << "|" << moduleName << "|" << version << endl);
    return version;
}

tars::Int32 RouterImp::getRouterVersion(const string &moduleName,
                                        tars::Int32 &version,
                                        tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->getRouterVersion(moduleName, version);
    }

    int v = _dbHandle->getVersion(moduleName);
    TLOGDEBUG(current->getIp() << "|" << current->getPort() << "|" << __FUNCTION__ << "|"
                               << moduleName << "|" << v << endl);

    if (v != -1)
    {
        version = v;
        return ROUTER_SUCC;
    }
    return ROUTER_INFO_ERR;
}

tars::Int32 RouterImp::getRouterVersionBatch(const vector<string> &moduleList,
                                             map<string, tars::Int32> &mapModuleVersion,
                                             tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->getRouterVersionBatch(moduleList, mapModuleVersion);
    }

    TLOGDEBUG(FILE_FUN << "get request from : " << current->getIp() << ":" << current->getPort()
                       << endl);

    tars::Int32 v;
    vector<string>::const_iterator it;
    for (it = moduleList.begin(); it != moduleList.end(); ++it)
    {
        v = _dbHandle->getVersion(*it);
        TLOGDEBUG(current->getIp() << "|" << current->getPort() << "|" << __FUNCTION__ << "|" << *it
                                   << "|" << v << endl);
        mapModuleVersion.insert(make_pair(*it, v));
    }
    return ROUTER_SUCC;
}

tars::Int32 RouterImp::getModuleList(vector<string> &moduleList, tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->getModuleList(moduleList);
    }

    return _dbHandle->getModuleList(moduleList);
}

tars::Int32 RouterImp::heartBeatReport(const string &moduleName,
                                       const string &groupName,
                                       const string &serverName,
                                       tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->heartBeatReport(moduleName, groupName, serverName);
    }

    TLOGDEBUG("[heartBeatReport]entered.module:" << moduleName << "|group:" << groupName
                                                 << "|server:" << serverName << "|ip"
                                                 << current->getIp() << endl);

    TC_ThreadLock::Lock lock1(g_app.getHeartbeatInfoLock());
    GroupHeartBeatInfo *info = NULL;
    string errMsg;
    if (queryGroupHeartBeatInfo(moduleName, groupName, errMsg, &info) != 0)
    {
        TLOGERROR(FILE_FUN << errMsg << endl);
        return 0;
    }

    time_t nowTime = TC_TimeProvider::getInstance()->getNow();
    assert(info);
    if (info->masterServer == serverName)
    {
        if (info->masterLastReportTime < nowTime)
        {
            info->masterLastReportTime = nowTime;
        }
    }
    else if (info->slaveServer == serverName)
    {
        info->slaveLastReportTime = nowTime;
    }
    else
    {
        map<string, time_t>::iterator itrMirror = info->mirrorInfo.find(serverName);
        if (itrMirror != info->mirrorInfo.end())
        {
            itrMirror->second = nowTime;
            FDLOG("switch") << "recvie heartBeat:" << moduleName << " groupName:" << groupName
                            << " serverName:" << serverName << endl;
        }
        else
        {
            FDLOG("switch") << "serverName no find module:" << moduleName
                            << " groupName:" << groupName << " serverName:" << serverName
                            << " master:" << info->masterServer << " slave:" << info->slaveServer
                            << endl;
        }
    }

    return 0;
}

tars::Int32 RouterImp::recoverMirrorStat(const string &moduleName,
                                         const string &groupName,
                                         const string &mirrorIdc,
                                         string &errMsg,
                                         tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->recoverMirrorStat(moduleName, groupName, mirrorIdc, errMsg);
    }

    //先在路由表中查询该group，查看其是否是无镜像备机
    string serverName;
    TLOGDEBUG("RouterImp::recoverMirrorStat: new request|" << moduleName << "|" << groupName << "|"
                                                           << mirrorIdc << endl);

    {
        TC_ThreadLock::Lock lock(g_app.getHeartbeatInfoLock());
        vector<string> *idcList;

        if (queryMirrorInfo(moduleName, groupName, mirrorIdc, errMsg, &idcList) != 0)
        {
            TLOGERROR(errMsg << endl);
            return -1;
        }

        // 检查idc下是否只有一台镜像
        if (idcList->size() != 1)
        {
            errMsg = "RouterImp::recoverMirrorStat idc has more than 1 mirror " + moduleName + "|" +
                     groupName + "|" + mirrorIdc;
            TLOGERROR(errMsg << endl);
            return -1;
        }

        serverName = (*idcList)[0];
    }

    // 检验镜像是否真的恢复
    if (sendHeartBeat(serverName) != 0)
    {
        errMsg = "RouterImp::recoverMirrorStat mirror not recover " + moduleName + "|" + groupName +
                 "|" + mirrorIdc;
        TLOGERROR(FILE_FUN << errMsg << endl);
        return -1;
    }

    TLOGDEBUG("RouterImp::recoverMirrorStat find group success" << endl);
    //设置DB与MEM的状态, 并将版本号+1
    if (_dbHandle->recoverMirrorInDbAndMem(moduleName, groupName, serverName) != 0)
    {
        errMsg = "RouterImp::recoverMirrorStat update db failed";
        TLOGERROR(FILE_FUN << errMsg << endl);
        return -1;
    }
    TLOGDEBUG("RouterImp::recoverMirrorStat recoverMirrorInDbAndMem success" << endl);

    //更改心跳信息:修改group的状态
    {
        TC_ThreadLock::Lock lock(g_app.getHeartbeatInfoLock());

        string msg;
        GroupHeartBeatInfo *groupInfo;
        if (queryGroupHeartBeatInfo(moduleName, groupName, msg, &groupInfo) == 0)
        {
            groupInfo->status = 0;
            map<string, int>::iterator its = groupInfo->serverStatus.find(serverName);
            if (its != groupInfo->serverStatus.end()) its->second = 0;

            TLOGDEBUG(FILE_FUN << "RouterImp::recoverMirrorStat update heardBeat success" << endl);
        }
    }
    //通知PROXY reload router
    //目前的机制是PROXY每隔1秒就向ROUTER请求最新的路由版本，如果版本更新就重新加载路由

    //更新切换结果记录表
    errMsg = "recoverMirrorStat success";
    _dbHandle->updateSwitchGroupStatus(moduleName, groupName, mirrorIdc, errMsg, 0);

    return 0;
}

tars::Int32 RouterImp::switchByGroup(const string &moduleName,
                                     const string &groupName,
                                     bool forceSwitch,
                                     tars::Int32 maxBinlogDiffTime,
                                     string &errMsg,
                                     tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->switchByGroup(moduleName, groupName, forceSwitch, maxBinlogDiffTime, errMsg);
    }
    //主备切换
    string curMasterServer;
    string curSlaveServer;

    TLOGDEBUG("RouterImp::switchByGroup: new request|" << moduleName << "|" << groupName << "|"
                                                       << forceSwitch << "|" << maxBinlogDiffTime
                                                       << endl);

    //切换前，先向数据库插入一条切换记录
    long switchRecordID = -1;
    _dbHandle->insertSwitchInfo(moduleName, groupName, "", "", "", "", 0, switchRecordID);
    int switchResult = 0;
    int groupStatus = 0;

    //是否清理模块切换信息
    bool cleanSwitchInfo = false;
    try
    {
        do
        {
            if (_dbHandle->hasTransferingLoc(moduleName, groupName))
            {
                errMsg = "[swicth_by_group_fail] module is transfering: " + moduleName +
                         ", reject grant";
                FDLOG("switch") << errMsg << endl;
                switchResult = 3;
                break;
            }

            {
                TC_ThreadLock::Lock lock1(g_app.getHeartbeatInfoLock());
                HeartbeatInfo &heartbeatInfo = g_app.getHeartbeatInfo();
                map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
                    heartbeatInfo.find(moduleName);
                if (itr != heartbeatInfo.end())
                {
                    map<string, GroupHeartBeatInfo>::iterator itrGroupInfo =
                        itr->second.find(groupName);
                    if (itrGroupInfo != itr->second.end())
                    {
                        if (itrGroupInfo->second.status != 0)
                        {
                            errMsg =
                                "[swicth_by_group_fail] group is switching! module:" + moduleName +
                                " group:" + groupName;
                            TLOGERROR(errMsg << endl);
                            switchResult = 3;
                            break;
                        }
                        itrGroupInfo->second.status = 1;
                    }
                    else
                    {
                        errMsg = "[swicth_by_group_fail] can not find module " + moduleName;
                        TLOGERROR(errMsg << endl);
                        switchResult = 3;
                        break;
                    }
                }
                else
                {
                    errMsg = "[swicth_by_group_fail] can not find module " + moduleName;
                    TLOGERROR(errMsg << endl);
                    switchResult = 3;
                    break;
                }
                g_app.addSwitchModule(moduleName);
                cleanSwitchInfo = true;
            }
            PackTable packTable;
            if (_dbHandle->getPackTable(moduleName, packTable) != 0)
            {
                errMsg =
                    "[swicth_by_group_fail]do not support module: " + moduleName + ", reject grant";
                FDLOG("switch") << FILE_FUN << errMsg << endl;
                switchResult = 3;
                break;
            }
            map<string, GroupInfo>::iterator it = packTable.groupList.find(groupName);
            if (it == packTable.groupList.end())
            {
                errMsg = "[swicth_by_group_fail]module: " + moduleName +
                         " do not have group: " + groupName;
                FDLOG("switch") << FILE_FUN << errMsg << endl;
                switchResult = 3;
                break;
            }
            it->second.accessStatus = 0;
            string idcName;
            //生成临时的切换后的路由
            map<string, ServerInfo>::iterator it1 = packTable.serverList.begin(),
                                              it1End = packTable.serverList.end();
            for (; it1 != it1End; ++it1)
            {
                if (it1->second.ServerStatus == "M" && it1->second.groupName == groupName)
                {
                    curMasterServer = it1->first;
                    idcName = it1->second.idc;
                    break;
                }
            }
            if (it1 == it1End)
            {
                errMsg = "[swicth_by_group_fail]no masterServer find module: " + moduleName +
                         " group: " + groupName;
                FDLOG("switch") << FILE_FUN << errMsg << endl;
                switchResult = 3;
                break;
            }
            //找备机
            map<string, ServerInfo>::iterator it2 = packTable.serverList.begin(),
                                              it2End = packTable.serverList.end();
            for (; it2 != it2End; ++it2)
            {
                if (it2->second.ServerStatus == "S" && it2->second.groupName == groupName &&
                    it2->second.idc == idcName)
                {
                    curSlaveServer = it2->first;
                    break;
                }
            }

            if (it2 == it2End)
            {
                errMsg = "[swicth_by_group_fail]no slaveServer find module: " + moduleName +
                         " group: " + groupName;
                FDLOG("switch") << FILE_FUN << errMsg << endl;
                switchResult = 3;
                break;
            }

            TLOGDEBUG("curMasterServer" << curMasterServer << " "
                                        << "curSlaveServer" << curSlaveServer << endl);
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
            vector<string> &idcList = it->second.idcList[idcName];
            vector<string>::iterator vIt = idcList.begin(), vItEnd = idcList.end();
            while (vIt != vItEnd)
            {
                if (*vIt == curSlaveServer)
                {
                    idcList.erase(vIt);
                    break;
                }
                ++vIt;
            }

            vector<string> idcListNew;
            idcListNew.push_back(curSlaveServer);
            idcListNew.insert(idcListNew.end(), idcList.begin(), idcList.end());
            it->second.idcList[idcName] = idcListNew;
            ++packTable.info.version;
            //生成临时路由表完成

            //验证备机差异
            if (!forceSwitch)
            {
                ServerInfo &serverInfo = packTable.serverList[curSlaveServer];
                RouterClientPrx routerClientPrx =
                    Application::getCommunicator()->stringToProxy<RouterClientPrx>(
                        serverInfo.RouteClientServant);
                routerClientPrx->tars_timeout(100);
                try
                {
                    int slaveBinlogDiff;
                    if (routerClientPrx->getBinlogdif(slaveBinlogDiff) != 0)
                    {
                        errMsg =
                            "[swicth_by_group_fail] the slave may be in self-built state, not "
                            "allowed to switch";
                        FDLOG("switch") << FILE_FUN << errMsg << endl;
                        switchResult = 3;
                        break;
                    }
                    else
                    {
                        if (slaveBinlogDiff > maxBinlogDiffTime)
                        {
                            errMsg = "[swicth_by_group_fail] the binlog diff of the slave is " +
                                     I2S(slaveBinlogDiff);
                            FDLOG("switch") << FILE_FUN << errMsg << endl;
                            switchResult = 3;
                            break;
                        }
                    }
                }
                catch (const exception &ex)
                {
                    errMsg =
                        "[swicth_by_group_fail] get slave binlog diff error : " + string(ex.what());
                    FDLOG("switch") << FILE_FUN << errMsg << endl;
                    switchResult = 3;
                    break;
                }
            }

            //向主机推送路由
            string objName = packTable.serverList[curMasterServer].RouteClientServant;
            RouterClientPrx masterRouterClientPrx =
                Application::getCommunicator()->stringToProxy<RouterClientPrx>(objName);
            masterRouterClientPrx->tars_timeout(100);
            try
            {
                if (masterRouterClientPrx->setRouterInfoForSwitch(moduleName, packTable) != 0)
                {
                    errMsg =
                        "[swicth_by_group_fail]The slave machine is in rebuild state, and it "
                        "is not allowed to switch";
                    FDLOG("switch") << FILE_FUN << errMsg << endl;
                    switchResult = 3;
                    break;
                }
            }
            catch (const exception &ex)
            {
                errMsg =
                    "[routerClientPrx->setRouterInfoForSwitch]  exception: " + string(ex.what());
                FDLOG("switch") << FILE_FUN << errMsg << endl;
            }

            //向备机推送路由
            string prxName = packTable.serverList[curSlaveServer].RouteClientServant;
            RouterClientPrx slaveRouterClientPrx =
                Application::getCommunicator()->stringToProxy<RouterClientPrx>(prxName);
            slaveRouterClientPrx->tars_timeout(100);
            try
            {
                if (slaveRouterClientPrx->setRouterInfoForSwitch(moduleName, packTable) != 0)
                {
                    errMsg =
                        "[swicth_by_group_fail]The slave machine is in rebuild state, and it "
                        "is not allowed to switch";
                    FDLOG("switch") << FILE_FUN << errMsg << endl;
                    switchResult = 3;
                    break;
                }
            }
            catch (const exception &ex)
            {
                errMsg =
                    "[routerClientPrx->setRouterInfoForSwitch]  exception: " + string(ex.what());
                FDLOG("switch") << FILE_FUN << errMsg << endl;
            }
            //如果上面步骤全部成功 修改内存和数据库
            if (_dbHandle->switchMasterAndSlaveInDbAndMem(
                    moduleName, groupName, curMasterServer, curSlaveServer, packTable) != 0)
            {
                errMsg =
                    "[swicth_by_group_fail]Failed to update database and memory routing "
                    "table. Please handle it manually";
                FDLOG("switch") << FILE_FUN << errMsg << endl;
                switchResult = 3;
                break;
            }
            errMsg = "[swicth_by_group_success]";
            _dbHandle->updateStatusToRelationDB(curSlaveServer, "M");
            _dbHandle->updateStatusToRelationDB(curMasterServer, "S");
            switchResult = 1;
        } while (0);

        TLOGDEBUG("swicth end" << endl);

        if (cleanSwitchInfo)
        {
            g_app.removeSwitchModule(moduleName);
            cleanSwitchInfo = false;
        }

        //手动切换完成后 再更改心跳信息
        if (1 == switchResult)
        {
            FDLOG("switch") << "switch over now change zhe report" << endl;
            TC_ThreadLock::Lock lock1(g_app.getHeartbeatInfoLock());
            HeartbeatInfo &heartbeatInfo = g_app.getHeartbeatInfo();
            map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
                heartbeatInfo.find(moduleName);
            if (itr != heartbeatInfo.end())
            {
                FDLOG("switch") << "switch over now change zhe report find module" << moduleName
                                << endl;
                map<string, GroupHeartBeatInfo>::iterator itrGroupInfo =
                    itr->second.find(groupName);
                if (itrGroupInfo != itr->second.end())
                {
                    FDLOG("switch")
                        << "switch over now change zhe report find groupName" << groupName << endl;
                    time_t nowTime = TC_TimeProvider::getInstance()->getNow();
                    itrGroupInfo->second.status = 0;
                    string tmp = itrGroupInfo->second.masterServer;
                    itrGroupInfo->second.masterServer = itrGroupInfo->second.slaveServer;
                    itrGroupInfo->second.slaveServer = tmp;
                    itrGroupInfo->second.masterLastReportTime = nowTime + SECONDS_IN_DAY;
                    itrGroupInfo->second.slaveLastReportTime = nowTime + SECONDS_IN_DAY;
                }
            }
        }
        else
        {
            FDLOG("switch") << "switch over now change zhe report" << endl;
            TC_ThreadLock::Lock lock1(g_app.getHeartbeatInfoLock());
            HeartbeatInfo &heartbeatInfo = g_app.getHeartbeatInfo();
            map<string, map<string, GroupHeartBeatInfo>>::iterator itr =
                heartbeatInfo.find(moduleName);
            if (itr != heartbeatInfo.end())
            {
                FDLOG("switch") << "switch over now change zhe report find module" << moduleName
                                << endl;
                map<string, GroupHeartBeatInfo>::iterator itrGroupInfo =
                    itr->second.find(groupName);
                if (itrGroupInfo != itr->second.end())
                {
                    FDLOG("switch")
                        << "switch over now change zhe report find groupName" << groupName << endl;
                    itrGroupInfo->second.status = 0;
                }
            }
        }

        _dbHandle->updateSwitchInfo(
            switchRecordID, switchResult, errMsg, groupStatus, curMasterServer, curSlaveServer);
        return switchResult == 1 ? 0 : -1;
    }
    catch (TarsException &e)
    {
        errMsg = "Tars exception:" + string(e.what());
        FDLOG("switch") << FILE_FUN << errMsg << endl;
    }
    catch (exception &e)
    {
        errMsg = "std exception:" + string(e.what());
        FDLOG("switch") << FILE_FUN << errMsg << endl;
    }
    catch (...)
    {
        errMsg = "unkown error";
        FDLOG("switch") << FILE_FUN << errMsg << endl;
    }

    if (cleanSwitchInfo)
    {
        g_app.removeSwitchModule(moduleName);
    }

    {
        TC_ThreadLock::Lock lock1(g_app.getHeartbeatInfoLock());
        HeartbeatInfo &heartbeatInfo = g_app.getHeartbeatInfo();
        map<string, map<string, GroupHeartBeatInfo>>::iterator itr = heartbeatInfo.find(moduleName);
        if (itr != heartbeatInfo.end())
        {
            map<string, GroupHeartBeatInfo>::iterator itrGroupInfo = itr->second.find(groupName);
            if (itrGroupInfo != itr->second.end())
            {
                itrGroupInfo->second.status = 0;
            }
        }
    }

    _dbHandle->updateSwitchInfo(
        switchRecordID, 3, errMsg, groupStatus, curMasterServer, curSlaveServer);
    return -1;
}

tars::Int32 RouterImp::getIdcInfo(const string &moduleName,
                                  const MachineInfo &machineInfo,
                                  IDCInfo &idcInfo,
                                  tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->getIdcInfo(moduleName, machineInfo, idcInfo);
    }

    try
    {
        int ret = 0;

        map<string, string>::const_iterator it = _cityToIDC.find(machineInfo.idc_city);
        if (it != _cityToIDC.end())
        {
            idcInfo.idc = it->second;
        }
        else
        {
            ret = _dbHandle->getMasterIdc(moduleName, idcInfo.idc);
            if (ret != 0)
            {
                TLOGERROR(__FUNCTION__ << ":getMasterIdc err:" << ret << endl);
                idcInfo.idc = "";
            }
        }
        return ret;
    }
    catch (exception &e)
    {
        TLOGERROR(__FUNCTION__ << ":exception:" << e.what() << endl);
    }
    catch (...)
    {
        TLOGERROR(__FUNCTION__ << ": unknow exception:" << endl);
    }
    return -1;
}

tars::Int32 RouterImp::serviceRestartReport(const string &moduleName,
                                            const string &groupName,
                                            tars::TarsCurrentPtr current)
{
    if (!isMaster())
    {
        TLOGDEBUG(FILE_FUN << "This is slave, request will proxy to master" << endl);
        if (updateMasterPrx() != 0)
        {
            return ROUTER_INFO_ERR;
        }
        return _prx->serviceRestartReport(moduleName, groupName);
    }

    TLOGDEBUG("[serviceRestartReport]entered.module:" << moduleName << "|group:" << groupName
                                                      << endl);

    //主机上报重启后，如果当前模块和组正在做迁移，则唤醒迁移线程
    if (!_dbHandle->hasTransferingLoc(moduleName, groupName)) return 0;

    TLOGDEBUG("[RouterImp::serviceRestartReport]: weakup the waiting transfering thread" << endl);
    map<string, map<string, TransferMutexCondPtr>> transferMutexCond;
    if (_dbHandle->getTransferMutexCond(transferMutexCond) != 0) return 0;

    map<string, map<string, TransferMutexCondPtr>>::iterator it =
        transferMutexCond.find(moduleName);
    if (it == transferMutexCond.end()) return 0;

    map<string, TransferMutexCondPtr> &_mMutexCond = it->second;
    map<string, TransferMutexCondPtr>::iterator itPtr;
    //遍历唤醒所有等待的迁移线程
    for (itPtr = _mMutexCond.begin(); itPtr != _mMutexCond.end(); ++itPtr)
    {
        TransferMutexCondPtr &mutextCond = itPtr->second;
        pthread_mutex_lock(&(mutextCond->_mutex));
        pthread_cond_signal(&(mutextCond->_cond));
        pthread_mutex_unlock(&(mutextCond->_mutex));
    }

    return 0;
}

tars::Bool RouterImp::procAdminCommand(const string &command,
                                       const string &params,
                                       string &result,
                                       tars::TarsCurrentPtr current)
{
    return g_app.procAdminCommand(command, params, result);
}

int RouterImp::sendHeartBeat(const string &serverName)
{
    string ServerObj = serverName + ".RouterClientObj";
    RouterClientPrx pRouterClientPrx =
        Application::getCommunicator()->stringToProxy<RouterClientPrx>(ServerObj);
    pRouterClientPrx->tars_timeout(3000);
    try
    {
        pRouterClientPrx->helloBaby();
        TLOGDEBUG(FILE_FUN << "RouterImp::heartBeatSend send heartBeat ok, ServerName:"
                           << serverName << endl);
        return 0;
    }
    catch (const TarsException &ex)
    {
        TLOGERROR(FILE_FUN << "RouterImp::heartBeatSend catch exception: " << ex.what() << endl);
        try
        {
            pRouterClientPrx->helloBaby();
            TLOGDEBUG(FILE_FUN << "RouterImp::heartBeatSend send heartBeat ok, ServerName:"
                               << serverName << endl);
            return 0;
        }
        catch (const TarsException &ex)
        {
            TLOGERROR(FILE_FUN << "RouterImp::heartBeatSend catch exception: " << ex.what()
                               << endl);
        }
    }
    return -1;
}

int RouterImp::updateMasterPrx()
{
    std::string o = g_app.getMasterRouterObj();
    TLOGDEBUG(FILE_FUN << "master router obj = " << o << endl);
    if (o == "" || o == _selfObj)
    {
        // 全局对象中的RouterObj还未被设置
        TLOGERROR(FILE_FUN << "master router obj not set" << endl);
        return -1;
    }

    if (_masterRouterObj != o)
    {
        TLOGDEBUG(FILE_FUN << "master router obj update from " << _masterRouterObj << " to " << o
                           << endl);
        _masterRouterObj = o;
        _prx = Application::getCommunicator()->stringToProxy<RouterPrx>(_masterRouterObj);
    }

    return 0;
}

inline bool RouterImp::isMaster() const { return g_app.getRouterType() == ROUTER_MASTER; }
