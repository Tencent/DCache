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
#include "EtcdThread.h"
#include "EtcdHandle.h"
#include "RouterServer.h"

using namespace tars;

extern RouterServer g_app;

EtcdThread::~EtcdThread()
{
    if (isAlive())
    {
        terminate();
        getThreadControl().join();
    }
}

void EtcdThread::init(const RouterServerConfig &conf, std::shared_ptr<EtcdHandle> etcdHandle)
{
    try
    {
        _stop = false;
        _heartbeatTTL = conf.getETcdHeartBeatTTL(60);
        _refreshHeartbeatInterval = conf.getETcdHeartBeatTTL(60) / 6;

        _etcdHandle = etcdHandle;
        std::string obj = ServerConfig::Application + "." + ServerConfig::ServerName + ".RouterObj";
        std::string adapter = obj + "Adapter";
        TC_Endpoint ep = Application::getEpollServer()->getBindAdapter(adapter)->getEndpoint();
        _selfObj = obj + "@" + ep.toString();
    }
    catch (TC_Config_Exception &e)
    {
        cerr << "catch config exception: " << e.what() << endl;
        exit(-1);
    }
    catch (TC_ConfigNoParam_Exception &e)
    {
        cerr << "get domain error: " << e.what() << endl;
        exit(-1);
    }
    catch (TarsException &e)
    {
        cerr << "Tars exception:" << e.what() << endl;
        exit(-1);
    }
    catch (...)
    {
        cerr << "unkown error" << endl;
        exit(-1);
    }

    TLOGDEBUG("init EtcdThread succ!" << endl);
}

void EtcdThread::recoverRouterType() const
{
    // 服务启动时，通过ETCD来判断服务当前的状态。对于备机，要立刻进行一次抢主尝试。
    // 但是如果是主机挂掉后马上重启，此时key没有超时依然存活，就要恢复主机的状态。
    if (!isRouterMaster())
    {
        g_app.setRouterType(ROUTER_SLAVE);
        (void)registerMaster();
    }
    else
    {
        // 当发现自己是主机后，要马上为key保活。但可能在这个间隙被其他备机抢主成功而导致保活失败
        if (refreshHeartBeat() == 0)
        {
            g_app.setRouterType(ROUTER_MASTER);
        }
        else
        {
            g_app.setRouterType(ROUTER_SLAVE);
        }
    }
}

void EtcdThread::run()
{
    recoverRouterType();

    enum RouterType lastType = g_app.getRouterType();
    int64_t lastHeartBeat = 0;
    while (!_stop)
    {
        if (g_app.getRouterType() == ROUTER_SLAVE)
        {
            TLOGDEBUG(FILE_FUN << "I am a slave, waiting master down" << endl);
            if (lastType == ROUTER_MASTER)
            {
                TLOGDEBUG(FILE_FUN << "downgrad from master to slave");
                g_app.downgrade();
            }
            lastType = ROUTER_SLAVE;
            // 阻塞调用，等待主机挂掉。
            watchMasterDown();
            (void)registerMaster();
            continue;
        }

        assert(g_app.getRouterType() == ROUTER_MASTER);

        lastType = ROUTER_MASTER;
        TLOGDEBUG(FILE_FUN << "I am the master, refresh master key" << endl);

        time_t now = TNOW;
        if (now - lastHeartBeat >= _refreshHeartbeatInterval)
        {
            if (refreshHeartBeat() == 0)
            {
                lastHeartBeat = now;
            }
        }

        {
            TC_ThreadLock::Lock lock(*this);
            timedWait(5000);
        }
    }
}

void EtcdThread::terminate()
{
    _stop = true;
    tars::TC_ThreadLock::Lock sync(*this);
    notifyAll();
}

void EtcdThread::watchMasterDown() const
{
    if (_etcdHandle->watchRouterMaster() == 0)
    {
        TLOGDEBUG(FILE_FUN << "router master is down" << endl);
    }
}

bool EtcdThread::isRouterMaster() const
{
    try
    {
        std::string obj;
        (void)_etcdHandle->getRouterObj(&obj);
        return obj == _selfObj;
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("EtcdThread::isRouterMaster exception:" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("EtcdThread::isRouterMaster unknow exception:" << endl);
    }

    return false;
}

int EtcdThread::registerMaster() const
{
    // 抢主就是每个机器都尝试去创建同一个etcd key(该Key带有TTL)，创建成功的那个成为主机。
    if (_etcdHandle->createCAS(_heartbeatTTL, _selfObj) == 0)
    {
        g_app.setRouterType(ROUTER_MASTER);
        TLOGDEBUG(FILE_FUN << "createCAS succ，I become the master" << endl);
    }

    // 不管抢主是否成功，都要去更新masterRouterObj，用于备机向主机通信。
    std::string obj;
    if (_etcdHandle->getRouterObj(&obj) == 0)
    {
        TLOGDEBUG(FILE_FUN << "master router obj is : " << obj << endl);
        g_app.setMasterRouterObj(obj);
        return 0;
    }
    else
    {
        return -1;
    }
}

int EtcdThread::refreshHeartBeat() const
{
    // 为主机的etcd key续命
    try
    {
        if (_etcdHandle->refreshCAS(_heartbeatTTL, _selfObj) == 0)
        {
            TLOGDEBUG(FILE_FUN << "refreshCAS succ" << endl);
            return 0;
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR(FILE_FUN << "exception:" << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR(FILE_FUN << "unknow exception:" << endl);
        return -1;
    }

    return -1;
}
