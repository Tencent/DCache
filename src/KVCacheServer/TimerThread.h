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
#ifndef _TIMERTHREAD_H
#define _TIMERTHREAD_H

#include <iostream>
#include "servant/Application.h"
#include "Router.h"
#include "CacheGlobe.h"
/**
 * 定时作业线程类，淘汰数据、同步路由等等
 */
class TimerThread
{
public:
    TimerThread() :_isStart(false), _isRuning(false), _isInSlaveCreating(false) {}
    ~TimerThread() {}

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
    *同步路由
    */
    void syncRoute();

    /*
    *get脏数据量
    */
    size_t getDirtyNum();

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

    bool isInSlaveCreatingStatus() {
        return _isInSlaveCreating;
    }
protected:
    enum ServerType getType();
    void handleConnectHbTimeout();

protected:
    //线程启动停止标志
    bool _isStart;

    //线程当前状态
    bool _isRuning;

    //线程是否处于备件数据自建状态
    bool _isInSlaveCreating;

    TC_Config _tcConf;
    string _config;

    //同步路由的时间间隔
    int _syncRouteInterval;
    //binlog心跳时间间隔
    int _binlogHeartbeatInterval;
    string _moduleName;

    string _binlogFile;

    string _keyBinlogFile;
    bool _recordBinLog;
    bool _recordKeyBinLog;

    unsigned int _shmNum;

    PropertyReportPtr _srp_dirtyCnt;
    PropertyReportPtr _srp_hitcount;
    PropertyReportPtr _srp_memSize;        //上报总内存大小
    PropertyReportPtr _srp_memInUse;	    //上报数据内存区的使用率
    PropertyReportPtr _srp_dirtyRatio;		//上报脏数据比率
    PropertyReportPtr _srp_chunksOnceEle;	//上报已使用chunk与总元素比率
    PropertyReportPtr _srp_elementCount;	//上报元素总数
    PropertyReportPtr _srp_onlykeyCount;   //上报onlykey个数
    PropertyReportPtr _srp_maxJmemUsage;       //上报Jmem中最大的内存使用率

    int _downgradeTimeout;	//主机自动降级
};

//用于定时生成binlog文件
class CreateBinlogFileThread
{
public:
    CreateBinlogFileThread() :_stop(false) {};

    /*
    *生成线程
    */
    void createThread();

    /*
    *线程run
    */
    static void* Run(void* arg);

    void stop() {
        _stop = true;
    }

private:
    bool _stop;
};

//用于定时上报心跳
class HeartBeatThread
{
public:
    HeartBeatThread() :_stop(false), _enableConHb(false) {};
    /*
    *生成线程
    */
    void createThread();

    /*
    *线程run
    */
    static void* Run(void* arg);

    void stop() {
        _stop = true;
    }

    void enableConHb(bool enable) {
        _enableConHb = enable;
    }
private:
    bool _stop;
    bool _enableConHb;
};

#endif
