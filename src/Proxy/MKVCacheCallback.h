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
#ifndef __MKVCACHE_CALLBACK_H_
#define __MKVCACHE_CALLBACK_H_

#include "MKCache.h"
#include "MKWCache.h"
#include "CacheCallbackComm.h"
#include "Global.h"

using namespace std;
using namespace tars;
using namespace DCache;

#define MKCacheCallbackLog(class_name, module_name, main_key, object_name, ret) TLOG_DEBUG(LOG_MEM_FUN(class_name) \
                                                                                          << " mk:" << main_key << "|module:" << module_name << "|object:" << object_name << "|ret:" << ret << endl)

#define MKCacheCallbackExcLog(class_name, module_name, main_key, object_name, ret) TLOG_DEBUG(LOG_MEM_FUN(class_name) \
                                                                                             << " mk:" << main_key << "|module:" << module_name << "|object:" << object_name << "|errno:" << ret << endl)

#define MKCacheCallbackDo(module_name, main_key, object_name, ret, rsp, recall_fun, rsp_fun) \
    MKCacheCallbackLog(MKCacheCallback, module_name, main_key, object_name, ret);            \
    ResponserPtr responser = make_responser(rsp_fun, rsp);                                   \
    procCallback(ret, recall_fun, responser)

#define MKCacheCallbackExcDo(module_name, main_key, object_name, ret, rsp_para, rsp_fun) \
    MKCacheCallbackExcLog(MKCacheCallback, module_name, main_key, object_name, ret);     \
    rsp_para rsp;                                                                        \
    ResponserPtr responser = make_responser(rsp_fun, rsp);                               \
    procExceptionCall(ret, responser)

#define MKWCacheCallbackDo(module_name, main_key, object_name, ret, recall_fun, rsp_fun) \
    MKCacheCallbackLog(MKWCacheCallback, module_name, main_key, object_name, ret);       \
    ResponserPtr responser = make_responser(rsp_fun);                                    \
    procCallback(main_key, ret, recall_fun, responser)

#define MKWCacheCallbackExcDo(module_name, main_key, object_name, ret, rsp_fun)       \
    MKCacheCallbackExcLog(MKWCacheCallback, module_name, main_key, object_name, ret); \
    ResponserPtr responser = make_responser(rsp_fun);                                 \
    procExceptionCall(main_key, ret, responser)

#define MKCacheBatchCallbackLog(class_name, module_name, object_name, ret) TLOG_DEBUG(LOG_MEM_FUN(class_name) \
                                                                                     << " module:" << module_name << "|object:" << object_name << "|ret:" << ret << endl)

#define MKCacheBatchCallbackExcLog(class_name, module_name, object_name, ret) TLOG_DEBUG(LOG_MEM_FUN(class_name) \
                                                                                        << " module:" << module_name << "|object:" << object_name << "|errno:" << ret << endl)

#define MKCacheBatchCallbackDo(ret, rsp, reroute_fun, recall_fun, rsp_fun) \
    procCallback(ret, rsp, reroute_fun, recall_fun, rsp_fun)

#define MKCacheBatchCallbackExcDo(module_name, object_name, ret, rsp_fun)            \
    MKCacheBatchCallbackExcLog(MKCacheBatchCallback, module_name, object_name, ret); \
    procExceptionCall(ret, rsp_fun)

#define MKWCacheBatchCallbackDo(ret, rsp, data_errcode, recall_fun, rsp_fun) \
    procCallback(ret, rsp, data_errcode, recall_fun, rsp_fun)

#define MKWCacheBatchCallbackExcDo(module_name, object_name, ret, data_errcode, rsp_fun) \
    MKCacheBatchCallbackExcLog(MKWCacheBatchCallback, module_name, object_name, ret);    \
    procExceptionCall(ret, data_errcode, rsp_fun)

template <typename Request, typename ClassName>
struct ProcMKCacheCallback : public CacheCallbackComm
{

    ProcMKCacheCallback(TarsCurrentPtr &current,
                        const Request &req,
                        const string &objectName,
                        const int64_t beginTime,
                        const string &idcArea = "",
                        const bool repeatFlag = false)
        : CacheCallbackComm(current, req.moduleName, objectName, beginTime, repeatFlag), _idcArea(idcArea), _req(req)
    {
    }
    virtual ~ProcMKCacheCallback() {}

  protected:
    /**
         * 对回调做统一处理
         * @param ret，接口返回值
         * @param recallmf，CacheServer接口函数指针
         * @param responser，负责应答的对象
         * @return void
         */
    void procCallback(const int ret, void (MKCacheProxy::*recallmf)(MKCachePrxCallbackPtr, const Request &, const map<string, string> &), const ResponserPtr responser)
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];
        const string &mainKey = _req.mainKey;
        const string &moduleName = _req.moduleName;

        if (ret != ET_SUCC)
        {
            FDLOG("CBError") << caller << "|mk:" << mainKey << "|module:" << moduleName << "|object:" << _objectName << "|ret:" << ret << endl;

            if ((ret == ET_KEY_AREA_ERR || ret == ET_SERVER_TYPE_ERR) && (!_repeatFlag))
            {
                reportException("CBError");

                string errMsg;
                try
                {
                    string objectName;
                    MKCachePrx prxCache;

                    // 根据模块名和mainkey重新获取要访问的cache服务的objectname以及代理，期间会强制更新路由
                    int ret = getCacheProxy(moduleName, mainKey, objectName, prxCache, _idcArea);

                    if (ret != ET_SUCC)
                    {
                        doResponse(ret, caller, EXCE, responser);

                        return;
                    }

                    MKCachePrxCallbackPtr cb = new ClassName(_current, _req, objectName, _beginTime, _idcArea, true);
                    //重新访问CacheServer
                    (((MKCacheProxy *)prxCache.get())->*recallmf)(cb, _req, map<string, string>());

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

                TLOG_ERROR("MKCacheCallback::procCallback " << errMsg << endl);

                FDLOG("CBError") << caller << "|mk:" << mainKey << "|module:" << moduleName << "|recall error:" << errMsg << endl;

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

        FDLOG("CBError") << caller << "|mk:" << _req.mainKey << "|module:" << _req.moduleName << "|object:" << _objectName << "|errno:" << ret << endl;

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
    string _idcArea; // 访问的模块存在镜像时，指定要访问的cache所属地区
    Request _req;
};

struct GetMKVCallback : public ProcMKCacheCallback<GetMKVReq, GetMKVCallback>, public MKCachePrxCallback
{
    GetMKVCallback(TarsCurrentPtr &current,
                   const GetMKVReq &req,
                   const string &objectName,
                   const int64_t beginTime,
                   const string &idcArea = "",
                   const bool repeatFlag = false)
        : ProcMKCacheCallback<GetMKVReq, GetMKVCallback>(current, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetMKVCallback() {}
    virtual void callback_getMKV(int ret, const GetMKVRsp &rsp)
    {
        MKCacheCallbackDo(_req.moduleName, _req.mainKey, _objectName, ret, rsp, &MKCacheProxy::async_getMKV, &Proxy::async_response_getMKV);
    }

    virtual void callback_getMKV_exception(int ret)
    {
        MKCacheCallbackExcDo(_req.moduleName, _req.mainKey, _objectName, ret, GetMKVRsp, &Proxy::async_response_getMKV);
    }
};

struct GetMainKeyCountCallback : public ProcMKCacheCallback<MainKeyReq, GetMainKeyCountCallback>, public MKCachePrxCallback
{
    GetMainKeyCountCallback(TarsCurrentPtr &current,
                            const MainKeyReq &req,
                            const string &objectName,
                            const int64_t beginTime,
                            const string &idcArea = "",
                            const bool repeatFlag = false)
        : ProcMKCacheCallback<MainKeyReq, GetMainKeyCountCallback>(current, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetMainKeyCountCallback() {}
    virtual void callback_getMainKeyCount(int ret)
    {
        MKCacheCallbackLog(MKCacheCallback, _req.moduleName, _req.mainKey, _objectName, ret);

        ResponserPtr responser = make_responser(&Proxy::async_response_getMainKeyCount);

        procCallback(ret, &MKCacheProxy::async_getMainKeyCount, responser);
    }

    virtual void callback_getMainKeyCount_exception(int ret)
    {
        MKCacheCallbackExcLog(MKCacheCallback, _req.moduleName, _req.mainKey, _objectName, ret);

        ResponserPtr responser = make_responser(&Proxy::async_response_getMainKeyCount);

        procExceptionCall(ret, responser);
    }
};

struct GetListCallback : public ProcMKCacheCallback<GetListReq, GetListCallback>, public MKCachePrxCallback
{
    GetListCallback(TarsCurrentPtr &current,
                    const GetListReq &req,
                    const string &objectName,
                    const int64_t beginTime,
                    const string &idcArea = "",
                    const bool repeatFlag = false)
        : ProcMKCacheCallback<GetListReq, GetListCallback>(current, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetListCallback() {}

    virtual void callback_getList(int ret, const GetListRsp &rsp)
    {
        MKCacheCallbackDo(_req.moduleName, _req.mainKey, _objectName, ret, rsp, &MKCacheProxy::async_getList, &Proxy::async_response_getList);
    }

    virtual void callback_getList_exception(int ret)
    {
        MKCacheCallbackExcDo(_req.moduleName, _req.mainKey, _objectName, ret, GetListRsp, &Proxy::async_response_getList);
    }
};

struct GetRangeListCallback : public ProcMKCacheCallback<GetRangeListReq, GetRangeListCallback>, public MKCachePrxCallback
{
    GetRangeListCallback(TarsCurrentPtr &current,
                         const GetRangeListReq &req,
                         const string &objectName,
                         const int64_t beginTime,
                         const string &idcArea = "",
                         const bool repeatFlag = false)
        : ProcMKCacheCallback<GetRangeListReq, GetRangeListCallback>(current, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetRangeListCallback() {}

    virtual void callback_getRangeList(int ret, const BatchEntry &rsp)
    {
        MKCacheCallbackDo(_req.moduleName, _req.mainKey, _objectName, ret, rsp, &MKCacheProxy::async_getRangeList, &Proxy::async_response_getRangeList);
    }

    virtual void callback_getRangeList_exception(int ret)
    {
        MKCacheCallbackExcDo(_req.moduleName, _req.mainKey, _objectName, ret, BatchEntry, &Proxy::async_response_getRangeList);
    }
};

struct GetSetCallback : public ProcMKCacheCallback<GetSetReq, GetSetCallback>, public MKCachePrxCallback
{
    GetSetCallback(TarsCurrentPtr &current,
                   const GetSetReq &req,
                   const string &objectName,
                   const int64_t beginTime,
                   const string &idcArea = "",
                   const bool repeatFlag = false)
        : ProcMKCacheCallback<GetSetReq, GetSetCallback>(current, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetSetCallback() {}

    virtual void callback_getSet(int ret, const BatchEntry &rsp)
    {
        MKCacheCallbackDo(_req.moduleName, _req.mainKey, _objectName, ret, rsp, &MKCacheProxy::async_getSet, &Proxy::async_response_getSet);
    }

    virtual void callback_getSet_exception(int ret)
    {
        MKCacheCallbackExcDo(_req.moduleName, _req.mainKey, _objectName, ret, BatchEntry, &Proxy::async_response_getSet);
    }
};

struct GetZSetScoreCallback : public ProcMKCacheCallback<GetZsetScoreReq, GetZSetScoreCallback>, public MKCachePrxCallback
{
    GetZSetScoreCallback(TarsCurrentPtr &current,
                         const GetZsetScoreReq &req,
                         const string &objectName,
                         const int64_t beginTime,
                         const string &idcArea = "",
                         const bool repeatFlag = false)
        : ProcMKCacheCallback<GetZsetScoreReq, GetZSetScoreCallback>(current, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetZSetScoreCallback() {}

    virtual void callback_getZSetScore(int ret, double score)
    {
        MKCacheCallbackDo(_req.moduleName, _req.mainKey, _objectName, ret, score, &MKCacheProxy::async_getZSetScore, &Proxy::async_response_getZSetScore);
    }

    virtual void callback_getZSetScore_exception(int ret)
    {
        MKCacheCallbackExcDo(_req.moduleName, _req.mainKey, _objectName, ret, double, &Proxy::async_response_getZSetScore);
    }
};

struct GetZSetPosCallback : public ProcMKCacheCallback<GetZsetPosReq, GetZSetPosCallback>, public MKCachePrxCallback
{
    GetZSetPosCallback(TarsCurrentPtr &current,
                       const GetZsetPosReq &req,
                       const string &objectName,
                       const int64_t beginTime,
                       const string &idcArea = "",
                       const bool repeatFlag = false)
        : ProcMKCacheCallback<GetZsetPosReq, GetZSetPosCallback>(current, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetZSetPosCallback() {}

    virtual void callback_getZSetPos(int ret, tars::Int64 pos)
    {
        MKCacheCallbackDo(_req.moduleName, _req.mainKey, _objectName, ret, pos, &MKCacheProxy::async_getZSetPos, &Proxy::async_response_getZSetPos);
    }

    virtual void callback_getZSetPos_exception(int ret)
    {
        MKCacheCallbackExcDo(_req.moduleName, _req.mainKey, _objectName, ret, long long, &Proxy::async_response_getZSetPos);
    }
};

struct GetZSetByPosCallback : public ProcMKCacheCallback<GetZsetByPosReq, GetZSetByPosCallback>, public MKCachePrxCallback
{
    GetZSetByPosCallback(TarsCurrentPtr &current,
                         const GetZsetByPosReq &req,
                         const string &objectName,
                         const int64_t beginTime,
                         const string &idcArea = "",
                         const bool repeatFlag = false)
        : ProcMKCacheCallback<GetZsetByPosReq, GetZSetByPosCallback>(current, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetZSetByPosCallback() {}

    virtual void callback_getZSetByPos(int ret, const BatchEntry &rsp)
    {
        MKCacheCallbackDo(_req.moduleName, _req.mainKey, _objectName, ret, rsp, &MKCacheProxy::async_getZSetByPos, &Proxy::async_response_getZSetByPos);
    }

    virtual void callback_getZSetByPos_exception(int ret)
    {
        MKCacheCallbackExcDo(_req.moduleName, _req.mainKey, _objectName, ret, BatchEntry, &Proxy::async_response_getZSetByPos);
    }
};

struct GetZSetByScoreCallback : public ProcMKCacheCallback<GetZsetByScoreReq, GetZSetByScoreCallback>, public MKCachePrxCallback
{
    GetZSetByScoreCallback(TarsCurrentPtr &current,
                           const GetZsetByScoreReq &req,
                           const string &objectName,
                           const int64_t beginTime,
                           const string &idcArea = "",
                           const bool repeatFlag = false)
        : ProcMKCacheCallback<GetZsetByScoreReq, GetZSetByScoreCallback>(current, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetZSetByScoreCallback() {}

    virtual void callback_getZSetByScore(int ret, const BatchEntry &rsp)
    {
        MKCacheCallbackDo(_req.moduleName, _req.mainKey, _objectName, ret, rsp, &MKCacheProxy::async_getZSetByScore, &Proxy::async_response_getZSetByScore);
    }

    virtual void callback_getZSetByScore_exception(int ret)
    {
        MKCacheCallbackExcDo(_req.moduleName, _req.mainKey, _objectName, ret, BatchEntry, &Proxy::async_response_getZSetByScore);
    }
};

//////////////////////////////////////////////////////////////////////////

template <typename Request, typename ClassName>
struct ProcMKWCacheCallback : public CacheCallbackComm
{
    ProcMKWCacheCallback(TarsCurrentPtr &current,
                         const Request &req,
                         const string &mainKey,
                         const string &objectName,
                         const int64_t beginTime,
                         const bool repeatFlag = false)
        : CacheCallbackComm(current, req.moduleName, objectName, beginTime, repeatFlag), _mainKey(mainKey), _req(req)
    {
    }
    virtual ~ProcMKWCacheCallback() {}

  protected:
    /**
         * 对回调做统一处理
         * @param ret，接口返回值
         * @param recallmf，CacheServer接口函数指针
         * @param responser，负责应答的对象
         * @return void
         */
    void procCallback(const string &mainKey, const int ret, void (MKWCacheProxy::*recallmf)(MKWCachePrxCallbackPtr, const Request &, const map<string, string> &), const ResponserPtr responser)
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];
        const string &moduleName = _req.moduleName;

        if (ret != ET_SUCC)
        {
            FDLOG("CBError") << caller << "|mk:" << mainKey << "|module:" << moduleName << "|object:" << _objectName << "|ret:" << ret << endl;

            if ((ret == ET_KEY_AREA_ERR || ret == ET_SERVER_TYPE_ERR) && (!_repeatFlag))
            {
                reportException("CBError");

                string errMsg;
                try
                {
                    string objectName;
                    MKWCachePrx prxCache;

                    // 根据模块名和key重新获取要访问的cache服务的objectname以及代理，期间会强制更新路由
                    int ret = getWCacheProxy(moduleName, mainKey, objectName, prxCache);

                    if (ret != ET_SUCC)
                    {
                        doResponse(ret, caller, EXCE, responser);

                        return;
                    }

                    MKWCachePrxCallbackPtr cb = new ClassName(_current, _req, objectName, _beginTime, true);
                    //重新访问CacheServer
                    (((MKWCacheProxy *)prxCache.get())->*recallmf)(cb, _req, map<string, string>());

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

                TLOG_ERROR("MKWCacheCallback::procCallback " << errMsg << endl);

                FDLOG("CBError") << caller << "|mk:" << mainKey << "|module:" << moduleName << "|recall error:" << errMsg << endl;

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
    void procExceptionCall(const string &mainKey, const int ret, const ResponserPtr responser)
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        FDLOG("CBError") << caller << "|mk:" << mainKey << "|module:" << _req.moduleName << "|object:" << _objectName << "|errno:" << ret << endl;

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
    string _mainKey;
    Request _req;
};

struct InsertMKVCallback : public ProcMKWCacheCallback<InsertMKVReq, InsertMKVCallback>, public MKWCachePrxCallback
{
    InsertMKVCallback(TarsCurrentPtr &current,
                      const InsertMKVReq &req,
                      const string &objectName,
                      const int64_t beginTime,
                      const bool repeatFlag = false)
        : ProcMKWCacheCallback<InsertMKVReq, InsertMKVCallback>(current, req, req.data.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~InsertMKVCallback() {}

    virtual void callback_insertMKV(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_insertMKV, &Proxy::async_response_insertMKV);
    }

    virtual void callback_insertMKV_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_insertMKV);
    }
};

struct UpdateMKVCallback : public ProcMKWCacheCallback<UpdateMKVReq, UpdateMKVCallback>, public MKWCachePrxCallback
{
    UpdateMKVCallback(TarsCurrentPtr &current,
                      const UpdateMKVReq &req,
                      const string &objectName,
                      const int64_t beginTime,
                      const bool repeatFlag = false)
        : ProcMKWCacheCallback<UpdateMKVReq, UpdateMKVCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~UpdateMKVCallback() {}

    virtual void callback_updateMKV(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_updateMKV, &Proxy::async_response_updateMKV);
    }

    virtual void callback_updateMKV_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_updateMKV);
    }
};

struct UpdateMKVAtomCallback : public ProcMKWCacheCallback<UpdateMKVAtomReq, UpdateMKVAtomCallback>, public MKWCachePrxCallback
{
    UpdateMKVAtomCallback(TarsCurrentPtr &current,
                          const UpdateMKVAtomReq &req,
                          const string &objectName,
                          const int64_t beginTime,
                          const bool repeatFlag = false)
        : ProcMKWCacheCallback<UpdateMKVAtomReq, UpdateMKVAtomCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~UpdateMKVAtomCallback() {}

    virtual void callback_updateMKVAtom(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_updateMKVAtom, &Proxy::async_response_updateMKVAtom);
    }

    virtual void callback_updateMKVAtom_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_updateMKVAtom);
    }
};

struct EraseMKVCallback : public ProcMKWCacheCallback<MainKeyReq, EraseMKVCallback>, public MKWCachePrxCallback
{
    EraseMKVCallback(TarsCurrentPtr &current,
                     const MainKeyReq &req,
                     const string &objectName,
                     const int64_t beginTime,
                     const bool repeatFlag = false)
        : ProcMKWCacheCallback<MainKeyReq, EraseMKVCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~EraseMKVCallback() {}

    virtual void callback_eraseMKV(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_eraseMKV, &Proxy::async_response_eraseMKV);
    }

    virtual void callback_eraseMKV_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_eraseMKV);
    }
};

struct DelMKVCallback : public ProcMKWCacheCallback<DelMKVReq, DelMKVCallback>, public MKWCachePrxCallback
{
    DelMKVCallback(TarsCurrentPtr &current,
                   const DelMKVReq &req,
                   const string &objectName,
                   const int64_t beginTime,
                   const bool repeatFlag = false)
        : ProcMKWCacheCallback<DelMKVReq, DelMKVCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~DelMKVCallback() {}

    virtual void callback_delMKV(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_delMKV, &Proxy::async_response_delMKV);
    }

    virtual void callback_delMKV_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_delMKV);
    }
};

struct PushListCallback : public ProcMKWCacheCallback<PushListReq, PushListCallback>, public MKWCachePrxCallback
{
    PushListCallback(TarsCurrentPtr &current,
                     const PushListReq &req,
                     const string &objectName,
                     const int64_t beginTime,
                     const bool repeatFlag = false)
        : ProcMKWCacheCallback<PushListReq, PushListCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~PushListCallback() {}
    virtual void callback_pushList(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_pushList, &Proxy::async_response_pushList);
    }

    virtual void callback_pushList_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_pushList);
    }
};

struct PopListCallback : public ProcMKWCacheCallback<PopListReq, PopListCallback>, public MKWCachePrxCallback
{
    PopListCallback(TarsCurrentPtr &current,
                    const PopListReq &req,
                    const string &objectName,
                    const int64_t beginTime,
                    const bool repeatFlag = false)
        : ProcMKWCacheCallback<PopListReq, PopListCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~PopListCallback() {}
    virtual void callback_popList(int ret, const PopListRsp &rsp)
    {
        MKCacheCallbackLog(MKWCacheCallback, _req.moduleName, _mainKey, _objectName, ret);

        ResponserPtr responser = make_responser(&Proxy::async_response_popList, rsp);

        procCallback(_mainKey, ret, &MKWCacheProxy::async_popList, responser);
    }

    virtual void callback_popList_exception(int ret)
    {
        MKCacheCallbackExcLog(MKWCacheCallback, _req.moduleName, _mainKey, _objectName, ret);

        PopListRsp rsp;
        ResponserPtr responser = make_responser(&Proxy::async_response_popList, rsp);

        procExceptionCall(_mainKey, ret, responser);
    }
};

struct ReplaceListCallback : public ProcMKWCacheCallback<ReplaceListReq, ReplaceListCallback>, public MKWCachePrxCallback
{
    ReplaceListCallback(TarsCurrentPtr &current,
                        const ReplaceListReq &req,
                        const string &objectName,
                        const int64_t beginTime,
                        const bool repeatFlag = false)
        : ProcMKWCacheCallback<ReplaceListReq, ReplaceListCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~ReplaceListCallback() {}

    virtual void callback_replaceList(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_replaceList, &Proxy::async_response_replaceList);
    }

    virtual void callback_replaceList_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_replaceList);
    }
};

struct TrimListCallback : public ProcMKWCacheCallback<TrimListReq, TrimListCallback>, public MKWCachePrxCallback
{
    TrimListCallback(TarsCurrentPtr &current,
                     const TrimListReq &req,
                     const string &objectName,
                     const int64_t beginTime,
                     const bool repeatFlag = false)
        : ProcMKWCacheCallback<TrimListReq, TrimListCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~TrimListCallback() {}

    virtual void callback_trimList(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_trimList, &Proxy::async_response_trimList);
    }

    virtual void callback_trimList_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_trimList);
    }
};

struct RemListCallback : public ProcMKWCacheCallback<RemListReq, RemListCallback>, public MKWCachePrxCallback
{
    RemListCallback(TarsCurrentPtr &current,
                    const RemListReq &req,
                    const string &objectName,
                    const int64_t beginTime,
                    const bool repeatFlag = false)
        : ProcMKWCacheCallback<RemListReq, RemListCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~RemListCallback() {}

    virtual void callback_remList(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_remList, &Proxy::async_response_remList);
    }

    virtual void callback_remList_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_remList);
    }
};

struct AddSetCallback : public ProcMKWCacheCallback<AddSetReq, AddSetCallback>, public MKWCachePrxCallback
{
    AddSetCallback(TarsCurrentPtr &current,
                   const AddSetReq &req,
                   const string &objectName,
                   const int64_t beginTime,
                   const bool repeatFlag = false)
        : ProcMKWCacheCallback<AddSetReq, AddSetCallback>(current, req, req.value.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~AddSetCallback() {}

    virtual void callback_addSet(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_addSet, &Proxy::async_response_addSet);
    }

    virtual void callback_addSet_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_addSet);
    }
};

struct DelSetCallback : public ProcMKWCacheCallback<DelSetReq, DelSetCallback>, public MKWCachePrxCallback
{
    DelSetCallback(TarsCurrentPtr &current,
                   const DelSetReq &req,
                   const string &objectName,
                   const int64_t beginTime,
                   const bool repeatFlag = false)
        : ProcMKWCacheCallback<DelSetReq, DelSetCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~DelSetCallback() {}

    virtual void callback_delSet(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_delSet, &Proxy::async_response_delSet);
    }

    virtual void callback_delSet_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_delSet);
    }
};

struct AddZSetCallback : public ProcMKWCacheCallback<AddZSetReq, AddZSetCallback>, public MKWCachePrxCallback
{
    AddZSetCallback(TarsCurrentPtr &current,
                    const AddZSetReq &req,
                    const string &objectName,
                    const int64_t beginTime,
                    const bool repeatFlag = false)
        : ProcMKWCacheCallback<AddZSetReq, AddZSetCallback>(current, req, req.value.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~AddZSetCallback() {}

    virtual void callback_addZSet(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_addZSet, &Proxy::async_response_addZSet);
    }

    virtual void callback_addZSet_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_addZSet);
    }
};

struct IncScoreZSetCallback : public ProcMKWCacheCallback<IncZSetScoreReq, IncScoreZSetCallback>, public MKWCachePrxCallback
{
    IncScoreZSetCallback(TarsCurrentPtr &current,
                         const IncZSetScoreReq &req,
                         const string &objectName,
                         const int64_t beginTime,
                         const bool repeatFlag = false)
        : ProcMKWCacheCallback<IncZSetScoreReq, IncScoreZSetCallback>(current, req, req.value.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~IncScoreZSetCallback() {}

    virtual void callback_incScoreZSet(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_incScoreZSet, &Proxy::async_response_incScoreZSet);
    }

    virtual void callback_incScoreZSet_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_incScoreZSet);
    }
};

struct DelZSetCallback : public ProcMKWCacheCallback<DelZSetReq, DelZSetCallback>, public MKWCachePrxCallback
{
    DelZSetCallback(TarsCurrentPtr &current,
                    const DelZSetReq &req,
                    const string &objectName,
                    const int64_t beginTime,
                    const bool repeatFlag = false)
        : ProcMKWCacheCallback<DelZSetReq, DelZSetCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~DelZSetCallback() {}

    virtual void callback_delZSet(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_delZSet, &Proxy::async_response_delZSet);
    }

    virtual void callback_delZSet_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_delZSet);
    }
};

struct DelZSetByScoreCallback : public ProcMKWCacheCallback<DelZSetByScoreReq, DelZSetByScoreCallback>, public MKWCachePrxCallback
{
    DelZSetByScoreCallback(TarsCurrentPtr &current,
                           const DelZSetByScoreReq &req,
                           const string &objectName,
                           const int64_t beginTime,
                           const bool repeatFlag = false)
        : ProcMKWCacheCallback<DelZSetByScoreReq, DelZSetByScoreCallback>(current, req, req.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~DelZSetByScoreCallback() {}

    virtual void callback_delZSetByScore(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_delZSetByScore, &Proxy::async_response_delZSetByScore);
    }

    virtual void callback_delZSetByScore_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_delZSetByScore);
    }
};

struct UpdateZSetCallback : public ProcMKWCacheCallback<UpdateZSetReq, UpdateZSetCallback>, public MKWCachePrxCallback
{
    UpdateZSetCallback(TarsCurrentPtr &current,
                       const UpdateZSetReq &req,
                       const string &objectName,
                       const int64_t beginTime,
                       const bool repeatFlag = false)
        : ProcMKWCacheCallback<UpdateZSetReq, UpdateZSetCallback>(current, req, req.value.mainKey, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~UpdateZSetCallback() {}

    virtual void callback_updateZSet(int ret)
    {
        MKWCacheCallbackDo(_req.moduleName, _mainKey, _objectName, ret, &MKWCacheProxy::async_updateZSet, &Proxy::async_response_updateZSet);
    }

    virtual void callback_updateZSet_exception(int ret)
    {
        MKWCacheCallbackExcDo(_req.moduleName, _mainKey, _objectName, ret, &Proxy::async_response_updateZSet);
    }
};

//////////////////////////////////////////////////////////////////////////

template <typename Response>
struct MKCacheBatchCallParam : public BatchCallParamComm
{
    MKCacheBatchCallParam(const size_t count) : BatchCallParamComm(count), _end(false) {}

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
struct ProcMKCacheBatchCallback : public CacheCallbackComm
{

    ProcMKCacheBatchCallback(TarsCurrentPtr &current,
                             BatchCallParamPtr &param,
                             const Request &req,
                             const string &objectName,
                             const int64_t beginTime,
                             const string &idcArea = "",
                             const bool repeatFlag = false)
        : CacheCallbackComm(current, req.moduleName, objectName, beginTime, repeatFlag), _idcArea(idcArea), _param(param), _req(req)
    {
    }
    virtual ~ProcMKCacheBatchCallback() {}

  protected:
    /**
         * 对回调做统一处理
         * @param ret，接口返回值
         * @param recallmf，CacheServer接口函数指针
         * @param responser，负责应答的对象
         * @return void
         */
    void procCallback(int ret,
                      const Response &rsp,
                      int (ProcMKCacheBatchCallback::*reroutemf)(const RouterTable &, map<string, Request> &),
                      void (MKCacheProxy::*recallmf)(MKCachePrxCallbackPtr, const Request &, const map<string, string> &),
                      void (*resmf)(TarsCurrentPtr, int, const Response &))
    {
        MKCacheBatchCallParam<Response> *param = (MKCacheBatchCallParam<Response> *)(_param.get());

        if (param->_end)
        {
            return;
        }

        const string &caller = _current->getContext()[CONTEXT_CALLER];
        const string &moduleName = _req.moduleName;

        if (ret == ET_SUCC)
        {
            {
                TC_ThreadLock::Lock lock(param->_lock);

                param->_rsp.data.insert(param->_rsp.data.end(), rsp.data.begin(), rsp.data.end());
            }

            if ((--param->_count) <= 0)
            {
                ResponserPtr responser = make_responser(resmf, param->_rsp);

                doResponse(ret, caller, SUCC, responser);

                param->_end = true;
            }
        }
        else
        {
            FDLOG("CBError") << caller << "|module:" << moduleName << "|object:" << _objectName << "|ret:" << ret << endl;

            reportException("CBError");

            if (ret == ET_KEY_AREA_ERR || ret == ET_SERVER_TYPE_ERR)
            {
                if (!_repeatFlag)
                {
                    ret = doRecall(reroutemf, recallmf);

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
            else
            {
                // 将cache端返回的ET_SYS_ERR映射为ET_CACHE_ERR返回给客户端
                if (ret == ET_SYS_ERR)
                {
                    ret = ET_CACHE_ERR;
                }
            }

            if (param->setEnd())
            {
                Response rsp;
                ResponserPtr responser = make_responser(resmf, rsp);

                doResponse(ret, caller, SUCC, responser);
            }
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

        MKCacheBatchCallParam<Response> *param = (MKCacheBatchCallParam<Response> *)(_param.get());
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

    /**
         * 对路由失败的key重试
         */
    int doRecall(int (ProcMKCacheBatchCallback::*reroutemf)(const RouterTable &, map<string, Request> &),
                 void (MKCacheProxy::*recallmf)(MKCachePrxCallbackPtr, const Request &, const map<string, string> &))
    {
        const string &moduleName = _req.moduleName;

        RouterTableInfo *routeTableInfo = g_app._routerTableInfoFactory->getRouterTableInfo(moduleName);
        if (!routeTableInfo)
        {
            TLOG_ERROR("MKCacheBatchCallback::doRecall do not support module:" << moduleName << endl);

            return ET_MODULE_NAME_INVALID;
        }

        int ret = routeTableInfo->update();
        if (ret != ET_SUCC)
        {
            TLOG_ERROR("MKCacheBatchCallback::doRecall update route table failed, module:" << moduleName << "|ret:" << ret << endl);
        }
        else
        {
            RouterTable &routeTable = routeTableInfo->getRouterTable();

            map<string, Request> objectReq;

            ret = (this->*reroutemf)(routeTable, objectReq);

            if (ret == ET_SUCC)
            {
                _param->_count.fetch_add(objectReq.size() - 1);

                typename map<string, Request>::const_iterator it = objectReq.begin();
                typename map<string, Request>::const_iterator itEnd = objectReq.end();
                for (; it != itEnd; ++it)
                {
                    try
                    {
                        MKCachePrxCallbackPtr cb = new ClassName(_current, _param, it->second, it->first, _beginTime, _idcArea, true);
                        MKCachePrx prxCache;
                        ret = getProxy(it->first, prxCache);
                        if (ret == ET_SUCC)
                        {
                            //重新访问CacheServer
                            (((MKCacheProxy *)prxCache.get())->*recallmf)(cb, it->second, map<string, string>());

                            continue;
                        }
                        else
                        {
                            return ret;
                        }
                    }
                    catch (exception &ex)
                    {
                        TLOG_ERROR("MKCacheBatchCallback::doRecall exception:" << ex.what() << endl);
                    }
                    catch (...)
                    {
                        TLOG_ERROR("MKCacheBatchCallback::doRecall catch unkown exception" << endl);
                    }

                    return ET_CACHE_ERR;
                }
            }
        }

        if (ret != ET_SUCC)
        {
            ret = ET_SYS_ERR; // 将各种错误统一转换为ET_SYS_ERR返回给客户端
        }

        return ret;
    }

  protected:
    int reRouteGetMKVBatch(const RouterTable &routeTable, map<string, MKVBatchReq> &objectReq)
    {
        int ret = ET_SUCC;

        for (size_t i = 0; i < _req.mainKeys.size(); ++i)
        {
            // 从路由表中获得要访问的CacheServer的节点信息
            ServerInfo serverInfo;
            if (_idcArea.empty())
            {
                ret = routeTable.getMaster(_req.mainKeys[i], serverInfo);
            }
            else
            {
                ret = routeTable.getIdcServer(_req.mainKeys[i], _idcArea, false, serverInfo);
            }
            if (ret != RouterTable::RET_SUCC)
            {
                // 如果通过key无法获取CacheServer的节点信息
                TLOG_ERROR("MKCacheBatchCallback::reRouteGetMKVBatch can not locate key:" << _req.mainKeys[i] << "|ret:" << ret << endl);

                break;
            }

            const string &objectName = serverInfo.CacheServant;
            map<string, MKVBatchReq>::iterator it = objectReq.find(objectName);
            if (it == objectReq.end())
            {
                MKVBatchReq req(_req);
                req.mainKeys.clear();
                pair<map<string, MKVBatchReq>::iterator, bool> p = objectReq.insert(make_pair(objectName, req));
                it = p.first;
            }
            it->second.mainKeys.push_back(_req.mainKeys[i]);
        }

        return ret;
    }

    int reRouteGetMUKBatch(const RouterTable &routeTable, map<string, MUKBatchReq> &objectReq)
    {
        int ret = ET_SUCC;

        for (size_t i = 0; i < _req.primaryKeys.size(); ++i)
        {
            const string &mainKey = _req.primaryKeys[i].mainKey;

            // 从路由表中获得要访问的CacheServer的节点信息
            ServerInfo serverInfo;
            if (_idcArea.empty())
            {
                ret = routeTable.getMaster(mainKey, serverInfo);
            }
            else
            {
                ret = routeTable.getIdcServer(mainKey, _idcArea, false, serverInfo);
            }
            if (ret != RouterTable::RET_SUCC)
            {
                // 如果通过key无法获取CacheServer的节点信息
                TLOG_ERROR("MKCacheBatchCallback::reRouteGetMUKBatch can not locate key:" << mainKey << "|ret:" << ret << endl);

                break;
            }

            const string &objectName = serverInfo.CacheServant;
            map<string, MUKBatchReq>::iterator it = objectReq.find(objectName);
            if (it == objectReq.end())
            {
                MUKBatchReq req(_req);
                req.primaryKeys.clear();
                pair<map<string, MUKBatchReq>::iterator, bool> p = objectReq.insert(make_pair(objectName, req));
                it = p.first;
            }
            it->second.primaryKeys.push_back(_req.primaryKeys[i]);
        }

        return ret;
    }

    int reRouteGetMKVBatchEx(const RouterTable &routeTable, map<string, MKVBatchExReq> &objectReq)
    {
        int ret = ET_SUCC;

        for (size_t i = 0; i < _req.cond.size(); ++i)
        {
            const string &mainKey = _req.cond[i].mainKey;

            // 从路由表中获得要访问的CacheServer的节点信息
            ServerInfo serverInfo;
            if (_idcArea.empty())
            {
                ret = routeTable.getMaster(mainKey, serverInfo);
            }
            else
            {
                ret = routeTable.getIdcServer(mainKey, _idcArea, false, serverInfo);
            }
            if (ret != RouterTable::RET_SUCC)
            {
                // 如果通过key无法获取CacheServer的节点信息
                TLOG_ERROR("MKCacheBatchCallback::reRouteGetMKVBatchEx can not locate key:" << mainKey << "|ret:" << ret << endl);

                break;
            }

            const string &objectName = serverInfo.CacheServant;
            map<string, MKVBatchExReq>::iterator it = objectReq.find(objectName);
            if (it == objectReq.end())
            {
                MKVBatchExReq req(_req);
                req.cond.clear();
                pair<map<string, MKVBatchExReq>::iterator, bool> p = objectReq.insert(make_pair(objectName, req));
                it = p.first;
            }
            it->second.cond.push_back(_req.cond[i]);
        }

        return ret;
    }

  protected:
    string _idcArea;
    BatchCallParamPtr _param;
    Request _req;
};

struct GetMKVBatchCallback : public ProcMKCacheBatchCallback<MKVBatchReq, MKVBatchRsp, GetMKVBatchCallback>, public MKCachePrxCallback
{

    GetMKVBatchCallback(TarsCurrentPtr &current,
                        BatchCallParamPtr &param,
                        const MKVBatchReq &req,
                        const string &objectName,
                        const int64_t beginTime,
                        const string &idcArea = "",
                        const bool repeatFlag = false)
        : ProcMKCacheBatchCallback<MKVBatchReq, MKVBatchRsp, GetMKVBatchCallback>(current, param, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetMKVBatchCallback() {}

    virtual void callback_getMKVBatch(int ret, const MKVBatchRsp &rsp)
    {
        MKCacheBatchCallbackDo(ret, rsp, &GetMKVBatchCallback::reRouteGetMKVBatch, &MKCacheProxy::async_getMKVBatch, &Proxy::async_response_getMKVBatch);
    }

    virtual void callback_getMKVBatch_exception(int ret)
    {
        MKCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_getMKVBatch);
    }
};

struct GetMUKBatchCallback : public ProcMKCacheBatchCallback<MUKBatchReq, MUKBatchRsp, GetMUKBatchCallback>, public MKCachePrxCallback
{

    GetMUKBatchCallback(TarsCurrentPtr &current,
                        BatchCallParamPtr &param,
                        const MUKBatchReq &req,
                        const string &objectName,
                        const int64_t beginTime,
                        const string &idcArea = "",
                        const bool repeatFlag = false)
        : ProcMKCacheBatchCallback<MUKBatchReq, MUKBatchRsp, GetMUKBatchCallback>(current, param, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetMUKBatchCallback() {}

    virtual void callback_getMUKBatch(int ret, const MUKBatchRsp &rsp)
    {
        MKCacheBatchCallbackDo(ret, rsp, &GetMUKBatchCallback::reRouteGetMUKBatch, &MKCacheProxy::async_getMUKBatch, &Proxy::async_response_getMUKBatch);
    }

    virtual void callback_getMUKBatch_exception(int ret)
    {
        MKCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_getMUKBatch);
    }
};

struct GetMKVBatchExCallback : public ProcMKCacheBatchCallback<MKVBatchExReq, MKVBatchExRsp, GetMKVBatchExCallback>, public MKCachePrxCallback
{

    GetMKVBatchExCallback(TarsCurrentPtr &current,
                          BatchCallParamPtr &param,
                          const MKVBatchExReq &req,
                          const string &objectName,
                          const int64_t beginTime,
                          const string &idcArea = "",
                          const bool repeatFlag = false)
        : ProcMKCacheBatchCallback<MKVBatchExReq, MKVBatchExRsp, GetMKVBatchExCallback>(current, param, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetMKVBatchExCallback() {}

    virtual void callback_getMKVBatchEx(int ret, const MKVBatchExRsp &rsp)
    {
        MKCacheBatchCallbackDo(ret, rsp, &GetMKVBatchExCallback::reRouteGetMKVBatchEx, &MKCacheProxy::async_getMKVBatchEx, &Proxy::async_response_getMKVBatchEx);
    }

    virtual void callback_getMKVBatchEx_exception(int ret)
    {
        MKCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_getMKVBatchEx);
    }
};

struct GetAllMainKeyCallback : public ProcMKCacheBatchCallback<GetAllKeysReq, GetAllKeysRsp, GetAllMainKeyCallback>, public MKCachePrxCallback
{

    GetAllMainKeyCallback(TarsCurrentPtr &current,
                          BatchCallParamPtr &param,
                          const GetAllKeysReq &req,
                          const string &objectName,
                          const int64_t beginTime,
                          const string &idcArea = "",
                          const bool repeatFlag = false)
        : ProcMKCacheBatchCallback<GetAllKeysReq, GetAllKeysRsp, GetAllMainKeyCallback>(current, param, req, objectName, beginTime, idcArea, repeatFlag)
    {
    }
    virtual ~GetAllMainKeyCallback() {}

    virtual void callback_getAllMainKey(int ret, const GetAllKeysRsp &rsp)
    {
        MKCacheBatchCallParam<GetAllKeysRsp> *param = (MKCacheBatchCallParam<GetAllKeysRsp> *)(_param.get());

        if (param->_end)
        {
            return;
        }

        if (ret == ET_SUCC)
        {
            {
                TC_ThreadLock::Lock lock(param->_lock);
                param->_rsp.keys.insert(param->_rsp.keys.end(), rsp.keys.begin(), rsp.keys.end());
            }

            if (!rsp.isEnd)
            {
                param->_rsp.isEnd = false;
            }

            if ((--param->_count) <= 0)
            {
                ResponserPtr responser = make_responser(&Proxy::async_response_getAllMainKey, param->_rsp);
                const string &caller = _current->getContext()[CONTEXT_CALLER];
                doResponse(ret, caller, SUCC, responser);

                param->_end = true;
            }
        }
        else
        {
            const string &caller = _current->getContext()[CONTEXT_CALLER];
            const string &moduleName = _req.moduleName;

            FDLOG("CBError") << caller << "|module:" << moduleName << "|object:" << _objectName << "|ret:" << ret << endl;

            reportException("CBError");

            if (param->setEnd())
            {
                ResponserPtr responser = make_responser(&Proxy::async_response_getAllMainKey, rsp);

                ret = (ret == ET_KEY_AREA_ERR || ret == ET_SERVER_TYPE_ERR) ? ET_SYS_ERR : ET_CACHE_ERR;

                doResponse(ret, caller, SUCC, responser);
            }
        }
    }

    virtual void callback_getAllMainKey_exception(int ret)
    {
        MKCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, &Proxy::async_response_getAllMainKey);
    }
};

//////////////////////////////////////////////////////////////////////////

template <typename Request>
struct MKWCacheBatchCallParam : public BatchCallParamComm
{
    MKWCacheBatchCallParam(const Request &req, const size_t count) : BatchCallParamComm(count)
    {
        _req = req;
    }

    void addResult(const map<int, int> &rspData)
    {
        TC_ThreadLock::Lock lock(_lock);

        _rsp.rspData.insert(rspData.begin(), rspData.end());
    }

    Request _req;

    MKVBatchWriteRsp _rsp;
};

template <typename Request, typename ClassName>
struct ProcMKWCacheBatchCallback : public CacheCallbackComm
{
    /**
     * 构造函数
     */
    ProcMKWCacheBatchCallback(TarsCurrentPtr &current,
                              BatchCallParamPtr &param,
                              const vector<size_t> &indexs,
                              const Request &req,
                              const string &objectName,
                              const int64_t beginTime,
                              const bool repeatFlag = false)
        : CacheCallbackComm(current, req.moduleName, objectName, beginTime, repeatFlag), _param(param), _indexs(indexs), _req(req)
    {
    }
    virtual ~ProcMKWCacheBatchCallback() {}

  protected:
    /**
         * 对回调做统一处理
         * @param ret，接口返回值
         * @param recallmf，CacheServer接口函数指针
         * @param responser，负责应答的对象
         * @return void
         */
    void procCallback(int ret, const MKVBatchWriteRsp &rsp, const int errcode, void (MKWCacheProxy::*recallmf)(MKWCachePrxCallbackPtr, const Request &, const map<string, string> &), void (*resmf)(TarsCurrentPtr, int, const MKVBatchWriteRsp &))
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        MKWCacheBatchCallParam<Request> *param = (MKWCacheBatchCallParam<Request> *)_param.get();

        if (ret == ET_SUCC)
        {
            if (!rsp.rspData.empty())
            {
                map<int, int> rspData;

                map<int, int>::const_reverse_iterator it = rsp.rspData.rbegin(), itEnd = rsp.rspData.rend();

                assert(it->first < _indexs.size());

                for (; it != itEnd; ++it)
                {
                    rspData[_indexs[it->first]] = it->second;
                }

                param->addResult(rspData);
            }
        }
        else
        {
            const string &moduleName = _req.moduleName;

            FDLOG("CBError") << caller << "|module:" << moduleName << "|object:" << _objectName << "|ret:" << ret << endl;

            map<int, int> failedIndexRet;

            if (ret == ET_PARTIAL_FAIL)
            {
                map<int, int>::const_reverse_iterator it = rsp.rspData.rbegin(), itEnd = rsp.rspData.rend();

                assert((!rsp.rspData.empty()) && (it->first < _indexs.size()));

                vector<size_t> repeatIndexs;
                for (; it != itEnd; ++it)
                {
                    if (!_repeatFlag && (it->second == ET_KEY_AREA_ERR))
                    {
                        repeatIndexs.push_back(_indexs[it->first]);
                    }
                    else
                    {
                        failedIndexRet[_indexs[it->first]] = (it->second == ET_KEY_AREA_ERR) ? errcode : it->second;
                    }
                }
                if (!repeatIndexs.empty())
                {
                    ret = doRecall(failedIndexRet, repeatIndexs, errcode, recallmf);
                    if (ret != ET_SUCC)
                    {
                        for (size_t i = 0; i < repeatIndexs.size(); ++i)
                        {
                            failedIndexRet[repeatIndexs[i]] = errcode;
                        }
                    }
                }
            }
            else if (ret == ET_SERVER_TYPE_ERR)
            {
                reportException("CBError");

                if (_repeatFlag || doRecall(failedIndexRet, _indexs, errcode, recallmf) != ET_SUCC)
                {
                    for (size_t i = 0; i < _indexs.size(); ++i)
                    {
                        failedIndexRet[_indexs[i]] = errcode;
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < _indexs.size(); ++i)
                {
                    failedIndexRet[_indexs[i]] = errcode;
                }
            }

            param->addResult(failedIndexRet);
        }

        if ((--param->_count) <= 0)
        {
            ResponserPtr responser = make_responser(resmf, param->_rsp);

            doResponse(ET_SUCC, caller, SUCC, responser);
        }
    }

    /**
         * 对异常回调做统一处理
         * @param ret，接口返回值
         * @param responser，负责应答的对象
         * @return void
         */
    void procExceptionCall(const int ret, const int errcode, void (*resmf)(TarsCurrentPtr, int, const MKVBatchWriteRsp &))
    {
        const string &caller = _current->getContext()[CONTEXT_CALLER];

        FDLOG("CBError") << caller << "|module:" << _req.moduleName << "|object:" << _objectName << "|errno:" << ret << endl;

        reportException("CBError");

        map<int, int> rspData;
        for (size_t i = 0; i < _indexs.size(); ++i)
        {
            rspData[_indexs[i]] = errcode;
        }

        MKWCacheBatchCallParam<Request> *param = (MKWCacheBatchCallParam<Request> *)_param.get();

        param->addResult(rspData);

        if ((--_param->_count) <= 0)
        {
            ResponserPtr responser = make_responser(resmf, param->_rsp);

            doResponse(ET_SUCC, caller, SUCC, responser);
        }
    }

    /**
         * 对路由失败的key重试
         */
    int doRecall(map<int, int> &failedIndexRet,
                 const vector<size_t> &repeatIndexs,
                 const int errcode,
                 void (MKWCacheProxy::*recallmf)(MKWCachePrxCallbackPtr, const Request &, const map<string, string> &))
    {
        const string &moduleName = _req.moduleName;

        RouterTableInfo *routeTableInfo = g_app._routerTableInfoFactory->getRouterTableInfo(moduleName);
        if (!routeTableInfo)
        {
            TLOG_ERROR("MKWCacheBatchCallback::doRecall do not support module:" << moduleName << endl);

            return ET_MODULE_NAME_INVALID;
        }

        int ret = routeTableInfo->update();
        if (ret != ET_SUCC)
        {
            TLOG_ERROR("MKWCacheBatchCallback::doRecall update route table failed, module:" << moduleName << "|ret:" << ret << endl);
        }
        else
        {
            RouterTable &routeTable = routeTableInfo->getRouterTable();

            map<string, Request> objectReq;
            map<string, vector<size_t>> objectDataIndexs;
            const Request &req = ((MKWCacheBatchCallParam<Request> *)(_param.get()))->_req;
            for (size_t i = 0; i < repeatIndexs.size(); ++i)
            {
                // 从路由表中获得要访问的CacheServer的节点信息
                ServerInfo serverInfo;
                ret = routeTable.getMaster(req.data[repeatIndexs[i]].mainKey, serverInfo);
                if (ret != RouterTable::RET_SUCC)
                {
                    // 如果通过key无法获取CacheServer的节点信息
                    TLOG_ERROR("MKWCacheBatchCallback::doRecall can not locate key:" << req.data[repeatIndexs[i]].mainKey << "|ret:" << ret << endl);
                    failedIndexRet[repeatIndexs[i]] = errcode;

                    continue;
                }

                const string &objectName = serverInfo.WCacheServant;
                typename map<string, Request>::iterator it = objectReq.find(objectName);
                if (it == objectReq.end())
                {
                    Request req;
                    req.moduleName = moduleName;
                    pair<typename map<string, Request>::iterator, bool> p = objectReq.insert(make_pair(objectName, req));
                    it = p.first;
                }
                it->second.data.push_back(req.data[repeatIndexs[i]]);
                objectDataIndexs[objectName].push_back(repeatIndexs[i]);
            }

            ret = ET_SUCC;

            if (!objectReq.empty())
            {
                _param->_count.fetch_add(objectReq.size());

                typename map<string, Request>::const_iterator it = objectReq.begin(), itEnd = objectReq.end();
                map<string, vector<size_t>>::const_iterator itIndex = objectDataIndexs.begin();
                for (; it != itEnd; ++it, ++itIndex)
                {
                    try
                    {
                        MKWCachePrxCallbackPtr cb = new ClassName(_current, _param, itIndex->second, it->second, it->first, _beginTime, true);
                        MKWCachePrx prxCache;
                        if (getProxy(it->first, prxCache) == ET_SUCC)
                        {
                            //重新访问CacheServer
                            (((MKWCacheProxy *)prxCache.get())->*recallmf)(cb, it->second, map<string, string>());
                            continue;
                        }
                    }
                    catch (exception &ex)
                    {
                        TLOG_ERROR("MKWCacheBatchCallback::doRecall exception:" << ex.what() << endl);
                    }
                    catch (...)
                    {
                        TLOG_ERROR("MKWCacheBatchCallback::doRecall catch unkown exception" << endl);
                    }

                    reportException("CBError");

                    const vector<size_t> &indexs = itIndex->second;
                    for (size_t i = 0; i < indexs.size(); ++i)
                    {
                        failedIndexRet[indexs[i]] = errcode;
                    }

                    --_param->_count;
                }
            }
        }

        return ret;
    }

  protected:
    BatchCallParamPtr _param;
    vector<size_t> _indexs;
    Request _req;
};

struct InsertMKVBatchCallback : public ProcMKWCacheBatchCallback<InsertMKVBatchReq, InsertMKVBatchCallback>, public MKWCachePrxCallback
{
    /**
     * 构造函数
     */
    InsertMKVBatchCallback(TarsCurrentPtr &current,
                           BatchCallParamPtr &param,
                           const vector<size_t> &indexs,
                           const InsertMKVBatchReq &req,
                           const string &objectName,
                           const int64_t beginTime,
                           const bool repeatFlag = false)
        : ProcMKWCacheBatchCallback<InsertMKVBatchReq, InsertMKVBatchCallback>(current, param, indexs, req, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~InsertMKVBatchCallback() {}

    virtual void callback_insertMKVBatch(int ret, const MKVBatchWriteRsp &rsp)
    {
        MKWCacheBatchCallbackDo(ret, rsp, ET_SYS_ERR, &MKWCacheProxy::async_insertMKVBatch, &Proxy::async_response_insertMKVBatch);
    }

    virtual void callback_insertMKVBatch_exception(int ret)
    {
        MKWCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, ET_CACHE_ERR, &Proxy::async_response_insertMKVBatch);
    }
};

struct UpdateMKVBatchCallback : public ProcMKWCacheBatchCallback<UpdateMKVBatchReq, UpdateMKVBatchCallback>, public MKWCachePrxCallback
{
    /**
         * 构造函数
         */
    UpdateMKVBatchCallback(TarsCurrentPtr &current,
                           BatchCallParamPtr &param,
                           const vector<size_t> &indexs,
                           const UpdateMKVBatchReq &req,
                           const string &objectName,
                           const int64_t beginTime,
                           const bool repeatFlag = false)
        : ProcMKWCacheBatchCallback<UpdateMKVBatchReq, UpdateMKVBatchCallback>(current, param, indexs, req, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~UpdateMKVBatchCallback() {}

    virtual void callback_updateMKVBatch(int ret, const MKVBatchWriteRsp &rsp)
    {
        MKWCacheBatchCallbackDo(ret, rsp, ET_SYS_ERR, &MKWCacheProxy::async_updateMKVBatch, &Proxy::async_response_updateMKVBatch);
    }

    virtual void callback_updateMKVBatch_exception(int ret)
    {
        MKWCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, ET_CACHE_ERR, &Proxy::async_response_updateMKVBatch);
    }
};

struct DelMKVBatchCallback : public ProcMKWCacheBatchCallback<DelMKVBatchReq, DelMKVBatchCallback>, public MKWCachePrxCallback
{
    /**
         * 构造函数
         */
    DelMKVBatchCallback(TarsCurrentPtr &current,
                        BatchCallParamPtr &param,
                        const vector<size_t> &indexs,
                        const DelMKVBatchReq &req,
                        const string &objectName,
                        const int64_t beginTime,
                        const bool repeatFlag = false)
        : ProcMKWCacheBatchCallback<DelMKVBatchReq, DelMKVBatchCallback>(current, param, indexs, req, objectName, beginTime, repeatFlag)
    {
    }
    virtual ~DelMKVBatchCallback() {}

    virtual void callback_delMKVBatch(int ret, const MKVBatchWriteRsp &rsp)
    {
        assert(ret != ET_PARTIAL_FAIL);

        MKWCacheBatchCallbackDo(ret, rsp, WRITE_ERROR, &MKWCacheProxy::async_delMKVBatch, &Proxy::async_response_delMKVBatch);
    }

    virtual void callback_delMKVBatch_exception(int ret)
    {
        MKWCacheBatchCallbackExcDo(_req.moduleName, _objectName, ret, WRITE_ERROR, &Proxy::async_response_delMKVBatch);
    }
};

#endif

