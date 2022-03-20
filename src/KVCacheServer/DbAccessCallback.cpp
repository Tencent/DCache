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
#include "DbAccessCallback.h"

CBQueue g_cbQueue;

int DbAccessCBParam::AddUpdate(UpdateCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(_mutex);
    if (_status != DbAccessCBParam::INIT)
        return 1;
    _updateParams.push_back(param);

    return 0;
}

DbAccessCBParamPtr CBQueue::getCBParamPtr(const string &mainKey, bool &isCreate)
{
    TC_LockT<TC_ThreadMutex> lock(_mutex);
    DbAccessCBParamPtr p;
    map<string, DbAccessCBParamPtr>::iterator it = _keyParamQueue.find(mainKey);
    if (it == _keyParamQueue.end())
    {
        p = new DbAccessCBParam(mainKey);
        p->setStatus(DbAccessCBParam::INIT);
        _keyParamQueue[mainKey] = p;
        isCreate = true;
    }
    else
    {
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        if (tNow - it->second->getCreateTime() > _timeout)
        {
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            TLOG_ERROR("CBQueue::getCBParamPtr: " << mainKey << " timeout, erase it" << endl);
            _keyParamQueue.erase(mainKey);
            p = new DbAccessCBParam(mainKey);
            p->setStatus(DbAccessCBParam::INIT);
            _keyParamQueue[mainKey] = p;
            isCreate = true;
        }
        else
        {
            p = it->second;
            isCreate = false;
        }
    }
    return p;
}

void CBQueue::erase(const string &mainKey)
{
    TC_LockT<TC_ThreadMutex> lock(_mutex);
    _keyParamQueue.erase(mainKey);
}

void DbAccessCallback::callback_get(tars::Int32 ret, const std::string &value, tars::Int32 iExpireTime)
{
    //TLOG_DEBUG("DbAccessCallback::callback_get return iret = " << ret << " , key = " << _key << endl);
    try
    {
        if (ret == eDbSucc)
        {
            if (_batchReq)
            {
                if (_pParam->bEnd)
                    return;
                if (g_app.gstat()->isExpireEnabled() && (iExpireTime != 0) && (iExpireTime < TC_TimeProvider::getInstance()->getNow()))
                {
                    _pParam->addValue(_key, "", VALUE_NO_DATA, 1, 0);
                }
                else
                {
                    _pParam->addValue(_key, value, VALUE_SUCC, 2, iExpireTime);
                }
                if ((--_pParam->count) <= 0)
                {
                    GetKVBatchRsp rsp;
                    rsp.values = _pParam->vtValue;
                    Cache::async_response_getKVBatch(_current, ET_SUCC, rsp);
                    _pParam->bEnd = true;
                }
            }
            else
            {
                if (_type == "")
                {
                    if (g_app.gstat()->isExpireEnabled() && (iExpireTime != 0) && (iExpireTime < TC_TimeProvider::getInstance()->getNow()))
                    {
                        GetKVRsp rsp;
                        Cache::async_response_getKV(_current, ET_NO_DATA, rsp);
                        TLOG_DEBUG("DbAccessCallback::callback_get data expired, return ET_NO_DATA , key = " << _key << endl);
                    }
                    else
                    {
                        GetKVRsp rsp;
                        rsp.value = value;
                        rsp.ver = 2;
                        rsp.expireTime = iExpireTime;
                        Cache::async_response_getKV(_current, ET_SUCC, rsp);
                    }
                }
                else if (_type == "add")
                {
                    //FIXME, 数据已过期应该insert
                    WCache::async_response_insertKV(_current, ET_DATA_EXIST);
                }
                else if (_type == "updateEx")
                {
                    TC_LockT<TC_ThreadMutex> lock(_cbParam->getMutex());

                    vector<DbAccessCBParam::UpdateCBParam> &vUpdate = _cbParam->getUpdate();

                    int iRet = g_sHashMap.set(_key, value, false, 0, 1); //iExpireTime

                    if ((iRet == TC_HashMapMalloc::RT_OK) || (iRet == TC_HashMapMalloc::RT_DATA_VER_MISMATCH))
                    {
                        if (iRet == TC_HashMapMalloc::RT_OK)
                        {
                            if (_isRecordBinLog)
                            {
                                TBinLogEncode logEncode;
                                CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, false, _key, value, 0) + "\n", _binlogFile); //iExpireTime
                                if (g_app.gstat()->serverType() == MASTER)
                                    g_app.gstat()->setBinlogTime(0, TNOW);
                            }
                            if (_isRecordKeyBinLog)
                            {
                                TBinLogEncode logEncode;
                                CacheServer::WriteToFile(logEncode.EncodeSetKey(_key) + "\n", _binlogFile + "key");
                                if (g_app.gstat()->serverType() == MASTER)
                                    g_app.gstat()->setBinlogTime(0, TNOW);
                            }
                        }

                        for (unsigned int i = 0; i < vUpdate.size(); i++)
                        {
                            int ret;
                            string retValue;
                            if (g_app.gstat()->isExpireEnabled())
                            {
                                ret = g_sHashMap.update(vUpdate[i].key, vUpdate[i].value, vUpdate[i].option, vUpdate[i].dirty, vUpdate[i].expireTime, true, TC_TimeProvider::getInstance()->getNow(), retValue);
                            }
                            else
                            {
                                ret = g_sHashMap.update(vUpdate[i].key, vUpdate[i].value, vUpdate[i].option, vUpdate[i].dirty, vUpdate[i].expireTime, false, -1, retValue);
                            }

                            if (ret == TC_HashMapMalloc::RT_OK)
                            {
                                UpdateKVRsp rsp;
                                rsp.retValue = retValue;
                                WCache::async_response_updateKV(vUpdate[i].current, ET_SUCC, rsp);
                                if (_isRecordBinLog)
                                {
                                    TBinLogEncode logEncode;
                                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, vUpdate[i].dirty, _key, retValue, vUpdate[i].expireTime) + "\n", _binlogFile);
                                    if (g_app.gstat()->serverType() == MASTER)
                                        g_app.gstat()->setBinlogTime(0, TNOW);
                                }
                                if (_isRecordKeyBinLog)
                                {
                                    TBinLogEncode logEncode;
                                    CacheServer::WriteToFile(logEncode.EncodeSetKey(_key) + "\n", _binlogFile + "key");
                                    if (g_app.gstat()->serverType() == MASTER)
                                        g_app.gstat()->setBinlogTime(0, TNOW);
                                }
                            }
                            else if ((ret == TC_HashMapMalloc::RT_NO_DATA) || (ret == TC_HashMapMalloc::RT_ONLY_KEY))
                            {
                                UpdateKVRsp rsp;
                                WCache::async_response_updateKV(vUpdate[i].current, ET_NO_DATA, rsp);
                            }
                            else if (ret == TC_HashMapMalloc::RT_DATATYPE_ERR || ret == TC_HashMapMalloc::RT_DECODE_ERR)
                            {
                                UpdateKVRsp rsp;
                                WCache::async_response_updateKV(vUpdate[i].current, ET_PARAM_OP_ERR, rsp);
                            }
                            else
                            {
                                UpdateKVRsp rsp;
                                WCache::async_response_updateKV(vUpdate[i].current, ET_SYS_ERR, rsp);
                            }
                        }
                    }
                    else
                    {
                        for (unsigned int i = 0; i < vUpdate.size(); i++)
                        {
                            UpdateKVRsp rsp;
                            if (iRet == TC_HashMapMalloc::RT_NO_MEMORY)
                            {
                                WCache::async_response_updateKV(vUpdate[i].current, ET_MEM_FULL, rsp);
                            }
                            else
                            {
                                WCache::async_response_updateKV(vUpdate[i].current, ET_SYS_ERR, rsp);
                            }
                        }

                        TLOG_ERROR("DbAccessCallback::callback_getString hashmap.set(" << _key << ") error:" << iRet << endl);
                        if (iRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                        {
                            g_app.ppReport(PPReport::SRP_EX, 1);
                        }
                    }

                    _cbParam->setStatus(DbAccessCBParam::FINISH);
                    lock.release();
                    g_cbQueue.erase(_cbParam->getMainKey());
                }
            }

            try
            {
                if (_type != "updateEx")
                {
                    if (!g_app.gstat()->isExpireEnabled() || (g_app.gstat()->isExpireEnabled() && (iExpireTime == 0 || iExpireTime > TC_TimeProvider::getInstance()->getNow())))
                    {
                        int iRet = g_sHashMap.set(_key, value, false, iExpireTime, 1); //@2016.3.23
                        if (iRet == TC_HashMapMalloc::RT_OK)
                        {
                            if (_isRecordBinLog)
                            {
                                TBinLogEncode logEncode;
                                CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, false, _key, value, iExpireTime) + "\n", _binlogFile);
                                if (g_app.gstat()->serverType() == MASTER)
                                    g_app.gstat()->setBinlogTime(0, TNOW);
                            }
                            if (_isRecordKeyBinLog)
                            {
                                TBinLogEncode logEncode;
                                CacheServer::WriteToFile(logEncode.EncodeSetKey(_key) + "\n", _binlogFile + "key");
                                if (g_app.gstat()->serverType() == MASTER)
                                    g_app.gstat()->setBinlogTime(0, TNOW);
                            }
                        }
                        else
                        {
                            TLOG_ERROR("DbAccessCallback::callback_get hashmap.set(" << _key << ") error:" << iRet << endl);
                            if (iRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                            {
                                g_app.ppReport(PPReport::SRP_EX, 1);
                            }
                        }
                    }
                }
            }
            catch (const TarsException & ex)
            {
                TLOG_ERROR("DbAccessCallback::callback_get exception: " << ex.what() << ", key = " << _key << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
        else if (ret == eDbRecordNotExist)
        {
            if (_batchReq)
            {
                if (_pParam->bEnd)
                    return;
                _pParam->addValue(_key, "", VALUE_NO_DATA, 1, 0);
                if ((--_pParam->count) <= 0)
                {
                    GetKVBatchRsp rsp;
                    rsp.values = _pParam->vtValue;
                    Cache::async_response_getKVBatch(_current, ET_SUCC, rsp);
                    _pParam->bEnd = true;
                }
            }
            else
            {
                if (_type == "")
                {
                    GetKVRsp rsp;
                    Cache::async_response_getKV(_current, ET_NO_DATA, rsp);
                }
                else if (_type == "add")
                {
                    int iRet = g_sHashMap.set(_key, _value, _dirty, _expireTimeSecond);
                    if (iRet != TC_HashMapMalloc::RT_OK)
                    {
                        TLOG_ERROR("CacheImp::addStringKey callbacke" << _key << ") error:" << iRet << endl);
                        if (iRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                        {
                            if (iRet == TC_HashMapMalloc::RT_NO_MEMORY)
                            {
                                WCache::async_response_insertKV(_current, ET_MEM_FULL);
                            }
                            else
                            {
                                g_app.ppReport(PPReport::SRP_EX, 1);
                                WCache::async_response_insertKV(_current, ET_SYS_ERR);
                            }
                        }
                        else
                            WCache::async_response_insertKV(_current, ET_DATA_VER_MISMATCH);
                    }
                    else
                    {
                        WCache::async_response_insertKV(_current, ET_SUCC);
                        if (_isRecordBinLog)
                        {
                            TBinLogEncode logEncode;
                            CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, true, _key, _value, _expireTimeSecond) + "\n", _binlogFile);
                            if (g_app.gstat()->serverType() == MASTER)
                                g_app.gstat()->setBinlogTime(0, TNOW);
                        }
                        if (_isRecordKeyBinLog)
                        {
                            TBinLogEncode logEncode;
                            CacheServer::WriteToFile(logEncode.EncodeSetKey(_key) + "\n", _binlogFile + "key");
                            if (g_app.gstat()->serverType() == MASTER)
                                g_app.gstat()->setBinlogTime(0, TNOW);
                        }
                    }
                }
                else if (_type == "updateEx")
                {
                    TC_LockT<TC_ThreadMutex> lock(_cbParam->getMutex());

                    vector<DbAccessCBParam::UpdateCBParam> &vUpdate = _cbParam->getUpdate();
                    for (unsigned int i = 0; i < vUpdate.size(); i++)
                    {
                        UpdateKVRsp rsp;
                        WCache::async_response_updateKV(vUpdate[i].current, ET_NO_DATA, rsp);
                    }

                    _cbParam->setStatus(DbAccessCBParam::FINISH);
                    lock.release();
                    g_cbQueue.erase(_cbParam->getMainKey());
                }
            }

            if (_saveOnlyKey)
            {
                try
                {
                    if (_type == "add")
                        return;

                    if (_key.size() >= 1000)
                        return;
                    int iRet = g_sHashMap.set(_key, 1);
                    if (iRet != TC_HashMapMalloc::RT_OK)
                    {
                        TLOG_ERROR("DbAccessCallback::callback_get hashmap.set(" << _key << ") error:" << iRet << endl);
                        if (iRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                        {
                            g_app.ppReport(PPReport::SRP_EX, 1);
                        }
                    }
                    else
                    {
                        //写Binlog
                        if (_isRecordBinLog)
                        {
                            TBinLogEncode logEncode;
                            string value;
                            CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET_ONLYKEY, true, _key, value) + "\n", _binlogFile);
                            if (g_app.gstat()->serverType() == MASTER)
                                g_app.gstat()->setBinlogTime(0, TNOW);
                        }
                        if (_isRecordKeyBinLog)
                        {
                            TBinLogEncode logEncode;
                            string value;
                            CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET_ONLYKEY, true, _key, value) + "\n", _binlogFile + "key");
                            if (g_app.gstat()->serverType() == MASTER)
                                g_app.gstat()->setBinlogTime(0, TNOW);
                        }
                    }
                }
                catch (const TarsException & ex)
                {
                    TLOG_ERROR("DbAccessCallback::callback_get exception: " << ex.what() << ", key = " << _key << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
            }
        }
        else
        {
            if (_batchReq)
            {
                if (_pParam->bEnd)
                    return;

                GetKVBatchRsp rsp;
                Cache::async_response_getKVBatch(_current, ET_DB_ERR, rsp);
                _pParam->bEnd = true;
            }
            else
            {
                if (_type == "")
                {
                    GetKVRsp rsp;
                    Cache::async_response_getKV(_current, ET_DB_ERR, rsp);
                }
                else if (_type == "add")
                    WCache::async_response_insertKV(_current, ET_DB_ERR);
                else if (_type == "updateEx")
                {
                    TC_LockT<TC_ThreadMutex> lock(_cbParam->getMutex());

                    vector<DbAccessCBParam::UpdateCBParam> &vUpdate = _cbParam->getUpdate();
                    for (unsigned int i = 0; i < vUpdate.size(); i++)
                    {
                        UpdateKVRsp rsp;
                        WCache::async_response_updateKV(vUpdate[i].current, ET_DB_ERR, rsp);
                    }

                    _cbParam->setStatus(DbAccessCBParam::FINISH);
                    lock.release();
                    g_cbQueue.erase(_cbParam->getMainKey());
                }
            }
            TLOG_ERROR("DbAccessCallback::callback_getString error: ret = " << ret << endl);
            g_app.ppReport(PPReport::SRP_DB_ERR, 1);
        }
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("DbAccessCallback::callback_getString exception: " << ex.what() << " , key = " << _key << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }
    catch (...)
    {
        TLOG_ERROR("DbAccessCallback::callback_getString unkown_exception, key = " << _key << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }
}


void DbAccessCallback::callback_get_exception(tars::Int32 ret)
{
    TLOG_ERROR("DbAccessCallback::callback_get_exception ret =" << ret << ", key = " << _key << endl);
    g_app.ppReport(PPReport::SRP_DB_EX, 1);

    try
    {
        if (_batchReq)
        {
            if (_pParam->bEnd)
                return;

            GetKVBatchRsp rsp;
            Cache::async_response_getKVBatch(_current, ET_DB_ERR, rsp);
            _pParam->bEnd = true;
        }
        else
        {
            if (_type == "")
            {
                GetKVRsp rsp;
                Cache::async_response_getKV(_current, ET_DB_ERR, rsp);
            }
            else if (_type == "add")
                WCache::async_response_insertKV(_current, ET_DB_ERR);
            else if (_type == "updateEx")
            {
                TC_LockT<TC_ThreadMutex> lock(_cbParam->getMutex());

                vector<DbAccessCBParam::UpdateCBParam> &vUpdate = _cbParam->getUpdate();
                for (unsigned int i = 0; i < vUpdate.size(); i++)
                {
                    UpdateKVRsp rsp;
                    WCache::async_response_updateKV(vUpdate[i].current, ET_DB_ERR, rsp);
                }
                _cbParam->setStatus(DbAccessCBParam::FINISH);
                lock.release();
                g_cbQueue.erase(_cbParam->getMainKey());
            }
        }
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("DbAccessCallback::callback_get_exception exception: " << ex.what() << " , key = " << _key << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }
    catch (...)
    {
        TLOG_ERROR("DbAccessCallback::callback_get_exception unkown_exception, key = " << _key << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }
}


void DbAccessCallback::callback_del(tars::Int32 ret)
{
    TLOG_DEBUG("DbAccessCallback::callback_del return iret = " << ret << " , key = " << _key << endl);
    try
    {
        if (ret == eDbSucc || ret == eDbRecordNotExist)
        {
            if (_batchReq)
            {
                TLOG_DEBUG("DbAccessCallback::callback_del batch" << endl);
                if (_pDelParam->bEnd)
                    return;

                if (_isRecordBinLog)
                {
                    TBinLogEncode logEncode;
                    string value;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_DEL, true, _key, value) + "\n", _binlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
                if (_isRecordKeyBinLog)
                {
                    TBinLogEncode logEncode;
                    string value;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_DEL, true, _key, value) + "\n", _binlogFile + "key");
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }

                _pDelParam->addValue(_key, DEL_SUCC);

                if ((--_pDelParam->count) <= 0)
                {
                    WCache::async_response_delKVBatch(_current, ET_SUCC, _pDelParam->result);
                    _pDelParam->bEnd = true;
                }
            }
            else
            {
                WCache::async_response_delKV(_current, ET_SUCC);
                //写Binlog
                if (_isRecordBinLog)
                {
                    TBinLogEncode logEncode;
                    string value;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_DEL, true, _key, value) + "\n", _binlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
                if (_isRecordKeyBinLog)
                {
                    TBinLogEncode logEncode;
                    string value;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_DEL, true, _key, value) + "\n", _binlogFile + "key");
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
            }
        }
        else
        {
            if (_batchReq)
            {
                if (_pDelParam->bEnd)
                    return;

                _pDelParam->addValue(_key, DEL_ERROR);

                if ((--_pDelParam->count) <= 0)
                {
                    WCache::async_response_delKVBatch(_current, ET_SUCC, _pDelParam->result);
                    _pDelParam->bEnd = true;
                }
            }
            else
            {
                WCache::async_response_delKV(_current, ET_DB_ERR);
            }

            TLOG_ERROR("DbAccessCallback::callback_del error: ret = " << ret << endl);
            g_app.ppReport(PPReport::SRP_DB_ERR, 1);
        }
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("DbAccessCallback::callback_del exception: " << ex.what() << " , key = " << _key << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }
    catch (...)
    {
        TLOG_ERROR("DbAccessCallback::callback_del unkown_exception, key = " << _key << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }
}

void DbAccessCallback::callback_del_exception(tars::Int32 ret)
{
    TLOG_ERROR("DbAccessCallback::callback_del_exception ret =" << ret << ", key = " << _key << endl);
    g_app.ppReport(PPReport::SRP_DB_EX, 1);

    try
    {
        if (_batchReq)
        {
            if (_pDelParam->bEnd)
                return;

            DCache::RemoveKVBatchRsp result;

            WCache::async_response_delKVBatch(_current, ET_DB_ERR, result);
            _pDelParam->bEnd = true;
        }
        else
        {
            WCache::async_response_delKV(_current, ET_DB_ERR);
        }
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("DbAccessCallback::callback_del_exception exception: " << ex.what() << " , key = " << _key << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }
    catch (...)
    {
        TLOG_ERROR("DbAccessCallback::callback_del_exception unkown_exception, key = " << _key << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }
}


