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
#ifndef __ROUTERIMP_H__
#define __ROUTERIMP_H__

#include "Router.h"
#include "RouterServer.h"
#include "RouterShare.h"
#include "global.h"
#include "servant/Application.h"

using namespace tars;
using namespace std;
using namespace DCache;

class DbHandle;
namespace DCache
{
class Transfer;
}

class RouterImp : public Router
{
public:
    RouterImp() = default;
    virtual ~RouterImp() = default;
    virtual void initialize();
    virtual void destroy(){};

public:
    virtual tars::Int32 getRouterInfo(const string &moduleName,
                                      PackTable &packTable,
                                      tars::TarsCurrentPtr current);

    virtual tars::Int32 getRouterInfoFromCache(const string &moduleName,
                                               PackTable &packTable,
                                               tars::TarsCurrentPtr current);

    virtual tars::Int32 getTransRouterInfo(const string &moduleName,
                                           tars::Int32 &transInfoListVer,
                                           vector<TransferInfo> &transferingInfoList,
                                           PackTable &packTable,
                                           tars::TarsCurrentPtr current);

    virtual tars::Int32 getVersion(const std::string &moduleName, tars::TarsCurrentPtr current);

    virtual tars::Int32 getRouterVersion(const string &moduleName,
                                         tars::Int32 &version,
                                         tars::TarsCurrentPtr current);

    virtual tars::Int32 getRouterVersionBatch(const vector<string> &moduleList,
                                              map<string, tars::Int32> &mapModuleVersion,
                                              tars::TarsCurrentPtr current);

    virtual tars::Int32 heartBeatReport(const string &moduleName,
                                        const string &groupName,
                                        const string &serverName,
                                        tars::TarsCurrentPtr current);

    virtual tars::Int32 getModuleList(vector<string> &moduleList, tars::TarsCurrentPtr current);

    virtual tars::Int32 recoverMirrorStat(const string &moduleName,
                                          const string &groupName,
                                          const string &mirrorIdc,
                                          string &err,
                                          tars::TarsCurrentPtr current);

    virtual tars::Int32 switchByGroup(const string &moduleName,
                                      const string &groupName,
                                      bool bForceSwitch,
                                      tars::Int32 iDifBinlogTime,
                                      string &err,
                                      tars::TarsCurrentPtr current);

    virtual tars::Int32 getIdcInfo(const string &moduleName,
                                   const MachineInfo &machineInfo,
                                   IDCInfo &idcInfo,
                                   tars::TarsCurrentPtr current);

    virtual tars::Int32 serviceRestartReport(const string &moduleName,
                                             const string &groupName,
                                             tars::TarsCurrentPtr current);

    virtual tars::Bool procAdminCommand(const string &command,
                                        const string &params,
                                        string &result,
                                        tars::TarsCurrentPtr current);

private:
    int sendHeartBeat(const std::string &serverName);

    // 更新Router Master的Prx
    int updateMasterPrx();

    // 当前机器是否是主机
    bool isMaster() const;

    int queryGroupHeartBeatInfo(const string &moduleName,
                                const string &groupName,
                                string &errMsg,
                                GroupHeartBeatInfo **info);

    int queryMirrorInfo(const string &moduleName,
                        const string &groupName,
                        const string &mirrorIdc,
                        string &errMsg,
                        vector<string> **idcList);

private:
    std::shared_ptr<DbHandle> _dbHandle;  // 数据库操作的句柄
    map<string, string> _cityToIDC;       // idc_city到idc的映射
    std::string _masterRouterObj;         // Router master的OBJ字符串
    RouterPrx _prx;                       // 和Router master通信的proxy
    std::string _selfObj;                 // Router本机的obj
};

#endif  // __ROUTERIMP_H__
