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
#ifndef _SYNCTHREAD_H
#define _SYNCTHREAD_H

#include <iostream>
#include "servant/Application.h"
#include "MKCache.h"
#include "MKCacheGlobe.h"

/**
 * 定时回写脏数据和同步回写时间到备机
 */
class SyncThread
{
public:
    SyncThread() :_isStart(false), _isRuning(false) {}
    ~SyncThread() {}

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

    /*
    *校正回写数据指针
    */
    void sync();

    /*
    *回写脏数据
    */
    void syncData(time_t t);

    void setStart(bool bStart)
    {
        _isStart = bStart;
    }

    /*
    *停止线程
    */
    void stop()
    {
        _isStart = false;
    }

    bool isStart()
    {
        return _isStart;
    }

    bool isRuning()
    {
        return _isRuning;
    }

    void setRuning(bool bRuning)
    {
        _isRuning = bRuning;
    }

    int getThreadNum()
    {
        return _syncThreadNum;
    }

    int getSyncTime()
    {
        return _syncTime;
    }
protected:
    string getBakSourceAddr();

protected:
    //线程是否已启动
    bool _isStart;

    //线程当前状态
    bool _isRuning;

    TC_Config _tcConf;
    string _config;

    //每次回写脏数据的时间间隔（秒）
    int _syncInterval;

    //主机回写脏数据时间点
    int _syncTime;

    //回写线程数
    int _syncThreadNum;

    //回写频率
    size_t _syncSpeed;

    //每秒回写数据量
    size_t _syncCount;

    //屏蔽回写的时间间隔
    vector<pair<time_t, time_t> > _blockTime;

    //解除屏蔽的脏数据比率
    unsigned int _unBlockPercent;
};

#endif
