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
#ifndef __DBHANDLE_H__
#define __DBHANDLE_H__

#include <pthread.h>
#include "RouterServerConfig.h"
#include "RouterShare.h"
#include "global.h"
#include "servant/Application.h"
#include "util/tc_common.h"
#include "util/tc_mysql.h"

using namespace tars;
using namespace DCache;
using namespace std;

#define MAX_TRANSFER_VERSION 0x7FFFFFFF
#define START_TRANSFER_VERSION 0x1

struct ModuleTransferingInfo
{
    ModuleTransferingInfo() : version(TRANSFER_CLEAN_VERSION) {}
    vector<TransferInfo> transferingInfoList;
    int version;
    map<string, int> transferingGroup;
};

class TransferMutexCond : public TC_HandleBase
{
public:
    TransferMutexCond() { initMutexCond(); }
    ~TransferMutexCond() { destroyMutexCond(); }

protected:
    void initMutexCond()
    {
        int rc;
        {
            pthread_mutexattr_t attr;
            rc = pthread_mutexattr_init(&attr);
            assert(rc == 0);

            rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
            assert(rc == 0);

            rc = pthread_mutex_init(&_mutex, &attr);
            assert(rc == 0);

            rc = pthread_mutexattr_destroy(&attr);
            assert(rc == 0);
        }

        {
            pthread_condattr_t attr;
            rc = pthread_condattr_init(&attr);
            assert(rc == 0);
            rc = pthread_cond_init(&_cond, &attr);
            assert(rc == 0);
            rc = pthread_condattr_destroy(&attr);
            assert(rc == 0);
        }
    }
    void destroyMutexCond()
    {
        pthread_cond_destroy(&_cond);
        pthread_mutex_destroy(&_mutex);
    }

public:
    pthread_mutex_t _mutex;
    pthread_cond_t _cond;
};
typedef tars::TC_AutoPtr<TransferMutexCond> TransferMutexCondPtr;

class MySqlHandlerWrapper
{
public:
    /**
     * @brief 构造函数
     */
    MySqlHandlerWrapper() = default;

    virtual void init(const map<string, string> &dbConfig)
    {
        TC_DBConf tcDBConf;
        tcDBConf.loadFromMap(dbConfig);
        _mySql.init(tcDBConf);
    }

    virtual ~MySqlHandlerWrapper() = default;

    virtual void execute(const string &sql) { _mySql.execute(sql); }

    virtual void disconnect() { _mySql.disconnect(); }

    virtual tars::TC_Mysql::MysqlData queryRecord(const string &sql)
    {
        return _mySql.queryRecord(sql);
    }

    virtual size_t updateRecord(const string &tableName,
                                const map<string, pair<tars::TC_Mysql::FT, string>> &columns,
                                const string condition)
    {
        return _mySql.updateRecord(tableName, columns, condition);
    }

    virtual size_t insertRecord(const string &tableName,
                                const map<string, pair<tars::TC_Mysql::FT, string>> &columns)
    {
        return _mySql.insertRecord(tableName, columns);
    }

    virtual size_t deleteRecord(const string &tableName, const string &condition = "")
    {
        return _mySql.deleteRecord(tableName, condition);
    }

    virtual long lastInsertID() { return _mySql.lastInsertID(); }

    virtual string buildInsertSQL(const string &tableName,
                                  const map<string, pair<tars::TC_Mysql::FT, string>> &columns)
    {
        return _mySql.buildInsertSQL(tableName, columns);
    }

    virtual string buildUpdateSQL(const string &tableName,
                                  const map<string, pair<tars::TC_Mysql::FT, string>> columns,
                                  const string &condition)
    {
        return _mySql.buildUpdateSQL(tableName, columns, condition);
    }

    virtual size_t getAffectedRows() { return _mySql.getAffectedRows(); }

private:
    tars::TC_Mysql _mySql;
};

class DbHandle
{
public:
    enum eTransferStatus
    {
        UNTRANSFER = 0,    // 未迁移
        TRANSFERING,       // 正在迁移
        TRANSFERED,        // 迁移结束
        SET_TRANSFER_PAGE  // 设置迁移页
    };

    DbHandle();
    virtual ~DbHandle();

    /*初始化配置*/
    virtual void init(int64_t reloadTime,
                      std::unique_ptr<MySqlHandlerWrapper> mysql,
                      std::unique_ptr<MySqlHandlerWrapper> mysqlDBRelation,
                      std::unique_ptr<MySqlHandlerWrapper> mysqlMigrate,
                      PropertyReportPtr report);

    virtual void initCheck();
    //检查路由是否有冗余，有就把多余的删掉
    virtual int checkRoute();

    /*载入路由到内存*/
    virtual int loadRouteToMem();

    /*按模块载入路由到内存*/
    virtual int loadRouteToMem(const string &moduleName);

    /*重新加载数据库路由*/
    virtual int reloadRouter();

    virtual int reloadRouter(const string &moduleName);

    /*重新加载配置文件及数据库*/
    virtual void reloadConf(const RouterServerConfig &conf);

    /*获取路由信息*/
    virtual int getRouterInfo(const string &moduleName,
                              int &version,
                              vector<TransferInfo> &transferingInfoList,
                              PackTable &packTable);

    /*获取版本信息*/
    virtual int getVersion(const string &sModuleName);

    /*获取自动切换类型*/
    virtual int getSwitchType(const string &sModuleName);

    /*获取路由信息*/
    virtual int getPackTable(const string &sModuleName, PackTable &packTable);

    /*获取idc 映射信息*/
    virtual int getIdcMap(map<string, string> &idcMap);

    /*获取主idc地区*/
    virtual int getMasterIdc(const string &sModuleName, string &idc);

    /*获取指定模块当前处于迁移状态的所有记录*/
    virtual int getTransferingInfos(const string &sModuleName, ModuleTransferingInfo &info);

    /*获取正在迁移的记录*/
    virtual int getTransferInfos(map<string, map<string, TransferInfo>> &info);

    /*清除内存中所有迁移记录*/
    virtual bool clearTransferInfos();

    /*获取所有业务模块信息*/
    virtual int getModuleInfos(map<string, ModuleInfo> &info);

    /*获取所有业务模块信息*/
    virtual int getModuleList(vector<string> &moduleList);

    /*判断是否正在迁移*/
    virtual bool hasTransfering();
    virtual bool hasTransfering(const string &sModuleName);
    virtual bool hasTransferingLoc(const string &sModuleName, const string &sGroupName);
    virtual bool hasTransfering(const TransferInfo &transferInfo);
    virtual bool hasTransfering(const string &sModuleName, pthread_t threadId);

    /*在路由表中插入记录*/
    virtual int insertRouterRecord(const vector<RecordInfo> &addRecords);

    virtual int insertRouterRecord(const RecordInfo &record);

    /*迁移成功后，修改路由表中的相应记录*/
    virtual int changeDbRecord(const TransferInfo &transferInfo);

    /*开始迁移前修改数据库中的路由信息，使迁移范围内的页码在一条记录内*/
    virtual int changeDbRecord2(const TransferInfo &transferInfo);

    /*根据迁移信息修改内存中的路由信息*/
    virtual int modifyMemRecord(const TransferInfo &transferInfo);

    /*迁移前修改内存中的路由信息，使迁移范围内页码在一条记录内*/
    virtual int modifyMemRecord2(const TransferInfo &transferInfo);

    /*整理数据库中分散的路由记录为连续的*/
    virtual int defragDbRecord(const TransferInfo &transferInfo);
    virtual int defragDbRecord(const string &sModuleName,
                               vector<RecordInfo> &vOldRecord,
                               vector<RecordInfo> &vNewRecord);
    virtual int defragDbRecord(string &moduleNames,
                               map<string, vector<RecordInfo>> &mOldRecord,
                               map<string, vector<RecordInfo>> &mNewRecord);

    /*整理内存中分散的路由记录为连续的*/
    virtual int defragMemRecord(const TransferInfo &transferInfo);

    /*内存迁移记录中插入记录*/
    virtual int addMemTransferRecord(const TransferInfo &transferInfo);

    /*更新内存中处于迁移状态的记录*/
    virtual int updateTransferingRecord(const TransferInfo *const pTransInfoNew,
                                        const TransferInfo *const pTransInfoComplete,
                                        bool bCleanSrc,
                                        bool bCleanDest,
                                        bool &bSrcHasInc,
                                        bool &bDestHasInc,
                                        int iReturn = 0);

    virtual int updateTransferingRecord(const string &moduleName,
                                        const string &groupName1,
                                        const string &groupName2 = "");

    /*数据库表中插入迁移记录*/
    virtual int writeDbTransferRecord(TransferInfo &transferInfo);

    /*移除内存中的迁移记录*/
    virtual int removeMemTransferRecord(const TransferInfo &transferInfo, bool &bModuleComplete);

    /*增加业务模块版本号*/
    virtual int updateVersion(const string &sModuleName);

    /*设置迁移记录状态*/
    virtual int setTransferStatus(const TransferInfo &transferInfo,
                                  eTransferStatus eway,
                                  const string &remark);

    /*设置开始迁移记录状态*/
    virtual int setStartTransferStatus(const TransferInfo &transferInfo);

    /*获取待迁移记录*/
    virtual int getTransferTask(TransferInfo &transferInfo);

    /*执行主切备后修改路由相应记录*/

    virtual int switchMasterAndSlaveInDbAndMem(const string &moduleName,
                                               const string &groupName,
                                               const string &lastMaster,
                                               const string &lastSlave,
                                               PackTable &packTable);

    /*自动切换切换读写流程*/
    virtual int switchRWDbAndMem(const string &moduleName,
                                 const string &groupName,
                                 const string &lastMaster,
                                 const string &lastSlave);

    /*自动切换切换只读流程*/
    virtual int switchReadOnlyInDbAndMem(const string &moduleName, const string &groupName);

    /*镜像切换流程*/
    virtual int switchMirrorInDbAndMem(const string &moduleName,
                                       const string &groupName,
                                       const string &serverName);

    virtual int recoverMirrorInDbAndMem(const string &moduleName,
                                        const string &groupName,
                                        const string &serverName);

    /*把服务设成不可读*/
    virtual int setServerstatus(const string &moduleName,
                                const string &groupName,
                                const string &serverName,
                                const int iStatus);

    /*镜像切换流程*/
    virtual int switchMirrorInDbAndMemByIdc(const string &moduleName,
                                            const string &groupName,
                                            const string &idc,
                                            bool autoSwitch,
                                            string &masterImage,
                                            string &slaveImage);

    virtual int loadSwitchInfo();

    //只加载特定模块的切换信息
    virtual int loadSwitchInfo(const string &moduleName);

    /*切换完成后更新服务主备最新状态到三者关系数据库*/
    virtual void updateStatusToRelationDB(const string &serverName, const string &status);

    /*开始切换前将模块，服务组，状态等信息写入数据库*/
    virtual int insertSwitchInfo(const string &moduleName,
                                 const string &groupName,
                                 const string &masterServer,
                                 const string &slaveServer,
                                 const string &mirrorIdc,
                                 const string &mirrorServer,
                                 const int switchType,
                                 long &id);
    virtual int insertSwitchInfo(const string &moduleName,
                                 const string &groupName,
                                 const string &masterServer,
                                 const string &slaveServer,
                                 const string &mirrorIdc,
                                 const string &masterMirror,
                                 const string &slaveMirror,
                                 const string &switchProperty,
                                 const string &comment,
                                 const int switchType,
                                 const int switchResult,
                                 const int groupSatus,
                                 const time_t switchBeginTime);

    /*更新切换结果*/
    virtual void updateSwitchInfo(long iID,
                                  int iResult,
                                  const string &sComment,
                                  int iGroupStatus = 0,
                                  const string &sCurMasterServer = "",
                                  const string &sCurSlaveServer = "");

    virtual void updateSwitchMirrorInfo(long iID,
                                        int iResult,
                                        const string &sComment,
                                        int iGroupStatus = 0,
                                        const string &sCurMasterImage = "",
                                        const string &sCurSlaveImage = "");

    virtual void updateSwitchGroupStatus(const string &moduleName,
                                         const string &groupName,
                                         const string &sIdc,
                                         const string &comment,
                                         int iGroupStatus);
    virtual void updateSwitchGroupStatus(long iID, int iGroupStatus);

    /*自动切换切换读写流程之前，生成临时路由推送给主备机*/
    virtual int getPackTable4SwitchRW(const string &moduleName,
                                      const string &groupName,
                                      const string &lastMaster,
                                      const string &lastSlave,
                                      PackTable &tmpPackTable);

    /*从数据库获取所有的业务模块信息*/
    virtual vector<ModuleInfo> getDBModuleList();
    virtual ModuleInfo getDBModuleList(const string &moduleName);

    /*从数据库获取所有路由记录*/
    virtual vector<RecordInfo> getDBRecordList();
    virtual vector<RecordInfo> getDBRecordList(const string &moduleName);

    /*从数据库获取所有服务器组信息*/
    virtual vector<GroupInfo> getDBGroupList();
    virtual vector<GroupInfo> getDBGroupList(const string &moduleName);

    /*从数据库获取所有服务器信息*/
    virtual map<string, ServerInfo> getDBServerList();
    virtual map<string, ServerInfo> getDBServerList(const string &moduleName);

    virtual int addMirgrateInfo(const string &strIp, const string &strReason);

    virtual int getAllServerInIp(const string &strIp, vector<string> &vServerName);

    virtual bool doTransaction(const vector<string> &vSqlSet, string &sErrMsg);

    /* 保存迁移线程条件锁到内存 */
    virtual int addMemTransferMutexCond(const TransferInfo &transferInfo);

    /* 移除内存中迁移线程条件锁 */
    virtual int removeMemTransferMutexCond(const TransferInfo &transferInfo);

    /* 获取指定等待的迁移线程的条件锁 */
    virtual int getTransferMutexCond(const string &moduleName,
                                     const string &threadID,
                                     pthread_mutex_t **pMutex,
                                     pthread_cond_t **pCond);

    /* 获取所有等待的迁移线程的条件锁 */
    virtual int getTransferMutexCond(map<string, map<string, TransferMutexCondPtr>> &mMutexCond);

    /* 检查服务是否下线 */
    virtual int checkServerOffline(const string &serverName, bool &bOffline);

    /* Router主机从MASTER降级到SLAVE时调用，用来清理数据 */
    virtual void downgrade();

private:
    map<string, PackTable> *_mapPackTables;  // 业务模块名，打包的路由信息
    TC_ThreadLock _lock;                     // 数据库配置加载锁
    map<string, map<string, TransferInfo>> _mapTransferInfos;  // 每个模块当前的迁移记录
    map<string, ModuleTransferingInfo> _mapTransferingInfos;  // 每个模块当前处于迁移状态信息
    TC_ThreadLock _transLock;                                 // 迁移互斥
    map<string, PackTable> *_mapPackTables1;  // 备份数据一套，用于重载配置时切换
    int64_t _lastLoadTime;                    // 正式路由信息的最后加载时间(us)
    TC_ThreadLock _dbLock;                    // 数据库锁
    std::unique_ptr<MySqlHandlerWrapper> _mysql;            // 数据库连接
    TC_ThreadLock _relationLock;                            // 三者关系数据库锁
    std::unique_ptr<MySqlHandlerWrapper> _mysqlDBRelation;  // 三者关系数据库
    int64_t _reloadTime;         // 重载配置的最小间隔时间(us)，从配置文件读取
    PropertyReportPtr _exRep;    // 异常属性上报
    TC_ThreadLock _migrateLock;  // 新增迁移状态数据库锁
    std::unique_ptr<MySqlHandlerWrapper> _mysqlMigrate;  // 新增迁移状态数据库
    map<string, map<string, TransferMutexCondPtr>> _mapTransferMutexCond;  // 每个迁移任务的锁
};

#endif  // __DBHANDLE_H__
