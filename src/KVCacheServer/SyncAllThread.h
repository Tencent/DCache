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
#ifndef _SYNCALLTHREAD_H
#define _SYNCALLTHREAD_H

#include <iostream>
#include "servant/Application.h"
#include "Cache.h"
#include "CacheGlobe.h"

/**
 * 回写所有脏数据
 */
class SyncAllThread
{
public:
    SyncAllThread() :_isStart(false), _isRuning(false) {}
    ~SyncAllThread() {}

    /*
    *初始化
    */
    void init(const string &sConf);

    /*
    *重载配置
    */
    void reload();

    /*
    *生成线程
    */
    void createThread();

    /*
    *线程run
    */
    static void* Run(void* arg);

    void setStart(bool bStart) {
        _isStart = bStart;
    }

    /*
    *停止线程
    */
    void stop() {
        _isStart = false;
    }

    bool isStart() {
        return _isStart;
    }

    bool isRuning() {
        return _isRuning;
    }

    void setRuning(bool bRuning) {
        _isRuning = bRuning;
    }

    int getSyncTime() {
        return _syncTime;
    }
protected:
    /*
    *校正回写数据指针
    */
    void sync();

    /*
    *回写所有脏数据
    */
    int syncData(time_t t);

protected:
    //线程启动停止标志
    bool _isStart;
    //线程当前状态
    bool _isRuning;
    TC_Config _tcConf;
    string _config;
    //回写脏数据时间（秒）
    int _syncTime;
};

#endif
