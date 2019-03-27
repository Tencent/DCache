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
#include "EtcdHost.h"
#include <stdlib.h>
#include "EtcdHttp.h"
#include "global.h"
#include "util/tc_clientsocket.h"
#include "util/tc_timeprovider.h"

#define TEST_KEY string("healthcheck/key_" + ServerConfig::LocalIp + "_" + ServerConfig::ServerName)
#define TEST_VALUE_PREFIX string("healthcheckvalue")

// EtcdHost线程循环执行的时间间隔(单位:毫秒)
const int REPEAT_INTERVAL = 1000;

// 测试Key的超时时间(单位:秒)
const int TEST_KEY_TTL = 100;

tars::TC_ThreadRWLocker EtcdHost::_activeHostsLock;

using namespace tars;

EtcdHost::~EtcdHost()
{
    _terminate = true;

    if (isAlive())
    {
        getThreadControl().join();
    }

    TC_ThreadLock::Lock lock(*this);
    notifyAll();
}

int EtcdHost::init(const RouterServerConfig &conf)
{
    try
    {
        _appName = conf.getAppName("");
        if (_appName == "")
        {
            TLOGERROR(FILE_FUN << "read AppName config error!" << endl);
            return -1;
        }
        std::string defaultHost = "127.0.0.1:2379";
        std::vector<std::string> hosts = conf.getEtcdHosts(defaultHost);
        if (loadHostsInfo(hosts) != 0)
        {
            TLOGERROR(FILE_FUN << "load etcd Host Info error!" << endl);
            return -1;
        }
        checkHostHealth();
        if (_activeHosts.empty())
        {
            TLOGERROR(FILE_FUN << "All etcd host have been dead" << endl);
            return -1;
        }
        TLOGDEBUG(FILE_FUN << "EtcdHost initiliaze succ" << endl);
        return 0;
    }
    catch (TC_Exception &ex)
    {
        TLOGERROR(FILE_FUN << "get etcd host error: " << ex.what() << endl);
        return -1;
    }
}

void EtcdHost::terminate()
{
    _terminate = true;
    TC_ThreadLock::Lock sync(*this);
    notifyAll();
}

std::string EtcdHost::choseHost()
{
    tars::TC_ThreadRLock readLock(_activeHostsLock);
    return _activeHosts[_count++ % _activeHosts.size()];
}

bool EtcdHost::checkHostWritable(const std::string &url,
                                 const std::string &key,
                                 const std::string &val)
{
    try
    {
        auto req = makeEtcdHttpRequest(url, HTTP_PUT);
        req->setDefaultHeader();
        req->setKey(key);
        req->setKeyTTL(TEST_KEY_TTL);
        req->setValue(val);

        std::string resp;
        int rc = req->doRequest(DEFAULT_HTTP_REQUEST_TIMEOUT, &resp);
        if (rc != 0)
        {
            TLOGERROR(FILE_FUN << "etcd request " << req->dumpURL() << " | "
                               << "ret code = " << rc << endl);
            return false;
        }
        TLOGDEBUG(FILE_FUN << "etcd response " << resp << endl);

        return true;
    }
    catch (const std::exception &ex)
    {
        TLOGERROR(FILE_FUN << " exception:" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR(FILE_FUN << " unknow exception:" << endl);
    }

    return false;
}

bool EtcdHost::checkHostReadable(const std::string &url,
                                 const std::string &key,
                                 const std::string &val,
                                 bool checkValue)
{
    try
    {
        auto req = makeEtcdHttpRequest(url, HTTP_GET);
        req->setKey(key);

        std::string resp;
        if (req->doRequest(DEFAULT_HTTP_REQUEST_TIMEOUT, &resp) != TC_ClientSocket::EM_SUCCESS)
        {
            TLOGERROR(FILE_FUN << "etcd request " << req->dumpURL() << "| FAILED" << endl);
            return false;
        }

        EtcdHttpParser parser;
        if (parser.parseContent(resp) != 0)
        {
            TLOGERROR(FILE_FUN << "parse http resp error: " << resp << endl);
            return false;
        }

        int errCode;
        std::string errMsg;
        if (parser.getEtcdError(&errCode, &errMsg) != 0)
        {
            TLOGERROR(FILE_FUN << "etcd error. errCode : " << errCode << " errMsg : " << errMsg);
            return false;
        }

        std::string v;
        if (parser.getCurrentNodeValue(&v) != 0)
        {
            TLOGERROR(FILE_FUN << "get etcd value error" << endl);
            return false;
        }

        if (checkValue && v != val)
        {
            return false;
        }

        // 如果不需要检查查询到的value，就返回true
        return true;
    }
    catch (const std::exception &ex)
    {
        TLOGERROR(FILE_FUN << " exception:" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR(FILE_FUN << " unknow exception:" << endl);
    }

    return false;
}

std::string EtcdHost::createRandomTestValue() const
{
    return std::string(TEST_VALUE_PREFIX + "_" + TC_Common::tostr(rand() % TNOWMS));
}

std::string EtcdHost::getRouterHost()
{
    std::string s;
    {
        TC_ThreadRLock readLock(_activeHostsLock);
        s = _activeHosts[_count++ % _activeHosts.size()];
    }
    return s + "/" + _appName + "/router/router_endpoint";
}

void EtcdHost::timeWait()
{
    TC_ThreadLock::Lock lock(*this);
    timedWait(REPEAT_INTERVAL);
}

void EtcdHost::run()
{
    while (!_terminate)
    {
        timeWait();
        try
        {
            checkHostHealth();
        }
        catch (const std::exception &ex)
        {
            TLOGERROR(FILE_FUN << " exception:" << ex.what() << endl);
        }
        catch (...)
        {
            TLOGERROR(FILE_FUN << " unknow exception:" << endl);
        }
    }
}

void EtcdHost::checkHostHealth()
{
    ::srand(TNOWMS);

    HostMap::const_iterator it = _configHosts.begin();
    std::vector<std::string> activeHostsURL;

    for (; it != _configHosts.end(); ++it)
    {
        try
        {
            const HostInfo &host = it->second;
            bool writeHealth = false, readHealth = false;
            std::string testVal = createRandomTestValue();

            writeHealth = checkHostWritable(host._url, TEST_KEY, testVal);

            // 读写ETCD之间间隔1s
            ::sleep(1);

            readHealth = checkHostReadable(host._url, TEST_KEY, testVal, writeHealth);

            if (writeHealth || readHealth)
            {
                activeHostsURL.push_back(host._url);
            }

            if (!writeHealth || !readHealth)
            {
                std::string s = readHealth ? "readonly" : "dead";
                std::string errMsg = "etcd host " + host._ip + ":" + host._port + " is " + s;
                TLOGERROR(FILE_FUN << host._url << "|" << errMsg << endl);
            }
        }
        catch (const std::exception &ex)
        {
            TLOGERROR(FILE_FUN << " exception:" << ex.what() << endl);
        }
        catch (...)
        {
            TLOGERROR(FILE_FUN << " unknow exception:" << endl);
        }
    }

    if (activeHostsURL.empty())
    {
        TLOGERROR(FILE_FUN << "All etcd host have been dead" << endl);
    }
    else
    {
        TC_ThreadWLock writeLock(_activeHostsLock);
        _activeHosts.swap(activeHostsURL);
    }
}

int EtcdHost::loadHostsInfo(const std::vector<std::string> &hosts)
{
    if (hosts.empty())
    {
        TLOGERROR(FILE_FUN << "load etcd Host Info error, has no etcd host!" << endl);
        return -1;
    }

    std::vector<std::string>::const_iterator it = hosts.begin();
    for (; it != hosts.end(); ++it)
    {
        std::string etcdHostUrl = "http://" + *it + "/v2/keys/";

        TC_URL url;
        if (!url.parseURL(etcdHostUrl))
        {
            TLOGERROR(FILE_FUN << "get etcd host error: " << etcdHostUrl << endl);
            return -1;
        }

        std::string addr = url.getDomain() + ":" + url.getPort();
        HostInfo h(url.getDomain(), url.getPort());
        _configHosts[addr] = h;
        TLOGDEBUG(FILE_FUN << "get a ETCD host. URL : " << h._url << "|IP : " << h._ip << ":"
                           << h._port << endl);
    }

    assert(!_configHosts.empty());
    return 0;
}
