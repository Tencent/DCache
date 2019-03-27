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
#ifndef _DBACCESSIMP_H_
#define _DBACCESSIMP_H_

#include <algorithm>
#include "servant/Application.h"
#include "util/tc_config.h"
#include "util/tc_common.h"
#include "util/tc_mysql.h"
#include "DbAccess.h"

using namespace std;
using namespace DCache;
/**
 *
 *
 */
class DbAccessImp : public DCache::DbAccess
{
public:
    /**
     *
     */
    DbAccessImp() {};
    virtual ~DbAccessImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy();

    /**
     *
     */

     //for KV
    virtual tars::Int32 get(const std::string & keyItem, std::string &value, tars::Int32 &expireTime, tars::TarsCurrentPtr current);

    virtual tars::Int32 set(const std::string & keyItem, const std::string & value, tars::Int32 expireTime, tars::TarsCurrentPtr current);//@2016.3.18

    virtual tars::Int32 del(const std::string & keyItem, tars::TarsCurrentPtr current);

    //for MKV
    virtual tars::Int32 select(const std::string & mainKey, const std::string & field, const vector<DCache::DbCondition> & vtCond, vector<map<std::string, std::string> > &vtData, tars::TarsCurrentPtr current);

    virtual tars::Int32 replace(const std::string & mainKey, const map<std::string, DCache::DbUpdateValue> & mpValue, const vector<DCache::DbCondition> & vtCond, tars::TarsCurrentPtr current);

    virtual tars::Int32 delCond(const std::string & mainKey, const vector<DCache::DbCondition> & vtCond, tars::TarsCurrentPtr current);

private:

    string OP2STR(DCache::Op op);
    TC_Mysql::FT DT2FT(DCache::DataType type);
    string buildConditionSQL(const vector<DCache::DbCondition> &vtCond, TC_Mysql *pMysql);
    bool isDigit(const string &key);
private:
    string _sigKeyNameInDB;//一期的主key在DB中的字段名
    string _sigValueNameInDB;

    size_t _selectLimit;
    bool _isMKDBAccess;

    TC_Mysql* _mysqlDB;
    TC_DBConf _confDB;
    string _tableName;
};
/////////////////////////////////////////////////////
#endif

