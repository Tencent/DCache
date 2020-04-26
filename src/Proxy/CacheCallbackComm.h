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
#ifndef __CACHE_CALLBACK_COMM_H_
#define __CACHE_CALLBACK_COMM_H_

#include <pthread.h>

#include "util/tc_common.h"
#include "util/tc_monitor.h"
#include "util/tc_timeprovider.h"

#include "ProxyServer.h"
#include "StatThread.h"
#include "CacheProxyFactory.h"

using namespace std;
using namespace tars;
using namespace DCache;

#define SLAVE_HEAD "DCache."
#define CALLTIMEOUT TARSASYNCCALLTIMEOUT
#define CALLBACK_STATE                                 \
    using CacheCallbackComm<Request>::_current;        \
    using CacheCallbackComm<Request>::_req;            \
    using CacheCallbackComm<Request>::_objectName;     \
    using CacheCallbackComm<Request>::_beginTime;      \
    using CacheCallbackComm<Request>::_repeatFlag;     \
    using CacheCallbackComm<Request>::reportException; \
    using CacheCallbackComm<Request>::getWCacheProxy;  \
    using CacheCallbackComm<Request>::getCacheProxy;   \
    using CacheCallbackComm<Request>::getProxy;        \
    using CacheCallbackComm<Request>::doResponse;
#define LOG_MEM_FUN(class_name) #class_name << "::" << __FUNCTION__

//////////////////////////////////////////////////////////////////////////

struct Responser : TC_HandleBase
{
    virtual void response(TarsCurrentPtr current, int ret) = 0;

    virtual ~Responser(){};
};

typedef TC_AutoPtr<Responser> ResponserPtr;

template <typename FUN>
class Return : public Responser
{
    FUN _resfun;

  public:
    Return(FUN fun) : _resfun(fun) {}
    virtual void response(TarsCurrentPtr current, int ret) { _resfun(current, ret); }
};

template <typename FUN>
inline ResponserPtr make_responser(FUN fun)
{
    return new Return<FUN>(fun);
}

template <typename FUN, typename Response>
class ReturnWithRsp : public Responser
{
  public:
    ReturnWithRsp(FUN fun, const Response &rsp) : _resfun(fun), _rsp(rsp) {}

    virtual void response(TarsCurrentPtr current, int ret)
    {
        _resfun(current, ret, _rsp);
    }

  private:
    FUN _resfun;
    const Response &_rsp;
};

template <typename FUN, typename Response>
inline ResponserPtr make_responser(FUN fun, const Response &rsp)
{
    return new ReturnWithRsp<FUN, Response>(fun, rsp);
}

//////////////////////////////////////////////////////////////////////////

struct BatchCallParamComm : public TC_HandleBase
{
    BatchCallParamComm(size_t count);

    std::atomic<int> _count; // 批量访问分路由后的请求计数

    TC_ThreadLock _lock;
};

typedef TC_AutoPtr<BatchCallParamComm> BatchCallParamPtr;

//////////////////////////////////////////////////////////////////////////

struct ThreadData //线程私有数据，包括_cacheProxyFactory、_statDataNode和_exPropReport
{
    ThreadData(const CommunicatorPtr &communicator, RouterTableInfoFactory *routeTableFactory)
        : _cacheProxyFactory(communicator, routeTableFactory)
    {
        _statDataNode = new StatDataNode();
    }

    CacheProxyFactory _cacheProxyFactory; // 这个为什么要成为线程私有对象，难道不是线程安全的？

    StatDataNodePtr _statDataNode;

    map<string, PropertyReportPtr> _exPropReport; // 这应该是个静态变量(原版本是个静态变量)
};

//////////////////////////////////////////////////////////////////////////

class ThreadKey
{
  protected:
    static void destructor(void *p);

    class KeyInitialize
    {
      public:
        KeyInitialize()
        {
            int ret = pthread_key_create(&ThreadKey::_threadKey, ThreadKey::destructor);
            if (ret != 0)
            {
                throw TarsException("[ThreadKey::KeyInitialize] pthread_key_create error", ret);
            }
        }

        ~KeyInitialize()
        {
            pthread_key_delete(ThreadKey::_threadKey);
        }
    };

    static KeyInitialize _threadKeyInitialize;

    static pthread_key_t _threadKey;
};

//////////////////////////////////////////////////////////////////////////
class CacheCallbackComm : public ThreadKey
{
  public:
    CacheCallbackComm(TarsCurrentPtr &current,
                      const string &moduleName,
                      const string &objectName,
                      const int64_t beginTime,
                      const bool repeatFlag)
    {
        _current = current;
        _moduleName = moduleName;
        _objectName = objectName;
        _beginTime = beginTime;
        _repeatFlag = repeatFlag;
    }

    virtual ~CacheCallbackComm() {}

  protected:
    ThreadData *getThreadData() // 获取线程私有数据
    {
        ThreadData *ptr = (ThreadData *)pthread_getspecific(_threadKey);

        if (!ptr)
        {
            ptr = new ThreadData(Application::getCommunicator(), g_app._routerTableInfoFactory);

            pthread_setspecific(_threadKey, ptr);

            g_app.getStatThread()->addNode(ptr->_statDataNode);
        }

        return ptr;
    }

    void doStatReport(const string &interfaceName,
                      const StatType type,
                      const int retValue)
    {
        ThreadData *data = getThreadData();

        if (data)
        {

//            const string &masterName = _current->getContext()[ServantProxy::TARS_MASTER_KEY];
            auto it = _current->getRequestStatus().find("DCACHE_MASTER_NAME");            
	    if(it == _current->getRequestStatus().end()) 
	        return;
            string masterName = it->second;

            // Proxy的stat上报主要是用来统计访问某个模块的tars服务信息的，返回值默认填0
            data->_statDataNode->report(masterName,
                                        _current->getIp(),
                                        SLAVE_HEAD + _moduleName,
                                        "127.0.0.1",
                                        0,
                                        interfaceName,
                                        type,
                                        TNOWMS - _beginTime,
                                        0);
        }
    }

    void reportException(const string &name)
    {
        ThreadData *data = getThreadData();

        if (data)
        {
            map<string, PropertyReportPtr>::iterator it = data->_exPropReport.find(name);
            if (it == data->_exPropReport.end())
            {
                try
                {
                    PropertyReportPtr ptr = Application::getCommunicator()->getStatReport()->createPropertyReport(name,
                                                                                                                  PropertyReport::count());

                    if (ptr)
                    {
                        pair<map<string, PropertyReportPtr>::iterator, bool> p =
                            data->_exPropReport.insert(pair<string, PropertyReportPtr>(name, ptr));

                        it = p.first;
                    }
                    else
                    {
                        string err = "ERROR: failed to create property[" + name + "]";
                        TARS_NOTIFY_ERROR(err);
                        TLOGERROR("CacheCallbackComm::reportException, " << err << endl);

                        return;
                    }
                }
                catch (const exception &ex)
                {
                    TLOGERROR("CacheCallbackComm::reportException exception:" << ex.what() << endl);

                    return;
                }
            }

            it->second->report(1);
        }
    }

    template <class T>
    int getCacheProxy(const string &moduleName,
                      const string &key,
                      string &objectName,
                      T &prxCache,
                      const string &idcArea)
    {
        ThreadData *data = getThreadData();

        if (data)
        {
            return data->_cacheProxyFactory.getCacheProxy(moduleName, key, objectName, prxCache, idcArea, true);
        }

        return ET_SYS_ERR;
    }

    template <class T>
    int getWCacheProxy(const string &moduleName,
                       const string &key,
                       string &objectName,
                       T &prxCache)
    {
        ThreadData *data = getThreadData();

        if (data)
        {
            return data->_cacheProxyFactory.getWCacheProxy(moduleName, key, objectName, prxCache, true);
        }

        return ET_SYS_ERR;
    }

    template <class T>
    int getProxy(const string &objectName, T &prx)
    {
        ThreadData *data = getThreadData();

        if (data)
        {
            prx = data->_cacheProxyFactory.getProxy<T>(objectName);
            return ET_SUCC;
        }

        return ET_SYS_ERR;
    }

    /**
     * 对请求做统一应答
     * @param ret，接口返回值
     * @param caller，调用的接口
     * @param statType, 接口调用统计上报类型
     * @param responser，负责应答的对象
     * @return void
     */
    void doResponse(const int ret, const string &caller, const StatType statType, const ResponserPtr responser)
    {
        try
        {
            responser->response(_current, ret);

            if (g_app.needStatReport())
            {
                doStatReport(caller, statType, ret);
            }
        }
        catch (const exception &ex)
        {
            TLOGERROR("CacheCallbackComm::doResponse exception:" << ex.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("CacheCallbackComm::doResponse catch unkown exception" << endl);
        }
    }

  protected:
    TarsCurrentPtr _current;

    string _moduleName; // 客户端请求结构体

    string _objectName; // 访问CacheServer的obj名

    bool _repeatFlag; // 重复访问CacheServer的标志

    int64_t _beginTime; // 发起异步调用时间
};

#endif

