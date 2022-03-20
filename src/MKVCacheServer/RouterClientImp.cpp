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
#include "RouterClientImp.h"

extern UnpackTable g_route_table;

void RouterClientImp::initialize()
{
    //initialize servant here:
    //...

    string sConf = ServerConfig::BasePath + "MKCacheServer.conf";
    _tcConf.parseFile(sConf);

    _moduleName = _tcConf["/Main<ModuleName>"];

    _binlogFile = _tcConf["/Main/BinLog<LogFile>"];

    _keyBinlogFile = _tcConf["/Main/BinLog<LogFile>"] + "key";

    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _cleanDataFunctor.init(_tcConf);

    _tpool.init(1);
    _tpool.start();

    _transContent.reserve(20000);
    _hashmapValue.reserve(20000);

    TLOG_DEBUG("RouterClientImp::initialize Succ" << endl);
}

void RouterClientImp::destroy()
{
}

tars::Int32 RouterClientImp::setRouterInfo(const string &moduleName, const PackTable & packTable, tars::TarsCurrentPtr current)
{
    return _routerHandle->setRouterInfo(moduleName, packTable, current);
}

tars::Int32 RouterClientImp::setTransRouterInfo(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const PackTable& packTable,
        tars::TarsCurrentPtr current)
{
    return _routerHandle->setTransRouterInfo(moduleName, transInfoListVer, transferingInfoList, packTable, current);
}

tars::Int32 RouterClientImp::fromTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const PackTable& packTable, tars::TarsCurrentPtr current)
{
    return _routerHandle->fromTransferStart(moduleName, transInfoListVer, transferingInfoList, packTable, current);
}

tars::Int32 RouterClientImp::fromTransferDo(const TransferInfo& transferingInfo, tars::TarsCurrentPtr current)
{
    return _routerHandle->fromTransferDo(transferingInfo, current);
}

tars::Int32 RouterClientImp::toTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const TransferInfo& transferingInfo, const PackTable& packTable, tars::TarsCurrentPtr current)
{
    return _routerHandle->toTransferStart(moduleName, transInfoListVer, transferingInfoList, transferingInfo, packTable, current);
}

tars::Int32 RouterClientImp::getBinlogdif(tars::Int32 &difBinlogTime, tars::TarsCurrentPtr current)
{
    return _routerHandle->getBinlogdif(difBinlogTime, current);
}

tars::Int32 RouterClientImp::setRouterInfoForSwitch(const std::string & moduleName, const DCache::PackTable & packTable, tars::TarsCurrentPtr current)
{
    return _routerHandle->setRouterInfoForSwicth(moduleName, packTable, current);
}
tars::Int32
RouterClientImp::helloBaby(tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("i am ok" << endl);
    return 0;
}
tars::Int32 RouterClientImp::helleBabyByName(const std::string & serverName, tars::TarsCurrentPtr current)
{
    return 0;
}
tars::Int32 RouterClientImp::setFroCompressTransEx(const std::string & moduleName, const std::string & mainKey, const std::string & transContent, tars::Bool compress, tars::Bool full, tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("RouterClientImp::setFroCompressTrans recv : " << mainKey << "|" << transContent.length() << "|" << compress << "|" << full << endl);

    _transContent.clear();
    _hashmapValue.clear();

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            TLOG_ERROR("[RouterClientImp::setFroCompressTrans] ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (moduleName != _moduleName)
        {
            TLOG_ERROR("[RouterClientImp::setFroCompressTrans] moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOG_ERROR("[RouterClientImp::setFroCompressTrans] " << mainKey << " forbid trans" << endl);
                return ET_FORBID_OPT;
            }
        }

        if (!g_route_table.isMySelf(mainKey))
        {
            TLOG_ERROR("[RouterClientImp::setFroCompressTrans] " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        if (transContent.empty())
        {
            int iRet = g_HashMap.set(mainKey);
            if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
            {
                if (_recordBinLog)
                    WriteBinLog::setMKOnlyKey(mainKey, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::setMKOnlyKey(mainKey, _keyBinlogFile);
            }
            return ET_SUCC;
        }
        else
        {
            bool bParseStrOk;
            if (compress)
            {
                string sUncompress;
                bool bGzipOk = StringUtil::gzipUncompress(transContent.c_str(), transContent.length(), sUncompress);
                if (!bGzipOk)
                {
                    TLOG_ERROR("[RouterClientImp::setFroCompressTrans] transContent gzip uncompress error" << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_SYS_ERR;
                }
                bParseStrOk = StringUtil::parseString(sUncompress, _transContent);
            }
            else
            {
                bParseStrOk = StringUtil::parseString(transContent, _transContent);
            }

            if (!bParseStrOk)
            {
                TLOG_ERROR("[RouterClientImp::setFroCompressTrans] transContent string parse error" << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_SYS_ERR;
            }

            for (size_t i = 0; i < _transContent.size(); ++i)
            {
                MultiHashMap::Value v;
                TC_PackOut po(_transContent[i].c_str(), _transContent[i].length());
                tars::Char dirty;
                int deleteType = 0;
                po >> deleteType;
                po >> dirty;
                po >> v._iExpireTime;
                po >> v._ukey;
                po >> v._value;
                v._iVersion = 0;
                v._mkey = mainKey;
                v._dirty = dirty == 1 ? true : false;
                v._isDelete = (TC_Multi_HashMap_Malloc::DELETETYPE)deleteType;

                _hashmapValue.push_back(v);
            }

            TC_Multi_HashMap_Malloc::DATATYPE eType;
            if (full)
                eType = TC_Multi_HashMap_Malloc::FULL_DATA;
            else
                eType = TC_Multi_HashMap_Malloc::PART_DATA;
            int iRet = g_HashMap.set(_hashmapValue, eType, false, false, false, true);

            if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
            {
                TLOG_ERROR("[RouterClientImp::setFroCompressTrans] hashmap set error, ret= " << iRet << ", mkey = " << mainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_KEY_AREA_ERR;
            }

            if (_recordBinLog)
            {
                WriteBinLog::set(mainKey, _hashmapValue, full, _binlogFile);
            }
            if (_recordKeyBinLog)
            {
                WriteBinLog::set(mainKey, _hashmapValue, full, _keyBinlogFile);
            }
        }
    }
    catch (const std::exception &ex)
    {
        TLOG_ERROR("[RouterClientImp::setFroCompressTrans] exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::setFroCompressTrans] unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 RouterClientImp::setFroCompressTransOnlyKey(const std::string & moduleName, const vector<std::string> & mainKey, tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("RouterClientImp::setFroCompressTransOnlyKey recv : " << mainKey.size() << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            TLOG_ERROR("[RouterClientImp::setFroCompressTransOnlyKey] ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (moduleName != _moduleName)
        {
            TLOG_ERROR("[RouterClientImp::setFroCompressTransOnlyKey] moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }

        vector<std::string>::const_iterator it = mainKey.begin();
        for (; it != mainKey.end(); it++)
        {
            if (g_route_table.isTransfering(*it))
            {
                int iPageNo = g_route_table.getPageNo(*it);
                if (isTransSrc(iPageNo))
                {
                    TLOG_ERROR("[RouterClientImp::setFroCompressTransOnlyKey] " << *it << " forbid trans" << endl);
                    return ET_FORBID_OPT;
                }
            }

            if (!g_route_table.isMySelf(*it))
            {
                TLOG_ERROR("[RouterClientImp::setFroCompressTransOnlyKey] " << *it << " is not in self area" << endl);
                return ET_KEY_AREA_ERR;
            }

            int iRet = g_HashMap.set(*it);
            if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
            {
                if (_recordBinLog)
                    WriteBinLog::setMKOnlyKey(*it, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::setMKOnlyKey(*it, _keyBinlogFile);
            }
        }
    }
    catch (const std::exception &ex)
    {
        TLOG_ERROR("[RouterClientImp::setFroCompressTransOnlyKey] exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::setFroCompressTransOnlyKey] unkown exception, " << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}
tars::Int32 RouterClientImp::setFroCompressTransWithType(const std::string & moduleName, const std::string & mainKey, const std::string & transContent, tars::Char keyType, tars::Bool compress, tars::Bool full, tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("RouterClientImp::setFroCompressTransWithType recv : " << mainKey << "|" << transContent.length() << "|" << compress << "|" << full << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (moduleName != _moduleName)
        {
            TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] " << mainKey << " forbid trans" << endl);
                return ET_FORBID_OPT;
            }
        }

        if (!g_route_table.isMySelf(mainKey))
        {
            TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        if (transContent.empty())
        {
            if (keyType == TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE)
            {
                int iRet = g_HashMap.set(mainKey);
                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    if (_recordBinLog)
                        WriteBinLog::setMKOnlyKey(mainKey, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::setMKOnlyKey(mainKey, _keyBinlogFile);
                }
            }
            else if (keyType == TC_Multi_HashMap_Malloc::MainKey::SET_TYPE)
            {
                int iRet = g_HashMap.addSet(mainKey);
                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    if (_recordBinLog)
                        WriteBinLog::addSet(mainKey, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::addSet(mainKey, _keyBinlogFile);
                }
            }
            else if (keyType == TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE)
            {
                int iRet = g_HashMap.addZSet(mainKey);
                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    if (_recordBinLog)
                        WriteBinLog::addZSet(mainKey, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::addZSet(mainKey, _keyBinlogFile);
                }
            }
            return ET_SUCC;
        }
        else
        {
            bool bParseStrOk;
            vector<string> vTransContent;
            if (compress)
            {
                string sUncompress;
                bool bGzipOk = StringUtil::gzipUncompress(transContent.c_str(), transContent.length(), sUncompress);
                if (!bGzipOk)
                {
                    TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] transContent gzip uncompress error" << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_SYS_ERR;
                }
                bParseStrOk = StringUtil::parseString(sUncompress, vTransContent);
            }
            else
            {
                bParseStrOk = StringUtil::parseString(transContent, vTransContent);
            }

            if (!bParseStrOk)
            {
                TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] transContent string parse error" << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_SYS_ERR;
            }

            vector<MultiHashMap::Value> vs;
            for (size_t i = 0; i < vTransContent.size(); ++i)
            {
                MultiHashMap::Value v;
                TC_PackOut po(vTransContent[i].c_str(), vTransContent[i].length());
                tars::Char dirty;
                int deleteType = 0;
                po >> deleteType;
                po >> dirty;
                po >> v._iExpireTime;
                po >> v._score;
                po >> v._ukey;
                po >> v._value;
                v._iVersion = 0;
                v._mkey = mainKey;
                v._dirty = dirty == 1 ? true : false;
                v._isDelete = (TC_Multi_HashMap_Malloc::DELETETYPE)deleteType;

                vs.push_back(v);
            }

            TC_Multi_HashMap_Malloc::DATATYPE eType;
            if (full)
                eType = TC_Multi_HashMap_Malloc::FULL_DATA;
            else
                eType = TC_Multi_HashMap_Malloc::PART_DATA;

            if (keyType == TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE)
            {
                int iRet = g_HashMap.set(vs, eType, false, false, false, true);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] hashmap set error, ret= " << iRet << ", mkey = " << mainKey << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_KEY_AREA_ERR;
                }

                if (_recordBinLog)
                {
                    WriteBinLog::set(mainKey, vs, full, _binlogFile);
                }
                if (_recordKeyBinLog)
                {
                    WriteBinLog::set(mainKey, vs, full, _keyBinlogFile);
                }
            }
            else if (keyType == TC_Multi_HashMap_Malloc::MainKey::LIST_TYPE)
            {
                int iRet = g_HashMap.pushList(mainKey, vs);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] hashmap set error, ret= " << iRet << ", mkey = " << mainKey << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_KEY_AREA_ERR;
                }

                if (_recordBinLog)
                {
                    WriteBinLog::pushList(mainKey, vs, full, _binlogFile);
                }
                if (_recordKeyBinLog)
                {
                    WriteBinLog::pushList(mainKey, vs, full, _keyBinlogFile);
                }
            }
            else if (keyType == TC_Multi_HashMap_Malloc::MainKey::SET_TYPE)
            {
                int iRet = g_HashMap.addSet(mainKey, vs, eType);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] hashmap set error, ret= " << iRet << ", mkey = " << mainKey << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_KEY_AREA_ERR;
                }

                if (_recordBinLog)
                {
                    WriteBinLog::addSet(mainKey, vs, full, _binlogFile);
                }
                if (_recordKeyBinLog)
                {
                    WriteBinLog::addSet(mainKey, vs, full, _keyBinlogFile);
                }
            }
            else if (keyType == TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE)
            {
                int iRet = g_HashMap.addZSet(mainKey, vs, eType);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] hashmap set error, ret= " << iRet << ", mkey = " << mainKey << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_KEY_AREA_ERR;
                }

                if (_recordBinLog)
                {
                    WriteBinLog::addZSet(mainKey, vs, full, _binlogFile);
                }
                if (_recordKeyBinLog)
                {
                    WriteBinLog::addZSet(mainKey, vs, full, _keyBinlogFile);
                }
            }

        }
    }
    catch (const std::exception &ex)
    {
        TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOG_ERROR("[RouterClientImp::setFroCompressTransWithType] unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 RouterClientImp::cleanFromTransferData(const std::string & moduleName, tars::Int32 fromPageNo, tars::Int32 toPageNo, tars::TarsCurrentPtr current)
{
    if (g_app.gstat()->serverType() != MASTER)
    {
        //SLAVE状态下不做迁移
        TLOG_ERROR("[RouterClientImp::cleanFromTransferData] is not Master" << endl);
        return 1;
    }
    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOG_ERROR("[RouterClientImp::cleanFromTransferData] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
        return 2;
    }

    if (fromPageNo > toPageNo)
    {
        TLOG_ERROR("[RouterClientImp::cleanFromTransferData]range error! fromPageNo:" << fromPageNo << " toPageNo:" << toPageNo << endl);
        return -1;
    }

//    TC_Functor<void, TL::TLMaker<int, int>::Result> cmd(_cleanDataFunctor);
//    TC_Functor<void, TL::TLMaker<int, int>::Result>::wrapper_type fwrapper(cmd, fromPageNo, toPageNo);
    _tpool.exec(std::bind(&CleanDataFunctor::operator(), _cleanDataFunctor, fromPageNo, toPageNo));

    return 0;
}

int RouterClientImp::isReadyForSwitch(const std::string & moduleName, bool &bReady, tars::TarsCurrentPtr current)
{
    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOG_ERROR("RouterClientImp::isReadyForSwitch: moduleName error" << endl);
        return -1;
    }

    bReady = true;

    if (g_app.gstat()->isSlaveCreating() == true)
    {
        bReady = false;
        TLOG_ERROR("RouterClientImp::isReadyForSwitch slaveCreating.. can't switch. " << endl);
    }

    return 0;
}

tars::Int32 RouterClientImp::notifyMasterDowngrade(const std::string & moduleName, tars::TarsCurrentPtr current)
{
    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOG_ERROR("RouterClientImp::notifyMasterDowngrade: moduleName error" << endl);
        return -1;
    }

    return _routerHandle->masterDowngrade();
}

int RouterClientImp::disconnectFromMaster(const std::string & moduleName, tars::TarsCurrentPtr current)
{
    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOG_ERROR("RouterClientImp::disconnectFromMaster: moduleName error" << endl);
        return -1;
    }

    if (g_app.gstat()->serverType() == MASTER)
    {
        string err("RouterClientImp::disconnectFromMaster, server is master. error.");
        TLOG_ERROR(err << endl);
        FDLOG("switch") << err << endl;
        return -1;
    }

    int ret = 0;

    g_app.enableConnectHb(false);
    FDLOG("switch") << "disconnectFromMaster succ." << endl;

    return ret;
}

void RouterClientImp::CleanDataFunctor::init(TC_Config& conf)
{
    _pageSize = TC_Common::strto<unsigned int>(conf["/Main/Router<PageSize>"]);

    _eraseDataInPageFunc.init(conf);
}

void RouterClientImp::CleanDataFunctor::operator()(int pageNoStart, int pageNoEnd)
{
    unsigned int iHash = pageNoStart * _pageSize;
    int count = 6;
    while (count-- > 0)
    {
        //如果还是属于本机的路由范围的，就等待
        if (g_route_table.isMySelfByHash(iHash))
            sleep(10);
        else
            break;
    }

    if (count <= 0)
    {
        TLOG_ERROR("[CleanDataFunctor::operator()] pageno error! " << pageNoStart << " " << pageNoEnd << endl);
        return;
    }

    _eraseDataInPageFunc(pageNoStart, pageNoEnd);
}

bool RouterClientImp::isTransSrc(int pageNo)
{
    ServerInfo srcServer;
    ServerInfo destServer;
    g_route_table.getTrans(pageNo, srcServer, destServer);

    if (srcServer.serverName == (ServerConfig::Application + "." + ServerConfig::ServerName))
        return true;
    return false;
}
