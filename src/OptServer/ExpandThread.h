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
#ifndef _EXPAND_THREAD_H_
#define _EXPAND_THREAD_H_

#include <pthread.h>
#include <map>
#include <set>
#include <utility>
#include <vector>
#include <string>

#include "util/tc_common.h"
#include "util/tc_mysql.h"
#include "util/tc_config.h"
#include "util/tc_monitor.h"

#include "AdminReg.h"
#include "Node.h"

using namespace std;
using namespace tars;

class ExpandThread
{

public:

    ExpandThread(){}
    ~ExpandThread(){}

    /*
    *初始化
    */
    void init(const string &sConf);
    /*
    *生成线程
    */
    void createThread();

    /*
    *线程run
    */
    static void* Run(void* arg);

    void terminate();

    static TC_ThreadLock _lock;

protected:

	int getRouterObj(const string & appName, string & routerObj, string & errmsg);

    int getRouterDBInfo(const string &appName, TC_DBConf &routerDbInfo);

    int defragRouterRecord(const string & app, const std::string & routerName, std::string & errmsg);

protected:

    bool _terminate;

    TC_Config _tcConf;

    tars::CommunicatorPtr _communicator;

    AdminRegPrx _adminRegPrx;

    int _checkInterval;

    TC_Mysql _mysqlRelationDB;

    map<string, int> _mFailCounts;

};

#endif

