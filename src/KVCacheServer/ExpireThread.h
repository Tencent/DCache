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
#ifndef _EXPIRETHREAD_H_
#define _EXPIRETHREAD_H_

#include <iostream>
#include "servant/Application.h"
#include "CacheGlobe.h"

/**
 * 定时作业线程类，清除过期数据
 */
class ExpireThread : public TC_Singleton<ExpireThread>
{
public:
    ExpireThread() :_isStart(false), _isRuning(false) {}
    ~ExpireThread() {}

    /*
    *线程run
    */
    static void* Run(void* arg);

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
    tars::Int32 createThread();

    /*
    *清除数据
    */
    void eraseData();

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
protected:
    enum ServerType getType();

protected:
    //线程启动停止标志
    bool _isStart;
    //线程当前状态
    bool _isRuning;
    TC_Config _tcConf;
    string _config;
    //超时处理的时间间隔
    int _expireInterval;
    bool _delDB;
    bool _existDB;
    //清除过期数据频率
    size_t _expireSpeed;
};

#endif
