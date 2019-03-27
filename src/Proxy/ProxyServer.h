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
#ifndef _PROXYSERVER_H_
#define _PROXYSERVER_H_

#include <iostream>

#include "util/tc_config.h"
#include "util/tc_common.h"
#include "servant/Application.h"

#include "StatThread.h"
#include "RouterTableInfoFactory.h"
#include "TimerThread.h"

using namespace tars;

class ProxyServer : public Application
{
  public:
    ProxyServer() {}

    virtual ~ProxyServer() {}

    virtual void initialize();

    virtual void destroyApp();

    bool help(const string &command, const string &params, string &sResult);

    bool reloadConf(const string &command, const string &params, string &sResult);

    bool showStatus(const string &command, const string &params, string &sResult);

    bool module(const string &command, const string &params, string &sResult);

    bool needStatReport()
    {
        return _needStatReport;
    }

    StatThread *getStatThread() const
    {
        return _statThread;
    }

  private:
    void addManagementCmds();

    void initRouter(TC_Config &conf);

    void initStatReport(TC_Config &conf);

    void initOthers(TC_Config &conf);

  public:
    RouterTableInfoFactory *_routerTableInfoFactory;

    unsigned int _keyCountLimit;

    bool _printBatchCount;

  private:
    int _reloadConfInterval; //_minInterval2ReloadConf

    int64_t _lastReloadConfTime; // 上次重新载入所有配置文件的时间

    StatThread *_statThread;

    TimerThread *_timerThread;

    bool _needStatReport;
};

extern ProxyServer g_app;

#endif

