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
#ifndef _TRANSFER_H_
#define _TRANSFER_H_

#include <atomic>
#include "RouterServerConfig.h"
#include "global.h"

class OuterProxyFactory;
class DbHandle;

namespace DCache
{
class Transfer
{
public:
    Transfer(std::shared_ptr<OuterProxyFactory> outerProxy);

    // 初始化
    void init(std::shared_ptr<DbHandle> dbHandle);

    // 重新加载配置
    void reloadConf();

    // 执行迁移逻辑
    int doTransfer(const TransferInfo &transferInfoIn, string &info);

    // 通知业务模块相关的cacheserver重新加载路由
    void notifyAllServer(const string moduleNames,
                         map<string, vector<string>> &succServers,
                         map<string, vector<string>> &failServers);

private:
    // 通知迁移目的服务器准备迁移
    int notifyTransDestServer(const TransferInfo &transferInfo);

    // 通知迁移源服务器准备迁移
    int notifyTransSrcServer(const TransferInfo &transferInfo);

    // 通知迁移源服务器进行迁移
    int notifyTransSrcServerDo(const TransferInfo &transferInfo);

    // 通知迁移源、目的服务器迁移结果，加载新路由
    int notifyTransResult(const TransferInfo &transferInfo);

    // 通知迁移服务器清除当前所有迁移记录
    int notifyTransServerClean(const string &moduleName, const string &groupName);

    // 通知业务模块相关的cacheserver重新加载路由
    int notifyAllServer(const string &moduleName,
                        vector<string> &succServers,
                        vector<string> &failServers);

    // 修改内存当前处于迁移状态的记录
    int updateTransferingRecord(const TransferInfo *pTransInfoNew,
                                const TransferInfo *pTransInfoComplete);

    // 迁移成功后修改数据库及内存中的路由
    int modifyRouterAfterTrans(TransferInfo *pTransInfoComplete);

    // 迁移前修改数据库及内存中的路由信息，使迁移范围内页码在一条记录内
    int modifyRouterBefTrans(const TransferInfo &transferInfo);

    // 整理数据库及内存中分散的路由记录为连续的
    int defragRouterInfo(const TransferInfo &transferInfo);

    // 判断当前是否能进行迁移
    int canTransfer(TransferInfo &transferInfo);

    int canTransfer(const TransferInfo &transferInfo, const PackTable &packTable);

    // 判断迁移页是否属于源服务器
    bool isOwnedbySrcGroup(const TransferInfo &transferInfo, const PackTable &packTable);

    // 清除内存中已迁移记录
    int removeTransfer(const TransferInfo &transferInfo, bool &bModuleComplete);

    int getRouterClientJceAddr(PackTable &packTable, const string &groupName, string &addr);

    string getRouterClientJceAddr(const string &server, PackTable &packTable);

    // push 路由信息到迁移服务器组中的master
    int setServerRouterInfo(int iTransInfoListVer,
                            const vector<TransferInfo> &transferingInfoList,
                            PackTable &packTable,
                            const string &sGroupName);

    // push路由信息到服务器组中的master
    int setServerRouterInfo(PackTable &packTable, const string &groupName);

    // 重新加载路由
    int reloadRouter();

    // 设置迁移记录的迁移结束状态
    int SetTransferEnd(const TransferInfo &transferInfo, const string &info);

    // 将返回值数组转化为字符串信息
    string RetVal2Info(const vector<int> &vRet);

    int deleteTransSrcServer(const TransferInfo &transferInfo);

private:
    std::shared_ptr<DbHandle> _dbHandle;
    std::shared_ptr<OuterProxyFactory> _outerProxy;  // 代理工厂
    int _transTo;                                    // 迁移源服务器迁移超时时间(us)
    int _defragDbInterval;                           // 隔多少页整理一下数据库记录
    int _retryTranInterval;                          // 迁移一页失败时，重试时间间隔
    int _retryTranMaxTimes;                          // 迁移一页失败时，重试最大次数
    int _transferPagesOnce;           // 每次通知源和目的服务器迁移的页数
    int _transferPagesOnceMax;        // 每次通知源和目的服务器迁移的最大页数
    TC_ThreadLock _transferPageLock;  // 动态调整迁移页数锁
};

struct TransferParam : public TC_HandleBase
{
    TransferParam(pthread_mutex_t *mutex, pthread_cond_t *cond, int ret)
        : _mutex(mutex), _cond(cond), _return(ret)
    {
    }

    ~TransferParam() {}

    pthread_mutex_t *_mutex;
    pthread_cond_t *_cond;

    int _return;
};
typedef tars::TC_AutoPtr<TransferParam> TransferParamPtr;

struct RouterClientCallback : public RouterClientPrxCallback
{
    RouterClientCallback(const TransferParamPtr &param) { _param = param; }
    virtual ~RouterClientCallback() {}

    virtual void callback_toTransferStart(tars::Int32 ret);
    virtual void callback_toTransferStart_exception(tars::Int32 ret);
    virtual void callback_fromTransferStart(tars::Int32 ret);
    virtual void callback_fromTransferStart_exception(tars::Int32 ret);
    virtual void callback_fromTransferDo(tars::Int32 ret);
    virtual void callback_fromTransferDo_exception(tars::Int32 ret);
    virtual void callback_cleanFromTransferData(tars::Int32 ret);
    virtual void callback_cleanFromTransferData_exception(tars::Int32 ret);

private:
    // 唤醒等待线程
    void wakeUpWaitingThread()
    {
        pthread_mutex_lock(_param->_mutex);
        pthread_cond_signal(_param->_cond);
        pthread_mutex_unlock(_param->_mutex);
    }
    TransferParamPtr _param;
};

class TransferDispatcher : public std::enable_shared_from_this<TransferDispatcher>
{
public:
    typedef std::function<int(const TransferInfo &, std::string &)> DoTransfer;

    TransferDispatcher(DoTransfer func,
                       int threadPoolSize,
                       int minSizeEachGroup,
                       int maxSizeEachGroup);

    // 向任务队列中增加任务
    void addTransferTask(std::shared_ptr<TransferInfo> task);

    // 执行任务队列中已经存在的任务
    void doTransferTask();

    void clearTasks();

    void terminate();

    // 获取队列中指定组的待迁移任务数
    int getQueueTaskNum(const std::string &groupName) const;

    // 获取队列中所有待迁移的任务数
    int getTotalQueueTaskNum() const { return _tasksNumInQueue; }

    // 获取指定组正在迁移的任务数
    int getTransferingTaskNum(const std::string &groupName) const;

    // 获取全部正在执行迁移的任务数
    int getTotalTransferingThreadNum() const { return _TransferingTasksNum; }

private:
    typedef std::unordered_map<std::string, std::queue<std::shared_ptr<TransferInfo>>>
        TransferQueue;
    // 分配任务，每个组最多分配maxThreadNum个线程
    void dispatcherTask(int maxThreadNum);
    void transferFuncWrapper(std::shared_ptr<TransferInfo> task);
    TransferQueue _taskQueue;  //待执行迁移的任务队列;eue;
    // mutable tars::TC_ThreadLock _taskQueueLock;
    std::unordered_map<std::string, int> _transferingTasks;  // 正在执行迁移的任务
    mutable tars::TC_ThreadLock _transferingTasksLock;
    std::atomic<int> _tasksNumInQueue;      // 待迁移的任务数
    std::atomic<int> _TransferingTasksNum;  // 正在迁移的任务数
    std::atomic<int> _idleThreadNum;        // 线程池中空闲线程数目
    TC_ThreadPool _tpool;                   // 线程池
    DoTransfer _transferFunc;               // 执行迁移任务的函数
    const int _minThreadEachGroup;          // 每组分配的最小线程数
    const int _maxThreadEachGroup;          // 每组分配的最大线程数
};

}  // namespace DCache

#endif  // _TRANSFER_H_
