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
#include "RouterHandle.h"
#include "CacheServer.h"

void RouterClientCallback::callback_setBatchFroTrans(tars::Int32 ret)
{
    if (ret == ET_SUCC)
    {
        if (_pParam->isFinish())
        {
            RouterClient::async_response_fromTransferDo(_current, 0);
        }
    }
    else
    {
        if (_pParam->setEnd())
        {
            RouterClient::async_response_fromTransferDo(_current, -1);
        }
    }
}

void RouterClientCallback::callback_setBatchFroTrans_exception(tars::Int32 ret)
{
    if (_pParam->setEnd())
    {
        RouterClient::async_response_fromTransferDo(_current, -1);
    }
}

void RouterClientCallback::callback_setBatchFroTransOnlyKey(tars::Int32 ret)
{
    if (ret == ET_SUCC)
    {
        if (_pParam->isFinish())
        {
            RouterClient::async_response_fromTransferDo(_current, 0);
        }
    }
    else
    {
        if (_pParam->setEnd())
        {
            RouterClient::async_response_fromTransferDo(_current, -1);
        }
    }
}

void RouterClientCallback::callback_setBatchFroTransOnlyKey_exception(tars::Int32 ret)
{
    if (_pParam->setEnd())
    {
        RouterClient::async_response_fromTransferDo(_current, -1);
    }
}


RouterHandle::RouterHandle() : _transInfoListVer(TRANSFER_CLEAN_VERSION)
{
}

RouterHandle::~RouterHandle()
{
}

void RouterHandle::init(TC_Config& conf)
{
    _moduleName = conf["/Main<ModuleName>"];

    _routeFile = ServerConfig::DataPath + "/" + conf["/Main/Router<RouteFile>"];

    _routeObj = conf["/Main/Router<ObjName>"];

    _routePrx = Application::getCommunicator()->stringToProxy<RouterPrx>(_routeObj);

    _pageSize = TC_Common::strto<unsigned int>(conf["/Main/Router<PageSize>"]);

    TLOG_DEBUG("RouterHandle::initialize Succ" << endl);
}

tars::Int32 RouterHandle::initRoute(bool forbidFromFile, bool &syncFromRouterSuccess)
{
    syncFromRouterSuccess = false;
    {
        PackTable packTable;
        try
        {
            int iRet = _routePrx->getRouterInfoFromCache(_moduleName, packTable);

            if (iRet != ROUTER_SUCC)
            {
                TLOG_ERROR("[RouterHandle::initRoute] getRouterInfo from " << _routeObj << " failed, ret=" << iRet << endl);
            }
            else
            {
                //保存最新路由表
                string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
                int iRouteRet = g_route_table.init(packTable, sServerName);
                if (iRouteRet != UnpackTable::RET_SUCC)
                {
                    TLOG_ERROR("[RouterHandle::initRoute] g_route_table.init failed, ret=" << iRouteRet << endl);
                }
                else
                {
                    syncFromRouterSuccess = true;
                    g_route_table.toFile(_routeFile);
                    return 0;
                }
            }
        }
        catch (const exception& ex)
        {
            TLOG_ERROR("[RouterHandle::initRoute] getRouterInfo from " << _routeObj << " exception:" << ex.what() << endl);
        }

    }

    if (forbidFromFile)
    {
        return -1;
    }

    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
    int iRet = g_route_table.fromFile(_routeFile, sServerName);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOG_ERROR("[RouterHandle::initRoute] g_route_table.fromFile failed, ret=" << iRet << endl);
        return -1;
    }

    return 0;
}

void RouterHandle::serviceRestartReport()
{
    int iRet = 0;
    try
    {
        iRet = _routePrx->serviceRestartReport(_moduleName, g_app.gstat()->groupName());
        if (iRet != 0)
        {
            TLOG_ERROR("RouterHandle::serviceRestartReport error" << endl);
        }

        TLOG_DEBUG("RouterHandle::serviceRestartReport, moduleName: " << _moduleName << ", groupName: " << g_app.gstat()->groupName() << endl);
    }
    catch (const TarsException & ex)
    {
        try
        {
            iRet = _routePrx->serviceRestartReport(_moduleName, g_app.gstat()->groupName());
            if (iRet != 0)
            {
                TLOG_ERROR("RouterHandle::serviceRestartReport error" << endl);
            }
        }
        catch (const TarsException & ex)
        {
            TLOG_ERROR("RouterHandle::serviceRestartReport exception: " << ex.what() << endl);
        }
    }
}

void RouterHandle::syncRoute()
{
    try
    {
        bool bTransfering = false;
        {
            TC_ThreadLock::Lock lock(_lock);
            if (!_transferingInfoList.empty())
            {
                bTransfering = true;
            }
        }
        if (bTransfering)
        {
            struct PackTable packTable;
            int iTransInfoListVer;
            vector<TransferInfo> vTransferingInfoList;

            int iRet = _routePrx->getTransRouterInfo(_moduleName, iTransInfoListVer, vTransferingInfoList, packTable);
            if (iRet != 0)
            {
                TLOG_ERROR("[RouterClientImp::syncRoute] getTransRouterInfo fail" << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                return;
            }

            TC_ThreadLock::Lock lock(_lock);
            if (_transferingInfoList.empty())
            {
                return;
            }
            if (iTransInfoListVer > _transInfoListVer || iTransInfoListVer == TRANSFER_CLEAN_VERSION)
            {
                iRet = g_route_table.reload(packTable, vTransferingInfoList);
                if (iRet != UnpackTable::RET_SUCC)
                {
                    TLOG_ERROR("[RouterClientImp::syncRoute] reload route error: " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return;
                }
                _transInfoListVer = iTransInfoListVer;
                _transferingInfoList = vTransferingInfoList;

                g_route_table.toFile(ServerConfig::DataPath + "/" + _routeFile);

                if (g_app.gstat()->serverType() == SLAVE)
                {
                    _transferingInfoList.clear();
                }

                updateCSyncKeyLimit();

                TLOG_DEBUG("update route succ" << endl);
            }
        }
        else
        {
            int iVer = _routePrx->getVersion(_moduleName);
            int iCurVer;

            iCurVer = g_route_table.getVersion();

            if (iVer != iCurVer)
            {
                {
                    TC_ThreadLock::Lock lock(_lock);
                    if (!_transferingInfoList.empty())
                    {
                        return;
                    }
                }
                struct PackTable packTable;
                int iRet = _routePrx->getRouterInfoFromCache(_moduleName, packTable);
                if (iRet != 0)
                {
                    TLOG_ERROR("[RouterClientImp::syncRoute] getRouterInfo fail" << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return;
                }

                TC_ThreadLock::Lock lock(_lock);
                if (!_transferingInfoList.empty())
                {
                    return;
                }
                iRet = g_route_table.reload(packTable);
                if (iRet != UnpackTable::RET_SUCC)
                {
                    TLOG_ERROR("[RouterClientImp::SyncRoute] reload route error, iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return;
                }
                g_route_table.toFile(ServerConfig::DataPath + "/" + _routeFile);

                updateCSyncKeyLimit();

                TLOG_DEBUG("update route succ" << endl);
            }
        }
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("[RouterClientImp::syncRoute] exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
    }
}

tars::Int32 RouterHandle::setRouterInfo(const string &moduleName, const PackTable & packTable)
{
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不做迁移
            TLOG_ERROR("[RouterClientImp::setRouterInfo] is not Master" << endl);
            return -1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("[RouterClientImp::setRouterInfo] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
            return -1;
        }

        TC_ThreadLock::Lock lock(_lock);

        int iRet = g_route_table.reload(packTable);
        if (iRet != UnpackTable::RET_SUCC)
        {
            TLOG_ERROR("[RouterClientImp::setRouterInfo] reload route error, iRet = " << iRet << endl);
            return -1;
        }
        _transInfoListVer = TRANSFER_CLEAN_VERSION;
        _transferingInfoList.clear();

        g_route_table.toFile(_routeFile);

        updateCSyncKeyLimit();
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("[RouterClientImp::setRouterInfo] exception: " << ex.what() << endl);
        return -1;
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("[RouterClientImp::setRouterInfo] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::setRouterInfo] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::setTransRouterInfo(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const PackTable& packTable)
{
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不做迁移
            TLOG_ERROR("[RouterClientImp::setTransRouterInfo] is not Master" << endl);
            return -1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("[RouterClientImp::setTransRouterInfo] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
            return -1;
        }

        TC_ThreadLock::Lock lock(_lock);
        if (transInfoListVer < _transInfoListVer && transInfoListVer != TRANSFER_CLEAN_VERSION)
        {
            return 0;
        }

        int iRet = g_route_table.reload(packTable, transferingInfoList);
        if (iRet != UnpackTable::RET_SUCC)
        {
            TLOG_ERROR("[RouterClientImp::setTransRouterInfo] reload route error, iRet = " << iRet << endl);
            return -1;
        }
        _transInfoListVer = transInfoListVer;
        _transferingInfoList = transferingInfoList;

        g_route_table.toFile(_routeFile);

        if (g_app.gstat()->serverType() == SLAVE)
        {
            _transferingInfoList.clear();
        }

        updateCSyncKeyLimit();
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("[RouterClientImp::setTransRouterInfo] exception: " << ex.what() << endl);
        return -1;
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("[RouterClientImp::setTransRouterInfo] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::setTransRouterInfo] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::fromTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const PackTable& packTable)
{
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不做迁移
            TLOG_ERROR("[RouterClientImp::fromTransferStart] is not Master" << endl);
            return 1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("[RouterClientImp::fromTransferStart] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
            return 2;
        }

        {
            TC_ThreadLock::Lock lock(_lock);
            if (transInfoListVer > _transInfoListVer || transInfoListVer == TRANSFER_CLEAN_VERSION)
            {
                int iRet = g_route_table.reload(packTable, transferingInfoList);
                if (iRet != UnpackTable::RET_SUCC)
                {
                    TLOG_ERROR("[RouterClientImp::fromTransferStart] reload route error, iRet = " << iRet << endl);
                    return -1;
                }
                _transInfoListVer = transInfoListVer;
                _transferingInfoList = transferingInfoList;

                g_route_table.toFile(_routeFile);

                TLOG_DEBUG("RouterClientImp::fromTransferStart route table:" << g_route_table.toString() << endl);

                if (g_app.gstat()->serverType() != MASTER)
                {
                    _transferingInfoList.clear();
                    updateCSyncKeyLimit();
                    TLOG_ERROR("[RouterClientImp::fromTransferStart] Server Type error" << endl);
                    return 3;
                }

                updateCSyncKeyLimit();
            }
        }
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("[RouterClientImp::fromTransferStart] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::fromTransferStart] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::fromTransferDo(const TransferInfo& transferingInfo, tars::TarsCurrentPtr current)
{
    if (g_app.gstat()->serverType() != MASTER)
    {
        //SLAVE状态下不做迁移
        TLOG_ERROR("[RouterClientImp::fromTransferDo] is not Master" << endl);
        return 1;
    }
    if (transferingInfo.moduleName != _moduleName)
    {
        //返回模块错误
        TLOG_ERROR("[RouterClientImp::fromTransferDo] moduleName error, " << _moduleName << " != " << transferingInfo.moduleName << "(config != param)" << endl);
        return 2;
    }

    if (!isInTransferingInfoList(transferingInfo))
    {
        //要执行的迁移已经不在迁移记录列表中
        TLOG_ERROR("[RouterClientImp::fromTransferDo] transferingInfo invalid" << endl);
        return 4;
    }

    while (true)
    {
        if (!g_app.isAllSync())
        {
            usleep(1000);
        }
        else
            break;
    }

    unsigned int uBegin = (unsigned int)transferingInfo.fromPageNo*_pageSize;
    unsigned int uEnd = (unsigned int)(transferingInfo.toPageNo + 1)*_pageSize - 1;
    if (uEnd < uBegin)
    {
        uEnd = UnpackTable::__max;
    }
    while (true)
    {
        if (g_app.haveSyncKeyIn(uBegin, uEnd))
        {
            usleep(10000);
        }
        else
            break;
    }

    current->setResponse(false);
    TransParamPtr param = new TransParam();
    try
    {
        int iPageNo = transferingInfo.fromPageNo;

        string sAddr = getTransDest(iPageNo);
        if (sAddr.length() <= 0)
        {
            TLOG_ERROR("[RouterClientImp::fromTransferDo] trans dest addr is null" << endl);
            throw(TarsException("trans dest addr is null"));
        }
        TLOG_DEBUG("RouterClientImp::fromTransferDo trans dest = " << sAddr << endl);
        RouterClientPrx pRouterClientPrx = Application::getCommunicator()->stringToProxy<RouterClientPrx>(sAddr);

        vector<Data> v;
        v.reserve(20);
        vector<string> vtOnlyKey;
        vtOnlyKey.reserve(100);
        for (; iPageNo <= transferingInfo.toPageNo; ++iPageNo)
        {
            unsigned int uBegin = (unsigned int)iPageNo*_pageSize;
            unsigned int uEnd = uBegin + _pageSize - 1;
            if (uEnd < uBegin)
            {
                uEnd = UnpackTable::__max;
            }
            int iRet;
            bool bFinish = false;

            v.clear();
            vtOnlyKey.clear();
            size_t i = uBegin;
            while (true)
            {
                KeyMatch k;

                vector<SHashMap::CacheDataRecord> vv;
                k.Set(i);
                iRet = g_sHashMap.getHashWithOnlyKey(i, vv, k);
                if (iRet != TC_HashMapMalloc::RT_OK)
                {
                    TLOG_ERROR("[RouterClientImp::fromTransferDo] llhashmap getHash error, iret =  " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    break;
                }

                for (size_t j = 0; j < vv.size(); ++j)
                {
                    if (vv[j]._onlyKey)
                    {
                        vtOnlyKey.push_back(vv[j]._key);
                    }
                    else
                    {
                        v.push_back(Data());
                        Data &data = v.back();
                        data.k.keyItem = vv[j]._key;
                        data.v.value = vv[j]._value;
                        data.expireTimeSecond = vv[j]._expiret;
                        data.dirty = vv[j]._dirty;

                    }
                }

                if (v.size() >= 20 || (i == uEnd && v.size() > 0))
                {
                    ++param->_count;
                    try
                    {
                        RouterClientPrxCallbackPtr cb = new RouterClientCallback(current, param);
                        pRouterClientPrx->async_setBatchFroTrans(cb, _moduleName, v);
                        if (i != uEnd)
                            v.clear();
                    }
                    catch (const TarsException & ex)
                    {
                        TLOG_ERROR("[RouterClientImp::fromTransferDo] trans exception: " << ex.what() << endl);
                        break;
                    }
                }

                if ((vtOnlyKey.size() >= 100) || (i == uEnd && vtOnlyKey.size() > 0))
                {
                    ++param->_count;
                    try
                    {
                        RouterClientPrxCallbackPtr cb = new RouterClientCallback(current, param);
                        pRouterClientPrx->async_setBatchFroTransOnlyKey(cb, _moduleName, vtOnlyKey);
                        if (i != uEnd)
                        {
                            vtOnlyKey.clear();
                        }
                    }
                    catch (const TarsException & ex)
                    {
                        TLOG_ERROR("[RouterHandle::fromTransferDo] trans exception: " << ex.what() << endl);
                        break;
                    }
                }

                if (i == uEnd)
                {
                    bFinish = true;
                    break;
                }
                ++i;
            }
            if (!bFinish)
                break;
        }
        if (iPageNo > transferingInfo.toPageNo)
        {
            if (param->setFinish())
            {
                RouterClient::async_response_fromTransferDo(current, 0);
            }
            return 0;
        }
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("[RouterClientImp::fromTransferDo] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::fromTransferDo] unkown_exception" << endl);
    }

    if (param->setEnd())
    {
        RouterClient::async_response_fromTransferDo(current, -1);
    }
    return 0;
}

tars::Int32 RouterHandle::toTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const TransferInfo& transferingInfo, const PackTable& packTable)
{
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不做迁移
            TLOG_ERROR("[RouterClientImp::toTransferStart] is not Master" << endl);
            return 1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("[RouterClientImp::toTransferStart] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
            return 2;
        }

        {
            TC_ThreadLock::Lock lock(_lock);
            if (transInfoListVer > _transInfoListVer || transInfoListVer == TRANSFER_CLEAN_VERSION)
            {
                int iRet = g_route_table.reload(packTable, transferingInfoList);
                if (iRet != UnpackTable::RET_SUCC)
                {
                    TLOG_ERROR("[RouterClientImp::toTransferStart] reload route error, iRet = " << iRet << endl);
                    return -1;
                }
                _transInfoListVer = transInfoListVer;
                _transferingInfoList = transferingInfoList;

                g_route_table.toFile(_routeFile);

                if (g_app.gstat()->serverType() != MASTER)
                {
                    _transferingInfoList.clear();
                    updateCSyncKeyLimit();
                    TLOG_ERROR("[RouterClientImp::toTransferStart] Server Type error" << endl);
                    return 3;
                }
                updateCSyncKeyLimit();
            }
        }

        for (int iPageNo = transferingInfo.fromPageNo; iPageNo <= transferingInfo.toPageNo; ++iPageNo)
        {
            unsigned int uBegin = (unsigned int)iPageNo*_pageSize;
            unsigned int uEnd = uBegin + _pageSize - 1;
            if (uEnd < uBegin)
            {
                uEnd = UnpackTable::__max;
            }
            int iRet;

            size_t i = uBegin;
            while (true)
            {
                KeyMatch k;

                k.Set(i);
                iRet = g_sHashMap.eraseHashByForce(i, k);

                if (iRet != TC_HashMapMalloc::RT_OK)
                {
                    TLOG_ERROR("[RouterClientImp::toTransferStart] llhashmap eraseHashByForce error, iret =  " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return -1;
                }
                if (i == uEnd)
                {
                    break;
                }
                ++i;
            }
        }
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("[RouterClientImp::toTransferStart] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::toTransferStart] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::setRouterInfoForSwicth(const std::string & moduleName, const DCache::PackTable & packTable)
{
    try
    {
        if (g_app.gstat()->serverType() == SLAVE && g_app.gstat()->isSlaveCreating() == true)
        {
            //在SLAVE自建的状态下 禁止切换
            TLOG_ERROR("[RouterClientImp::setRouterInfoForSwicth] cant swicth" << endl);
            return -1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("[RouterClientImp::setRouterInfoForSwicth] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
            return -1;
        }

        TC_ThreadLock::Lock lock(_lock);
        int iRet = g_route_table.reload(packTable);
        if (iRet != UnpackTable::RET_SUCC)
        {
            TLOG_ERROR("[RouterClientImp::setRouterInfoForSwicth] reload route error, iRet = " << iRet << endl);
            return -1;
        }
        _transInfoListVer = TRANSFER_CLEAN_VERSION;
        _transferingInfoList.clear();

        g_route_table.toFile(_routeFile);

        //因为有主动降级功能，主备角色只有服务启动和主备切换时才能设置
        enum ServerType role = getType();
        enum ServerType curRole = g_app.gstat()->serverType();
        if (curRole == MASTER && role == SLAVE)
            g_app.enableConnectHb(true);
        else if (role == MASTER)
            g_app.enableConnectHb(false);

        g_app.gstat()->setServerType(role);
        FDLOG("switch") << __FUNCTION__ << "|server change from " << (curRole == MASTER ? "MASTER" : "SLAVE")
                        << " to " << (role == MASTER ? "MASTER" : "SLAVE") << endl;

        updateCSyncKeyLimit();
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("[RouterClientImp::setRouterInfoForSwicth] exception: " << ex.what() << endl);
        return -1;
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("[RouterClientImp::setRouterInfoForSwicth] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::setRouterInfoForSwicth] unkown_exception" << endl);
        return -1;
    }
    return 0;
}
tars::Int32 RouterHandle::getBinlogdif(tars::Int32 &difBinlogTime)
{
    try
    {
        if (g_app.gstat()->serverType() == MASTER)
        {
            TLOG_ERROR("[RouterClientImp::getBinlogdif] is not Slave" << endl);
            return 1;
        }
        else
        {
            difBinlogTime = g_app.gstat()->getBinlogTimeLast() - g_app.gstat()->getBinlogTimeSync();
            return 0;
        }
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("[RouterClientImp::getBinlogdif] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::getBinlogdif] unkown_exception" << endl);
        return -1;
    }
}

void RouterHandle::heartBeat()
{
    try
    {
        string serverName = ServerConfig::Application + "." + ServerConfig::ServerName;
        _routePrx->heartBeatReport(_moduleName, g_app.gstat()->groupName(), serverName);
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("[RouterHandle::heartBeat] exception: " << ex.what() << endl);

        try
        {
            string serverName = ServerConfig::Application + "." + ServerConfig::ServerName;
            _routePrx->heartBeatReport(_moduleName, g_app.gstat()->groupName(), serverName);
        }
        catch (const TarsException & ex)
        {
            TLOG_ERROR("[RouterHandle::heartBeat] exception: " << ex.what() << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        }
    }
}

int RouterHandle::getUpdateServant(const string & mainkey, int index, bool bWrite, const string &idc, RspUpdateServant &updateServant)
{
    int ret = 0;

    try
    {

        if (bWrite)
        {
            //从路由表中获得要访问的CacheServer的节点信息
            ServerInfo serverInfo;
            ret = g_route_table.getMaster(mainkey, serverInfo);
            if (ret != 0)
            {
                TLOG_ERROR("RouterHandle::getUpdateServant get server error:" << ret << endl);
                return ret;
            }

            updateServant.mpServant[serverInfo.WCacheServant].push_back(index);
        }
        else
        {
            //从路由表中获得要访问的CacheServer的节点信息
            ServerInfo serverInfo;
            ret = g_route_table.getIdcServer(mainkey, idc, false, serverInfo);
            if (ret != 0)
            {
                TLOG_ERROR("RouterHandle::getUpdateServant get server error:" << ret << endl);
                return ret;
            }

            updateServant.mpServant[serverInfo.CacheServant].push_back(index);
        }

        return ret;
    }
    catch (const std::exception& ex)
    {
        TLOG_ERROR("[RouterHandle::getUpdateServant] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("[RouterHandle::getUpdateServant] unkown_exception" << endl);
    }

    return -1;
}

int RouterHandle::getUpdateServant(const string & mainkey, bool bWrite, const string &idc, RspUpdateServant &updateServant)
{
    return getUpdateServant(mainkey, 0, bWrite, idc, updateServant);
}


int RouterHandle::getUpdateServant(const vector<string> &vtMainkey, const vector<int> &vIndex, bool bWrite, const string &idc, RspUpdateServant &updateServant)
{
    int ret = 0;

    try
    {

        if (bWrite)
        {
            //从路由表中获得要访问的CacheServer的节点信息
            for (size_t i = 0; i < vtMainkey.size(); i++)
            {
                ServerInfo serverInfo;
                ret = g_route_table.getMaster(vtMainkey[i], serverInfo);
                if (ret != 0)
                    break;

                updateServant.mpServant[serverInfo.WCacheServant].push_back(vIndex[i]);
            }
        }
        else
        {
            //从路由表中获得要访问的CacheServer的节点信息
            for (size_t i = 0; i < vtMainkey.size(); i++)
            {
                ServerInfo serverInfo;
                ret = g_route_table.getIdcServer(vtMainkey[i], idc, false, serverInfo);
                if (ret != 0)
                    break;

                updateServant.mpServant[serverInfo.CacheServant].push_back(vIndex[i]);
            }
        }
        if (ret != 0)
        {
            TLOG_ERROR("RouterHandle::getUpdateServant get server error:" << ret << endl);
        }

        return ret;
    }
    catch (const std::exception& ex)
    {
        TLOG_ERROR("[RouterHandle::getUpdateServant] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("[RouterHandle::getUpdateServant] unkown_exception" << endl);
    }

    return -1;
}

int RouterHandle::getUpdateServant(const vector<string> &vtMainkey, bool bWrite, const string &idc, RspUpdateServant &updateServant)
{
    int ret = 0;

    try
    {

        if (bWrite)
        {
            //从路由表中获得要访问的CacheServer的节点信息
            for (size_t i = 0; i < vtMainkey.size(); i++)
            {
                ServerInfo serverInfo;
                ret = g_route_table.getMaster(vtMainkey[i], serverInfo);
                if (ret != 0)
                    break;

                updateServant.mpServant[serverInfo.WCacheServant].push_back(i);
            }
        }
        else
        {
            //从路由表中获得要访问的CacheServer的节点信息
            for (size_t i = 0; i < vtMainkey.size(); i++)
            {
                ServerInfo serverInfo;
                ret = g_route_table.getIdcServer(vtMainkey[i], idc, false, serverInfo);
                if (ret != 0)
                    break;

                updateServant.mpServant[serverInfo.CacheServant].push_back(i);
            }
        }
        if (ret != 0)
        {
            TLOG_ERROR("RouterHandle::getUpdateServant get server error:" << ret << endl);
        }

        return ret;
    }
    catch (const std::exception& ex)
    {
        TLOG_ERROR("[RouterHandle::getUpdateServant] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("[RouterHandle::getUpdateServant] unkown_exception" << endl);
    }

    return -1;
}

int RouterHandle::getUpdateServantKey(const vector<string> &vtMainkey, bool bWrite, const string &idc, RspUpdateServant &updateServant)
{
    int ret = 0;

    try
    {

        if (bWrite)
        {
            //从路由表中获得要访问的CacheServer的节点信息
            for (size_t i = 0; i < vtMainkey.size(); i++)
            {
                ServerInfo serverInfo;
                ret = g_route_table.getMaster(vtMainkey[i], serverInfo);
                if (ret != 0)
                    break;

                updateServant.mpServantKey[serverInfo.WCacheServant].push_back(vtMainkey[i]);
            }
        }
        else
        {
            //从路由表中获得要访问的CacheServer的节点信息
            for (size_t i = 0; i < vtMainkey.size(); i++)
            {
                ServerInfo serverInfo;
                ret = g_route_table.getIdcServer(vtMainkey[i], idc, false, serverInfo);
                if (ret != 0)
                    break;

                updateServant.mpServantKey[serverInfo.CacheServant].push_back(vtMainkey[i]);
            }
        }
        if (ret != 0)
        {
            TLOG_ERROR("RouterHandle::getUpdateServant get server error:" << ret << endl);
        }

        return ret;
    }
    catch (const std::exception& ex)
    {
        TLOG_ERROR("[RouterHandle::getUpdateServant] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("[RouterHandle::getUpdateServant] unkown_exception" << endl);
    }

    return -1;
}

int RouterHandle::updateServant2Str(const RspUpdateServant &updateServant, string &sRet)
{
    try
    {
        tars::TarsOutputStream<tars::BufferWriter> os;
        updateServant.writeTo(os);
        sRet.assign(os.getBuffer(), os.getLength());

        return 0;
    }
    catch (const std::exception& ex)
    {
        TLOG_ERROR("[RouterHandle::updateServant2Str] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("[RouterHandle::updateServant2Str] unkown_exception" << endl);
    }

    return -1;
}

string RouterHandle::getConnectHbAddr()
{
    string sRet("");
    try
    {
        if (g_app.gstat()->serverType() == MASTER)
        {
            return sRet;
        }

        string serverName = ServerConfig::Application + "." + ServerConfig::ServerName;
        GroupInfo group;
        int ret = g_route_table.getGroup(serverName, group);
        if (ret != 0)
        {
            TLOG_ERROR("RouterHandle::" << __FUNCTION__ << "|getGroup error:" << ret << endl);
            return sRet;
        }

        string &masterServer = group.masterServer;

        const PackTable& pack = g_route_table.getPackTable();
        map<string, ServerInfo>::const_iterator itMaster = pack.serverList.find(masterServer);
        map<string, ServerInfo>::const_iterator itSelf = pack.serverList.find(serverName);
        if (itMaster == pack.serverList.end() || itSelf == pack.serverList.end())
        {
            TLOG_ERROR("RouterHandle::" << __FUNCTION__ << "|find serverInfo error" << endl);
            return sRet;
        }

        if (itSelf->second.idc == itMaster->second.idc)
        {
            string example = itMaster->second.BinLogServant;
            vector<string> vtStr = TC_Common::sepstr<string>(example, ".");
            sRet = vtStr[0] + "." + vtStr[1] + ".ControlAckObj";
        }
    }
    catch (const exception & ex)
    {
        TLOG_ERROR("RouterHandle::" << __FUNCTION__ << " exception: " << ex.what() << endl);
    }

    return sRet;
}

int RouterHandle::masterDowngrade()
{
    if (g_app.gstat()->serverType() != MASTER)
    {
        string err("RouterHandle::masterDowngrade, server is not master. error.");
        TLOG_ERROR(err << endl);
        FDLOG("switch") << err << endl;
        return -1;
    }

    FDLOG("switch") << "RouterHandle::masterDowngrade start." << endl;

    int ret = 0;

    g_app.gstat()->setServerType(SLAVE);

    //降级后正常开始上报连接心跳给master
    g_app.enableConnectHb(true);

    FDLOG("switch") << "RouterHandle::masterDowngrade succ." << endl;

    return ret;
}

///////////////////////////////////////////////////////
void RouterHandle::updateCSyncKeyLimit()
{
    vector<CanSync::KeyLimit> vtKeyLimit;
    vtKeyLimit.clear();

    for (size_t i = 0; i < _transferingInfoList.size(); ++i)
    {
        CanSync::KeyLimit keyLimit;
        keyLimit.lower = (unsigned int)_transferingInfoList[i].fromPageNo*_pageSize;
        keyLimit.upper = (unsigned int)(_transferingInfoList[i].toPageNo + 1)*_pageSize - 1;
        if (keyLimit.upper < keyLimit.lower)
        {
            keyLimit.upper = UnpackTable::__max;
        }

        vtKeyLimit.push_back(keyLimit);
    }

    CanSync& canSync = g_app.gstat()->getCanSync();
    canSync.Set(vtKeyLimit);
}

string RouterHandle::getTransDest(int pageNo)
{
    ServerInfo srcServer;
    ServerInfo destServer;
    int iRet = g_route_table.getTrans(pageNo, srcServer, destServer);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOG_ERROR("[RouterClientImp::getTransDest] getTrans error, iRet = " << iRet << endl);
        return "";
    }
    string sTransDest = destServer.RouteClientServant;
    return sTransDest;
}

enum ServerType RouterHandle::getType()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    GroupInfo group;
    g_route_table.getGroup(sServerName, group);

    if (group.masterServer == sServerName)
    {
        return MASTER;
    }
    return SLAVE;
}

bool RouterHandle::isInTransferingInfoList(const TransferInfo& transferingInfo)
{
    TC_ThreadLock::Lock lock(_lock);

    vector<TransferInfo>::const_iterator it = _transferingInfoList.begin();
    for (; it != _transferingInfoList.end(); ++it)
    {
        if (*it == transferingInfo)
        {
            return true;
        }
    }

    return false;
}

