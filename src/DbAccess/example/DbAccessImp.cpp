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
#include "DbAccessImp.h"

////////////////////////////////////////////////////////
void DbAccessImp::initialize()
{
    TC_Config tcConf;
    tcConf.parseFile(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");
    _selectLimit = TC_Common::strto<size_t>(tcConf.get("/main/<SelectLimit>", "100000"));

    //type 1: KVCache, 2:MKVCache
    int cacheType = TC_Common::strto<int>(tcConf["/main/<CacheType>"]);
    _isMKDBAccess = cacheType == 2 ? true : false;

    if (!_isMKDBAccess)
    {
        _sigKeyNameInDB = tcConf["/main/KV/<KeyNameInDB>"];
        _sigValueNameInDB = tcConf["/main/KV/<ValueNameInDB>"];
    }


    _confDB.loadFromMap(tcConf.getDomainMap("/main/DBInfo/"));
    _mysqlDB = new TC_Mysql();
    _mysqlDB->init(_confDB);
    _tableName = tcConf["/main/<TableName>"];
}

void DbAccessImp::destroy()
{
    delete _mysqlDB;
}

tars::Int32 DbAccessImp::get(const std::string & keyItem, std::string &value, tars::Int32 &expireTime, tars::TarsCurrentPtr current)
{
    int iRet = eDbUnknownError;

    try
    {
        string sSql;

        sSql = "select * from " + _tableName + " where `" + _sigKeyNameInDB + "`='" + _mysqlDB->escapeString(keyItem) + "'";

        TC_Mysql::MysqlData recordSet = _mysqlDB->queryRecord(sSql);

        if (recordSet.size() == 0)
        {
            return eDbRecordNotExist;
        }
        TC_Mysql::MysqlRecord record = recordSet[0];

        expireTime = TC_Common::strto<tars::Int32>(record["sDCacheExpireTime"]);

        value = record[_sigValueNameInDB];

        iRet = eDbSucc;
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR(_mysqlDB->getLastSQL() << endl);
        TLOGERROR(ex.what() << endl);
        iRet = eDbError;
    }
    catch (std::exception &ex)
    {
        TLOGERROR(ex.what() << endl);
        iRet = eDbUnknownError;
    }
    return iRet;
}


tars::Int32 DbAccessImp::set(const std::string & keyItem, const std::string & value, tars::Int32 expireTime, tars::TarsCurrentPtr current)
{
    int iRet = eDbUnknownError;

    try
    {
        string sSql;
        TC_Mysql::RECORD_DATA updateData;

        updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_STR, keyItem);

        updateData[_sigValueNameInDB] = make_pair(TC_Mysql::DB_STR, value);

        updateData["sDCacheExpireTime"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(expireTime));

        int affect = 0;
        affect = _mysqlDB->replaceRecord(_tableName, updateData);
        iRet = eDbSucc;
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR(_mysqlDB->getLastSQL() << endl);
        TLOGERROR(ex.what() << endl);
        iRet = eDbError;
    }
    catch (std::exception &ex)
    {
        TLOGERROR(ex.what() << endl);
        iRet = eDbUnknownError;
    }
    return iRet;
}

tars::Int32 DbAccessImp::del(const std::string & keyItem, tars::TarsCurrentPtr current)
{
    int iRet = eDbUnknownError;

    try
    {
        string sCondition = "where `" + _sigKeyNameInDB + "`='" + _mysqlDB->escapeString(keyItem) + "'";
        int affect = 0;
        affect = _mysqlDB->deleteRecord(_tableName, sCondition);
        iRet = eDbSucc;
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR(_mysqlDB->getLastSQL() << endl);
        TLOGERROR(ex.what() << endl);
        iRet = eDbError;
    }
    catch (std::exception &ex)
    {
        TLOGERROR(ex.what() << endl);
        iRet = eDbUnknownError;
    }
    return iRet;

}


tars::Int32 DbAccessImp::select(const string &mainKey, const string &field, const vector<DbCondition> &vtCond,
    vector<map<string, string> > &vtData, tars::TarsCurrentPtr current)
{
    int iRet = eDbUnknownError;

    try
    {
        map<string, string> mKey;

        string sSql = "select ";
        vector<string> vtFields = TC_Common::sepstr<string>(field, ",", false);
        if (vtFields.size() == 0 || (vtFields.size() == 1 && vtFields[0] == "*"))
        {
            // 查询所有字段
            sSql += "*";
        }
        else
        {
            for (size_t i = 0; i < vtFields.size(); i++)
            {
                sSql += "`" + vtFields[i] + "`";
                if (i != vtFields.size() - 1)
                {
                    sSql += ", ";
                }
            }
        }

        sSql += " from " + _tableName + buildConditionSQL(vtCond, _mysqlDB) + " limit " + TC_Common::tostr(_selectLimit);

        TC_Mysql::MysqlData recordSet = _mysqlDB->queryRecord(sSql);

        size_t iRecordSize = recordSet.size();
        if (iRecordSize == 0)
        {
            return 0;
        }

        vtData = recordSet.data();
        TLOGDEBUG(_mysqlDB << "|" << _tableName << "|select|" << int(vtData.size()) << endl);

        return vtData.size();
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR(_mysqlDB->getLastSQL() << endl);
        TLOGERROR(ex.what() << endl);
        iRet = eDbError;
    }
    catch (std::exception &ex)
    {
        TLOGERROR(ex.what() << endl);
        iRet = eDbUnknownError;
    }

    return iRet;
}
tars::Int32 DbAccessImp::replace(const string &mainKey, const map<string, DbUpdateValue> &mpValue,
    const vector<DbCondition> &vtCond, tars::TarsCurrentPtr current)
{
    int iRet = eDbUnknownError;

    try
    {
        map<string, pair<TC_Mysql::FT, string> > mpColumns;
        map<string, DbUpdateValue>::const_iterator it = mpValue.begin();
        while (it != mpValue.end())
        {
            if (it->second.type == INT)
            {
                if (!isDigit(it->second.value))
                {
                    TLOGERROR("replace value type error." << endl);
                    return eDbDecodeError;
                }
            }
            mpColumns[it->first] = make_pair(DT2FT(it->second.type), it->second.value);

            it++;

        }

        int affect = 0;
        affect = _mysqlDB->replaceRecord(_tableName, mpColumns);
        iRet = eDbSucc;
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR(_mysqlDB->getLastSQL() << endl);
        TLOGERROR(ex.what() << endl);
        iRet = eDbError;
    }
    catch (std::exception &ex)
    {
        TLOGERROR(ex.what() << endl);
        iRet = eDbUnknownError;
    }

    return iRet;
}

tars::Int32 DbAccessImp::delCond(const string &mainKey, const vector<DbCondition> &vtCond, tars::TarsCurrentPtr current)
{
    int iRet = eDbUnknownError;

    try
    {
        string sCondition = buildConditionSQL(vtCond, _mysqlDB);
        int affect = 0;
        affect = _mysqlDB->deleteRecord(_tableName, sCondition);
        iRet = eDbSucc;
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR(_mysqlDB->getLastSQL() << endl);
        TLOGERROR(ex.what() << endl);
        iRet = eDbError;
    }
    catch (std::exception &ex)
    {
        TLOGERROR(ex.what() << endl);
        iRet = eDbUnknownError;
    }
    return iRet;
}

string DbAccessImp::OP2STR(Op op)
{
    switch (op)
    {
    case DCache::EQ:
        return "=";
    case DCache::NE:
        return "!=";
    case DCache::GT:
        return ">";
    case DCache::LT:
        return "<";
    case DCache::LE:
        return "<=";
    case DCache::GE:
        return ">=";
    default:
        throw runtime_error("invalid op: " + etos(op));
        break;
    }
}

TC_Mysql::FT DbAccessImp::DT2FT(DataType type)
{
    switch (type)
    {
    case INT:
        return TC_Mysql::DB_INT;
    case STR:
        return TC_Mysql::DB_STR;
    default:
        throw runtime_error("invalid data type: " + etos(type));
        break;
    }
}

string DbAccessImp::buildConditionSQL(const vector<DbCondition> &vtCond, TC_Mysql *pMysql)
{
    string sWhere;
    if (vtCond.size() == 0)
    {
        return sWhere;
    }
    sWhere += " where ";
    for (size_t i = 0; i < vtCond.size(); i++)
    {
        sWhere += "`" + vtCond[i].fieldName + "`" + OP2STR(vtCond[i].op);
        if (vtCond[i].type == INT)
        {
            sWhere += pMysql->escapeString(vtCond[i].value);
        }
        else
        {
            sWhere += "'" + pMysql->escapeString(vtCond[i].value) + "'";
        }
        if (i != vtCond.size() - 1)
        {
            sWhere += " and ";
        }
    }
    return sWhere;
}

bool DbAccessImp::isDigit(const string &key)
{
    string::const_iterator iter = key.begin();

    if (key.empty())
    {
        return false;
    }

    size_t length = key.size();

    size_t count = 0;

    bool bFindNode = false;

    bool bFirst = true;

    while (iter != key.end())
    {
        if (bFirst)
        {
            if (!::isdigit(*iter))
            {
                if (*iter != 45)
                {
                    return false;
                }
                else if (length == 1)
                {
                    return false;
                }
            }
        }
        else
        {
            if (!::isdigit(*iter))
            {
                if (*iter == 46)
                {
                    if ((!bFindNode) && (count != (length - 1)))
                    {
                        bFindNode = true;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
        }

        ++iter;
        count++;
        bFirst = false;
    }

    return true;
}


