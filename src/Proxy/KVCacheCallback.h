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
#ifndef __KVCACHE_CALLBACK_H_
#define __KVCACHE_CALLBACK_H_

#include <functional>


#include "Cache.h"
#include "WCache.h"
#include "CacheCallbackComm.h"
#include "Global.h"

using namespace std;
using namespace tars;
using namespace DCache;

#define CacheCallbackLog(class_name, module_name, key_item, object_name, ret) TLOGDEBUG(LOG_MEM_FUN(class_name) \
                                                                                        << " key:" << key_item << "|module:" << module_name << "|object:" << object_name << "|ret:" << ret << endl)

#define CacheCallbackExcLog(class_name, module_name, key_item, object_name, ret) TLOGDEBUG(LOG_MEM_FUN(class_name) \
                                                                                           << " key:" << key_item << "|module:" << module_name << "|object:" << object_name << "|errno:" << ret << endl)

#define CacheCallbackDo(module_name, key_item, object_name, ret, rsp, recall_fun, rsp_fun) \
    CacheCallbackLog(CacheCallback, module_name, key_item, object_name, ret);              \
    ResponserPtr responser = make_responser(rsp_fun, rsp);                                 \
    procCallback(ret, recall_fun, responser)

#define CacheCallbackExcDo(module_name, key_item, object_name, ret, rsp_para, rsp_fun) \
    CacheCallbackExcLog(CacheCallback, module_name, key_item, object_name, ret);       \
    rsp_para rsp;                                                                      \
    ResponserPtr responser = make_responser(rsp_fun, rsp);                             \
    procExceptionCall(ret, responser)

#define WCacheCallbackDo(module_name, key_item, object_name, ret, recall_fun, rsp_fun) \
    CacheCallbackLog(WCacheCallback, module_name, key_item, object_name, ret);         \
    ResponserPtr responser = make_responser(rsp_fun);                                  \
    procCallback(ret, recall_fun, responser)

#define WCacheCallbackExcDo(module_name, key_item, object_name, ret, rsp_fun)     \
    CacheCallbackExcLog(WCacheCallback, module_name, key_item, object_name, ret); \
    ResponserPtr responser = make_responser(rsp_fun);                             \
    procExceptionCall(ret, responser)

#define CacheBatchCallbackLog(class_name, module_name, object_name, ret) TLOGDEBUG(LOG_MEM_FUN(class_name) \
                                                                                   << " module:" << module_name << "|object:" << object_name << "|ret:" << ret << endl)

#define CacheBatchCallbackExcLog(class_name, module_name, object_name, ret) TLOGDEBUG(LOG_MEM_FUN(class_name) \
                                                                                      << " module:" << module_name << "|object:" << object_name << "|errno:" << ret << endl)

#define CacheBatchCallbackExcDo(module_name, object_name, ret, rsp_fun)          \
    CacheBatchCallbackExcLog(CacheBatchCallback, module_name, object_name, ret); \
    procExceptionCall(ret, rsp_fun)

#define WCacheBatchCallbackDo(ret, rsp, recall_fun, rsp_fun) \
    procCallback(ret, rsp, recall_fun, rsp_fun)

#define WCacheBatchCallbackExcDo(module_name, object_name, ret, rsp_fun)          \
    CacheBatchCallbackExcLog(WCacheBatchCallback, module_name, object_name, ret); \
    procExceptionCall(ret, rsp_fun)

struct GetKVCallback : public CacheCallbackComm, public CachePrxCallback
{
    GetKVCallback(TarsCurrentPtr &current,
                  const GetKVReq &req,
                  const string &objectName,
                  const int64_t beginTime,
                  function<void(TarsCurrentPtr,Int32, const GetKVRsp &)> &dealWithRsp,
                  const string &idcArea = "",
                  const bool repeatFlag = false)
        : CacheCallbackComm(current, req.moduleName, objectName, beginTime, repeatFlag), _dealWithRsp(dealWithRsp), _req(req), _idcArea(idcArea) {}

    virtual ~GetKVCallback() {}

    virtual void callback_getKV(int ret, const GetKVRsp &rsp)
    {
        CacheCallbackDo(_req.moduleName, _req.keyItem, _objectName, ret, rsp, &CacheProxy::async_getKV, _dealWithRsp);
    }

    virtual void callback_getKV_exception(int ret)
    {
        CacheCallbackExcDo(_req.moduleName, _req.keyItem, _objectName, ret, GetKVRsp, &Proxy::async_response_getKV);
    }

  protected:
    /**
   * 对回调做统一处理
   * @param ret，接口返回值
   * @param recallmf，CacheServer接口函数指针
   * @param responser，负责应答的对象
   * @return void
   */
    void procCallback(const int ret, void (CacheProxy::*recallmf)(CachePrxCallbackPtr, const GetKVReq &, const map<string, string> &), const ResponserPtr responser)
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];
        const string &keyItem = _req.keyItem;
        const string &moduleName = _req.moduleName;

        if (ret != ET_SUCC)
        {
            FDLOG("CBError") << caller << "|key:" << keyItem << "|module:" << moduleName << "|object:" << _objectName << "|ret:" << ret << endl;

            if ((ret == ET_KEY_AREA_ERR || ret == ET_SERVER_TYPE_ERR) && (!_repeatFlag))
            {
                reportException("CBError");

                string errMsg;
                try
                {
                    string objectName;
                    CachePrx prxCache;

                    // 根据模块名和key重新获取要访问的cache服务的objectname以及代理，期间会强制更新路由
                    int ret = getCacheProxy(moduleName, keyItem, objectName, prxCache, _idcArea); // 验证更新了路由表

                    if (ret != ET_SUCC)
                    {
                        doResponse(ret, caller, EXCE, responser);
                        return;
                    }

                    CachePrxCallbackPtr cb = new GetKVCallback(_current, _req, objectName, _beginTime, _dealWithRsp, _idcArea, true);
                    //重新访问CacheServer
                    (((CacheProxy *)prxCache.get())->*recallmf)(cb, _req, map<string, string>()); // 验证此函数被调用仅一次

                    return;
                }
                catch (exception &ex)
                {
                    errMsg = string("exception:") + ex.what();
                }
                catch (...)
                {
                    errMsg = "catch unkown exception";
                }

                TLOGERROR("CacheCallback::procCallback " << errMsg << endl);

                FDLOG("CBError") << caller << "|key:" << keyItem << "|module:" << moduleName << "|recall error:" << errMsg << endl;

                reportException("CBError");

                doResponse(ET_SYS_ERR, caller, EXCE, responser);

                return;
            }
        }
        doResponse(ret, caller, SUCC, responser);
    }

    /**
   * 对异常回调做统一处理
   * @param ret，接口返回值
   * @param responser，负责应答的对象
   * @return void
   */
    void procExceptionCall(const int ret, const ResponserPtr responser)
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        FDLOG("CBError") << caller << "|key:" << _req.keyItem << "|module:" << _req.moduleName << "|object:" << _objectName << "|errno:" << ret << endl;

        reportException("CBError");

        if (ret == CALLTIMEOUT)
        {
            doResponse(ET_CACHE_ERR, caller, TIME_OUT, responser);
        }
        else
        {
            doResponse(ET_CACHE_ERR, caller, EXCE, responser);
        }
    }

  private:
    function<void(TarsCurrentPtr,Int32, const GetKVRsp &)> _dealWithRsp;
    GetKVReq _req;
    string _idcArea;
};

//////////////////////////////////////////////////////////////////////////

template <typename Request, typename ClassName>
struct ProcWCacheCallback : public CacheCallbackComm
{
  public:
    ProcWCacheCallback(TarsCurrentPtr &current,
                       const Request &req,
                       const string &key,
                       const string &objectName,
                       const int64_t beginTime,
                       const bool repeatFlag)
        : CacheCallbackComm(current, req.moduleName, objectName, beginTime, repeatFlag), _req(req), _key(key)
    {
    }

    virtual ~ProcWCacheCallback() {}

  protected:
    /**
     * 对回调做统一处理
     * @param ret，接口返回值
     * @param recallmf，CacheServer接口函数指针
     * @param responser，负责应答的对象
     * @return void
     */

    void procCallback(const int ret, void (WCacheProxy::*recallmf)(WCachePrxCallbackPtr, const Request &, const map<string, string> &), const ResponserPtr responser)
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];
        const string &keyItem = _key;
        const string &moduleName = _req.moduleName;

        if (ret != ET_SUCC)
        {
            FDLOG("CBError") << caller << "|key:" << keyItem << "|module:" << moduleName << "|object:" << _objectName << "|ret:" << ret << endl;

            if ((ret == ET_KEY_AREA_ERR || ret == ET_SERVER_TYPE_ERR) && (!_repeatFlag))
            {
                reportException("CBError");

                string errMsg;
                try
                {
                    string objectName;
                    WCachePrx prxCache;

                    // 根据模块名和key重新获取要访问的cache服务的objectname以及代理，期间会强制更新路由
                    int ret = getWCacheProxy(moduleName, keyItem, objectName, prxCache);

                    if (ret != ET_SUCC)
                    {
                        doResponse(ret, caller, EXCE, responser);

                        return;
                    }

                    WCachePrxCallbackPtr cb = new ClassName(_current, _req, objectName, _beginTime, true);
                    //重新访问CacheServer
                    (((WCacheProxy *)prxCache.get())->*recallmf)(cb, _req, map<string, string>());

                    return;
                }
                catch (exception &ex)
                {
                    errMsg = string("exception:") + ex.what();
                }
                catch (...)
                {
                    errMsg = "catch unkown exception";
                }

                TLOGERROR("WCacheCallback::procCallback " << errMsg << endl);

                FDLOG("CBError") << caller << "|key:" << keyItem << "|module:" << moduleName << "|recall error:" << errMsg << endl;

                reportException("CBError");

                doResponse(ET_CACHE_ERR, caller, EXCE, responser);

                return;
            }
        }

        doResponse(ret, caller, SUCC, responser);
    }

    /**
     * 对异常回调做统一处理
     * @param ret，接口返回值
     * @param responser，负责应答的对象
     * @return void
     */
    void procExceptionCall(const int ret, const ResponserPtr responser)
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        FDLOG("CBError") << caller << "|key:" << _key << "|module:" << _req.moduleName << "|object:" << _objectName << "|errno:" << ret << endl;

        reportException("CBError");

        if (ret == CALLTIMEOUT)
        {
            doResponse(ET_CACHE_ERR, caller, TIME_OUT, responser);
        }
        else
        {
            doResponse(ET_CACHE_ERR, caller, EXCE, responser);
        }
    }

  protected:
    Request _req;
    string _key;
};

struct SetKVCallback : public ProcWCacheCallback<SetKVReq, SetKVCallback>, public WCachePrxCallback
{

    SetKVCallback(TarsCurrentPtr &current,
                  const SetKVReq &req,
                  const string &objectName,
                  const int64_t beginTime,
                  const bool repeatFlag = false)
        : ProcWCacheCallback<SetKVReq, SetKVCallback>(current, req, req.data.keyItem, objectName, beginTime, repeatFlag)
    {
    }

    virtual ~SetKVCallback() {}
    virtual void callback_setKV(int ret)
    {
        WCacheCallbackDo(_req.moduleName, _req.data.keyItem, _objectName, ret, &WCacheProxy::async_setKV, &Proxy::async_response_setKV);
    }
    virtual void callback_setKV_exception(int ret)
    {
        WCacheCallbackExcDo(_req.moduleName, _req.data.keyItem, _objectName, ret, &Proxy::async_response_setKV);
    }
};

struct InsertKVCallback : public ProcWCacheCallback<SetKVReq, InsertKVCallback>, public WCachePrxCallback
{

    InsertKVCallback(TarsCurrentPtr &current,
                     const SetKVReq &req,
                     const string &objectName,
                     const int64_t beginTime,
                     const bool repeatFlag = false)
        : ProcWCacheCallback<SetKVReq, InsertKVCallback>(current, req, req.data.keyItem, objectName, beginTime, repeatFlag)
    {
    }

    virtual ~InsertKVCallback() {}
    virtual void callback_insertKV(int ret)
    {
        WCacheCallbackDo(_req.moduleName, _req.data.keyItem, _objectName, ret, &WCacheProxy::async_insertKV, &Proxy::async_response_insertKV);
    }
    virtual void callback_insertKV_exception(int ret)
    {
        WCacheCallbackExcDo(_req.moduleName, _req.data.keyItem, _objectName, ret, &Proxy::async_response_setKV);
    }
};

struct UpdateKVCallback : public ProcWCacheCallback<UpdateKVReq, UpdateKVCallback>, public WCachePrxCallback
{
    UpdateKVCallback(TarsCurrentPtr &current,
                     const UpdateKVReq &req,
                     const string &objectName,
                     const int64_t beginTime,
                     const bool repeatFlag = false)
        : ProcWCacheCallback<UpdateKVReq, UpdateKVCallback>(current, req, req.data.keyItem, objectName, beginTime, repeatFlag)
    {
    }

    virtual ~UpdateKVCallback() {}

    virtual void callback_updateKV(int ret, const UpdateKVRsp &rsp)
    {
        TLOGDEBUG("WCacheCallback::callback_updateKV key:" << _key << "|module:" << _req.moduleName << "|object:" << _objectName << "|ret:" << ret << endl);

        ResponserPtr responser = make_responser(&Proxy::async_response_updateKV, rsp);

        procCallback(ret, &WCacheProxy::async_updateKV, responser);
    }

    virtual void callback_updateKV_exception(int ret)
    {
        TLOGERROR("WCacheCallback::callback_updateKV_exception key:" << _key << "|module:" << _req.moduleName << "|object:" << _objectName << "|errno:" << ret << endl);

        UpdateKVRsp rsp;
        ResponserPtr responser = make_responser(&Proxy::async_response_updateKV, rsp);

        procExceptionCall(ret, responser);
    }
};

struct EraseKVCallback : public ProcWCacheCallback<RemoveKVReq, EraseKVCallback>, public WCachePrxCallback
{
    EraseKVCallback(TarsCurrentPtr &current,
                    const RemoveKVReq &req,
                    const string &objectName,
                    const int64_t beginTime,
                    const bool repeatFlag = false)
        : ProcWCacheCallback<RemoveKVReq, EraseKVCallback>(current, req, req.keyInfo.keyItem, objectName, beginTime, repeatFlag)
    {
    }

    virtual ~EraseKVCallback() {}

    virtual void callback_eraseKV(int ret)
    {
        WCacheCallbackDo(_req.moduleName, _key, _objectName, ret, &WCacheProxy::async_eraseKV, &Proxy::async_response_eraseKV);
    }

    virtual void callback_eraseKV_exception(int ret)
    {
        WCacheCallbackExcDo(_req.moduleName, _key, _objectName, ret, &Proxy::async_response_eraseKV);
    }
};

struct DelKVCallback : public ProcWCacheCallback<RemoveKVReq, DelKVCallback>, public WCachePrxCallback
{
    DelKVCallback(TarsCurrentPtr &current,
                  const RemoveKVReq &req,
                  const string &objectName,
                  const int64_t beginTime,
                  const bool repeatFlag = false)
        : ProcWCacheCallback<RemoveKVReq, DelKVCallback>(current, req, req.keyInfo.keyItem, objectName, beginTime, repeatFlag)
    {
    }

    virtual ~DelKVCallback() {}

    virtual void callback_delKV(int ret)
    {
        WCacheCallbackDo(_req.moduleName, _key, _objectName, ret, &WCacheProxy::async_delKV, &Proxy::async_response_delKV);
    }

    virtual void callback_delKV_exception(int ret)
    {
        WCacheCallbackExcDo(_req.moduleName, _key, _objectName, ret, &Proxy::async_response_delKV);
    }
};

//////////////////////////////////////////////////////////////////////////

template <typename Response>
struct CacheBatchCallParam : public BatchCallParamComm
{
    CacheBatchCallParam(const size_t count) : BatchCallParamComm(count), _end(false) {}

    bool setEnd()
    {
        TC_ThreadLock::Lock lock(_lock);
        if (_end)
        {
            return false;
        }
        else
        {
            _end = true;
        }

        return true;
    }

    bool _end;

    Response _rsp;
};

template <typename Request, typename Response, typename ClassName>
struct ProcCacheBatchCallback : public CacheCallbackComm
{
  public:
    ProcCacheBatchCallback(TarsCurrentPtr &current,
                           BatchCallParamPtr &param,
                           const Request &req,
                           const string &objectName,
                           const int64_t beginTime,
                           const string &idcArea = "",
                           const bool repeatFlag = false)
        : CacheCallbackComm(current, req.moduleName, objectName, beginTime, repeatFlag), _req(req), _idcArea(idcArea), _param(param) {}

    virtual ~ProcCacheBatchCallback() {}

  protected:
    void procFailCall(int ret, void (CacheProxy::*recallmf)(CachePrxCallbackPtr, const Request &, const map<string, string> &), void (*resmf)(TarsCurrentPtr, int, const Response &), const bool needRetry = true)
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        if (ret != ET_SUCC)
        {
            const string &moduleName = _req.moduleName;

            FDLOG("CBError") << caller << "|module:" << moduleName << "|object:" << _objectName << "|ret:" << ret << endl;

            reportException("CBError");

            if (ret == ET_KEY_AREA_ERR || ret == ET_SERVER_TYPE_ERR)
            {
                if (!_repeatFlag && needRetry)
                {
                    ret = doRecall(recallmf);

                    if (ret == ET_SUCC)
                    {
                        return;
                    }
                }
                else
                {
                    ret = ET_SYS_ERR;
                }
            }
        }

        CacheBatchCallParam<Response> *param = (CacheBatchCallParam<Response> *)_param.get();
        if (param->setEnd())
        {
            Response rsp;
            ResponserPtr responser = make_responser(resmf, rsp);

            doResponse(ret, caller, SUCC, responser);
        }
    }

    int doRecall(void (CacheProxy::*recallmf)(CachePrxCallbackPtr, const Request &, const map<string, string> &))
    {
        const string &moduleName = _req.moduleName;

        RouterTableInfo *routeTableInfo = g_app._routerTableInfoFactory->getRouterTableInfo(moduleName);
        if (!routeTableInfo)
        {
            TLOGERROR("CacheBatchCallback::doRecall do not support module:" << moduleName << endl);
            return ET_MODULE_NAME_INVALID;
        }

        int ret = routeTableInfo->update(); // 更新路由表
        if (ret != ET_SUCC)
        {
            TLOGERROR("CacheBatchCallback::doRecall update route table failed, module:" << moduleName << "|ret:" << ret << endl);
        }
        else
        {
            RouterTable &routeTable = routeTableInfo->getRouterTable();

            map<string, Request> objectReq;
            for (size_t i = 0; i < _req.keys.size(); ++i)
            {
                // 从路由表中获得要访问的CacheServer的节点信息
                ServerInfo serverInfo;
                if (_idcArea.empty())
                {
                    ret = routeTable.getMaster(_req.keys[i], serverInfo);
                }
                else
                {
                    ret = routeTable.getIdcServer(_req.keys[i], _idcArea, false, serverInfo);
                }
                if (ret != RouterTable::RET_SUCC)
                {
                    // 如果通过key无法获取CacheServer的节点信息
                    TLOGERROR("CacheBatchCallback::doRecall can not locate key:" << _req.keys[i] << "|ret:" << ret << endl);
                    break;
                }

                const string &objectName = serverInfo.CacheServant;
                objectReq[objectName].keys.push_back(_req.keys[i]);
            }

            if (ret == ET_SUCC)
            {
                _param->_count.add(objectReq.size() - 1);

                typename map<string, Request>::iterator it = objectReq.begin();
                typename map<string, Request>::iterator itEnd = objectReq.end();
                for (; it != itEnd; ++it)
                {
                    try
                    {
                        it->second.moduleName = moduleName;
                        CachePrxCallbackPtr cb = new ClassName(_current, _param, it->second, it->first, _beginTime, _idcArea, true);

                        CachePrx prxCache;
                        ret = getProxy(it->first, prxCache);
                        if (ret == ET_SUCC)
                        {
                            //重新访问CacheServer
                            (((CacheProxy *)prxCache.get())->*recallmf)(cb, it->second, map<string, string>());
                            continue;
                        }
                        else
                        {
                            return ret;
                        }
                    }
                    catch (exception &ex)
                    {
                        TLOGERROR("CacheBatchCallback::doRecall exception:" << ex.what() << endl);
                    }
                    catch (...)
                    {
                        TLOGERROR("CacheBatchCallback::doRecall catch unkown exception" << endl);
                    }

                    return ET_SYS_ERR;
                }
            }
        }

        return ret;
    }

    void procExceptionCall(const int ret, void (*resmf)(TarsCurrentPtr, int, const Response &))
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        FDLOG("CBError") << caller << "|module:" << _req.moduleName << "|object:" << _objectName << "|errno:" << ret << endl;

        reportException("CBError");

        CacheBatchCallParam<Response> *param = (CacheBatchCallParam<Response> *)_param.get();
        if (param->setEnd())
        {
            Response rsp;
            ResponserPtr responser = make_responser(resmf, rsp);

            if (ret == CALLTIMEOUT)
            {
                doResponse(ET_CACHE_ERR, caller, TIME_OUT, responser);
            }
            else
            {
                doResponse(ET_CACHE_ERR, caller, EXCE, responser);
            }
        }
    }

  public:
    Request _req;
    string _idcArea;
    BatchCallParamPtr _param;
};

struct GetKVBatchCallback : public ProcCacheBatchCallback<GetKVBatchReq, GetKVBatchRsp, GetKVBatchCallback>, public CachePrxCallback
{

    GetKVBatchCallback(TarsCurrentPtr &current,
                       BatchCallParamPtr &param,
                       const GetKVBatchReq &req,
                       const string &objectName,
                       const int64_t beginTime,
                       const string &idcArea = "",
                       const bool repeatFlag = false)
        : ProcCacheBatchCallback<GetKVBatchReq, GetKVBatchRsp, GetKVBatchCallback>(current, param, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetKVBatchCallback() {}

    virtual void callback_getKVBatch(int ret, const GetKVBatchRsp &rsp)
    {
        CacheBatchCallParam<GetKVBatchRsp> *param = (CacheBatchCallParam<GetKVBatchRsp> *)_param.get();
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        if (param->_end)
        {
            return;
        }

        if (ret == ET_SUCC)
        {
            {
                TC_ThreadLock::Lock lock(_param->_lock);
                param->_rsp.values.insert(param->_rsp.values.end(), rsp.values.begin(), rsp.values.end());
            }

            if (param->_count.dec() <= 0)
            {
                ResponserPtr responser = make_responser(&Proxy::async_response_getKVBatch, param->_rsp);

                doResponse(ret, caller, SUCC, responser);

                param->_end = true;
            }
        }
        else
        {
            procFailCall(ret, &CacheProxy::async_getKVBatch, &Proxy::async_response_getKVBatch);
        }
    }

    virtual void callback_getKVBatch_exception(int ret)
    {
        CacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_getKVBatch);
    }
};

struct CheckKeyCallback : public ProcCacheBatchCallback<CheckKeyReq, CheckKeyRsp, CheckKeyCallback>, public CachePrxCallback
{

    CheckKeyCallback(TarsCurrentPtr &current,
                     BatchCallParamPtr &param,
                     const CheckKeyReq &req,
                     const string &objectName,
                     const int64_t beginTime,
                     const string &idcArea = "",
                     const bool repeatFlag = false)
        : ProcCacheBatchCallback<CheckKeyReq, CheckKeyRsp, CheckKeyCallback>(current, param, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~CheckKeyCallback() {}

    virtual void callback_checkKey(int ret, const CheckKeyRsp &rsp)
    {
        CacheBatchCallParam<CheckKeyRsp> *param = (CacheBatchCallParam<CheckKeyRsp> *)_param.get();
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        if (param->_end)
        {
            return;
        }

        if (ret == ET_SUCC)
        {
            {
                TC_ThreadLock::Lock lock(_param->_lock);
                param->_rsp.keyStat.insert(rsp.keyStat.begin(), rsp.keyStat.end());
            }

            if (param->_count.dec() <= 0)
            {
                ResponserPtr responser = make_responser(&Proxy::async_response_checkKey, param->_rsp);

                doResponse(ret, caller, SUCC, responser);

                param->_end = true;
            }
        }
        else
        {
            procFailCall(ret, &CacheProxy::async_checkKey, &Proxy::async_response_checkKey);
        }
    }

    virtual void callback_checkKey_exception(int ret)
    {
        CacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_checkKey);
    }
};

struct GetAllKeysCallback : public CachePrxCallback, public CacheCallbackComm

{

    GetAllKeysCallback(TarsCurrentPtr &current,
                       BatchCallParamPtr &param,
                       const GetAllKeysReq &req,
                       const string &objectName,
                       const int64_t beginTime,
                       const string &idcArea = "",
                       const bool repeatFlag = false)
        : CacheCallbackComm(current, req.moduleName, objectName, beginTime, repeatFlag), _req(req), _idcArea(idcArea), _param(param)
    {
    }
    virtual ~GetAllKeysCallback() {}

    virtual void callback_getAllKeys(int ret, const GetAllKeysRsp &rsp)
    {
        CacheBatchCallParam<GetAllKeysRsp> *param = (CacheBatchCallParam<GetAllKeysRsp> *)_param.get();
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        if (param->_end)
        {
            return;
        }

        if (ret == ET_SUCC)
        {
            {
                TC_ThreadLock::Lock lock(_param->_lock);
                param->_rsp.keys.insert(param->_rsp.keys.end(), rsp.keys.begin(), rsp.keys.end());
            }

            if (!rsp.isEnd)
            {
                param->_rsp.isEnd = false;
            }

            if (param->_count.dec() <= 0)
            {
                ResponserPtr responser = make_responser(&Proxy::async_response_getAllKeys, param->_rsp);

                doResponse(ret, caller, SUCC, responser);

                param->_end = true;
            }
        }
        else
        {
            //procFailCall(ret, &CacheProxy::async_getAllKeys, &Proxy::async_response_getAllKeys, false);
        }
    }

    virtual void callback_getAllKeys_exception(int ret)
    {
        CacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_getAllKeys);
    }

    void procExceptionCall(const int ret, void (*resmf)(TarsCurrentPtr, int, const GetAllKeysRsp &))
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        FDLOG("CBError") << caller << "|module:" << _req.moduleName << "|object:" << _objectName << "|errno:" << ret << endl;

        reportException("CBError");

        CacheBatchCallParam<GetAllKeysRsp> *param = (CacheBatchCallParam<GetAllKeysRsp> *)_param.get();
        if (param->setEnd())
        {
            GetAllKeysRsp rsp;
            ResponserPtr responser = make_responser(resmf, rsp);

            if (ret == CALLTIMEOUT)
            {
                doResponse(ET_CACHE_ERR, caller, TIME_OUT, responser);
            }
            else
            {
                doResponse(ET_CACHE_ERR, caller, EXCE, responser);
            }
        }
    }

  private:
    GetAllKeysReq _req;
    string _idcArea;
    BatchCallParamPtr _param;
};

//////////////////////////////////////////////////////////////////////////

template <typename Response>
struct WCacheBatchCallParam : public BatchCallParamComm
{
    WCacheBatchCallParam(const size_t count) : BatchCallParamComm(count), allWriteSucc(true) {}

    void addResult(const map<string, int> &keyResult)
    {
        TC_ThreadLock::Lock lock(_lock);

        _rsp.keyResult.insert(keyResult.begin(), keyResult.end());
    }

    bool allWriteSucc;
    Response _rsp;
};

template <typename Request, typename Response, typename ClassName>
struct ProcWCacheBatchCallback : public CacheCallbackComm
{
    /**
     * 构造函数
     */
    ProcWCacheBatchCallback(TarsCurrentPtr &current,
                            BatchCallParamPtr &param,
                            const Request &req,
                            const string &objectName,
                            const int64_t beginTime,
                            const bool repeatFlag = false)
        : CacheCallbackComm(current, req.moduleName, objectName, beginTime, repeatFlag), _req(req), _param(param)
    {
    }
    virtual ~ProcWCacheBatchCallback() {}

  protected:
    /**
     * 对回调做统一处理
     * @param ret，接口返回值
     * @param recallmf，CacheServer接口函数指针
     * @param responser，负责应答的对象
     * @return void
     */
    void procCallback(const int ret, const Response &rsp, void (WCacheProxy::*recallmf)(WCachePrxCallbackPtr, const Request &, const map<string, string> &), void (*resmf)(TarsCurrentPtr, int, const Response &))
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        map<string, int> &keyResult = (const_cast<Response &>(rsp)).keyResult;

        WCacheBatchCallParam<Response> *param = (WCacheBatchCallParam<Response> *)_param.get();

        if (ret != ET_SUCC)
        {
            if (ret == ET_PARTIAL_FAIL)
            {
                param->allWriteSucc = false;
            }
            else
            {
                const string &moduleName = _req.moduleName;

                FDLOG("CBError") << caller << "|module:" << moduleName << "|object:" << _objectName << "|ret:" << ret << endl;

                reportException("CBError");

                if (ret == ET_KEY_AREA_ERR || ret == ET_SERVER_TYPE_ERR)
                {
                    if (_repeatFlag || doRecall(keyResult, recallmf) != ET_SUCC)
                    {
                        param->allWriteSucc = false;
                        for (size_t i = 0; i < _req.data.size(); ++i)
                        {
                            keyResult.insert(make_pair(_req.data[i].keyItem, WRITE_ERROR));
                        }
                    }
                }
                else
                {
                    TLOGERROR("in procCallback() callback ret = " << ret << endl);
                    param->allWriteSucc = false;
                    for (size_t i = 0; i < _req.data.size(); ++i)
                    {
                        keyResult[_req.data[i].keyItem] = WRITE_ERROR;
                    }
                }
            }
        }

        param->addResult(keyResult);

        if (param->_count.dec() <= 0)
        {
            ResponserPtr responser = make_responser(resmf, param->_rsp);

            doResponse(param->allWriteSucc ? ET_SUCC : ET_PARTIAL_FAIL, caller, SUCC, responser);
        }
    }

    /**
     * 对异常回调做统一处理
     * @param ret，接口返回值
     * @param responser，负责应答的对象
     * @return void
     */
    void procExceptionCall(const int ret, void (*resmf)(TarsCurrentPtr, int, const Response &))
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        FDLOG("CBError") << caller << "|module:" << _req.moduleName << "|object:" << _objectName << "|errno:" << ret << endl;

        reportException("CBError");

        map<string, int> keyResult;
        for (size_t i = 0; i < _req.data.size(); ++i)
        {
            keyResult[_req.data[i].keyItem] = WRITE_ERROR;
        }

        WCacheBatchCallParam<Response> *param = (WCacheBatchCallParam<Response> *)_param.get();
        param->allWriteSucc = false;

        param->addResult(keyResult);

        if (param->_count.dec() <= 0)
        {
            ResponserPtr responser = make_responser(resmf, param->_rsp);

            doResponse(ET_PARTIAL_FAIL, caller, SUCC, responser);
        }
    }

    /**
     * 对路由失败的key重试
     */
    int doRecall(map<string, int> &keyResult, void (WCacheProxy::*recallmf)(WCachePrxCallbackPtr, const Request &, const map<string, string> &))
    {
        const string &moduleName = _req.moduleName;

        RouterTableInfo *routeTableInfo = g_app._routerTableInfoFactory->getRouterTableInfo(moduleName);
        if (!routeTableInfo)
        {
            TLOGERROR("ProcWCacheBatchCallback::doRecall do not support module:" << moduleName << endl);

            return ET_MODULE_NAME_INVALID;
        }

        int ret = routeTableInfo->update();
        if (ret != ET_SUCC)
        {
            TLOGERROR("ProcWCacheBatchCallback::doRecall update route table failed, module:" << moduleName << "|ret:" << ret << endl);
        }
        else
        {
            bool isEmpty = keyResult.empty();

            RouterTable &routeTable = routeTableInfo->getRouterTable();

            WCacheBatchCallParam<Response> *param = (WCacheBatchCallParam<Response> *)_param.get();

            map<string, Request> objectReq;
            for (size_t i = 0; i < _req.data.size(); ++i)
            {
                if (isEmpty || (keyResult.find(_req.data[i].keyItem) == keyResult.end()))
                {
                    // 从路由表中获得要访问的CacheServer的节点信息
                    ServerInfo serverInfo;
                    ret = routeTable.getMaster(_req.data[i].keyItem, serverInfo);
                    if (ret != RouterTable::RET_SUCC)
                    {
                        // 如果通过key无法获取CacheServer的节点信息
                        TLOGERROR("ProcWCacheBatchCallback::doRecall can not locate key:" << _req.data[i].keyItem << "|ret:" << ret << endl);

                        param->allWriteSucc = false;
                        keyResult[_req.data[i].keyItem] = WRITE_ERROR;

                        continue;
                    }

                    const string &objectName = serverInfo.WCacheServant;
                    typename map<string, Request>::iterator it = objectReq.find(objectName);
                    if (it == objectReq.end())
                    {
                        Request req(_req);
                        req.data.clear();
                        pair<typename map<string, Request>::iterator, bool> p = objectReq.insert(make_pair(objectName, req));
                        it = p.first;
                    }
                    it->second.data.push_back(_req.data[i]);
                }
            }

            ret = ET_SUCC;

            if (!objectReq.empty())
            {
                _param->_count.add(objectReq.size());

                typename map<string, Request>::const_iterator it = objectReq.begin();
                typename map<string, Request>::const_iterator itEnd = objectReq.end();
                for (; it != itEnd; ++it)
                {
                    try
                    {
                        WCachePrxCallbackPtr cb = new ClassName(_current, _param, it->second, it->first, _beginTime, true);
                        WCachePrx prxCache;
                        if (getProxy<WCachePrx>(it->first, prxCache) == ET_SUCC)
                        {
                            //重新访问CacheServer
                            (((WCacheProxy *)prxCache.get())->*recallmf)(cb, it->second, map<string, string>());
                            continue;
                        }
                    }
                    catch (exception &ex)
                    {
                        TLOGERROR("WCacheBatchCallback::doRecall exception:" << ex.what() << endl);
                    }
                    catch (...)
                    {
                        TLOGERROR("WCacheBatchCallback::doRecall catch unkown exception" << endl);
                    }

                    reportException("CBError");
                    param->allWriteSucc = false;
                    const Request &req = it->second;
                    for (size_t i = 0; i < req.data.size(); ++i)
                    {
                        keyResult[req.data[i].keyItem] = WRITE_ERROR;
                    }

                    _param->_count.dec();
                }
            }
        }

        return ret;
    }

  public:
    Request _req;
    BatchCallParamPtr _param; //
};

struct SetKVBatchCallback : public ProcWCacheBatchCallback<SetKVBatchReq, SetKVBatchRsp, SetKVBatchCallback>, public WCachePrxCallback
{

    SetKVBatchCallback(TarsCurrentPtr &current,
                       BatchCallParamPtr &param,
                       const SetKVBatchReq &req,
                       const string &objectName,
                       const int64_t beginTime,
                       const bool repeatFlag = false)
        : ProcWCacheBatchCallback<SetKVBatchReq, SetKVBatchRsp, SetKVBatchCallback>(current, param, req, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~SetKVBatchCallback() {}
    virtual void callback_setKVBatch(int ret, const SetKVBatchRsp &rsp)
    {
        WCacheBatchCallbackDo(ret, rsp, &WCacheProxy::async_setKVBatch, &Proxy::async_response_setKVBatch);
    }

    virtual void callback_setKVBatch_exception(int ret)
    {
        WCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_setKVBatch);
    }
};

struct EraseKVBatchCallback : public ProcWCacheBatchCallback<RemoveKVBatchReq, RemoveKVBatchRsp, EraseKVBatchCallback>, public WCachePrxCallback
{

    EraseKVBatchCallback(TarsCurrentPtr &current,
                         BatchCallParamPtr &param,
                         const RemoveKVBatchReq &req,
                         const string &objectName,
                         const int64_t beginTime,
                         const bool repeatFlag = false)
        : ProcWCacheBatchCallback<RemoveKVBatchReq, RemoveKVBatchRsp, EraseKVBatchCallback>(current, param, req, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~EraseKVBatchCallback() {}
    virtual void callback_eraseKVBatch(int ret, const RemoveKVBatchRsp &rsp)
    {
        WCacheBatchCallbackDo(ret, rsp, &WCacheProxy::async_eraseKVBatch, &Proxy::async_response_eraseKVBatch);
    }

    virtual void callback_eraseKVBatch_exception(int ret)
    {
        WCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_eraseKVBatch);
    }
};

struct DelKVBatchCallback : public ProcWCacheBatchCallback<RemoveKVBatchReq, RemoveKVBatchRsp, DelKVBatchCallback>, public WCachePrxCallback
{

    DelKVBatchCallback(TarsCurrentPtr &current,
                       BatchCallParamPtr &param,
                       const RemoveKVBatchReq &req,
                       const string &objectName,
                       const int64_t beginTime,
                       const bool repeatFlag = false)
        : ProcWCacheBatchCallback<RemoveKVBatchReq, RemoveKVBatchRsp, DelKVBatchCallback>(current, param, req, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~DelKVBatchCallback() {}

    virtual void callback_delKVBatch(int ret, const RemoveKVBatchRsp &rsp)
    {
        WCacheBatchCallbackDo(ret, rsp, &WCacheProxy::async_delKVBatch, &Proxy::async_response_delKVBatch);
    }

    virtual void callback_delKVBatch_exception(int ret)
    {
        WCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_delKVBatch);
    }
};

#endif

