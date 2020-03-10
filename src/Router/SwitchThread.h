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
#ifndef _SWITCHTHREAD_H_
#define _SWITCHTHREAD_H_

#include <sys/time.h>
#include <string>
#include <vector>
#include "framework/AdminReg.h"
#include "DbHandle.h"
#include "ProxyWrapper.h"
#include "global.h"
#include "servant/EndpointF.h"
#include "servant/EndpointInfo.h"
#include "servant/QueryF.h"
#include "util/tc_thread.h"

using namespace tars;
using namespace std;

struct SwitchWork
{
    int type;  //是主备切换还是镜像切换
    string moduleName;
    string groupName;
    string masterServer;
    string slaveServer;
    string mirrorIdc;
    string mirrorServer;
    int switchType;
    time_t switchBeginTime;
};

typedef std::shared_ptr<AdminRegProxyWrapper> AdminProxyWrapperPtr;

class DoCheckTransThread : public TC_HandleBase, public TC_Thread, public TC_ThreadLock
{
public:
    DoCheckTransThread() = default;

    ~DoCheckTransThread() = default;

    void init(AdminProxyWrapperPtr adminProxy,
              const string &moduleName,
              const string &serverName,
              const string &reasonStr,
              std::shared_ptr<DbHandle> dbHandle);

    void run();

    int doCheckTrans(const string &moduleName, const string &serverName, const string &reasonStr);

    int checkMachineDown(const string &serverIp);

    bool isFinish() { return _isFinish; };

private:
    std::shared_ptr<DbHandle> _dbHandle;
    std::shared_ptr<AdminRegProxyWrapper> _adminProxy;
    static map<string, time_t> _isCheckTransDown;  // key 为服务器名， value为上次入库时间
    string _moduleName;
    string _serverName;
    string _reasonStr;
    bool _isFinish;
};

typedef TC_AutoPtr<DoCheckTransThread> DoCheckTransThreadPtr;

class DoSwitchThread : public TC_HandleBase
{
public:
    DoSwitchThread(AdminProxyWrapperPtr adminProxy,
                   std::shared_ptr<DbHandle> dbHandle,
                   int switchTimeOut,
                   int switchBinLogDiffLimit,
                   int switchMaxTimes,
                   int downGradeTimeout);

    virtual ~DoSwitchThread() = default;

    static void *run(void *arg)
    {
        pthread_detach(pthread_self());
        DoSwitchThread *pthis = (DoSwitchThread *)arg;

        pthis->resetSwitchTimes();

        pthis->doSwitch();

        return NULL;
    }

    void join() { pthread_join(thread, NULL); };

    void CreateThread(SwitchWork task);

    bool isFinish() { return _isFinish; };

    void doSwitch();

    SwitchWork getWork() { return _work; };

protected:
    //把备机设置为不可读
    void doSlaveDead(const SwitchWork &switchWork);

    /*执行主备自动切换流程*/
    void doSwitchMaterSlave(const SwitchWork &switchWork);

    /*执行镜像自动切换流程*/
    void doSwitchMirror(const SwitchWork &switchWork);

    /*主备自动切换流程完毕*/
    void doSwitchOver(const string &moduleName,
                      const string &groupName,
                      int timeReportSet = 30,
                      bool readOver = false,
                      bool bMasterChange = false);

    /*镜像自动切换流程完毕*/
    void doSwitchMirrorOver(const string &moduleName,
                            const string &groupName,
                            int timeReportSet = 30,
                            bool succ = false,
                            bool bMasterMirrorChange = false,
                            const string &sIdc = "");

    /*向指定server发送心跳*/
    int heartBeatSend(const string &serverName);

    /*查询备机的同步差异*/
    int slaveBinlogdif(const string &serverName, int &diffBinlogTime);

    /*修改路由为只读*/
    int chageRouterOnlyRead(const string &moduleName, const string &groupName);

    /*修改为镜像不可用*/
    int chageMirrorBad(const string &moduleName, const string &groupName, const string &serverName);

    /*修改为镜像主备切换*/
    int chageMirrorBad(const string &moduleName,
                       const string &groupName,
                       const string &mirrorIdc,
                       string &masterImage,
                       string &slaveImage);

    //设置心跳服务状态
    int setHeartBeatServerStatus(const string &moduleName,
                                 const string &groupName,
                                 const string &serverName,
                                 const int iStatus);

    //检查切换次数
    int checkSwitchTimes(const string &moduleName, const string &groupName, const int &type);

    //更新切换次数
    void updateSwitchTimes(const string &moduleName,
                           const string &groupName,
                           const int &type,
                           const time_t &tNowTime);

    //重置切换次数
    void resetSwitchTimes();

    //备机是否准备好去切换
    int slaveReady(const string &moduleName, const string &serverName, bool &bReady);

    //通知备机断开和主机的连接
    int disconFromMaster(const string &moduleName, const string &serverName);

    //下发路由
    int setRouter4Switch(const string &moduleName,
                         const string &groupName,
                         const string &oldMaster,
                         const string &newMaster);

    //通知主机降级
    int notifyMasterDowngrade(const string &moduleName, const string &serverName);

private:
    //检查服务是否被人工关闭
    int checkServerSettingState(const string &serverName);

    std::shared_ptr<DbHandle> _dbHandle;
//    int _switchTimeout;
    int _switchBlogDifLimit;
    int _switchMaxTimes;    //每天最多切换次数
    int _downGradeTimeout;  //主机降级的等待时间
    bool _isFinish;
    bool _start;
    SwitchWork _work;
    pthread_t thread;
    std::shared_ptr<AdminRegProxyWrapper> _adminProxy;
};

typedef TC_AutoPtr<DoSwitchThread> DoSwitchThreadPtr;

class SwitchThread : public TC_Thread, public TC_ThreadLock
{
public:
    SwitchThread();

    virtual ~SwitchThread();

    void init(AdminProxyWrapperPtr adminProxy, std::shared_ptr<DbHandle> dbHandle);

    virtual void run();

    void terminate();

    int checkServerSettingState(const string &serverName);

    int checkMachineDown(const string &serverName, const string &serverIp);

    int doCheckTrans(const string &moduleName, const string &serverName, const string &reasonStr);

    // Router主机由MASTER降级到SLAVE
    void downgrade();

protected:
    /*自动切换检查*/
    void doSwitchCheck();

    //检查备机是否可用
    void doSlaveCheck();

private:
    // 重新加载路由信息
    int reloadRouter();

private:
    std::shared_ptr<DbHandle> _dbHandle;
    bool _terminate;
	bool _enable;
    int _switchTimeout;
    int _switchCheckInterval;  //自动切换check间隔
    time_t _lastNotifyTime;
    vector<DoCheckTransThreadPtr> _doCheckTransThreads;
    int _slaveTimeout;
    std::shared_ptr<AdminRegProxyWrapper> _adminProxy;
};

#endif
