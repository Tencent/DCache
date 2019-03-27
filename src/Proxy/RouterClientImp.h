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
#ifndef _ROUTERCLIENTIMP_H_
#define _ROUTERCLIENTIMP_H_

#include "UnpackTable.h"
#include "RouterClient.h"
#include "util/tc_config.h"
#include "util/tc_common.h"
#include "servant/Application.h"

using namespace tars;
using namespace DCache;

class RouterClientImp : public RouterClient
{
  public:
    RouterClientImp(){};

    ~RouterClientImp(){};

    virtual void initialize();

    // Router定时推送路由信息到Proxy端
    virtual int setRouterInfo(const string &moduleName, const PackTable &packTable, TarsCurrentPtr current);

    virtual int setTransRouterInfo(const string &moduleName, int transInfoListVer, const vector<TransferInfo> &transferingInfoList, const PackTable &packTable, TarsCurrentPtr current);

    virtual tars::Int32 fromTransferStart(const std::string &moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo> &transferingInfoList, const PackTable &packTable, tars::TarsCurrentPtr current);

    virtual tars::Int32 fromTransferDo(const TransferInfo &transferingInfo, tars::TarsCurrentPtr current);

    virtual tars::Int32 toTransferStart(const string &moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo> &transferingInfoList, const TransferInfo &transferingInfo, const PackTable &packTable, tars::TarsCurrentPtr current);

    virtual tars::Int32 recallLease(tars::TarsCurrentPtr current);

    virtual tars::Int32 grantLease(tars::Int32 leaseHoldTime, tars::TarsCurrentPtr current);

    virtual tars::Int32 getBinlogdif(tars::Int32 &difBinlogTime, tars::TarsCurrentPtr current);

    virtual tars::Int32 setRouterInfoForSwicth(const std::string &moduleName, const DCache::PackTable &packTable, tars::TarsCurrentPtr current);

    virtual tars::Int32 helloBaby(tars::TarsCurrentPtr current);

    virtual tars::Int32 helleBabyByName(const std::string &serverName, tars::TarsCurrentPtr current);

    virtual tars::Int32 setBatchFroTrans(const std::string &moduleName, const vector<DCache::Data> &keyValue, tars::TarsCurrentPtr current) { return 0; }

    virtual tars::Int32 setBatchFroTransOnlyKey(const std::string &moduleName, const vector<std::string> &mainKey, tars::TarsCurrentPtr current) { return 0; }

    virtual tars::Int32 setFroCompressTransOnlyKey(const std::string &moduleName, const vector<std::string> &mainKey, tars::TarsCurrentPtr current) { return 0; };

    virtual tars::Int32 setFroCompressTransEx(const std::string &moduleName, const std::string &mainKey, const std::string &transContent, tars::Bool compress, tars::Bool full, tars::TarsCurrentPtr current) { return 0; }

    virtual tars::Int32 setFroCompressTransWithType(const std::string &moduleName, const std::string &mainKey, const std::string &transContent, tars::Char keyType, tars::Bool compress, tars::Bool full, tars::TarsCurrentPtr current) { return 0; }

    virtual tars::Int32 cleanFromTransferData(const std::string &moduleName, tars::Int32 fromPageNo, tars::Int32 toPageNo, tars::TarsCurrentPtr current) { return 0; };

    virtual tars::Int32 isReadyForSwitch(const std::string &moduleName, tars::Bool &bReady, tars::TarsCurrentPtr current) { return 0; }

    virtual tars::Int32 notifyMasterDowngrade(const std::string &moduleName, tars::TarsCurrentPtr current) { return 0; }

    virtual tars::Int32 setRouterInfoForSwitch(const std::string &moduleName, const DCache::PackTable &packTable, tars::TarsCurrentPtr current) { return 0; }

    virtual tars::Int32 disconnectFromMaster(const std::string &moduleName, tars::TarsCurrentPtr current) { return 0; }

    virtual void destroy(){};

  private:
    string confFile;

    TC_Config conf;
};

#endif
