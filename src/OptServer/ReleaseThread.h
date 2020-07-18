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
#ifndef _RELEASE_TREAHD_H_
#define _RELEASE_TREAHD_H_

#include <deque>

#include "servant/Application.h"
#include "util/tc_thread.h"

#include "DCacheOpt.h"
#include "Assistance.h"

using namespace std;
using namespace tars;
using namespace DCache;

class ReleaseThread;

struct ReleaseRequest
{
    int requestId;
    vector<ReleaseInfo> releaseInfo;
};

enum ReStatus
{
    RELEASING,
    RELEASE_FINISH,
    RELEASE_FAILED
};

struct ReleaseStatus
{
    int percent;
    ReStatus status;
    string errmsg;
};

struct ReleaseProgress
{
    ReleaseStatus releaseStatus;
    ReleaseRequest * releaseReq;
};

class ReleaseRequestQueueManager :  public TC_ThreadLock
{
public:

    /**
    * 构造函数
    */
    ReleaseRequestQueueManager();

    /**
    * 析构函数
    */
    ~ReleaseRequestQueueManager();

    /**
    * 结束线程
    */
    void terminate();

    /**
    * 线程运行
    */
    void start(int iThreadNum);

public:

    /**
    * 插入发布请求
    */
    void push_back(ReleaseRequest * request);

    /**
    * 从发布队列中获取发布请求
    */
    bool pop_front(ReleaseRequest ** request);

    /**
    * 添加release进度记录
    */
    void addProgressRecord(int requestId, ReleaseRequest * request);

    /**
    * 设置release进度记录
    */
    void setProgressRecord(int requestId, int percent, ReStatus status, const string &errmsg="");

   /**
    * 获取release极度
    */
    ReleaseStatus getProgressRecord(int requestId, vector<ReleaseInfo> & releaseInfo);

    /**
    * 删除release进度记录
    */
    void deleteProgressRecord(int requestId);

    size_t getReleaseQueueSize();

private:

    vector<ReleaseThread *> _releaseRunners;

    deque<ReleaseRequest *> _requestQueue;

    TC_ThreadLock _requestMutex;

    //记录release进度
    map<int, ReleaseProgress> _releaseProgress;

    TC_ThreadLock   _progressMutex;

};

class ReleaseThread : public TC_Thread
{
public:

    ReleaseThread(ReleaseRequestQueueManager * rrqm);

    ~ReleaseThread();

    virtual void run();

    void terminate();

protected:

    void doReleaseRequest(ReleaseRequest * request);

    int releaseServer(ReleaseInfo &serverInfo, string & errmsg);

private:

    ReleaseRequestQueueManager * _queueManager;

    bool _shutDown;

    CommunicatorPtr _communicator;

    AdminRegPrx     _adminproxy;

};

#endif
