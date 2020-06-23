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
#ifndef __ROUTERSERVER_H__
#define __ROUTERSERVER_H__

#include <atomic>
#include <memory>
//#include "EtcdThread.h"
#include "SwitchThread.h"
#include "TimerThread.h"
#include "global.h"
#include "servant/Application.h"

using namespace tars;
using namespace std;

enum RouterType
{
    ROUTER_MASTER = 0,
    ROUTER_SLAVE
};

struct SwitchTimes
{
    int switchTimes;
    time_t firstTime;
};

struct GroupHeartBeatInfo
{
    string masterServer;
    string slaveServer;
    time_t masterLastReportTime;
    time_t slaveLastReportTime;
    map<string, time_t> mirrorInfo;         // mapkey为镜像服务名
    map<string, vector<string>> mirrorIdc;  // mapkey为idc value为idc
    map<string, int> serverStatus;          //服务状态，0表示正常，非0表示不正常
    int status;                             // 0 正常状态 1 自动切换中
};

typedef map<string, map<string, map<int, SwitchTimes>>> SwitchTimesMap;
typedef map<string, map<string, GroupHeartBeatInfo>> HeartbeatInfo;

#define ADD_ADMIN_CMD_NORMAL(c,f) \
    do { TARS_ADD_ADMIN_CMD_NORMAL(c, f); addAdminCommand(string(c), std::bind(&f, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));} while (0);

class RouterServer : public Application
{
public:
    typedef std::function<std::shared_ptr<AdminRegProxyWrapper>()> AdminRegCreatePolicy;

    RouterServer()
        : _dbHandle(std::make_shared<DbHandle>()),
          _outerProxy(std::make_shared<OuterProxyFactory>()),
          _transfer(std::make_shared<DCache::Transfer>(_outerProxy)),
//          _enableEtcd(false),
          _routerType(ROUTER_SLAVE)
    {
    }

    virtual ~RouterServer(){};

    virtual void initialize();

    virtual bool reloadRouter(const string &command, const string &params, string &result);

    virtual bool reloadRouterByModule(const string &command, const string &params, string &result);

    virtual bool reloadConf(const string &command, const string &params, string &result);

    virtual bool getVersions(const string &command, const string &params, string &result);

    virtual bool getRouterInfos(const string &command, const string &params, string &result);

    virtual bool getTransferInfos(const string &command, const string &params, string &result);

    virtual bool getTransferingInfos(const string &command, const string &params, string &result);

    virtual bool clearTransferInfos(const string &command, const string &params, string &result);

    virtual bool notifyCacheServers(const string &command, const string &params, string &result);

    virtual bool defragRouteRecords(const string &command, const string &params, string &result);

    virtual bool deleteAllProxy(const string &command, const string &params, string &result);

    virtual bool help(const string &command, const string &params, string &result);

    virtual bool switchByGroup(const string &command, const string &params, string &result);

    virtual bool switchMirrorByGroup(const string &command, const string &params, string &result);

    virtual bool showHeartBeatInfo(const string &command, const string &params, string &result);

    virtual bool resetServerStatus(const string &command, const string &params, string &result);

    virtual bool checkModule(const string &command, const string &params, string &result);

    virtual bool procAdminCommand(const string &command, const string &params, string &result);

//    virtual bool isEnableEtcd() const { return _enableEtcd; }

//    virtual void setEnableEtcd(bool b) { _enableEtcd = b; }

    virtual void setRouterType(enum RouterType t) { _routerType = t; }

//    virtual void updateLastHeartbeat(const int64_t lastHeartbeat) { _etcdThread.updateLastHeartbeat(lastHeartbeat); }

    virtual enum RouterType getRouterType() const { return _routerType; }

    virtual void setMasterRouterObj(const std::string obj)
    {
        TC_ThreadLock::Lock lock(_objLock);
        _masterRouterObj = obj;
    }

    virtual std::string getMasterRouterObj() const
    {
        TC_ThreadLock::Lock lock(_objLock);
        return _masterRouterObj;
    }

    virtual int getMasterRouterPrx(RouterPrx &prx, string &masterRouterObj) const;

    virtual void destroyApp();

    virtual void clearSwitchThreads();

    virtual void terminateSwitchThreads();

    virtual void removeFinishedSwitchThreads();

    virtual void addSwitchThread(DoSwitchThreadPtr threadPtr);

    virtual int checkModuleSwitching(const TransferInfo &transferInfo) const;

    virtual int switchingModuleNum() const;

    virtual bool isModuleSwitching(const std::string &moduleName) const;

    virtual void addSwitchModule(const std::string &moduleName);

    virtual void removeSwitchModule(const std::string &moduleName);

    virtual int checkSwitchTimes(const std::string &moduleName,
                                 const std::string &groupName,
                                 int type,
                                 int maxTimes) const;

    virtual void addSwitchTime(const string &moduleName,
                               const string &groupName,
                               const int &type,
                               time_t now);

    virtual void resetSwitchTimes();

    virtual void loadSwitchInfo(std::map<string, PackTable> *packTables, const bool isUpgrade = false);

    virtual void loadSwitchInfo(std::map<string, PackTable> *packTables,
                                const std::string &moduleName);

    virtual void setHeartbeatServerStatus(const string &moduleName,
                                          const string &groupName,
                                          const string &serverName,
                                          int status);

    virtual void swapMasterSlave(const string &moduleName,
                                 const string &groupName,
                                 int timeReportSet,
                                 bool readOver,
                                 bool bMasterChange);

    virtual void switchMirrorOver(const string &moduleName,
                                  const string &groupName,
                                  int timeReportSet,
                                  bool succ,
                                  bool masterMirrorChange,
                                  const string &idc);

    virtual tars::TC_ThreadLock &getHeartbeatInfoLock() { return _heartbeatInfoLock; }

    virtual const HeartbeatInfo &getHeartbeatInfo() const { return _heartbeatInfo; }

    virtual HeartbeatInfo &getHeartbeatInfo() { return _heartbeatInfo; }

    // 本机从master降级为slave
    virtual void downgrade();

    // 本机从slave升级为master
    virtual void upgrade();

    virtual std::shared_ptr<DbHandle> getDbHandle() { return _dbHandle; }

    virtual std::shared_ptr<AdminRegProxyWrapper> createAdminRegProxyWrapper()
    {
        return _adminCreatePolicy();
    }

    virtual const RouterServerConfig &getGlobalConfig() const { return _conf; }

    virtual std::shared_ptr<DCache::Transfer> getTransfer() { return _transfer; }

private:
    // 启动ETCD相关的流程。
   int setUpEtcd();

protected:
    // 测试用，不要在正式代码中调用此方法。
    void setAdminRegProxyWrapperFactory(AdminRegCreatePolicy f) { _adminCreatePolicy = f; }

    std::unique_ptr<MySqlHandlerWrapper> createMySqlHandler(const std::map<string, string> &dbConfig) const;

    virtual PropertyReportPtr createDBPropertyReport() const
    {
        return Application::getCommunicator()->getStatReport()->createPropertyReport(
            "DBError", PropertyReport::count());
    }

    void addAdminCommand(const string& command, TAdminFunc func);
private:
    AdminRegCreatePolicy _adminCreatePolicy;
    std::shared_ptr<DbHandle> _dbHandle;               // 数据库操作句柄
    std::shared_ptr<OuterProxyFactory> _outerProxy;    // 代理工厂
    std::shared_ptr<DCache::Transfer> _transfer;
//    std::shared_ptr<EtcdHandle> _etcdHandle;
//    bool _enableEtcd;                                  // 是否开启ETCD
    std::atomic<enum RouterType> _routerType;          // router的类型(主机或备机)
    RouterServerConfig _conf;                          // 配置文件管理
    std::string _masterRouterObj;                      // master主机的obj
    mutable tars::TC_ThreadLock _objLock;              // 对_masterRouterObj的锁
    vector<DoSwitchThreadPtr> _doSwitchThreads;        // 执行切换的线程集合
    mutable tars::TC_ThreadLock _doSwitchThreadsLock;  // 对_doSwitchThreads的锁
    map<string, int> _moduleSwitching;                 // 哪些模块在切换中
    mutable tars::TC_ThreadLock _moduleSwitchingLock;  // 对_moduleSwitching的锁
    SwitchTimesMap _switchTimes;                       // 每个组的切换次数：map<moduleName, map<groupName, // map<switchType, times> > >
    mutable tars::TC_ThreadLock _switchTimesLock;      // 对_switchTimes的锁
    HeartbeatInfo _heartbeatInfo;                      // 主机心跳上报信息，key为groupName
    mutable tars::TC_ThreadLock _heartbeatInfoLock;    // 对_heartbeatInfo的锁
    TimerThread  _timerThread;                         // 定时任务线程
//    EtcdThread   _etcdThread;                          // ETCD处理线程
    SwitchThread _switchThread;                        // 主备切换线程
    map<string, TAdminFunc> _procAdminCommandFunctors; // 处理管理命令函数列表
};

#endif  // __ROUTERSERVER_H__
