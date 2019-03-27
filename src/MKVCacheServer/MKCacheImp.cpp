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
#include "MKCacheImp.h"

using namespace std;

//////////////////////////////////////////////////////
void MKCacheImp::initialize()
{
    //initialize servant here:
    //...
    _config = ServerConfig::BasePath + "MKCacheServer.conf";
    _tcConf.parseFile(_config);

    TARS_ADD_ADMIN_CMD_NORMAL("reload", MKCacheImp::reloadConf);

    _moduleName = _tcConf["/Main<ModuleName>"];
    _binlogFile = _tcConf["/Main/BinLog<LogFile>"];
    _keyBinlogFile = _tcConf["/Main/BinLog<LogFile>"] + "key";
    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(_tcConf["/Main/DbAccess<ObjName>"]);

    _saveOnlyKey = (_tcConf["/Main/Cache<SaveOnlyKey>"] == "Y" || _tcConf["/Main/Cache<SaveOnlyKey>"] == "y") ? true : false;
    _readDB = (_tcConf["/Main/DbAccess<ReadDbFlag>"] == "Y" || _tcConf["/Main/DbAccess<ReadDbFlag>"] == "y") ? true : false;

    _existDB = (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y") ? true : false;

    _insertAtHead = (_tcConf["/Main/Cache<InsertOrder>"] == "Y" || _tcConf["/Main/Cache<InsertOrder>"] == "y") ? true : false;

    string sUpdateOrder = _tcConf.get("/Main/Cache<UpdateOrder>", "N");

    _updateInOrder = (sUpdateOrder == "Y" || sUpdateOrder == "y") ? true : false;

    _orderItem = _tcConf.get("/Main/Cache<OrderItem>", "");
    string sKeyType = _tcConf.get("/Main/Cache<MainKeyType>", "hash");
    if (sKeyType == "hash")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE;
    else if (sKeyType == "set")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::SET_TYPE;
    else if (sKeyType == "zset")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE;
    else if (sKeyType == "list")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::LIST_TYPE;
    else
        assert(false);

    string sOrderDesc = _tcConf.get("/Main/Cache<OrderDesc>", "Y");
    _orderByDesc = (sOrderDesc == "Y" || sOrderDesc == "y") ? true : false;

    _hitIndex = g_app.gstat()->genHitIndex();
    TLOGDEBUG("MKCacheImp::initialize _hitIndex:" << _hitIndex << endl);

    _mkeyMaxSelectCount = TC_Common::strto<size_t>(_tcConf.get("/Main<MKeyMaxBlockCount>", "20000"));
    g_app.gstat()->getFieldConfig(_fieldConf);
    TLOGDEBUG("MKCacheImp::initialize Succ" << endl);
}

//////////////////////////////////////////////////////
void MKCacheImp::destroy()
{
    //destroy servant here:
    //...
}

bool MKCacheImp::reloadConf(const string& command, const string& params, string& result)
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

    _mkeyMaxSelectCount = TC_Common::strto<size_t>(_tcConf.get("/Main<MKeyMaxBlockCount>", "20000"));

    TLOGDEBUG("MKCacheImp::reloadConf Succ" << endl);
    result = "SUCC";

    return true;
}

tars::Int32 MKCacheImp::getMKV(const DCache::GetMKVReq &req, DCache::GetMKVRsp &rsp, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const std::string & field = req.field;
    const vector<DCache::Condition> & vtCond = req.cond;
    bool bGetMKCout = req.retEntryCnt;
    vector<map<std::string, std::string> > &vtData = rsp.data;

    TLOGDEBUG("MKCacheImp::getMKV recv : " << mainKey << "|" << field << "|" << FormatLog::tostr(vtCond) << "|" << bGetMKCout << endl);
    int iMKRecord;
    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::getMKV: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::getMKV: " << mainKey << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(mainKey, false, context[API_IDC], updateServant);
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

        //计算vector预留的大小
        unsigned int iCondSize = vtCond.size();
        unsigned int iReseveSize;
        if (iCondSize < 50)
            iReseveSize = iCondSize;
        else
            iReseveSize = 50;

        vector<DCache::Condition> vtUKCond;
        vtUKCond.reserve(iReseveSize);
        vector<DCache::Condition> vtValueCond;
        vtValueCond.reserve(iReseveSize);

        Limit stLimit;
        bool bUKey;
        int iRetCode;

        if (!checkCondition(vtCond, vtUKCond, vtValueCond, stLimit, bUKey, iRetCode))
        {
            TLOGERROR("MKCacheImp::getMKV: param error" << endl);
            return iRetCode;
        }

        vector<string> vtField;
        convertField(field, vtField);

        if (bUKey)
        {
            int iRet = procSelectUK(current, vtField, mainKey, vtUKCond, vtValueCond, stLimit, bGetMKCout, vtData, iMKRecord);
            if (iRet < 0)
                return iRet;
            else if (iRet == 1)
                return 0;
        }
        else
        {
            int iRet = procSelectMK(current, vtField, mainKey, vtUKCond, vtValueCond, stLimit, bGetMKCout, vtData, iMKRecord);
            if (iRet < 0)
                return iRet;
            else if (iRet == 1)
                return 0;

        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::getMKV exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::getMKV unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return iMKRecord;
}

tars::Int32 MKCacheImp::getMKVBatch(const DCache::MKVBatchReq &req, DCache::MKVBatchRsp &rsp, tars::TarsCurrentPtr current)
{
    const vector<std::string> & vtMainKey = req.mainKeys;
    const string& field = req.field;
    const vector<DCache::Condition> & vtCond = req.cond;
    vector<DCache::MainKeyValue> & vtData = rsp.data;

    TLOGDEBUG("MKCacheImp::getMKVBatch recv : " << vtMainKey.size() << "|" << TC_Common::tostr(vtMainKey.begin(), vtMainKey.end(), ";") << "|" << field << "|" << FormatLog::tostr(vtCond) << endl);

    if (req.moduleName != _moduleName)
    {
        TLOGERROR("MKCacheImp::getMKVBatch: moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    //计算vector预留的大小
    unsigned int iCondSize = vtCond.size();
    unsigned int iReseveSize;
    if (iCondSize < 50)
        iReseveSize = iCondSize;
    else
        iReseveSize = 50;

    vector<string> vtField;
    vector<DCache::Condition> vtUKCond;
    vtUKCond.reserve(iReseveSize);
    vector<DCache::Condition> vtValueCond;
    vtValueCond.reserve(iReseveSize);

    Limit stLimit;
    bool bUKey;
    string sUK;
    size_t i = 0;
    vector<string> vtNeedDBAccessMainKey;
    vtNeedDBAccessMainKey.reserve(vtMainKey.size());

    try
    {
        int iRetCode;
        if (!checkCondition(vtCond, vtUKCond, vtValueCond, stLimit, bUKey, iRetCode))
        {
            TLOGERROR("MKCacheImp::getMKVBatch: param error" << endl);
            return iRetCode;
        }

        convertField(field, vtField);

        size_t MainkeyCount = vtMainKey.size();
        vtData.reserve(MainkeyCount);

        if (bUKey)
        {
            TarsEncode uKeyEncode;
            for (i = 0; i < vtUKCond.size(); ++i)
            {
                const DCache::Condition &cond = vtUKCond[i];
                const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[cond.fieldName];
                uKeyEncode.write(cond.value, fieldInfo.tag, fieldInfo.type);
            }
            sUK.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());





            for (i = 0; i < MainkeyCount; ++i)
            {
                if (!g_route_table.isMySelf(vtMainKey[i]))
                {
                    TLOGERROR("MKCacheImp::getMKVBatch: " << vtMainKey[i] << " is not in self area" << endl);
                    vtData.clear();
                    map<string, string>& context = current->getContext();
                    //API直连模式，返回增量更新路由
                    if (VALUE_YES == context[GET_ROUTE])
                    {
                        RspUpdateServant updateServant;
                        map<string, string> rspContext;
                        rspContext[ROUTER_UPDATED] = "";
                        int ret = RouterHandle::getInstance()->getUpdateServant(vtMainKey, false, context[API_IDC], updateServant);
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

                DCache::MainKeyValue keyValue;
                tars::Int32 iRet = selectUKCache(current, vtField, vtMainKey[i], sUK, vtUKCond, vtValueCond, stLimit, false, keyValue.value, keyValue.ret);
                if (iRet != 0)
                {
                    if (iRet == 1)
                    {
                        vtNeedDBAccessMainKey.push_back(vtMainKey[i]);
                    }
                    else
                    {
                        TLOGERROR("MKCacheImp::getMKVBatch: error, mainKey = " << vtMainKey[i] << endl);
                        vtData.clear();
                        return ET_SYS_ERR;
                    }
                }
                else
                {
                    keyValue.mainKey = vtMainKey[i];
                    vtData.push_back(keyValue);
                }
            }
        }
        else
        {
            for (i = 0; i < MainkeyCount; ++i)
            {
                if (!g_route_table.isMySelf(vtMainKey[i]))
                {
                    TLOGERROR("MKCacheImp::getMKVBatch: " << vtMainKey[i] << " is not in self area" << endl);
                    vtData.clear();
                    map<string, string>& context = current->getContext();
                    //API直连模式，返回增量更新路由
                    if (VALUE_YES == context[GET_ROUTE])
                    {
                        RspUpdateServant updateServant;
                        map<string, string> rspContext;
                        rspContext[ROUTER_UPDATED] = "";
                        int ret = RouterHandle::getInstance()->getUpdateServant(vtMainKey, false, context[API_IDC], updateServant);
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

                DCache::MainKeyValue keyValue;
                tars::Int32 iRet = selectMKCache(current, vtField, vtMainKey[i], vtUKCond, vtValueCond, stLimit, false, keyValue.value, keyValue.ret);
                if (iRet != 0)
                {
                    if (iRet == 1 || iRet == 2)
                    {
                        vtNeedDBAccessMainKey.push_back(vtMainKey[i]);
                    }
                    else if (iRet == -2)
                    {
                        TLOGERROR("MKCacheImp::getMKVBatch: error, select data count exceed the max, mainKey = " << vtMainKey[i] << endl);
                        vtData.clear();
                        return ET_DATA_TOO_MUCH;
                    }
                    else
                    {
                        TLOGERROR("MKCacheImp::getMKVBatch: error, mainKey = " << vtMainKey[i] << endl);
                        vtData.clear();
                        return ET_SYS_ERR;
                    }
                }
                else
                {
                    keyValue.mainKey = vtMainKey[i];
                    vtData.push_back(keyValue);
                }
            }
        }
    }
    catch (std::exception& ex)
    {
        TLOGERROR("MKCacheImp::getMKVBatch exception: " << ex.what() << ", mainKey = " << vtMainKey[i] << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        vtData.clear();
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::getMKVBatch unkown exception, ,mainKey = " << vtMainKey[i] << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        vtData.clear();
        return ET_SYS_ERR;
    }

    if (vtNeedDBAccessMainKey.size() > 0 && _existDB  && _readDB)
    {
        current->setResponse(false);
        SelectBatchParamPtr pParam = new SelectBatchParam(vtNeedDBAccessMainKey.size(), vtData);

        for (i = 0; i < vtNeedDBAccessMainKey.size(); ++i)
        {
            if (pParam->bEnd)
            {
                return ET_SYS_ERR;
            }

            MKDbAccessCBParam::SelectCBParam param;
            param.current = current;
            param.mk = vtNeedDBAccessMainKey[i];
            param.bUKey = bUKey;
            param.vtUKCond = vtUKCond;
            param.vtValueCond = vtValueCond;
            param.stLimit = stLimit;
            param.bGetMKCout = false;
            param.bBatch = true;
            param.bMUKBatch = false;
            param.bBatchOr = false;
            param.pParam = pParam;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(vtNeedDBAccessMainKey[i], isCreate);
            tars::Int32 iAddret = pCBParam->AddSelect(param);
            if (iAddret == 0)
            {
                if (isCreate)
                {
                    TLOGDEBUG("MKCacheImp::getMKVBatch: async select db, mainKey = " << vtNeedDBAccessMainKey[i] << endl);
                    try
                    {
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                        //异步调用DBAccess
                        asyncDbSelect(vtNeedDBAccessMainKey[i], cb);
                        continue;
                    }
                    catch (std::exception& ex)
                    {
                        TLOGERROR("MKCacheImp::getMKVBatch exception: " << ex.what() << ", mainKey = " << vtNeedDBAccessMainKey[i] << endl);
                    }
                    catch (...)
                    {
                        TLOGERROR("MKCacheImp::getMKVBatch unkown exception, mainKey = " << vtNeedDBAccessMainKey[i] << endl);
                    }
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    if (pParam->setEnd())
                    {
                        MKVBatchRsp rsp;
                        MKCache::async_response_getMKVBatch(current, ET_SYS_ERR, rsp);
                    }
                    break;
                }
                else
                {
                    TLOGDEBUG("MKCacheImp::getMKVBatch: set into cbparam, mainKey = " << vtNeedDBAccessMainKey[i] << endl);
                }
            }
            else
            {
                tars::Int32 iRet;
                MainKeyValue keyValue;
                if (bUKey)
                {
                    iRet = selectUKCache(current, vtField, vtNeedDBAccessMainKey[i], sUK, vtUKCond, vtValueCond, stLimit, false, keyValue.value, keyValue.ret);
                }
                else
                {
                    iRet = selectMKCache(current, vtField, vtNeedDBAccessMainKey[i], vtUKCond, vtValueCond, stLimit, false, keyValue.value, keyValue.ret);
                }
                if (iRet != 0)
                {
                    if (iRet == -2)
                    {
                        TLOGERROR("MKCacheImp::getMKVBatch: select second error, mainKey = " << vtNeedDBAccessMainKey[i] << endl);
                        if (pParam->setEnd())
                        {
                            MKVBatchRsp rsp;
                            MKCache::async_response_getMKVBatch(current, ET_DATA_TOO_MUCH, rsp);
                        }
                    }
                    else
                    {
                        TLOGERROR("MKCacheImp::getMKVBatch: select second error, mainKey = " << vtNeedDBAccessMainKey[i] << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        if (pParam->setEnd())
                        {
                            MKVBatchRsp rsp;
                            MKCache::async_response_getMKVBatch(current, ET_SYS_ERR, rsp);
                        }
                    }
                    break;
                }
                else
                {
                    keyValue.mainKey = vtNeedDBAccessMainKey[i];
                    pParam->addValue(keyValue);
                    if (pParam->count.dec() <= 0)
                    {
                        pParam->bEnd = true;
                        MKVBatchRsp rsp;
                        rsp.data = pParam->vtKeyValue;
                        MKCache::async_response_getMKVBatch(current, ET_SUCC, rsp);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        if (vtNeedDBAccessMainKey.size() > 0)
        {
            for (i = 0; i < vtNeedDBAccessMainKey.size(); ++i)
            {
                MainKeyValue keyValue;
                keyValue.mainKey = vtNeedDBAccessMainKey[i];
                keyValue.ret = 0;
                vtData.push_back(keyValue);
            }
        }
    }

    return ET_SUCC;
}

tars::Int32 MKCacheImp::getMUKBatch(const DCache::MUKBatchReq &req, DCache::MUKBatchRsp &rsp, tars::TarsCurrentPtr current)
{
    const vector<DCache::Record> & vtMUKey = req.primaryKeys;
    const std::string & field = req.field;
    vector<DCache::Record> &vtData = rsp.data;

    TLOGDEBUG("MKCacheImp::getMUKBatch recv : " << vtMUKey.size() << "|" << field << "|" << endl);

    if (req.moduleName != _moduleName)
    {
        TLOGERROR("MKCacheImp::getMUKBatch: moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    if (vtMUKey.empty())
    {
        TLOGERROR(__FUNCTION__ << ":" << __LINE__ << " mainkey and ukey param has not been set yet" << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    vector<string> vtField;
    convertField(field, vtField);

    Limit stLimit;
    stLimit.bLimit = false;
    vector<DCache::Condition> vtValueCond;
    map<string, vector<vector<Condition> > > mpNeedDBAccess;  //每条记录对应某主key下的联合key集合
    string sMainKey;

    size_t MUKeyCount = vtMUKey.size();

    try
    {
        for (size_t i = 0; i < MUKeyCount; ++i)
        {
            vector<Condition> vtUKCond;
            int iRetCode;
            if (!checkRecord(vtMUKey[i], sMainKey, vtUKCond, iRetCode))
            {
                TLOGERROR("MKCacheImp::getMUKBatch: param error" << endl);
                vtData.clear();
                return iRetCode;
            }

            if (!g_route_table.isMySelf(sMainKey))
            {
                TLOGERROR("MKCacheImp::getMUKBatch: " << sMainKey << " is not in self area" << endl);
                vtData.clear();

                map<string, string>& context = current->getContext();
                //API直连模式，返回增量更新路由
                if (VALUE_YES == context[GET_ROUTE])
                {
                    vector<string> vtMainKey;
                    for (size_t j = 0; j < MUKeyCount; j++)
                    {
                        vtMainKey.push_back(vtMUKey[j].mainKey);
                    }

                    RspUpdateServant updateServant;
                    map<string, string> rspContext;
                    rspContext[ROUTER_UPDATED] = "";
                    int ret = RouterHandle::getInstance()->getUpdateServant(vtMainKey, false, context[API_IDC], updateServant);
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

            string sUK;
            TarsEncode uKeyEncode;
            for (size_t k = 0; k < vtUKCond.size(); ++k)
            {
                const Condition &cond = vtUKCond[k];
                const FieldInfo &fieldInfo = _fieldConf.mpFieldInfo[cond.fieldName];
                uKeyEncode.write(cond.value, fieldInfo.tag, fieldInfo.type);
            }
            sUK.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

            Record record;
            record = vtMUKey[i];

            DCache::MainKeyValue keyValue;
            tars::Int32 iRet = selectUKCache(current, vtField, sMainKey, sUK, vtUKCond, vtValueCond, stLimit, false, keyValue.value, keyValue.ret);
            if (iRet != 0)
            {
                if (iRet == 1)
                {

                    mpNeedDBAccess[sMainKey].push_back(vtUKCond);

                }
                else
                {
                    TLOGERROR("MKCacheImp::getMUKBatch: error, mainKey = " << sMainKey << endl);
                    vtData.clear();
                    return ET_SYS_ERR;
                }
            }
            else
            {
                if (keyValue.value.size() == 1)
                {
                    record.mpRecord.insert(keyValue.value[0].begin(), keyValue.value[0].end());
                    record.ret = 0; //success
                }
                else if (keyValue.value.size() > 1)
                {
                    //非唯一数据
                    TLOGERROR(__FUNCTION__ << ":" << __LINE__ << " keyvalue size error:" << keyValue.value.size() << endl);
                    vtData.clear();
                    return ET_SYS_ERR;
                }
                else if (keyValue.value.size() == 0)
                {
                    record.ret = ET_ONLY_KEY;  //only key
                }

                vtData.push_back(record);

            }

        }
    }
    catch (std::exception& ex)
    {
        TLOGERROR("MKCacheImp::getMUKBatch exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        vtData.clear();
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::getMUKBatch unkown exception, ,mainKey = " << sMainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        vtData.clear();
        return ET_SYS_ERR;
    }



    map<string, vector<vector<Condition> > >::const_iterator needDbIt = mpNeedDBAccess.begin();
    map<string, vector<vector<Condition> > >::const_iterator needDbEndIt = mpNeedDBAccess.end();
    if (mpNeedDBAccess.size() > 0 && _existDB  && _readDB)
    {
        current->setResponse(false);
        SelectBatchParamPtr pParam = new SelectBatchParam(mpNeedDBAccess.size(), vtData);

        for (; needDbIt != needDbEndIt; ++needDbIt)
        {
            if (pParam->bEnd)
            {
                return ET_SYS_ERR;
            }

            MKDbAccessCBParam::SelectCBParam param;
            param.current = current;
            param.mk = needDbIt->first;
            param.bUKey = true;
            param.vvUKConds = needDbIt->second;
            param.stLimit = stLimit;
            param.bGetMKCout = false;
            param.bBatch = true;
            param.bMUKBatch = true;
            param.bBatchOr = false;
            param.pParam = pParam;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(needDbIt->first, isCreate);
            tars::Int32 iAddret = pCBParam->AddSelect(param);
            if (iAddret == 0)
            {
                if (isCreate)
                {
                    TLOGDEBUG("MKCacheImp::getMUKBatch: async select db, mainKey = " << needDbIt->first << endl);
                    try
                    {
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                        //异步调用DBAccess
                        asyncDbSelect(needDbIt->first, cb);
                        continue;
                    }
                    catch (std::exception& ex)
                    {
                        TLOGERROR("MKCacheImp::getMUKBatch exception: " << ex.what() << ", mainKey = " << needDbIt->first << endl);
                    }
                    catch (...)
                    {
                        TLOGERROR("MKCacheImp::getMUKBatch unkown exception, mainKey = " << needDbIt->first << endl);
                    }
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    if (pParam->setEnd())
                    {
                        MUKBatchRsp rsp;
                        MKCache::async_response_getMUKBatch(current, ET_SYS_ERR, rsp);
                    }
                    break;
                }
                else
                {
                    TLOGDEBUG("MKCacheImp::getMUKBatch: set into cbparam, mainKey = " << needDbIt->first << endl);
                }
            }
            else
            {
                const vector<vector<Condition> > &vvUKCon = needDbIt->second;
                for (size_t n = 0; n < vvUKCon.size(); ++n)
                {
                    tars::Int32 iRet;
                    MainKeyValue keyValue;

                    TarsEncode uKeyEncode;
                    string sUK;

                    const vector<Condition> &vtSingleUKCon = vvUKCon[n];
                    for (size_t k = 0; k < vtSingleUKCon.size(); ++k)
                    {
                        uKeyEncode.write(vtSingleUKCon[k].value, _fieldConf.mpFieldInfo[vtSingleUKCon[k].fieldName].tag, _fieldConf.mpFieldInfo[vtSingleUKCon[k].fieldName].type);
                    }
                    sUK.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

                    iRet = selectUKCache(current, vtField, needDbIt->first, sUK, vtSingleUKCon, vtValueCond, stLimit, false, keyValue.value, keyValue.ret);

                    if (iRet != 0)
                    {
                        TLOGERROR("MKCacheImp::getMUKBatch: select second error, mainKey = " << needDbIt->first << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        if (pParam->setEnd())
                        {
                            MUKBatchRsp rsp;
                            MKCache::async_response_getMUKBatch(current, ET_SYS_ERR, rsp);
                        }
                        break;
                    }
                    else
                    {
                        keyValue.mainKey = needDbIt->first;
                        pParam->addValue(keyValue);
                        if (pParam->count.dec() <= 0)
                        {
                            pParam->bEnd = true;
                            MUKBatchRsp rsp;
                            rsp.data = pParam->vtData;
                            MKCache::async_response_getMUKBatch(current, ET_SUCC, rsp);
                            break;
                        }
                    }
                }
            }
        }
    }
    else
    {
        if (mpNeedDBAccess.size() > 0)
        {

            for (; needDbIt != needDbEndIt; ++needDbIt)
            {
                for (size_t i = 0; i < needDbIt->second.size(); ++i)
                {
                    Record record;
                    record.mainKey = needDbIt->first;
                    map<string, string> mpTmpVtUkey;
                    convertFromUKCondition(needDbEndIt->second[i], mpTmpVtUkey);
                    record.mpRecord.insert(mpTmpVtUkey.begin(), mpTmpVtUkey.end());
                    record.ret = ET_NO_DATA; //no data
                    vtData.push_back(record);
                }
            }

        }
    }

    return ET_SUCC;
}

tars::Int32 MKCacheImp::getMainKeyCount(const DCache::MainKeyReq &req, tars::TarsCurrentPtr current)
{
    int iMKRecord = 0;
    const string& mainKey = req.mainKey;
    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::getMainKeyCount: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::getMainKeyCount: " << mainKey << " is not in self area" << endl);
            map<string, string>& context = current->getContext();
            //API直连模式，返回增量更新路由
            if (VALUE_YES == context[GET_ROUTE])
            {
                RspUpdateServant updateServant;
                map<string, string> rspContext;
                rspContext[ROUTER_UPDATED] = "";
                int ret = RouterHandle::getInstance()->getUpdateServant(mainKey, false, context[API_IDC], updateServant);
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

        //如果有dbaccess，但内存数据为空或不完全，就要去查一下db
        if (_existDB && _readDB)
        {
            int iCheckRet = g_HashMap.checkMainKey(mainKey);

            if (iCheckRet != TC_Multi_HashMap_Malloc::RT_OK
                    && iCheckRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY
                    && iCheckRet != TC_Multi_HashMap_Malloc::RT_PART_DATA
                    && iCheckRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                    && iCheckRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
            {
                TLOGERROR("MKCacheImp::getMainKeyCount: checkMainKey error, ret = " << iCheckRet << ",mainKey = " << mainKey << endl);
                return ET_SYS_ERR;
            }
            else if (iCheckRet == TC_Multi_HashMap_Malloc::RT_PART_DATA || iCheckRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
            {
                MKDbAccessCBParam::GetCountCBParam param;
                param.current = current;
                param.mk = mainKey;

                bool isCreate;
                MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mainKey, isCreate);
                int iAddret = pCBParam->AddGetCount(param);
                try
                {
                    if (iAddret == 0)
                    {
                        if (isCreate)
                        {
                            current->setResponse(false);
                            TLOGDEBUG("MKCacheImp::getMainKeyCount: async select db, mainKey = " << mainKey << endl);
                            DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, _storeKeyType);
                            //异步调用DBAccess
                            asyncDbSelect(mainKey, cb);
                        }
                        else
                        {
                            TLOGDEBUG("MKCacheImp::getMainKeyCount: set into cbparam, mainKey = " << mainKey << endl);
                        }
                    }

                    return g_HashMap.count(mainKey);
                }
                catch (std::exception& ex)
                {
                    TLOGERROR("MKCacheImp::getMainKeyCount exception: " << ex.what() << ", mainKey = " << mainKey << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_SYS_ERR;
                }
                catch (...)
                {
                    TLOGERROR("MKCacheImp::getMainKeyCount unkown exception, ,mainKey = " << mainKey << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_SYS_ERR;
                }
            }
            else
                iMKRecord = g_HashMap.count(mainKey);
        }
        else//没有就直接返回内存中的数量
            iMKRecord = g_HashMap.count(mainKey);

    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::getMainKeyCount exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::getMainKeyCount unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }

    return iMKRecord;
}

tars::Int32 MKCacheImp::getAllMainKey(const DCache::GetAllKeysReq &req, DCache::GetAllKeysRsp &rsp, tars::TarsCurrentPtr current)
{
    int index = req.index;
    int count = req.count;

    TLOGDEBUG("MKCacheImp::getMKAllMainKey recv : " << index << "|" << count << endl);

    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::getMKAllMainKey: moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }

        //检查条件是否正确
        if ((index < 0) || (count < -1))
            return ET_INPUT_PARAM_ERROR;

        tars::Int32 iCount = 0;
        MultiHashMap::mk_hash_iterator it = g_HashMap.mHashByPos(index);

        if (it == g_HashMap.mHashEnd())
        {
            TLOGERROR("MKCacheImp::getMKAllMainKey mHashByPos return end! index:" << index << endl);
            rsp.isEnd = true;
            return ET_SUCC;
        }

        while (it != g_HashMap.mHashEnd())
        {
            {
                if ((count == 0) || ((count > 0) && (iCount++ >= count)))
                {
                    rsp.isEnd = false;
                    return ET_SUCC;
                }
                vector<string> tmpData;
                it->getKey(tmpData);
                rsp.keys.insert(rsp.keys.end(), tmpData.begin(), tmpData.end());
            }

            it++;
        }

        TLOGDEBUG("MKCacheImp::getMKAllMainKey reach the end! " << index << "|" << count << endl);

        rsp.isEnd = true;

    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::getMKAllMainKey exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::getMKAllMainKey unkown exception" << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }

    return ET_SUCC;
}

tars::Int32 MKCacheImp::getMKVBatchEx(const DCache::MKVBatchExReq &req, DCache::MKVBatchExRsp &rsp, tars::TarsCurrentPtr current)
{
    const vector<DCache::MainKeyCondition> & vtKey = req.cond;
    vector<DCache::MainKeyValue> &vtData = rsp.data;

    TLOGDEBUG("MKCacheImp::getMKVBatchEx recv : " << FormatLog::tostr(vtKey) << endl);

    if (req.moduleName != _moduleName)
    {
        TLOGERROR("MKCacheImp::getMKVBatchEx: moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    vector<MKDbAccessCBParam::SelectCBParam> vtNeedDBAccessMainKey;
    vtNeedDBAccessMainKey.reserve(vtKey.size());

    try
    {
        for (unsigned int i = 0; i < vtKey.size(); i++)
        {
            const DCache::MainKeyCondition &mkCond = vtKey[i];
            if (!g_route_table.isMySelf(mkCond.mainKey))
            {
                TLOGERROR("MKCacheImp::getMKVBatchEx: " << mkCond.mainKey << " is not in self area" << endl);
                vtData.clear();

                map<string, string>& context = current->getContext();
                //API直连模式，返回增量更新路由
                if (VALUE_YES == context[GET_ROUTE])
                {
                    vector<string> vtMainKey;
                    for (size_t j = 0; j < vtKey.size(); j++)
                    {
                        vtMainKey.push_back(vtKey[j].mainKey);
                    }

                    RspUpdateServant updateServant;
                    map<string, string> rspContext;
                    rspContext[ROUTER_UPDATED] = "";
                    int ret = RouterHandle::getInstance()->getUpdateServant(vtMainKey, false, context[API_IDC], updateServant);
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

            Limit stLimit;
            stLimit.bLimit = false;
            if (mkCond.limit.op == DCache::LIMIT)
            {
                stLimit.bLimit = true;
                vector<string> vt = TC_Common::sepstr<string>(mkCond.limit.value, ":");
                if (vt.size() != 2)
                {
                    TLOGERROR("MKCacheImp::getMKVBatchEx: limit value error" << endl);
                    return ET_PARAM_LIMIT_VALUE_ERR;;
                }
                stLimit.iIndex = TC_Common::strto<size_t>(TC_Common::trim(vt[0]));
                stLimit.iCount = TC_Common::strto<size_t>(TC_Common::trim(vt[1]));
            }

            vector<string> vtField;
            convertField(mkCond.field, vtField);

            DCache::MainKeyValue keyValue;
            tars::Int32 iRet = selectMKCache(vtField, mkCond.mainKey, mkCond.cond, stLimit, mkCond.bGetMKCout, keyValue.value, keyValue.ret);

            if (iRet != 0)
            {
                if (iRet == 1 || iRet == 2)
                {

                    vtNeedDBAccessMainKey.push_back(MKDbAccessCBParam::SelectCBParam());

                    MKDbAccessCBParam::SelectCBParam &param = vtNeedDBAccessMainKey[vtNeedDBAccessMainKey.size() - 1];
                    param.current = current;
                    param.mk = mkCond.mainKey;
                    param.vtField = vtField;
                    param.bUKey = false;
                    param.vvUKConds = mkCond.cond;
                    param.stLimit = stLimit;
                    param.bGetMKCout = mkCond.bGetMKCout;
                    param.bBatch = true;
                    param.bMUKBatch = false;
                    param.bBatchOr = true;
                }
                else if (iRet == -2)
                {
                    TLOGERROR("MKCacheImp::getMKVBatchEx: error, select data count exceed the max, mainKey = " << mkCond.mainKey << endl);
                    vtData.clear();
                    return ET_DATA_TOO_MUCH;
                }
                else
                {
                    TLOGERROR("MKCacheImp::getMKVBatchEx: error, mainKey = " << mkCond.mainKey << endl);
                    vtData.clear();
                    return ET_SYS_ERR;
                }
            }
            else
            {
                keyValue.mainKey = mkCond.mainKey;
                vtData.push_back(keyValue);
            }
        }
    }
    catch (std::exception& ex)
    {
        TLOGERROR("MKCacheImp::getMKVBatchEx exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        vtData.clear();
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::getMKVBatchEx unkown exception, " << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        vtData.clear();
        return ET_SYS_ERR;
    }

    if (vtNeedDBAccessMainKey.size() > 0 && _existDB && _readDB)
    {
        current->setResponse(false);
        SelectBatchParamPtr pParam = new SelectBatchParam(vtNeedDBAccessMainKey.size(), vtData);

        for (unsigned i = 0; i < vtNeedDBAccessMainKey.size(); ++i)
        {
            if (pParam->bEnd)
            {
                return ET_SYS_ERR;
            }

            vtNeedDBAccessMainKey[i].pParam = pParam;

            bool isCreate;
            MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(vtNeedDBAccessMainKey[i].mk, isCreate);
            tars::Int32 iAddret = pCBParam->AddSelect(vtNeedDBAccessMainKey[i]);
            if (iAddret == 0)
            {
                if (isCreate)
                {
                    TLOGDEBUG("MKCacheImp::getMKVBatchEx: async select db, mainKey = " << vtNeedDBAccessMainKey[i].mk << endl);
                    try
                    {
                        DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                        //异步调用DBAccess
                        asyncDbSelect(vtNeedDBAccessMainKey[i].mk, cb);
                        continue;
                    }
                    catch (std::exception& ex)
                    {
                        TLOGERROR("MKCacheImp::getMKVBatchEx exception: " << ex.what() << ", mainKey = " << vtNeedDBAccessMainKey[i].mk << endl);
                    }
                    catch (...)
                    {
                        TLOGERROR("MKCacheImp::getMKVBatchEx unkown exception, mainKey = " << vtNeedDBAccessMainKey[i].mk << endl);
                    }
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    if (pParam->setEnd())
                    {
                        MKVBatchExRsp rsp;
                        MKCache::async_response_getMKVBatchEx(current, ET_SYS_ERR, rsp);
                    }
                    break;
                }
                else
                {
                    TLOGDEBUG("MKCacheImp::getMKVBatchEx: set into cbparam, mainKey = " << vtNeedDBAccessMainKey[i].mk << endl);
                }
            }
            else
            {
                tars::Int32 iRet;
                MainKeyValue keyValue;
                iRet = selectMKCache(vtNeedDBAccessMainKey[i].vtField, vtNeedDBAccessMainKey[i].mk, vtNeedDBAccessMainKey[i].vvUKConds, vtNeedDBAccessMainKey[i].stLimit, vtNeedDBAccessMainKey[i].bGetMKCout, keyValue.value, keyValue.ret);

                if (iRet != 0)
                {
                    if (iRet == -2)
                    {
                        TLOGERROR("MKCacheImp::getMKVBatchEx: select second error, mainKey = " << vtNeedDBAccessMainKey[i].mk << endl);
                        if (pParam->setEnd())
                        {
                            MKVBatchExRsp rsp;
                            MKCache::async_response_getMKVBatchEx(current, ET_DATA_TOO_MUCH, rsp);
                        }
                    }
                    else
                    {
                        TLOGERROR("MKCacheImp::getMKVBatchEx: select second error, mainKey = " << vtNeedDBAccessMainKey[i].mk << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        if (pParam->setEnd())
                        {
                            MKVBatchExRsp rsp;
                            MKCache::async_response_getMKVBatchEx(current, ET_SYS_ERR, rsp);
                        }
                    }
                    break;
                }
                else
                {
                    keyValue.mainKey = vtNeedDBAccessMainKey[i].mk;
                    pParam->addValue(keyValue);
                    if (pParam->count.dec() <= 0)
                    {
                        pParam->bEnd = true;
                        MKVBatchExRsp rsp;
                        rsp.data = pParam->vtKeyValue;
                        MKCache::async_response_getMKVBatchEx(current, ET_SUCC, rsp);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        if (vtNeedDBAccessMainKey.size() > 0)
        {
            for (unsigned i = 0; i < vtNeedDBAccessMainKey.size(); ++i)
            {
                MainKeyValue keyValue;
                keyValue.mainKey = vtNeedDBAccessMainKey[i].mk;
                keyValue.ret = 0;
                vtData.push_back(keyValue);
            }
        }
    }

    return ET_SUCC;
}

tars::Int32 MKCacheImp::getRangeList(const DCache::GetRangeListReq &req, DCache::BatchEntry &rsp, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const std::string & field = req.field;
    tars::Int64 iStart = req.startIndex;
    tars::Int64 iEnd = req.endIndex;

    TLOGDEBUG("MKCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << " : moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << " : " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }
        g_app.gstat()->tryHit(_hitIndex);

        vector<string> vs;
        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.getList(mainKey, iStart, iEnd, TC_TimeProvider::getInstance()->getNow(), vs);
        else
            iRet = g_HashMap.getList(mainKey, iStart, iEnd, 0, vs);

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << " : " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }

        if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
        {
            vector<string> vtField;
            convertField(field, vtField);
            for (unsigned int i = 0; i < vs.size(); i++)
            {
                TarsDecode vDecode;
                vDecode.setBuffer(vs[i]);
                setResult(vtField, mainKey, vDecode, 0, 0, rsp.entries);
            }

            g_app.ppReport(PPReport::SRP_GET_CNT, vs.size());
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKCacheImp::getList(const DCache::GetListReq &req, DCache::GetListRsp &rsp, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const std::string & field = req.field;
    tars::Int64 iPos = req.index;

    TLOGDEBUG("MKCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }
        g_app.gstat()->tryHit(_hitIndex);

        vector<string> vs;
        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.getList(mainKey, iPos, iPos, TC_TimeProvider::getInstance()->getNow(), vs);
        else
            iRet = g_HashMap.getList(mainKey, iPos, iPos, 0, vs);

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }

        if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
        {
            if (vs.size() > 0)
            {
                vector<string> vtField;
                convertField(field, vtField);
                vector<map<std::string, std::string> > vtData;
                TarsDecode vDecode;
                vDecode.setBuffer(vs[0]);
                setResult(vtField, mainKey, vDecode, 0, 0, vtData);

                rsp.entry.data = vtData[0];
            }
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKCacheImp::getSet(const DCache::GetSetReq &req, DCache::BatchEntry &rsp, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const std::string & field = req.field;

    TLOGDEBUG("MKCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }
        g_app.gstat()->tryHit(_hitIndex);

        vector<string> vtField;
        convertField(field, vtField);

        vector<TC_Multi_HashMap_Malloc::Value> vt;
        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.getSet(mainKey, TC_TimeProvider::getInstance()->getNow(), vt);
        else
            iRet = g_HashMap.getSet(mainKey, 0, vt);

        if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
        {
            g_app.gstat()->hit(_hitIndex);
            if (_existDB  && _readDB)
            {
                iRet = asyncGetSet(current, vtField, mainKey);

                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getSet(mainKey, TC_TimeProvider::getInstance()->getNow(), vt);
                    else
                        iRet = g_HashMap.getSet(mainKey, 0, vt);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getSet second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            if (_existDB  && _readDB)
            {
                iRet = asyncGetSet(current, vtField, mainKey);

                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getSet(mainKey, TC_TimeProvider::getInstance()->getNow(), vt);
                    else
                        iRet = g_HashMap.getSet(mainKey, 0, vt);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getSet second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
        }
        else if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }

        for (unsigned int i = 0; i < vt.size(); i++)
        {
            TarsDecode vDecode;
            vDecode.setBuffer(vt[i]._value);
            setResult(vtField, mainKey, vDecode, 0, 0, rsp.entries);
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}


tars::Int32 MKCacheImp::getZSetScore(const DCache::GetZsetScoreReq &req, tars::Double &score, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const vector<DCache::Condition> & vtCond = req.cond;

    TLOGDEBUG("MKCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ":" << mainKey << " is not in self area" << endl);
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

        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.getScoreZSet(mainKey, value, TC_TimeProvider::getInstance()->getNow(), score);
        else
            iRet = g_HashMap.getScoreZSet(mainKey, value, 0, score);

        g_app.gstat()->hit(_hitIndex);

        if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
        {
            iRet = g_HashMap.checkMainKey(mainKey);
            TLOGDEBUG("[MKCacheImp::" << __FUNCTION__ << "] checkMainKey ret :" << iRet << endl);
            if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
            {
                return ET_NO_DATA;
            }
            else if (_existDB  && _readDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA))
            {
                iRet = asyncGetZSet(current, mainKey, value);

                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getScoreZSet(mainKey, value, TC_TimeProvider::getInstance()->getNow(), score);
                    else
                        iRet = g_HashMap.getScoreZSet(mainKey, value, 0, score);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getScoreZSet second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
            else
                return ET_NO_DATA;
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            if (_existDB  && _readDB)
            {
                iRet = asyncGetZSet(current, mainKey, value);

                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getScoreZSet(mainKey, value, TC_TimeProvider::getInstance()->getNow(), score);
                    else
                        iRet = g_HashMap.getScoreZSet(mainKey, value, 0, score);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getScoreZSet second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
            else
                return ET_NO_DATA;
        }
        else if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ET_NO_DATA;
        }

        TLOGDEBUG("[MKCacheImp::" << __FUNCTION__ << "] mk:" << mainKey << " score:" << score << endl);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}


tars::Int32 MKCacheImp::getZSetPos(const DCache::GetZsetPosReq &req, tars::Int64 &pos, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const vector<DCache::Condition> & vtCond = req.cond;
    bool bOrder = req.positiveOrder;

    TLOGDEBUG("MKCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);

    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
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

        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.getRankZSet(mainKey, value, bOrder, TC_TimeProvider::getInstance()->getNow(), pos);
        else
            iRet = g_HashMap.getRankZSet(mainKey, value, bOrder, 0, pos);

        g_app.gstat()->tryHit(_hitIndex);

        if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
        {
            iRet = g_HashMap.checkMainKey(mainKey);
            TLOGDEBUG("[MKCacheImp::" << __FUNCTION__ << "] checkMainKey ret :" << iRet << endl);
            if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
            {
                return ET_NO_DATA;
            }
            else if (_existDB  && _readDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA))
            {
                iRet = asyncGetZSet(current, mainKey, value, bOrder);

                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getRankZSet(mainKey, value, bOrder, TC_TimeProvider::getInstance()->getNow(), pos);
                    else
                        iRet = g_HashMap.getRankZSet(mainKey, value, bOrder, 0, pos);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getRankZSet second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
            else
                return ET_NO_DATA;
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            if (_existDB  && _readDB)
            {
                iRet = asyncGetZSet(current, mainKey, value, bOrder);

                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getRankZSet(mainKey, value, bOrder, TC_TimeProvider::getInstance()->getNow(), pos);
                    else
                        iRet = g_HashMap.getRankZSet(mainKey, value, bOrder, 0, pos);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getRankZSet second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
            else
                return ET_NO_DATA;
        }
        else if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }

        if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ET_NO_DATA;
        }

        TLOGDEBUG("[MKCacheImp::" << __FUNCTION__ << "] mk:" << mainKey << " pos:" << pos << endl);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKCacheImp::getZSetByPos(const DCache::GetZsetByPosReq &req, DCache::BatchEntry &rsp, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const std::string & field = req.field;
    int64_t iStart = req.start;
    int64_t iEnd = req.end;
    bool bUp = req.positiveOrder;

    TLOGDEBUG("MKCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);
    g_app.gstat()->tryHit(_hitIndex);

    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        list<TC_Multi_HashMap_Malloc::Value> vt;

        vector<string> vtField;
        convertField(field, vtField);

        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.getZSet(mainKey, iStart, iEnd, bUp, TC_TimeProvider::getInstance()->getNow(), vt);
        else
            iRet = g_HashMap.getZSet(mainKey, iStart, iEnd, bUp, 0, vt);

        if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
        {
            iRet = g_HashMap.checkMainKey(mainKey);
            if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
            {
                return ET_SUCC;
            }
            else if (_existDB  && _readDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA))
            {
                iRet = asyncGetZSet(current, mainKey, vtField, iStart, iEnd, bUp);

                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getZSet(mainKey, iStart, iEnd, bUp, TC_TimeProvider::getInstance()->getNow(), vt);
                    else
                        iRet = g_HashMap.getZSet(mainKey, iStart, iEnd, bUp, 0, vt);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getZSet second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
            else
                return ET_SUCC;
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            g_app.gstat()->hit(_hitIndex);
            if (_existDB  && _readDB)
            {
                iRet = asyncGetZSet(current, mainKey, vtField, iStart, iEnd, bUp);

                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getZSet(mainKey, iStart, iEnd, bUp, TC_TimeProvider::getInstance()->getNow(), vt);
                    else
                        iRet = g_HashMap.getZSet(mainKey, iStart, iEnd, bUp, 0, vt);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getZSet second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
        }
        else if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }

        g_app.gstat()->hit(_hitIndex);

        list<TC_Multi_HashMap_Malloc::Value>::iterator it = vt.begin();
        while (it != vt.end())
        {
            TarsDecode vDecode;
            vDecode.setBuffer((*it)._value);
            setResult(vtField, mainKey, vDecode, 0, 0, (*it)._score, rsp.entries);

            it++;
        }

        g_app.ppReport(PPReport::SRP_GET_CNT, vt.size());
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKCacheImp::getZSetByScore(const DCache::GetZsetByScoreReq &req, DCache::BatchEntry &rsp, tars::TarsCurrentPtr current)
{
    const std::string & mainKey = req.mainKey;
    const std::string & field = req.field;
    double iMin = req.minScore;
    double iMax = req.maxScore;

    TLOGDEBUG("MKCacheImp::" << __FUNCTION__ << " recv : " << mainKey << endl);
    g_app.gstat()->tryHit(_hitIndex);

    try
    {
        if (req.moduleName != _moduleName)
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
            return ET_MODULE_NAME_INVALID;
        }
        //检查key是否是在自己服务范围内
        if (!g_route_table.isMySelf(mainKey))
        {
            //返回模块错误
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " is not in self area" << endl);
            return ET_KEY_AREA_ERR;
        }

        list<TC_Multi_HashMap_Malloc::Value> vt;
        vector<string> vtField;
        int iRet;
        if (g_app.gstat()->isExpireEnabled())
            iRet = g_HashMap.getZSetByScore(mainKey, iMin, iMax, TC_TimeProvider::getInstance()->getNow(), vt);
        else
            iRet = g_HashMap.getZSetByScore(mainKey, iMin, iMax, 0, vt);

        if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
        {
            iRet = g_HashMap.checkMainKey(mainKey);
            if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
            {
                return ET_SUCC;
            }
            else if (_existDB  && _readDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA))
            {
                iRet = asyncGetZSetByScore(current, mainKey, vtField, iMin, iMax);
                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getZSetByScore(mainKey, iMin, iMax, TC_TimeProvider::getInstance()->getNow(), vt);
                    else
                        iRet = g_HashMap.getZSetByScore(mainKey, iMin, iMax, 0, vt);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getZSetByScore second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
            else
                return ET_SUCC;
        }
        else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            if (_existDB  && _readDB)
            {
                iRet = asyncGetZSetByScore(current, mainKey, vtField, iMin, iMax);
                if (iRet == 1)
                {
                    if (g_app.gstat()->isExpireEnabled())
                        iRet = g_HashMap.getZSetByScore(mainKey, iMin, iMax, TC_TimeProvider::getInstance()->getNow(), vt);
                    else
                        iRet = g_HashMap.getZSetByScore(mainKey, iMin, iMax, 0, vt);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] getZSetByScore second error! " << mainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return ET_SYS_ERR;
                    }
                }
                else
                    return iRet;
            }
        }
        else if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << mainKey << " failed, ret = " << iRet << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
        g_app.gstat()->hit(_hitIndex);
        list<TC_Multi_HashMap_Malloc::Value>::iterator it = vt.begin();
        while (it != vt.end())
        {
            vector<string> vtField;
            convertField(field, vtField);
            TarsDecode vDecode;
            vDecode.setBuffer((*it)._value);
            setResult(vtField, mainKey, vDecode, 0, 0, (*it)._score, rsp.entries);
            it++;
        }

        g_app.ppReport(PPReport::SRP_GET_CNT, vt.size());
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << ", mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " unkown exception, mkey = " << mainKey << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

tars::Int32 MKCacheImp::getZSetBatch(const DCache::GetZSetBatchReq &req, DCache::GetZSetBatchRsp &rsp, tars::TarsCurrentPtr current)
{
    const vector<string> &vtMainKey = req.mainKeys;
    const string &field = req.field;
    const vector<Condition> &vtCond = req.cond;
    vector<MainKeyValue> &vtData = rsp.rspData;

    TLOGDEBUG("MKCacheImp::" << __FUNCTION__ << " recv : " << vtMainKey.size() << "|" << TC_Common::tostr(vtMainKey.begin(), vtMainKey.end(), ";") << "|" << field << "|" << FormatLog::tostr(vtCond) << endl);

    if (req.moduleName != _moduleName)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    //计算vector预留的大小
    unsigned int iCondSize = vtCond.size();
    unsigned int iReseveSize = 0;
    if (iCondSize < 50)
        iReseveSize = iCondSize;
    else
        iReseveSize = 50;

    vector<DCache::Condition> vtValueCond;
    vtValueCond.reserve(iReseveSize);
    vector<string> vtField;
    convertField(field, vtField);

    vector<string> vtNeedDBAccessMainKey;
    vtNeedDBAccessMainKey.reserve(vtMainKey.size());

    bool isLimit = false;
    Limit stLimit;
    vector<DCache::Condition> vtScoreCond;
    vtValueCond.reserve(iCondSize);

    try
    {
        int iRetCode;
        if (!checkValueCondition(vtCond, vtValueCond, stLimit, isLimit, vtScoreCond, iRetCode))
        {
            TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": param error" << endl);
            return iRetCode;
        }
        size_t MainkeyCount = vtMainKey.size();
        vtData.reserve(MainkeyCount);
        for (size_t i = 0; i < MainkeyCount; ++i)
        {
            if (!g_route_table.isMySelf(vtMainKey[i]))
            {
                TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << vtMainKey[i] << " is not in self area" << endl);
                vtData.clear();
                return ET_KEY_AREA_ERR;
            }

            DCache::MainKeyValue keyResult;
            keyResult.mainKey = vtMainKey[i];
            tars::Int32 iRet;
            list<TC_Multi_HashMap_Malloc::Value> vt;
            g_app.ppReport(PPReport::SRP_GET_CNT, 1);
            g_app.gstat()->tryHit(_hitIndex);
            //只有limit限制
            if (isLimit && (vtValueCond.size() == 0) && (vtScoreCond.size() == 0))
            {
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.getZSetLimit(vtMainKey[i], stLimit.iIndex, stLimit.iCount, true, TC_TimeProvider::getInstance()->getNow(), vt);
                else
                    iRet = g_HashMap.getZSetLimit(vtMainKey[i], stLimit.iIndex, stLimit.iCount, true, 0, vt);
            }
            else
            {
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.getZSet(vtMainKey[i], 0, ULONG_MAX, true, TC_TimeProvider::getInstance()->getNow(), vt);
                else
                    iRet = g_HashMap.getZSet(vtMainKey[i], 0, ULONG_MAX, true, 0, vt);
            }
            if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
            {
                iRet = g_HashMap.checkMainKey(vtMainKey[i]);
                if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
                {
                    keyResult.ret = TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
                    vtData.push_back(keyResult);
                    continue;
                }
                else if (_existDB  && _readDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA))
                {
                    iRet = asyncGetZSet(current, vtMainKey[i], vtField, 0, ULONG_MAX, true);
                    if (iRet == 1)
                    {
                        if (isLimit && (vtValueCond.size() == 0) && (vtScoreCond.size() == 0))
                        {
                            if (g_app.gstat()->isExpireEnabled())
                                iRet = g_HashMap.getZSetLimit(vtMainKey[i], stLimit.iIndex, stLimit.iCount, true, TC_TimeProvider::getInstance()->getNow(), vt);
                            else
                                iRet = g_HashMap.getZSetLimit(vtMainKey[i], stLimit.iIndex, stLimit.iCount, true, 0, vt);
                        }
                        else
                        {
                            if (g_app.gstat()->isExpireEnabled())
                                iRet = g_HashMap.getZSet(vtMainKey[i], 0, ULONG_MAX, true, TC_TimeProvider::getInstance()->getNow(), vt);
                            else
                                iRet = g_HashMap.getZSet(vtMainKey[i], 0, ULONG_MAX, true, 0, vt);
                        }

                        if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                        {
                            TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] selectZSetBatch second error! " << vtMainKey[i] << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            vtData.push_back(keyResult);
                            keyResult.ret = ET_SYS_ERR;
                            continue;
                        }
                    }
                    else
                    {
                        keyResult.ret = iRet;
                        vtData.push_back(keyResult);
                        continue;
                    }
                }
                else
                    keyResult.ret = ET_SUCC;
            }
            else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
            {
                if (_existDB  && _readDB)
                {
                    iRet = asyncGetZSet(current, vtMainKey[i], vtField, 0, ULONG_MAX, true);
                    if (iRet == 1)
                    {
                        if (isLimit && (vtValueCond.size() == 0) && (vtScoreCond.size() == 0))
                        {
                            if (g_app.gstat()->isExpireEnabled())
                                iRet = g_HashMap.getZSetLimit(vtMainKey[i], stLimit.iIndex, stLimit.iCount, true, TC_TimeProvider::getInstance()->getNow(), vt);
                            else
                                iRet = g_HashMap.getZSetLimit(vtMainKey[i], stLimit.iIndex, stLimit.iCount, true, 0, vt);
                        }
                        else
                        {
                            if (g_app.gstat()->isExpireEnabled())
                                iRet = g_HashMap.getZSet(vtMainKey[i], 0, ULONG_MAX, true, TC_TimeProvider::getInstance()->getNow(), vt);
                            else
                                iRet = g_HashMap.getZSet(vtMainKey[i], 0, ULONG_MAX, true, 0, vt);
                        }

                        if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                        {
                            TLOGERROR("[MKCacheImp::" << __FUNCTION__ << "] selectZSetBatch second error! " << vtMainKey[i] << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            keyResult.ret = ET_SYS_ERR;
                            vtData.push_back(keyResult);
                            continue;
                        }
                    }
                    else
                    {
                        keyResult.ret = iRet;
                        vtData.push_back(keyResult);
                        continue;
                    }
                }
            }
            else if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
            {
                TLOGERROR("MKCacheImp::" << __FUNCTION__ << ": " << vtMainKey[i] << " failed, ret = " << iRet << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                keyResult.ret = ET_SYS_ERR;
                vtData.push_back(keyResult);
                continue;
            }
            g_app.gstat()->hit(_hitIndex);
            list<TC_Multi_HashMap_Malloc::Value>::iterator it = vt.begin();
            int iJudgeCount = 0;
            if (!isLimit)
            {
                while (it != vt.end())
                {
                    iJudgeCount++;
                    TarsDecode vDecode;
                    vDecode.setBuffer((*it)._value);
                    TLOGDEBUG("value:" << it->_value << endl);
                    if (judge(vDecode, vtValueCond))
                    {
                        if (judgeScore((*it)._score, vtScoreCond))
                        {
                            setResult(vtField, vtMainKey[i], vDecode, 0, (*it)._iExpireTime, (*it)._score, keyResult.value);
                            keyResult.ret = ET_SUCC;
                        }
                    }
                    it++;
                }
            }
            // Has limit condition
            else
            {
                size_t iIndex = 0;
                size_t iEnd = stLimit.iIndex + stLimit.iCount;

                // judge only limit condition
                if ((vtValueCond.size() == 0) && (vtScoreCond.size() == 0))
                {
                    while (it != vt.end())
                    {
                        iJudgeCount++;
                        TarsDecode vDecode;
                        vDecode.setBuffer((*it)._value);
                        setResult(vtField, vtMainKey[i], vDecode, 0, (*it)._iExpireTime, (*it)._score, keyResult.value);
                        keyResult.ret = ET_SUCC;
                        it++;
                    }
                }
                else
                {
                    while (it != vt.end())
                    {
                        iJudgeCount++;
                        TarsDecode vDecode;
                        vDecode.setBuffer((*it)._value);
                        TLOGDEBUG("value:" << it->_value << endl);
                        if (judge(vDecode, vtValueCond))
                        {
                            if (judgeScore((*it)._score, vtScoreCond))
                            {
                                if (iIndex >= stLimit.iIndex && iIndex < iEnd)
                                {
                                    setResult(vtField, vtMainKey[i], vDecode, 0, (*it)._iExpireTime, (*it)._score, keyResult.value);
                                    keyResult.ret = ET_SUCC;
                                }
                                iIndex++;
                                if (iIndex >= iEnd)
                                    break;
                            }
                        }
                        it++;
                    }
                }
            }

            vtData.push_back(keyResult);
            g_app.ppReport(PPReport::SRP_GET_CNT, iJudgeCount);
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    catch (...)
    {
        TLOGERROR("MKCacheImp::" << __FUNCTION__ << " unkown exception" << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

void MKCacheImp::asyncDbSelect(const string &mainKey, DbAccessPrxCallbackPtr cb)
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

int MKCacheImp::procSelectUK(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord)
{
    string uk;

    TarsEncode uKeyEncode;
    for (size_t i = 0; i < vtUKCond.size(); i++)
    {
        const DCache::Condition &ukCond = vtUKCond[i];
        const FieldInfo &fileInfo = _fieldConf.mpFieldInfo[ukCond.fieldName];
        uKeyEncode.write(ukCond.value, fileInfo.tag, fileInfo.type);
    }
    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

    if (bGetMKCout && _existDB && _readDB)
    {
        int iCheckRet = g_HashMap.checkMainKey(mk);

        if (iCheckRet != TC_Multi_HashMap_Malloc::RT_OK
                && iCheckRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY
                && iCheckRet != TC_Multi_HashMap_Malloc::RT_PART_DATA
                && iCheckRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                && iCheckRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
        {
            TLOGERROR("MKCacheImp::selectUKCache: checkMainKey error, ret = " << iCheckRet << ",mainKey = " << mk << endl);
            return ET_SYS_ERR;
        }
        else if (iCheckRet == TC_Multi_HashMap_Malloc::RT_PART_DATA || iCheckRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
        {
            g_app.gstat()->tryHit(_hitIndex);
            if (asyncSelect(current, vtField, mk, vtUKCond, vtValueCond, stLimit, true, bGetMKCout) == 0)
            {
                return 1;
            }
        }
    }
    int iRet = selectUKCache(current, vtField, mk, uk, vtUKCond, vtValueCond, stLimit, bGetMKCout, vtData, iMKRecord);
    if (iRet != 0)
    {
        if (iRet == 1)
        {
            if (_existDB  && _readDB)
            {
                if (asyncSelect(current, vtField, mk, vtUKCond, vtValueCond, stLimit, true, bGetMKCout) == 0)
                {
                    return 1;
                }
                else
                {
                    int iRet2 = selectUKCache(current, vtField, mk, uk, vtUKCond, vtValueCond, stLimit, bGetMKCout, vtData, iMKRecord);
                    {
                        if (iRet2 == 1)
                        {
                            return ET_NO_DATA;
                        }
                        if (iRet2 != 0)
                        {
                            TLOGERROR("MKCacheImp::selectUKCache: second error, mainKey = " << mk << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            return ET_SYS_ERR;
                        }
                    }
                }
            }
        }
        else
        {
            TLOGERROR("MKCacheImp::selectUKCache: error, mainKey = " << mk << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
    }
    return 0;
}

int MKCacheImp::procSelectMK(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord)
{
    int iRet = selectMKCache(current, vtField, mk, vtUKCond, vtValueCond, stLimit, bGetMKCout, vtData, iMKRecord);
    if (iRet != 0)
    {
        if (iRet == 1 || iRet == 2)
        {
            if (_existDB  && _readDB)
            {
                if (asyncSelect(current, vtField, mk, vtUKCond, vtValueCond, stLimit, false, bGetMKCout) == 0)
                {
                    return 1;
                }
                else
                {
                    int iRet2 = selectMKCache(current, vtField, mk, vtUKCond, vtValueCond, stLimit, bGetMKCout, vtData, iMKRecord);
                    {
                        if (iRet2 == 1)
                        {
                            return ET_NO_DATA;
                        }
                        else if (iRet == -2)
                        {
                            TLOGERROR("MKCacheImp::selectMKCache: error, select data count exceed the max, mainKey = " << mk << endl);
                            return ET_DATA_TOO_MUCH;
                        }
                        else if (iRet2 != 0)
                        {
                            TLOGERROR("MKCacheImp::selectMKCache: second error, mainKey = " << mk << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            return ET_SYS_ERR;
                        }
                    }
                }
            }
        }
        else if (iRet == -2)
        {
            TLOGERROR("MKCacheImp::selectMKCache: error, select data count exceed the max, mainKey = " << mk << endl);
            return ET_DATA_TOO_MUCH;
        }
        else
        {
            TLOGERROR("MKCacheImp::selectMKCache: error, mainKey = " << mk << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return ET_SYS_ERR;
        }
    }
    return 0;
}

bool MKCacheImp::isTransSrc(int pageNo)
{
    ServerInfo srcServer;
    ServerInfo destServer;
    g_route_table.getTrans(pageNo, srcServer, destServer);

    if (srcServer.serverName == (ServerConfig::Application + "." + ServerConfig::ServerName))
        return true;
    return false;
}

string MKCacheImp::getTransDest(int pageNo)
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

tars::Int32 MKCacheImp::getSyncTime(tars::TarsCurrentPtr current)
{
    return g_app.getSyncTime();
}

tars::Int32 MKCacheImp::getDeleteTime(tars::TarsCurrentPtr current)
{
    return g_app.gstat()->deleteTime();
}

int MKCacheImp::selectUKCache(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const string &uk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord)
{
    g_app.ppReport(PPReport::SRP_GET_CNT, 1);
    MultiHashMap::Value v;
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
        TarsDecode vDecode;
        vDecode.setBuffer(v._value);

        if (judge(vDecode, vtValueCond))
        {
            if (!stLimit.bLimit || (stLimit.bLimit && stLimit.iIndex == 0 && stLimit.iCount >= 1))
                setResult(vtField, mk, vtUKCond, vDecode, v._iVersion, v._iExpireTime, vtData);
        }
        if (bGetMKCout)
        {
            iMKRecord = g_HashMap.count(mk);
        }
        else
        {
            iMKRecord = vtData.size();
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
    {
        g_app.gstat()->hit(_hitIndex);
        if (bGetMKCout)
        {
            iMKRecord = g_HashMap.count(mk);
        }
        else
        {
            iMKRecord = 0;
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
    {
        if (_existDB  && _readDB)
        {
            return 1;
        }
        else
        {
            if (bGetMKCout)
            {
                iMKRecord = g_HashMap.count(mk);
            }
            else
            {
                iMKRecord = 0;
            }
        }
    }
    else
    {
        TLOGERROR("MKCacheImp::selectUKCache: g_HashMap.get(mk, uk, v) error, mainKey = " << mk << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return -1;
    }
    return 0;
}

int MKCacheImp::selectMKCache(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord)
{
    vector<MultiHashMap::Value> vtValue;

    g_app.gstat()->tryHit(_hitIndex);

    size_t iCount = -1;
    size_t iStart = 0;
    size_t iDataCount = 0;
    if (!stLimit.bLimit)
    {
        g_HashMap.get(mk, iDataCount);
        if (iDataCount > _mkeyMaxSelectCount)
        {
            FDLOG("MainKeyDataCount") << "[selectMKCache] mainKey=" << mk << " dataCount=" << iDataCount << endl;
            TLOGERROR("MKCacheImp::selectMKCache: g_HashMap.get(mk, iDataCount) error, mainKey = " << mk << " iDataCount = " << iDataCount << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return -2;
        }
    }
    if (stLimit.bLimit && vtUKCond.size() == 0 && vtValueCond.size() == 0)
    {
        iStart = stLimit.iIndex;
        iCount = stLimit.iCount;
    }

    int iRet;
    if (g_app.gstat()->isExpireEnabled())
    {
        iRet = g_HashMap.get(mk, vtValue, iCount, iStart, true, true, TC_TimeProvider::getInstance()->getNow());
    }
    else
    {
        iRet = g_HashMap.get(mk, vtValue, iCount, iStart);
    }

    TLOGDEBUG("get value size = " << vtValue.size() << ", ret = " << iRet << endl);

    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
    {
        g_app.gstat()->hit(_hitIndex);
        getSelectResult(vtField, mk, vtValue, vtUKCond, vtValueCond, stLimit, vtData);
        if (bGetMKCout)
        {
            iMKRecord = g_HashMap.count(mk);
            TLOGDEBUG("MKCacheImp::selectMKCache. Get " << vtData.size() << " result|g_HashMap.count(mk)=" << iMKRecord << "|mk=" << mk << "|vtUKCond.size()=" << vtUKCond.size() << "|vtValueCond.size()=" << vtValueCond.size() << "|stLimit.iCount=" << stLimit.iCount << endl);
        }
        else
        {
            iMKRecord = vtData.size();
            TLOGDEBUG("MKCacheImp::selectMKCache. Get " << iMKRecord << " result|g_HashMap.count(mk)=" << g_HashMap.count(mk) << "|mk=" << mk << "|vtUKCond.size()=" << vtUKCond.size() << "|vtValueCond.size()=" << vtValueCond.size() << "|stLimit.iCount=" << stLimit.iCount << endl);
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
    {
        g_app.ppReport(PPReport::SRP_GET_CNT, 1);
        g_app.gstat()->hit(_hitIndex);
        iMKRecord = 0;
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
    {
        g_app.ppReport(PPReport::SRP_GET_CNT, 1);
        if (_existDB  && _readDB)
        {
            return 1;
        }
        else
        {
            iMKRecord = 0;
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
    {
        if (_existDB  && _readDB)
        {
            return 2;
        }
        else
        {
            g_app.gstat()->hit(_hitIndex);
            getSelectResult(vtField, mk, vtValue, vtUKCond, vtValueCond, stLimit, vtData);
            if (bGetMKCout)
            {
                iMKRecord = g_HashMap.count(mk);
                TLOGDEBUG("MKCacheImp::selectMKCache. Get " << vtData.size() << " result|g_HashMap.count(mk)=" << iMKRecord << "|mk=" << mk << "|vtUKCond.size()=" << vtUKCond.size() << "|vtValueCond.size()=" << vtValueCond.size() << "|stLimit.iCount=" << stLimit.iCount << endl);
            }
            else
            {
                iMKRecord = vtData.size();
                TLOGDEBUG("MKCacheImp::selectMKCache. Get " << iMKRecord << " result|g_HashMap.count(mk)=" << g_HashMap.count(mk) << "|mk=" << mk << "|vtUKCond.size()=" << vtUKCond.size() << "|vtValueCond.size()=" << vtValueCond.size() << "|stLimit.iCount=" << stLimit.iCount << endl);
            }
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
    {
        //数据已标记为删除
        g_app.ppReport(PPReport::SRP_GET_CNT, 1);
        iMKRecord = 0;
    }
    else
    {
        TLOGERROR("MKCacheImp::selectMKCache: g_HashMap.get(mk, vtValue) error, mainKey = " << mk << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return -1;
    }
    return 0;
}

int MKCacheImp::selectMKCache(const vector<string> &vtField, const string &mk, const vector<vector<DCache::Condition> > & vtCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord)
{
    vector<MultiHashMap::Value> vtValue;

    g_app.gstat()->tryHit(_hitIndex);

    size_t iCount = -1;
    size_t iStart = 0;
    size_t iDataCount = 0;
    if (!stLimit.bLimit)
    {
        g_HashMap.get(mk, iDataCount);
        if (iDataCount > _mkeyMaxSelectCount)
        {
            FDLOG("MainKeyDataCount") << "[selectMKCache] mainKey=" << mk << " dataCount=" << iDataCount << endl;
            TLOGERROR("MKCacheImp::selectMKCache: g_HashMap.get(mk, iDataCount) error, mainKey = " << mk << " iDataCount = " << iDataCount << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return -2;
        }
    }
    if (stLimit.bLimit && vtCond.size() == 0)
    {
        iStart = stLimit.iIndex;
        iCount = stLimit.iCount;
    }

    int iRet;
    if (g_app.gstat()->isExpireEnabled())
    {
        iRet = g_HashMap.get(mk, vtValue, iCount, iStart, true, true, TC_TimeProvider::getInstance()->getNow());
    }
    else
    {
        iRet = g_HashMap.get(mk, vtValue, iCount, iStart);
    }

    TLOGDEBUG("get value size = " << vtValue.size() << endl);

    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
    {
        g_app.gstat()->hit(_hitIndex);
        getSelectResult(vtField, mk, vtValue, vtCond, stLimit, vtData);
        if (bGetMKCout)
        {
            iMKRecord = g_HashMap.count(mk);
            TLOGDEBUG("MKCacheImp::selectMKCache. Get " << vtData.size() << " result|g_HashMap.count(mk)=" << iMKRecord << "|mk=" << mk << "|vtCond.size()=" << vtCond.size() << "|stLimit.iCount=" << stLimit.iCount << endl);
        }
        else
        {
            iMKRecord = vtData.size();
            TLOGDEBUG("MKCacheImp::selectMKCache. Get " << iMKRecord << " result|g_HashMap.count(mk)=" << g_HashMap.count(mk) << "|mk=" << mk << "|vtCond.size()=" << vtCond.size() << "|stLimit.iCount=" << stLimit.iCount << endl);
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
    {
        g_app.ppReport(PPReport::SRP_GET_CNT, 1);
        g_app.gstat()->hit(_hitIndex);
        iMKRecord = 0;
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
    {
        g_app.ppReport(PPReport::SRP_GET_CNT, 1);
        if (_existDB  && _readDB)
        {
            return 1;
        }
        else
        {
            iMKRecord = 0;
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
    {
        if (_existDB  && _readDB)
        {
            return 2;
        }
        else
        {
            g_app.gstat()->hit(_hitIndex);
            getSelectResult(vtField, mk, vtValue, vtCond, stLimit, vtData);
            if (bGetMKCout)
            {
                iMKRecord = g_HashMap.count(mk);
                TLOGDEBUG("MKCacheImp::selectMKCache. Get " << vtData.size() << " result|g_HashMap.count(mk)=" << iMKRecord << "|mk=" << mk << "|vtCond.size()=" << vtCond.size() << "|stLimit.iCount=" << stLimit.iCount << endl);
            }
            else
            {
                iMKRecord = vtData.size();
                TLOGDEBUG("MKCacheImp::selectMKCache. Get " << iMKRecord << " result|g_HashMap.count(mk)=" << g_HashMap.count(mk) << "|mk=" << mk << "|vtCond.size()=" << vtCond.size() << "|stLimit.iCount=" << stLimit.iCount << endl);
            }
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
    {
        //数据已标记为删除
        g_app.ppReport(PPReport::SRP_GET_CNT, 1);
        iMKRecord = 0;
    }
    else
    {
        TLOGERROR("MKCacheImp::selectMKCache: g_HashMap.get(mk, vtValue) error, mainKey = " << mk << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return -1;
    }
    return 0;
}

int MKCacheImp::asyncSelect(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, bool bUkey, tars::Bool bGetMKCout)
{
    MKDbAccessCBParam::SelectCBParam param;
    param.current = current;
    param.mk = mk;
    param.vtField = vtField;
    param.bUKey = bUkey;
    param.vtUKCond = vtUKCond;
    param.vtValueCond = vtValueCond;
    param.stLimit = stLimit;
    param.bGetMKCout = bGetMKCout;
    param.bBatch = false;
    param.bMUKBatch = false;
    param.bBatchOr = false;

    bool isCreate;
    MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
    int iAddret = pCBParam->AddSelect(param);
    try
    {
        if (iAddret == 0)
        {
            current->setResponse(false);
            if (isCreate)
            {
                TLOGDEBUG("MKCacheImp::asyncSelect: async select db, mainKey = " << mk << endl);
                DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount);
                //异步调用DBAccess
                asyncDbSelect(mk, cb);
            }
            else
            {
                TLOGDEBUG("MKCacheImp::asyncSelect: set into cbparam, mainKey = " << mk << endl);
            }
            return 0;
        }
    }
    catch (std::exception& ex)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncSelect: async select db exception, mainKey = " << mk << "," << ex.what() << endl);
        throw;
    }
    catch (...)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncSelect: async select db unkown exception, mainKey = " << mk << endl);
        throw;
    }
    return 1;
}

int MKCacheImp::asyncGetSet(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk)
{
    MKDbAccessCBParam::GetSetCBParam param;
    param.current = current;
    param.vtField = vtField;
    param.mk = mk;

    bool isCreate;
    MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
    int iAddret = pCBParam->AddGetSet(param);
    try
    {
        if (iAddret == 0)
        {
            current->setResponse(false);
            if (isCreate)
            {
                TLOGDEBUG("MKCacheImp::asyncGetSet: async select db, mainKey = " << mk << endl);
                DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, TC_Multi_HashMap_Malloc::MainKey::SET_TYPE);
                //异步调用DBAccess
                asyncDbSelect(mk, cb);
            }
            else
            {
                TLOGDEBUG("MKCacheImp::asyncGetSet: set into cbparam, mainKey = " << mk << endl);
            }
            return 0;
        }
    }
    catch (std::exception& ex)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetSet: async select db exception, mainKey = " << mk << "," << ex.what() << endl);
        throw;
    }
    catch (...)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetSet: async select db unkown exception, mainKey = " << mk << endl);
        throw;
    }
    return 1;
}

int MKCacheImp::asyncGetZSet(tars::TarsCurrentPtr current, const string &mk, const string &value)
{
    MKDbAccessCBParam::GetScoreZSetCBParam param;
    param.current = current;
    param.value = value;
    param.mk = mk;

    bool isCreate;
    MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
    int iAddret = pCBParam->AddGetScoreZSet(param);
    try
    {
        if (iAddret == 0)
        {
            current->setResponse(false);
            if (isCreate)
            {
                TLOGDEBUG("MKCacheImp::asyncGetZSet: async select db, mainKey = " << mk << endl);
                DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE);
                //异步调用DBAccess
                asyncDbSelect(mk, cb);
            }
            else
            {
                TLOGDEBUG("MKCacheImp::asyncGetZSet: set into cbparam, mainKey = " << mk << endl);
            }
            return 0;
        }
    }
    catch (std::exception& ex)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetZSet: async select db exception, mainKey = " << mk << "," << ex.what() << endl);
        throw;
    }
    catch (...)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetZSet: async select db unkown exception, mainKey = " << mk << endl);
        throw;
    }
    return 1;
}

int MKCacheImp::asyncGetZSet(tars::TarsCurrentPtr current, const string &mk, const string &value, const bool bOrder)
{
    MKDbAccessCBParam::GetRankZSetCBParam param;
    param.current = current;
    param.value = value;
    param.mk = mk;
    param.bOrder = bOrder;

    bool isCreate;
    MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
    int iAddret = pCBParam->AddGetRankZSet(param);
    try
    {
        if (iAddret == 0)
        {
            current->setResponse(false);
            if (isCreate)
            {
                TLOGDEBUG("MKCacheImp::asyncGetZSet: async select db, mainKey = " << mk << endl);
                DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE);
                //异步调用DBAccess
                asyncDbSelect(mk, cb);
            }
            else
            {
                TLOGDEBUG("MKCacheImp::asyncGetZSet: set into cbparam, mainKey = " << mk << endl);
            }
            return 0;
        }
    }
    catch (std::exception& ex)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetZSet: async select db exception, mainKey = " << mk << "," << ex.what() << endl);
        throw;
    }
    catch (...)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetZSet: async select db unkown exception, mainKey = " << mk << endl);
        throw;
    }
    return 1;
}

int MKCacheImp::asyncGetZSet(tars::TarsCurrentPtr current, const string &mk, const vector<string> &field, long iStart, long iEnd, bool bUp)
{
    MKDbAccessCBParam::GetRangeZSetCBParam param;
    param.current = current;
    param.mk = mk;
    param.vtField = field;
    param.iStart = iStart;
    param.iEnd = iEnd;
    param.bUp = bUp;

    bool isCreate;
    MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
    int iAddret = pCBParam->AddGetRangeZSet(param);
    try
    {
        if (iAddret == 0)
        {
            current->setResponse(false);
            if (isCreate)
            {
                TLOGDEBUG("MKCacheImp::asyncGetZSet: async select db, mainKey = " << mk << endl);
                DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE);
                //异步调用DBAccess
                asyncDbSelect(mk, cb);
            }
            else
            {
                TLOGDEBUG("MKCacheImp::asyncGetZSet: set into cbparam, mainKey = " << mk << endl);
            }
            return 0;
        }
    }
    catch (std::exception& ex)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetZSet: async select db exception, mainKey = " << mk << "," << ex.what() << endl);
        throw;
    }
    catch (...)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetZSet: async select db unkown exception, mainKey = " << mk << endl);
        throw;
    }
    return 1;
}

int MKCacheImp::asyncGetZSetByScore(tars::TarsCurrentPtr current, const string &mk, const vector<string> &field, double iMin, double iMax)
{
    MKDbAccessCBParam::GetRangeZSetByScoreCBParam param;
    param.current = current;
    param.mk = mk;
    param.vtField = field;
    param.iMin = iMin;
    param.iMax = iMax;

    bool isCreate;
    MKDbAccessCBParamPtr pCBParam = g_cbQueue.getCBParamPtr(mk, isCreate);
    int iAddret = pCBParam->AddGetRangeZSetByScore(param);
    try
    {
        if (iAddret == 0)
        {
            current->setResponse(false);
            if (isCreate)
            {
                TLOGDEBUG("MKCacheImp::asyncGetZSetByScore: async select db, mainKey = " << mk << endl);
                DbAccessPrxCallbackPtr cb = new MKDbAccessCallback(pCBParam, _binlogFile, _recordBinLog, _recordKeyBinLog, _saveOnlyKey, _insertAtHead, _updateInOrder, _orderItem, _orderByDesc, _mkeyMaxSelectCount, TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE);
                //异步调用DBAccess
                asyncDbSelect(mk, cb);
            }
            else
            {
                TLOGDEBUG("MKCacheImp::asyncGetZSetByScore: set into cbparam, mainKey = " << mk << endl);
            }
            return 0;
        }
    }
    catch (std::exception& ex)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetZSetByScore: async select db exception, mainKey = " << mk << "," << ex.what() << endl);
        throw;
    }
    catch (...)
    {
        g_cbQueue.erase(mk);
        TLOGERROR("MKCacheImp::asyncGetZSetByScore: async select db unkown exception, mainKey = " << mk << endl);
        throw;
    }
    return 1;
}

