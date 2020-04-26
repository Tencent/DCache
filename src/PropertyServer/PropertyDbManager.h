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
#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include "util/tc_common.h"
#include "util/tc_thread.h"
#include "util/tc_option.h"
#include "util/tc_file.h"
#include "util/tc_mysql.h"
#include "util/tc_config.h"
#include "servant/Application.h"
#include "jmem/jmem_hashmap.h"
#include "Property.h"
#include "PropertyHashMap.h"

using namespace tars;

class PropertyDbManager : public TC_Singleton<PropertyDbManager>, public TC_ThreadMutex 
{
public:
    enum CutType
    {
        CUT_BY_DAY      = 0,
        CUT_BY_HOUR     = 1,
        CUT_BY_MINUTE   = 2,
    };

public:
   
    /**
     * 初始化
     *
     * @return void
     */
    void init(TC_Config& conf);

    /**
     * 将特性上报信息写入db
     *
     * @return int
     */
    int insert2Db(const PropertyMsg &mPropMsg, const string &sDate, const string &sFlag);

    int queryPropData(const DCache::QueryPropCond &req, vector<DCache::QueriedResult> &rsp);
    /**
     * 设置结束标志
     *
     * @return void
     */
    void setTerminateFlag(bool bFlag) {_terminate = bFlag;}

private:

    int createTable(const string &sTbName);

    int updateEcsStatus(const string &sLastTime, const string &sTbNamePre = "");
    int getLastTimestamp(string &date, string &time);

    int createEcsTable(const string &sTbName, const string &sSql);
    
    int insert2Db(const PropertyMsg &mPropMsg, const string &sDate, const string &sFlag, const string &sTbNamePre, int iOldWriteNum, int &iNowWriteNum);

    string createDBValue(const PropKey &key, const PropValue &value, const string &sDate, const string &sFlag);

    bool hasTableExist(const string &sTbName);

    string getIpAndPort();

    int sendAlarmSMS(const string &sMsg);

private:

    string              _sql;              // 创建表
    string              _sqlStatus;        // 创建表t_ecstatus
    string              _tbNamePre;        // 表前缀
    string              _ipAndPort;
    int                 _maxInsertCount;   // 一次最大插入条数
    vector<string>      _appNames;
    CutType             _cutType;          // 分表类型
    TC_Mysql            _mysql;
    bool                _terminate;
};

#endif


