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
#include "util/tc_common.h"
#include "servant/Application.h"

#include "ProxyServer.h"
#include "TimerThread.h"

using namespace DCache;
using namespace tars;
using namespace std;

TimerThread::TimerThread(const time_t &tTime)
    : _stop(false), _lastSynRouterTime(tTime), _lastSynRouterTableFactoryTime(tTime) {}

TimerThread::~TimerThread() {}

void TimerThread::init(TC_Config &conf)
{
    readConf(conf);
    TLOGDEBUG("init TimerThread succ" << endl);
}

void TimerThread::reloadConf(TC_Config &conf)
{
    readConf(conf);
    TLOGDEBUG("TimerThread reload config ..." << endl);
}

void TimerThread::readConf(TC_Config &conf)
{
    _intervalToSyncRouterTable = TC_Common::strto<int>(conf["/Main<SynRouterTableInterval>"]);
    _intervalToSyncRouterTableFactory = TC_Common::strto<int>(conf["/Main<SynRouterTableFactoryInterval>"]);
}

string TimerThread::showStatus()
{
    string sResult = "";
    sResult = "-----------------------TimerThread-------------------------\n";
    sResult += "the SynRouterTableInterval: " + TC_Common::tostr<int>(_intervalToSyncRouterTable) + "\n";
    sResult += "the SynRouterTableFactoryInterval: " + TC_Common::tostr<int>(_intervalToSyncRouterTableFactory) + "\n";
    return sResult;
}

void TimerThread::run()
{
    while (true)
    {
        // 同步已存在模块的路由表数据
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        if (tNow - _lastSynRouterTime >= _intervalToSyncRouterTable)
        {
            syncRouterTable();
            _lastSynRouterTime = TC_TimeProvider::getInstance()->getNow();
        }

        // 同步模块变化数据（模块新增或者下线）
        if (tNow - _lastSynRouterTableFactoryTime >= _intervalToSyncRouterTableFactory)
        {
            syncRouterTableFactory();
            _lastSynRouterTableFactoryTime = TC_TimeProvider::getInstance()->getNow();
        }
        
        if (_stop)
        {
            TLOGDEBUG("now! stop TimerThread" << endl);
            break;
        }

        sleep(1);
    }
}

void TimerThread::stop()
{
    _stop = true;
}


void TimerThread::syncRouterTableFactory()
{
    string strErr;
    try
    {
        g_app._routerTableInfoFactory->checkModuleChange();
        return;
    }
    catch (exception &e)
    {
        strErr = string("[TimerThread::syncRouterTableFactory] exception:") + e.what();
    }
    catch (...)
    {
        strErr = "[TimerThread::syncRouterTableFactory] UnkownException";
    }

    TLOGERROR(strErr << endl);
    TARS_NOTIFY_WARN(strErr);
}


void TimerThread::syncRouterTable()
{
    string strErr;
    try
    {
        g_app._routerTableInfoFactory->synRouterTableInfo();
        return;
    }
    catch (exception &e)
    {
        strErr = string("[TimerThread::synRouterTable] exception:") + e.what();
    }
    catch (...)
    {
        strErr = "[TimerThread::synRouterTable] UnkownException";
    }

    TLOGERROR(strErr << endl);
    TARS_NOTIFY_WARN(strErr);
}
