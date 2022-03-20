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
#include <sys/resource.h>
#include "MKCacheServer.h"
#include "MKCacheImp.h"
#include "MKWCacheImp.h"
#include "MKBinLogImp.h"
#include "RouterClientImp.h"
#include "MKBackUpImp.h"
#include "MKControlAckImp.h"

MKCacheServer g_app;

//本地路由表
UnpackTable g_route_table;

MultiHashMap g_HashMap;

void MKCacheToDoFunctor::init(const string& sConf)
{
    _config = sConf;
    TC_Config conf;
    conf.parseFile(sConf);

    string sRDBaccessObj = conf["/Main/DbAccess<ObjName>"];
    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(sRDBaccessObj);

    _binlogFile = conf["/Main/BinLog<LogFile>"];
    _keyBinlogFile = conf["/Main/BinLog<LogFile>"] + "key";

    string sRecordBinLog = conf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = conf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _dbDayLog = conf["/Main/Log<DbDayLog>"];

    _existDB = conf["/Main/DbAccess<DBFlag>"] == "Y" ? true : false;
    _insertAtHead = (conf["/Main/Cache<InsertOrder>"] == "Y" || conf["/Main/Cache<InsertOrder>"] == "y") ? true : false;
    string sUpdateOrder = conf.get("/Main/Cache<UpdateOrder>", "N");
    _updateInOrder = (sUpdateOrder == "Y" || sUpdateOrder == "y") ? true : false;

    uint8_t keyType;
    string sKeyType = conf.get("/Main/Cache<MainKeyType>", "hash");
    if (sKeyType == "hash")
        keyType = 0;
    else if (sKeyType == "set")
        keyType = 1;
    else if (sKeyType == "zset")
        keyType = 2;
    else if (sKeyType == "list")
        keyType = 3;
    else
        assert(false);

    _storeKeyType = (TC_Multi_HashMap_Malloc::MainKey::KEYTYPE)keyType;
    g_app.gstat()->getFieldConfig(_fieldConf);
}

void MKCacheToDoFunctor::reload()
{
    TC_Config conf;
    conf.parseFile(_config);

    string sRDBaccessObj = conf["/Main/DbAccess<ObjName>"];
    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(sRDBaccessObj);

    _binlogFile = conf["/Main/BinLog<LogFile>"];
    _keyBinlogFile = conf["/Main/BinLog<LogFile>"] + "key";
    string sRecordBinLog = conf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = conf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _dbDayLog = conf["/Main/Log<DbDayLog>"];
    _existDB = conf["/Main/DbAccess<DBFlag>"] == "Y" ? true : false;
    _insertAtHead = (conf["/Main/Cache<InsertOrder>"] == "Y" || conf["/Main/Cache<InsertOrder>"] == "y") ? true : false;

    uint8_t keyType;
    string sKeyType = conf.get("/Main/Cache<MainKeyType>", "hash");
    if (sKeyType == "hash")
        keyType = 0;
    else if (sKeyType == "set")
        keyType = 1;
    else if (sKeyType == "zset")
        keyType = 2;
    else if (sKeyType == "list")
        keyType = 3;
    else
        assert(false);

    _storeKeyType = (TC_Multi_HashMap_Malloc::MainKey::KEYTYPE)keyType;
}

void MKCacheToDoFunctor::del(bool bExists, const MKCacheToDoFunctor::DataRecord &data)
{
    if ((g_app.gstat()->serverType() == MASTER) && _existDB)
    {
        int iRet;
        vector<DCache::DbCondition> vtDbCond;
        DbCondition cond;
        cond.fieldName = _fieldConf.sMKeyName;
        cond.op = DCache::EQ;
        cond.value = data._mkey;
        cond.type = ConvertDbType(_fieldConf.mpFieldInfo[_fieldConf.sMKeyName].type);
        vtDbCond.push_back(cond);

        UKBinToDbCondition(data._ukey, vtDbCond);

        string sLogCond = FormatLog::tostr(vtDbCond);
        try
        {
            iRet = _dbaccessPrx->delCond(data._mkey, vtDbCond);
            if (iRet < 0)
            {
                FDLOG(_dbDayLog) << "del|" << data._mkey << "|" << sLogCond << "|failed|" << iRet << endl;
                TLOG_ERROR("MKCacheToDoFunctor::del delString error, key = " << data._mkey << ", iRet = " << iRet << endl);
            }
            else
            {
                FDLOG(_dbDayLog) << "del|" << data._mkey << "|" << sLogCond << "|succ|" << endl;
                return;
            }
        }
        catch (const std::exception & ex)
        {
            FDLOG(_dbDayLog) << "del|" << data._mkey << "|" << sLogCond << "|failed|" << ex.what() << endl;
            TLOG_ERROR("MKCacheToDoFunctor::del delString exception: " << ex.what() << ", key = " << data._mkey << endl);
        }
        if (bExists)
        {
            int iSetRet = g_HashMap.set(data._mkey, data._ukey, data._value, data._iExpireTime, data._iVersion, TC_Multi_HashMap_Malloc::DELETE_AUTO, true, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, false);

            if (iSetRet == TC_Multi_HashMap_Malloc::RT_OK)
            {
                if (_recordBinLog)
                    WriteBinLog::set(data._mkey, data._ukey, data._value, data._iExpireTime, true, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::set(data._mkey, data._ukey, _keyBinlogFile);
            }
        }
        g_app.ppReport(PPReport::SRP_DB_EX, 1);
        throw MKDCacheException("MKCacheToDoFunctor::del delString error");
    }
}

void MKCacheToDoFunctor::sync(const MKCacheToDoFunctor::DataRecord &data)
{
    TLOG_DEBUG("[MKCacheToDoFunctor::sync] data._mkey:" << data._mkey << endl);
    {
        TC_ThreadLock::Lock lock(_lock);
        _syncMKey.insert(data._mkey);
        ++_syncCount;
    }

    try
    {
        if (g_route_table.isTransfering(data._mkey))
        {
            if (!g_route_table.isMySelf(data._mkey) || g_app.gstat()->serverType() != MASTER)
            {
                TC_ThreadLock::Lock lock(_lock);
                _syncMKey.erase(data._mkey);
                return;
            }

            if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == _storeKeyType)
            {
                int iRetCheck = g_HashMap.checkDirty(data._mkey, data._ukey);

                if ((iRetCheck != TC_Multi_HashMap_Malloc::RT_DIRTY_DATA) && (iRetCheck != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                {
                    int iSetRet = g_HashMap.set(data._mkey, data._ukey, data._value, data._iExpireTime, data._iVersion, TC_Multi_HashMap_Malloc::DELETE_AUTO, true, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, false);

                    if (iSetRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        if (_recordBinLog)
                            WriteBinLog::set(data._mkey, data._ukey, data._value, data._iExpireTime, true, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::set(data._mkey, data._ukey, _keyBinlogFile);
                    }
                    else
                    {
                        TLOG_ERROR("MKCacheToDoFunctor::sync reset dirty data, mkey = " << data._mkey << ", error:" << iSetRet << endl);
                        if (iSetRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                        {
                            g_app.ppReport(PPReport::SRP_EX, 1);
                        }
                    }
                }
            }
            else if (TC_Multi_HashMap_Malloc::MainKey::SET_TYPE == _storeKeyType)
            {
                int iRetCheck = g_HashMap.checkDirty(data._mkey, data._value);

                if ((iRetCheck != TC_Multi_HashMap_Malloc::RT_DIRTY_DATA) && (iRetCheck != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                {
                    int iSetRet = g_HashMap.addSet(data._mkey, data._value, data._iExpireTime, data._iVersion, true, TC_Multi_HashMap_Malloc::DELETE_AUTO);

                    if (iSetRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        if (_recordBinLog)
                            WriteBinLog::addSet(data._mkey, data._value, data._iExpireTime, true, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::addSet(data._mkey, data._value, data._iExpireTime, true, _keyBinlogFile);
                    }
                    else
                    {
                        TLOG_ERROR("MKCacheToDoFunctor::sync reset dirty data, mkey = " << data._mkey << ", error:" << iSetRet << endl);
                        if (iSetRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                        {
                            g_app.ppReport(PPReport::SRP_EX, 1);
                        }
                    }
                }
            }
            else if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == _storeKeyType)
            {
                int iRetCheck = g_HashMap.checkDirty(data._mkey, data._value);

                if ((iRetCheck != TC_Multi_HashMap_Malloc::RT_DIRTY_DATA) && (iRetCheck != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                {
                    int iSetRet = g_HashMap.addZSet(data._mkey, data._value, data._score, data._iExpireTime, data._iVersion, true, false, TC_Multi_HashMap_Malloc::DELETE_AUTO);

                    if (iSetRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        if (_recordBinLog)
                            WriteBinLog::addZSet(data._mkey, data._value, data._score, data._iExpireTime, true, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::addZSet(data._mkey, data._value, data._score, data._iExpireTime, true, _keyBinlogFile);
                    }
                    else
                    {
                        TLOG_ERROR("MKCacheToDoFunctor::sync reset dirty data, mkey = " << data._mkey << ", error:" << iSetRet << endl);
                        if (iSetRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                        {
                            g_app.ppReport(PPReport::SRP_EX, 1);
                        }
                    }
                }
            }
        }
        else if (g_app.gstat()->serverType() == MASTER  && _existDB && g_route_table.isMySelf(data._mkey))
        {
            int iRet = 0;
            bool bEx = false;

            map<string, DCache::DbUpdateValue> mpDbValue;
            BinToDbValue(data._mkey, data._ukey, data._value, data._iExpireTime, mpDbValue);
            vector<DCache::DbCondition> vtDbCond;
            DbCondition cond;
            cond.fieldName = _fieldConf.sMKeyName;
            cond.op = DCache::EQ;
            cond.value = data._mkey;
            cond.type = ConvertDbType(_fieldConf.mpFieldInfo[_fieldConf.sMKeyName].type);
            vtDbCond.push_back(cond);

            if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == _storeKeyType)
            {
                UKBinToDbCondition(data._ukey, vtDbCond);
            }
            else if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == _storeKeyType)
            {
                DbUpdateValue value;

                value.op = DCache::SET;
                value.value = TC_Common::tostr(data._score);
                value.type = INT;
                mpDbValue["sDCacheZSetScore"] = value;
            }

            string sLogValue = FormatLog::tostr(mpDbValue);
            try
            {
                iRet = _dbaccessPrx->replace(data._mkey, mpDbValue, vtDbCond);

                if (iRet >= 0)
                {
                    FDLOG(_dbDayLog) << "set|" << data._mkey << "|" << sLogValue << "|succ|" << iRet << endl;
                }
                else
                {
                    FDLOG(_dbDayLog) << "set|" << data._mkey << "|" << sLogValue << "|failed|" << iRet << endl;
                    g_app.ppReport(PPReport::SRP_DB_ERR, 1);
                }
            }
            catch (const TarsException & ex)
            {
                bEx = true;
                FDLOG(_dbDayLog) << "set|" << data._mkey << "|" << sLogValue << "|failed|" << ex.what() << endl;
                TLOG_ERROR("MKCacheToDoFunctor::sync setString exception: " << ex.what() << ", key = " << data._mkey << endl);
                g_app.ppReport(PPReport::SRP_DB_EX, 1);
            }

            if (iRet < 0 || bEx)
            {
                if (iRet == eDbErrorNeedDel)
                {
                    if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == _storeKeyType)
                    {
                        g_HashMap.erase(data._mkey, data._ukey);

                        if (_recordBinLog)
                            WriteBinLog::erase(data._mkey, data._ukey, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::erase(data._mkey, data._ukey, _keyBinlogFile);
                    }
                    else if (TC_Multi_HashMap_Malloc::MainKey::SET_TYPE == _storeKeyType)
                    {
                        g_HashMap.delSetSetBit(data._mkey, data._value, time(NULL));

                        if (_recordBinLog)
                            WriteBinLog::delSet(data._mkey, data._value, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::delSet(data._mkey, data._value, _keyBinlogFile);
                    }
                    else if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == _storeKeyType)
                    {
                        g_HashMap.delZSetSetBit(data._mkey, data._value, time(NULL));

                        if (_recordBinLog)
                            WriteBinLog::delZSet(data._mkey, data._value, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::delZSet(data._mkey, data._value, _keyBinlogFile);
                    }
                    g_HashMap.setFullData(data._mkey, false);

                }
                else
                {
                    if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == _storeKeyType)
                    {
                        int iRetCheck = g_HashMap.checkDirty(data._mkey, data._ukey);

                        if ((iRetCheck != TC_Multi_HashMap_Malloc::RT_DIRTY_DATA) && (iRetCheck != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                        {
                            int iSetRet = g_HashMap.set(data._mkey, data._ukey, data._value, data._iExpireTime, data._iVersion, TC_Multi_HashMap_Malloc::DELETE_AUTO, true, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, false);

                            if (iSetRet == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                if (_recordBinLog)
                                    WriteBinLog::set(data._mkey, data._ukey, data._value, data._iExpireTime, true, _binlogFile);
                                if (_recordKeyBinLog)
                                    WriteBinLog::set(data._mkey, data._ukey, _keyBinlogFile);
                            }
                            else
                            {
                                TLOG_ERROR("MKCacheToDoFunctor::sync reset dirty data, mkey = " << data._mkey << ", error:" << iSetRet << endl);
                                if (iSetRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                                {
                                    g_app.ppReport(PPReport::SRP_EX, 1);
                                }
                            }
                        }
                    }
                    else if (TC_Multi_HashMap_Malloc::MainKey::SET_TYPE == _storeKeyType)
                    {
                        int iRetCheck = g_HashMap.checkDirty(data._mkey, data._value);

                        if ((iRetCheck != TC_Multi_HashMap_Malloc::RT_DIRTY_DATA) && (iRetCheck != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                        {
                            int iSetRet = g_HashMap.addSet(data._mkey, data._value, data._iExpireTime, data._iVersion, true, TC_Multi_HashMap_Malloc::DELETE_AUTO);

                            if (iSetRet == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                if (_recordBinLog)
                                    WriteBinLog::addSet(data._mkey, data._value, data._iExpireTime, true, _binlogFile);
                                if (_recordKeyBinLog)
                                    WriteBinLog::addSet(data._mkey, data._value, data._iExpireTime, true, _keyBinlogFile);
                            }
                            else
                            {
                                TLOG_ERROR("MKCacheToDoFunctor::sync reset dirty data, mkey = " << data._mkey << ", error:" << iSetRet << endl);
                                if (iSetRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                                {
                                    g_app.ppReport(PPReport::SRP_EX, 1);
                                }
                            }
                        }
                    }
                    else if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == _storeKeyType)
                    {
                        int iRetCheck = g_HashMap.checkDirty(data._mkey, data._value);

                        if ((iRetCheck != TC_Multi_HashMap_Malloc::RT_DIRTY_DATA) && (iRetCheck != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                        {
                            int iSetRet = g_HashMap.addZSet(data._mkey, data._value, data._score, data._iExpireTime, data._iVersion, true, false, TC_Multi_HashMap_Malloc::DELETE_AUTO);

                            if (iSetRet == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                if (_recordBinLog)
                                    WriteBinLog::addZSet(data._mkey, data._value, data._score, data._iExpireTime, true, _binlogFile);
                                if (_recordKeyBinLog)
                                    WriteBinLog::addZSet(data._mkey, data._value, data._score, data._iExpireTime, true, _keyBinlogFile);
                            }
                            else
                            {
                                TLOG_ERROR("MKCacheToDoFunctor::sync reset dirty data, mkey = " << data._mkey << ", error:" << iSetRet << endl);
                                if (iSetRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                                {
                                    g_app.ppReport(PPReport::SRP_EX, 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    catch (exception& e)
    {
        TLOG_ERROR("MKCacheToDoFunctor::sync exception: " << e.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("MKCacheToDoFunctor::sync unknown exception" << endl);
    }

    TC_ThreadLock::Lock lock(_lock);
    _syncMKey.erase(data._mkey);
}

void MKCacheToDoFunctor::erase(const MKCacheToDoFunctor::DataRecord &data)
{
    if (data._dirty)
    {
        if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == _storeKeyType)
        {
            int iRet = g_HashMap.set(data._mkey, data._ukey, data._value, data._iExpireTime, data._iVersion, TC_Multi_HashMap_Malloc::DELETE_AUTO, true, TC_Multi_HashMap_Malloc::PART_DATA, _insertAtHead, false);

            if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
            {
                if (_recordBinLog)
                    WriteBinLog::set(data._mkey, data._ukey, data._value, data._iExpireTime, true, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::set(data._mkey, data._ukey, _keyBinlogFile);
                TLOG_DEBUG("MKCacheToDoFunctor::erase reset dirty data, key = " << data._mkey << endl);
            }
        }
        else if (TC_Multi_HashMap_Malloc::MainKey::SET_TYPE == _storeKeyType)
        {
            int iSetRet = g_HashMap.addSet(data._mkey, data._value, data._iExpireTime, data._iVersion, true, TC_Multi_HashMap_Malloc::DELETE_AUTO);

            if (iSetRet == TC_Multi_HashMap_Malloc::RT_OK)
            {
                if (_recordBinLog)
                    WriteBinLog::addSet(data._mkey, data._value, data._iExpireTime, true, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::addSet(data._mkey, data._value, data._iExpireTime, true, _keyBinlogFile);
            }
        }
        else if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == _storeKeyType)
        {
            int iSetRet = g_HashMap.addZSet(data._mkey, data._value, data._score, data._iExpireTime, data._iVersion, true, false, TC_Multi_HashMap_Malloc::DELETE_AUTO);

            if (iSetRet == TC_Multi_HashMap_Malloc::RT_OK)
            {
                if (_recordBinLog)
                    WriteBinLog::addZSet(data._mkey, data._value, data._score, data._iExpireTime, true, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::addZSet(data._mkey, data._value, data._score, data._iExpireTime, true, _keyBinlogFile);
            }
        }
    }
}
void MKCacheToDoFunctor::eraseRadio(const MKCacheToDoFunctor::DataRecord &data)
{
    g_app.ppReport(PPReport::SRP_ERASE_CNT, 1);
    if (data._iExpireTime > TC_TimeProvider::getInstance()->getNow())
    {
        g_app.ppReport(PPReport::SRP_ERASE_UNEXPIRE_CNT, 1);
    }
}
bool MKCacheToDoFunctor::isAllSync()
{
    TC_ThreadLock::Lock lock(_lock);

    CanSync& canSync = g_app.gstat()->getCanSync();
    size_t iCanSyncCount = canSync.getCanSyncCount();

    return (_syncCount == iCanSyncCount);
}

bool MKCacheToDoFunctor::haveSyncKeyIn(unsigned int hashBegin, unsigned int hashEnd)
{
    TC_ThreadLock::Lock lock(_lock);

    set<string>::const_iterator it = _syncMKey.begin(), itEnd = _syncMKey.end();
    while (it != itEnd)
    {
        unsigned int hash = g_route_table.hashKey(*it);
        if (hash >= hashBegin && hash <= hashEnd)
            return true;
        else
            ++it;
    }

    return false;
}

/////////////////////////////////////////////////////////////////

void EraseDataInPageFunctor::init(TC_Config& conf)
{
    _pageSize = TC_Common::strto<unsigned int>(conf["/Main/Router<PageSize>"]);

    _binlogFile = conf["/Main/BinLog<LogFile>"];

    _keyBinlogFile = conf["/Main/BinLog<LogFile>"] + "key";
    string sRecordBinLog = conf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = conf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;
}

void EraseDataInPageFunctor::reload(TC_Config& conf)
{
    string sRecordBinLog = conf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = conf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;
}

void EraseDataInPageFunctor::operator()(int pageNoStart, int pageNoEnd)
{
    uint32_t iMaxPageNo = UnpackTable::__allPageCount - 1;
    if (pageNoStart < 0 || (uint32_t)pageNoStart > iMaxPageNo)
    {
        TLOG_ERROR("[EraseDataInPageFunctor::operator] start page error, " << pageNoStart << " not in [0," << iMaxPageNo << "]" << endl);
        return;
    }

    if (pageNoEnd <0 || (uint32_t)pageNoEnd > iMaxPageNo)
    {
        TLOG_ERROR("[EraseDataInPageFunctor::operator] end page error, " << pageNoEnd << " not in [0," << iMaxPageNo << "]" << endl);
        return;
    }

    if (pageNoEnd < pageNoStart)
    {
        TLOG_ERROR("[EraseDataInPageFunctor::operator] pageNoStart greater pageNoEnd error" << endl);
        return;
    }

    int iPageNo;
    for (iPageNo = pageNoStart; iPageNo <= pageNoEnd; ++iPageNo)
    {
        unsigned int uBegin = (unsigned int)iPageNo*_pageSize;
        unsigned int uEnd = uBegin + _pageSize - 1;
        if (uEnd < uBegin)
        {
            uEnd = UnpackTable::__max;
        }
        int iRet;
        bool bFinish = false;

        size_t i = uBegin;
        while (true)
        {
            KeyMatch k;
            vector<string> vDelMKey;
            vDelMKey.clear();

            if (g_app.gstat()->serverType() != MASTER)
            {
                string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
                TLOG_ERROR("[EraseDataInPageFunctor::operator] " << sServerName << " is not still MASTER, so cannot continue" << endl);
                break;
            }

            //因为hashmap内部把有符号int32的hash值转成size_t来计算hash桶的位置，这里必须保持一致
            uint32_t hash = i;

            if (g_route_table.isMySelfByHash(i))
            {
                TLOG_ERROR("[EraseDataInPageFunctor::operator] hash:" << i << " is still in self area" << endl);
                break;
            }

            k.Set(hash);
            iRet = g_HashMap.eraseHashMByForce(hash, k, vDelMKey);

            for (size_t j = 0; j < vDelMKey.size(); ++j)
            {
                if (_recordBinLog)
                    WriteBinLog::erase(vDelMKey[j], _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::erase(vDelMKey[j], _keyBinlogFile);
            }

            if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
            {
                TLOG_ERROR("[EraseDataInPageFunctor::operator] llhashmap eraseHashMByForce error, hash = " << i << ", iRet = " << iRet << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                break;
            }

            if (i == uEnd)
            {
                bFinish = true;
                break;
            }

            ++i;
        }

        if (!bFinish)
        {
            TLOG_ERROR("[EraseDataInPageFunctor::operator] erase data in page: " << iPageNo << " failed" << endl);
            break;
        }
        TLOG_DEBUG("[EraseDataInPageFunctor::operator] erase data in page: " << iPageNo << " succ" << endl);
    }

    usleep(1000 * 10);

    if (iPageNo > pageNoEnd)
    {
        TLOG_DEBUG("[EraseDataInPageFunctor::operator] erase data in [" << pageNoStart << "," << pageNoEnd << "] succ" << endl);
    }
}

/////////////////////////////////////////////////////////////////
void MKCacheServer::errorAndExit(const string &result)
{
    TLOG_ERROR("[MKCacheServer::initialize] " << result  << endl);
    TARS_NOTIFY_ERROR(result);
    TC_Common::msleep(100);
    exit(-1);
}

void
MKCacheServer::initialize()
{
    //禁止写远程按天日志
    TarsTimeLogger::getInstance()->enableRemote("", false);
    TarsTimeLogger::getInstance()->initFormat("", "%Y%m%d%H");

    _configPrx = Application::getCommunicator()->stringToProxy<DCache::ConfigPrx>("DCache.ConfigServer.ConfigObj");
    addConfig("MKCacheServer.conf");
    _tcConf.parseFile(ServerConfig::BasePath + "MKCacheServer.conf");

    addServant<RouterClientImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".RouterClientObj");
    addServant<MKBackUpImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".BackUpObj");
    addServant<MKCacheImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".CacheObj");
    addServant<MKWCacheImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".WCacheObj");
    addServant<MKBinLogImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".BinLogObj");
    addServant<MKControlAckImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".ControlAckObj");

    TARS_ADD_ADMIN_CMD_NORMAL("help", MKCacheServer::help);
    TARS_ADD_ADMIN_CMD_NORMAL("status", MKCacheServer::showStatus);
    TARS_ADD_ADMIN_CMD_NORMAL("statushash", MKCacheServer::showStatusWithHash);
    TARS_ADD_ADMIN_CMD_NORMAL("syncdata", MKCacheServer::syncData);
    TARS_ADD_ADMIN_CMD_NORMAL("reload", MKCacheServer::reloadConf);
    TARS_ADD_ADMIN_CMD_NORMAL("cachestatus", MKCacheServer::cacheStatus);
    TARS_ADD_ADMIN_CMD_NORMAL("binlogstatus", MKCacheServer::binlogStatus);
    TARS_ADD_ADMIN_CMD_NORMAL("ver", MKCacheServer::showVer);
    TARS_ADD_ADMIN_CMD_NORMAL("erasedatainpage", MKCacheServer::eraseDataInPage);
    TARS_ADD_ADMIN_CMD_NORMAL("erasewrongrouterdata", MKCacheServer::eraseWrongRouterData);
    TARS_ADD_ADMIN_CMD_NORMAL("calculateData", MKCacheServer::calculateData);
    TARS_ADD_ADMIN_CMD_NORMAL("resetCalculatePoint", MKCacheServer::resetCalculatePoint);
    TARS_ADD_ADMIN_CMD_NORMAL("setalldirty", MKCacheServer::setAllDirty);
    TARS_ADD_ADMIN_CMD_NORMAL("clearCache", MKCacheServer::clearCache);
    TARS_ADD_ADMIN_CMD_NORMAL("servertype", MKCacheServer::showServerType);
    TARS_ADD_ADMIN_CMD_NORMAL("key", MKCacheServer::showKey);
    TARS_ADD_ADMIN_CMD_NORMAL("dirtystatic", MKCacheServer::dirtyStatic);


    int iRet = _ppReport.init();
    assert(iRet == 0);

    iRet = _gStat.init();
    assert(iRet == 0);

    //初始化RouterHandle
    _routerHandle = RouterHandle::getInstance();
    _routerHandle->init(_tcConf);

    //初始化路由
    bool bSyncFromRouter = false;
    if (_routerHandle->initRoute(false, bSyncFromRouter) != 0)
    {
        TLOG_ERROR("MKCacheServer::initialize init route failed" << endl);
        assert(false);
    }

    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
    GroupInfo groupinfo;
    g_route_table.getGroup(sServerName, groupinfo);
    _gStat.setGroupName(groupinfo.groupName);
    TLOG_DEBUG("MKCacheServer::initialize groupName:" << groupinfo.groupName << endl);

    // 设置允许产生coredump的大小
    struct rlimit rl;
    getrlimit(RLIMIT_CORE, &rl);
    size_t iOldLimit = rl.rlim_cur;
    string sCoreLimit = _tcConf.get("/Main<CoreSizeLimit>", "");
    if (sCoreLimit == "-1")
    {
        rl.rlim_cur = rl.rlim_max;
    }
    else
    {
        rl.rlim_cur = TC_Common::toSize(sCoreLimit, 0);
    }
    iRet = setrlimit(RLIMIT_CORE, &rl);
    if (iRet != 0)
    {
        TLOG_ERROR("MKCacheServer::initialize set coredump limit fail, errno=" << errno << ", old rlim_cur:"
                     << iOldLimit << ", rlim_max:" << rl.rlim_max << ", try set rlim_cur:" << rl.rlim_cur << endl);
    }

    map<string, int> mTypeLenInDB;
    mTypeLenInDB["byte"] = 1;
    mTypeLenInDB["short"] = 4;
    mTypeLenInDB["int"] = 4;
    mTypeLenInDB["long"] = 8;
    mTypeLenInDB["float"] = 4;
    mTypeLenInDB["double"] = 8;
    mTypeLenInDB["unsigned int"] = 4;
    mTypeLenInDB["unsigned short"] = 4;
    mTypeLenInDB["string"] = 0;
    //初始化字段信息
    FieldConf fieldConfig;
    fieldConfig.sMKeyName = _tcConf["/Main/Record<MKey>"];
    fieldConfig.mpFieldType[fieldConfig.sMKeyName] = 0;
    vector<string> vtUKeyName = TC_Common::sepstr<string>(_tcConf["/Main/Record<UKey>"], "|");
    for (size_t i = 0; i < vtUKeyName.size(); i++)
    {
        string ukeyName = TC_Common::trim(vtUKeyName[i]);
        fieldConfig.vtUKeyName.push_back(ukeyName);
        fieldConfig.mpFieldType[ukeyName] = 1;
    }
    vector<string> vtValueName = TC_Common::sepstr<string>(_tcConf["/Main/Record<VKey>"], "|");
    for (size_t i = 0; i < vtValueName.size(); i++)
    {
        string valueName = TC_Common::trim(vtValueName[i]);
        fieldConfig.vtValueName.push_back(valueName);
        fieldConfig.mpFieldType[valueName] = 2;
    }
    map<string, string> mpFiedInfo = _tcConf.getDomainMap("/Main/Record/Field");
    for (map<string, string>::iterator it = mpFiedInfo.begin(); it != mpFiedInfo.end(); it++)
    {
        vector<string> vtInfo = TC_Common::sepstr<string>(it->second, "|", true);
        struct FieldInfo info;
        info.tag = TC_Common::strto<uint32_t>(TC_Common::trim(vtInfo[0]));
        info.type = TC_Common::trim(vtInfo[1]);
        info.bRequire = (TC_Common::trim(vtInfo[2]) == "require") ? true : false;
        info.defValue = TC_Common::trim(vtInfo[3]);
        info.lengthInDB = mTypeLenInDB[info.type];
        if (vtInfo.size() >= 5)
        {
            info.lengthInDB = TC_Common::strto<int>(vtInfo[4]);
        }
        fieldConfig.mpFieldInfo[it->first] = info;
    }
    _gStat.setFieldConfig(fieldConfig);

    //生成binlog文件
    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    bool _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    bool _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    string path = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + ServerConfig::Application + "." + ServerConfig::ServerName + "_";

    char str[128];
    struct tm ptr;
    time_t nowSec;
    string normalBinlogPath, keyBinlogPath;

    memset(str, '\0', 128);
    nowSec = time(NULL);
    ptr = *(localtime(&nowSec));
    strftime(str, 80, "%Y%m%d%H", &ptr);

    if (_recordBinLog)
    {
        normalBinlogPath = path + _tcConf["/Main/BinLog<LogFile>"] + "_" + str + ".log";
        iRet = WriteBinLog::createBinLogFile(normalBinlogPath, false);
        assert(iRet == 0);
        TLOG_DEBUG("[MKCacheServer::initialize] open file succ! " << normalBinlogPath << endl);
    }

    if (_recordKeyBinLog)
    {
        keyBinlogPath = path + _tcConf["/Main/BinLog<LogFile>"] + "key_" + str + ".log";
        iRet = WriteBinLog::createBinLogFile(keyBinlogPath, true);
        assert(iRet == 0);
        TLOG_DEBUG("[MKCacheServer::initialize] open file succ! " << keyBinlogPath << endl);
    }

    //启动定时生成binlog文件线程
    _createBinlogFileThread.createThread();

    //hashmap初始化
    MKHash *pHash = new MKHash();

    bool bCreate = false;
    string sSemKeyFile = TC_File::simplifyDirectory(ServerConfig::DataPath + "/SemKey.dat");
    string sSemKey = "";
    if (access(sSemKeyFile.c_str(), F_OK) != 0)
    {
        if (errno != ENOENT)
        {
            errorAndExit("access file: " + sSemKeyFile + " error");
        }

        bCreate = true;
    }
    else
    {
        ifstream ifs(sSemKeyFile.c_str(), ios::in | ios::binary);
        if (!ifs)
        {
            string result = "open file: " + sSemKeyFile + " error";
            errorAndExit(result);
            g_app.ppReport(PPReport::SRP_EX, 1);
        }
        else
        {
            if (!getline(ifs, sSemKey))
            {
                if (ifs.eof())
                {
                    TLOG_ERROR("[MKCacheServer::initialize] file: " << sSemKeyFile << " is empty" << endl);
                }
                else
                {
                    TLOG_ERROR("[MKCacheServer::initialize] read file: " << sSemKeyFile << " failed" << endl);
                }
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            else
            {
                TLOG_DEBUG("MKCacheServer::initialize get semkey from file: " << sSemKeyFile << " succ, semkey = " << sSemKey << endl);
            }

            ifs.close();
        }
    }

    if(sSemKey.empty())
    {
        key_t key;
        key = TC_Common::strto<key_t>(_tcConf.get("/Main/Cache<ShmKey>", "0"));
        if (key == 0)
        {
            key = ftok(ServerConfig::BasePath.c_str(), 'D');
        }

        _shmKey = TC_Common::tostr(key);

        // if (!sSemKey.empty() && sSemKey != _shmKey)
        // {
        //     errorAndExit("semkey from file != ftok/config, (" + sSemKey + " != " + TC_Common::tostr(key) + ")");
        // }
    }
    else
    {
        _shmKey = sSemKey;
    }

    key_t key = TC_Common::strto<key_t>(_shmKey);

    TARS_NOTIFY_ERROR("shmkey:" + _shmKey);
    TLOG_DEBUG("MKCacheServer::initialize semkey = " << key << endl);

    size_t n = TC_Common::toSize(_tcConf["/Main/Cache<ShmSize>"], 0);
    TLOG_DEBUG("MKCacheServer::initialize shmSize=" << n << endl);

    if (bCreate)
    {
        if (shmget(key, 0, 0) != -1 || errno != ENOENT)
        {
            errorAndExit("shmkey: " + TC_Common::tostr(key) + " has been used, may be a conflict key");
        }
    }
    else
    {
        if (sSemKey.empty()) //文件中读取的内容为空
        {
            int iShmid = shmget(key, 0, 0);
            if (iShmid == -1)
            {
                errorAndExit("file: " + sSemKeyFile + " exist and has no content, try shmget " + TC_Common::tostr(key) + " error");
            }
            else
            {
                struct shmid_ds buf;
                if (shmctl(iShmid, IPC_STAT, &buf) != 0)
                {
                    errorAndExit("shmctl "+ TC_Common::tostr(key) + " failed");
                }
                else if (buf.shm_nattch != 0)
                {
                    errorAndExit("number of current attach to shm: " + TC_Common::tostr(key) + " is not zero error");
                }
            }
        }
        else
        {
            int iShmid = shmget(key, 0, 0);
            if (iShmid == -1)
            {
                if (MASTER == getServerType()) //主机不允许启动，应该触发主备切换
                {
                    errorAndExit("file: " + sSemKeyFile + " exist, but shmget " + TC_Common::tostr(key) + " error: " + TC_Exception::getSystemError()  + ",master server assert.");
                }
                else
                {
                    bool hasDb = _tcConf["/Main/DbAccess<DBFlag>"] == "Y" ? true : false;
                    if (hasDb)
                    {
                        errorAndExit("file: " + sSemKeyFile + " exist, but shmget " + TC_Common::tostr(key) + " error: " + TC_Exception::getSystemError()  + ",allowed boot up because there is backend DB.");
                    }
                    else
                    {
                        errorAndExit("file: " + sSemKeyFile + " exist, but shmget " + TC_Common::tostr(key) + " error: " + TC_Exception::getSystemError()  + ",has no backend DB.");
                    }
                }
            }
        }
    }

    if (bCreate || sSemKey == "")
    {
        ofstream ofs(sSemKeyFile.c_str(), ios::out | ios::binary);
        if (!ofs)
        {
            TLOG_ERROR("[MKCacheServer::initialize] open file: " << sSemKeyFile << " failed, semkey can not be noted" << endl);
            if (bCreate)
            {
                assert(false);
            }
        }
        else
        {
            string sKey = TC_Common::tostr(key);
            ofs.write(sKey.c_str(), sKey.length());
            ofs.close();
        }
    }

    _todoFunctor.init(ServerConfig::BasePath + "MKCacheServer.conf");

    //默认2个jmem
    unsigned int iShmNum = TC_Common::strto<unsigned int>(_tcConf.get("/Main/Cache<JmemNum>", "2"));
    g_HashMap.init(iShmNum);
    g_HashMap.initMainKeySize(0);
    g_HashMap.initHashRatio(TC_Common::strto<float>(_tcConf["/Main/Cache<HashRadio>"]));
    g_HashMap.initMainKeyHashRatio(TC_Common::strto<float>(_tcConf["/Main/Cache<MKHashRadio>"]));
    g_HashMap.initDataSize(TC_Common::strto<size_t>(_tcConf["/Main/Cache<AvgDataSize>"]));
    g_HashMap.initLock(key, iShmNum, -1);


    uint8_t keyType;
    string sKeyType = _tcConf.get("/Main/Cache<MainKeyType>", "hash");
    if (sKeyType == "hash")
        keyType = 0;
    else if (sKeyType == "set")
        keyType = 1;
    else if (sKeyType == "zset")
        keyType = 2;
    else if (sKeyType == "list")
        keyType = 3;
    else
        assert(false);

    g_HashMap.initStore(key, n, keyType);

    g_HashMap.setSyncTime(TC_Common::strto<unsigned int>(_tcConf["/Main/Cache<SyncTime>"]));

    g_HashMap.setToDoFunctor(&_todoFunctor);

//    TC_Multi_HashMap_Malloc::hash_functor cmd_mk(pHash, static_cast<TpMem>(&MKHash::HashMK));
//    TC_Multi_HashMap_Malloc::hash_functor cmd_mkuk(pHash, static_cast<TpMem>(&MKHash::HashMKUK));
    g_HashMap.setHashFunctorM(std::bind(&MKHash::HashMK, pHash, std::placeholders::_1));
    g_HashMap.setHashFunctor(std::bind(&MKHash::HashMKUK, pHash, std::placeholders::_1));
    g_HashMap.setAutoErase(false);

    _gStat.setSlaveCreating(false);
    string sSlaveCreatingDataFile = ServerConfig::DataPath + "/SlaveCreating.dat";
    if (access(sSlaveCreatingDataFile.c_str(), F_OK) != 0)
    {
        if (errno != ENOENT)
        {
            TLOG_ERROR("[MKCacheServer::initialize] access file: " << sSlaveCreatingDataFile << " error, errno = " << errno << endl);
            assert(false);
        }
    }
    else
    {
        ostringstream os;
        if (getServerType() == MASTER)
        {
            os << "server can not changed from SLAVE to MASTER when in creating data status" << endl;
            TARS_NOTIFY_ERROR(os.str());
            TLOG_ERROR("[MKCacheServer::initialize] " << os.str() << endl);
            assert(false);
        }
        _gStat.setSlaveCreating(true);

        os << "slave still in creating data status";
        TARS_NOTIFY_ERROR(os.str());
        TLOG_DEBUG(os.str() << endl);
    }

    string sDumpingDataFile = ServerConfig::DataPath + "/Dumping.dat";
    if (access(sDumpingDataFile.c_str(), F_OK) != 0)
    {
        if (errno != ENOENT)
        {
            TLOG_ERROR("[MKCacheServer::initialize] access file: " << sDumpingDataFile << " error, errno = " << errno << endl);
            assert(false);
        }
    }
    else
    {
        string sFile = ServerConfig::DataPath + "/Dumping.dat";
        remove(sFile.c_str());

        ostringstream os;
        os << "Dumping not finished";
        TARS_NOTIFY_ERROR(os.str());
        TLOG_ERROR(os.str() << endl);
    }

    //确定server启动模式，是master还是slave
    enum ServerType role = getServerType();
    _gStat.setServerType(role);

    //主机上报重启信息
    if (role == MASTER)
    {
        _routerHandle->serviceRestartReport();
    }

    for (size_t i = 0; i < 50; i++)
    {
        _gStat.resetHitCnt(i);
    }

    //创建获取Binlog日志线程
    //创建获取备份源最后记录binlog时间的线程
    _syncBinlogThread.init(ServerConfig::BasePath + "MKCacheServer.conf");
    _syncBinlogThread.createThread();
    _binlogTimeThread.init(ServerConfig::BasePath + "MKCacheServer.conf");
    _binlogTimeThread.createThread();

    //创建定时作业线程，负责读取路由表和回写脏数据等等
    _timerThread.init(ServerConfig::BasePath + "MKCacheServer.conf");
    _timerThread.createThread();

    _eraseThread.init(ServerConfig::BasePath + "MKCacheServer.conf");
    _eraseThread.createThread();

    _syncThread.init(ServerConfig::BasePath + "MKCacheServer.conf");
    _syncThread.createThread();

    _heartBeatThread.createThread();
    string sMasterConAddr = RouterHandle::getInstance()->getConnectHbAddr();
    if (!sMasterConAddr.empty())
        _heartBeatThread.enableConHb(true);

    string StartDeleteThread = _tcConf.get("/Main/Cache<StartDeleteThread>", "Y");
    if ((StartDeleteThread == "Y") || (StartDeleteThread == "y"))
    {
        _deleteThread.init(ServerConfig::BasePath + "MKCacheServer.conf");
        _deleteThread.createThread();
    }

    if ((_tcConf["/Main/Cache<StartExpireThread>"] == "Y" || _tcConf["/Main/Cache<StartExpireThread>"] == "y"))
    {
        _gStat.enableExpire(true);
        _expireThread.init(ServerConfig::BasePath + "MKCacheServer.conf");
        _expireThread.createThread();
    }

    _syncAllThread.init(ServerConfig::BasePath + "MKCacheServer.conf");

    _eraseDataInPageFunc.init(_tcConf);

    // 创建线程池，暂时设定线程数为1
    _tpool.init(1);
    _tpool.start();

    //启动统计线程
    _dirtyStatisticThread.createThread();

    string sCaculateEnable = _tcConf.get("/Main/Cache<coldDataCalEnable>", "Y");
    _clodDataCntingThread._enable = (sCaculateEnable == "Y" || sCaculateEnable == "y") ? true : false;
    _clodDataCntingThread._resetCycleDays = TC_Common::strto<int>(_tcConf.get("/Main/Cache<coldDataCalCycle>", "7"));

    _clodDataCntingThread.createThread();

    TLOG_DEBUG("MKCacheServer::initialize Succ" << endl);
}

bool MKCacheServer::help(const string& command, const string& params, string& result)
{
    result = "status: 显示Server的状态信息\n";
    result += "reload: 重新加载配置\n";
    result += "syncdata: 回写所有脏数据\n";
    result += "cachestatus: 内存使用情况\n";
    result += "binlogstatus: binlog同步情况\n";
    result += "ver: 显示Cache版本\n";
    result += "erasedatainpage: 删除指定页范围内的数据\n";
    result += "erasewrongrouterdata:删除不属于此cacheserver的页\n";
    result += "servertype: 显示服务状态，是主还是备\n";
    result += "key: 显示信号量或者共享内存key\n";
    result += "recreatedata: 备机重建数据\n";
    result += "dirtystatic：统计不可淘汰的脏数据\n";
    result += "calculateData: 统计未被访问数据大小\n";
    result += "setalldirty：将cache内全部数据设置成脏数据\n";
    result += "clearcache：清空cache，危险操作，请三思\n";

    return true;
}

bool MKCacheServer::dirtyStatic(const string& command, const string& params, string& result)
{
    TLOG_DEBUG("MKCacheServer::dirtyStatic begin" << endl);

    _dirtyStatisticNowThread.createThread();
    result = "dirtyStatic had send";

    TLOG_DEBUG("MKCacheServer::dirtyStatic succ" << endl);
    return true;
}

bool MKCacheServer::showStatus(const string& command, const string& params, string& result)
{
    TLOG_DEBUG("MKCacheServer::showStatus begin" << endl);

    string sServerType = _gStat.serverType() == MASTER ? "Master" : "Slave";
    result = string("Server Type: ") + sServerType + "\n";
    result += "--------------------------------------------\n";

    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    if (_gStat.serverType() != MASTER)
    {
        ServerInfo server;
        g_route_table.getBakSource(sServerName, server);
        result += "Master Addr: \n";
        result += formatServerInfo(server);
    }

    result += "Binlog SyncTime: " + TC_Common::tm2str(g_app.gstat()->getBinlogTimeSync()) + "\n";
    result += "Binlog LastTime: " + TC_Common::tm2str(g_app.gstat()->getBinlogTimeLast()) + "\n";


    result += "--------------------------------------------\n";
    result += "Cache Info: \n";

    vector<TC_Multi_HashMap_Malloc::tagMapHead> vHead;


    g_HashMap.getMapHead(vHead);
    vector<string> vDesc;
    string desc;
    g_HashMap.desc(vDesc);
    for (size_t i = 0; i < vDesc.size(); ++i)
    {
        desc += vDesc[i];
        desc += "\n";
    }

    result += "数据可用的内存大小: " + TC_Common::tostr(g_HashMap.getDataMemSize()) + "\n";

    result += "不可淘汰数据个数：" + TC_Common::tostr(g_app.gstat()->dirtyNum()) + "\n";
    result += "Delete标记数据个数：" + TC_Common::tostr(g_app.gstat()->delNum()) + "\n";

    result += formatCacheHead(vHead);

    result += desc;

    result += "--------------------------------------------\n";
    result += "Router Info: \n";

    result += g_route_table.toString();

    TLOG_DEBUG("MKCacheServer::showStatus Succ" << endl);
    return true;
}

bool MKCacheServer::showStatusWithHash(const string& command, const string& params, string& result)
{
    TLOG_DEBUG("MKCacheServer::showStatus begin" << endl);

    string sServerType = _gStat.serverType() == MASTER ? "Master" : "Slave";
    result = string("Server Type: ") + sServerType + "\n";
    result += "--------------------------------------------\n";

    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    if (_gStat.serverType() != MASTER)
    {
        ServerInfo server;
        g_route_table.getBakSource(sServerName, server);
        result += "Master Addr: \n";
        result += formatServerInfo(server);
    }

    result += "Binlog SyncTime: " + TC_Common::tm2str(g_app.gstat()->getBinlogTimeSync()) + "\n";
    result += "Binlog LastTime: " + TC_Common::tm2str(g_app.gstat()->getBinlogTimeLast()) + "\n";


    result += "--------------------------------------------\n";
    result += "Cache Info: \n";

    vector<TC_Multi_HashMap_Malloc::tagMapHead> vHead;


    g_HashMap.getMapHead(vHead);
    vector<string> vDesc;
    string desc;
    g_HashMap.descWithHash(vDesc);
    for (size_t i = 0; i < vDesc.size(); ++i)
    {
        desc += vDesc[i];
        desc += "\n";
    }

    result += "数据可用的内存大小: " + TC_Common::tostr(g_HashMap.getDataMemSize()) + "\n";
    result += formatCacheHead(vHead);


    result += desc;

    result += "--------------------------------------------\n";
    result += "Router Info: \n";

    result += g_route_table.toString();

    TLOG_DEBUG("MKCacheServer::showStatus Succ" << endl);
    return true;
}

bool MKCacheServer::syncData(const string& command, const string& params, string& result)
{
    TLOG_DEBUG("MKCacheServer::syncData begin" << endl);

    _syncAllThread.createThread();
    result = "syncdata had send";

    TLOG_DEBUG("MKCacheServer::syncData succ" << endl);
    return true;
}

bool MKCacheServer::reloadConf(const string& command, const string& params, string& result)
{
    addConfig("MKCacheServer.conf");
    _tcConf.parseFile(ServerConfig::BasePath + "MKCacheServer.conf");

    _todoFunctor.reload();
    g_HashMap.setSyncTime(TC_Common::strto<unsigned int>(_tcConf["/Main/Cache<SyncTime>"]));

    _syncBinlogThread.reload();
    _timerThread.reload();
    _syncThread.reload();
    _syncAllThread.reload();
    _eraseDataInPageFunc.reload(_tcConf);
    _binlogTimeThread.reload();
    if ((_tcConf["/Main/Cache<StartExpireThread>"] == "Y" || _tcConf["/Main/Cache<StartExpireThread>"] == "y"))
    {
        _expireThread.reload();
    }

    string sCaculateEnable = _tcConf.get("/Main/Cache<coldDataCalEnable>", "Y");
    _clodDataCntingThread._enable = (sCaculateEnable == "Y" || sCaculateEnable == "y") ? true : false;
    _clodDataCntingThread._resetCycleDays = TC_Common::strto<int>(_tcConf.get("/Main/Cache<coldDataCalCycle>", "7"));

    result = "reload config finish";
    TLOG_ERROR("MKCacheServer::reloadConf Succ" << endl);
    return true;
}

bool MKCacheServer::cacheStatus(const string& command, const string& params, string& result)
{
    return true;
}

bool MKCacheServer::binlogStatus(const string& command, const string& params, string& result)
{
    result = TC_Common::tostr(g_app.gstat()->getBinlogTimeSync()) + "|" + TC_Common::tostr(g_app.gstat()->getBinlogTimeLast());
    return true;
}

bool MKCacheServer::showVer(const string& command, const string& params, string& result)
{
    result = CACHE_VER;
    return true;
}

bool MKCacheServer::showServerType(const string& command, const string& params, string& result)
{
    string sServerType = _gStat.serverType() == MASTER ? "Master" : "Slave";
    result = string("Server Type: ") + sServerType;

    return true;
}

bool MKCacheServer::showKey(const string& command, const string& params, string& result)
{
    result = _shmKey;
    return true;
}

bool MKCacheServer::setAllDirty(const string& command, const string& params, string& result)
{
    ostringstream os;
    int ret = TC_Multi_HashMap_Malloc::RT_OK;
    MultiHashMap::hash_iterator it = g_HashMap.hashBegin();
    size_t index = 0;
    while (it != g_HashMap.hashEnd())
    {
        int ret = it->setDirty();
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            os << "failed to set hashmap[" << index << "] data dirty, ret=" << ret;
            break;
        }
        ++it;
        ++index;
    }

    if (ret != TC_Multi_HashMap_Malloc::RT_OK)
    {
        result = os.str();
        TLOG_ERROR("[MKCacheServer::setAllDirty] " << os.str() << endl);
    }
    else
    {
        result = "set all data dirty successfully";
    }

    return true;
}

bool MKCacheServer::clearCache(const string& command, const string& params, string& result)
{
    // 鉴权

    bool certified = true;

    if (certified)
    {
        g_HashMap.clear();
    }

    result = "clear cache successfully";

    return true;
}

bool MKCacheServer::isAllInSlaveCreatingStatus()
{
    return (_syncBinlogThread.isInSlaveCreatingStatus() && _binlogTimeThread.isInSlaveCreatingStatus() && _timerThread.isInSlaveCreatingStatus());
}

string MKCacheServer::formatServerInfo(ServerInfo &serverInfo)
{
    string s;

    s = "serverName :" + serverInfo.serverName + "\n";
    s += "ip :" + serverInfo.ip + "\n";
    s += "BinLogServant :" + serverInfo.BinLogServant + "\n";
    s += "CacheServant :" + serverInfo.CacheServant + "\n";
    s += "RouteClientServant :" + serverInfo.RouteClientServant + "\n";

    return s;
}

string MKCacheServer::formatCacheHead(vector<TC_Multi_HashMap_Malloc::tagMapHead>& vHead)
{
    string s;

    size_t totalMem = 0;
    size_t totalElementCount = 0;
    size_t totalDirtyCount = 0;
    size_t totalUsedDataMem = 0;
    size_t totalDataUsedChunk = 0;
    size_t totalGetCount = 0;
    size_t totalHitCount = 0;

    for (size_t i = 0; i < vHead.size(); ++i)
    {
        totalMem += vHead[i]._iMemSize;
        totalElementCount += vHead[i]._iElementCount;
        totalDirtyCount += vHead[i]._iDirtyCount;
        totalUsedDataMem += vHead[i]._iUsedDataMem;
        totalDataUsedChunk += vHead[i]._iDataUsedChunk;
        totalGetCount += vHead[i]._iGetCount;
        totalHitCount += vHead[i]._iHitCount;
    }


    s += "===========================totalStart===========================\n";
    s += "内存大小：" + TC_Common::tostr(totalMem) + "\n";
    s += "总元素个数：" + TC_Common::tostr(totalElementCount) + "\n";
    s += "脏数据个数：" + TC_Common::tostr(totalDirtyCount) + "\n";
    s += "已经使用的数据内存大小：" + TC_Common::tostr(totalUsedDataMem) + "\n";
    s += "数据已经使用的内存块：" + TC_Common::tostr(totalDataUsedChunk) + "\n";
    s += "get次数：" + TC_Common::tostr(totalGetCount) + "\n";
    s += "命中次数：" + TC_Common::tostr(totalHitCount) + "\n";
    s += "===========================totalEnd===========================\n\n";





    for (size_t i = 0; i < vHead.size(); ++i)
    {
        s += "jmemNum=" + TC_Common::tostr(i) + "\n";
        s += "内存大小：" + TC_Common::tostr(vHead[i]._iMemSize) + "\n";
        s += "总元素个数：" + TC_Common::tostr(vHead[i]._iElementCount) + "\n";
        s += "脏数据个数：" + TC_Common::tostr(vHead[i]._iDirtyCount) + "\n";
        s += "每次删除个数：" + TC_Common::tostr(vHead[i]._iEraseCount) + "\n";
        s += "回写时间：" + TC_Common::tostr(vHead[i]._iSyncTime) + "\n";
        s += "已经使用的数据内存大小：" + TC_Common::tostr(vHead[i]._iUsedDataMem) + "\n";
        s += "数据已经使用的内存块：" + TC_Common::tostr(vHead[i]._iDataUsedChunk) + "\n";
        s += "get次数：" + TC_Common::tostr(vHead[i]._iGetCount) + "\n";
        s += "命中次数：" + TC_Common::tostr(vHead[i]._iHitCount) + "\n";
        s += "==============================================================\n";
    }
    return s;
}



bool MKCacheServer::eraseDataInPage(const string& command, const string& params, string& result)
{
    TLOG_DEBUG("MKCacheServer::eraseDataInPage, " << params << endl);

    if (_gStat.serverType() != MASTER)
    {
        string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
        result = sServerName + " is not MASTER, so cannot execute this command";
        return true;
    }

    if (params.empty() || TC_Common::sepstr<string>(params, " ").size() != 2)
    {
        result = "usage: erasedatainpage 起始页 结束页\n";
        result += "for example: 要删除属于1-100页范围内的数据\n";
        result += "erasedatainpage 1 100";
        return true;
    }

    vector<string> vtParams = TC_Common::sepstr<string>(params, " ");
    if (!TC_Common::isdigit(vtParams[0]) || !TC_Common::isdigit(vtParams[1]))
    {
        result = "params error, usage: erasedatainpage 起始页 结束页\n";
        result += "for example: 要删除属于1-100页范围内的数据\n";
        result += "erasedatainpage 1 100";
        return true;
    }
    int iPageNoStart, iPageNoEnd;
    iPageNoStart = TC_Common::strto<int>(vtParams[0]);
    iPageNoEnd = TC_Common::strto<int>(vtParams[1]);

//    TC_Functor<void, TL::TLMaker<int, int>::Result> cmd(_eraseDataInPageFunc);
//    TC_Functor<void, TL::TLMaker<int, int>::Result>::wrapper_type fwrapper(cmd, iPageNoStart, iPageNoEnd);
    _tpool.exec(std::bind(&EraseDataInPageFunctor::operator(), _eraseDataInPageFunc, iPageNoStart, iPageNoEnd));

    result = "begin erase data in page [" + vtParams[0] + "," + vtParams[1] + "]\n";
    result += "please wait and check result later, bye";
    return true;
}

//在确保此命令运行完成后，不得再进行新的迁移
bool MKCacheServer::eraseWrongRouterData(const string& command, const string& params, string& result)
{
    TLOG_DEBUG("MKCacheServer::eraseWrongRouterData start." << endl);
    if (_gStat.serverType() != MASTER)
    {
        string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
        result = sServerName + " is not MASTER, so cannot execute this command";
        return true;
    }

    ostringstream os;
    os << "begin erase data in page ";
    const PackTable& packTable = g_route_table.getPackTable();
    size_t nRangeCount = packTable.recordList.size();

    TLOG_DEBUG("MKCacheServer::eraseWrongRouterData nRangeCount=" << nRangeCount << endl);
    for (size_t i = 0; i < nRangeCount; ++i)
    {
        TLOG_DEBUG("MKCacheServer::eraseWrongRouterData groupName=" << packTable.recordList[i].groupName << "|groupName=" << _gStat.groupName() << endl);
        if (packTable.recordList[i].groupName != _gStat.groupName())
        {
            int iPageNoStart = packTable.recordList[i].fromPageNo;
            int iPageNoEnd = packTable.recordList[i].toPageNo;
            TLOG_DEBUG("MKCacheServer::eraseWrongRouterData iPageNoStart=" << iPageNoStart << "|iPageNoEnd=" << iPageNoEnd << endl);
            os << "[" << iPageNoStart << "," << iPageNoEnd << "],";

//            TC_Functor<void, TL::TLMaker<int, int>::Result> cmd(_eraseDataInPageFunc);
//            TC_Functor<void, TL::TLMaker<int, int>::Result>::wrapper_type fwrapper(cmd, iPageNoStart, iPageNoEnd);
//            _tpool.exec(fwrapper);

	        _tpool.exec(std::bind(&EraseDataInPageFunctor::operator(), _eraseDataInPageFunc, iPageNoStart, iPageNoEnd));

        }
    }
    os << " please wait and check result later, bye.";
    result = os.str();
    TLOG_DEBUG("MKCacheServer::eraseWrongRouterData finish." << endl);
    return true;
}

bool MKCacheServer::calculateData(const string& command, const string& params, string& result)
{
    _clodDataCntingThread.runImmediately(true);

    result = "calculate is running!";

    return true;
}

bool MKCacheServer::resetCalculatePoint(const string& command, const string& params, string& result)
{
    g_HashMap.resetCalculateData();
    _clodDataCntingThread._startTime = time(NULL);
    _clodDataCntingThread._reportCnt = 0;

    result = "reset calculate point success!";
    FDLOG("unread") << result << endl;

    return true;
}

void CalculateThread::createThread()
{
    if (_isRunning)
        return;

    //创建线程
    pthread_t thread;

    if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
    {
        TLOG_ERROR("[CalculateThread::createThread] Create SyncAllThread fail" << endl);
    }
}

void* CalculateThread::Run(void* arg)
{
    while (true)
    {
        CalculateThread *t = (CalculateThread*)arg;

        if (!t->_enable)
        {
            sleep(300);
            continue;
        }

        if (!t->_runNow)
        {
            //当前时间
            time_t tNow = time(NULL);
            struct tm tm_now;
            localtime_r(&tNow, &tm_now);

            char now_str[31] = "\0";
            strftime(now_str, 31, "%d,%H,%M", &tm_now);
            string sNow(now_str);
            //TLOG_DEBUG("CalculateThread::Run, sNow:" << sNow << endl);
            vector<string> vNow = TC_Common::sepstr<string>(sNow, ",");
            if (vNow.size() != 3)
            {
                TLOG_ERROR("CalculateThread::Run, vNow size error, sNow" << sNow << endl);
                FDLOG("unread") << "CalculateThread::Run, vNow size error, sNow" << sNow << endl;
                sleep(300);
                continue;
            }

            //上一次统计的时间
            struct tm tm_start;
            localtime_r(&(t->_startTime), &tm_start);

            char start_str[31] = "\0";
            strftime(start_str, 31, "%d,%H,%M", &tm_start);
            string sStart(start_str);
            //TLOG_DEBUG("CalculateThread::Run, sStart:" << sStart << endl);
            vector<string> vStart = TC_Common::sepstr<string>(sStart, ",");
            if (vStart.size() != 3)
            {
                TLOG_ERROR("CalculateThread::Run, vStart size error, sStart" << sStart << endl);
                FDLOG("unread") << "CalculateThread::Run, vStart size error, sStart" << sStart << endl;
                sleep(300);
                continue;
            }

            // same day
            if (vNow[0] == vStart[0])
            {
                sleep(300);
                continue;
            }
            else
            {
                //different day, do statistic at 2~3 clock
                int randMinute = rand() % 60;
                int minute = 60 * (TC_Common::strto<Int32>(vNow[1])) + TC_Common::strto<Int32>(vNow[2]);
                if (minute < (2 * 60 + randMinute))
                {
                    sleep(300);
                    continue;
                }
            }

        }

        t->_isRunning = true;

        string result;
        uint64_t count = 0;
        //默认是2个内存块
        for (int i = 0; i < 2; i++)
        {
            bool isEnd = false;
            while (!isEnd)
            {
                uint32_t tmp;
                g_HashMap.calculateData(i, tmp, isEnd);
                count += tmp;
            }
        }

        size_t totalMemSize = g_HashMap.getTotalMemSize();
        size_t totalDataMemSize = g_HashMap.getDataMemSize();
        size_t usedDataMemSize = g_HashMap.getTotalUsedMemSize();
        size_t totalElementCnt = g_HashMap.getTotalElementCount();

        size_t unreadRatio = 100;

        //根据内存大小统计
        //if (totalDataMemSize > 0)
        //{
        //	unreadRatio = (totalDataMemSize - usedDataMemSize + count) * 100 / totalDataMemSize;
        //}

        //根据记录条数进行统计，1 - ( usedDataMemSize * (1 - count / totalElementCnt) ) / totalDataMemSize
        if ((totalDataMemSize > 0) && (totalElementCnt > 0))
        {
            unreadRatio = (totalDataMemSize - usedDataMemSize + (usedDataMemSize*count / totalElementCnt)) * 100 / totalDataMemSize;
        }
        g_app.ppReport(PPReport::SRP_COLD_RATIO, unreadRatio);
        //result = "unread:" + TC_Common::tostr(count) + " usedDataMem:" + TC_Common::tostr(usedDataMemSize) + " totalDataMem:" + TC_Common::tostr(totalDataMemSize) +
        //	" totalMem:" + TC_Common::tostr(totalMemSize) + " unuseMemPercent:" + TC_Common::tostr(unreadRatio) + "% startTime:" + TC_Common::tostr(t->_startTime);
        result = "unread:" + TC_Common::tostr(count) + " totalElementCnt:" + TC_Common::tostr(totalElementCnt) +
                 " usedDataMem:" + TC_Common::tostr(usedDataMemSize) + " totalDataMem:" + TC_Common::tostr(totalDataMemSize) +
                 " totalMem:" + TC_Common::tostr(totalMemSize) + " unuseMemPercent:" + TC_Common::tostr(unreadRatio) +
                 "% startTime:" + TC_Common::tostr(t->_startTime);
        FDLOG("unread") << result << endl;
        TLOG_ERROR(result << endl);

        t->_startTime = time(NULL);

        t->_isRunning = false;

        t->_runNow = false;

        t->_reportCnt++;
        if (t->_reportCnt >= t->_resetCycleDays)
        {
            g_HashMap.resetCalculateData();
            t->_reportCnt = 0;
            TLOG_DEBUG("CalculateThread::Run, cycle reset!" << endl);
            FDLOG("unread") << "CalculateThread::Run, cycle reset!" << endl;
        }
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////
bool MKCacheServer::addConfig(const string &filename)
{
    string result;

    if (pullConfig(filename, result))
    {
        TLOG_DEBUG("MKCacheServer::addConfig pullConfig result = " << result << endl);
        TarsRemoteNotify::getInstance()->report(result);

        return true;
    }

    TLOG_ERROR("[MKCacheServer::addConfig] pullConfig result = " << result << endl);
    TarsRemoteNotify::getInstance()->report(result);

    return true;
}


bool MKCacheServer::pullConfig(const string & fileName, string &result, bool bAppConfigOnly, int _iMaxBakNum)
{
    int retryTimes = 1;
    do {
        try
        {
            string sFullFileName = ServerConfig::BasePath + "/" + fileName;

            string newFile = getRemoteFile(fileName, bAppConfigOnly);

            if (newFile.empty() || access(newFile.c_str(), R_OK) != 0)//拉取不到配置中心的配置文件
            {
                if (access(sFullFileName.c_str(), R_OK) == 0) //获取本地配置成功，返回成功，但需要告警一下。
                {
                    result = "[fail] get remote config:" + fileName + "fail,use the local config.";

                    return true;
                }
                throw runtime_error("access file error:" + newFile);
            }

            if (TC_File::load2str(newFile) != TC_File::load2str(sFullFileName))
            {
                for (int i = _iMaxBakNum - 1; i >= 1; --i)
                {
                    if (access(index2file(sFullFileName, i).c_str(), F_OK) == 0)
                    {
                        localRename(index2file(sFullFileName, i), index2file(sFullFileName, i + 1));
                    }
                }

                if (access(sFullFileName.c_str(), F_OK) == 0)
                {
                    localRename(sFullFileName, index2file(sFullFileName, 1));
                }
            }

            localRename(newFile, sFullFileName);

            assert(!access(sFullFileName.c_str(), R_OK));

            result = "[succ] get remote config:" + fileName;

            return true;
        }
        catch (std::exception& e)
        {
            TLOG_ERROR("[MKCacheServer::pullConfig] exception: " << e.what() << endl);
            result = "[fail] get remote config '" + fileName + "' error:" + string(e.what());
        }
        catch (...)
        {
            TLOG_ERROR("[MKCacheServer::pullConfig] unknown exception" << endl);
            result = "[fail] get remote config '" + fileName + "' unknown error";
        }
    } while (retryTimes--);

    return false;
}

string MKCacheServer::getRemoteFile(const string &fileName, bool bAppConfigOnly)
{
    if (_configPrx)
    {
        string stream;
        int ret = _configPrx->readConfig(ServerConfig::Application, (bAppConfigOnly ? "" : ServerConfig::ServerName), stream);
        if (ret != 0)
        {
            throw runtime_error("remote config file is empty:" + fileName);
        }
        if (stream.empty())
        {
            throw runtime_error("my remote config file is empty:" + fileName);
        }

        string newFile = ServerConfig::BasePath + "/" + fileName + "." + TC_Common::tostr(time(NULL));

        std::ofstream out(newFile.c_str());

        string result;
        if (out)
        {
            out << stream;//如果硬盘满了，是否能写入成功需要进行判断。
            if (out.bad())
            {
                result = "[fail] copy stream to disk error.";
                TarsRemoteNotify::getInstance()->report(result);
                return "";
            }
            else
            {
                return newFile;
            }
        }
    }
    return "";
}

string MKCacheServer::index2file(const string & sFullFileName, int index)
{
    return   sFullFileName + "." + TC_Common::tostr(index) + ".bak";
}

void MKCacheServer::localRename(const string& oldFile, const string& newFile)
{
    if (::rename(oldFile.c_str(), newFile.c_str()) != 0)
    {
        throw runtime_error("rename file error:" + oldFile + "->" + newFile);
    }
}

enum ServerType MKCacheServer::getServerType()
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

bool MKCacheServer::haveDeleteKeyIn(unsigned int begin, unsigned int end)
{
    TC_ThreadLock::Lock lock(_deleteLock);

    set<string>::const_iterator it = _deleteMKey.begin(), itEnd = _deleteMKey.end();
    while (it != itEnd)
    {
        unsigned int hash = g_route_table.hashKey(*it);
        if (hash >= begin && hash <= end)
            return true;
        else
            ++it;
    }

    return false;
}

int MKCacheServer::insertDeleteKey(string &sKey)
{
    TC_ThreadLock::Lock lock(_deleteLock);
    _deleteMKey.insert(sKey);

    return 0;
}

int MKCacheServer::earseDeleteKey(string &sKey)
{
    TC_ThreadLock::Lock lock(_deleteLock);
    _deleteMKey.erase(sKey);

    return 0;
}

void MKCacheServer::ppReport(enum PPReport::PropertyType type, int value)
{
    _ppReport.report(type, value);
}

bool MKCacheServer::isAllSync()
{
    return _todoFunctor.isAllSync();
}

bool MKCacheServer::haveSyncKeyIn(unsigned int begin, unsigned int end)
{
    return _todoFunctor.haveSyncKeyIn(begin, end);
}

GlobalStat* MKCacheServer::gstat()
{
    return &_gStat;
}

int MKCacheServer::getSyncTime()
{
    return _syncThread.getSyncTime();
}

DumpThread* MKCacheServer::dumpThread()
{
    return &_dumpThread;
}

SlaveCreateThread* MKCacheServer::slaveCreateThread()
{
    return &_slaveCreateThread;
}

/////////////////////////////////////////////////////////////////
void
MKCacheServer::destroyApp()
{
    //destroy application here:
    //...
    _syncBinlogThread.stop();
    _binlogTimeThread.stop();
    _timerThread.stop();
    _eraseThread.stop();
    _syncThread.stop();
    _syncAllThread.stop();
    _dumpThread.stop();
    _slaveCreateThread.stop();
    _deleteThread.stop();
    _heartBeatThread.stop();

    if ((_tcConf["/Main/Cache<StartExpireThread>"] == "Y" || _tcConf["/Main/Cache<StartExpireThread>"] == "y"))
    {
        _expireThread.stop();
    }

    while (_syncBinlogThread.isRuning() || _binlogTimeThread.isRuning() || _timerThread.isRuning() || _syncAllThread.isRuning() || _syncThread.isRuning() || _eraseThread.isRuning() || _slaveCreateThread.isRuning() || _dumpThread.isRuning())
    {
        usleep(10000);
    }
    if ((_tcConf["/Main/Cache<StartExpireThread>"] == "Y" || _tcConf["/Main/Cache<StartExpireThread>"] == "y"))
    {
        while (_expireThread.isRuning())
        {
            usleep(10000);
        }
    }
    TLOG_ERROR("MKCacheServer::destroyApp Succ" << endl);
}
/////////////////////////////////////////////////////////////////
int
main(int argc, char* argv[])
{
    try
    {
        g_app.main(argc, argv);
        g_app.waitForShutdown();
    }
    catch (std::exception& e)
    {
        cerr << "std::exception:" << e.what() << std::endl;
    }
    catch (...)
    {
        cerr << "unknown exception." << std::endl;
    }
    return -1;
}
/////////////////////////////////////////////////////////////////
