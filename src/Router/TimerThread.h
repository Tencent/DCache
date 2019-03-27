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
#include <atomic>
#include <string>
#include <vector>
#include "OuterProxyFactory.h"
#include "RouterServerConfig.h"
#include "Transfer.h"
#include "global.h"
#include "util/tc_thread.h"
#include "util/tc_thread_pool.h"

using namespace tars;
using namespace std;

class DbHandle;

class TimerThread : public TC_Thread, public TC_ThreadLock
{
public:
    TimerThread();

    virtual ~TimerThread() = default;

    void init(const RouterServerConfig &conf,
              std::shared_ptr<DbHandle> dbHandle,
              std::shared_ptr<OuterProxyFactory> outerProxy);

    virtual void run();

    void terminate();

    // Router主机由MASTER降级为SLAVE时调用，做一些清理任务
    void downgrade();

protected:
    // 清理超时代理缓存
    void doClearProxy();

    // 做组并行的迁移任务
    void doTransferParallel();

private:
    std::atomic<bool> _stop;
    int _transferInterval;                // 轮询迁移数据库的时间
    int _clearProxyInterval;              // 清理代理的间隔时间
    int _threadPollSize;                  // 线程池大小
    int _minSizeEachGroup;                // 每个组的最小迁移线程数
    int _maxSizeEachGroup;                // 每个组的最大迁移线程数
    time_t _lastTransferTime;             // 最近轮询迁移数据库的时间
    time_t _lastClearProxyTime;           // 最近清理代理的间隔时间
    std::shared_ptr<DbHandle> _dbHandle;  // 数据库操作句柄
    std::shared_ptr<OuterProxyFactory> _outerProxy;
    std::shared_ptr<TransferDispatcher> _transferDispatcher;
};

#endif
