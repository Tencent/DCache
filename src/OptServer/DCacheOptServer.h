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
#ifndef _DCacheOptServer_H_
#define _DCacheOptServer_H_

#include <iostream>

#include "servant/Application.h"

#include "UninstallThread.h"
#include "ReleaseThread.h"

using namespace tars;

/**
 *
 **/
class DCacheOptServer : public Application
{
public:
    /**
     *
     **/
    virtual ~DCacheOptServer() {};

    /**
     *
     **/
    virtual void initialize();

    /**
     *
     **/
    virtual void destroyApp();

    bool defragRoute(const string& command, const string& params, string& sResult);

    bool stopDefrag(const string& command, const string& params, string& sResult);

    bool help(const string& command, const string& params, string& sResult);

    int defragRouteRecordFromDB(const string & sApp, const std::string & sRouterServerName, std::string & sResult);

    ReleaseRequestQueueManager* releaseRequestQueueManager();

    int getReleaseID();

    UninstallRequestQueueManager* uninstallRequestQueueManager();


protected:

    // release 任务ID
    TC_Atomic _releaseID;

    // release 请求管理器
    ReleaseRequestQueueManager _releaseRequestQueueManager;

    // uninstall 请求管理器
    UninstallRequestQueueManager _uninstallRequestQueueManager;
};

extern DCacheOptServer g_app;

////////////////////////////////////////////
#endif
