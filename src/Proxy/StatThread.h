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
#ifndef __STAT_THREAD_H_
#define __STAT_THREAD_H_

#include "servant/StatF.h"
#include "servant/StatReport.h"
#include "servant/Application.h"

using namespace tars;

typedef map<StatMicMsgHead, StatMicMsgBody> StatMicMsg;

enum StatType
{
    SUCC = 0,
    TIME_OUT,
    EXCE,
};

struct StatDataNode : public TC_HandleBase
{
    TC_ThreadLock _lock;

    StatMicMsg _statMsg;

    void report(const string &masterName,
                const string &masterIp,
                const string &slaveName,
                const string &slaveIp,
                uint16_t slavePort,
                const string &interfaceName,
                StatType statType,
                int rspTime,
                int retValue);
};
typedef TC_AutoPtr<StatDataNode> StatDataNodePtr;

class StatThread : public TC_Thread, public TC_ThreadLock
{

  public:
    /**
     * 构造函数
     */
    StatThread();

    /**
     * 析构函数
     */
    ~StatThread();

    /**
     * 轮询函数
     */
    virtual void run();

    /**
     * 结束线程
     */
    void terminate();

    /**
     * 初始化
     * @param statPrx, 模块间调用服务器地址
     * @param reportInterval, 上报间隔单位秒
     */
    void setReportInfo(const StatFPrx &statPrx, int reportInterval = 60);

    void init(const string &sStatObj, const int &interval);

    /**
     * 添加StatDataNode
     */
    void addNode(StatDataNodePtr node)
    {
        Lock lock(*this);

        _nodeList.push_back(node);
    }

  private:
    void reportStatData();

  private:
    list<StatDataNodePtr> _nodeList;

  private:
    time_t _time;

    int _reportInterval; // stat线程上报时间间隔，单位秒

    bool _terminate; // 线程结束标志

    StatFPrx _statPrx;
};

#endif
