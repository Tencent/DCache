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
#include <functional>

#include "Global.h"
#include "ProxyImp.h"
#include "RouterShare.h"
#include "KVCacheCallback.h"
#include "MKVCacheCallback.h"
#include "CacheCallbackComm.h"

using namespace tars;
using namespace std;
using namespace DCache;

ProxyImp::ProxyImp()
    : _cacheProxyFactory(NULL), _printReadLog(false), _printWriteLog(false), _idcArea("")
{
    _printLogModules.clear();
}

void ProxyImp::initialize()
{
    TLOGDEBUG("begin to initialize ProxyImp servant ..." << endl);

    // 载入配置文件
    TC_Config conf;
    conf.parseFile(ServerConfig::BasePath + "ProxyServer.conf");

    // 需要打印日志的模块
    string sPrintLogModule = conf.get("/Main<PrintLogModule>");
    vector<string> moduleList = TC_Common::sepstr<string>(sPrintLogModule, "|", false);
    _printLogModules.insert(moduleList.begin(), moduleList.end());

    _idcArea = conf["/Main<IdcArea>"];
    
    string printLogType = conf.get("/Main<PrintLogType>", "r");
    vector<string> pLogType = TC_Common::sepstr<string>(printLogType, "|", false);
    for (size_t i = 0; i < pLogType.size(); i++)
    {
        if (pLogType[i].find("r") != string::npos)
        {
            _printReadLog = true;
        }
        else if (pLogType[i].find("w") != string::npos)
        {
            _printWriteLog = true;
        }
    }

    assert(g_app._routerTableInfoFactory != NULL);
    _cacheProxyFactory = new CacheProxyFactory(Application::getCommunicator(), g_app._routerTableInfoFactory);
}

void ProxyImp::logBatchCount(const string &moduleName, const string &callerName, size_t uCount)
{
    if (g_app._printBatchCount)
    {
        FDLOG("batchCount") << "moduleName | " << moduleName << ", | caller = " << callerName << ", | Batch size = " << uCount << endl;
    }
}

int ProxyImp::getKV(const GetKVReq &req, GetKVRsp &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &key = req.keyItem;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", key = " << key << ", from ip = " << current->getIp() << endl);

    if (_printReadLog && _printLogModules.count(req.moduleName))
    {
        FDLOG("get") << req.moduleName << "|" << req.keyItem << "|" << req.idcSpecified << "|" << current->getIp() << endl;
    }

    if (req.keyItem.empty())
    {
        TLOGERROR("ProxyImp::getKV key is empty module:" << req.moduleName << "|master:" << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    string objectName;
    CachePrx prxCache;
    
    int ret = _cacheProxyFactory->getCacheProxy(req.moduleName, req.keyItem, objectName, prxCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getKV";
    }

    // 发起异步调用前设置禁止应答，等收到cache回包后再应答客户端
    current->setResponse(false);
    try
    {
        function<void(TarsCurrentPtr, Int32, const GetKVRsp &)> dealWithRsp = Proxy::async_response_getKV;
        CachePrxCallbackPtr cb = new GetKVCallback(current, req, objectName, TNOWMS, dealWithRsp, idcArea);
        prxCache->async_getKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("ProxyImp::getKV async call exception:" << ex.what() << "|module:" << req.moduleName << "|key:" << req.keyItem << "|master:" << current->getIp() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getKVBatch(const GetKVBatchReq &req, GetKVBatchRsp &rsp, TarsCurrentPtr current)
{
    const vector<string> &keys = req.keys;
    const string &moduleName = req.moduleName;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", recv " << keys.size() << " keys, from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getKVBatch";
    }

    if (_printReadLog && _printLogModules.count(moduleName) && keys.size() > 0)
    {
        FDLOG("get") << keys[0] << "|" << __FUNCTION__ << "|" << keys.size() << "|" << moduleName << endl;
    }

    //检查key的数量是否在限制范围内
    size_t keyCount = keys.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::getKVBatch] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    //读操作时需要这个
    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    map<string, GetKVBatchReq> mProxyKeyItem;
    map<string, CachePrx> mProxyCachePrx;
    string objectName;
    CachePrx prxCache;
    for (size_t i = 0; i < keys.size(); i++)
    {
        string key = keys[i];
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }

        int ret = _cacheProxyFactory->getCacheProxy(moduleName, key, objectName, prxCache, idcArea);
        if (ret != ET_SUCC)
        {
            return ret;
        }
        mProxyKeyItem[objectName].keys.push_back(key);
        mProxyCachePrx[objectName] = prxCache;
    }

    BatchCallParamPtr pParam = new CacheBatchCallParam<GetKVBatchRsp>(mProxyKeyItem.size());

    current->setResponse(false);
    try
    {
        map<string, GetKVBatchReq>::iterator mIt = mProxyKeyItem.begin();
        for (; mIt != mProxyKeyItem.end(); ++mIt)
        {
            string objectName = mIt->first;
            GetKVBatchReq &partReq = mIt->second;
            partReq.moduleName = moduleName;
            partReq.idcSpecified = idcArea;

            CachePrxCallbackPtr cb = new GetKVBatchCallback(current, pParam, partReq, objectName, TNOWMS, idcArea);
            mProxyCachePrx[objectName]->async_getKVBatch(cb, partReq);
        }
        return ET_SUCC;
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getMKVBatch] exception:" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[ProxyImp::getStringBatch] UnkownException" << endl);
    }

    //当发生未知（不可恢复）异常（系统错误时），提起结束本次调用并返回空数据。（读和写操作对此种错误的响应机制不一样）!!!dengyouwang
    CacheBatchCallParam<GetKVBatchRsp> *tmpParam = (CacheBatchCallParam<GetKVBatchRsp> *)(pParam.get());
    if (tmpParam->setEnd())
    {
        GetKVBatchRsp tempRsp;
        Proxy::async_response_getKVBatch(current, ET_SYS_ERR, tempRsp);
    }
    return ET_SYS_ERR;
}

int ProxyImp::checkKey(const CheckKeyReq &req, CheckKeyRsp &rsp, TarsCurrentPtr current)
{
    const string moduleName = req.moduleName;
    const vector<string> &keys = req.keys;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", recv " << keys.size() << " keys, from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "checkKey";
        if (_printReadLog && _printLogModules.count(moduleName) && keys.size() > 0)
        {
            FDLOG("checkKey") << keys[0] << "|" << keys.size() << "|" << moduleName << endl;
        }
    }

    //检查key的数量在限制范围内
    size_t keyCount = keys.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::checkKey] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    map<string, CheckKeyReq> mProxyKeyItem;
    map<string, CachePrx> mProxyCachePrx;
    string objectName;
    CachePrx prxCache;
    for (size_t i = 0; i < keys.size(); i++)
    {
        string key = keys[i];
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }
        int ret = _cacheProxyFactory->getCacheProxy(moduleName, key, objectName, prxCache, idcArea);
        if (ret != ET_SUCC)
        {
            return ret;
        }
        mProxyKeyItem[objectName].keys.push_back(key);
        mProxyCachePrx[objectName] = prxCache;
    }

    BatchCallParamPtr pParam = new CacheBatchCallParam<CheckKeyRsp>(mProxyKeyItem.size());

    current->setResponse(false);
    try
    {
        map<string, CheckKeyReq>::iterator mIt = mProxyKeyItem.begin();
        for (; mIt != mProxyKeyItem.end(); ++mIt)
        {
            string objectName = mIt->first;
            CheckKeyReq &partReq = mIt->second;
            partReq.moduleName = moduleName;
            CachePrxCallbackPtr cb = new CheckKeyCallback(current, pParam, partReq, objectName, TNOWMS, idcArea);

            mProxyCachePrx[objectName]->async_checkKey(cb, partReq);
        }
        return ET_SUCC;
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::checkKey] exception:" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[ProxyImp::checkKey] UnkownException" << endl);
    }
    //当发生未知（不可恢复）异常（系统错误）时，提起结束本次调用并返回空数据。（读和写操作对此种错误的响应机制不一样）!!!dengyouwang
    CacheBatchCallParam<CheckKeyRsp> *tmpParam = (CacheBatchCallParam<CheckKeyRsp> *)(pParam.get());
    if (tmpParam->setEnd())
    {
        CheckKeyRsp tempRsp;
        Proxy::async_response_checkKey(current, ET_SYS_ERR, tempRsp);
    }
    return ET_SYS_ERR;
}

int ProxyImp::insertKV(const SetKVReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &key = req.data.keyItem;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", key = " << key << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "insertKV";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("insertKV") << key << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (key.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    WCachePrx prxWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        WCachePrxCallbackPtr cb = new InsertKVCallback(current, req, objectName, TNOWMS);
        prxWCache->async_insertKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::insertKV] async_insertKV exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::setKV(const SetKVReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &key = req.data.keyItem;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", key = " << key << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "setKV";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("setKV") << key << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (key.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    WCachePrx prxWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        WCachePrxCallbackPtr cb = new SetKVCallback(current, req, objectName, TNOWMS);
        prxWCache->async_setKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::setKV] async_setKV exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::setKVBatch(const SetKVBatchReq &req, SetKVBatchRsp &rsp, TarsCurrentPtr current)
{
    const string moduleName = req.moduleName;
    const vector<SSetKeyValue> &keyValue = req.data;

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "setKVBatch";
        if (_printWriteLog && _printLogModules.count(moduleName) && keyValue.size() > 0)
        {
            FDLOG("set") << keyValue[0].keyItem << "|" << __FUNCTION__ << "|" << keyValue.size() << "|" << moduleName << endl;
        }
    }

    //检查key的数量在限制范围内
    size_t keyCount = keyValue.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::setKVBatch] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    map<string, SetKVBatchReq> mProxyKeyItem;
    map<string, WCachePrx> mProxyCachePrx;
    string objectName;
    WCachePrx prxWCache;
    for (size_t i = 0; i < keyValue.size(); i++)
    {
        string key = keyValue[i].keyItem;
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }
        int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxWCache);
        if (ret != ET_SUCC)
        {
            rsp.keyResult[key] = SET_ERROR;
            // TODO(dengyouwang): add log? because it will hardly happen!
            continue;
        }
        mProxyKeyItem[objectName].data.push_back(keyValue[i]);
        mProxyCachePrx[objectName] = prxWCache;
    }

    if (mProxyKeyItem.empty())
    {
        rsp.keyResult.clear();
        return ET_KEY_INVALID;
    }

    BatchCallParamPtr pParam = new WCacheBatchCallParam<SetKVBatchRsp>(mProxyKeyItem.size());

    if (!rsp.keyResult.empty())
    {
        ((WCacheBatchCallParam<SetKVBatchRsp> *)(pParam.get()))->addResult(rsp.keyResult);
    }

    current->setResponse(false);
    map<string, SetKVBatchReq>::iterator mIt = mProxyKeyItem.begin();
    for (; mIt != mProxyKeyItem.end(); ++mIt)
    {
        try
        {
            string objectName = mIt->first;
            SetKVBatchReq &partReq = mIt->second;
            partReq.moduleName = moduleName;
            WCachePrxCallbackPtr cb = new SetKVBatchCallback(current, pParam, partReq, objectName, TNOWMS);
            mProxyCachePrx[objectName]->async_setKVBatch(cb, partReq);
            continue;
        }
        catch (exception &e)
        {
            TLOGERROR("[ProxyImp::setKVBatchBatch] exception:" << e.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("[ProxyImp::setKVBatchBatch] UnkownException" << endl);
        }

        SetKVBatchRsp tempRsp;
        for (size_t i = 0; i < mIt->second.data.size(); i++)
        {
            tempRsp.keyResult[(mIt->second.data)[i].keyItem] = SET_ERROR;
        }
        WCacheBatchCallParam<SetKVBatchRsp> *tmpParam = (WCacheBatchCallParam<SetKVBatchRsp> *)(pParam.get());
        tmpParam->addResult(tempRsp.keyResult);

        if (pParam->_count.dec() <= 0)
        {
            Proxy::async_response_setKVBatch(current, ET_PARTIAL_FAIL, tmpParam->_rsp);
        }
    }

    return ET_SUCC;
}

int ProxyImp::updateKV(const UpdateKVReq &req, UpdateKVRsp &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &key = req.data.keyItem;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", key = " << key << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "updateKV";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("updateKV") << key << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (key.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    WCachePrx prxWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        WCachePrxCallbackPtr cb = new UpdateKVCallback(current, req, objectName, TNOWMS);
        prxWCache->async_updateKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::updateStringEx] async_setStringEx exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::eraseKV(const RemoveKVReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &key = req.keyInfo.keyItem;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", key = " << key << ", version = " << req.keyInfo.version << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "eraseKV";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("eraseKV") << key << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (key.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    WCachePrx prxWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        WCachePrxCallbackPtr cb = new EraseKVCallback(current, req, objectName, TNOWMS);
        prxWCache->async_eraseKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::eraseString] async_eraseKV exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::eraseKVBatch(const RemoveKVBatchReq &req, RemoveKVBatchRsp &rsp, TarsCurrentPtr current)
{
    const string moduleName = req.moduleName;
    const vector<KeyInfo> &keys = req.data;

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "eraseKVBatch";
        if (_printWriteLog && _printLogModules.count(moduleName) && keys.size() > 0)
        {
            FDLOG("erase") << keys[0].keyItem << "|" << __FUNCTION__ << "|" << keys.size() << "|" << moduleName << endl;
        }
    }

    //检查key的数量在限制范围内
    size_t keyCount = keys.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::eraseKVBatch] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    map<string, RemoveKVBatchReq> mProxyKeyItem;
    map<string, WCachePrx> mProxyCachePrx;
    string objectName;
    WCachePrx prxWCache;
    for (size_t i = 0; i < keys.size(); i++)
    {
        string key = keys[i].keyItem;
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }
        int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxWCache);
        if (ret != ET_SUCC)
        {
            rsp.keyResult[key] = DEL_ERROR;
            continue;
        }
        mProxyKeyItem[objectName].data.push_back(keys[i]);
        mProxyCachePrx[objectName] = prxWCache;
    }

    if (mProxyKeyItem.empty())
    {
        rsp.keyResult.clear();
        return ET_KEY_INVALID;
    }

    BatchCallParamPtr pParam = new WCacheBatchCallParam<RemoveKVBatchRsp>(mProxyKeyItem.size());

    if (!rsp.keyResult.empty())
    {
        ((WCacheBatchCallParam<RemoveKVBatchRsp> *)pParam.get())->addResult(rsp.keyResult);
    }

    current->setResponse(false);
    map<string, RemoveKVBatchReq>::iterator mIt = mProxyKeyItem.begin();
    for (; mIt != mProxyKeyItem.end(); mIt++)
    {
        try
        {
            string objectName = mIt->first;
            RemoveKVBatchReq &partReq = mIt->second;
            partReq.moduleName = moduleName;
            WCachePrxCallbackPtr cb = new EraseKVBatchCallback(current, pParam, partReq, objectName, TNOWMS);
            mProxyCachePrx[objectName]->async_eraseKVBatch(cb, partReq);
            continue;
        }
        catch (exception &e)
        {
            TLOGERROR("[ProxyImp::eraseKVBatch] exception:" << e.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("[ProxyImp::eraseKVBatch] UnkownException" << endl);
        }

        RemoveKVBatchRsp tempRsp;
        for (size_t i = 0; i < (mIt->second).data.size(); i++)
        {
            tempRsp.keyResult[(mIt->second).data[i].keyItem] = DEL_ERROR;
        }
        WCacheBatchCallParam<RemoveKVBatchRsp> *tmpParam = (WCacheBatchCallParam<RemoveKVBatchRsp> *)(pParam.get());
        tmpParam->addResult(tempRsp.keyResult);

        if (pParam->_count.dec() <= 0)
        {
            Proxy::async_response_eraseKVBatch(current, ET_PARTIAL_FAIL, tmpParam->_rsp);
        }
    }
    return ET_SUCC;
}

int ProxyImp::delKV(const RemoveKVReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &key = req.keyInfo.keyItem;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", key = " << key << ", version = " << req.keyInfo.version << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "delKV";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("del") << key << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (key.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    WCachePrx prxWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxWCache, false);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        WCachePrxCallbackPtr cb = new DelKVCallback(current, req, objectName, TNOWMS);
        prxWCache->async_delKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::delString] async_delKV exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::delKVBatch(const RemoveKVBatchReq &req, RemoveKVBatchRsp &rsp, TarsCurrentPtr current)
{
    const string moduleName = req.moduleName;
    const vector<KeyInfo> &keys = req.data;

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "delKVBatch";
        if (_printWriteLog && _printLogModules.count(moduleName) && keys.size() > 0)
        {
            FDLOG("del") << keys[0].keyItem << "|" << __FUNCTION__ << "|" << keys.size() << "|" << moduleName << endl;
        }
    }

    //检查key的数量在限制范围内
    size_t keyCount = keys.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::delKVBatch] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    map<string, RemoveKVBatchReq> mProxyKeyItem;
    map<string, WCachePrx> mProxyCachePrx;
    string objectName;
    WCachePrx prxWCache;
    for (size_t i = 0; i < keys.size(); i++)
    {
        string key = keys[i].keyItem;
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }
        int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxWCache);
        if (ret != ET_SUCC)
        {
            rsp.keyResult[key] = WRITE_ERROR;
            continue;
        }
        mProxyKeyItem[objectName].data.push_back(keys[i]);
        mProxyCachePrx[objectName] = prxWCache;
    }

    if (mProxyKeyItem.empty())
    {
        rsp.keyResult.clear();
        return ET_KEY_INVALID;
    }

    BatchCallParamPtr pParam = new WCacheBatchCallParam<RemoveKVBatchRsp>(mProxyKeyItem.size());

    current->setResponse(false);
    map<string, RemoveKVBatchReq>::iterator mIt = mProxyKeyItem.begin();
    for (; mIt != mProxyKeyItem.end(); mIt++)
    {
        try
        {
            const string &objectName = mIt->first;
            RemoveKVBatchReq &partReq = mIt->second;
            partReq.moduleName = moduleName;
            WCachePrxCallbackPtr cb = new DelKVBatchCallback(current, pParam, partReq, objectName, TNOWMS);
            mProxyCachePrx[objectName]->async_delKVBatch(cb, partReq);
            continue;
        }
        catch (exception &e)
        {
            TLOGERROR("[ProxyImp::delKVBatch] exception:" << e.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("[ProxyImp::delKVBatch] UnkownException" << endl);
        }

        RemoveKVBatchRsp tempRsp;
        for (size_t i = 0; i < (mIt->second).data.size(); i++)
        {
            tempRsp.keyResult[(mIt->second).data[i].keyItem] = WRITE_ERROR;
        }
        WCacheBatchCallParam<RemoveKVBatchRsp> *tmpParam = (WCacheBatchCallParam<RemoveKVBatchRsp> *)(pParam.get());
        tmpParam->addResult(tempRsp.keyResult);

        if (pParam->_count.dec() <= 0)
        {
            Proxy::async_response_delKVBatch(current, ET_PARTIAL_FAIL, tmpParam->_rsp);
        }
    }

    return ET_SUCC;
}

//---------------------------------DCache二期接口-----------------------------//
int ProxyImp::getMKV(const GetMKVReq &req, GetMKVRsp &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", mainKey = " << mainKey << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getMKV";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getMKV") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::getMKV mainKey is empty, module:" << moduleName << "|master:" << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    MKCachePrx prxMKCache;
    string objectName;
    int ret = _cacheProxyFactory->getCacheProxy(moduleName, mainKey, objectName, prxMKCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    // 异步调用CacheServer
    current->setResponse(false);
    try
    {
        MKCachePrxCallbackPtr cb = new GetMKVCallback(current, req, objectName, TNOWMS, idcArea);
        prxMKCache->async_getMKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getMKV] async_getMKV exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getMKVBatch(const MKVBatchReq &req, MKVBatchRsp &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const vector<string> &vtMainKey = req.mainKeys;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", recv " << vtMainKey.size() << " mainKeys, from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getMKVBatch";
        if (_printReadLog && _printLogModules.count(moduleName) && vtMainKey.size() > 0)
        {
            FDLOG("getMKVBatch") << vtMainKey[0] << "|" << vtMainKey.size() << "|" << moduleName << endl;
        }
    }

    //批量操作时，对mainKey数量有限制
    size_t keyCount = vtMainKey.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::getMKVBatch] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    //根据key将请求拆分
    map<string, MKVBatchReq> mProxyKeyItem;
    map<string, MKCachePrx> mProxyCachePrx;
    string objectName;
    MKCachePrx prxMKCache;
    for (size_t i = 0; i < vtMainKey.size(); i++)
    {
        string key = vtMainKey[i];
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }

        int ret = _cacheProxyFactory->getCacheProxy(moduleName, key, objectName, prxMKCache, idcArea);
        if (ret != ET_SUCC)
        {
            return ret;
        }
        mProxyKeyItem[objectName].mainKeys.push_back(key);
        mProxyCachePrx[objectName] = prxMKCache;
    }

    BatchCallParamPtr pParam = new MKCacheBatchCallParam<MKVBatchRsp>(mProxyKeyItem.size());

    current->setResponse(false);
    try
    {
        map<string, MKVBatchReq>::iterator mIt = mProxyKeyItem.begin();
        for (; mIt != mProxyKeyItem.end(); ++mIt)
        {
            string objectName = mIt->first;
            MKVBatchReq &partReq = mIt->second;
            partReq.moduleName = req.moduleName;
            partReq.field = req.field;
            partReq.cond = req.cond;

            MKCachePrxCallbackPtr cb = new GetMKVBatchCallback(current, pParam, partReq, objectName, TNOWMS, idcArea);
            mProxyCachePrx[objectName]->async_getMKVBatch(cb, partReq);
        }
        return ET_SUCC;
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getMKVBatch] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[ProxyImp::getMKVBatch] unkown exception" << endl);
    }
    //当发生未知（不可恢复）异常（系统错误时），提起结束本次调用并返回空数据。（读和写操作对此种错误的响应机制不一样）!!!dengyouwang
    MKCacheBatchCallParam<MKVBatchRsp> *tmpParam = (MKCacheBatchCallParam<MKVBatchRsp> *)(pParam.get());
    if (tmpParam->setEnd())
    {
        MKVBatchRsp tempRsp;
        Proxy::async_response_getMKVBatch(current, ET_SYS_ERR, tempRsp);
    }
    return ET_SYS_ERR;
}

bool ProxyImp::checkKeyCount(size_t keyCount)
{
    bool outOfRange = false;

    if (keyCount == 0 || (g_app._keyCountLimit && keyCount > g_app._keyCountLimit))
    {
        outOfRange = true;
    }

    return outOfRange;
}

int ProxyImp::getMUKBatch(const MUKBatchReq &req, MUKBatchRsp &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const vector<Record> &vtMUKey = req.primaryKeys;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", recv " << vtMUKey.size() << " mainKeys, from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getMUKBatch";
        if (_printReadLog && _printLogModules.count(moduleName) && vtMUKey.size() > 0)
        {
            FDLOG("getMKV") << vtMUKey[0].mainKey << "|" << __FUNCTION__ << "|" << vtMUKey.size() << "|" << moduleName << endl;
        }
    }

    //批量操作时，对mainKey数量有限制
    size_t keyCount = vtMUKey.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::getMUKBatch] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    //读操作时需要这个
    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    //根据key将请求拆分
    map<string, MUKBatchReq> mProxyKeyItem;
    map<string, MKCachePrx> mProxyCachePrx;
    string objectName;
    MKCachePrx prxMKCache;
    for (size_t i = 0; i < vtMUKey.size(); i++)
    {
        string key = vtMUKey[i].mainKey;
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }

        int ret = _cacheProxyFactory->getCacheProxy(moduleName, key, objectName, prxMKCache, idcArea);
        if (ret != ET_SUCC)
        {
            return ret;
        }
        mProxyKeyItem[objectName].primaryKeys.push_back(vtMUKey[i]);
        mProxyCachePrx[objectName] = prxMKCache;
    }

    BatchCallParamPtr pParam = new MKCacheBatchCallParam<MUKBatchRsp>(mProxyKeyItem.size());

    current->setResponse(false);
    try
    {
        map<string, MUKBatchReq>::iterator mIt = mProxyKeyItem.begin();
        for (; mIt != mProxyKeyItem.end(); ++mIt)
        {
            string objectName = mIt->first;
            MUKBatchReq &partReq = mIt->second;
            partReq.moduleName = req.moduleName;
            partReq.field = req.field;

            MKCachePrxCallbackPtr cb = new GetMUKBatchCallback(current, pParam, partReq, objectName, TNOWMS, idcArea);
            mProxyCachePrx[objectName]->async_getMUKBatch(cb, partReq);
        }
        return ET_SUCC;
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getMUKBatch] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[ProxyImp::getMUKBatch] unkown exception" << endl);
    }
    //当发生未知（不可恢复）异常（系统错误时），提起结束本次调用并返回空数据。（读和写操作对此种错误的响应机制不一样）!!!dengyouwang
    MKCacheBatchCallParam<MUKBatchRsp> *tmpParam = (MKCacheBatchCallParam<MUKBatchRsp> *)(pParam.get());
    if (tmpParam->setEnd())
    {
        MUKBatchRsp tempRsp;
        Proxy::async_response_getMUKBatch(current, ET_SYS_ERR, tempRsp);
    }
    return ET_SYS_ERR;
}

int ProxyImp::getMKVBatchEx(const MKVBatchExReq &req, MKVBatchExRsp &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const vector<MainKeyCondition> &vtKey = req.cond;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", recv " << vtKey.size() << " mainKeys, from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getMKVBatchEx";
        if (_printReadLog && _printLogModules.count(moduleName) && vtKey.size() > 0)
        {
            FDLOG("getMKV") << vtKey[0].mainKey << "|" << __FUNCTION__ << "|" << vtKey.size() << "|" << moduleName << endl;
        }
    }

    //批量操作时，对mainKey数量有限制
    size_t keyCount = vtKey.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::getMKVBatchEx] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    //读操作时需要这个
    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    //根据key将请求拆分
    map<string, MKVBatchExReq> mProxyKeyItem;
    map<string, MKCachePrx> mProxyCachePrx;
    string objectName;
    MKCachePrx prxMKCache;
    for (size_t i = 0; i < vtKey.size(); i++)
    {
        string key = vtKey[i].mainKey;
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }

        int ret = _cacheProxyFactory->getCacheProxy(moduleName, key, objectName, prxMKCache, idcArea);
        if (ret != ET_SUCC)
        {
            return ret;
        }
        mProxyKeyItem[objectName].cond.push_back(vtKey[i]);
        mProxyCachePrx[objectName] = prxMKCache;
    }

    BatchCallParamPtr pParam = new MKCacheBatchCallParam<MKVBatchExRsp>(mProxyKeyItem.size());

    current->setResponse(false);
    try
    {
        map<string, MKVBatchExReq>::iterator mIt = mProxyKeyItem.begin();
        for (; mIt != mProxyKeyItem.end(); ++mIt)
        {
            string objectName = mIt->first;
            MKVBatchExReq &partReq = mIt->second;
            partReq.moduleName = req.moduleName;

            MKCachePrxCallbackPtr cb = new GetMKVBatchExCallback(current, pParam, partReq, objectName, TNOWMS, idcArea);
            mProxyCachePrx[objectName]->async_getMKVBatchEx(cb, partReq);
        }
        return ET_SUCC;
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getMKVBatchEx] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[ProxyImp::getMKVBatchEx] unkown exception" << endl);
    }
    //当发生未知（不可恢复）异常（系统错误时），提起结束本次调用并返回空数据。（读和写操作对此种错误的响应机制不一样）!!!dengyouwang
    MKCacheBatchCallParam<MKVBatchExRsp> *tmpParam = (MKCacheBatchCallParam<MKVBatchExRsp> *)(pParam.get());
    if (tmpParam->setEnd())
    {
        MKVBatchExRsp tempRsp;
        Proxy::async_response_getMKVBatchEx(current, ET_SYS_ERR, tempRsp);
    }
    return ET_SYS_ERR;
}

int ProxyImp::insertMKV(const InsertMKVReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.data.mainKey;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", mainKey = " << mainKey << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "insertMKV";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("insertMKV") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::insertMKV mainKey is empty.|moduleName=" << moduleName << "|master:" << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new InsertMKVCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_insertMKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::insertMKV] async_insertMKV exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::updateMKVBatch(const UpdateMKVBatchReq &req, MKVBatchWriteRsp &rsp, TarsCurrentPtr current)
{
    const string moduleName = req.moduleName;
    const vector<UpdateKeyValue> &vtReqData = req.data;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", recv " << vtReqData.size() << " mainKeys, from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "updateMKVBatch";
        if (_printWriteLog && _printLogModules.count(moduleName) && vtReqData.size() > 0)
        {
            FDLOG("update") << vtReqData[0].mainKey << "|" << __FUNCTION__ << "|" << vtReqData.size() << "|" << moduleName << endl;
        }
    }

    //批量操作时，对mainKey数量有限制
    size_t keyCount = vtReqData.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::updateMKVBatch] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    //根据mainKey拆分请求
    map<string, UpdateMKVBatchReq> mProxyKeyItem;
    map<string, MKWCachePrx> mProxyCachePrx;
    map<string, vector<size_t>> mProxyIndex;
    string objectName;
    MKWCachePrx prxMKWCache;
    for (size_t i = 0; i < vtReqData.size(); i++)
    {
        string key = vtReqData[i].mainKey;
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }
        int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxMKWCache);
        if (ret != ET_SUCC)
        {
            TLOGERROR("can't locate ServerInfo, invalid key: " << key << endl);
            rsp.rspData[i] = ET_KEY_INVALID;
            continue;
        }
        mProxyKeyItem[objectName].data.push_back(vtReqData[i]);
        mProxyCachePrx[objectName] = prxMKWCache;
        mProxyIndex[objectName].push_back(i);
    }

    if (mProxyKeyItem.empty())
    {
        rsp.rspData.clear();
        return ET_KEY_INVALID;
    }

    BatchCallParamPtr pParam = new MKWCacheBatchCallParam<UpdateMKVBatchReq>(req, mProxyKeyItem.size());

    if (!rsp.rspData.empty())
    {
        ((MKWCacheBatchCallParam<UpdateMKVBatchReq> *)(pParam.get()))->addResult(rsp.rspData);
    }

    current->setResponse(false);
    map<string, UpdateMKVBatchReq>::iterator mIt = mProxyKeyItem.begin();
    for (; mIt != mProxyKeyItem.end(); ++mIt)
    {
        string objectName = mIt->first;
        try
        { 
            UpdateMKVBatchReq &partReq = mIt->second;
            partReq.moduleName = moduleName;
            MKWCachePrxCallbackPtr cb = new UpdateMKVBatchCallback(current, pParam, mProxyIndex[objectName], partReq, objectName, TNOWMS);
            mProxyCachePrx[objectName]->async_updateMKVBatch(cb, partReq);
            continue;
        }
        catch (exception &ex)
        {
            TLOGERROR("[ProxyImp::updateMKVBatch] exception: " << ex.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("[ProxyImp::updateMKVBatch] unkown exception" << endl);
        }

        MKVBatchWriteRsp tempRsp;
        const vector<size_t> &vIndex = mProxyIndex[objectName];
        for (size_t i = 0; i < vIndex.size(); ++i)
        {
            tempRsp.rspData[vIndex[i]] = ET_SYS_ERR;
        }
        MKWCacheBatchCallParam<UpdateMKVBatchReq> *tmpParam = (MKWCacheBatchCallParam<UpdateMKVBatchReq> *)(pParam.get());
        tmpParam->addResult(tempRsp.rspData);

        if (pParam->_count.dec() <= 0)
        {
            Proxy::async_response_updateMKVBatch(current, ET_PARTIAL_FAIL, tmpParam->_rsp);
            break;
        }
    }

    return ET_SUCC;
}

int ProxyImp::insertMKVBatch(const InsertMKVBatchReq &req, MKVBatchWriteRsp &rsp, TarsCurrentPtr current)
{
    const string moduleName = req.moduleName;
    const vector<InsertKeyValue> &vtReqData = req.data;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", recv " << vtReqData.size() << " mainKeys, from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "insertMKVBatch";
        if (_printWriteLog && _printLogModules.count(moduleName) && vtReqData.size() > 0)
        {
            FDLOG("update") << vtReqData[0].mainKey << "|" << __FUNCTION__ << "|" << vtReqData.size() << "|" << moduleName << endl;
        }
    }

    //批量操作时，对mainKey数量有限制
    size_t keyCount = vtReqData.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::insertMKVBatch] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    map<string, InsertMKVBatchReq> mProxyKeyItem;
    map<string, MKWCachePrx> mProxyCachePrx;
    map<string, vector<size_t>> mProxyIndex;
    string objectName;
    MKWCachePrx prxMKWCache;
    for (size_t i = 0; i < vtReqData.size(); i++)
    {
        string key = vtReqData[i].mainKey;
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }
        int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxMKWCache);
        if (ret != ET_SUCC)
        {
            TLOGERROR("can't locate ServerInfo, invalid key: " << key << endl);
            rsp.rspData[i] = ET_KEY_INVALID;
            continue;
        }
        mProxyKeyItem[objectName].data.push_back(vtReqData[i]);
        mProxyCachePrx[objectName] = prxMKWCache;
        mProxyIndex[objectName].push_back(i);
    }

    if (mProxyKeyItem.empty())
    {
        rsp.rspData.clear();
        return ET_KEY_INVALID;
    }

    BatchCallParamPtr pParam = new MKWCacheBatchCallParam<InsertMKVBatchReq>(req, mProxyKeyItem.size());

    if (!rsp.rspData.empty())
    {
        ((MKWCacheBatchCallParam<InsertMKVBatchReq> *)pParam.get())->addResult(rsp.rspData);
    }

    current->setResponse(false);

    map<string, InsertMKVBatchReq>::iterator mIt = mProxyKeyItem.begin();
    for (; mIt != mProxyKeyItem.end(); mIt++)
    {
        string objectName = mIt->first;
        try
        {
           

            InsertMKVBatchReq &partReq = mIt->second;
            partReq.moduleName = moduleName;
            MKWCachePrxCallbackPtr cb = new InsertMKVBatchCallback(current, pParam, mProxyIndex[objectName], partReq, objectName, TNOWMS);
            mProxyCachePrx[objectName]->async_insertMKVBatch(cb, partReq);
            continue;
        }
        catch (exception &ex)
        {
            TLOGERROR("[ProxyImp::insertMKVBatch] exception: " << ex.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("[ProxyImp::insertMKVBatch] unkown exception" << endl);
        }

        MKVBatchWriteRsp tempRsp;
        const vector<size_t> &vIndex = mProxyIndex[objectName];
        for (size_t i = 0; i < vIndex.size(); ++i)
        {
            tempRsp.rspData[vIndex[i]] = ET_SYS_ERR;
        }
        MKWCacheBatchCallParam<InsertMKVBatchReq> *tmpParam = (MKWCacheBatchCallParam<InsertMKVBatchReq> *)(pParam.get());
        tmpParam->addResult(tempRsp.rspData);

        if (pParam->_count.dec() <= 0)
        {
            Proxy::async_response_insertMKVBatch(current, ET_PARTIAL_FAIL, tmpParam->_rsp);
            break;
        }
    }

    return ET_SUCC;
}

int ProxyImp::updateMKV(const UpdateMKVReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", mainKey = " << mainKey << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "updateMKV";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("update") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new UpdateMKVCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_updateMKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::updateMKV] async_updateMKV exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::updateMKVAtom(const UpdateMKVAtomReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", mainKey = " << mainKey << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "updateMKVAtom";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("update") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new UpdateMKVAtomCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_updateMKVAtom(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::updateMKVAtom] async_updateMKVAtom exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::eraseMKV(const MainKeyReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", mainKey = " << mainKey << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "eraseMKV";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("update") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new EraseMKVCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_eraseMKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::erase] async_erase exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::delMKVBatch(const DelMKVBatchReq &req, MKVBatchWriteRsp &rsp, TarsCurrentPtr current)
{
    const string moduleName = req.moduleName;
    const vector<DelCondition> &vtReqData = req.data;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", recv " << vtReqData.size() << " mainKeys, from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "delMKVBatch";
        if (_printWriteLog && _printLogModules.count(moduleName) && vtReqData.size() > 0)
        {
            FDLOG("update") << vtReqData[0].mainKey << "|" << __FUNCTION__ << "|" << vtReqData.size() << "|" << moduleName << endl;
        }
    }

    //批量操作时，对mainKey数量有限制
    size_t keyCount = vtReqData.size();
    logBatchCount(moduleName, context[CONTEXT_CALLER], keyCount);
    if (checkKeyCount(keyCount))
    {
        TLOGERROR("[ProxyImp::delMKVBatch] keyCount for batch  out of limit, moduleName:" << moduleName << " keyCount:" << keyCount << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    map<string, DelMKVBatchReq> mProxyKeyItem;
    map<string, MKWCachePrx> mProxyCachePrx;
    map<string, vector<size_t>> mProxyIndex;
    string objectName;
    MKWCachePrx prxMKWCache;
    for (size_t i = 0; i < vtReqData.size(); i++)
    {
        string key = vtReqData[i].mainKey;
        if (key.empty())
        {
            TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
            return ET_INPUT_PARAM_ERROR;
        }
        int ret = _cacheProxyFactory->getWCacheProxy(moduleName, key, objectName, prxMKWCache);
        if (ret != ET_SUCC)
        {
            TLOGERROR("can't locate ServerInfo, invalid key: " << key << endl);
            rsp.rspData[i] = ET_KEY_INVALID;
            continue;
        }
        mProxyKeyItem[objectName].data.push_back(vtReqData[i]);
        mProxyCachePrx[objectName] = prxMKWCache;
        mProxyIndex[objectName].push_back(i);
    }

    if (mProxyKeyItem.empty())
    {
        rsp.rspData.clear();
        return ET_KEY_INVALID;
    }

    BatchCallParamPtr pParam = new MKWCacheBatchCallParam<DelMKVBatchReq>(req, mProxyKeyItem.size());

    if (!rsp.rspData.empty())
    {
        ((MKWCacheBatchCallParam<DelMKVBatchReq> *)(pParam.get()))->addResult(rsp.rspData);
    }

    current->setResponse(false);
    map<string, DelMKVBatchReq>::iterator mIt = mProxyKeyItem.begin();
    for (; mIt != mProxyKeyItem.end(); ++mIt)
    {
        string objectName = mIt->first;
        try
        {
            DelMKVBatchReq &partReq = mIt->second;
            partReq.moduleName = moduleName;
            MKWCachePrxCallbackPtr cb = new DelMKVBatchCallback(current, pParam, mProxyIndex[objectName], partReq, objectName, TNOWMS);
            mProxyCachePrx[objectName]->async_delMKVBatch(cb, partReq);
            continue;
        }
        catch (exception &e)
        {
            TLOGERROR("[ProxyImp::delMKVBatch] exception:" << e.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("[ProxyImp::delMKVBatch] UnkownException" << endl);
        }

        MKVBatchWriteRsp tempRsp;
        const vector<size_t> &vIndex = mProxyIndex[objectName];
        for (size_t i = 0; i < vIndex.size(); i++)
        {
            tempRsp.rspData[vIndex[i]] = ET_SYS_ERR;
        }
        ((MKWCacheBatchCallParam<DelMKVBatchReq> *)(pParam.get()))->addResult(tempRsp.rspData);

        if (pParam->_count.dec() <= 0)
        {
            Proxy::async_response_delMKVBatch(current, ET_PARTIAL_FAIL, ((MKWCacheBatchCallParam<DelMKVBatchReq> *)(pParam.get()))->_rsp);
            break;
        }
    }

    return ET_SUCC;
}

int ProxyImp::delMKV(const DelMKVReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", mainKey = " << mainKey << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "delMKV";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("delMKV: mainKey = ") << mainKey << ", " << __FUNCTION__ << " |1| " << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty! moduleName = " << moduleName << ", CALLER = " << current->getContext()[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new DelMKVCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_delMKV(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::delMKV] async_del exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getMainKeyCount(const MainKeyReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", mainKey = " << mainKey << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getMainKeyCount";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getMainKeyCount") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::getMainKeyCount mainKey is empty, module:" << moduleName << "|master:" << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    MKCachePrx prxMKCache;
    string objectName;
    int ret = _cacheProxyFactory->getCacheProxy(moduleName, mainKey, objectName, prxMKCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKCachePrxCallbackPtr cb = new GetMainKeyCountCallback(current, req, objectName, TNOWMS, idcArea);
        prxMKCache->async_getMainKeyCount(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getMainKeyCount] async_getMainKeyCount exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getAllKeys(const GetAllKeysReq &req, GetAllKeysRsp &rsp, TarsCurrentPtr current)
{
    const string moduleName = req.moduleName;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", index = " << req.index << ", count = " << req.count << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getAllKeys";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getAllKeys") << "|" << __FUNCTION__ << "|" << moduleName << endl;
        }
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    RouterTableInfo *pRouterTableInfo = g_app._routerTableInfoFactory->getRouterTableInfo(moduleName);
    if (pRouterTableInfo == NULL)
    {
        TLOGDEBUG("[ProxyImp::getAllKeys] do not support moduleName: " << moduleName << endl);
        return ET_MODULE_NAME_INVALID;
    }
    RouterTable &routerTable = pRouterTableInfo->getRouterTable();

    vector<ServerInfo> servers;
    if (routerTable.getAllIdcServer(idcArea, servers) < 0)
    {
        TLOGERROR("[ProxyImp::getAllKeys] get idc server error!" << endl);
        return ET_SYS_ERR;
    }

    BatchCallParamPtr pParam = new CacheBatchCallParam<GetAllKeysRsp>(servers.size());

    current->setResponse(false);
    try
    {
        for (size_t i = 0; i < servers.size(); i++)
        {
            string objectName = servers[i].CacheServant;
            CachePrxCallbackPtr cb = new GetAllKeysCallback(current, pParam, req, objectName, TNOWMS, idcArea);
            CachePrx prxCache = _cacheProxyFactory->getProxy<CachePrx>(objectName);
            prxCache->async_getAllKeys(cb, req);
        }
        return ET_SUCC;
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getAllKeys] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[ProxyImp::getAllKeys] UnkownException" << endl);
    }
    //当发生未知（不可恢复）异常（系统错误时），提起结束本次调用并返回空数据。（读和写操作对此种错误的响应机制不一样）!!!dengyouwang
    CacheBatchCallParam<GetAllKeysRsp> *tmpParam = (CacheBatchCallParam<GetAllKeysRsp> *)(pParam.get());
    if (tmpParam->setEnd())
    {
        GetAllKeysRsp tempRsp;
        Proxy::async_response_getAllKeys(current, ET_SYS_ERR, tempRsp);
    }
    return ET_SYS_ERR;
}

int ProxyImp::getAllMainKey(const GetAllKeysReq &req, GetAllKeysRsp &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;

    TLOGDEBUG("ProxyImp::" << __FUNCTION__ << ", moduleName = " << moduleName << ", index = " << req.index << ", count = " << req.count << ", from ip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getAllMainKey";
    }

    //读操作时需要这个
    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    RouterTableInfo *pRouterTableInfo = g_app._routerTableInfoFactory->getRouterTableInfo(moduleName);
    if (pRouterTableInfo == NULL)
    {
        TLOGDEBUG("[ProxyImp::getMKAllMainKey]do not support moduleName: " << moduleName << endl);
        return ET_MODULE_NAME_INVALID;
    }
    RouterTable &routerTable = pRouterTableInfo->getRouterTable();

    vector<ServerInfo> servers;
    if (routerTable.getAllIdcServer(idcArea, servers) < 0)
    {
        TLOGERROR("[ProxyImp::getAllMainKey get idc server error!" << endl);
        return ET_SYS_ERR;
    }

    BatchCallParamPtr pParam = new MKCacheBatchCallParam<GetAllKeysRsp>(servers.size());

    current->setResponse(false);
    try
    {
        for (size_t i = 0; i < servers.size(); i++)
        {
            string objectName = servers[i].CacheServant;
            MKCachePrxCallbackPtr cb = new GetAllMainKeyCallback(current, pParam, req, objectName, TNOWMS, idcArea);
            MKCachePrx prxMKCache = _cacheProxyFactory->getProxy<MKCachePrx>(objectName);
            prxMKCache->async_getAllMainKey(cb, req);
        }
        return ET_SUCC;
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getAllMainKey] exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("[ProxyImp::getAllMainKey] unkown exception" << endl);
    }
    //当发生未知（不可恢复）异常（系统错误时），提起结束本次调用并返回空数据。（读和写操作对此种错误的响应机制不一样）!!!dengyouwang
    MKCacheBatchCallParam<GetAllKeysRsp> *tmpParam = (MKCacheBatchCallParam<GetAllKeysRsp> *)(pParam.get());
    if (tmpParam->setEnd())
    {
        GetAllKeysRsp tempRsp;
        Proxy::async_response_getAllMainKey(current, ET_SYS_ERR, tempRsp);
    }
    return ET_SYS_ERR;
}

int ProxyImp::getRangeList(const GetRangeListReq &req, BatchEntry &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("getRangeList: moduleName = " << moduleName << ", | mainKey = " << mainKey << ", masterip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getRangeList";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getRangeList") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::getRangeList key is empty module:" << req.moduleName << "|master:" << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    MKCachePrx prxMKCache;
    string objectName;
    int ret = _cacheProxyFactory->getCacheProxy(moduleName, mainKey, objectName, prxMKCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKCachePrxCallbackPtr cb = new GetRangeListCallback(current, req, objectName, TNOWMS, idcArea);
        prxMKCache->async_getRangeList(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getRangeList] async_getRangeList exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getList(const GetListReq &req, GetListRsp &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("getList: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getList";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getList") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::getList key is empty module:" << req.moduleName << "|master:" << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    MKCachePrx prxMKCache;
    string objectName;
    int ret = _cacheProxyFactory->getCacheProxy(moduleName, mainKey, objectName, prxMKCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKCachePrxCallbackPtr cb = new GetListCallback(current, req, objectName, TNOWMS, idcArea);
        prxMKCache->async_getList(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getList] async_getRangeList exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getSet(const GetSetReq &req, BatchEntry &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("getSet: moduleName = " << moduleName << ", | mainKey = " << mainKey << ", masterip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getSet";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getSet") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::getSet key is empty, module = " << moduleName << ", | master: = " << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    MKCachePrx prxMKCache;
    string objectName;
    int ret = _cacheProxyFactory->getCacheProxy(moduleName, mainKey, objectName, prxMKCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKCachePrxCallbackPtr cb = new GetSetCallback(current, req, objectName, TNOWMS, idcArea);
        prxMKCache->async_getSet(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getSet] async_getSet exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getZSetScore(const GetZsetScoreReq &req, double &score, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("getZSetScore: moduleName = " << moduleName << ", | mainKey = " << mainKey << ", masterip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getZSetScore";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getZSetScore") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::getZSetScore key is empty, module = " << moduleName << ", | master: = " << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    MKCachePrx prxMKCache;
    string objectName;
    int ret = _cacheProxyFactory->getCacheProxy(moduleName, mainKey, objectName, prxMKCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKCachePrxCallbackPtr cb = new GetZSetScoreCallback(current, req, objectName, TNOWMS, idcArea);
        prxMKCache->async_getZSetScore(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getScoreZSet] async_getZSetScore exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getZSetPos(const GetZsetPosReq &req, long &pos, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("getZSetPos: moduleName = " << moduleName << ", | mainKey = " << mainKey << ", masterip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getZSetPos";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getZSetPos") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::getZSetPos key is empty, module = " << req.moduleName << ", | master: = " << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    MKCachePrx prxMKCache;
    string objectName;
    int ret = _cacheProxyFactory->getCacheProxy(moduleName, mainKey, objectName, prxMKCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKCachePrxCallbackPtr cb = new GetZSetPosCallback(current, req, objectName, TNOWMS, idcArea);
        prxMKCache->async_getZSetPos(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getZSetPos] async_getZSetPos exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getZSetByPos(const GetZsetByPosReq &req, BatchEntry &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("getZSetByPos: moduleName = " << moduleName << ", | mainKey = " << mainKey << ", masterip = " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getZSetByPos";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getZSetByPos") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::getZSetByPos key is empty, module = " << moduleName << ", | master: = " << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    MKCachePrx prxMKCache;
    string objectName;
    int ret = _cacheProxyFactory->getCacheProxy(moduleName, mainKey, objectName, prxMKCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKCachePrxCallbackPtr cb = new GetZSetByPosCallback(current, req, objectName, TNOWMS, idcArea);
        prxMKCache->async_getZSetByPos(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getZSetByPos] async_getZSetByPos exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::getZSetByScore(const GetZsetByScoreReq &req, BatchEntry &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("getZSetByScore: moduleName = " << moduleName << ", | mainKey = " << mainKey << ", masterip =" << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "getZSetByScore";
        if (_printReadLog && _printLogModules.count(moduleName))
        {
            FDLOG("getZSetByScore") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("ProxyImp::getZSetByScore key is empty, module = " << moduleName << ", | master: = " << current->getIp() << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    string idcArea = _idcArea;
    if (!req.idcSpecified.empty())
    {
        idcArea = req.idcSpecified;
    }

    MKCachePrx prxMKCache;
    string objectName;
    int ret = _cacheProxyFactory->getCacheProxy(moduleName, mainKey, objectName, prxMKCache, idcArea);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKCachePrxCallbackPtr cb = new GetZSetByScoreCallback(current, req, objectName, TNOWMS, idcArea);
        prxMKCache->async_getZSetByScore(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::getZSetByScore] async_getZSetByScore exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::pushList(const PushListReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;

    TLOGDEBUG("ProxyImp::pushList: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "pushList";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("pushList") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new PushListCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_pushList(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::pushList] async_update exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::popList(const PopListReq &req, PopListRsp &rsp, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("ProxyImp::popList: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "popList";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("popList") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new PopListCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_popList(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::popList] async_update exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::replaceList(const ReplaceListReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("ProxyImp::replaceList: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "replaceList";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("replaceList") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new ReplaceListCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_replaceList(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::replaceList] async_update exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::trimList(const TrimListReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("ProxyImp::trimList: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "trimList";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("trimList") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new TrimListCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_trimList(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::trimList] async_update exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::remList(const RemListReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("ProxyImp::remList: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "remList";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("remList") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new RemListCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_remList(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::remList] async_update exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::addSet(const AddSetReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.value.mainKey;
    TLOGDEBUG("ProxyImp::addSet: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "addSet";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("addSet") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new AddSetCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_addSet(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::addSet] async_update exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::delSet(const DelSetReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "delSet";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("delSet") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new DelSetCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_delSet(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::addSet] async_update exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::addZSet(const AddZSetReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.value.mainKey;
    TLOGDEBUG("ProxyImp::addZSet: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "addZSet";
        if (_printWriteLog && _printLogModules.count(req.moduleName))
        {
            FDLOG("addZSet") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new AddZSetCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_addZSet(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::addZSet] async_update exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::incScoreZSet(const IncZSetScoreReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.value.mainKey;
    TLOGDEBUG("ProxyImp::incScoreZSet: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "incScoreZSet";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("incScoreZSet") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new IncScoreZSetCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_incScoreZSet(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::incScoreZSet] async_incScoreZSet exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::delZSet(const DelZSetReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("ProxyImp::delZSet: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("delZSet") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new DelZSetCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_delZSet(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::delZSet] async_delZSet exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::delZSetByScore(const DelZSetByScoreReq &req, TarsCurrentPtr current)
{
    const string &moduleName = req.moduleName;
    const string &mainKey = req.mainKey;
    TLOGDEBUG("ProxyImp::delZSetByScore: " << moduleName << "|" << mainKey << " masterip: " << current->getIp() << endl);

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "delZSetByScore";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("delZSetByScore") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new DelZSetByScoreCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_delZSetByScore(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::delZSetByScore] async_delZSetByScore exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }
    return ET_SUCC;
}

int ProxyImp::updateZSet(const UpdateZSetReq &req, TarsCurrentPtr current)
{

    const string &moduleName = req.moduleName;
    const string &mainKey = req.value.mainKey;

    map<string, string> &context = current->getContext();
    if (!context.count(CONTEXT_CALLER))
    {
        context[CONTEXT_CALLER] = "updateZSet";
        if (_printWriteLog && _printLogModules.count(moduleName))
        {
            FDLOG("updateZSet") << mainKey << "|" << __FUNCTION__ << "|1|" << moduleName << endl;
        }
    }

    if (mainKey.empty())
    {
        TLOGERROR("The Key can not be empty.|moduleName=" << moduleName << "|CALLER=" << context[CONTEXT_CALLER] << endl);
        return ET_INPUT_PARAM_ERROR;
    }

    MKWCachePrx prxMKWCache;
    string objectName;
    int ret = _cacheProxyFactory->getWCacheProxy(moduleName, mainKey, objectName, prxMKWCache, false);
    if (ret != ET_SUCC)
    {
        return ret;
    }

    current->setResponse(false);
    try
    {
        MKWCachePrxCallbackPtr cb = new UpdateZSetCallback(current, req, objectName, TNOWMS);
        prxMKWCache->async_updateZSet(cb, req);
    }
    catch (exception &ex)
    {
        TLOGERROR("[ProxyImp::updateZSet] async_updateZSet exception: " << ex.what() << endl);
        current->setResponse(true);
        return ET_SYS_ERR;
    }

    return ET_SUCC;
}

