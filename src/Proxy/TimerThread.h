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
#ifndef _TIMERTHREAD_H_
#define _TIMERTHREAD_H_

#include <sys/time.h>

#include "util/tc_thread.h"
#include "util/tc_config.h"

using namespace tars;

class TimerThread : public TC_Thread
{
  public:
    TimerThread(const time_t &tTime);

    virtual ~TimerThread();

    void init(TC_Config &conf);

    void reloadConf(TC_Config &conf);

    string showStatus();

    virtual void run();

    void stop();

    // in order to do unit test, add interfaces as follows:
    bool checkStop() { return _stop; }
    
    int getIntervalToSyncRouterTable() { return _intervalToSyncRouterTable; }

    time_t getLastSynRouterTime() { return _lastSynRouterTime; }

    int getIntervalToSyncRouterTableFactory() { return _intervalToSyncRouterTableFactory; }

    time_t getLastSynRouterTableFactoryTime() { return _lastSynRouterTableFactoryTime; }

  protected:
    void readConf(TC_Config &conf);

    // 从路由服务同步当前所支持模块的路由表数据
    void syncRouterTable();

    // 从路由服务同步路由表工厂(因为新模块上线或者有模块卸载)
    void syncRouterTableFactory();

    void updateProxyFactory();

  private:
    volatile bool _stop;

    int _intervalToSyncRouterTable; // 同步路由表时间间隔

    time_t _lastSynRouterTime; //上次同步路由表时间

    int _intervalToSyncRouterTableFactory; // 同步模块变化的时间间隔

    time_t _lastSynRouterTableFactoryTime; //上次同步模块变化的时间
    
};

#endif
