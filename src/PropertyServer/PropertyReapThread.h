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
#ifndef __REAP_THREAD_H_
#define __REAP_THREAD_H_

#include <iostream>
#include "util/tc_thread.h"
#include "util/tc_common.h"
#include "servant/Application.h"
#include "Property.h"
#include "PropertyHashMap.h"

using namespace tars;
using namespace DCache;

/**
 * 用于定时将数据落到数据库的线程类
 */
class PropertyReapThread : public TC_Thread, public TC_ThreadLock
{
public:
    /**
     * 定义常量，轮询间隔时间
     */
    enum
    {
        REAP_INTERVAL = 10000
    };

    /**
     * 构造
     */
    PropertyReapThread()
    : _terminate(false)
    {}

    /**
     * 析够
     */
    ~PropertyReapThread();

    /**
     * 结束线程
     */
    void terminate();

    /**
     * 轮询函数
     */
    virtual void run();

private:

    void getPropertyMsg(const string &sCloneFile, PropertyMsg &mPropMsg);

    void sendAlarmSMS(const string &sMsg);

private:
    /**
     * 结束线程标识
     */
    bool _terminate;
};

#endif
