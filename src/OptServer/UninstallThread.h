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
#ifndef _UNINSTALL_TREAHD_H_
#define _UNINSTALL_TREAHD_H_

#include <deque>

#include "servant/Application.h"
#include "util/tc_thread.h"
#include "util/tc_mysql.h"

#include "Router.h"
#include "DCacheOpt.h"
#include "Assistance.h"

using namespace std;
using namespace tars;
using namespace DCache;

class UninstallThread;

struct UninstallRequest
{
    string requestId;
    UninstallInfo info;
};

enum UnStatus
{
    UNINSTALLING,
    FINISH,
    UNERROR
};

struct UninstallStatus
{
    string sPercent;
    UnStatus status;
    string sError;
};

class UninstallRequestQueueManager :  public TC_ThreadLock
{
public:

    /**
    * 构造函数
    */
    UninstallRequestQueueManager();

    /**
    * 析构函数
    */
    ~UninstallRequestQueueManager();

    /**
    * 结束线程
    */
    void terminate();

    /**
    * 线程运行
    */
    void start(int iThreadNum);

    //设置db配置
    void setTarsDbConf(TC_DBConf &dbConf);

    void setRelationDbMysql(TC_DBConf &dbConf);

    void setCacheBackupPath(const string &sCacheBakPath);

public:
    /**
    * 插入发布请求
    */
    void push_back(const UninstallRequest & request);

    /**
    * 从发布队列中获取发布请求
    */
    bool pop_front(UninstallRequest & request);

    /**
    * 添加下线进度记录
    */
    void addUninstallRecord(const string &sRequestId);


    /**
    *设置下线进度记录
    */
    void setUninstallRecord(const string &sRequestId, const string &sPercent, UnStatus status, const string &sError="");

    /**
    * 获取下线进度
    */
    UninstallStatus getUninstallRecord(const string &sRequestId);

    /**
    * 删除下线进度记录
    */
    void deleteUninstallRecord(const string &sRequestId);

    size_t getUnistallQueueSize();

private:

    vector<UninstallThread *> _uninstallRunners;

    deque<UninstallRequest> _requestQueue;

    TC_ThreadLock _requestMutex;

    set<string> _uninstalling;

    TC_DBConf _dbConf;

    string _cacheBakPath;

    //记载下线进度
    map<string, UninstallStatus> _uninstallingProgress;

    TC_ThreadLock _progressMutex;
};

class UninstallThread : public TC_Thread
{
public:
    UninstallThread(UninstallRequestQueueManager * urqm);

    ~UninstallThread();

    virtual void run();

    void terminate();

    void setTarsDbMysql(TC_DBConf &dbConf);

    void setRelationDbMysql(TC_DBConf &dbConf);

    void setCacheBackupPath(const string &sCacheBakPath);

protected:
    void doUninstallRequest(UninstallRequest &request);

    int getRouterInfo(TC_Mysql &mysqlRelationDb, const string &sFullCacheServer, string &moduleName, string &routerObj);

    void reloadRouterConfByModuleFromDB(const string &moduleName, const string &routerObj);

private:
    UninstallRequestQueueManager * _queueManager;

    bool _shutDown;

    TC_Mysql _mysqlTarsDb;

    TC_Mysql _mysqlRelationDb;

    string _cacheBakPath;

    //下线通知router重新加载路由用
    CommunicatorPtr _communicator;

    AdminRegPrx _adminproxy;
};

#endif
