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
#include "StatThread.h"

void StatDataNode::report(const string &masterName,
                          const string &masterIp,
                          const string &slaveName,
                          const string &slaveIp,
                          uint16_t slavePort,
                          const string &interfaceName,
                          StatType statType,
                          int rspTime,
                          int retValue)
{
    StatMicMsgHead head;
    StatMicMsgBody body;

    // 包头信息
    head.masterName = masterName;
    head.masterIp = masterIp;
    head.slaveName = slaveName;
    head.slaveIp = slaveIp;
    head.interfaceName = interfaceName;
    head.slavePort = slavePort;
    head.returnValue = retValue;

    // 包体信息
    if (statType == SUCC)
    {
        body.count = 1;

        body.totalRspTime = body.minRspTime = body.maxRspTime = rspTime;
    }
    else if (statType == TIME_OUT)
    {
        body.timeoutCount = 1;
    }
    else
    {
        body.execCount = 1;
    }

    TC_ThreadLock::Lock lock(_lock);

    StatMicMsg::iterator it = _statMsg.find(head);

    if (it != _statMsg.end())
    {
        StatMicMsgBody &oldBody = it->second;
        oldBody.count += body.count;
        oldBody.timeoutCount += body.timeoutCount;
        oldBody.execCount += body.execCount;
        oldBody.totalRspTime += body.totalRspTime;
        if (oldBody.maxRspTime < body.maxRspTime)
        {
            oldBody.maxRspTime = body.maxRspTime;
        }
        // 非0最小值
        if (oldBody.minRspTime == 0 || (oldBody.minRspTime > body.minRspTime && body.minRspTime != 0))
        {
            oldBody.minRspTime = body.minRspTime;
        }
    }
    else
    {
        _statMsg[head] = body;
    }
}

//////////////////////////////////////////////////////////////////////////

StatThread::StatThread() : _time(TNOW), _reportInterval(60), _terminate(false) {}

StatThread::~StatThread()
{
    if (isAlive())
    {
        terminate();

        getThreadControl().join();
    }
}

void StatThread::init(const string &sStatObj, const int &interval)
{
    _reportInterval = interval;
    _statPrx = Application::getCommunicator()->stringToProxy<StatFPrx>(sStatObj);
}

void StatThread::terminate()
{
    Lock lock(*this);

    _terminate = true;

    notifyAll();
}

void StatThread::setReportInfo(const StatFPrx &statPrx, int reportInterval)
{
    Lock lock(*this);

    _statPrx = statPrx;

    _time = TNOW;

    _reportInterval = reportInterval < 10000 ? 10000 : reportInterval;

    if (!isAlive())
    {
        start();
    }
}

void StatThread::run()
{
    while (!_terminate)
    {
        try
        {
            time_t now = TNOW;

            if (now - _time > _reportInterval)
            {
                reportStatData();

                _time = now;
            }

            TC_ThreadLock::Lock lock(*this);

            timedWait(1000);
        }
        catch (exception &ex)
        {
            TLOG_ERROR("StatThread::run exception:" << ex.what() << endl);
        }
        catch (...)
        {
            TLOG_ERROR("StatThread::run catch unkown exception" << endl);
        }
    }
}

void StatThread::reportStatData()
{
    TLOG_DEBUG("StatThread::reportStatData begin..." << endl);

    Lock lock(*this);

    list<StatDataNodePtr>::iterator it = _nodeList.begin();
    list<StatDataNodePtr>::iterator itEnd = _nodeList.end();
    for (; it != itEnd; ++it)
    {
        StatMicMsg statMsg;
        statMsg.clear();

        {
            Lock nodeLock((*it)->_lock);

            if ((*it)->_statMsg.empty())
            {
                continue;
            }

            (*it)->_statMsg.swap(statMsg);
        }

        TLOG_DEBUG("StatThread::reportStatData msg size:" << statMsg.size() << endl);

        _statPrx->async_reportMicMsg(NULL, statMsg, false);
    }
}
