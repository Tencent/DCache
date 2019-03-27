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
#ifndef __ETCDTHREAD_H_
#define __ETCDTHREAD_H_

#include <atomic>
#include <memory>
#include "RouterServerConfig.h"
#include "util/tc_monitor.h"
#include "util/tc_thread.h"

class EtcdHandle;

class EtcdThread : public tars::TC_Thread, public tars::TC_ThreadLock
{
public:
    EtcdThread()
        : _stop(false), _regMasterInterval(0), _refreshHeartbeatInterval(0), _heartbeatTTL(0)
    {
    }

    ~EtcdThread();

    void init(const RouterServerConfig &conf, std::shared_ptr<EtcdHandle> etcdHandle);

    virtual void run();

    void terminate();

private:
    // 判断当前主机是否是Master
    bool isRouterMaster() const;

    // 抢主。只有在备机才会执行
    int registerMaster() const;

    // 刷新心跳。只有在主机上才会执行。
    int refreshHeartBeat() const;

    // 阻塞调用，用于备机上，来监听主机是否挂掉。
    void watchMasterDown() const;

    // 尝试恢复服务的状态
    void recoverRouterType() const;

private:
    std::atomic<bool> _stop;        // 标记线程是否停止
    int _regMasterInterval;         // 抢主的时间间隔(单位:秒)
    int _refreshHeartbeatInterval;  // 刷新心跳的时间间隔(单位:秒)
    int _heartbeatTTL;              // 心跳的超时时间(即TTL, 单位:秒)
    std::string _selfObj;           //
    std::shared_ptr<EtcdHandle> _etcdHandle;
};

#endif  // __ETCDTHREAD_H_
