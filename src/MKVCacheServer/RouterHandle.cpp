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
#include "RouterClient.h"
#include "MKCacheServer.h"

extern MKCacheServer g_app;

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

    _pageSize = TC_Common::strto<unsigned int>(conf["/Main/Router<PageSize>"]);

    _routeObj = conf["/Main/Router<ObjName>"];

    _routePrx = Application::getCommunicator()->stringToProxy<RouterPrx>(_routeObj);

    string sTransferCompress = TC_Common::trim(conf.get("/Main/Cache<transferCompress>", "Y"));
    if (sTransferCompress == "Y" || sTransferCompress == "y")
        _transferCompress = true;
    else
        _transferCompress = false;

    TLOGDEBUG("RouterHandle::initialize Succ" << endl);
}

tars::Int32 RouterHandle::setRouterInfo(const string &moduleName, const PackTable & packTable, tars::TarsCurrentPtr current)
{
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不做迁移
            TLOGERROR("[RouterHandle::setRouterInfo] is not Master" << endl);
            return -1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("[RouterHandle::setRouterInfo] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
            return -1;
        }
        TC_ThreadLock::Lock lock(_lock);

        int iRet = g_route_table.reload(packTable);
        if (iRet != UnpackTable::RET_SUCC)
        {
            TLOGERROR("[RouterHandle::setRouterInfo] reload route error, iRet = " << iRet << endl);
            return -1;
        }
        _transInfoListVer = TRANSFER_CLEAN_VERSION;
        _transferingInfoList.clear();

        g_route_table.toFile(_routeFile);

        updateCSyncKeyLimit();
    }
    catch (const TarsException & ex)
    {
        TLOGERROR("[RouterHandle::setRouterInfo] exception: " << ex.what() << endl);
        return -1;
    }
    catch (const std::exception & ex)
    {
        TLOGERROR("[RouterHandle::setRouterInfo] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::setRouterInfo] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::setTransRouterInfo(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const PackTable& packTable,
        tars::TarsCurrentPtr current)
{
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不做迁移
            TLOGERROR("[RouterHandle::setTransRouterInfo] is not Master" << endl);
            return -1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("[RouterHandle::setTransRouterInfo] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
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
            TLOGERROR("[RouterHandle::setRouterInfo] reload route error, iRet = " << iRet << endl);
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
        TLOGERROR("[RouterHandle::setRouterInfo] exception: " << ex.what() << endl);
        return -1;
    }
    catch (const std::exception & ex)
    {
        TLOGERROR("[RouterHandle::setRouterInfo] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::setRouterInfo] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::fromTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const PackTable& packTable, tars::TarsCurrentPtr current)
{
    try
    {
        TLOGDEBUG("fromTransferStart: " << transInfoListVer << ", " << transferingInfoList.size() << endl);
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不做迁移
            TLOGERROR("[RouterHandle::fromTransferStart] is not Master" << endl);
            return 1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("[RouterHandle::fromTransferStart] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
            return 2;
        }

        {
            TC_ThreadLock::Lock lock(_lock);
            if (transInfoListVer > _transInfoListVer || transInfoListVer == TRANSFER_CLEAN_VERSION)
            {
                int iRet = g_route_table.reload(packTable, transferingInfoList);
                if (iRet != UnpackTable::RET_SUCC)
                {
                    TLOGERROR("[RouterHandle::fromTransferStart] reload route error, iRet = " << iRet << endl);
                    return -1;
                }
                _transInfoListVer = transInfoListVer;
                _transferingInfoList = transferingInfoList;

                g_route_table.toFile(_routeFile);

                TLOGDEBUG("RouterHandle::fromTransferStart route table:" << g_route_table.toString() << endl);

                if (g_app.gstat()->serverType() != MASTER)
                {
                    _transferingInfoList.clear();
                    updateCSyncKeyLimit();
                    TLOGERROR("[RouterHandle::fromTransferStart] Server Type error" << endl);
                    return 3;
                }

                updateCSyncKeyLimit();
            }
        }
    }
    catch (const std::exception & ex)
    {
        TLOGERROR("[RouterHandle::fromTransferStart] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::fromTransferStart] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::fromTransferDo(const TransferInfo& transferingInfo, tars::TarsCurrentPtr current)
{
    try
    {
        TLOGDEBUG("RouterHandle::fromTransferDo begin trans, fromPageNo: " << transferingInfo.fromPageNo << ", toPageNo: " << transferingInfo.toPageNo << endl);

        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不做迁移
            TLOGERROR("[RouterHandle::fromTransferDo] is not Master" << endl);
            return 1;
        }
        if (transferingInfo.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("[RouterHandle::fromTransferDo] moduleName error, " << _moduleName << " != " << transferingInfo.moduleName << "(config != param)" << endl);
            return 2;
        }

        if (!isInTransferingInfoList(transferingInfo))
        {
            //要执行的迁移已经不在迁移记录列表中
            TLOGERROR("[RouterHandle::fromTransferDo] transferingInfo invalid" << endl);
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
            if (g_app.haveSyncKeyIn(uBegin, uEnd) || (g_app.haveDeleteKeyIn(uBegin, uEnd)))
            {
                usleep(10000);
            }
            else
                break;
        }

        string sAddr = getTransDest(transferingInfo.fromPageNo);
        if (sAddr.length() <= 0)
        {
            TLOGERROR("[RouterHandle::fromTransferDo] trans dest addr is null" << endl);
            return -1;
        }
        TLOGDEBUG("RouterHandle::fromTransferDo trans dest = " << sAddr << endl);

        vector<string> vtOnlyKey;
        vtOnlyKey.reserve(100);

        string outStr;
        string sCompress;
        TC_PackIn pi;

        TC_Multi_HashMap_Malloc::MainKey::KEYTYPE keyType;
        g_HashMap.getMainKeyType(keyType);

        RouterClientPrx pRouterClientPrx = Application::getCommunicator()->stringToProxy<RouterClientPrx>(sAddr);
        TransDataSizeLimit stTransLimit = getDestTransLimit(sAddr);

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

                //转换为32位
                uint32_t hash = i;
                k.Set(hash);
                map<string, vector<MultiHashMap::Value> > vv;
                iRet = g_HashMap.getHashM(hash, vv, k);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    TLOGERROR("[RouterHandle::fromTransferDo] getHash error, iret =  " << iRet << endl);
                    return -1;
                }
//                map<string, vector<MultiHashMap::Value> >::iterator it = vv.begin(), itEnd = vv.end();
                for (map<string, vector<MultiHashMap::Value> >::iterator it = vv.begin(); it != vv.end(); ++it)
                {
                    bool full = (g_HashMap.checkMainKey(it->first) == TC_Multi_HashMap_Malloc::RT_OK ? true : false);

                    const vector<MultiHashMap::Value>& v = it->second;
                    if (v.size() == 0)
                    {
                        if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE != keyType)
                        {
                            try
                            {
                                int iRet;
                                iRet = pRouterClientPrx->setFroCompressTransWithType(_moduleName, it->first, "", (uint8_t)keyType, false, false);
                                if (iRet != ET_SUCC)
                                {
                                    TLOGERROR("[RouterHandle::fromTransferDo] set key " << it->first << ", hash = " << hash << ", iRet = " << iRet << endl);
                                    return -1;
                                }
                            }
                            catch (exception& ex)
                            {
                                TLOGERROR("[RouterHandle::fromTransferDo] trans exception: " << ex.what() << endl);
                                updateDestTransLimit(sAddr, stTransLimit, false);
                                return -1;
                            }
                        }
                        else
                        {
                            vtOnlyKey.push_back(it->first);
                            if (vtOnlyKey.size() >= 100)
                            {
                                try
                                {
                                    int iRet;
                                    iRet = pRouterClientPrx->setFroCompressTransOnlyKey(_moduleName, vtOnlyKey);
                                    if (iRet != ET_SUCC)
                                    {
                                        TLOGERROR("[RouterHandle::fromTransferDo] set onlykey key error! " << iRet << endl);
                                        return -1;
                                    }
                                }
                                catch (exception& ex)
                                {
                                    TLOGERROR("[RouterHandle::fromTransferDo] trans exception: " << ex.what() << endl);
                                    updateDestTransLimit(sAddr, stTransLimit, false);
                                    return -1;
                                }

                                vtOnlyKey.clear();
                            }
                        }
                    }
                    else
                    {
                        size_t i = 0;
                        while (i < v.size())
                        {
                            size_t j = i;
                            ostringstream os;
                            while (j < v.size())
                            {
                                TC_PackIn pi;
                                pi << int(v[j]._isDelete);
                                pi << ((v[j]._dirty) ? (tars::Char)1 : (tars::Char)0);
                                pi << v[j]._iExpireTime;
                                pi << v[j]._score;
                                pi << v[j]._ukey;
                                pi << v[j]._value;

                                if (j != i && pi.length() + outStr.length() > stTransLimit.iMaxSizeLimit)
                                {
                                    break;
                                }
                                uint32_t iLen = htonl((uint32_t)pi.length());
                                outStr.append((const char*)&iLen, sizeof(uint32_t));
                                outStr.append(pi.getBuffer());
                                ++j;
                            }

                            try
                            {
                                int iRet;
                                if (_transferCompress && (outStr.length() > StringUtil::GZIP_MIN_STR_LEN))
                                {
                                    sCompress.reserve(outStr.length());
                                    if (!StringUtil::gzipCompress(outStr.c_str(), outStr.length(), sCompress))
                                    {
                                        iRet = pRouterClientPrx->setFroCompressTransWithType(_moduleName, it->first, outStr, v[0]._keyType, false, j == v.size() ? full : false);
                                    }
                                    else
                                        iRet = pRouterClientPrx->setFroCompressTransWithType(_moduleName, it->first, sCompress, v[0]._keyType, true, j == v.size() ? full : false);
                                }
                                else
                                {
                                    iRet = pRouterClientPrx->setFroCompressTransWithType(_moduleName, it->first, outStr, v[0]._keyType, false, j == v.size() ? full : false);
                                }
                                if (iRet != ET_SUCC)
                                {
                                    TLOGERROR("[RouterHandle::fromTransferDo] set key " << it->first << ", hash = " << hash << ", iRet = " << iRet << endl);
                                    return -1;
                                }

                                sCompress.clear();
                                outStr.clear();
                            }
                            catch (exception& ex)
                            {
                                TLOGERROR("[RouterHandle::fromTransferDo] trans exception: " << ex.what() << endl);
                                updateDestTransLimit(sAddr, stTransLimit, false);
                                return -1;
                            }

                            i = j;
                        }
                    }
                }
                if (i == uEnd)
                {
                    break;
                }
                ++i;
            }
        }

        if (vtOnlyKey.size() > 0)
        {
            try
            {
                int iRet;
                iRet = pRouterClientPrx->setFroCompressTransOnlyKey(_moduleName, vtOnlyKey);
                if (iRet != ET_SUCC)
                {
                    TLOGERROR("[RouterHandle::fromTransferDo] set onlykey key error! " << iRet << endl);
                    return -1;
                }
            }
            catch (exception& ex)
            {
                TLOGERROR("[RouterHandle::fromTransferDo] trans exception: " << ex.what() << endl);
                updateDestTransLimit(sAddr, stTransLimit, false);
                return -1;
            }

            vtOnlyKey.clear();
        }

        updateDestTransLimit(sAddr, stTransLimit);

        TLOGDEBUG("RouterHandle::fromTransferDo trans succ, fromPageNo = " << transferingInfo.fromPageNo << ", toPageNo = " << transferingInfo.toPageNo << endl);
    }
    catch (const std::exception & ex)
    {
        TLOGERROR("[RouterHandle::fromTransferDo] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::fromTransferDo] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::toTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const TransferInfo& transferingInfo, const PackTable& packTable, tars::TarsCurrentPtr current)
{
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不做迁移
            TLOGERROR("[RouterHandle::toTransferStart] is not Master" << endl);
            return 1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("[RouterHandle::toTransferStart] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
            return 2;
        }

        {
            TC_ThreadLock::Lock lock(_lock);
            if (transInfoListVer > _transInfoListVer || transInfoListVer == TRANSFER_CLEAN_VERSION)
            {
                int iRet = g_route_table.reload(packTable, transferingInfoList);
                if (iRet != UnpackTable::RET_SUCC)
                {
                    TLOGERROR("[RouterHandle::toTransferStart] reload route error, iRet = " << iRet << endl);
                    return -1;
                }
                _transInfoListVer = transInfoListVer;
                _transferingInfoList = transferingInfoList;

                g_route_table.toFile(_routeFile);

                if (g_app.gstat()->serverType() != MASTER)
                {
                    _transferingInfoList.clear();
                    updateCSyncKeyLimit();
                    TLOGERROR("[RouterHandle::toTransferStart] Server Type error" << endl);
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

                //因为hashmap内部把有符号int32的hash值转成size_t来计算hash桶的位置，这里必须保持一致
                uint32_t hash = i;

                map<string, vector<MultiHashMap::Value> > vv;
                k.Set(hash);
                iRet = g_HashMap.getHashM(hash, vv, k);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    TLOGERROR("[RouterHandle::toTransferStart] getHash error, iret =  " << iRet << endl);
                    return -1;
                }

                for (map<string, vector<MultiHashMap::Value> >::iterator it = vv.begin(); it != vv.end(); ++it)
                {
                    iRet = g_HashMap.eraseByForce(it->first);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        TLOGERROR("[RouterHandle::toTransferStart] eraseByForce error, iret = " << iRet << endl);
                        return -1;
                    }
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
        TLOGERROR("[RouterHandle::toTransferStart] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::toTransferStart] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::initRoute(bool forbidFromFile, bool &syncFromRouterSuccess)
{
    syncFromRouterSuccess = false;
    PackTable packTable;
    try
    {
        int iRet = _routePrx->getRouterInfoFromCache(_moduleName, packTable);

        if (iRet != ROUTER_SUCC)
        {
            TLOGERROR("[RouterHandle::initRoute] getRouterInfo from " << _routeObj << " failed, ret=" << iRet << endl);
        }
        else
        {
            //保存最新路由表
            string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
            int iRouteRet = g_route_table.init(packTable, sServerName);
            if (iRouteRet != UnpackTable::RET_SUCC)
            {
                TLOGERROR("[RouterHandle::initRoute] g_route_table.init failed, ret=" << iRouteRet << endl);
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
        TLOGERROR("[RouterHandle::initRoute] getRouterInfo from " << _routeObj << " exception:" << ex.what() << endl);
    }

    if (forbidFromFile)
    {
        return -1;
    }

    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
    int iRet = g_route_table.fromFile(_routeFile, sServerName);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOGERROR("[RouterHandle::initRoute] g_route_table.fromFile failed, ret=" << iRet << endl);
        return -1;
    }

    return 0;
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
                TLOGERROR("[RouterHandle::syncRoute] getTransRouterInfo fail" << endl);
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
                    TLOGERROR("[RouterHandle::syncRoute] reload route error: " << iRet << endl);
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

                TLOGDEBUG("update route succ" << endl);
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
                    TLOGERROR("[RouterHandle::syncRoute] getRouterInfo fail" << endl);
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
                    TLOGERROR("[RouterHandle::SyncRoute] reload route error, iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return;
                }
                g_route_table.toFile(ServerConfig::DataPath + "/" + _routeFile);

                updateCSyncKeyLimit();

                TLOGDEBUG("update route succ" << endl);
            }
        }
    }
    catch (const TarsException & ex)
    {
        TLOGERROR("[RouterHandle::syncRoute] exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
    }
}

void RouterHandle::doAfterTypeChange()
{
    TC_ThreadLock::Lock lock(_lock);
    if (g_app.gstat()->serverType() == SLAVE)
    {
        _transferingInfoList.clear();

        updateCSyncKeyLimit();
    }
}

string RouterHandle::getTransDest(int pageNo)
{
    ServerInfo srcServer;
    ServerInfo destServer;
    int iRet = g_route_table.getTrans(pageNo, srcServer, destServer);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOGERROR("[RouterHandle::getTransDest] getTrans error, iRet = " << iRet << endl);
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

void RouterHandle::cleanDestTransLimit()
{
    time_t tNow = TC_TimeProvider::getInstance()->getNow();
    TC_ThreadLock::Lock lock(_lock);
    map<string, TransDataSizeLimit>::iterator it = _destTransLimit.begin();
    while (it != _destTransLimit.end())
    {
        if (tNow - it->second.tLastUpdate >= 3600)
        {
            _destTransLimit.erase(it++);
        }
        else
            ++it;
    }
}

TransDataSizeLimit RouterHandle::getDestTransLimit(const string& dest)
{
    TC_ThreadLock::Lock lock(_lock);

    map<string, TransDataSizeLimit>::const_iterator it = _destTransLimit.find(dest);
    if (it == _destTransLimit.end())
    {
        TransDataSizeLimit st;
        st.iStepFactor = 1;
        st.iMaxSizeLimit = PACKET_SIZE_LIMIT;
        st.iSlowWindow = PACKET_SIZE_LIMIT;
        st.tLastUpdate = TC_TimeProvider::getInstance()->getNow();
        _destTransLimit[dest] = st;
        return st;
    }

    return it->second;
}

void RouterHandle::updateDestTransLimit(const string& dest, TransDataSizeLimit& transLimit, bool transOk)
{
    if (transOk)
    {
        if (transLimit.iMaxSizeLimit < transLimit.iSlowWindow)
        {
            transLimit.iMaxSizeLimit = TRANS_ADDSIZE_STEP*((int)pow(2, transLimit.iStepFactor));
            if (transLimit.iMaxSizeLimit > transLimit.iSlowWindow)
            {
                transLimit.iMaxSizeLimit = transLimit.iSlowWindow;
            }
            else
            {
                ++transLimit.iStepFactor;
            }
        }
        else if (transLimit.iMaxSizeLimit < PACKET_SIZE_LIMIT)
        {
            transLimit.iMaxSizeLimit += transLimit.iStepFactor*TRANS_ADDSIZE_STEP;
            if (transLimit.iMaxSizeLimit > PACKET_SIZE_LIMIT)
            {
                transLimit.iMaxSizeLimit = PACKET_SIZE_LIMIT;
            }
        }
    }
    else
    {
        transLimit.iSlowWindow = transLimit.iMaxSizeLimit / 2;
        transLimit.iMaxSizeLimit = TRANS_ADDSIZE_STEP;
        transLimit.iStepFactor = 1;
    }

    transLimit.tLastUpdate = TC_TimeProvider::getInstance()->getNow();

    TC_ThreadLock::Lock lock(_lock);

    _destTransLimit[dest] = transLimit;
}

tars::Int32 RouterHandle::setRouterInfoForSwicth(const std::string & moduleName, const DCache::PackTable & packTable, tars::TarsCurrentPtr current)
{
    try
    {
        if (g_app.gstat()->serverType() == SLAVE && g_app.gstat()->isSlaveCreating() == true)
        {
            //在SLAVE自建的状态下 禁止切换
            TLOGERROR("[RouterHandle::setRouterInfoForSwicth] cant swicth" << endl);
            return -1;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("[RouterHandle::setRouterInfoForSwicth] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
            return -1;
        }

        TC_ThreadLock::Lock lock(_lock);
        int iRet = g_route_table.reload(packTable);
        if (iRet != UnpackTable::RET_SUCC)
        {
            TLOGERROR("[RouterHandle::setRouterInfoForSwicth] reload route error, iRet = " << iRet << endl);
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
        TLOGERROR("[RouterHandle::setRouterInfoForSwicth] exception: " << ex.what() << endl);
        return -1;
    }
    catch (const std::exception & ex)
    {
        TLOGERROR("[RouterHandle::setRouterInfoForSwicth] exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::setRouterInfoForSwicth] unkown_exception" << endl);
        return -1;
    }
    return 0;
}

tars::Int32 RouterHandle::getBinlogdif(tars::Int32 &difBinlogTime, tars::TarsCurrentPtr current)
{
    try
    {
        if (g_app.gstat()->serverType() == MASTER)
        {
            TLOGERROR("[RouterHandle::getBinlogdif] is not Slave" << endl);
            return 1;
        }
        else
        {
            difBinlogTime = g_app.gstat()->getBinlogTimeLast() - g_app.gstat()->getBinlogTimeSync();
            return 0;
        }
    }
    catch (const std::exception& ex)
    {
        TLOGERROR("[RouterHandle::getBinlogdif] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::getBinlogdif] unkown_exception" << endl);
    }
    return -1;
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
                TLOGERROR("RouterHandle::getUpdateServant get server error:" << ret << endl);
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
                TLOGERROR("RouterHandle::getUpdateServant get server error:" << ret << endl);
                return ret;
            }

            updateServant.mpServant[serverInfo.CacheServant].push_back(index);
        }

        return ret;
    }
    catch (const std::exception& ex)
    {
        TLOGERROR("[RouterHandle::getUpdateServant] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::getUpdateServant] unkown_exception" << endl);
    }

    return -1;
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
            TLOGERROR("RouterHandle::getUpdateServant get server error:" << ret << endl);
        }

        return ret;
    }
    catch (const std::exception& ex)
    {
        TLOGERROR("[RouterHandle::getUpdateServant] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::getUpdateServant] unkown_exception" << endl);
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
            TLOGERROR("RouterHandle::getUpdateServant get server error:" << ret << endl);
        }

        return ret;
    }
    catch (const std::exception& ex)
    {
        TLOGERROR("[RouterHandle::getUpdateServant] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::getUpdateServant] unkown_exception" << endl);
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
        TLOGERROR("[RouterHandle::updateServant2Str] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[RouterHandle::updateServant2Str] unkown_exception" << endl);
    }

    return -1;
}

void RouterHandle::serviceRestartReport()
{
    int iRet = 0;
    try
    {
        iRet = _routePrx->serviceRestartReport(_moduleName, g_app.gstat()->groupName());
        if (iRet != 0)
        {
            TLOGERROR("RouterHandle::serviceRestartReport error" << endl);
        }

        TLOGDEBUG("RouterHandle::serviceRestartReport, moduleName: " << _moduleName << ", groupName: " << g_app.gstat()->groupName() << endl);
    }
    catch (const TarsException & ex)
    {
        try
        {
            iRet = _routePrx->serviceRestartReport(_moduleName, g_app.gstat()->groupName());
            if (iRet != 0)
            {
                TLOGERROR("RouterHandle::serviceRestartReport error" << endl);
            }
        }
        catch (const TarsException & ex)
        {
            TLOGERROR("RouterHandle::serviceRestartReport exception: " << ex.what() << endl);
        }
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
        TLOGERROR("[RouterHandle::heartBeat] exception: " << ex.what() << endl);
        try
        {
            string serverName = ServerConfig::Application + "." + ServerConfig::ServerName;
            _routePrx->heartBeatReport(_moduleName, g_app.gstat()->groupName(), serverName);
        }
        catch (const TarsException & ex)
        {
            TLOGERROR("[RouterHandle::heartBeat] exception: " << ex.what() << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        }
    }
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
            TLOGERROR("RouterHandle::" << __FUNCTION__ << "|getGroup error:" << ret << endl);
            return sRet;
        }

        string &masterServer = group.masterServer;

        const PackTable& pack = g_route_table.getPackTable();
        map<string, ServerInfo>::const_iterator itMaster = pack.serverList.find(masterServer);
        map<string, ServerInfo>::const_iterator itSelf = pack.serverList.find(serverName);
        if (itMaster == pack.serverList.end() || itSelf == pack.serverList.end())
        {
            TLOGERROR("RouterHandle::" << __FUNCTION__ << "|find serverInfo error" << endl);
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
        TLOGERROR("RouterHandle::" << __FUNCTION__ << " exception: " << ex.what() << endl);
    }

    return sRet;
}

int RouterHandle::masterDowngrade()
{
    if (g_app.gstat()->serverType() != MASTER)
    {
        string err("RouterHandle::masterDowngrade, server is not master. error.");
        TLOGERROR(err << endl);
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

