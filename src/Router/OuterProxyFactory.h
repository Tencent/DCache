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
#ifndef _OUTER_PROXY_FACTORY_H_
#define _OUTER_PROXY_FACTORY_H_

#include <sys/time.h>
#include <map>
#include "RouterServerConfig.h"
#include "global.h"
#include "util/tc_common.h"
#include "util/tc_monitor.h"

struct ProxyInfo
{
    RouterClientPrx clientPrx;
    time_t lastAccessTime;
};

class OuterProxyFactory
{
public:
    OuterProxyFactory() : _proxyMaxSilentTime(0) {}

    virtual ~OuterProxyFactory() = default;

    void init(int proxyMaxSilentTime)
    {
        _proxyMaxSilentTime = proxyMaxSilentTime;
        TC_ThreadLock::Lock lock(_threadLock);
        _proxyInfoFactory.clear();
    }

    void reloadConf(const RouterServerConfig &conf)
    {
        TLOGDEBUG("OuterProxyFactory reload config ..." << endl);
        init(conf.getProxyMaxSilentTime(1800));
    }

    // 异常上报，仅上报异常次数，可灵活配置监控报警
    virtual void reportException(const string &name)
    {
        if (_exceptionRecords.find(name) == _exceptionRecords.end())
        {
            PropertyReportPtr ptr =
                Application::getCommunicator()->getStatReport()->createPropertyReport(
                    name, PropertyReport::count());
            if (ptr)
            {
                _exceptionRecords[name] = ptr;
            }
            else
            {
                ostringstream os;
                os << "create property " << name << " failed in OuterProxyFactory";
                TARS_NOTIFY_ERROR(os.str());
                return;
            }
        }
        _exceptionRecords[name]->report(1);
    }

    virtual int getProxy(const string &stringProxy, RouterClientPrx &clientPrx)
    {
        TC_ThreadLock::Lock lock(_threadLock);
        try
        {
            if (_proxyInfoFactory.find(stringProxy) == _proxyInfoFactory.end())
            {
                _proxyInfoFactory[stringProxy].clientPrx =
                    Application::getCommunicator()->stringToProxy<RouterClientPrx>(stringProxy);
            }

            _proxyInfoFactory[stringProxy].lastAccessTime =
                TC_TimeProvider::getInstance()->getNow();
            clientPrx = _proxyInfoFactory[stringProxy].clientPrx;
        }
        catch (TarsException &ex)
        {
            DAY_ERROR << "OuterProxyFactory::getProxy of '" << stringProxy << "' catch exception "
                      << ex.what() << endl;
            TARS_NOTIFY_WARN(string("OuterProxyFactory::getProxy|") + ex.what() + " at " +
                             stringProxy);
            reportException("getProxyError");
            return -1;
        }
        catch (...)
        {
            DAY_ERROR << "OuterProxyFactory::getProxy get proxy of '" << stringProxy
                      << "' catch unkown exception" << endl;
            TARS_NOTIFY_WARN(string("OuterProxyFactory::getProxy|catch unkown exception at ") +
                             stringProxy);
            reportException("getProxyError");
            return -1;
        }
        return 0;
    }

    virtual void clearProxy()
    {
        TLOGDEBUG("before clear, the number of Proxy in ProxyFactory is: "
                  << _proxyInfoFactory.size() << endl);
        time_t now = TC_TimeProvider::getInstance()->getNow();

        map<string, ::ProxyInfo>::iterator it;
        {
            TC_ThreadLock::Lock lock(_threadLock);
            for (it = _proxyInfoFactory.begin(); it != _proxyInfoFactory.end();)
            {
                if (now - it->second.lastAccessTime >= _proxyMaxSilentTime)
                    _proxyInfoFactory.erase(it++);
                else
                    it++;
            }
        }

        TLOGDEBUG("after clear, the number of Proxy in ProxyFactory is: "
                  << _proxyInfoFactory.size() << endl);
    }

    virtual void deleteProxy(const string &stringProxy)
    {
        TC_ThreadLock::Lock lock(_threadLock);
        if (_proxyInfoFactory.find(stringProxy) != _proxyInfoFactory.end())
        {
            _proxyInfoFactory.erase(stringProxy);
        }
    }

    virtual void deleteAllProxy()
    {
        TC_ThreadLock::Lock lock(_threadLock);
        _proxyInfoFactory.clear();
    }

private:
    map<string, ::ProxyInfo> _proxyInfoFactory;        // 存放proxy信息
    map<string, PropertyReportPtr> _exceptionRecords;  // 存放异常记录信息
    TC_ThreadLock _threadLock;                         // 线程锁
    tars::Int32 _proxyMaxSilentTime;  // proxy的最大被访问时间，超过这个时间的话proxy将会被删除
};

#endif  // _OUTER_PROXY_FACTORY_H_
