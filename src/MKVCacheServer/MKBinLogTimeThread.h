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
#ifndef _MKBINLOG_TIME_THREAD_H_
#define _MKBINLOG_TIME_THREAD_H_

#include <iostream>
#include "servant/Application.h"
#include "MKBinLog.h"
#include "MKCacheGlobe.h"

using namespace std;
using namespace tars;
using namespace DCache;

class MKBinLogTimeThread
{
public:

    MKBinLogTimeThread() :_isStart(false), _isRuning(false), _isInSlaveCreating(false), _failCount(0), _saveSyncTimeInterval(30) {}

    virtual ~MKBinLogTimeThread() {}

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
    *更新备份源的binlog最新记录时间
    */
    static void UpdateLastBinLogTime(uint32_t tLast, uint32_t tSync = 0);

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

    bool isInSlaveCreatingStatus()
    {
        return _isInSlaveCreating;
    }

    void getSyncTime();

    void saveSyncTime();

public:

    string getBakSourceAddr();

    void reportBinLogDiff();

private:

    //线程启动停止标志
    bool _isStart;

    //线程当前状态
    bool _isRuning;

    //线程是否处于备件数据自建状态
    bool _isInSlaveCreating;

    TC_Config _tcConf;
    string _config;

    static TC_ThreadLock _lock;

    MKBinLogPrx _binLogPrx;

    int _failCount;

    int _saveSyncTimeInterval;

    PropertyReportPtr _srp_binlogSynDiff;	//上报binlog的同步差异
};

#endif
