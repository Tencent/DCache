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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
// #include <time.h>
#include "DbHandle.h"
#include "MockRouterServerConfig.h"

class MockPropertyReport : public PropertyReport
{
public:
    virtual void report(int iValue) override {}
    // DbHandle并没有用到下面的方法
    virtual vector<pair<string, string>> get() override
    {
        return std::vector<std::pair<string, string>>();
    }
};

class MockMySqlHandlerWrapper : public MySqlHandlerWrapper
{
public:
    MOCK_METHOD1(init, void(const map<string, string> &dbConfig));
    MOCK_METHOD1(execute, void(const string &sql));
    MOCK_METHOD0(disconnect, void());
    MOCK_METHOD1(queryRecord, tars::TC_Mysql::MysqlData(const string &sql));
    MOCK_METHOD3(updateRecord,
                 size_t(const string &tableName,
                        const map<string, pair<tars::TC_Mysql::FT, string>> &columns,
                        const string condition));
    MOCK_METHOD2(insertRecord,
                 size_t(const string &tableName,
                        const map<string, pair<tars::TC_Mysql::FT, string>> &columns));
    MOCK_METHOD2(deleteRecord, size_t(string, string));
    MOCK_METHOD0(lastInsertID, long());
    MOCK_METHOD2(buildInsertSQL,
                 string(const string &tableName,
                        const map<string, pair<tars::TC_Mysql::FT, string>> &columns));
    MOCK_METHOD3(buildUpdateSQL,
                 string(const string &tableName,
                        const map<string, pair<tars::TC_Mysql::FT, string>> columns,
                        const string &condition));
    MOCK_METHOD0(getAffectedRows, size_t());
};

class MockDbHandle : public DbHandle
{
public:
    MOCK_METHOD5(init,
                 void(int64_t reloadTime,
                      std::unique_ptr<MySqlHandlerWrapper>,
                      std::unique_ptr<MySqlHandlerWrapper>,
                      std::unique_ptr<MySqlHandlerWrapper>,
                      PropertyReportPtr report));
    MOCK_METHOD0(initCheck, void());
    MOCK_METHOD0(checkRoute, int());
    MOCK_METHOD0(loadRouteToMem, int());
    MOCK_METHOD1(loadRouteToMem, int(const string &moduleName));
    MOCK_METHOD0(reloadRouter, int());
    MOCK_METHOD1(reloadRouter, int(const string &moduleName));
    MOCK_METHOD1(reloadConf, void(const RouterServerConfig &conf));
    MOCK_METHOD4(getRouterInfo,
                 int(const string &moduleName,
                     int &version,
                     vector<TransferInfo> &transferingInfoList,
                     PackTable &packTable));
    MOCK_METHOD2(getRouterInfo, int(const string &sModuleName, PackTable &packTable));
    MOCK_METHOD1(getVersion, int(const string &sModuleName));
    MOCK_METHOD1(getSwitchType, int(const string &sModuleName));
    MOCK_METHOD2(getPackTable, int(const string &sModuleName, PackTable &packTable));
    MOCK_METHOD1(getIdcMap, int(map<string, string> &idcMap));
    MOCK_METHOD2(getMasterIdc, int(const string &sModuleName, string &idc));
    MOCK_METHOD2(getTransferingInfos, int(const string &sModuleName, ModuleTransferingInfo &info));
    MOCK_METHOD1(getTransferInfos, int(map<string, map<string, TransferInfo>> &info));
    MOCK_METHOD0(clearTransferInfos, bool());
    MOCK_METHOD1(getModuleInfos, int(map<string, ModuleInfo> &info));
    MOCK_METHOD1(getModuleList, int(vector<string> &moduleList));
    MOCK_METHOD0(hasTransfering, bool());
    MOCK_METHOD1(hasTransfering, bool(const string &sModuleName));
    MOCK_METHOD2(hasTransferingLoc, bool(const string &sModuleName, const string &sGroupName));
    MOCK_METHOD1(hasTransfering, bool(const TransferInfo &transferInfo));
    MOCK_METHOD2(hasTransfering, bool(const string &sModuleName, pthread_t threadId));
    MOCK_METHOD1(insertRouterRecord, int(const vector<RecordInfo> &addRecords));
    MOCK_METHOD1(insertRouterRecord, int(const RecordInfo &record));
    MOCK_METHOD1(changeDbRecord, int(const TransferInfo &transferInfo));
    MOCK_METHOD1(changeDbRecord2, int(const TransferInfo &transferInfo));
    MOCK_METHOD1(modifyMemRecord, int(const TransferInfo &transferInfo));
    MOCK_METHOD1(modifyMemRecord2, int(const TransferInfo &transferInfo));
    MOCK_METHOD1(defragDbRecord, int(const TransferInfo &transferInfo));
    MOCK_METHOD3(defragDbRecord,
                 int(const string &sModuleName,
                     vector<RecordInfo> &vOldRecord,
                     vector<RecordInfo> &vNewRecord));
    MOCK_METHOD3(defragDbRecord,
                 int(string &moduleNames,
                     map<string, vector<RecordInfo>> &mOldRecord,
                     map<string, vector<RecordInfo>> &mNewRecord));
    MOCK_METHOD1(defragMemRecord, int(const TransferInfo &transferInfo));
    MOCK_METHOD1(addMemTransferRecord, int(const TransferInfo &transferInfo));
    MOCK_METHOD7(updateTransferingRecord,
                 int(TransferInfo, TransferInfo, bool, bool, bool, bool, int));
    MOCK_METHOD3(updateTransferingRecord, int(string, string, string));
    MOCK_METHOD1(writeDbTransferRecord, int(TransferInfo &transferInfo));
    MOCK_METHOD2(removeMemTransferRecord,
                 int(const TransferInfo &transferInfo, bool &bModuleComplete));
    MOCK_METHOD1(updateVersion, int(const string &sModuleName));
    MOCK_METHOD3(setTransferStatus,
                 int(const TransferInfo &transferInfo, eTransferStatus eway, const string &remark));
    MOCK_METHOD1(setStartTransferStatus, int(const TransferInfo &transferInfo));
    MOCK_METHOD1(getTransferTask, int(TransferInfo &transferInfo));
    MOCK_METHOD5(switchMasterAndSlaveInDbAndMem,
                 int(const string &moduleName,
                     const string &groupName,
                     const string &lastMaster,
                     const string &lastSlave,
                     PackTable &packTable));
    MOCK_METHOD4(switchRWDbAndMem,
                 int(const string &moduleName,
                     const string &groupName,
                     const string &lastMaster,
                     const string &lastSlave));
    MOCK_METHOD2(switchReadOnlyInDbAndMem, int(const string &moduleName, const string &groupName));
    MOCK_METHOD3(switchMirrorInDbAndMem,
                 int(const string &moduleName, const string &groupName, const string &serverName));
    MOCK_METHOD3(recoverMirrorInDbAndMem,
                 int(const string &moduleName, const string &groupName, const string &serverName));
    MOCK_METHOD4(setServerstatus,
                 int(const string &moduleName,
                     const string &groupName,
                     const string &serverName,
                     const int iStatus));
    MOCK_METHOD6(switchMirrorInDbAndMemByIdc,
                 int(const string &moduleName,
                     const string &groupName,
                     const string &idc,
                     bool autoSwitch,
                     string &masterImage,
                     string &slaveImage));
    MOCK_METHOD0(setForbidReload, void());
    MOCK_METHOD0(clearForbidReload, void());
    MOCK_METHOD0(loadSwitchInfo, int());
    MOCK_METHOD1(loadSwitchInfo, int(const string &moduleName));
    MOCK_METHOD2(updateStatusToRelationDB, void(const string &serverName, const string &status));
    MOCK_METHOD8(insertSwitchInfo,
                 int(const string &moduleName,
                     const string &groupName,
                     const string &masterServer,
                     const string &slaveServer,
                     const string &mirrorIdc,
                     const string &mirrorServer,
                     const int switchType,
                     long &id));
    // gmock不支持大于10个参数以上的函数
    // MOCK_METHOD13(insertSwitchInfo,
    //               int(const string &moduleName,
    //                   const string &groupName,
    //                   const string &masterServer,
    //                   const string &slaveServer,
    //                   const string &mirrorIdc,
    //                   const string &masterMirror,
    //                   const string &slaveMirror,
    //                   const string &switchProperty,
    //                   const string &comment,
    //                   const int switchType,
    //                   const int switchResult,
    //                   const int groupSatus,
    //                   const time_t switchBeginTime));
    MOCK_METHOD6(updateSwitchInfo, void(long, int, string, int, string, string));
    MOCK_METHOD6(updateSwitchMirrorInfo, void(long, int, string, int, string, string));
    MOCK_METHOD5(updateSwitchGroupStatus,
                 void(const string &moduleName,
                      const string &groupName,
                      const string &sIdc,
                      const string &comment,
                      int iGroupStatus));
    MOCK_METHOD2(updateSwitchGroupStatus, void(long iID, int iGroupStatus));
    MOCK_METHOD5(getPackTable4SwitchRW,
                 int(const string &moduleName,
                     const string &groupName,
                     const string &lastMaster,
                     const string &lastSlave,
                     PackTable &tmpPackTable));
    MOCK_METHOD0(getDBModuleList, vector<ModuleInfo>());
    MOCK_METHOD1(getDBModuleList, ModuleInfo(const string &moduleName));
    MOCK_METHOD0(getDBRecordList, vector<RecordInfo>());
    MOCK_METHOD1(getDBRecordList, vector<RecordInfo>(const string &moduleName));
    MOCK_METHOD0(getDBGroupList, vector<GroupInfo>());
    MOCK_METHOD1(getDBGroupList, vector<GroupInfo>(const string &moduleName));
    // The following line won't really compile, as the return
    // type has multiple template arguments.  To fix it, use a
    // typedef for the return type.
    MOCK_METHOD0(getDBServerList, map<string, ServerInfo>());
    // The following line won't really compile, as the return
    // type has multiple template arguments.  To fix it, use a
    // typedef for the return type.
    MOCK_METHOD1(getDBServerList, map<string, ServerInfo>(const string &moduleName));
    MOCK_METHOD2(addMirgrateInfo, int(const string &strIp, const string &strReason));
    MOCK_METHOD2(getAllServerInIp, int(const string &strIp, vector<string> &vServerName));
    MOCK_METHOD2(doTransaction, bool(const vector<string> &vSqlSet, string &sErrMsg));
    MOCK_METHOD1(addMemTransferMutexCond, int(const TransferInfo &transferInfo));
    MOCK_METHOD1(removeMemTransferMutexCond, int(const TransferInfo &transferInfo));
    MOCK_METHOD4(getTransferMutexCond,
                 int(const string &moduleName,
                     const string &threadID,
                     pthread_mutex_t **pMutex,
                     pthread_cond_t **pCond));
    MOCK_METHOD1(getTransferMutexCond,
                 int(map<string, map<string, TransferMutexCondPtr>> &mMutexCond));
    MOCK_METHOD2(checkServerOffline, int(const string &serverName, bool &bOffline));
    MOCK_METHOD0(downgrade, void());
};
