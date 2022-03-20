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
#include "Global.h"
#include "UnpackTable.h"
#include "RouterClientImp.h"
#include "ProxyImp.h"
#include "ProxyServer.h"

using namespace tars;
using namespace std;

void RouterClientImp::initialize()
{
    TLOG_DEBUG("begin to initialize RouterClientImp servant ..." << endl);
}

int RouterClientImp::setRouterInfo(const string &moduleName, const PackTable &packTable, TarsCurrentPtr current)
{
    string strErr;
    try
    {
        RouterTableInfo *pRouterTableInfo = g_app._routerTableInfoFactory->getRouterTableInfo(moduleName);
        if (!pRouterTableInfo)
        {
            TLOG_DEBUG("setRouterInfo failed, do not support this module: " << moduleName << endl);
            TARS_NOTIFY_WARN("[RouterClientImp::setRouterInfo] |failed| Invalid Module");
            return ET_MODULE_NAME_INVALID;
        }
        RouterTable &routerTable = pRouterTableInfo->getRouterTable();

        int iRet = routerTable.reload(packTable);
        if (iRet == 0)
        {
            pRouterTableInfo->setRouterTableToFile(); // 重新加载路由成功后，更新本地文件
            TLOG_DEBUG("RouterServer push router info for module: " << moduleName << " succ" << endl);
            TARS_NOTIFY_NORMAL("[RouterClientImp::setRouterInfo] |succ|");
            return ET_SUCC;
        }
        else
        {
            TLOG_DEBUG("RouterServer push router info for module: " << moduleName << " failed, reload errno=" << iRet << endl);
            TARS_NOTIFY_WARN("[RouterClientImp::setRouterInfo] |failed| [RouterTable::reload]");
            return ET_SYS_ERR;
        }
    }
    catch (exception &e)
    {
        strErr = string("[RouterClientImp::setRouterInfo] exception:") + e.what();
    }
    catch (...)
    {
        strErr = "[RouterClientImp::setRouterInfo] UnkownException";
    }

    TLOG_ERROR(strErr << endl);
    TARS_NOTIFY_WARN(strErr);
    return ET_SYS_ERR;
}

/*以下接口不允许RouterServer调用，否则返回ET_COMMAND_INVALID*/

int RouterClientImp::setTransRouterInfo(const string &moduleName, int transInfoListVer,
                                        const vector<TransferInfo> &transferingInfoList, const PackTable &packTable, TarsCurrentPtr current)
{
    TLOG_DEBUG("invalid access, do not support command: setTransRouterInfo" << endl);
    return ET_COMMAND_INVALID;
}

tars::Int32
RouterClientImp::fromTransferStart(const std::string &moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo> &transferingInfoList,
                                   const PackTable &packTable, tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("invalid access, do not support command: fromTransferStart" << endl);
    return ET_COMMAND_INVALID;
}

tars::Int32
RouterClientImp::fromTransferDo(const TransferInfo &transferingInfo, tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("invalid access, do not support command: fromTransferDo" << endl);
    return ET_COMMAND_INVALID;
}

tars::Int32
RouterClientImp::toTransferStart(const string &moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo> &transferingInfoList, const TransferInfo &transferingInfo, const PackTable &packTable, tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("invalid access, do not support command: toTransferStart" << endl);
    return ET_COMMAND_INVALID;
}

tars::Int32
RouterClientImp::recallLease(tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("invalid access, do not support command: recallLease" << endl);
    return ET_COMMAND_INVALID;
}

tars::Int32
RouterClientImp::grantLease(tars::Int32 leaseHoldTime, tars::TarsCurrentPtr curren)
{
    TLOG_DEBUG("invalid access, do not support command: grantLease" << endl);
    return ET_COMMAND_INVALID;
}

tars::Int32
RouterClientImp::getBinlogdif(tars::Int32 &difBinlogTime, tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("invalid access, do not support command: getBinlogdif" << endl);
    return ET_COMMAND_INVALID;
}

tars::Int32
RouterClientImp::setRouterInfoForSwicth(const std::string &moduleName, const DCache::PackTable &packTable, tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("invalid access, do not support command: setRouterInfoForSwicth" << endl);
    return ET_COMMAND_INVALID;
}

tars::Int32 RouterClientImp::helloBaby(tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("i am ok" << endl);
    return 0;
}

tars::Int32 RouterClientImp::helleBabyByName(const std::string &serverName, tars::TarsCurrentPtr current)
{
    string cacheObj = serverName + ".RouterClientObj";
    RouterClientPrx routerClientPrx = Application::getCommunicator()->stringToProxy<RouterClientPrx>(cacheObj);
    try
    {
        return routerClientPrx->helloBaby();
    }
    catch (const TarsException &ex)
    {
        TLOG_ERROR("[RouterClientImp::helleBabyByName] exception: " << ex.what() << " cacheObj:" << cacheObj << endl);
    }
    return -1;
}

