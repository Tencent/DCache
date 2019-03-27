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
#include "DCacheOptServer.h"
#include "DCacheOptImp.h"

using namespace std;

DCacheOptServer g_app;

void DCacheOptServer::initialize()
{
    //增加对象
    addServant<DCacheOptImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".DCacheOptObj");
    addConfig("DCacheOptServer.conf");

    TC_Config conf;
    conf.parseFile(ServerConfig::BasePath + "DCacheOptServer.conf");

    map<string, string> tarsDBInfo = conf.getDomainMap("/Main/DB/tars");
    TC_DBConf tarsDbConf;
    tarsDbConf.loadFromMap(tarsDBInfo);

    map<string, string> relationDBInfo = conf.getDomainMap("/Main/DB/relation");
    TC_DBConf relationDbConf;
    relationDbConf.loadFromMap(relationDBInfo);

    // release 任务ID 初始化为0
    _releaseID.set(0);

    // 初始化 release 线程
    int releaseThreadCount = TC_Common::strto<int>(conf.get("/Main/Release<ThreadCount>", "5"));
    _releaseRequestQueueManager.start(releaseThreadCount);

    // 初始化 uninstall 线程
    int uninstallThreadCount = TC_Common::strto<int>(conf.get("/Main/Uninstall<ThreadCount>", "2"));
    _uninstallRequestQueueManager.start(uninstallThreadCount);
    _uninstallRequestQueueManager.setTarsDbConf(tarsDbConf);
    _uninstallRequestQueueManager.setRelationDbMysql(relationDbConf);
    _uninstallRequestQueueManager.setCacheBackupPath(conf.get("/Main/Uninstall<BakPath>", "/usr/local/app"));
}

ReleaseRequestQueueManager* DCacheOptServer::releaseRequestQueueManager()
{
    return &_releaseRequestQueueManager;
}

int DCacheOptServer::getReleaseID()
{
    return _releaseID.inc();
}

UninstallRequestQueueManager* DCacheOptServer::uninstallRequestQueueManager()
{
    return &_uninstallRequestQueueManager;
}

void DCacheOptServer::destroyApp()
{
    TLOGDEBUG("DCacheOptServer::destroyApp ok" << endl);
}

/////////////////////////////////////////////////////////////////
