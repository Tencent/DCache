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
#include "CacheImp.h"
#include "RouterHandle.h"

using namespace std;

//////////////////////////////////////////////////////
void CacheImp::initialize()
{
    //initialize servant here:
    //...
    _config = ServerConfig::BasePath + "CacheServer.conf";
    _tcConf.parseFile(_config);

    TARS_ADD_ADMIN_CMD_NORMAL("reload", CacheImp::reloadConf);

    _moduleName = _tcConf["/Main<ModuleName>"];
    _binlogFile = _tcConf["/Main/BinLog<LogFile>"];

    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _isRecordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _isRecordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(_tcConf["/Main/DbAccess<ObjName>"]);

    _saveOnlyKey = (_tcConf["/Main/Cache<SaveOnlyKey>"] == "Y" || _tcConf["/Main/Cache<SaveOnlyKey>"] == "y") ? true : false;
    _readDB = (_tcConf["/Main/DbAccess<ReadDbFlag>"] == "Y" || _tcConf["/Main/DbAccess<ReadDbFlag>"] == "y") ? true : false;

    _hitIndex = g_app.gstat()->genHitIndex();
    TLOGDEBUG("CacheImp::initialize Succ, _hitIndex:" << _hitIndex << endl);
}

//////////////////////////////////////////////////////
void CacheImp::destroy()
{
    //destroy servant here:
    //...
}

bool CacheImp::reloadConf(const string& command, const string& params, string& result)
{
    _tcConf.parseFile(_config);
    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _isRecordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _isRecordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _saveOnlyKey = (_tcConf["/Main/Cache<SaveOnlyKey>"] == "Y" || _tcConf["/Main/Cache<SaveOnlyKey>"] == "y") ? true : false;
    _readDB = (_tcConf["/Main/DbAccess<ReadDbFlag>"] == "Y" || _tcConf["/Main/DbAccess<ReadDbFlag>"] == "y") ? true : false;
    TLOGDEBUG("CacheImp::reloadConf Succ" << endl);
    result = "SUCC";

    return true;
}

tars::Int32 CacheImp::checkString(const std::string& moduleName, const string& keyItem)
{
    try
    {
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("CacheImp::checkString: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(keyItem))
        {
            //返回模块错误
            TLOGERROR("CacheImp::checkString: " << keyItem << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        g_app.ppReport(PPReport::SRP_GET_CNT, 1);

        int iRet;
        if (g_app.gstat()->isExpireEnabled())
        {
            iRet = g_sHashMap.checkDirty(keyItem, true, TC_TimeProvider::getInstance()->getNow());
        }
        else
        {
            iRet = g_sHashMap.checkDirty(keyItem);
        }

        if (iRet == TC_HashMapMalloc::RT_OK)
        {
            return ET_SUCC;
        }
        else if (iRet == TC_HashMapMalloc::RT_NO_DATA || iRet == TC_HashMapMalloc::RT_DATA_EXPIRED)
        {
            return ET_NO_DATA;
        }
        else if (iRet == TC_HashMapMalloc::RT_ONLY_KEY)
        {
            return ET_ONLY_KEY;
        }
        else if (iRet == TC_HashMapMalloc::RT_DIRTY_DATA)
        {
            return ET_DIRTY_DATA;
        }
        else
        {
            TLOGERROR("CacheImp::checkString hashmap.checkDirty(" << keyItem << ") error:" << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("CacheImp::checkString exception: " << ex.what() << ", key = " << keyItem << endl);
    }
    catch (...)
    {
        TLOGERROR("CacheImp::checkString unkown exception, key = " << keyItem << endl);
    }

    return ET_SYS_ERR;
}

tars::Int32 CacheImp::checkKey(const DCache::CheckKeyReq &req, DCache::CheckKeyRsp &rsp, tars::TarsCurrentPtr current)
{
    try
    {
        size_t keyCount = req.keys.size();
        for (size_t i = 0; i < keyCount; ++i)
        {
            const string &sKeyItem = req.keys[i];
            int iRet = checkString(req.moduleName, sKeyItem);
            if (ET_SERVER_TYPE_ERR == iRet || ET_MODULE_NAME_INVALID == iRet
                    || ET_KEY_AREA_ERR == iRet || ET_SYS_ERR == iRet)
            {
                TLOGERROR("WCacheImp::checkKey error,key:" << sKeyItem << " ret:" << iRet << endl);
                if (ET_KEY_AREA_ERR == iRet)
                {
                    map<string, string>& context = current->getContext();
                    //API直连模式，返回增量更新路由
                    if (VALUE_YES == context[GET_ROUTE])
                    {
                        RspUpdateServant updateServant;
                        map<string, string> rspContext;
                        rspContext[ROUTER_UPDATED] = "";
                        int ret = RouterHandle::getInstance()->getUpdateServant(req.keys, true, "", updateServant);
                        if (ret != 0)
                        {
                            TLOGERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                        }
                        else
                        {
                            RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                            current->setResponseContext(rspContext);
                        }
                    }
                }
                return iRet;
            }

            SKeyStatus stat;
            if (ET_SUCC == iRet)
            {
                stat.exist = true;
                stat.dirty = false;
            }
            else if (ET_DIRTY_DATA == iRet)
            {
                stat.exist = true;
                stat.dirty = true;
            }
            else
            {
                stat.exist = false;
                stat.dirty = false;
            }

            rsp.keyStat[sKeyItem] = stat;
        }
        return ET_SUCC;
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("WCacheImp::checkKey exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("WCacheImp::checkKey unkown exception" << endl);
    }
    return ET_SYS_ERR;
}

tars::Int32 CacheImp::getKV(const DCache::GetKVReq &req, DCache::GetKVRsp &rsp, tars::TarsCurrentPtr current)
{
    return getValueExp(req.moduleName, req.keyItem, rsp.value, rsp.ver, rsp.expireTime, current);
}

tars::Int32 CacheImp::getKVBatch(const DCache::GetKVBatchReq &req, DCache::GetKVBatchRsp &rsp, tars::TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const vector<std::string>& vtKeyItem = req.keys;
    vector<SKeyValue>& vtValue = rsp.values;

    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOGERROR("CacheImp::getKVBatch: moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }
    vector<string> vtNoCacheKey;

    size_t keyCount = vtKeyItem.size();
    g_app.ppReport(PPReport::SRP_GET_CNT, keyCount);
    for (size_t i = 0; i < keyCount; ++i)
    {
        try
        {
            //检查key是否是在自己服务范围内
            if (!g_route_table.isMySelf(vtKeyItem[i]))
            {
                //返回模块错误
                TLOGERROR("CacheImp::getKVBatch: " << vtKeyItem[i] << " is not in self area" << endl);
                map<string, string>& context = current->getContext();
                //API直连模式，返回增量更新路由
                if (VALUE_YES == context[GET_ROUTE])
                {
                    RspUpdateServant updateServant;
                    map<string, string> rspContext;
                    rspContext[ROUTER_UPDATED] = "";
                    int ret = RouterHandle::getInstance()->getUpdateServant(vtKeyItem, false, context[API_IDC], updateServant);
                    if (ret != 0)
                    {
                        TLOGERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                    }
                    else
                    {
                        RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                        current->setResponseContext(rspContext);
                    }
                }
                return ET_KEY_AREA_ERR;
            }

            g_app.gstat()->tryHit(_hitIndex);
            string sValue;
            uint32_t iSynTime, iExpireTime;
            uint8_t iVersion;

            int iRet;
            if (g_app.gstat()->isExpireEnabled())
            {
                iRet = g_sHashMap.get(vtKeyItem[i], sValue, iSynTime, iExpireTime, iVersion, true, TC_TimeProvider::getInstance()->getNow());
            }
            else
            {
                iRet = g_sHashMap.get(vtKeyItem[i], sValue, iSynTime, iExpireTime, iVersion);
            }

            if (iRet == TC_HashMapMalloc::RT_OK)
            {
                SKeyValue sKeyValue;
                sKeyValue.keyItem = vtKeyItem[i];
                sKeyValue.value = sValue;
                sKeyValue.ret = VALUE_SUCC;
                sKeyValue.ver = iVersion;
                sKeyValue.expireTime = iExpireTime;
                vtValue.push_back(sKeyValue);
                g_app.gstat()->hit(_hitIndex);
            }
            else if (iRet == TC_HashMapMalloc::RT_NO_DATA)
            {
                vtNoCacheKey.push_back(vtKeyItem[i]);
            }
            else if (iRet == TC_HashMapMalloc::RT_ONLY_KEY)
            {
                SKeyValue sKeyValue;
                sKeyValue.keyItem = vtKeyItem[i];
                sKeyValue.value = "";
                sKeyValue.ret = VALUE_NO_DATA;
                sKeyValue.ver = 1;
                sKeyValue.expireTime = 0;
                vtValue.push_back(sKeyValue);
                g_app.gstat()->hit(_hitIndex);
                TLOGDEBUG("CacheImp::getKVBatch RT_ONLY_KEY, key = " << vtKeyItem[i] << endl);
            }
            else if (iRet == TC_HashMapMalloc::RT_DATA_EXPIRED)
            {
                SKeyValue sKeyValue;
                sKeyValue.keyItem = vtKeyItem[i];
                sKeyValue.value = "";
                sKeyValue.ret = VALUE_NO_DATA;
                sKeyValue.ver = iVersion;
                sKeyValue.expireTime = 0;
                vtValue.push_back(sKeyValue);
                g_app.gstat()->hit(_hitIndex);
                TLOGDEBUG("CacheImp::getKVBatch RT_DATA_EXPIRED, key = " << vtKeyItem[i] << endl);
            }
            else
            {
                TLOGERROR("CacheImp::getKVBatch hashmap.get(" << vtKeyItem[i] << ") error:" << iRet << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                return ET_SYS_ERR;
            }
        }
        catch (const std::exception &ex)
        {
            TLOGERROR("CacheImp::getKVBatch exception: " << ex.what() << ", key = " << vtKeyItem[i] << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
        catch (...)
        {
            TLOGERROR("CacheImp::getKVBatch unkown exception, key = " << vtKeyItem[i] << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
    }

    if (vtNoCacheKey.size() == 0)
        return ET_SUCC;

    if (_tcConf["/Main/DbAccess<DBFlag>"] == "Y"  && _readDB)
    {
        current->setResponse(false);

        BatchParamPtr pParam = new BatchParam(vtNoCacheKey.size(), vtValue);
        for (size_t i = 0; i < vtNoCacheKey.size(); ++i)
        {
            if (pParam->bEnd)
                return ET_SYS_ERR;

            TLOGDEBUG("CacheImp::getKVBatch async db, key = " << vtNoCacheKey[i] << endl);

            DbAccessPrxCallbackPtr cb = new DbAccessCallback(current, vtNoCacheKey[i], _binlogFile, _saveOnlyKey, true, _isRecordBinLog, _isRecordKeyBinLog, pParam);
            try
            {
                //异步调用DBAccess
                _dbaccessPrx->async_get(cb, vtNoCacheKey[i]);
            }
            catch (const std::exception &ex)
            {
                TLOGERROR("CacheImp::getKVBatch exception: " << ex.what() << ", key = " << vtNoCacheKey[i] << endl);
                pParam->bEnd = true;
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_SYS_ERR;
            }
            catch (...)
            {
                TLOGERROR("CacheImp::getKVBatch unkown exception, key = " << vtNoCacheKey[i] << endl);
                pParam->bEnd = true;
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_SYS_ERR;
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtNoCacheKey.size(); ++i)
        {
            SKeyValue sKeyValue;
            sKeyValue.keyItem = vtNoCacheKey[i];
            sKeyValue.value = "";
            sKeyValue.ret = VALUE_NO_DATA;
            sKeyValue.ver = 1;
            sKeyValue.expireTime = 0;
            vtValue.push_back(sKeyValue);
        }
    }

    return ET_SUCC;
}

tars::Int32 CacheImp::getAllKeys(const DCache::GetAllKeysReq &req, DCache::GetAllKeysRsp &rsp, tars::TarsCurrentPtr current)
{
    int index = req.index;
    int count = req.count;

    TLOGDEBUG("CacheImp::getAllMainKey: index:" << index << " num:" << count << endl);

    if (req.moduleName != _moduleName)
    {
        //返回模块错误
        TLOGERROR("CacheImp::getAllMainKey: moduleName error!" << req.moduleName << endl);
        return ET_MODULE_NAME_INVALID;
    }

    rsp.isEnd = false;

    try
    {
        //检查条件是否正确
        if ((index < 0) || (count < -1))
        {
            TLOGERROR("[CacheImp::getAllMainKey]: condition error" << endl);
            return ET_INPUT_PARAM_ERROR;
        }

        tars::Int32 iCount = 0;
        SHashMap::dcache_hash_iterator it = g_sHashMap.hashByPos(index);

        if (it == g_sHashMap.hashEnd())
        {
            TLOGERROR("CacheImp::getAllMainKey hashByPos return end! index:" << index << endl);
            rsp.isEnd = true;
            return ET_SUCC;
        }
        while (it != g_sHashMap.hashEnd())
        {
            {
                if ((count == 0) || ((count > 0) && (iCount++ >= count)))
                {
                    rsp.isEnd = false;
                    return ET_SUCC;
                }

                vector<string> tmpvec;
                it->getKey(tmpvec);

                rsp.keys.insert(rsp.keys.end(), tmpvec.begin(), tmpvec.end());
            }
            ++it;
        }

        TLOGDEBUG("MKCacheImp::getAllMainKey reach the end! " << index << "|" << count << endl);

        rsp.isEnd = true;

    }
    catch (const std::exception &ex)
    {
        TLOGERROR("CacheImp::getAllMainKey exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("CacheImp::getAllMainKey unkown exception" << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }

    return ET_SUCC;
}

bool CacheImp::isTransSrc(int pageNo)
{
    ServerInfo srcServer;
    ServerInfo destServer;
    g_route_table.getTrans(pageNo, srcServer, destServer);

    if (srcServer.serverName == (ServerConfig::Application + "." + ServerConfig::ServerName))
        return true;
    return false;
}

string CacheImp::getTransDest(int pageNo)
{
    ServerInfo srcServer;
    ServerInfo destServer;
    int iRet = g_route_table.getTrans(pageNo, srcServer, destServer);
    if (iRet != UnpackTable::RET_SUCC)
    {
        return "";
    }

    string sTransDest = destServer.RouteClientServant;
    return sTransDest;
}

tars::Int32 CacheImp::getSyncTime(tars::TarsCurrentPtr current)
{
    return g_app.getSyncTime();
}

tars::Int32 CacheImp::getValueExp(const std::string & moduleName, const std::string & keyItem, std::string &value, tars::Char & ver, tars::Int32 &expireTime, tars::TarsCurrentPtr current, bool accessDB)
{
    //TLOGDEBUG("[CacheImp::getValueExp]entered." << moduleName << "|" << keyItem << endl);

    try
    {
        if (moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("CacheImp::getValueExp: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(keyItem))
        {
            //返回模块错误
            TLOGERROR("CacheImp::getValueExp: " << keyItem << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(keyItem, false, context[API_IDC], updateServant);
                if (ret != 0)
                {
                    TLOGERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                }
                else
                {
                    RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                    current->setResponseContext(rspContext);
                }
            }
            return ET_KEY_AREA_ERR;
        }

        g_app.ppReport(PPReport::SRP_GET_CNT, 1);

        g_app.gstat()->tryHit(_hitIndex);

        uint32_t iSynTime, iExpireTime;
        uint8_t iVersion;
        ver = 1;
        int iRet;
        if (g_app.gstat()->isExpireEnabled())
        {
            iRet = g_sHashMap.get(keyItem, value, iSynTime, iExpireTime, iVersion, true, TC_TimeProvider::getInstance()->getNow());
        }
        else
        {
            iRet = g_sHashMap.get(keyItem, value, iSynTime, iExpireTime, iVersion);
        }

        if (iRet == TC_HashMapMalloc::RT_OK)
        {
            ver = iVersion;
            expireTime = iExpireTime;
            g_app.gstat()->hit(_hitIndex);
            return ET_SUCC;
        }
        else if (iRet == TC_HashMapMalloc::RT_NO_DATA)
        {
            if (accessDB && _tcConf["/Main/DbAccess<DBFlag>"] == "Y"  && _readDB)
            {
                TLOGDEBUG("CacheImp::getValueExp async db, key = " << keyItem << endl);

                current->setResponse(false);
                BatchParamPtr pParam = new BatchParam();
                DbAccessPrxCallbackPtr cb = new DbAccessCallback(current, keyItem, _binlogFile, _saveOnlyKey, false, _isRecordBinLog, _isRecordKeyBinLog, pParam);
                //异步调用DBAccess
                _dbaccessPrx->async_get(cb, keyItem);
            }

            return ET_NO_DATA;
        }
        else if (iRet == TC_HashMapMalloc::RT_ONLY_KEY)
        {
            TLOGDEBUG("CacheImp::getValueExp RT_ONLY_KEY, key = " << keyItem << endl);
            g_app.gstat()->hit(_hitIndex);
            return ET_NO_DATA;
        }
        else if (iRet == TC_HashMapMalloc::RT_DATA_EXPIRED)
        {
            ver = iVersion;
            TLOGDEBUG("CacheImp::getValueExp RT_DATA_EXPIRED, key = " << keyItem << endl);
            g_app.gstat()->hit(_hitIndex);
            return ET_NO_DATA;
        }
        else
        {
            TLOGERROR("CacheImp::getValueExp hashmap.get(" << keyItem << ") error:" << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
    }
    catch (const TarsException & ex)
    {
        TLOGERROR("CacheImp::getValueExp exception: " << ex.what() << ", key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("CacheImp::getValueExp exception: " << ex.what() << ", key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("CacheImp::getValueExp unkown exception, key = " << keyItem << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }

    return ET_SUCC;
}

