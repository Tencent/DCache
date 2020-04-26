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
#ifndef __CACHEINFO_UPDATE_THREAD_H_
#define __CACHEINFO_UPDATE_THREAD_H_

#include <iostream>
#include "util/tc_thread.h"
#include "util/tc_common.h"
#include "servant/Application.h"

using namespace tars;

/**
 * 用于执行定时更新Cache服务信息的线程类
 */
class CacheInfoUpdateThread : public TC_Thread, public TC_ThreadLock
{
public:

    CacheInfoUpdateThread(int iReloadInterval)
    : _terminate(false), _reloadInterval(iReloadInterval)
    {}

    ~CacheInfoUpdateThread();

    /**
     * 结束线程
     */
    void terminate();

    /**
     * 轮询函数
     */
    virtual void run();

private:
    /**
     * 结束线程标识
     */
    bool _terminate;
    
    /**
     * 重新加载Cache服务信息的时间间隔，单位秒
     */
    int _reloadInterval;
};

#endif

