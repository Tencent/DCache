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
#include "WCacheImp.h"
#include "RouterHandle.h"

using namespace std;

extern CBQueue g_cbQueue;

inline static bool IsDigit(const string &key)
{
    string::const_iterator iter = key.begin();

    if (key.empty())
    {
        return false;
    }

    size_t length = key.size();

    size_t count = 0;

    //是否找到小数点

    bool bFindNode = false;

    bool bFirst = true;

    while (iter != key.end())
    {
        if (bFirst)
        {
            if (!::isdigit(*iter))
            {
                if (*iter != 45)
                {
                    return false;
                }
                else if (length == 1)
                {
                    return false;
                }

            }
        }
        else
        {
            if (!::isdigit(*iter))
            {
                if (*iter == 46)
                {
                    if ((!bFindNode) && (count != (length - 1)))
                    {
                        bFindNode = true;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
        }
        ++iter;
        count++;
        bFirst = false;
    }
    return true;
}


//////////////////////////////////////////////////////
void WCacheImp::initialize()
{
    //initialize servant here:
    //...
    _config = ServerConfig::BasePath + "CacheServer.conf";
    _tcConf.parseFile(_config);

    _moduleName = _tcConf["/Main<ModuleName>"];
    _binlogFile = _tcConf["/Main/BinLog<LogFile>"];
    _keyBinlogFile = _tcConf["/Main/BinLog<LogFile>"] + "key";

    _maxKeyLengthInDB = TC_Common::strto<size_t>(_tcConf.get("/Main/Cache<MaxKeyLengthInDB>", "767"));

    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _isRecordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _isRecordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _existDB = (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y") ? true : false;

    string sRDBaccessObj = _tcConf["/Main/DbAccess<ObjName>"];
    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(sRDBaccessObj);

    _saveOnlyKey = (_tcConf["/Main/Cache<SaveOnlyKey>"] == "Y" || _tcConf["/Main/Cache<SaveOnlyKey>"] == "y") ? true : false;
    _readDB = (_tcConf["/Main/DbAccess<ReadDbFlag>"] == "Y" || _tcConf["/Main/DbAccess<ReadDbFlag>"] == "y") ? true : false;

    _hitIndex = g_app.gstat()->genHitIndex();

    TARS_ADD_ADMIN_CMD_NORMAL("reload", WCacheImp::reloadConf);

    TLOG_DEBUG("WCacheImp::initialize Succ, _hitIndex:" << _hitIndex << endl);
}

//////////////////////////////////////////////////////
void WCacheImp::destroy()
{
    //destroy servant here:
    //...
}

bool WCacheImp::reloadConf(const string& command, const string& params, string& result)
{
    _tcConf.parseFile(_config);
    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _isRecordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _isRecordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _saveOnlyKey = (_tcConf["/Main/Cache<SaveOnlyKey>"] == "Y" || _tcConf["/Main/Cache<SaveOnlyKey>"] == "y") ? true : false;
    _readDB = (_tcConf["/Main/DbAccess<ReadDbFlag>"] == "Y" || _tcConf["/Main/DbAccess<ReadDbFlag>"] == "y") ? true : false;
    TLOG_DEBUG("WCacheImp::reloadConf Succ" << endl);
    result = "SUCC";

    return true;
}


tars::Int32 WCacheImp::setKV(const DCache::SetKVReq &req, tars::TarsCurrentPtr current)
{
    TLOG_DEBUG("[WCacheImp::" << __FUNCTION__ << "]|" << req.moduleName << "|"
              << req.data.keyItem << "|" << (int)req.data.version << "|" << req.data.dirty
              << "|" << req.data.expireTimeSecond << endl);
    int iRet = setStringKey(req.moduleName, req.data.keyItem, req.data.value,
                            req.data.version, req.data.dirty, req.data.expireTimeSecond, current);
    return iRet;
}

tars::Int32 WCacheImp::setKVBatch(const DCache::SetKVBatchReq &req, DCache::SetKVBatchRsp &rsp, tars::TarsCurrentPtr current)
{
    if (g_app.gstat()->serverType() != MASTER)
    {
        //SLAVE状态下不提供接口服务
        TLOG_ERROR("WCacheImp::setKVBatch: ServerType is not Master" << endl);
        return ET_SERVER_TYPE_ERR;
    }
    if (req.moduleName != _moduleName)
    {
        //返回模块错误
        TLOG_ERROR("WCacheImp::setKVBatch: moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    const vector<SSetKeyValue>& keyValue = req.data;
    map<std::string, tars::Int32>& keyResult = rsp.keyResult;

    g_app.ppReport(PPReport::SRP_SET_CNT, keyValue.size());
    vector<SSetKeyValue>::const_iterator vIt = keyValue.begin();
    for (int iIndex = 0; vIt != keyValue.end(); ++vIt, ++iIndex)
    {
        bool dirty = vIt->dirty;
        const string& keyItem = vIt->keyItem;

        try
        {
            size_t iKeyLength = keyItem.length();
            if (iKeyLength > _maxKeyLengthInDB)
            {
                TLOG_ERROR("WCacheImp::setKVBatch: " << keyItem << " keylength:" << iKeyLength << "limit:" << _maxKeyLengthInDB << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                keyResult[keyItem] = SET_ERROR;
                continue;
            }
            //迁移时禁止set，由于迁移时允许set的逻辑有漏洞，在解决漏洞之前禁止Set
            if (g_route_table.isTransfering(keyItem))
            {
                int iPageNo = g_route_table.getPageNo(keyItem);
                if (isTransSrc(iPageNo))
                {
                    TLOG_ERROR("WCacheImp::setKVBatch: " << keyItem << " forbid set" << endl);
                    keyResult[keyItem] = SET_ERROR;
                    continue;
                }
            }

            //检查key是否是在自己服务范围内
            if (!g_route_table.isMySelf(keyItem))
            {
                //返回模块错误
                TLOG_ERROR("WCacheImp::setKVBatch: " << keyItem << " is not in self area" << endl);
                TLOG_ERROR(g_route_table.toString() << endl);
                map<string, string>& context = current->getContext();
                //API直连模式，返回增量更新路由
                if (VALUE_YES == context[GET_ROUTE])
                {
                    //只返回剩余的key，已set完成的不返回
                    vector<string> vtKeys;
                    vector<int> vtIndex;
                    while (vIt != keyValue.end())
                    {
                        vtKeys.push_back(vIt->keyItem);
                        vtIndex.push_back(iIndex);
                        ++vIt;
                        ++iIndex;
                    }

                    RspUpdateServant updateServant;
                    map<string, string> rspContext;
                    rspContext[ROUTER_UPDATED] = "";
                    int ret = RouterHandle::getInstance()->getUpdateServant(vtKeys, vtIndex, true, "", updateServant);
                    if (ret != 0)
                    {
                        TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                    }
                    else
                    {
                        RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                        current->setResponseContext(rspContext);
                    }
                }
                return ET_KEY_AREA_ERR;
            }

            if (!dirty)
            {
                if (_existDB)
                {
                    dirty = true;
                }
            }

            int iRet;
            if (g_app.gstat()->isExpireEnabled())
            {
                iRet = g_sHashMap.set(keyItem, vIt->value, dirty, vIt->expireTimeSecond, vIt->version, true, TC_TimeProvider::getInstance()->getNow());
            }
            else
            {
                iRet = g_sHashMap.set(keyItem, vIt->value, dirty, vIt->expireTimeSecond, vIt->version);
            }
            if (iRet != TC_HashMapMalloc::RT_OK)
            {
                if (iRet == TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                {
                    TLOG_ERROR("WCacheImp::setKVBatch hashmap.set(" << keyItem << ") error:" << iRet << "|ver mismatch" << endl);
                    keyResult[keyItem] = SET_DATA_VER_MISMATCH;
                }
                else if (iRet == TC_HashMapMalloc::RT_NO_MEMORY)
                {
                    TLOG_ERROR("WCacheImp::setKVBatch hashmap.set(" << keyItem << ") error:" << iRet << "|no memory" << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    keyResult[keyItem] = SET_ERROR;
                }
                else
                {
                    TLOG_ERROR("WCacheImp::setKVBatch hashmap.set(" << keyItem << ") error:" << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    keyResult[keyItem] = SET_ERROR;
                }
                continue;
            }
        }
        catch (const std::exception & ex)
        {
            TLOG_ERROR("WCacheImp::setKVBatch exception: " << ex.what() << " , key = " << keyItem << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            keyResult[keyItem] = SET_ERROR;
            continue;
        }
        catch (...)
        {
            TLOG_ERROR("WCacheImp::setKVBatch unkown_exception, key = " << keyItem << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            keyResult[keyItem] = SET_ERROR;
            continue;
        }

        //写Binlog
        if (_isRecordBinLog)
        {
            TBinLogEncode logEncode;
            CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, dirty, keyItem, vIt->value, vIt->expireTimeSecond) + "\n", _binlogFile);
            if (g_app.gstat()->serverType() == MASTER)
                g_app.gstat()->setBinlogTime(0, TNOW);
        }
        if (_isRecordKeyBinLog)
        {
            TBinLogEncode logEncode;
            CacheServer::WriteToFile(logEncode.EncodeSetKey(keyItem) + "\n", _keyBinlogFile);
            if (g_app.gstat()->serverType() == MASTER)
                g_app.gstat()->setBinlogTime(0, TNOW);
        }
        keyResult[keyItem] = SET_SUCC;
    }

    return ET_SUCC;
}

tars::Int32 WCacheImp::insertKV(const DCache::SetKVReq &req, tars::TarsCurrentPtr current)
{
    const string &keyItem = req.data.keyItem;
    const string &value = req.data.value;
    bool dirty = req.data.dirty;
    uint32_t expireTimeSecond = req.data.expireTimeSecond;

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOG_ERROR("WCacheImp::insertKV: ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::insertKV: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }

        //迁移时禁止set，由于迁移时允许set的逻辑有漏洞，在解决漏洞之前禁止Set
        if (g_route_table.isTransfering(keyItem))
        {
            int iPageNo = g_route_table.getPageNo(keyItem);
            if (isTransSrc(iPageNo))
            {
                TLOG_ERROR("WCacheImp::insertKV: " << keyItem << " forbid set" << endl);
                return ET_FORBID_OPT;
            }
        }

        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(keyItem))
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::insertKV: " << keyItem << " is not in self area" << endl);
            TLOG_ERROR(g_route_table.toString() << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(keyItem, true, "", updateServant);
                if (ret != 0)
                {
                    TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                }
                else
                {
                    RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                    current->setResponseContext(rspContext);
                }
            }
            return ET_KEY_AREA_ERR;
        }
        uint32_t iSynTime, iExpireTime;
        uint8_t iVersion;
        string sValuetmp;
        int iRet = g_sHashMap.get(keyItem, sValuetmp, iSynTime, iExpireTime, iVersion);

        if (iRet == TC_HashMapMalloc::RT_OK)
        {
            g_app.gstat()->hit(_hitIndex);
            TLOG_ERROR("WCacheImp::insertKV: " << keyItem << " forbid set key has exist" << endl);
            return ET_DATA_EXIST;
        }
        else if (iRet == TC_HashMapMalloc::RT_NO_DATA)
        {
            if (_existDB && _readDB)
            {
                TLOG_DEBUG("WCacheImp::insertKV async db in insertKV, key = " << keyItem << endl);

                current->setResponse(false);
                BatchParamPtr pParam = new BatchParam();
                DbAccessPrxCallbackPtr cb = new DbAccessCallback(current, keyItem, _binlogFile, _saveOnlyKey, false, _isRecordBinLog, _isRecordKeyBinLog, pParam, "add", value, dirty, expireTimeSecond);
                //异步调用DBAccess
                _dbaccessPrx->async_get(cb, keyItem);
            }
            else
            {
                int iRet = g_sHashMap.set(keyItem, value, dirty, expireTimeSecond);

                if (iRet != TC_HashMapMalloc::RT_OK)
                {
                    TLOG_ERROR("WCacheImp::insertKV hashmap.set(" << keyItem << ") error:" << iRet << endl);
                    if (iRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                    {
                        if (iRet == TC_HashMapMalloc::RT_NO_MEMORY)
                        {
                            return ET_MEM_FULL;
                        }
                        else
                        {
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            return ET_SYS_ERR;
                        }
                    }
                    else
                        return ET_DATA_VER_MISMATCH;
                }
                if (_isRecordBinLog)
                {
                    TBinLogEncode logEncode;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, dirty, keyItem, value, expireTimeSecond) + "\n", _binlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
                if (_isRecordKeyBinLog)
                {
                    TBinLogEncode logEncode;
                    CacheServer::WriteToFile(logEncode.EncodeSetKey(keyItem) + "\n", _keyBinlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
            }
        }
        else if (iRet == TC_HashMapMalloc::RT_ONLY_KEY)
        {
            int iRet = g_sHashMap.set(keyItem, value, dirty, expireTimeSecond);

            if (iRet != TC_HashMapMalloc::RT_OK)
            {
                TLOG_ERROR("WCacheImp::insertKV hashmap.set(" << keyItem << ") error:" << iRet << endl);
                if (iRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                {
                    if (iRet == TC_HashMapMalloc::RT_NO_MEMORY)
                    {
                        return ET_MEM_FULL;
                    }
                    else
                    {
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return ET_DATA_VER_MISMATCH;
            }
            if (_isRecordBinLog)
            {
                TBinLogEncode logEncode;
                CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, dirty, keyItem, value, expireTimeSecond) + "\n", _binlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
            if (_isRecordKeyBinLog)
            {
                TBinLogEncode logEncode;
                CacheServer::WriteToFile(logEncode.EncodeSetKey(keyItem) + "\n", _keyBinlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
        }
        else
        {
            TLOG_ERROR("WCacheImp::insertKV hashmap.get(" << keyItem << ") error:" << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("WCacheImp::insertKV exception: " << ex.what() << ", key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("WCacheImp::insertKV exception: " << ex.what() << " , key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOG_ERROR("WCacheImp::insertKV unkown_exception, key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }

    return ET_SUCC;
}

tars::Int32 WCacheImp::updateKV(const DCache::UpdateKVReq &req, DCache::UpdateKVRsp &rsp, tars::TarsCurrentPtr current)
{
    const std::string &keyItem = req.data.keyItem;
    const std::string &value = req.data.value;
    bool dirty = req.data.dirty;
    uint32_t expireTimeSecond = req.data.expireTimeSecond;
    DCache::Op option = req.option;
    std::string &retValue = rsp.retValue;

    int iRet;
    try
    {
        size_t iKeyLength = keyItem.size();
        if (iKeyLength > _maxKeyLengthInDB)
        {
            TLOG_ERROR("WCacheImp::updateKV: " << keyItem << " keylength:" << iKeyLength << "limit:" << _maxKeyLengthInDB << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_PARAM_TOO_LONG;
        }
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOG_ERROR("WCacheImp::updateKV: ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::updateKV: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }

        //迁移时禁止set，由于迁移时允许set的逻辑有漏洞，在解决漏洞之前禁止Set
        if (g_route_table.isTransfering(keyItem))
        {
            int iPageNo = g_route_table.getPageNo(keyItem);
            if (isTransSrc(iPageNo))
            {
                TLOG_ERROR("WCacheImp::updateKV: " << keyItem << " forbid set" << endl);
                return ET_FORBID_OPT;
            }
        }

        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(keyItem))
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::updateKV: " << keyItem << " is not in self area" << endl);
            TLOG_ERROR(g_route_table.toString() << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(keyItem, true, "", updateServant);
                if (ret != 0)
                {
                    TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                }
                else
                {
                    RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                    current->setResponseContext(rspContext);
                }
            }
            return ET_KEY_AREA_ERR;
        }

        if (!IsDigit(value))
        {
            TLOG_ERROR("[WCacheImp::updateKV] update value is not digit! " << value << endl);
            return ET_INPUT_PARAM_ERROR;
        }

        g_app.ppReport(PPReport::SRP_SET_CNT, 1);

        if (!dirty)
        {
            if (_existDB)
            {
                dirty = true;
            }
        }

        if (g_app.gstat()->isExpireEnabled())
        {
            iRet = g_sHashMap.update(keyItem, value, option, dirty, expireTimeSecond, true, TC_TimeProvider::getInstance()->getNow(), retValue);
        }
        else
        {
            iRet = g_sHashMap.update(keyItem, value, option, dirty, expireTimeSecond, false, -1, retValue);
        }

        if (iRet == TC_HashMapMalloc::RT_OK)
        {
            g_app.gstat()->hit(_hitIndex);
        }
        else if (iRet == TC_HashMapMalloc::RT_NO_DATA)
        {
            if (_existDB && _readDB)
            {
                TLOG_DEBUG("WCacheImp::updateKV async db, key = " << keyItem << endl);

                DbAccessCBParam::UpdateCBParam param;
                param.current = current;
                param.key = keyItem;
                param.value = value;
                param.dirty = dirty;
                param.expireTime = expireTimeSecond;
                param.option = option;

                bool isCreate;
                DbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(keyItem, isCreate);
                int iAddret = pCBParam->AddUpdate(param);
                if (iAddret == 0)
                {
                    current->setResponse(false);
                    if (isCreate)
                    {
                        TLOG_DEBUG("WCacheImp::updateKV: async select db, mainKey = " << keyItem << endl);
                        DbAccessPrxCallbackPtr cb = new DbAccessCallback(current, keyItem, _binlogFile, _saveOnlyKey, false, _isRecordBinLog, _isRecordKeyBinLog, "updateEx", pCBParam);
                        //异步调用DBAccess
                        _dbaccessPrx->async_get(cb, keyItem);

                        return 1;
                    }
                    else
                    {
                        TLOG_DEBUG("WCacheImp::updateKV: set into cbparam, mainKey = " << keyItem << endl);
                        return 1;
                    }
                }
                else
                {   //之前应该是查询db成功了的
                    if (g_app.gstat()->isExpireEnabled())
                    {
                        iRet = g_sHashMap.update(keyItem, value, option, dirty, expireTimeSecond, true, TC_TimeProvider::getInstance()->getNow(), retValue);
                    }
                    else
                    {
                        iRet = g_sHashMap.update(keyItem, value, option, dirty, expireTimeSecond, false, -1, retValue);
                    }

                    if (iRet == TC_HashMapMalloc::RT_OK)
                    {
                        g_app.gstat()->hit(_hitIndex);
                    }
                    else if (iRet == TC_HashMapMalloc::RT_ONLY_KEY || iRet == TC_HashMapMalloc::RT_NO_DATA)
                    {
                        TLOG_DEBUG("WCacheImp::updateKV RT_ONLY_KEY, key = " << keyItem << endl);
                        g_app.gstat()->hit(_hitIndex);
                        return ET_NO_DATA;
                    }
                    else if (iRet == TC_HashMapMalloc::RT_DATA_EXPIRED)
                    {
                        TLOG_DEBUG("WCacheImp::updateKV RT_DATA_EXPIRED, key = " << keyItem << endl);
                        g_app.gstat()->hit(_hitIndex);
                        return ET_NO_DATA;
                    }
                    else if (iRet == TC_HashMapMalloc::RT_DATATYPE_ERR || iRet == TC_HashMapMalloc::RT_DECODE_ERR)
                    {
                        TLOG_ERROR("[WCacheImp::updateStringEx] RT_DATATYPE_ERR, key = " << keyItem << endl);
                        g_app.gstat()->hit(_hitIndex);
                        return ET_PARAM_OP_ERR;
                    }
                    else
                    {
                        TLOG_ERROR("WCacheImp::updateKV hashmap.get(" << keyItem << ") error:" << iRet << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
            }
            else
                return ET_NO_DATA;
        }
        else if (iRet == TC_HashMapMalloc::RT_ONLY_KEY)
        {
            TLOG_DEBUG("WCacheImp::updateKV RT_ONLY_KEY, key = " << keyItem << endl);
            g_app.gstat()->hit(_hitIndex);
            return ET_NO_DATA;
        }
        else if (iRet == TC_HashMapMalloc::RT_DATA_EXPIRED)
        {
            TLOG_DEBUG("WCacheImp::updateKV RT_DATA_EXPIRED, key = " << keyItem << endl);
            g_app.gstat()->hit(_hitIndex);
            return ET_NO_DATA;
        }
        else if (iRet == TC_HashMapMalloc::RT_DATATYPE_ERR || iRet == TC_HashMapMalloc::RT_DECODE_ERR)
        {
            TLOG_ERROR("[WCacheImp::updateKV] RT_DATATYPE_ERR, key = " << keyItem << endl);
            g_app.gstat()->hit(_hitIndex);
            return ET_PARAM_OP_ERR;
        }
        else if (iRet == TC_HashMapMalloc::RT_NO_MEMORY)
        {
            TLOG_ERROR("[WCacheImp::updateKV] RT_NO_MEMORY, key = " << keyItem << endl);
            g_app.gstat()->hit(_hitIndex);
            return ET_MEM_FULL;
        }
        else
        {
            TLOG_ERROR("WCacheImp::updateKV hashmap.get(" << keyItem << ") error:" << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }

        if (iRet != TC_HashMapMalloc::RT_OK)
        {
            TLOG_ERROR("WCacheImp::updateKV hashmap.set(" << keyItem << ") error:" << iRet << endl);
            if (iRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
            {
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_SYS_ERR;
            }
            else
                return ET_DATA_VER_MISMATCH;
        }
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("WCacheImp::updateKV exception: " << ex.what() << ", key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("WCacheImp::updateKV exception: " << ex.what() << " , key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOG_ERROR("WCacheImp::updateKV unkown_exception, key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    //写Binlog
    if (_isRecordBinLog)
    {
        TBinLogEncode logEncode;
        CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, dirty, keyItem, retValue, expireTimeSecond) + "\n", _binlogFile);
        if (g_app.gstat()->serverType() == MASTER)
            g_app.gstat()->setBinlogTime(0, TNOW);
    }
    if (_isRecordKeyBinLog)
    {
        TBinLogEncode logEncode;
        CacheServer::WriteToFile(logEncode.EncodeSetKey(keyItem) + "\n", _keyBinlogFile);
        if (g_app.gstat()->serverType() == MASTER)
            g_app.gstat()->setBinlogTime(0, TNOW);
    }
    return ET_SUCC;
}

tars::Int32 WCacheImp::eraseKV(const DCache::RemoveKVReq &req, tars::TarsCurrentPtr current)
{
    const string &keyItem = req.keyInfo.keyItem;
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOG_ERROR("WCacheImp::eraseKV: ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::eraseKV: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }

        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(keyItem))
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::eraseKV: " << keyItem << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(keyItem, true, "", updateServant);
                if (ret != 0)
                {
                    TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                }
                else
                {
                    RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                    current->setResponseContext(rspContext);
                }
            }
            return ET_KEY_AREA_ERR;
        }

        int iRet = g_sHashMap.checkDirty(keyItem);

        if (iRet == TC_HashMapMalloc::RT_DIRTY_DATA)
        {
            TLOG_ERROR("WCacheImp::eraseKV: " << keyItem << " failed, it is dirty data" << iRet << endl);
            return ET_ERASE_DIRTY_ERR;
        }

        iRet = g_sHashMap.erase(keyItem, req.keyInfo.version);

        if (iRet == TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
        {
            return ET_DATA_VER_MISMATCH;
        }
        else if (iRet != TC_HashMapMalloc::RT_OK && iRet != TC_HashMapMalloc::RT_NO_DATA
                 && iRet != TC_HashMapMalloc::RT_ONLY_KEY)
        {
            TLOG_ERROR("WCacheImp::eraseKV hashmap.erase(" << keyItem << ") error:" << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
        //写Binlog
        if (_isRecordBinLog)
        {
            TBinLogEncode logEncode;
            string value;
            CacheServer::WriteToFile(logEncode.Encode(BINLOG_ERASE, true, keyItem, value) + "\n", _binlogFile);
            if (g_app.gstat()->serverType() == MASTER)
                g_app.gstat()->setBinlogTime(0, TNOW);
        }
        if (_isRecordKeyBinLog)
        {
            TBinLogEncode logEncode;
            string value;
            CacheServer::WriteToFile(logEncode.Encode(BINLOG_ERASE, true, keyItem, value) + "\n", _keyBinlogFile);
            if (g_app.gstat()->serverType() == MASTER)
                g_app.gstat()->setBinlogTime(0, TNOW);
        }
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("WCacheImp::eraseKV exception: " << ex.what() << ", key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (const std::exception &ex)
    {
        TLOG_ERROR("WCacheImp::eraseKV exception: " << ex.what() << ", key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOG_ERROR("WCacheImp::eraseKV unkown exception, key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }

    return ET_SUCC;
}

tars::Int32 WCacheImp::eraseKVBatch(const DCache::RemoveKVBatchReq &req, DCache::RemoveKVBatchRsp &rsp, tars::TarsCurrentPtr current)
{
    if (g_app.gstat()->serverType() != MASTER)
    {
        //SLAVE状态下不提供接口服务
        TLOG_ERROR("WCacheImp::eraseKVBatch: ServerType is not Master" << endl);
        return ET_SERVER_TYPE_ERR;
    }
    if (req.moduleName != _moduleName)
    {
        //返回模块错误
        TLOG_ERROR("WCacheImp::eraseKVBatch: moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    g_app.ppReport(PPReport::SRP_SET_CNT, req.data.size());

    for (size_t i = 0; i < req.data.size(); i++)
    {
        const string &keyItem = req.data[i].keyItem;
        char version = req.data[i].version;
        try
        {
            //检查key是否是在自己服务范围内
            if (!g_route_table.isMySelf(keyItem))
            {
                //返回模块错误
                TLOG_ERROR("WCacheImp::eraseKVBatch: " << keyItem << " is not in self area" << endl);
                map<string, string>& context = current->getContext();
                //API直连模式，返回增量更新路由
                if (VALUE_YES == context[GET_ROUTE])
                {
                    //只返回剩余的key，已set完成的不返回
                    vector<string> vtKeys;
                    while (i < req.data.size())
                    {
                        vtKeys.push_back(req.data[i].keyItem);
                        ++i;
                    }

                    RspUpdateServant updateServant;
                    map<string, string> rspContext;
                    rspContext[ROUTER_UPDATED] = "";
                    int ret = RouterHandle::getInstance()->getUpdateServantKey(vtKeys, true, "", updateServant);
                    if (ret != 0)
                    {
                        TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                    }
                    else
                    {
                        RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                        current->setResponseContext(rspContext);
                    }
                }
                return ET_KEY_AREA_ERR;
            }

            int iRet = g_sHashMap.checkDirty(keyItem);

            if (iRet == TC_HashMapMalloc::RT_DIRTY_DATA)
            {
                TLOG_ERROR("WCacheImp::eraseKVBatch: " << keyItem << " failed, it is dirty data" << iRet << endl);
                rsp.keyResult[keyItem] = DEL_ERROR;
                continue;
            }

            iRet = g_sHashMap.erase(keyItem, version);

            if (iRet != TC_HashMapMalloc::RT_OK)
            {
                if (iRet == TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                {
                    TLOG_DEBUG("WCacheImp::eraseKVBatch hashmap.erase(" << keyItem << ") error:" << iRet << endl);
                    rsp.keyResult[keyItem] = DEL_DATA_VER_MISMATCH;
                    continue;
                }
                else if ((iRet == TC_HashMapMalloc::RT_NO_DATA) || (iRet == TC_HashMapMalloc::RT_ONLY_KEY))
                {
                    rsp.keyResult[keyItem] = DEL_SUCC;
                }
                else
                {
                    TLOG_ERROR("WCacheImp::eraseKVBatch hashmap.erase(" << keyItem << ") error:" << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    rsp.keyResult[keyItem] = DEL_ERROR;
                    continue;
                }
            }
            else
                rsp.keyResult[keyItem] = DEL_SUCC;

            //写Binlog
            if (_isRecordBinLog)
            {
                TBinLogEncode logEncode;
                string value;
                CacheServer::WriteToFile(logEncode.Encode(BINLOG_ERASE, true, keyItem, value) + "\n", _binlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
            if (_isRecordKeyBinLog)
            {
                TBinLogEncode logEncode;
                string value;
                CacheServer::WriteToFile(logEncode.Encode(BINLOG_ERASE, true, keyItem, value) + "\n", _keyBinlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
        }
        catch (const std::exception &ex)
        {
            TLOG_ERROR("WCacheImp::eraseKVBatch exception: " << ex.what() << ", key = " << keyItem << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
        catch (...)
        {
            TLOG_ERROR("WCacheImp::eraseKVBatch unkown exception, key = " << keyItem << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
    }

    return ET_SUCC;
}

tars::Int32 WCacheImp::delKV(const DCache::RemoveKVReq &req, tars::TarsCurrentPtr current)
{
    const string &keyItem = req.keyInfo.keyItem;
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOG_ERROR("WCacheImp::delKV: ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::delKV: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }

        if (g_route_table.isTransfering(keyItem))
        {
            TLOG_ERROR("WCacheImp::delKV: " << keyItem << " forbid del" << endl);
            return ET_FORBID_OPT;
        }

        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(keyItem))
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::delKV: " << keyItem << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(keyItem, true, "", updateServant);
                if (ret != 0)
                {
                    TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                }
                else
                {
                    RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                    current->setResponseContext(rspContext);
                }
            }
            return ET_KEY_AREA_ERR;
        }

        int iRet = g_sHashMap.del(keyItem, req.keyInfo.version);

        if (iRet == TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
        {
            return ET_DATA_VER_MISMATCH;
        }
        else if (iRet != TC_HashMapMalloc::RT_OK && iRet != TC_HashMapMalloc::RT_NO_DATA
                 && iRet != TC_HashMapMalloc::RT_ONLY_KEY)
        {
            TLOG_ERROR("WCacheImp::delKV hashmap.del(" << keyItem << ") error:" << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (_existDB)
        {
            current->setResponse(false);
            DbAccessPrxCallbackPtr cb = new DbAccessCallback(current, keyItem, _binlogFile, _isRecordBinLog, _isRecordKeyBinLog);
            //异步调用DBAccess
            _dbaccessPrx->async_del(cb, keyItem);
        }
        else
        {
            //写Binlog
            if (_isRecordBinLog)
            {
                TBinLogEncode logEncode;
                string value;
                CacheServer::WriteToFile(logEncode.Encode(BINLOG_DEL, true, keyItem, value) + "\n", _binlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
            if (_isRecordKeyBinLog)
            {
                TBinLogEncode logEncode;
                string value;
                CacheServer::WriteToFile(logEncode.Encode(BINLOG_DEL, true, keyItem, value) + "\n", _keyBinlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
        }
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("WCacheImp::delKV exception: " << ex.what() << ", key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("WCacheImp::delKV exception: " << ex.what() << ", key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOG_ERROR("WCacheImp::delKV unkown exception, key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }

    return ET_SUCC;
}

tars::Int32 WCacheImp::delKVBatch(const DCache::RemoveKVBatchReq &req, DCache::RemoveKVBatchRsp &rsp, tars::TarsCurrentPtr current)
{
    if (req.moduleName != _moduleName)
    {
        //返回模块错误
        TLOG_ERROR("WCacheImp::delKVBatch: moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    g_app.ppReport(PPReport::SRP_SET_CNT, req.data.size());

    vector<string> delDBKey;

    for (size_t i = 0; i < req.data.size(); i++)
    {
        const string &keyItem = req.data[i].keyItem;
        char version = req.data[i].version;
        try
        {
            //检查key是否是在自己服务范围内
            if (!g_route_table.isMySelf(keyItem))
            {
                //返回模块错误
                TLOG_ERROR("WCacheImp::delKVBatch: " << keyItem << " is not in self area" << endl);
                map<string, string>& context = current->getContext();
                //API直连模式，返回增量更新路由
                if (VALUE_YES == context[GET_ROUTE])
                {
                    //只返回剩余的key，已set完成的不返回
                    vector<string> vtKeys;
                    while (i < req.data.size())
                    {
                        vtKeys.push_back(req.data[i].keyItem);
                        ++i;
                    }

                    RspUpdateServant updateServant;
                    map<string, string> rspContext;
                    rspContext[ROUTER_UPDATED] = "";
                    int ret = RouterHandle::getInstance()->getUpdateServantKey(vtKeys, true, "", updateServant);
                    if (ret != 0)
                    {
                        TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                    }
                    else
                    {
                        RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                        current->setResponseContext(rspContext);
                    }
                }
                return ET_KEY_AREA_ERR;
            }

            if (g_route_table.isTransfering(keyItem))
            {
                TLOG_ERROR("WCacheImp::delKVBatch: " << keyItem << " forbid del" << endl);
                return ET_FORBID_OPT;
            }

            int iRet = g_sHashMap.del(keyItem, version);

            if (iRet == TC_HashMapMalloc::RT_OK || iRet == TC_HashMapMalloc::RT_NO_DATA)
            {
                delDBKey.push_back(keyItem);
                if (_existDB)
                    rsp.keyResult[keyItem] = DEL_ERROR;
                else
                    rsp.keyResult[keyItem] = DEL_SUCC;
            }
            else if (iRet == TC_HashMapMalloc::RT_ONLY_KEY)
            {
                rsp.keyResult[keyItem] = DEL_SUCC;
            }
            else if (iRet == TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
            {
                TLOG_DEBUG("WCacheImp::delKVBatch hashmap.del ret:" << iRet << endl);
                rsp.keyResult[keyItem] = DEL_DATA_VER_MISMATCH;
            }
            else
            {
                TLOG_ERROR("WCacheImp::delKVBatch hashmap.del error:" << iRet << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                rsp.keyResult[keyItem] = DEL_ERROR;
            }
        }
        catch (const std::exception &ex)
        {
            TLOG_ERROR("WCacheImp::delKVBatch exception: " << ex.what() << ", key = " << keyItem << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
        catch (...)
        {
            TLOG_ERROR("WCacheImp::delKVBatch unkown exception, key = " << keyItem << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
    }

    if (_existDB && (delDBKey.size() > 0))
    {
        current->setResponse(false);

        DelBatchParamPtr pParam = new DelBatchParam(delDBKey.size(), delDBKey, rsp);
        for (size_t i = 0; i < delDBKey.size(); ++i)
        {
            if (pParam->bEnd)
                return ET_SYS_ERR;

            TLOG_DEBUG("WCacheImp::delKVBatch async db, key = " << delDBKey[i] << endl);

            DbAccessPrxCallbackPtr cb = new DbAccessCallback(current, delDBKey[i], _binlogFile, true, _isRecordBinLog, _isRecordKeyBinLog, pParam);
            try
            {
                //异步调用DBAccess
                _dbaccessPrx->async_del(cb, delDBKey[i]);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("CacheImp::delKVBatch exception: " << ex.what() << ", key = " << delDBKey[i] << endl);
                pParam->bEnd = true;
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_SYS_ERR;
            }
            catch (...)
            {
                TLOG_ERROR("CacheImp::delKVBatch unkown exception, key = " << delDBKey[i] << endl);
                pParam->bEnd = true;
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_SYS_ERR;
            }
        }
    }
    else
    {
        for (size_t i = 0; i < delDBKey.size(); ++i)
        {
            //写Binlog
            if (_isRecordBinLog)
            {
                TBinLogEncode logEncode;
                string value;
                CacheServer::WriteToFile(logEncode.Encode(BINLOG_DEL, true, delDBKey[i], value) + "\n", _binlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
            if (_isRecordKeyBinLog)
            {
                TBinLogEncode logEncode;
                string value;
                CacheServer::WriteToFile(logEncode.Encode(BINLOG_DEL, true, delDBKey[i], value) + "\n", _keyBinlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
        }
    }
    return ET_SUCC;
}

bool WCacheImp::isTransSrc(int pageNo)
{
    ServerInfo srcServer;
    ServerInfo destServer;
    g_route_table.getTrans(pageNo, srcServer, destServer);

    if (srcServer.serverName == (ServerConfig::Application + "." + ServerConfig::ServerName))
        return true;
    return false;
}

tars::Int32 WCacheImp::setStringKey(const std::string & moduleName, const std::string & keyItem, const std::string & value, tars::Char ver, tars::Bool dirty, tars::Int32 expireTimeSecond, tars::TarsCurrentPtr current)
{
    try
    {
        size_t iKeyLength = keyItem.size();
        if (iKeyLength > _maxKeyLengthInDB)
        {
            TLOG_ERROR("WCacheImp::setStringKey: " << keyItem << " keylength:" << iKeyLength << "limit:" << _maxKeyLengthInDB << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_PARAM_TOO_LONG;
        }
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOG_ERROR("WCacheImp::setStringKey: ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::setStringKey: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }

        //迁移时禁止set，由于迁移时允许set的逻辑有漏洞，在解决漏洞之前禁止Set
        if (g_route_table.isTransfering(keyItem))
        {
            int iPageNo = g_route_table.getPageNo(keyItem);
            if (isTransSrc(iPageNo))
            {
                TLOG_ERROR("WCacheImp::setStringKey: " << keyItem << " forbid set" << endl);
                return ET_FORBID_OPT;
            }
        }

        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(keyItem))
        {
            //返回模块错误
            TLOG_ERROR("WCacheImp::setStringKey: " << keyItem << " is not in self area" << endl);
            TLOG_ERROR(g_route_table.toString() << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(keyItem, true, "", updateServant);
                if (ret != 0)
                {
                    TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                }
                else
                {
                    RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                    current->setResponseContext(rspContext);
                }
            }
            return ET_KEY_AREA_ERR;
        }

        g_app.ppReport(PPReport::SRP_SET_CNT, 1);

        if (!dirty)
        {
            if (_existDB)
            {
                dirty = true;
            }
        }

        int iRet;
        if (g_app.gstat()->isExpireEnabled())
        {
            iRet = g_sHashMap.set(keyItem, value, dirty, expireTimeSecond, ver, true, TC_TimeProvider::getInstance()->getNow());
        }
        else
        {
            iRet = g_sHashMap.set(keyItem, value, dirty, expireTimeSecond, ver);
        }
        if (iRet != TC_HashMapMalloc::RT_OK)
        {
            TLOG_ERROR("WCacheImp::setStringKey hashmap.set(" << keyItem << ") error:" << iRet << endl);
            if (iRet == TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
            {
                TLOG_ERROR("set failed : data version mismatch !" << endl);
                return ET_DATA_VER_MISMATCH;
            }
            else if (iRet == TC_HashMapMalloc::RT_NO_MEMORY)
            {
                TLOG_ERROR("set failed : no memory!" << endl);
                return ET_MEM_FULL;
            }
            else
            {
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_CACHE_ERR;
            }
        }
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("WCacheImp::setStringKey exception: " << ex.what() << ", key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (const std::exception & ex)
    {
        TLOG_ERROR("WCacheImp::setStringKey exception: " << ex.what() << " , key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOG_ERROR("WCacheImp::setStringKey unkown_exception, key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    //写Binlog
    if (_isRecordBinLog)
    {
        TBinLogEncode logEncode;
        CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, dirty, keyItem, value, expireTimeSecond) + "\n", _binlogFile);
        if (g_app.gstat()->serverType() == MASTER)
            g_app.gstat()->setBinlogTime(0, TNOW);
    }
    if (_isRecordKeyBinLog)
    {
        TBinLogEncode logEncode;
        CacheServer::WriteToFile(logEncode.EncodeSetKey(keyItem) + "\n", _keyBinlogFile);
        if (g_app.gstat()->serverType() == MASTER)
            g_app.gstat()->setBinlogTime(0, TNOW);
    }
    return ET_SUCC;
}


