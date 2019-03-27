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
#include "MKWCacheImp.h"

using namespace std;


//////////////////////////////////////////////////////
void MKWCacheImp::initialize()
{
    //initialize servant here:
    //...
    _config = ServerConfig::BasePath + "MKCacheServer.conf";
    _tcConf.parseFile(_config);

    TARS_ADD_ADMIN_CMD_NORMAL("reload", MKWCacheImp::reloadConf);

    _moduleName = _tcConf["/Main<ModuleName>"];
    _binlogFile = _tcConf["/Main/BinLog<LogFile>"];
    _keyBinlogFile = _tcConf["/Main/BinLog<LogFile>"] + "key";

    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    string sRDBaccessObj = _tcConf["/Main/DbAccess<ObjName>"];
    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(sRDBaccessObj);

    _saveOnlyKey = (_tcConf["/Main/Cache<SaveOnlyKey>"] == "Y" || _tcConf["/Main/Cache<SaveOnlyKey>"] == "y") ? true : false;
    _readDB = (_tcConf["/Main/DbAccess<ReadDbFlag>"] == "Y" || _tcConf["/Main/DbAccess<ReadDbFlag>"] == "y") ? true : false;
    _existDB = (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y") ? true : false;

    _insertAtHead = (_tcConf["/Main/Cache<InsertOrder>"] == "Y" || _tcConf["/Main/Cache<InsertOrder>"] == "y") ? true : false;

    string sUpdateOrder = _tcConf.get("/Main/Cache<UpdateOrder>", "N");

    _updateInOrder = (sUpdateOrder == "Y" || sUpdateOrder == "y") ? true : false;

    _mkeyMaxDataCount = TC_Common::strto<size_t>(_tcConf.get("/Main/Cache<MkeyMaxDataCount>", "0"));

    _maxKeyLengthInDB = TC_Common::strto<size_t>(_tcConf.get("/Main/Cache<MaxKeyLengthInDB>", "767"));

    if (_readDB && _mkeyMaxDataCount > 0)
    {
        TLOGERROR("MKWCacheImp::initialize error: read db flag is true and MkeyMaxDataCount>0 at the same time,,MkeyMaxDataCount's value is initialized to 0" << endl);
        //重新设置为无数量限制
        _mkeyMaxDataCount = 0;
    }

    _orderItem = _tcConf.get("/Main/Cache<OrderItem>", "");

    //如果排序是递减，就默认最新的数据值都是最大的，反之，默认新数据值为最小值
    string sOrderDesc = _tcConf.get("/Main/Cache<OrderDesc>", "Y");
    _orderByDesc = (sOrderDesc == "Y" || sOrderDesc == "y") ? true : false;

    //限制主key下最大限数据量功能开启时，如果有db，就不能删除脏数据
    if (_existDB)
        _deleteDirty = false;
    else
        _deleteDirty = true;

    g_app.gstat()->getFieldConfig(_fieldConf);

    _mkIsInteger = (ConvertDbType(_fieldConf.mpFieldInfo[_fieldConf.sMKeyName].type) == INT) ? true : false;
    _hitIndex = g_app.gstat()->genHitIndex();
    TLOGDEBUG("MKWCacheImp::initialize _hitIndex:" << _hitIndex << endl);

    _mkeyMaxSelectCount = TC_Common::strto<size_t>(_tcConf.get("/Main<MKeyMaxBlockCount>", "20000"));

    TLOGDEBUG("MKWCacheImp::initialize Succ" << endl);
}

//////////////////////////////////////////////////////
void MKWCacheImp::destroy()
{
    //destroy servant here:
    //...
}

bool MKWCacheImp::reloadConf(const string& command, const string& params, string& result)
{
    _tcConf.parseFile(_config);
    _saveOnlyKey = (_tcConf["/Main/Cache<SaveOnlyKey>"] == "Y" || _tcConf["/Main/Cache<SaveOnlyKey>"] == "y") ? true : false;
    _readDB = (_tcConf["/Main/DbAccess<ReadDbFlag>"] == "Y" || _tcConf["/Main/DbAccess<ReadDbFlag>"] == "y") ? true : false;
    _existDB = (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y") ? true : false;
    _insertAtHead = (_tcConf["/Main/Cache<InsertOrder>"] == "Y" || _tcConf["/Main/Cache<InsertOrder>"] == "y") ? true : false;
    string sUpdateOrder = _tcConf.get("/Main/Cache<UpdateOrder>", "N");
    _updateInOrder = (sUpdateOrder == "Y" || sUpdateOrder == "y") ? true : false;

    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _mkeyMaxDataCount = TC_Common::strto<size_t>(_tcConf.get("/Main/Cache<MkeyMaxDataCount>", "0"));
    if (_readDB && _mkeyMaxDataCount > 0)
    {
        TLOGERROR("MKWCacheImp::reloadConf error: read db flag is true and MkeyMaxDataCount>0 at the same time,MkeyMaxDataCount's value is reset to 0" << endl);
        result = "dbflag is true and MkeyMaxDataCount>0 at the same time,failed to set MkeyMaxDataCount and MkeyMaxDataCount's value is reset to 0";
        //重新设置为无数量限制
        _mkeyMaxDataCount = 0;
        return true;
    }

    //限制主key下最大限数据量功能开启时，如果有db，就不能删除脏数据
    if (_existDB)
        _deleteDirty = false;
    else
        _deleteDirty = true;

    _mkeyMaxSelectCount = TC_Common::strto<size_t>(_tcConf.get("/Main<MKeyMaxBlockCount>", "20000"));
    TLOGDEBUG("MKWCacheImp::reloadConf Succ" << endl);
    result = "SUCC";

    return true;
}

tars::Int32 MKWCacheImp::insertMKV(const DCache::InsertMKVReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.data.mainKey;
    const map<std::string, DCache::UpdateValue> & mpValue = req.data.mpValue;
    tars::Char ver = req.data.ver;
    bool dirty = req.data.dirty;
    bool replace = req.data.replace;
    int expireTimeSecond = req.data.expireTimeSecond;

    string sLogValue = FormatLog::tostr(mpValue);

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << "|" << sLogValue << "|" << (int)ver << "|" << dirty << "|" << replace << "|" << expireTimeSecond << endl);
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid insert" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(mainKey, true, "", updateServant);
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
        map<string, DCache::UpdateValue> mpUK;
        map<string, DCache::UpdateValue> mpJValue;

        g_app.ppReport(PPReport::SRP_SET_CNT, 1);
        int iRetCode;
        if (!checkMK(mainKey, _mkIsInteger, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error, retcode = " << iRetCode << endl);
            return iRetCode;
        }
        if (!checkSetValue(mpValue, mpUK, mpJValue, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error, retcode = " << iRetCode << endl);
            return iRetCode;
        }
        TarsEncode uKeyEncode;

        size_t KeyLengthInDB = 0;
        for (size_t i = 0; i < _fieldConf.vtUKeyName.size(); i++)
        {
            const string &sUKName = _fieldConf.vtUKeyName[i];
            const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sUKName];
            uKeyEncode.write(mpUK[sUKName].value, fieldInfo.tag, fieldInfo.type);

            KeyLengthInDB += mpUK[sUKName].value.size();
        }

        KeyLengthInDB += mainKey.size();
        if (KeyLengthInDB > _maxKeyLengthInDB)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " keyLength > " << _maxKeyLengthInDB << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_PARAM_TOO_LONG;
        }

        string uk;
        MultiHashMap::Value vData;

        uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

        TarsEncode vEncode;
        for (size_t i = 0; i < _fieldConf.vtValueName.size(); i++)
        {
            const string &sValueName = _fieldConf.vtValueName[i];
            const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
            vEncode.write(mpJValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
        }

        if (!dirty)
        {
            if (_existDB)
                dirty = true;
        }

        string value;
        value.assign(vEncode.getBuffer(), vEncode.getLength());

        int iRet;
        if (!replace)
        {
            g_app.gstat()->tryHit(_hitIndex);
            if (g_app.gstat()->isExpireEnabled())
            {
                iRet = g_HashMap.get(mainKey, uk, vData, true, TC_TimeProvider::getInstance()->getNow());
            }
            else
            {
                iRet = g_HashMap.get(mainKey, uk, vData);
            }



            if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
            {
                g_app.gstat()->hit(_hitIndex);
                return ET_DATA_EXIST;
            }
            else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
            {
                //查数据库
                if (_existDB  && _readDB)
                {
                    MKDbAccessCBParam::InsertCBParam param;
                    param.current = current;
                    //param.mpValue = mpValue;
                    param.mk = mainKey;
                    param.uk = uk;
                    param.value = value;
                    param.logValue = sLogValue;
                    param.ver = ver;
                    param.dirty = dirty;
                    param.replace = replace;
                    param.expireTime = expireTimeSecond;
                    param.bBatch = false;

                    bool isCreate;
                    MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mainKey, isCreate);
                    int iAddret = pCBParam->AddInsert(param);
                    if (iAddret == 0)
                    {
                        current->setResponse(false);
                        if (isCreate)
                        {
                            TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": async select db, mainKey = " << mainKey << endl);
                            DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                            //异步调用DBAccess
                            asyncDbSelect(mainKey, cb);

                            return 0;
                        }
                        else
                        {
                            TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": set into cbparam, mainKey = " << mainKey << endl);
                            return 0;
                        }
                    }
                    else
                    {
                        g_app.gstat()->tryHit(_hitIndex);
                        int iGetRet;
                        if (g_app.gstat()->isExpireEnabled())
                        {
                            iGetRet = g_HashMap.get(mainKey, uk, vData, true, TC_TimeProvider::getInstance()->getNow());
                        }
                        else
                        {
                            iGetRet = g_HashMap.get(mainKey, uk, vData);
                        }


                        if (iGetRet == TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            g_app.gstat()->hit(_hitIndex);
                            return ET_DATA_EXIST;
                        }
                        else if ((iGetRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY) && (iGetRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                        {
                            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": g_HashMap.get(mk, uk, vData) error, mainKey = " << mainKey << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            return ET_SYS_ERR;
                        }
                    }
                }
            }
            else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
            {
                g_app.gstat()->hit(_hitIndex);
            }
            else
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.get error, ret = " << iRet << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                return ET_SYS_ERR;
            }
        }

        if (g_app.gstat()->isExpireEnabled())
        {
            iRet = g_HashMap.set(mainKey, uk, value, expireTimeSecond, ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty, true, TC_TimeProvider::getInstance()->getNow());
        }
        else
        {
            iRet = g_HashMap.set(mainKey, uk, value, expireTimeSecond, ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);
        }

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
        {
            if (iRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set ver mismatch" << endl);
                return ET_DATA_VER_MISMATCH;
            }
            else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set no memory" << endl);
                return ET_MEM_FULL;
            }
            else
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set error, ret = " << iRet << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                return ET_SYS_ERR;
            }

        }

        if (_recordBinLog)
            WriteBinLog::set(mainKey, uk, value, expireTimeSecond, dirty, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::set(mainKey, uk, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::insertMKVBatch(const DCache::InsertMKVBatchReq &req, DCache::MKVBatchWriteRsp &rsp, tars::TarsCurrentPtr current)
{
    const vector<InsertKeyValue> & vtKeyValue = req.data;
    map<tars::Int32, tars::Int32> & mpFailIndexReason = rsp.rspData;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << FormatLog::tostr(vtKeyValue) << endl);

    if (g_app.gstat()->serverType() != MASTER)
    {
        //SLAVE状态下不提供接口服务
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
        return ET_SERVER_TYPE_ERR;
    }

    if (req.moduleName != _moduleName)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error, input:" << req.moduleName << "!=" << _moduleName << endl);
        return ET_MODULE_NAME_INVALID;
    }

    bool bCanAccessDB = (_existDB && _readDB) ? true : false;
    vector<MKDbAccessCBParam::InsertCBParam> vtMKDbAccessCBParam;
    vtMKDbAccessCBParam.clear();

    size_t keyCount = vtKeyValue.size();
    g_app.ppReport(PPReport::SRP_SET_CNT, keyCount);

    //API 直连，路由错误则返回正确路由
    RspUpdateServant updateServant;
    bool bGetRoute = false;
    {
        map<string, string>& context = current->getContext();
        //API直连模式，返回增量更新路由
        if (VALUE_YES == context[GET_ROUTE])
        {
            bGetRoute = true;
        }
    }

    for (size_t i = 0; i < keyCount; ++i)
    {
        const InsertKeyValue &tmpKeyValue = vtKeyValue[i];
        bool dirty = true;
        string sLogValue = FormatLog::tostr(tmpKeyValue.mpValue);

        try
        {
            if (g_route_table.isTransfering(tmpKeyValue.mainKey))
            {
                int iPageNo = g_route_table.getPageNo(tmpKeyValue.mainKey);
                if (isTransSrc(iPageNo))
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << tmpKeyValue.mainKey << " forbid insert" << endl);
                    mpFailIndexReason[i] = ET_FORBID_OPT;
                    continue;
                }
            }

            //检查key是否是在自己服务范围内
            if (!g_route_table.isMySelf(tmpKeyValue.mainKey))
            {
                //返回模块错误
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << tmpKeyValue.mainKey << " is not in self area" << endl);
                mpFailIndexReason[i] = ET_KEY_AREA_ERR;

                //API直连模式，返回增量更新路由
                if (bGetRoute)
                {
                    int ret = RouterHandle::getInstance()->getUpdateServant(tmpKeyValue.mainKey, i, true, "", updateServant);
                    if (ret != 0)
                    {
                        TLOGERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                    }
                }
                continue;
            }

            map<string, DCache::UpdateValue> mpUK;
            map<string, DCache::UpdateValue> mpJValue;
            int iRetCode;
            if (!checkSetValue(tmpKeyValue.mpValue, mpUK, mpJValue, iRetCode) || !checkMK(tmpKeyValue.mainKey, _mkIsInteger, iRetCode))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": input param error, mainKey = " << tmpKeyValue.mainKey << endl);
                mpFailIndexReason[i] = iRetCode;
                continue;
            }

            dirty = tmpKeyValue.dirty;
            if (!dirty)
            {
                if (_existDB)
                    dirty = true;
            }

            TarsEncode uKeyEncode;
            size_t KeyLengthInDB = 0;
            for (size_t j = 0; j < _fieldConf.vtUKeyName.size(); j++)
            {
                const string &sUKName = _fieldConf.vtUKeyName[j];
                const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sUKName];
                uKeyEncode.write(mpUK[sUKName].value, fieldInfo.tag, fieldInfo.type);

                KeyLengthInDB += mpUK[sUKName].value.size();
            }

            KeyLengthInDB += tmpKeyValue.mainKey.size();
            if (KeyLengthInDB > _maxKeyLengthInDB)
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " keyLength > " << _maxKeyLengthInDB << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                mpFailIndexReason[i] = ET_PARAM_TOO_LONG;
                continue;
            }

            string uk;
            uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

            TarsEncode vEncode;
            for (size_t j = 0; j < _fieldConf.vtValueName.size(); ++j)
            {
                const string& sValueName = _fieldConf.vtValueName[j];
                const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
                vEncode.write(mpJValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
            }

            string value;
            value.assign(vEncode.getBuffer(), vEncode.getLength());

            tars::Int32 iRet;
            MultiHashMap::Value vData;
            if (!tmpKeyValue.replace)
            {
                g_app.gstat()->tryHit(_hitIndex);
                if (g_app.gstat()->isExpireEnabled())
                {
                    iRet = g_HashMap.get(tmpKeyValue.mainKey, uk, vData, true, TC_TimeProvider::getInstance()->getNow());
                }
                else
                {
                    iRet = g_HashMap.get(tmpKeyValue.mainKey, uk, vData);
                }


                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    g_app.gstat()->hit(_hitIndex);
                    mpFailIndexReason[i] = ET_DATA_EXIST;
                    continue;
                }
                else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
                {
                    //查数据库
                    if (bCanAccessDB)
                    {
                        const InsertKeyValue &keyValue = tmpKeyValue;
                        MKDbAccessCBParam::InsertCBParam param;
                        param.current = current;
                        param.mk = keyValue.mainKey;
                        param.uk = uk;
                        param.value = value;
                        param.logValue = sLogValue;
                        param.ver = keyValue.ver;
                        param.dirty = dirty;
                        param.replace = keyValue.replace;
                        param.expireTime = keyValue.expireTimeSecond;
                        param.bBatch = true;
                        param.iIndex = i;
                        vtMKDbAccessCBParam.push_back(param);
                        continue;
                    }
                }
                else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    g_app.gstat()->hit(_hitIndex);
                }
                else
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap get error, ret = " << iRet << ", mainKey = " << tmpKeyValue.mainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    mpFailIndexReason[i] = ET_SYS_ERR;
                    continue;
                }
            }

            if (g_app.gstat()->isExpireEnabled())
            {
                iRet = g_HashMap.set(tmpKeyValue.mainKey, uk, value, tmpKeyValue.expireTimeSecond, tmpKeyValue.ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty, true, TC_TimeProvider::getInstance()->getNow());
            }
            else
            {
                iRet = g_HashMap.set(tmpKeyValue.mainKey, uk, value, tmpKeyValue.expireTimeSecond, tmpKeyValue.ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);
            }

            if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
            {
                if (iRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set ver mismatch, mainKey = " << tmpKeyValue.mainKey << endl);
                    mpFailIndexReason[i] = ET_DATA_VER_MISMATCH;
                }
                else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set no memory, mainKey = " << tmpKeyValue.mainKey << endl);
                    mpFailIndexReason[i] = ET_MEM_FULL;
                }
                else
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set error, ret = " << iRet << ", mainKey = " << tmpKeyValue.mainKey << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    mpFailIndexReason[i] = ET_SYS_ERR;
                }
                continue;
            }

            if (_recordBinLog)
                WriteBinLog::set(tmpKeyValue.mainKey, uk, value, tmpKeyValue.expireTimeSecond, dirty, _binlogFile);
            if (_recordKeyBinLog)
                WriteBinLog::set(tmpKeyValue.mainKey, uk, _keyBinlogFile);

            continue;
        }
        catch (const exception& ex)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mainKey = " << tmpKeyValue.mainKey << endl);
        }
        catch (...)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << tmpKeyValue.mainKey << endl);
        }
        g_app.ppReport(PPReport::SRP_EX, 1);
        mpFailIndexReason[i] = ET_SYS_ERR;
    }

    if (!vtMKDbAccessCBParam.empty())
    {
        current->setResponse(false);
        InsertBatchParamPtr pParam = new InsertBatchParam(vtMKDbAccessCBParam.size(), mpFailIndexReason);
        if (bGetRoute)
        {
            pParam->updateServant = updateServant;
        }


        for (size_t i = 0; i < vtMKDbAccessCBParam.size(); ++i)
        {
            MKDbAccessCBParam::InsertCBParam& param = vtMKDbAccessCBParam[i];
            param.pParam = pParam;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(param.mk, isCreate);
            tars::Int32 iAddret = pCBParam->AddInsert(param);
            if (iAddret == 0)
            {
                if (isCreate)
                {
                    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": async insert db, mainKey = " << param.mk << endl);
                    try
                    {
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                        //异步调用DBAccess
                        asyncDbSelect(param.mk, cb);
                        continue;
                    }
                    catch (std::exception& ex)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mainKey = " << param.mk << endl);
                    }
                    catch (...)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mainKey = " << param.mk << endl);
                    }
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    pParam->addFailIndexReason(param.iIndex, ET_SYS_ERR);
                    if (pParam->count.dec() <= 0)
                    {
                        if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_insertMKVBatch(current, ET_PARTIAL_FAIL, pParam->rsp);
                        break;
                    }
                }
                else
                {
                    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": set into cbparam, mainKey = " << param.mk << endl);
                }
            }
            else
            {
                g_app.gstat()->tryHit(_hitIndex);
                MultiHashMap::Value vData;
                tars::Int32 iRet;
                if (g_app.gstat()->isExpireEnabled())
                {
                    iRet = g_HashMap.get(param.mk, param.uk, vData, true, TC_TimeProvider::getInstance()->getNow());
                }
                else
                {
                    iRet = g_HashMap.get(param.mk, param.uk, vData);
                }


                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    g_app.gstat()->hit(_hitIndex);
                    pParam->addFailIndexReason(param.iIndex, ET_DATA_EXIST);
                    if (pParam->count.dec() <= 0)
                    {
                        if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_insertMKVBatch(current, ET_PARTIAL_FAIL, pParam->rsp);
                        break;
                    }
                    continue;
                }
                else if ((iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY) && (iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": g_HashMap.get(mk, uk, vData) error: " << iRet << ", mainKey = " << param.mk << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    pParam->addFailIndexReason(param.iIndex, ET_SYS_ERR);
                    if (pParam->count.dec() <= 0)
                    {
                        if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_insertMKVBatch(current, ET_PARTIAL_FAIL, pParam->rsp);
                        break;
                    }
                    continue;
                }
                else
                {
                    g_app.gstat()->hit(_hitIndex);
                }

                iRet = g_HashMap.set(param.mk, param.uk, param.value, param.expireTime, param.ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, param.dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    if (iRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set ver mismatch, mainKey = " << param.mk << endl);
                        pParam->addFailIndexReason(param.iIndex, ET_DATA_VER_MISMATCH);
                    }
                    else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set no memory, mainKey = " << param.mk << endl);
                        pParam->addFailIndexReason(param.iIndex, ET_MEM_FULL);
                    }
                    else
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set error, ret = " << iRet << ", mainKey = " << param.mk << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        pParam->addFailIndexReason(param.iIndex, ET_SYS_ERR);
                    }
                    if (pParam->count.dec() <= 0)
                    {
                        if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_insertMKVBatch(current, ET_PARTIAL_FAIL, pParam->rsp);
                        break;
                    }
                    continue;
                }

                if (_recordBinLog)
                    WriteBinLog::set(param.mk, param.uk, param.value, param.expireTime, param.dirty, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::set(param.mk, param.uk, _keyBinlogFile);
                if (pParam->count.dec() <= 0)
                {
                    if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                    {
                        map<string, string> rspContext;
                        rspContext[ROUTER_UPDATED] = "";
                        RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                        current->setResponseContext(rspContext);
                    }
                    MKWCache::async_response_insertMKVBatch(current, pParam->rsp.rspData.empty() ? ET_SUCC : ET_PARTIAL_FAIL, pParam->rsp);
                    break;
                }
            }
        }
    }
    else
    {
        if (bGetRoute && updateServant.mpServant.size() > 0)
        {
            map<string, string> rspContext;
            rspContext[ROUTER_UPDATED] = "";
            RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
            current->setResponseContext(rspContext);
        }
    }

    return mpFailIndexReason.empty() ? ET_SUCC : ET_PARTIAL_FAIL;
}

tars::Int32 MKWCacheImp::updateMKVBatch(const DCache::UpdateMKVBatchReq &req, DCache::MKVBatchWriteRsp &rsp, tars::TarsCurrentPtr current)
{
    const vector<DCache::UpdateKeyValue> & vtKeyValue = req.data;
    map<tars::Int32, tars::Int32> &mpFailIndexReason = rsp.rspData;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << FormatLog::tostr(vtKeyValue) << endl);
    if (g_app.gstat()->serverType() != MASTER)
    {
        //SLAVE状态下不提供接口服务
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
        return ET_SERVER_TYPE_ERR;
    }

    if (req.moduleName != _moduleName)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error, input:" << req.moduleName << "!=" << _moduleName << endl);
        return ET_MODULE_NAME_INVALID;
    }

    bool bCanAccessDB = (_existDB && _readDB) ? true : false;
    vector<MKDbAccessCBParam::UpdateCBParamForBatch> vtMKDbAccessCBParam;
    vtMKDbAccessCBParam.clear();

    //API 直连，路由错误则返回正确路由
    RspUpdateServant updateServant;
    bool bGetRoute = false;
    {
        map<string, string>& context = current->getContext();
        //API直连模式，返回增量更新路由
        if (VALUE_YES == context[GET_ROUTE])
        {
            bGetRoute = true;
        }
    }

    size_t keyCount = vtKeyValue.size();
    g_app.ppReport(PPReport::SRP_SET_CNT, keyCount);

    for (size_t i = 0; i < keyCount; ++i)
    {
        const DCache::UpdateKeyValue &tmpKeyValue = vtKeyValue[i];
        bool dirty = tmpKeyValue.dirty;
        if (!dirty)
        {
            if (_existDB)
                dirty = true;
        }
        string sLogValue = FormatLog::tostr(tmpKeyValue.mpValue);

        try
        {
            if (g_route_table.isTransfering(tmpKeyValue.mainKey))
            {
                int iPageNo = g_route_table.getPageNo(tmpKeyValue.mainKey);
                if (isTransSrc(iPageNo))
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << tmpKeyValue.mainKey << " forbid insert" << endl);
                    mpFailIndexReason[i] = ET_FORBID_OPT;
                    continue;
                }
            }

            //检查key是否是在自己服务范围内
            if (!g_route_table.isMySelf(tmpKeyValue.mainKey))
            {
                //返回模块错误
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << tmpKeyValue.mainKey << " is not in self area" << endl);
                mpFailIndexReason[i] = ET_KEY_AREA_ERR;

                //API直连模式，返回增量更新路由
                if (bGetRoute)
                {
                    int ret = RouterHandle::getInstance()->getUpdateServant(tmpKeyValue.mainKey, i, true, "", updateServant);
                    if (ret != 0)
                    {
                        TLOGERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                    }
                }
                continue;
            }

            map<string, DCache::UpdateValue> mpUK;
            map<string, DCache::UpdateValue> mpInsertValue;
            map<string, DCache::UpdateValue> mpUpdateValue;

            //其中插入数据用mpInsertValue 更新数据用mpUpdateValue
            int iRetCode;
            if (!checkSetValue(tmpKeyValue.mpValue, mpUK, mpInsertValue, mpUpdateValue, tmpKeyValue.insert, iRetCode) || !checkMK(tmpKeyValue.mainKey, _mkIsInteger, iRetCode))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": input param error, mainKey = " << tmpKeyValue.mainKey << endl);
                mpFailIndexReason[i] = iRetCode;
                continue;
            }

            TarsEncode uKeyEncode;

            size_t KeyLengthInDB = 0;
            for (size_t j = 0; j < _fieldConf.vtUKeyName.size(); j++)
            {
                const string &sUKName = _fieldConf.vtUKeyName[j];
                const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sUKName];

                uKeyEncode.write(mpUK[sUKName].value, fieldInfo.tag, fieldInfo.type);

                KeyLengthInDB += mpUK[sUKName].value.size();
            }

            KeyLengthInDB += tmpKeyValue.mainKey.size();
            if (KeyLengthInDB > _maxKeyLengthInDB)
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " keyLength > " << _maxKeyLengthInDB << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                mpFailIndexReason[i] = ET_PARAM_TOO_LONG;
                continue;
            }

            string uk;
            uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

            tars::Int32 iRet;
            MultiHashMap::Value vData;
            g_app.gstat()->tryHit(_hitIndex);
            if (g_app.gstat()->isExpireEnabled())
            {
                iRet = g_HashMap.get(tmpKeyValue.mainKey, uk, vData, true, TC_TimeProvider::getInstance()->getNow());
            }
            else
                iRet = g_HashMap.get(tmpKeyValue.mainKey, uk, vData);


            if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
            {
                g_app.gstat()->hit(_hitIndex);
                string value;
                value = updateValue(mpUpdateValue, vData._value);

                int iSetRet = g_HashMap.set(tmpKeyValue.mainKey, uk, value, tmpKeyValue.expireTimeSecond, tmpKeyValue.ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, 0, false);


                if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set ver mismatch" << endl);
                        mpFailIndexReason[i] = ET_DATA_VER_MISMATCH;
                    }
                    else
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set error, ret = " << iSetRet << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        mpFailIndexReason[i] = ET_SYS_ERR;
                    }
                }
                if (_recordBinLog)
                    WriteBinLog::set(tmpKeyValue.mainKey, uk, value, tmpKeyValue.expireTimeSecond, dirty, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::set(tmpKeyValue.mainKey, uk, _keyBinlogFile);
            }
            else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
            {
                //查数据库
                if (bCanAccessDB)
                {
                    const DCache::UpdateKeyValue &keyValue = tmpKeyValue;
                    MKDbAccessCBParam::UpdateCBParamForBatch param;
                    param.current = current;
                    param.mk = keyValue.mainKey;
                    param.uk = uk;
                    param.mpInsertValue = mpInsertValue;
                    param.mpUpdateValue = mpUpdateValue;
                    param.logValue = sLogValue;
                    param.ver = keyValue.ver;
                    param.dirty = dirty;
                    param.insert = keyValue.insert;
                    param.expireTime = keyValue.expireTimeSecond;
                    //param.bBatch  = true;
                    param.iIndex = i;
                    vtMKDbAccessCBParam.push_back(param);
                }
                else
                {
                    if (tmpKeyValue.insert)
                    {
                        TarsEncode vEncode;
                        for (size_t j = 0; j < _fieldConf.vtValueName.size(); ++j)
                        {
                            const string& sValueName = _fieldConf.vtValueName[j];
                            const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
                            vEncode.write(mpInsertValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
                        }

                        string insertValue;
                        insertValue.assign(vEncode.getBuffer(), vEncode.getLength());

                        int iSetRet = g_HashMap.set(tmpKeyValue.mainKey, uk, insertValue, tmpKeyValue.expireTimeSecond, tmpKeyValue.ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);
                        if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                            {
                                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set ver mismatch" << endl);
                                mpFailIndexReason[i] = ET_DATA_VER_MISMATCH;
                            }
                            else if (iSetRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
                            {
                                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set no memory" << endl);
                                mpFailIndexReason[i] = ET_MEM_FULL;
                            }
                            else
                            {
                                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set error, ret = " << iSetRet << endl);
                                g_app.ppReport(PPReport::SRP_EX, 1);
                                mpFailIndexReason[i] = ET_SYS_ERR;
                            }
                        }
                        if (_recordBinLog)
                            WriteBinLog::set(tmpKeyValue.mainKey, uk, insertValue, tmpKeyValue.expireTimeSecond, dirty, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::set(tmpKeyValue.mainKey, uk, _keyBinlogFile);
                    }
                    else
                    {
                        mpFailIndexReason[i] = ET_NO_DATA;
                    }
                }
            }
            else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
            {
                g_app.gstat()->hit(_hitIndex);
                if (tmpKeyValue.insert)
                {
                    TarsEncode vEncode;
                    for (size_t j = 0; j < _fieldConf.vtValueName.size(); ++j)
                    {
                        const string& sValueName = _fieldConf.vtValueName[j];
                        const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
                        vEncode.write(mpInsertValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
                    }

                    string insertValue;
                    insertValue.assign(vEncode.getBuffer(), vEncode.getLength());

                    int iSetRet = g_HashMap.set(tmpKeyValue.mainKey, uk, insertValue, tmpKeyValue.expireTimeSecond, tmpKeyValue.ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);


                    if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                        {
                            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set ver mismatch" << endl);
                            mpFailIndexReason[i] = ET_DATA_VER_MISMATCH;
                        }
                        else if (iSetRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
                        {
                            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set no memory" << endl);
                            mpFailIndexReason[i] = ET_MEM_FULL;
                        }
                        else
                        {
                            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set error, ret = " << iSetRet << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            mpFailIndexReason[i] = ET_SYS_ERR;
                        }
                    }
                    if (_recordBinLog)
                        WriteBinLog::set(tmpKeyValue.mainKey, uk, insertValue, tmpKeyValue.expireTimeSecond, dirty, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::set(tmpKeyValue.mainKey, uk, _keyBinlogFile);
                }
                else
                {
                    mpFailIndexReason[i] = ET_NO_DATA;
                }
            }
            else
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap get error, ret = " << iRet << ", mainKey = " << tmpKeyValue.mainKey << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                mpFailIndexReason[i] = ET_SYS_ERR;
            }
            continue;
        }
        catch (const exception& ex)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mainKey = " << tmpKeyValue.mainKey << endl);
        }
        catch (...)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << tmpKeyValue.mainKey << endl);
        }
        g_app.ppReport(PPReport::SRP_EX, 1);
        mpFailIndexReason[i] = ET_SYS_ERR;
    }

    if (!vtMKDbAccessCBParam.empty())
    {
        current->setResponse(false);
        UpdateBatchParamPtr pParam = new UpdateBatchParam(vtMKDbAccessCBParam.size(), mpFailIndexReason);
        if (bGetRoute)
        {
            pParam->updateServant = updateServant;
        }

        for (size_t i = 0; i < vtMKDbAccessCBParam.size(); ++i)
        {
            MKDbAccessCBParam::UpdateCBParamForBatch& param = vtMKDbAccessCBParam[i];
            param.pParam = pParam;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(param.mk, isCreate);
            tars::Int32 iAddret = pCBParam->AddUpdateForBatch(param);
            if (iAddret == 0)
            {
                if (isCreate)
                {
                    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": async update db, mainKey = " << param.mk << endl);
                    try
                    {
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                        //异步调用DBAccess
                        asyncDbSelect(param.mk, cb);
                        continue;
                    }
                    catch (std::exception& ex)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mainKey = " << param.mk << endl);
                    }
                    catch (...)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mainKey = " << param.mk << endl);
                    }
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    pParam->addFailIndexReason(param.iIndex, ET_SYS_ERR);
                    if (pParam->count.dec() <= 0)
                    {
                        if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_updateMKVBatch(current, ET_PARTIAL_FAIL, pParam->rsp);
                        break;
                    }
                }
                else
                {
                    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": set into cbparam, mainKey = " << param.mk << endl);
                }
            }
            else
            {

                g_app.gstat()->tryHit(_hitIndex);
                MultiHashMap::Value vData;
                tars::Int32 iRet;
                if (g_app.gstat()->isExpireEnabled())
                {
                    iRet = g_HashMap.get(param.mk, param.uk, vData, true, TC_TimeProvider::getInstance()->getNow());
                }
                else
                    iRet = g_HashMap.get(param.mk, param.uk, vData);


                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    g_app.gstat()->hit(_hitIndex);
                    string value;
                    value = updateValue(param.mpUpdateValue, vData._value);

                    int iSetRet = g_HashMap.set(param.mk, param.uk, value, param.expireTime, param.ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, param.dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);


                    if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                        {
                            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set ver mismatch" << endl);
                            pParam->addFailIndexReason(param.iIndex, ET_DATA_VER_MISMATCH);
                        }
                        else
                        {
                            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set error, ret = " << iSetRet << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            pParam->addFailIndexReason(param.iIndex, ET_SYS_ERR);
                        }
                        if (pParam->count.dec() <= 0)
                        {
                            if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                            {
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                current->setResponseContext(rspContext);
                            }
                            MKWCache::async_response_updateMKVBatch(current, ET_PARTIAL_FAIL, pParam->rsp);
                            break;
                        }
                        continue;
                    }
                    if (_recordBinLog)
                        WriteBinLog::set(param.mk, param.uk, value, param.expireTime, param.dirty, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::set(param.mk, param.uk, _keyBinlogFile);
                    if (pParam->count.dec() <= 0)
                    {
                        if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_updateMKVBatch(current, pParam->rsp.rspData.empty() ? ET_SUCC : ET_PARTIAL_FAIL, pParam->rsp);
                        break;
                    }
                }
                else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
                        g_app.gstat()->hit(_hitIndex);
                    if (param.insert)
                    {
                        TarsEncode vEncode;
                        for (size_t j = 0; j < _fieldConf.vtValueName.size(); ++j)
                        {
                            const string& sValueName = _fieldConf.vtValueName[j];
                            const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
                            vEncode.write(param.mpInsertValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
                        }

                        string insertValue;
                        insertValue.assign(vEncode.getBuffer(), vEncode.getLength());

                        int iSetRet = g_HashMap.set(param.mk, param.uk, insertValue, param.expireTime, param.ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, param.dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);


                        if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                            {
                                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set ver mismatch" << endl);
                                pParam->addFailIndexReason(param.iIndex, ET_DATA_VER_MISMATCH);
                            }
                            else if (iSetRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
                            {
                                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set no memory" << endl);
                                pParam->addFailIndexReason(param.iIndex, ET_MEM_FULL);
                            }
                            else
                            {
                                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.set error, ret = " << iSetRet << endl);
                                g_app.ppReport(PPReport::SRP_EX, 1);
                                pParam->addFailIndexReason(param.iIndex, ET_SYS_ERR);
                            }
                            if (pParam->count.dec() <= 0)
                            {
                                if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                                {
                                    map<string, string> rspContext;
                                    rspContext[ROUTER_UPDATED] = "";
                                    RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                    current->setResponseContext(rspContext);
                                }
                                MKWCache::async_response_updateMKVBatch(current, ET_PARTIAL_FAIL, pParam->rsp);
                                break;
                            }
                            continue;
                        }
                        if (_recordBinLog)
                            WriteBinLog::set(param.mk, param.uk, insertValue, param.expireTime, param.dirty, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::set(param.mk, param.uk, _keyBinlogFile);
                        if (pParam->count.dec() <= 0)
                        {
                            if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                            {
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                current->setResponseContext(rspContext);
                            }
                            MKWCache::async_response_updateMKVBatch(current, pParam->rsp.rspData.empty() ? ET_SUCC : ET_PARTIAL_FAIL, pParam->rsp);
                            break;
                        }
                    }
                    else
                    {
                        pParam->addFailIndexReason(param.iIndex, ET_NO_DATA);
                        if (pParam->count.dec() <= 0)
                        {
                            if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                            {
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                current->setResponseContext(rspContext);
                            }
                            MKWCache::async_response_updateMKVBatch(current, ET_PARTIAL_FAIL, pParam->rsp);
                            break;
                        }
                    }
                }
                else
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": g_HashMap.get(mk, uk, vData) error: " << iRet << ", mainKey = " << param.mk << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    pParam->addFailIndexReason(param.iIndex, ET_SYS_ERR);
                    if (pParam->count.dec() <= 0)
                    {
                        if (bGetRoute && pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_updateMKVBatch(current, ET_PARTIAL_FAIL, pParam->rsp);
                        break;
                    }
                }
                continue;
            }
        }
    }
    else
    {
        if (bGetRoute && updateServant.mpServant.size() > 0)
        {
            map<string, string> rspContext;
            rspContext[ROUTER_UPDATED] = "";
            RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
            current->setResponseContext(rspContext);
        }
    }

    return mpFailIndexReason.empty() ? ET_SUCC : ET_PARTIAL_FAIL;
}

tars::Int32 MKWCacheImp::updateMKV(const DCache::UpdateMKVReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const map<std::string, DCache::UpdateValue> & mpValue = req.value;
    const vector<DCache::Condition> & vtCond = req.cond;
    tars::Char ver = req.ver;
    bool dirty = req.dirty;
    int expireTimeSecond = req.expireTimeSecond;
    bool insert = req.insert;

    int iUpdateCount = 0;
    string sLogValue = FormatLog::tostr(mpValue);
    string sLogCond = FormatLog::tostr(vtCond);

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << "|" << sLogValue << "|" << sLogCond << "|" << (int)ver << "|" << dirty << "|" << expireTimeSecond << "|" << insert << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid update" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(mainKey, true, "", updateServant);
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

        int iRetCode;
        if (!checkUpdateValue(mpValue, iRetCode) || !checkMK(mainKey, _mkIsInteger, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": value param error" << endl);
            return iRetCode;
        }

        vector<DCache::Condition> vtUKCond;
        vector<DCache::Condition> vtValueCond;
        bool bUKey;
        Limit stLimit;

        if (!checkCondition(vtCond, vtUKCond, vtValueCond, stLimit, bUKey, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": cond param error" << endl);
            return iRetCode;
        }

        if (!dirty)
        {
            if (_existDB)
                dirty = true;
        }

        if (bUKey)
        {
            int iRet = procUpdateUK(current, mainKey, mpValue, vtUKCond, vtValueCond, stLimit, ver, dirty, expireTimeSecond, iUpdateCount, insert);
            if (iRet < 0)
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << mainKey << "|" << sLogValue << "|" << sLogCond << "|" << (int)ver << "|" << dirty << "|" << expireTimeSecond << "|" << insert << "|failed|procUpdateUK error , ret =" << iRet << endl);
                return iRet;
            }
            else if (iRet == 1)
                return 0;
        }
        else
        {
            int iRet = procUpdateMK(current, mainKey, mpValue, vtUKCond, vtValueCond, stLimit, ver, dirty, expireTimeSecond, iUpdateCount);
            if (iRet < 0)
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << mainKey << "|" << sLogValue << "|" << sLogCond << "|" << (int)ver << "|" << dirty << "|" << expireTimeSecond << "|" << insert << "|failed|procUpdateMK error , ret =" << iRet << endl);
                return iRet;
            }
            else if (iRet == 1)
                return 0;
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return iUpdateCount;
}

tars::Int32 MKWCacheImp::updateMKVAtom(const DCache::UpdateMKVAtomReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const map<std::string, DCache::UpdateValue> & mpValue = req.value;
    const vector<DCache::Condition> & vtCond = req.cond;
    bool dirty = req.dirty;
    int expireTimeSecond = req.expireTimeSecond;

    int iUpdateCount = 0;
    string sLogValue = FormatLog::tostr(mpValue);
    string sLogCond = FormatLog::tostr(vtCond);

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << "|" << sLogValue << "|" << sLogCond << "|" << dirty << "|" << expireTimeSecond << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid update" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(mainKey, true, "", updateServant);
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

        int iRetCode;
        if (!checkUpdateValue(mpValue, iRetCode) || !checkMK(mainKey, _mkIsInteger, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": value param error" << endl);
            return iRetCode;
        }

        vector<DCache::Condition> vtUKCond;
        vector<DCache::Condition> vtValueCond;
        bool bUKey;
        Limit stLimit;

        if (!checkCondition(vtCond, vtUKCond, vtValueCond, stLimit, bUKey, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": cond param error" << endl);
            return iRetCode;
        }

        if (!dirty)
        {
            if (_existDB)
                dirty = true;
        }

        if (bUKey)
        {
            int iRet = procUpdateUKAtom(current, mainKey, mpValue, vtUKCond, vtValueCond, stLimit, dirty, expireTimeSecond, iUpdateCount);
            if (iRet < 0)
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << mainKey << "|" << sLogValue << "|" << sLogCond << "|" << dirty << "|" << expireTimeSecond << "|failed|procUpdateUK error , ret =" << iRet << endl);
                return iRet;
            }
            else if (iRet == 1)
                return 0;
        }
        else
        {
            int iRet = procUpdateMKAtom(current, mainKey, mpValue, vtUKCond, vtValueCond, stLimit, dirty, expireTimeSecond, iUpdateCount);
            if (iRet < 0)
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << mainKey << "|" << sLogValue << "|" << sLogCond << "|" << dirty << "|" << expireTimeSecond << "|failed|procUpdateMK error , ret =" << iRet << endl);
                return iRet;
            }
            else if (iRet == 1)
                return 0;
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return iUpdateCount;
}

tars::Int32 MKWCacheImp::eraseMKV(const DCache::MainKeyReq &req, tars::TarsCurrentPtr current)
{
    const string &mainKey = req.mainKey;
    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(mainKey, true, "", updateServant);
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

        int iCheckRet = g_HashMap.checkDirty(mainKey);

        if (iCheckRet == TC_Multi_HashMap_Malloc::RT_DIRTY_DATA)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, is dirty" << endl);
            return ET_ERASE_DIRTY_ERR;
        }

        int iRet = g_HashMap.erase(mainKey);

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        //把主key下的数据设为部分数据
        g_HashMap.setFullData(mainKey, false);

        if (_recordBinLog)
            WriteBinLog::erase(mainKey, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::erase(mainKey, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::delMKV(const DCache::DelMKVReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const vector<DCache::Condition> & vtCond = req.cond;

    string sLogCond = FormatLog::tostr(vtCond);

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << "|" << sLogCond << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid del" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(mainKey, true, "", updateServant);
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

        TC_Multi_HashMap_Malloc::MainKey::KEYTYPE keyType;
        g_HashMap.getMainKeyType(keyType);

        if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == keyType)
        {
            vector<DCache::Condition> vtUKCond;
            vector<DCache::Condition> vtValueCond;
            bool bUKey;
            Limit stLimit;
            int iRetCode;
            if (!checkCondition(vtCond, vtUKCond, vtValueCond, stLimit, bUKey, iRetCode))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
                return iRetCode;
            }

            if (bUKey)
            {
                int iRet = procDelUK(current, mainKey, vtUKCond, vtValueCond, stLimit);
                if (iRet < 0)
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << mainKey << "|" << sLogCond << "|failed|procDelUK error, ret = " << iRet << endl);
                    return iRet;
                }
                else if (iRet == 1)
                    return ET_SUCC;
            }
            else
            {
                int iRet = procDelMK(current, mainKey, vtUKCond, vtValueCond, stLimit);
                if (iRet < 0)
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << mainKey << "|" << sLogCond << "|failed|procDelMK error, ret = " << iRet << endl);
                    return iRet;
                }
                else if (iRet == 1)
                    return ET_SUCC;
            }
        }
        else if (TC_Multi_HashMap_Malloc::MainKey::LIST_TYPE == keyType)
        {
            DCache::MainKeyReq eraseReq;
            eraseReq.moduleName = req.moduleName;
            eraseReq.mainKey = mainKey;
            return eraseMKV(eraseReq, current);
        }
        else
        {
            int iRet = g_HashMap.checkMainKey(mainKey);
            TLOGDEBUG("[MKCacheImp::" << __FUNCTION__ << "] checkMainKey ret :" << iRet << endl);
            if (_existDB  && _readDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA))
            {
                MKDbAccessCBParam::DelCBParam param;
                param.current = current;
                param.mk = mainKey;
                param.bUKey = false;

                bool isCreate;
                MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mainKey, isCreate);
                int iAddret = pCBParam->AddDel(param);
                if (iAddret == 0)
                {
                    current->setResponse(false);
                    if (isCreate)
                    {
                        TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": async select db, mainKey = " << mainKey << endl);
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, keyType);
                        //异步调用DBAccess
                        asyncDbSelect(mainKey, cb);
                        return 1;
                    }
                    else
                    {
                        TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": set into cbparam, mainKey = " << mainKey << endl);
                        return 1;
                    }
                }
                else
                {
                    if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == keyType)
                    {
                        g_app.gstat()->tryHit(_hitIndex);
                        int iRet = g_HashMap.delZSetSetBit(mainKey, time(NULL));

                        if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                        {

                        }
                        else if ((iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                        {
                            g_app.gstat()->hit(_hitIndex);
                        }
                        else
                        {
                            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": error, ret = " << iRet << ", mainKey = " << mainKey << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            return ET_SYS_ERR;
                        }

                        if (_recordBinLog)
                            WriteBinLog::delZSet(mainKey, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::delZSet(mainKey, _keyBinlogFile);

                        return ET_SUCC;
                    }
                    else if (TC_Multi_HashMap_Malloc::MainKey::SET_TYPE == keyType)
                    {
                        g_app.gstat()->tryHit(_hitIndex);
                        int iRet = g_HashMap.delSetSetBit(mainKey, time(NULL));

                        if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                        {

                        }
                        else if ((iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                        {
                            g_app.gstat()->hit(_hitIndex);
                        }
                        else
                        {
                            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": error, ret = " << iRet << ", mainKey = " << mainKey << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            return ET_SYS_ERR;
                        }

                        if (_recordBinLog)
                            WriteBinLog::delSet(mainKey, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::delSet(mainKey, _keyBinlogFile);

                        return ET_SUCC;
                    }
                }
            }

            if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == keyType)
            {
                iRet = g_HashMap.delZSetSetBit(mainKey, time(NULL));

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_SYS_ERR;
                }

                if (_recordBinLog)
                    WriteBinLog::delZSet(mainKey, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::delZSet(mainKey, _keyBinlogFile);
            }
            else if (TC_Multi_HashMap_Malloc::MainKey::SET_TYPE == keyType)
            {
                iRet = g_HashMap.delSetSetBit(mainKey, time(NULL));

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_SYS_ERR;
                }

                if (_recordBinLog)
                    WriteBinLog::delSet(mainKey, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::delSet(mainKey, _keyBinlogFile);
            }
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::delMKVBatch(const DCache::DelMKVBatchReq &req, DCache::MKVBatchWriteRsp &rsp, tars::TarsCurrentPtr current)
{
    const vector<DCache::DelCondition> & vtCond = req.data;
    map<tars::Int32, tars::Int32> & mRet = rsp.rspData;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << vtCond.size() << endl);

    if (g_app.gstat()->serverType() != MASTER)
    {
        //SLAVE状态下不提供接口服务
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
        return ET_SERVER_TYPE_ERR;
    }
    if (req.moduleName != _moduleName)
    {
        //返回模块错误
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    //API 直连，路由错误则返回正确路由
    RspUpdateServant updateServant;
    bool bGetRoute = false;
    {
        map<string, string>& context = current->getContext();
        //API直连模式，返回增量更新路由
        if (VALUE_YES == context[GET_ROUTE])
        {
            bGetRoute = true;
        }
    }

    vector<MKDbAccessCBParam::DelCBParamForBatch> vtNeedDB;
    map<string, int> mIndex;

    for (unsigned int i = 0; i < vtCond.size(); i++)
    {
        string sLogCond = FormatLog::tostr(vtCond[i].cond);

        mIndex[vtCond[i].mainKey] = i;

        try
        {
            if (g_route_table.isTransfering(vtCond[i].mainKey))
            {
                int iPageNo = g_route_table.getPageNo(vtCond[i].mainKey);
                if (isTransSrc(iPageNo))
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << vtCond[i].mainKey << " forbid del" << endl);
                    mRet[i] = DEL_ERROR;
                    continue;
                }
            }
            //检查key是否是在自己服务范围内
            if (!g_route_table.isMySelf(vtCond[i].mainKey))
            {
                //返回模块错误
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << vtCond[i].mainKey << " is not in self area" << endl);
                mRet[i] = DEL_ERROR;
                //API直连模式，返回增量更新路由
                if (bGetRoute)
                {
                    int ret = RouterHandle::getInstance()->getUpdateServant(vtCond[i].mainKey, i, true, "", updateServant);
                    if (ret != 0)
                    {
                        TLOGERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                    }
                }
                continue;
            }

            vector<DCache::Condition> vtUKCond;
            vector<DCache::Condition> vtValueCond;
            bool bUKey;
            Limit stLimit;
            int iRetCode;
            if (!checkCondition(vtCond[i].cond, vtUKCond, vtValueCond, stLimit, bUKey, iRetCode))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
                mRet[i] = DEL_ERROR;
                continue;
            }

            if (bUKey)
            {
                int delCount = 0;
                int iRet = procDelUKVer(current, vtCond[i].mainKey, vtUKCond, vtValueCond, stLimit, vtCond[i].ver, delCount);
                if (iRet < 0)
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << vtCond[i].mainKey << "|" << sLogCond << "|failed|procDelUK error, ret = " << iRet << endl);
                    mRet[i] = DEL_ERROR;
                    continue;
                }
                else if (iRet == 1)
                {
                    MKDbAccessCBParam::DelCBParamForBatch param;
                    param.mk = vtCond[i].mainKey;
                    param.bUKey = true;
                    param.vtUKCond = vtUKCond;
                    param.vtValueCond = vtValueCond;
                    param.stLimit = stLimit;
                    param.pMKDBaccess = _dbaccessPrx;
                    param.ver = vtCond[i].ver;

                    vtNeedDB.push_back(param);

                    mRet[i] = DEL_ERROR;
                }
                else
                {
                    mRet[i] = delCount;
                }
            }
            else
            {
                int delCount = 0;
                int iRet = procDelMKVer(current, vtCond[i].mainKey, vtUKCond, vtValueCond, stLimit, delCount);
                if (iRet < 0)
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << vtCond[i].mainKey << "|" << sLogCond << "|failed|procDelMK error, ret = " << iRet << endl);
                    continue;
                }
                else if (iRet == 1)
                {
                    MKDbAccessCBParam::DelCBParamForBatch param;
                    param.mk = vtCond[i].mainKey;
                    param.bUKey = false;
                    param.vtUKCond = vtUKCond;
                    param.vtValueCond = vtValueCond;
                    param.stLimit = stLimit;
                    param.pMKDBaccess = _dbaccessPrx;
                    param.ver = vtCond[i].ver;

                    vtNeedDB.push_back(param);

                    mRet[i] = DEL_ERROR;
                }
                else
                {
                    mRet[i] = delCount;
                }
            }
        }
        catch (const std::exception &ex)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << vtCond[i].mainKey << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            mRet[i] = DEL_ERROR;
        }
        catch (...)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << vtCond[i].mainKey << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            mRet[i] = DEL_ERROR;
        }

    }

    if (vtNeedDB.size() > 0)
    {
        DelBatchParamPtr delParam = new DelBatchParam(current, vtNeedDB.size(), mRet, mIndex);
        if (bGetRoute)
        {
            delParam->updateServant = updateServant;
        }

        for (unsigned int i = 0; i < vtNeedDB.size(); i++)
        {
            MKDbAccessCBParam::DelCBParamForBatch &param = vtNeedDB[i];
            param.pParam = delParam;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(param.mk, isCreate);
            int iAddret = pCBParam->AddDelForBatch(param);
            if (iAddret == 0)
            {
                current->setResponse(false);
                if (isCreate)
                {
                    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": async select db, mainKey = " << param.mk << endl);
                    DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                    //异步调用DBAccess
                    asyncDbSelect(param.mk, cb);
                }
                else
                {
                    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": set into cbparam, mainKey = " << param.mk << endl);
                }
            }
            else
            {
                if (param.bUKey)
                {
                    int delCount = 0;
                    int iRet = procDelUKVer(current, param.mk, param.vtUKCond, param.vtValueCond, param.stLimit, vtCond[i].ver, delCount);
                    if (iRet < 0)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << param.mk << "|failed|procDelUK error, ret = " << iRet << endl);
                        delParam->addFailIndexReason(mIndex[param.mk], DEL_ERROR);
                        continue;
                    }
                    else if (iRet == 1)
                    {
                        TLOGERROR("[MKWCacheImp::" << __FUNCTION__ << "] del second error! " << mIndex[param.mk] << endl);
                        delParam->addFailIndexReason(mIndex[param.mk], DEL_ERROR);
                        continue;
                    }

                    delParam->addFailIndexReason(mIndex[param.mk], delCount);
                }
                else
                {
                    int delCount = 0;
                    int iRet = procDelMKVer(current, param.mk, param.vtUKCond, param.vtValueCond, param.stLimit, delCount);
                    if (iRet < 0)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << "|" << param.mk << "|failed|procDelMK error, ret = " << iRet << endl);
                        delParam->addFailIndexReason(mIndex[param.mk], DEL_ERROR);
                        continue;
                    }
                    else if (iRet == 1)
                    {
                        TLOGERROR("[MKWCacheImp::" << __FUNCTION__ << "] del second error! " << mIndex[param.mk] << endl);
                        delParam->addFailIndexReason(mIndex[param.mk], DEL_ERROR);
                        continue;
                    }

                    delParam->addFailIndexReason(mIndex[param.mk], delCount);
                }
            }
        }
    }
    else
    {
        if (bGetRoute && updateServant.mpServant.size() > 0)
        {
            map<string, string> rspContext;
            rspContext[ROUTER_UPDATED] = "";
            RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
            current->setResponseContext(rspContext);
        }
    }

    return ET_SUCC;
}

tars::Int32 MKWCacheImp::pushList(const DCache::PushListReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const vector<DCache::InsertKeyValue> & mpValue = req.data;
    bool bHead = req.atHead;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << " bHead:" << bHead << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        vector<pair<uint32_t, string> > vtValue;
        vtValue.resize(mpValue.size());
        for (unsigned int i = 0; i < mpValue.size(); i++)
        {
            map<std::string, DCache::UpdateValue> mpJValue;

            g_app.ppReport(PPReport::SRP_SET_CNT, 1);
            int iRetCode;
            if (!checkMK(mainKey, _mkIsInteger, iRetCode))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
                return iRetCode;
            }
            if (!checkSetValue(mpValue[i].mpValue, mpJValue, iRetCode))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
                return iRetCode;
            }

            TarsEncode vEncode;
            for (size_t j = 0; j < _fieldConf.vtValueName.size(); j++)
            {
                const string &sValueName = _fieldConf.vtValueName[j];
                const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
                vEncode.write(mpJValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
            }

            vtValue[i].second.assign(vEncode.getBuffer(), vEncode.getLength());
            vtValue[i].first = mpValue[i].expireTimeSecond;
        }

        int iRet = g_HashMap.pushList(mainKey, vtValue, bHead, false, 0, 0);

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (_recordBinLog)
            WriteBinLog::pushList(mainKey, vtValue, bHead, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::pushList(mainKey, vtValue, bHead, _keyBinlogFile);

    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::popList(const DCache::PopListReq &req, DCache::PopListRsp &rsp, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    bool bHead = req.atHead;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << " bHead:" << bHead << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        string value;
        uint64_t delSize = 0;
        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.trimList(mainKey, true, bHead, false, 0, 0, TC_TimeProvider::getInstance()->getNow(), value, delSize);
        else
            iRet = g_HashMap.trimList(mainKey, true, bHead, false, 0, 0, 0, value, delSize);

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
        {
            vector<string> vtField;
            TarsDecode vDecode;
            vDecode.setBuffer(value);
            vector<map<std::string, std::string> > vtData;
            setResult(vtField, mainKey, vDecode, 0, 0, vtData);

            rsp.entry.data = vtData[0];
        }

        if (_recordBinLog)
            WriteBinLog::popList(mainKey, bHead, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::popList(mainKey, bHead, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;

}

tars::Int32 MKWCacheImp::replaceList(const DCache::ReplaceListReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const map<std::string, DCache::UpdateValue> & mpValue = req.data;
    int64_t iPos = req.index;
    int iExpireTime = req.expireTime;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        map<std::string, DCache::UpdateValue> mpJValue;

        g_app.ppReport(PPReport::SRP_SET_CNT, 1);
        int iRetCode;
        if (!checkMK(mainKey, _mkIsInteger, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }
        if (!checkSetValue(mpValue, mpJValue, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }

        TarsEncode vEncode;
        for (size_t i = 0; i < _fieldConf.vtValueName.size(); i++)
        {
            const string &sValueName = _fieldConf.vtValueName[i];
            const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
            vEncode.write(mpJValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
        }

        vector<pair<uint32_t, string> > vtvalue;
        vtvalue.push_back(make_pair(iExpireTime, string(vEncode.getBuffer(), vEncode.getLength())));

        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.pushList(mainKey, vtvalue, false, true, iPos, TC_TimeProvider::getInstance()->getNow());
        else
            iRet = g_HashMap.pushList(mainKey, vtvalue, false, true, iPos, 0);


        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (_recordBinLog)
            WriteBinLog::replaceList(mainKey, vtvalue[0].second, iPos, iExpireTime, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::replaceList(mainKey, vtvalue[0].second, iPos, iExpireTime, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::trimList(const DCache::TrimListReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    int64_t iStart = req.startIndex;
    int64_t iEnd = req.endIndex;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        string value;
        uint64_t delSize = 0;
        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.trimList(mainKey, false, false, true, iStart, iEnd, TC_TimeProvider::getInstance()->getNow(), value, delSize);
        else
            iRet = g_HashMap.trimList(mainKey, false, false, true, iStart, iEnd, 0, value, delSize);

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (_recordBinLog)
            WriteBinLog::trimList(mainKey, iStart, iEnd, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::trimList(mainKey, iStart, iEnd, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::remList(const DCache::RemListReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    bool bHead = req.atHead;
    int64_t iCount = req.count;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        string value;
        uint64_t delSize = 0;
        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.trimList(mainKey, false, bHead, false, 0, iCount, TC_TimeProvider::getInstance()->getNow(), value, delSize);
        else
            iRet = g_HashMap.trimList(mainKey, false, bHead, false, 0, iCount, 0, value, delSize);

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (_recordBinLog)
            WriteBinLog::remList(mainKey, bHead, iCount, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::remList(mainKey, bHead, iCount, _keyBinlogFile);

        return delSize;
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::addSet(const DCache::AddSetReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.value.mainKey;
    const map<std::string, DCache::UpdateValue> & mpValue = req.value.data;
    int iExpireTime = req.value.expireTime;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        map<std::string, DCache::UpdateValue> mpJValue;

        g_app.ppReport(PPReport::SRP_SET_CNT, 1);
        int iRetCode;
        if (!checkMK(mainKey, _mkIsInteger, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }
        if (!checkSetValue(mpValue, mpJValue, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }

        TarsEncode vEncode;
        for (size_t i = 0; i < _fieldConf.vtValueName.size(); i++)
        {
            const string &sValueName = _fieldConf.vtValueName[i];
            const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
            vEncode.write(mpJValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
        }

        string value;
        value.assign(vEncode.getBuffer(), vEncode.getLength());

        int iRet = g_HashMap.addSet(mainKey, value, iExpireTime, 0, true, TC_Multi_HashMap_Malloc::DELETE_FALSE);

        if (iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXIST)
            return ET_DATA_EXIST;

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (_recordBinLog)
            WriteBinLog::addSet(mainKey, value, iExpireTime, true, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::addSet(mainKey, value, iExpireTime, true, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::delSet(const DCache::DelSetReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const vector<DCache::Condition> & vtCond = req.cond;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        vector<DCache::Condition> vtValueCond;

        bool bunique;
        int iRetCode;
        if (!checkValueCondition(vtCond, vtValueCond, bunique, iRetCode))
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }

        if (!bunique)
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return ET_PARAM_MISSING;
        }

        TarsEncode vEncode;
        for (size_t i = 0; i < _fieldConf.vtValueName.size(); i++)
        {
            const DCache::Condition &vCond = vtValueCond[i];
            const FieldInfo &fileInfo = _fieldConf.mpFieldInfo[vCond.fieldName];
            vEncode.write(vCond.value, fileInfo.tag, fileInfo.type);
        }
        string value;
        value.assign(vEncode.getBuffer(), vEncode.getLength());

        int iRet = g_HashMap.delSetSetBit(mainKey, value, time(NULL));

        switch (iRet)
        {
        case TC_Multi_HashMap_Malloc::RT_OK:
        {
        }
        break;
        case TC_Multi_HashMap_Malloc::RT_ONLY_KEY:
        {
            TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.delSetSetBit return RT_ONLY_KEY, mainKey = " << mainKey << endl);
            g_app.gstat()->hit(_hitIndex);
        }
        break;
        case TC_Multi_HashMap_Malloc::RT_NO_DATA:
        {
            iRet = g_HashMap.checkMainKey(mainKey);
            if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
            {
            }
            else if (_existDB  && _readDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA))
            {
                bool isCreate;
                MKDbAccessCBParam::DelSetCBParam param;
                param.current = current;
                param.mk = mainKey;
                param.value = value;
                MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mainKey, isCreate);
                int iAddret = pCBParam->AddDelSet(param);
                if (iAddret == 0)
                {
                    current->setResponse(false);
                    if (isCreate)
                    {
                        TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": async select db, mainKey = " << mainKey << endl);
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, TC_Multi_HashMap_Malloc::MainKey::SET_TYPE);
                        //异步调用DBAccess
                        asyncDbSelect(mainKey, cb);
                        return 1;
                    }
                    else
                    {
                        TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": set into cbparam, mainKey = " << mainKey << endl);
                        return 1;
                    }
                }
                else
                {
                    g_app.gstat()->tryHit(_hitIndex);
                    int iGetRet = g_HashMap.delSetSetBit(mainKey, value, time(NULL));

                    if (iGetRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {

                    }
                    else if ((iGetRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iGetRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                    {
                        g_app.gstat()->hit(_hitIndex);
                    }
                    else
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": error, ret = " << iGetRet << ", mainKey = " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        return ET_SYS_ERR;
                    }
                }
            }

        }
        break;
        case TC_Multi_HashMap_Malloc::RT_DATA_DEL:
        {
            TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.delSetSetBit return RT_DATA_DEL, mainKey = " << mainKey << endl);
            g_app.gstat()->hit(_hitIndex);
        }
        break;
        default:
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.delSetSetBit error, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
        }

        if (_recordBinLog)
            WriteBinLog::delSet(mainKey, value, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::delSet(mainKey, value, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::addZSet(const DCache::AddZSetReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.value.mainKey;
    const map<std::string, DCache::UpdateValue> & mpValue = req.value.data;
    int iExpireTime = req.value.expireTime;
    double score = req.score;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << " score:" << score << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        map<std::string, DCache::UpdateValue> mpJValue;

        g_app.ppReport(PPReport::SRP_SET_CNT, 1);
        int iRetCode;
        if (!checkMK(mainKey, _mkIsInteger, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }
        if (!checkSetValue(mpValue, mpJValue, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }

        TarsEncode vEncode;
        for (size_t i = 0; i < _fieldConf.vtValueName.size(); i++)
        {
            const string &sValueName = _fieldConf.vtValueName[i];
            const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
            vEncode.write(mpJValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
        }

        string value;
        value.assign(vEncode.getBuffer(), vEncode.getLength());

        int iRet = g_HashMap.addZSet(mainKey, value, score, iExpireTime, 0, true, false, TC_Multi_HashMap_Malloc::DELETE_FALSE);

        if (iRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            return ET_MEM_FULL;
        }
        else if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (_recordBinLog)
            WriteBinLog::addZSet(mainKey, value, score, iExpireTime, true, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::addZSet(mainKey, value, score, iExpireTime, true, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::incScoreZSet(const DCache::IncZSetScoreReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.value.mainKey;
    const map<std::string, DCache::UpdateValue> & mpValue = req.value.data;
    int iExpireTime = req.value.expireTime;
    double score = req.scoreDiff;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        map<std::string, DCache::UpdateValue> mpJValue;

        g_app.ppReport(PPReport::SRP_SET_CNT, 1);
        int iRetCode;
        if (!checkMK(mainKey, _mkIsInteger, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }
        if (!checkSetValue(mpValue, mpJValue, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }

        TarsEncode vEncode;
        for (size_t i = 0; i < _fieldConf.vtValueName.size(); i++)
        {
            const string &sValueName = _fieldConf.vtValueName[i];
            const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
            vEncode.write(mpJValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
        }

        string value;
        value.assign(vEncode.getBuffer(), vEncode.getLength());

        int iRet = g_HashMap.addZSet(mainKey, value, score, iExpireTime, 0, true, true, TC_Multi_HashMap_Malloc::DELETE_FALSE);

        if (iRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            return ET_MEM_FULL;
        }
        else if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (_recordBinLog)
            WriteBinLog::incScoreZSet(mainKey, value, score, iExpireTime, true, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::incScoreZSet(mainKey, value, score, iExpireTime, true, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKWCacheImp::delZSet(const DCache::DelZSetReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const vector<DCache::Condition> & vtCond = req.cond;
    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        vector<DCache::Condition> vtValueCond;

        bool bunique;
        int iRetCode;
        if (!checkValueCondition(vtCond, vtValueCond, bunique, iRetCode))
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": param error, ret = " << iRetCode << endl);
            return iRetCode;
        }

        if (!bunique)
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return ET_PARAM_MISSING;
        }

        TarsEncode vEncode;
        for (size_t i = 0; i < _fieldConf.vtValueName.size(); i++)
        {
            const DCache::Condition &vCond = vtValueCond[i];
            const FieldInfo &fileInfo = _fieldConf.mpFieldInfo[vCond.fieldName];
            vEncode.write(vCond.value, fileInfo.tag, fileInfo.type);
        }
        string value;
        value.assign(vEncode.getBuffer(), vEncode.getLength());

        int iRet = g_HashMap.delZSetSetBit(mainKey, value, time(NULL));

        switch (iRet)
        {
        case TC_Multi_HashMap_Malloc::RT_OK:
        {
        }
        break;
        case TC_Multi_HashMap_Malloc::RT_ONLY_KEY:
        {
            TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.delZSetSetBit return RT_ONLY_KEY, mainKey = " << mainKey << endl);
            g_app.gstat()->hit(_hitIndex);
        }
        break;
        case TC_Multi_HashMap_Malloc::RT_NO_DATA:
        {
            iRet = g_HashMap.checkMainKey(mainKey);
            if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
            {
            }
            else if (_existDB  && _readDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA))
            {
                bool isCreate;
                MKDbAccessCBParam::DelZSetCBParam param;
                param.current = current;
                param.mk = mainKey;
                param.value = value;
                MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mainKey, isCreate);
                int iAddret = pCBParam->AddDelZSet(param);
                if (iAddret == 0)
                {
                    current->setResponse(false);
                    if (isCreate)
                    {
                        TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": async select db, mainKey = " << mainKey << endl);
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE);
                        //异步调用DBAccess
                        asyncDbSelect(mainKey, cb);
                        return 1;
                    }
                    else
                    {
                        TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": set into cbparam, mainKey = " << mainKey << endl);
                        return 1;
                    }
                }
                else
                {
                    g_app.gstat()->tryHit(_hitIndex);
                    int iGetRet = g_HashMap.delZSetSetBit(mainKey, value, time(NULL));

                    if (iGetRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {

                    }
                    else if ((iGetRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iGetRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                    {
                        g_app.gstat()->hit(_hitIndex);
                    }
                    else
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": error, ret = " << iGetRet << ", mainKey = " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        return ET_SYS_ERR;
                    }
                }
            }

        }
        break;
        case TC_Multi_HashMap_Malloc::RT_DATA_DEL:
        {
            TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.delSetSetBit return RT_DATA_DEL, mainKey = " << mainKey << endl);
            g_app.gstat()->hit(_hitIndex);
        }
        break;
        default:
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.delSetSetBit error, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
        }

        if (_recordBinLog)
            WriteBinLog::delZSet(mainKey, value, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::delZSet(mainKey, value, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}


tars::Int32 MKWCacheImp::delZSetByScore(const DCache::DelZSetByScoreReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    double iMin = req.minScore;
    double iMax = req.maxScore;

    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        int iRet = g_HashMap.checkMainKey(mainKey);
        if (_existDB  && _readDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA))
        {
            bool isCreate;
            MKDbAccessCBParam::DelRangeZSetCBParam param;
            param.current = current;
            param.mk = mainKey;
            param.iMin = iMin;
            param.iMax = iMax;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mainKey, isCreate);
            int iAddret = pCBParam->AddDelRangeZSet(param);
            if (iAddret == 0)
            {
                current->setResponse(false);
                if (isCreate)
                {
                    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": async select db, mainKey = " << mainKey << endl);
                    DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE);
                    //异步调用DBAccess
                    asyncDbSelect(mainKey, cb);
                    return 1;
                }
                else
                {
                    TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": set into cbparam, mainKey = " << mainKey << endl);
                    return 1;
                }
            }
            else
            {
                g_app.gstat()->tryHit(_hitIndex);
                int iRet;
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.delRangeZSetSetBit(mainKey, iMin, iMax, TC_TimeProvider::getInstance()->getNow(), time(NULL));
                else
                    iRet = g_HashMap.delRangeZSetSetBit(mainKey, iMin, iMax, 0, time(NULL));

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {

                }
                else if ((iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                {
                    g_app.gstat()->hit(_hitIndex);
                }
                else
                {
                    TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": error, ret = " << iRet << ", mainKey = " << mainKey << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_SYS_ERR;
                }

                if (_recordBinLog)
                    WriteBinLog::delRangeZSet(mainKey, iMin, iMax, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::delRangeZSet(mainKey, iMin, iMax, _keyBinlogFile);

                return ET_SUCC;
            }
        }

        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.delRangeZSetSetBit(mainKey, iMin, iMax, TC_TimeProvider::getInstance()->getNow(), time(NULL));
        else
            iRet = g_HashMap.delRangeZSetSetBit(mainKey, iMin, iMax, 0, time(NULL));

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }

        if (_recordBinLog)
            WriteBinLog::delRangeZSet(mainKey, iMin, iMax, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::delRangeZSet(mainKey, iMin, iMax, _keyBinlogFile);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32  MKWCacheImp::updateZSet(const DCache::UpdateZSetReq &req, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.value.mainKey;
    const map<std::string, DCache::UpdateValue> & mpValue = req.value.data;
    int iExpireTime = req.value.expireTime;
    bool bDirty = req.value.dirty;
    const vector<DCache::Condition> & vtCond = req.cond;
    char iVersion = 0;

    string sLogValue = FormatLog::tostr(mpValue);
    string sLogCond = FormatLog::tostr(vtCond);

    try
    {
        if (g_app.gstat()->serverType() != MASTER)
        {
            //SLAVE状态下不提供接口服务
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": ServerType is not Master" << endl);
            return ET_SERVER_TYPE_ERR;
        }
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        if (g_route_table.isTransfering(mainKey))
        {
            int iPageNo = g_route_table.getPageNo(mainKey);
            if (isTransSrc(iPageNo))
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " forbid erase" << endl);
                return ET_FORBID_OPT;
            }
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        g_app.ppReport(PPReport::SRP_SET_CNT, 1);
        bool bOnlyScore = false;
        int iRetCode;
        if (!checkUpdateValue(mpValue, bOnlyScore, iRetCode) || !checkMK(mainKey, _mkIsInteger, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }

        if (!bDirty)
        {
            if (_existDB)
                bDirty = true;
        }

        vector<DCache::Condition> vtValueCond;
        bool bunique;
        if (!checkValueCondition(vtCond, vtValueCond, bunique, iRetCode))
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": condition param error" << endl);
            return iRetCode;
        }
        if (!bunique)
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": condition param error" << endl);
            return ET_PARAM_MISSING;
        }

        TarsEncode vEncode;
        for (size_t i = 0; i < _fieldConf.vtValueName.size(); i++)
        {
            const DCache::Condition &vCond = vtValueCond[i];
            const FieldInfo &fileInfo = _fieldConf.mpFieldInfo[vCond.fieldName];
            vEncode.write(vCond.value, fileInfo.tag, fileInfo.type);
        }
        string value;
        value.assign(vEncode.getBuffer(), vEncode.getLength());

        int iRet;
        TC_Multi_HashMap_Malloc::Value vData;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.getZSet(mainKey, value, TC_TimeProvider::getInstance()->getNow(), vData);
        else
            iRet = g_HashMap.getZSet(mainKey, value, 0, vData);

        if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
        {
            g_app.gstat()->hit(_hitIndex);
            int iUpdateRet = updateResult(mainKey, value, mpValue, vData._score, vData._iExpireTime, iExpireTime, iVersion, bDirty, bOnlyScore, _binlogFile, _keyBinlogFile, _recordBinLog, _recordKeyBinLog);
            if (iUpdateRet != 0)
            {
                TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": update error" << endl);
                return ET_SYS_ERR;
            }
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            g_app.gstat()->hit(_hitIndex);
            return iRet;
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
        {
            if (_existDB  && _readDB)
            {
                MKDbAccessCBParam::UpdateZSetCBParam param;
                param.current = current;
                param.mk = mainKey;
                param.mpValue = mpValue;
                param.vtValueCond = vtValueCond;
                param.iExpireTime = iExpireTime;
                param.iVersion = iVersion;
                param.bDirty = bDirty;
                param.bOnlyScore = bOnlyScore;

                bool isCreate;
                MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mainKey, isCreate);
                int iAddret = pCBParam->AddUpdateZSet(param);
                if (iAddret == 0)
                {
                    current->setResponse(false);
                    if (isCreate)
                    {
                        TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": async select db, mainKey = " << mainKey << endl);
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE);
                        //异步调用DBAccess
                        asyncDbSelect(mainKey, cb);
                    }
                    else
                    {
                        TLOGDEBUG("MKWCacheImp::" << __FUNCTION__ << ": set into cbparam, mainKey = " << mainKey << endl);
                    }
                    return 0;
                }
                else
                {
                    g_app.gstat()->tryHit(_hitIndex);
                    int iGetRet;

                    if (g_app.gstat()->isExpireEnabled())
                        iGetRet = g_HashMap.getZSet(mainKey, value, TC_TimeProvider::getInstance()->getNow(), vData);
                    else
                        iGetRet = g_HashMap.getZSet(mainKey, value, 0, vData);

                    if (iGetRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        g_app.gstat()->hit(_hitIndex);
                        int iUpdateRet = updateResult(mainKey, value, mpValue, vData._score, vData._iExpireTime, iExpireTime, iVersion, bDirty, bOnlyScore, _binlogFile, _keyBinlogFile, _recordBinLog, _recordKeyBinLog);
                        if (iUpdateRet != 0)
                        {
                            return ET_SYS_ERR;
                        }
                    }
                    else if ((iGetRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED) || (iGetRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                    {
                        g_app.gstat()->hit(_hitIndex);
                        return iGetRet;
                    }
                    else if (iGetRet != TC_Multi_HashMap_Malloc::RT_NO_DATA)
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << ": g_HashMap.getZSet error, ret = " << iGetRet << ", mainKey = " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_NO_DATA;
                    }
                    else
                    {
                        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.getZSet error, ret = " << iGetRet << ", mainKey = " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
            }
            else
            {
                return ET_NO_DATA;
            }
        }
        else
        {
            TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " g_HashMap.getZSet error, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKWCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

void MKWCacheImp::asyncDbSelect(const string &mainKey, DbAccessPrxCallbackPtr cb)
{
    vector<DbCondition> vtDbCond;
    DbCondition cond;
    cond.fieldName = _fieldConf.sMKeyName;
    cond.op = DCache::EQ;
    cond.value = mainKey;
    cond.type = ConvertDbType(_fieldConf.mpFieldInfo[_fieldConf.sMKeyName].type);
    vtDbCond.push_back(cond);
    _dbaccessPrx->async_select(cb, mainKey, "*", vtDbCond);
}

int MKWCacheImp::procUpdateUK(tars::TarsCurrentPtr current, const string &mk, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, char ver, bool dirty, uint32_t expireTimeSecond, int &iUpdateCount, bool insert)
{
    string uk;
    MultiHashMap::Value v;
    bool bExist = false;
    TarsEncode uKeyEncode;
    size_t KeyLengthInDB = 0;
    g_app.ppReport(PPReport::SRP_SET_CNT, 1);
    int lenthInDB = 0;
    for (size_t i = 0; i < vtUKCond.size(); i++)
    {
        const Condition &cond = vtUKCond[i];
        const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[cond.fieldName];
        lenthInDB = fieldInfo.lengthInDB;
        uKeyEncode.write(cond.value, fieldInfo.tag, fieldInfo.type);
        if (lenthInDB != 0)
        {
            //如果ukey是string类型，并且配置中有限制长度，则比较长度
            if (fieldInfo.type == "string")
            {
                size_t iVkeyLength = cond.value.size();
                if (int(iVkeyLength) > lenthInDB)
                {
                    TLOGERROR("procUpdateUK string uk:" << cond.fieldName << " length:" << iVkeyLength << " lengthLimit:" << lenthInDB << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    return ET_PARAM_TOO_LONG;
                }
            }
        }

        KeyLengthInDB += cond.value.size();
    }
    KeyLengthInDB += mk.size();
    if (KeyLengthInDB > _maxKeyLengthInDB)
    {
        TLOGERROR("MKWCacheImp::update keyLength > " << _maxKeyLengthInDB << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return ET_PARAM_TOO_LONG;
    }

    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

    g_app.gstat()->tryHit(_hitIndex);
    int iRet;
    if (g_app.gstat()->isExpireEnabled())
    {
        iRet = g_HashMap.get(mk, uk, v, true, TC_TimeProvider::getInstance()->getNow());
    }
    else
    {
        iRet = g_HashMap.get(mk, uk, v);
    }


    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
    {
        g_app.gstat()->hit(_hitIndex);
        bExist = true;
        TarsDecode vDecode;

        vDecode.setBuffer(v._value);


        if (judge(vDecode, vtValueCond))
        {
            if (!stLimit.bLimit || (stLimit.bLimit && stLimit.iIndex == 0 && stLimit.iCount >= 1))
            {
                string value;
                value = updateValue(mpValue, v._value);

                if (!dirty)
                {
                    iRet = g_HashMap.checkDirty(mk, uk);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_DIRTY_DATA)
                    {
                        dirty = true;
                    }
                }

                int iSetRet = g_HashMap.set(mk, uk, value, expireTimeSecond, ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);


                if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                    {
                        TLOGERROR("MKWCacheImp::update g_HashMap.set ver mismatch" << endl);
                        return ET_DATA_VER_MISMATCH;
                    }
                    else
                    {
                        TLOGERROR("MKWCacheImp::update g_HashMap.set error, ret = " << iSetRet << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        return ET_SYS_ERR;
                    }
                }
                iUpdateCount++;

                if (_recordBinLog)
                    WriteBinLog::set(mk, uk, value, expireTimeSecond, dirty, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::set(mk, uk, _keyBinlogFile);
            }
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
    {
        g_app.gstat()->hit(_hitIndex);
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
    {
        if (_existDB  && _readDB)
        {
            MKDbAccessCBParam::UpdateCBParam param;
            param.current = current;
            param.mk = mk;
            param.mpValue = mpValue;
            param.bUKey = true;
            param.vtUKCond = vtUKCond;
            param.vtValueCond = vtValueCond;
            param.ver = ver;
            param.dirty = dirty;
            param.stLimit = stLimit;
            param.expireTime = expireTimeSecond;
            param.insert = insert;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
            int iAddret = pCBParam->AddUpdate(param);
            if (iAddret == 0)
            {
                current->setResponse(false);
                if (isCreate)
                {
                    TLOGDEBUG("MKWCacheImp::select: async select db, mainKey = " << mk << endl);
                    DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                    //异步调用DBAccess
                    asyncDbSelect(mk, cb);
                    return 1;
                }
                else
                {
                    TLOGDEBUG("MKWCacheImp::select: set into cbparam, mainKey = " << mk << endl);
                    return 1;
                }
            }
            else
            {
                g_app.gstat()->tryHit(_hitIndex);
                int iGetRet;
                if (g_app.gstat()->isExpireEnabled())
                {
                    iGetRet = g_HashMap.get(mk, uk, v, true, TC_TimeProvider::getInstance()->getNow());
                }
                else
                {
                    iGetRet = g_HashMap.get(mk, uk, v);
                }


                if (iGetRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    g_app.gstat()->hit(_hitIndex);
                    bExist = true;
                    TarsDecode vDecode;

                    vDecode.setBuffer(v._value);

                    if (judge(vDecode, vtValueCond))
                    {
                        if (!stLimit.bLimit || (stLimit.bLimit && stLimit.iIndex == 0 && stLimit.iCount >= 1))
                        {
                            string value;
                            value = updateValue(mpValue, v._value);

                            if (!dirty)
                            {
                                iRet = g_HashMap.checkDirty(mk, uk);

                                if (iRet == TC_Multi_HashMap_Malloc::RT_DIRTY_DATA)

                                {
                                    dirty = true;
                                }
                            }

                            int iSetRet = g_HashMap.set(mk, uk, value, expireTimeSecond, ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);


                            if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                                {
                                    TLOGERROR("MKWCacheImp::update g_HashMap.set ver mismatch" << endl);
                                    return ET_DATA_VER_MISMATCH;
                                }
                                else
                                {
                                    TLOGERROR("MKWCacheImp::update g_HashMap.set error, ret = " << iSetRet << endl);
                                    g_app.ppReport(PPReport::SRP_EX, 1);
                                    return ET_SYS_ERR;
                                }
                            }
                            iUpdateCount++;

                            if (_recordBinLog)
                                WriteBinLog::set(mk, uk, value, expireTimeSecond, dirty, _binlogFile);
                            if (_recordKeyBinLog)
                                WriteBinLog::set(mk, uk, _keyBinlogFile);
                        }
                    }
                }
                else if (iGetRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iGetRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iGetRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    g_app.gstat()->hit(_hitIndex);
                }
                else
                {
                    if (iGetRet != TC_Multi_HashMap_Malloc::RT_NO_DATA)
                    {
                        TLOGERROR("MKWCacheImp::update: g_HashMap.get(mk, uk, v) error, ret = " << iGetRet << ", mainKey = " << mk << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
            }
        }
    }
    else
    {
        TLOGERROR("MKWCacheImp::update g_HashMap.get error, ret = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return ET_SYS_ERR;
    }

    if (!bExist && insert)
    {
        if (vtValueCond.size() != 0)
        {
            TLOGERROR("MKWCacheImp::update data not exist, need insert but param error" << endl);
            return ET_INPUT_PARAM_ERROR;
        }

        map<string, DCache::UpdateValue> mpJValue;
        int iRetCode;
        if (!checkSetValue(mpValue, mpJValue, iRetCode))
        {
            TLOGERROR("MKWCacheImp::update data not exist, need insert but param error" << endl);
            return iRetCode;
        }

        TarsEncode vEncode;
        for (size_t i = 0; i < _fieldConf.vtValueName.size(); i++)
        {
            const string &sValueName = _fieldConf.vtValueName[i];
            const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[sValueName];
            vEncode.write(mpJValue[sValueName].value, fieldInfo.tag, fieldInfo.type);
        }

        string value;
        value.assign(vEncode.getBuffer(), vEncode.getLength());

        int iSetRet = g_HashMap.set(mk, uk, value, expireTimeSecond, 1, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);


        if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
        {
            if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
            {
                TLOGERROR("MKWCacheImp::update data not exist, need insert but g_HashMap.set ver mismatch, mk = " << mk << endl);
                return ET_DATA_VER_MISMATCH;
            }
            else if (iSetRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
            {
                TLOGERROR("MKWCacheImp::update data not exist, need insert but g_HashMap.set no memory, mk = " << mk << endl);
                return ET_MEM_FULL;
            }
            else
            {
                TLOGERROR("MKWCacheImp::update data not exist, need insert but g_HashMap.set error, ret = " << iSetRet << ", mk = " << mk << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                return ET_SYS_ERR;
            }

        }
        TLOGDEBUG("update data not exist, need insert, insert succ, mk = " << mk << endl);
        ++iUpdateCount;
        if (_recordBinLog)
            WriteBinLog::set(mk, uk, value, expireTimeSecond, dirty, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::set(mk, uk, _keyBinlogFile);
    }
    return 0;
}

int MKWCacheImp::procUpdateUKAtom(tars::TarsCurrentPtr current, const string &mk, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, bool dirty, uint32_t expireTimeSecond, int &iUpdateCount)
{
    string uk;
    MultiHashMap::Value v;
    TarsEncode uKeyEncode;
    size_t KeyLengthInDB = 0;
    g_app.ppReport(PPReport::SRP_SET_CNT, 1);
    int lenthInDB = 0;
    for (size_t i = 0; i < vtUKCond.size(); i++)
    {
        const Condition &cond = vtUKCond[i];
        const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[cond.fieldName];
        lenthInDB = fieldInfo.lengthInDB;
        uKeyEncode.write(cond.value, fieldInfo.tag, fieldInfo.type);
        if (lenthInDB != 0)
        {
            //如果ukey是string类型，并且配置中有限制长度，则比较长度
            if (fieldInfo.type == "string")
            {
                size_t iVkeyLength = cond.value.size();
                if (int(iVkeyLength) > lenthInDB)
                {
                    TLOGERROR("procUpdateUK string uk:" << cond.fieldName << " length:" << iVkeyLength << " lengthLimit:" << lenthInDB << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    return ET_PARAM_TOO_LONG;
                }
            }
        }

        KeyLengthInDB += cond.value.size();
    }
    KeyLengthInDB += mk.size();
    if (KeyLengthInDB > _maxKeyLengthInDB)
    {
        TLOGERROR("MKWCacheImp::update keyLength > " << _maxKeyLengthInDB << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return ET_PARAM_TOO_LONG;
    }

    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

    string value;

    int iRet;
    if (g_app.gstat()->isExpireEnabled())
    {
        iRet = g_HashMap.update(mk, uk, mpValue, vtValueCond, reinterpret_cast<TC_Multi_HashMap_Malloc::FieldConf*>(&_fieldConf), stLimit.bLimit, stLimit.iIndex, stLimit.iCount, value, true, TC_TimeProvider::getInstance()->getNow(), expireTimeSecond, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);
    }
    else
    {
        iRet = g_HashMap.update(mk, uk, mpValue, vtValueCond, reinterpret_cast<TC_Multi_HashMap_Malloc::FieldConf*>(&_fieldConf), stLimit.bLimit, stLimit.iIndex, stLimit.iCount, value, false, 0, expireTimeSecond, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);
    }


    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
    {
        g_app.gstat()->tryHit(_hitIndex);

        if (_recordBinLog)
            WriteBinLog::set(mk, uk, value, expireTimeSecond, dirty, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::set(mk, uk, _keyBinlogFile);

        iUpdateCount = 1;
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
    {
        g_app.gstat()->hit(_hitIndex);
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
    {
        if (_existDB  && _readDB)
        {
            MKDbAccessCBParam::UpdateCBParam param;
            param.current = current;
            param.mk = mk;
            param.mpValue = mpValue;
            param.bUKey = true;
            param.vtUKCond = vtUKCond;
            param.vtValueCond = vtValueCond;
            param.dirty = dirty;
            param.stLimit = stLimit;
            param.expireTime = expireTimeSecond;
            param.insert = false;
            param.atom = true;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
            int iAddret = pCBParam->AddUpdate(param);
            if (iAddret == 0)
            {
                current->setResponse(false);
                if (isCreate)
                {
                    TLOGDEBUG("MKWCacheImp::select: async select db, mainKey = " << mk << endl);
                    DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                    //异步调用DBAccess
                    asyncDbSelect(mk, cb);
                    return 1;
                }
                else
                {
                    TLOGDEBUG("MKWCacheImp::select: set into cbparam, mainKey = " << mk << endl);
                    return 1;
                }
            }
            else
            {
                string value;

                int iRet;
                if (g_app.gstat()->isExpireEnabled())
                {
                    iRet = g_HashMap.update(mk, uk, mpValue, vtValueCond, reinterpret_cast<TC_Multi_HashMap_Malloc::FieldConf*>(&_fieldConf), stLimit.bLimit, stLimit.iIndex, stLimit.iCount, value, true, TC_TimeProvider::getInstance()->getNow(), expireTimeSecond, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);
                }
                else
                {
                    iRet = g_HashMap.update(mk, uk, mpValue, vtValueCond, reinterpret_cast<TC_Multi_HashMap_Malloc::FieldConf*>(&_fieldConf), stLimit.bLimit, stLimit.iIndex, stLimit.iCount, value, false, 0, expireTimeSecond, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, _mkeyMaxDataCount, _deleteDirty);
                }


                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    g_app.gstat()->tryHit(_hitIndex);
                    if (_recordBinLog)
                        WriteBinLog::set(mk, uk, value, expireTimeSecond, dirty, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::set(mk, uk, _keyBinlogFile);
                }
                else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    g_app.gstat()->hit(_hitIndex);
                }
                else
                {
                    if (iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA)
                    {
                        TLOGERROR("MKWCacheImp::update: g_HashMap.get(mk, uk, v) error, ret = " << iRet << ", mainKey = " << mk << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
            }
        }
    }
    else
    {
        TLOGERROR("MKWCacheImp::update g_HashMap.get error, ret = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return ET_SYS_ERR;
    }

    return 0;
}

int MKWCacheImp::procUpdateMK(tars::TarsCurrentPtr current, const string &mk, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, char ver, bool dirty, uint32_t expireTimeSecond, int &iUpdateCount)
{
    vector<MultiHashMap::Value> vtValue;

    g_app.gstat()->tryHit(_hitIndex);
    int iRet;
    if (g_app.gstat()->isExpireEnabled())
    {
        iRet = g_HashMap.get(mk, vtValue, true, TC_TimeProvider::getInstance()->getNow());
    }
    else
    {
        iRet = g_HashMap.get(mk, vtValue);
    }


    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
    {
        g_app.gstat()->hit(_hitIndex);
        int iUpdateRet = updateResult(mk, vtValue, mpValue, vtUKCond, vtValueCond, stLimit, 0, dirty, expireTimeSecond, _insertAtHead, _updateInOrder, _binlogFile, _recordBinLog, _recordKeyBinLog, iUpdateCount);
        if (iUpdateRet != 0)
        {
            return ET_SYS_ERR;
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
    {
        g_app.gstat()->hit(_hitIndex);
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
    {
        if (_existDB  && _readDB)
        {
            MKDbAccessCBParam::UpdateCBParam param;
            param.current = current;
            param.mk = mk;
            param.mpValue = mpValue;
            param.bUKey = false;
            param.vtUKCond = vtUKCond;
            param.vtValueCond = vtValueCond;
            param.ver = ver;
            param.dirty = dirty;
            param.stLimit = stLimit;
            param.expireTime = expireTimeSecond;
            param.insert = false;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
            int iAddret = pCBParam->AddUpdate(param);
            if (iAddret == 0)
            {
                current->setResponse(false);
                if (isCreate)
                {
                    TLOGDEBUG("MKWCacheImp::update: async select db, mainKey = " << mk << endl);
                    DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                    //异步调用DBAccess
                    asyncDbSelect(mk, cb);
                    return 1;
                }
                else
                {
                    TLOGDEBUG("MKWCacheImp::update: set into cbparam, mainKey = " << mk << endl);
                    return 1;
                }
            }
            else
            {
                g_app.gstat()->tryHit(_hitIndex);
                vtValue.clear();
                int iGetRet;
                if (g_app.gstat()->isExpireEnabled())
                {
                    iGetRet = g_HashMap.get(mk, vtValue, true, TC_TimeProvider::getInstance()->getNow());
                }
                else
                {
                    iGetRet = g_HashMap.get(mk, vtValue);
                }


                if (iGetRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    g_app.gstat()->hit(_hitIndex);
                    int iUpdateRet = updateResult(mk, vtValue, mpValue, vtUKCond, vtValueCond, stLimit, 0, dirty, expireTimeSecond, _insertAtHead, _updateInOrder, _binlogFile, _recordBinLog, _recordKeyBinLog, iUpdateCount);
                    if (iUpdateRet != 0)
                    {
                        return ET_SYS_ERR;
                    }
                }
                else if ((iGetRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iGetRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                {
                    g_app.gstat()->hit(_hitIndex);
                }
                else
                {
                    TLOGERROR("MKWCacheImp::update g_HashMap.get error, ret = " << iGetRet << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    return ET_SYS_ERR;
                }
            }
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            g_app.gstat()->hit(_hitIndex);
            int iUpdateRet = updateResult(mk, vtValue, mpValue, vtUKCond, vtValueCond, stLimit, 0, dirty, expireTimeSecond, _insertAtHead, _updateInOrder, _binlogFile, _recordBinLog, _recordKeyBinLog, iUpdateCount);
            if (iUpdateRet != 0)
            {
                return ET_SYS_ERR;
            }
        }
    }
    else
    {
        TLOGERROR("MKWCacheImp::update g_HashMap.get error, ret = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return ET_SYS_ERR;
    }
    return 0;
}

int MKWCacheImp::procUpdateMKAtom(tars::TarsCurrentPtr current, const string &mk, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, bool dirty, uint32_t expireTimeSecond, int &iUpdateCount)
{
    vector<MultiHashMap::Value> vtValue;

    g_app.gstat()->tryHit(_hitIndex);
    int iRet;
    if (g_app.gstat()->isExpireEnabled())
    {
        iRet = g_HashMap.get(mk, vtValue, true, TC_TimeProvider::getInstance()->getNow());
    }
    else
    {
        iRet = g_HashMap.get(mk, vtValue);
    }


    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
    {
        g_app.gstat()->hit(_hitIndex);
        int iUpdateRet = updateResultAtom(mk, vtValue, mpValue, vtUKCond, vtValueCond, stLimit, 0, dirty, expireTimeSecond, _insertAtHead, _updateInOrder, _binlogFile, _recordBinLog, _recordKeyBinLog, iUpdateCount);
        if (iUpdateRet != 0)
        {
            return ET_SYS_ERR;
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
    {
        g_app.gstat()->hit(_hitIndex);
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
    {
        if (_existDB  && _readDB)
        {
            MKDbAccessCBParam::UpdateCBParam param;
            param.current = current;
            param.mk = mk;
            param.mpValue = mpValue;
            param.bUKey = false;
            param.vtUKCond = vtUKCond;
            param.vtValueCond = vtValueCond;
            param.ver = 0;
            param.dirty = dirty;
            param.stLimit = stLimit;
            param.expireTime = expireTimeSecond;
            param.insert = false;
            param.atom = true;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
            int iAddret = pCBParam->AddUpdate(param);
            if (iAddret == 0)
            {
                current->setResponse(false);
                if (isCreate)
                {
                    TLOGDEBUG("MKWCacheImp::update: async select db, mainKey = " << mk << endl);
                    DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                    //异步调用DBAccess
                    asyncDbSelect(mk, cb);
                    return 1;
                }
                else
                {
                    TLOGDEBUG("MKWCacheImp::update: set into cbparam, mainKey = " << mk << endl);
                    return 1;
                }
            }
            else
            {
                g_app.gstat()->tryHit(_hitIndex);
                vtValue.clear();
                int iGetRet;
                if (g_app.gstat()->isExpireEnabled())
                {
                    iGetRet = g_HashMap.get(mk, vtValue, true, TC_TimeProvider::getInstance()->getNow());
                }
                else
                {
                    iGetRet = g_HashMap.get(mk, vtValue);
                }


                if (iGetRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    g_app.gstat()->hit(_hitIndex);
                    int iUpdateRet = updateResultAtom(mk, vtValue, mpValue, vtUKCond, vtValueCond, stLimit, 0, dirty, expireTimeSecond, _insertAtHead, _updateInOrder, _binlogFile, _recordBinLog, _recordKeyBinLog, iUpdateCount);
                    if (iUpdateRet != 0)
                    {
                        return ET_SYS_ERR;
                    }
                }
                else if ((iGetRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iGetRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                {
                    g_app.gstat()->hit(_hitIndex);
                }
                else
                {
                    TLOGERROR("MKWCacheImp::update g_HashMap.get error, ret = " << iGetRet << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    return ET_SYS_ERR;
                }
            }
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            g_app.gstat()->hit(_hitIndex);
            int iUpdateRet = updateResultAtom(mk, vtValue, mpValue, vtUKCond, vtValueCond, stLimit, 0, dirty, expireTimeSecond, _insertAtHead, _updateInOrder, _binlogFile, _recordBinLog, _recordKeyBinLog, iUpdateCount);
            if (iUpdateRet != 0)
            {
                return ET_SYS_ERR;
            }
        }
    }
    else
    {
        TLOGERROR("MKWCacheImp::update g_HashMap.get error, ret = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return ET_SYS_ERR;
    }
    return 0;
}

int MKWCacheImp::procDelUK(tars::TarsCurrentPtr current, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit)
{
    g_app.ppReport(PPReport::SRP_SET_CNT, 1);
    string uk;
    MultiHashMap::Value v;

    TarsEncode uKeyEncode;
    for (size_t i = 0; i < vtUKCond.size(); i++)
    {
        uKeyEncode.write(vtUKCond[i].value, _fieldConf.mpFieldInfo[vtUKCond[i].fieldName].tag, _fieldConf.mpFieldInfo[vtUKCond[i].fieldName].type);
    }
    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

    if (vtValueCond.size() > 0)
    {
        g_app.gstat()->tryHit(_hitIndex);
        int iRet = g_HashMap.get(mk, uk, v);

        switch (iRet)
        {
        case TC_Multi_HashMap_Malloc::RT_OK:
        {
            g_app.gstat()->hit(_hitIndex);
            TarsDecode vDecode;

            vDecode.setBuffer(v._value);


            if (judge(vDecode, vtValueCond))
            {
                if (!stLimit.bLimit || (stLimit.bLimit && stLimit.iIndex == 0 && stLimit.iCount >= 1))
                {
                    int iDelRet = g_HashMap.delSetBit(mk, uk, time(NULL));

                    if ((iDelRet != TC_Multi_HashMap_Malloc::RT_OK) && (iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                    {
                        TLOGERROR("MKWCacheImp::del g_HashMap.del error, ret = " << iDelRet << ", mainKey = " << mk << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }

                    if (_recordBinLog)
                        WriteBinLog::del(mk, uk, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::del(mk, uk, _keyBinlogFile);
                }
            }
        }
        break;
        case TC_Multi_HashMap_Malloc::RT_ONLY_KEY:
        {
            TLOGDEBUG("MKWCacheImp::del g_HashMap.get return RT_ONLY_KEY, mainKey = " << mk << endl);
            g_app.gstat()->hit(_hitIndex);
        }
        break;
        case TC_Multi_HashMap_Malloc::RT_NO_DATA:
        {
            if (_existDB  && _readDB)
            {
                MKDbAccessCBParam::DelCBParam param;
                param.current = current;
                param.mk = mk;
                param.bUKey = true;
                param.vtUKCond = vtUKCond;
                param.vtValueCond = vtValueCond;
                param.stLimit = stLimit;
                param.pMKDBaccess = _dbaccessPrx;
                bool isCreate;
                MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
                int iAddret = pCBParam->AddDel(param);
                if (iAddret == 0)
                {
                    current->setResponse(false);
                    if (isCreate)
                    {
                        TLOGDEBUG("MKWCacheImp::del: async select db, mainKey = " << mk << endl);
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                        //异步调用DBAccess
                        asyncDbSelect(mk, cb);
                        return 1;
                    }
                    else
                    {
                        TLOGDEBUG("MKWCacheImp::del: set into cbparam, mainKey = " << mk << endl);
                        return 1;
                    }
                }
                else
                {
                    g_app.gstat()->tryHit(_hitIndex);
                    int iGetRet = g_HashMap.get(mk, uk, v);

                    if (iGetRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        g_app.gstat()->hit(_hitIndex);
                        TarsDecode vDecode;

                        vDecode.setBuffer(v._value);


                        if (judge(vDecode, vtValueCond))
                        {
                            if (!stLimit.bLimit || (stLimit.bLimit && stLimit.iIndex == 0 && stLimit.iCount >= 1))
                            {
                                int iDelRet = g_HashMap.delSetBit(mk, uk, time(NULL));

                                if ((iDelRet != TC_Multi_HashMap_Malloc::RT_OK) || (iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                                {
                                    TLOGERROR("MKWCacheImp::del g_HashMap.erase error, ret = " << iDelRet << endl);
                                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                                    return ET_SYS_ERR;
                                }

                                if (_recordBinLog)
                                    WriteBinLog::del(mk, uk, _binlogFile);
                                if (_recordKeyBinLog)
                                    WriteBinLog::del(mk, uk, _keyBinlogFile);
                            }
                        }
                    }
                    else if ((iGetRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iGetRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                    {
                        g_app.gstat()->hit(_hitIndex);
                    }
                    else
                    {
                        TLOGERROR("MKWCacheImp::del: g_HashMap.get(mk, uk, v) error, ret = " << iGetRet << ", mainKey = " << mk << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
            }

        }
        break;
        case TC_Multi_HashMap_Malloc::RT_DATA_DEL:
        {
            TLOGDEBUG("MKWCacheImp::del g_HashMap.get return RT_DATA_DEL, mainKey = " << mk << endl);
            g_app.gstat()->hit(_hitIndex);
        }
        break;
        default:
        {
            TLOGERROR("MKWCacheImp::del g_HashMap.get error, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
        }
    }
    else
    {
        if (!stLimit.bLimit || (stLimit.bLimit && stLimit.iIndex == 0 && stLimit.iCount >= 1))
        {
            int iDelRet = g_HashMap.delSetBit(mk, uk, time(NULL));

            if (iDelRet != TC_Multi_HashMap_Malloc::RT_OK
                    && iDelRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                    && iDelRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY
                    && iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
            {
                TLOGERROR("MKWCacheImp::del g_HashMap.del error, ret = " << iDelRet << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                return ET_SYS_ERR;
            }

            //有db并且没有数据，就插入一条onlykey
            if (_existDB && iDelRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
                g_HashMap.setForDel(mk, uk, time(NULL));

            if (_recordBinLog)
                WriteBinLog::del(mk, uk, _binlogFile);
            if (_recordKeyBinLog)
                WriteBinLog::del(mk, uk, _keyBinlogFile);

        }
    }
    return 0;
}

//供delBatch使用
int MKWCacheImp::procDelUKVer(tars::TarsCurrentPtr current, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, uint8_t iVersion, int &ret)
{
    ret = 0;
    g_app.ppReport(PPReport::SRP_SET_CNT, 1);
    string uk;
    MultiHashMap::Value v;

    TarsEncode uKeyEncode;
    for (size_t i = 0; i < vtUKCond.size(); i++)
    {
        uKeyEncode.write(vtUKCond[i].value, _fieldConf.mpFieldInfo[vtUKCond[i].fieldName].tag, _fieldConf.mpFieldInfo[vtUKCond[i].fieldName].type);
    }
    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

    if (vtValueCond.size() > 0)
    {
        g_app.gstat()->tryHit(_hitIndex);
        int iRet = g_HashMap.get(mk, uk, v);

        switch (iRet)
        {
        case TC_Multi_HashMap_Malloc::RT_OK:
        {
            g_app.gstat()->hit(_hitIndex);
            TarsDecode vDecode;

            vDecode.setBuffer(v._value);


            if (judge(vDecode, vtValueCond))
            {
                if (!stLimit.bLimit || (stLimit.bLimit && stLimit.iIndex == 0 && stLimit.iCount >= 1))
                {
                    int iDelRet = g_HashMap.delSetBit(mk, uk, iVersion, time(NULL));

                    if ((iDelRet != TC_Multi_HashMap_Malloc::RT_OK) && (iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL) &&
                            (iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH))
                    {
                        TLOGERROR("MKWCacheImp::del g_HashMap.del error, ret = " << iDelRet << ", mainKey = " << mk << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        ret = DEL_ERROR;
                        return -1;
                    }

                    if (iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                    {
                        if (_recordBinLog)
                            WriteBinLog::del(mk, uk, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::del(mk, uk, _keyBinlogFile);

                        if (iDelRet == TC_Multi_HashMap_Malloc::RT_OK)
                            ret = 1;
                    }
                    else
                        ret = DEL_DATA_VER_MISMATCH;

                }
            }
        }
        break;
        case TC_Multi_HashMap_Malloc::RT_ONLY_KEY:
        {
            TLOGDEBUG("MKWCacheImp::del g_HashMap.get return RT_ONLY_KEY, mainKey = " << mk << endl);
            g_app.gstat()->hit(_hitIndex);
        }
        break;
        case TC_Multi_HashMap_Malloc::RT_NO_DATA:
        {
            if (_existDB  && _readDB)
            {
                return 1;
            }

        }
        break;
        case TC_Multi_HashMap_Malloc::RT_DATA_DEL:
        {
            TLOGDEBUG("MKWCacheImp::del g_HashMap.get return RT_DATA_DEL, mainKey = " << mk << endl);
            g_app.gstat()->hit(_hitIndex);
        }
        break;
        default:
        {
            TLOGERROR("MKWCacheImp::del g_HashMap.get error, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            ret = DEL_ERROR;
        }
        }
    }
    else
    {
        if (!stLimit.bLimit || (stLimit.bLimit && stLimit.iIndex == 0 && stLimit.iCount >= 1))
        {
            int iDelRet = g_HashMap.delSetBit(mk, uk, iVersion, time(NULL));

            if (iDelRet != TC_Multi_HashMap_Malloc::RT_OK
                    && iDelRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                    && iDelRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY
                    && iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL
                    && iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
            {
                TLOGERROR("MKWCacheImp::del g_HashMap.del error, ret = " << iDelRet << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                ret = DEL_ERROR;
                return ET_SYS_ERR;
            }

            if (iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
            {
                //有db并且没有数据，就插入一条onlykey
                if (_existDB && iDelRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
                    g_HashMap.setForDel(mk, uk, time(NULL));

                if (_recordBinLog)
                    WriteBinLog::del(mk, uk, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::del(mk, uk, _keyBinlogFile);

                if (iDelRet == TC_Multi_HashMap_Malloc::RT_OK)
                    ret = 1;
            }
            else
                ret = DEL_DATA_VER_MISMATCH;
        }
    }
    return 0;
}

int MKWCacheImp::procDelMK(tars::TarsCurrentPtr current, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit)
{
    g_app.gstat()->tryHit(_hitIndex);

    if ((!_existDB || g_HashMap.checkMainKey(mk) == TC_Multi_HashMap_Malloc::RT_OK) && !stLimit.bLimit && vtUKCond.size() == 0 && vtValueCond.size() == 0)
    {
        uint64_t delCount = 0;
        int iRet = g_HashMap.delSetBit(mk, time(NULL), delCount);

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK
                && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY
                && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::del g_HashMap.del error, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
        if (_recordBinLog)
            WriteBinLog::del(mk, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::del(mk, _keyBinlogFile);

        return 0;
    }
    vector<MultiHashMap::Value> vtValue;
    int iRet = g_HashMap.get(mk, vtValue);

    switch (iRet)
    {
    case TC_Multi_HashMap_Malloc::RT_OK:
    {
        g_app.gstat()->hit(_hitIndex);
        int iDelRet = DelResult(mk, vtValue, vtUKCond, vtValueCond, stLimit, _binlogFile, _recordBinLog, _recordKeyBinLog, current, _dbaccessPrx, _existDB);
        if (iDelRet != 0)
        {
            return ET_SYS_ERR;
        }
    }
    break;
    case TC_Multi_HashMap_Malloc::RT_ONLY_KEY:
    case TC_Multi_HashMap_Malloc::RT_DATA_DEL:
    {
        g_app.gstat()->hit(_hitIndex);
    }
    break;
    case TC_Multi_HashMap_Malloc::RT_NO_DATA:
    case TC_Multi_HashMap_Malloc::RT_PART_DATA:
    {
        if (_existDB  && _readDB)
        {
            MKDbAccessCBParam::DelCBParam param;
            param.current = current;
            param.mk = mk;
            param.bUKey = false;
            param.vtUKCond = vtUKCond;
            param.vtValueCond = vtValueCond;
            param.stLimit = stLimit;
            param.pMKDBaccess = _dbaccessPrx;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
            int iAddret = pCBParam->AddDel(param);
            if (iAddret == 0)
            {
                current->setResponse(false);
                if (isCreate)
                {
                    TLOGDEBUG("MKWCacheImp::del: async select db, mainKey = " << mk << endl);
                    DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                    //异步调用DBAccess
                    asyncDbSelect(mk, cb);
                    return 1;
                }
                else
                {
                    TLOGDEBUG("MKWCacheImp::del: set into cbparam, mainKey = " << mk << endl);
                    return 1;
                }
            }
            else
            {
                g_app.gstat()->tryHit(_hitIndex);
                vtValue.clear();
                int iGetRet = g_HashMap.get(mk, vtValue);

                if (iGetRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    g_app.gstat()->hit(_hitIndex);
                    int iDelRet = DelResult(mk, vtValue, vtUKCond, vtValueCond, stLimit, _binlogFile, _recordBinLog, _recordKeyBinLog, current, _dbaccessPrx, _existDB);
                    if (iDelRet != 0)
                    {
                        return ET_SYS_ERR;
                    }
                }
                else if ((iGetRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iGetRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                {
                    g_app.gstat()->hit(_hitIndex);
                }
                else
                {
                    TLOGERROR("MKWCacheImp::del: g_HashMap.get(mk, uk, v) error, ret = " << iGetRet << ", mainKey = " << mk << endl);
                    return ET_SYS_ERR;
                }
            }
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            g_app.gstat()->hit(_hitIndex);
            int iDelRet = DelResult(mk, vtValue, vtUKCond, vtValueCond, stLimit, _binlogFile, _recordBinLog, _recordKeyBinLog, current, _dbaccessPrx, _existDB);
            if (iDelRet != 0)
            {
                return ET_SYS_ERR;
            }
        }
    }
    break;
    default:
    {
        TLOGERROR("MKWCacheImp::del g_HashMap.get error, ret = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return ET_SYS_ERR;
    }
    }

    return 0;
}

//供delBatch使用
int MKWCacheImp::procDelMKVer(tars::TarsCurrentPtr current, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, int &ret)
{
    g_app.gstat()->tryHit(_hitIndex);

    if ((!_existDB || g_HashMap.checkMainKey(mk) == TC_Multi_HashMap_Malloc::RT_OK) && !stLimit.bLimit && vtUKCond.size() == 0 && vtValueCond.size() == 0)
    {
        uint64_t delCount = 0;
        int iRet = g_HashMap.delSetBit(mk, time(NULL), delCount);
        ret = delCount;

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK
                && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY
                && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKWCacheImp::procDelMKVer g_HashMap.del error, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
        if (_recordBinLog)
            WriteBinLog::del(mk, _binlogFile);
        if (_recordKeyBinLog)
            WriteBinLog::del(mk, _keyBinlogFile);

        return 0;
    }
    vector<MultiHashMap::Value> vtValue;
    int iRet = g_HashMap.get(mk, vtValue);

    switch (iRet)
    {
    case TC_Multi_HashMap_Malloc::RT_OK:
    {
        g_app.gstat()->hit(_hitIndex);
        int iDelRet = DelResult(mk, vtValue, vtUKCond, vtValueCond, stLimit, _binlogFile, _recordBinLog, _recordKeyBinLog, _dbaccessPrx, _existDB, ret);
        if (iDelRet != 0)
        {
            return ET_SYS_ERR;
        }
    }
    break;
    case TC_Multi_HashMap_Malloc::RT_ONLY_KEY:
    case TC_Multi_HashMap_Malloc::RT_DATA_DEL:
    {
        g_app.gstat()->hit(_hitIndex);
    }
    break;
    case TC_Multi_HashMap_Malloc::RT_NO_DATA:
    case TC_Multi_HashMap_Malloc::RT_PART_DATA:
    {
        if (_existDB  && _readDB)
        {
            return 1;
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            g_app.gstat()->hit(_hitIndex);
            int iDelRet = DelResult(mk, vtValue, vtUKCond, vtValueCond, stLimit, _binlogFile, _recordBinLog, _recordKeyBinLog, _dbaccessPrx, _existDB, ret);
            if (iDelRet != 0)
            {
                return ET_SYS_ERR;
            }
        }
    }
    break;
    default:
    {
        TLOGERROR("MKWCacheImp::procDelMKVer g_HashMap.get error, ret = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return ET_SYS_ERR;
    }
    }

    return 0;
}

bool MKWCacheImp::isTransSrc(int pageNo)
{
    ServerInfo srcServer;
    ServerInfo destServer;
    g_route_table.getTrans(pageNo, srcServer, destServer);

    if (srcServer.serverName == (ServerConfig::Application + "." + ServerConfig::ServerName))
        return true;
    return false;
}










