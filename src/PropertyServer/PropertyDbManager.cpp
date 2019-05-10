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
#include "PropertyDbManager.h"
#include <sstream>
#include "util/tc_config.h"
#include "PropertyServer.h"
#include "PropertyHashMap.h"
#include "CacheInfoManager.h"
#include "PropertyReapThread.h"

void PropertyDbManager::init(TC_Config& conf)
{
    TLOGDEBUG("PropertyDbManager::init begin ..." << endl);

    int proFieldNum = TC_Common::strto<int>(conf["/Main/DB<PropertyFieldNum>"]);
    _sql = "CREATE TABLE `${TABLE}` ( "
           " `stattime` timestamp NOT NULL default  CURRENT_TIMESTAMP, "
           " `f_date` date NOT NULL default '1970-01-01', "
           " `f_tflag` varchar(8) NOT NULL default '', "
           " `app_name` varchar(20) default NULL, "
           " `module_name` varchar(50) default NULL, "
           " `group_name` varchar(100) default NULL, "
           " `idc_area` varchar(10) default NULL, "
           " `server_status` varchar(10) default NULL, "
           " `master_name` varchar(128) NOT NULL default '', "
           " `master_ip` varchar(16) default NULL, "
           " `set_name` varchar(15) NOT NULL default '', "
           " `set_area` varchar(15) NOT NULL default '', "
           " `set_id` varchar(15) NOT NULL default  '', ";
    for (int i = 1; i <= proFieldNum; i++)
    {
        _sql += "`value";
        _sql += TC_Common::tostr(i);
        _sql += "` varchar(255) default NULL,";
    }
    _sql += " KEY(`f_date`,`f_tflag`,`master_name`,`master_ip`), "
            " KEY `IDX_MASTER_NAME` (`master_name`), "
            " KEY `IDX_MASTER_IP` (`master_ip`), "
            " KEY `IDX_TIME` (`stattime`), "
            " KEY `IDX_F_DATE` (`f_date`) "
            " ) ENGINE=MyISAM DEFAULT CHARSET=gbk";

    _tbNamePre      = conf.get("/Main/DB<TbNamePre>", "t_property_realtime_");
    _maxInsertCount = TC_Common::strto<int>(conf.get("/Main/DB<MaxInsertCount>", "1000"));
    _appNames		= TC_Common::sepstr<string>(conf.get("/Main/DB<AppName>", "t_property_realtime"), "|", false);
    _terminate      = false;

    _sqlStatus = "CREATE TABLE `t_ecstatus` ( "
            "  `id` int(11) NOT NULL auto_increment, "
            "  `appname` varchar(64) NOT NULL default '', "
            "  `action` tinyint(4) NOT NULL default '0', "
            "  `checkint` smallint(6) NOT NULL default '10', "
            "  `lasttime` varchar(16) NOT NULL default '', "
            "  `thestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
            "   PRIMARY KEY  (`appname`,`action`), "
            "   UNIQUE KEY `id` (`id`) "
            " ) ENGINE=InnoDB DEFAULT CHARSET=gbk";

    string sCutType = conf.get("/Main/DB<CutType>", "day");
    if (sCutType == "day")
    {
        _cutType = CUT_BY_DAY;
    }
    else if (sCutType == "minute")
    {
        _cutType = CUT_BY_MINUTE;
    }
    else
    {
        _cutType = CUT_BY_HOUR;
    }

    // 初始化db连接
    TC_DBConf tcDBConf;
    tcDBConf.loadFromMap(conf.getDomainMap("/Main/DB/property"));
    _mysql.init(tcDBConf);

    _ipAndPort  = "ip:";
    _ipAndPort += tcDBConf._host;
    _ipAndPort += "|port:";
    _ipAndPort += TC_Common::tostr(tcDBConf._port);

    TLOGDEBUG("PropertyDbManager::init ok" << endl);
}

int PropertyDbManager::insert2Db(const PropertyMsg &mPropMsg, const string &sDate, const string &sFlag)
{
    string sLastTime  = sDate + " " + sFlag;

    string sTbNamePre = "";

    TLOGDEBUG("PropertyDbManager::insert2Db begin ..." << endl);

    try
    {
        int iOldWrite = 0;
        int iNowWrite = 0;

        int64_t iBegin = TNOWMS;

        if (insert2Db(mPropMsg, sDate, sFlag, sTbNamePre, iOldWrite, iNowWrite) != 0)
        {
            if (_terminate)
            {
                return -1;
            }

            iOldWrite = iNowWrite;
            iNowWrite = 0;

            if (insert2Db(mPropMsg ,sDate, sFlag, sTbNamePre, iOldWrite, iNowWrite) != 0)
            {
                if (_terminate)
                {
                    return -1;
                }

                string sMsg("insert2Db_");
                sMsg += getIpAndPort();

                sendAlarmSMS(sMsg);
            }
            else
            {
                if (updateEcsStatus(sLastTime, sTbNamePre) != 0)
                {
                    string sMsg("updateEcsStatus_");
                    sMsg += getIpAndPort();

                    sendAlarmSMS(sMsg);
                }
            }
        }
        else
        {
            if (updateEcsStatus(sLastTime, sTbNamePre) != 0)
            {
                string sMsg("updateEcsStatus_");
                sMsg += getIpAndPort();

                sendAlarmSMS(sMsg);
            }
        }

        int64_t iEnd = TNOWMS;

        TLOGDEBUG("PropertyDbManager::insert2Db|" << getIpAndPort() << "|" << sDate << "|" << sFlag << "|" << mPropMsg.size() << "|" << (iEnd - iBegin) << endl);
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR("PropertyDbManager::insert2Db TC_Mysql_Exception: " << ex.what() << endl);
    }
    catch (exception &ex)
    {
        TLOGERROR("PropertyDbManager::insert2Db exception: " << ex.what() << endl);
    }

    return 0;
}

int PropertyDbManager::queryPropData(const DCache::QueryPropCond &req, vector<DCache::QueriedResult> &rsp)
{
    int ret = 0;
    try
    {
        string lastDate, lastTime;
        ret = getLastTimestamp(lastDate, lastTime);
        if (ret != 0)
            return -1;
        
        bool getSpecificServer = false;
        bool getAllServer = false;
        if (!req.serverName.empty())
        {
            if (req.serverName == "*")
                getAllServer = true;
            else
                getSpecificServer = true;
        }

        string sTbName = _tbNamePre;

        stringstream sql_stream;
        sql_stream << "select `f_tflag`";
        auto nameMap = g_app.getPropertyNameMap();
        for (auto &&prop : nameMap)
        {
            sql_stream << ",`" << prop.second << "`";
        }
        if (getAllServer || getSpecificServer)
            sql_stream << ",`master_name`";

        sql_stream << " from " << sTbName
                   << " where module_name ='" << req.moduleName << "'";
        if (getSpecificServer)
            sql_stream << " and master_name='" << req.serverName << "'";
        
        if (!getSpecificServer && !getAllServer)    //模块整体统计，只计算主cache
            sql_stream << " and server_status='M'";

        for (const auto &date : req.date)
        {
            stringstream exsql;
            exsql << sql_stream.str();
            exsql << " and f_date=" << date;

            if (date == lastDate)
            {
                if(req.startTime > lastTime)
                    continue;
                else
                    exsql << " and f_tflag >='" << req.startTime << "'";
                
                if (req.endTime > lastTime)
                    exsql << " and f_tflag <='" << lastTime << "'";
                else
                    exsql << " and f_tflag <='" << req.endTime << "'";
            }
            else if (date < lastDate)
            {
                exsql << " and f_tflag >='" << req.startTime << "' and f_tflag <='" << req.endTime << "'";
            }
            else
                continue;

            if (getSpecificServer)
                exsql << " order by f_tflag";
            else
                exsql << " order by master_name, f_tflag";

            TC_Mysql::MysqlData recordSet;
            recordSet = _mysql.queryRecord(exsql.str());

            //handle result
            {
                if (getSpecificServer)
                {
                    QueriedResult result;
                    result.moduleName = req.moduleName;
                    result.serverName = req.serverName;
                    result.date = date;

                    size_t recordNum = recordSet.size();
                    vector<map<string, string>> &recordData = recordSet.data();
                    for (size_t i = 0; i < recordNum; i++)
                    {
                        map<string, string> &record = recordData[i];
                        QueriedProp p;
                        p.timeStamp = record["f_tflag"];
                        for (auto && prop : nameMap)
                        {
                            //FIXME, how to handle NULL?
                            p.propData[prop.first] = TC_Common::strto<double>(record[prop.second]);
                        }
                        result.data.emplace_back(std::move(p));
                    }
 
                    rsp.emplace_back(std::move(result));
                }
                else if (getAllServer)
                {
                    map<string, QueriedProp> svrSum;
                    size_t recordNum = recordSet.size();
                    vector<map<string, string>> &recordData = recordSet.data();
                    for (size_t i = 0; i < recordNum; i++)
                    {
                        map<string, string> &record = recordData[i];
                        string svrName = record["master_name"];
                        auto iret = svrSum.insert({svrName, QueriedProp()});
                        auto itsvr = iret.first;
                        for (auto && prop : nameMap)
                        {
                            //FIXME, how to handle avg/max/min
                            itsvr->second.propData[prop.first] += TC_Common::strto<double>(record[prop.second]);
                        }
                    }

                    for (auto && svr : svrSum)
                    {
                        QueriedResult result;
                        result.moduleName = req.moduleName;
                        result.serverName = svr.first;
                        result.date = date;
                        result.data.emplace_back(std::move(svr.second));
                        
                        rsp.emplace_back(std::move(result));
                    }

                }
                else
                {
                    //module's statistic
                    QueriedResult result;
                    result.moduleName = req.moduleName;
                    result.serverName.clear();
                    result.date = date;

                    map<string, QueriedProp> timeSum;
                    size_t recordNum = recordSet.size();
                    vector<map<string, string>> &recordData = recordSet.data();
                    for (size_t i = 0; i < recordNum; i++)
                    {
                        map<string, string> &record = recordData[i];
                        string time = record["f_tflag"];
                        auto iret = timeSum.insert({time, QueriedProp()});
                        auto ittime = iret.first;
                        ittime->second.timeStamp = time;
                        for (auto &&prop : nameMap)
                        {
                            //FIXME, how to handle NULL?
                            //FIXME, how to handle avg/max/min
                            ittime->second.propData[prop.first] += TC_Common::strto<double>(record[prop.second]);
                        }
                    }

                    for (auto && t : timeSum)
                    {
                        result.data.emplace_back(std::move(t.second));
                    }
                    rsp.emplace_back(std::move(result));
                }

            }
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR(__FUNCTION__ << "|sql:" << _mysql.getLastSQL() << "|exception: " << ex.what() << endl);
        ret = -1;
    }
    catch (std::exception &ex)
    {
        TLOGERROR(__FUNCTION__ << "|exception: " << ex.what() << endl);
        ret = -1;
    }
    catch (...)
    {
        TLOGERROR(__FUNCTION__ << "|unknow exception:" << endl);
        ret = -1;
    }

    return ret;	
}

int PropertyDbManager::insert2Db(const PropertyMsg &mPropMsg, const string &sDate, const string &sFlag, const string &sTbNamePre, int iOldWriteNum, int &iNowWriteNum)
{
    int iTotalCount = 0, iValidCount = 0;
    int iHasWriteNum = iOldWriteNum;
    ostringstream osSqlPre, osSql;

    iNowWriteNum = iOldWriteNum;

    string sTbName = (sTbNamePre !="" ? sTbNamePre: _tbNamePre);

    try
    {
        createTable(sTbName);

        osSqlPre << "insert ignore into " + sTbName + " (f_date,f_tflag,app_name,module_name,group_name,idc_area,server_status,master_name,master_ip,set_name,set_area,set_id";
        for(size_t i = 1; i <= g_app.getPropertyNameMap().size(); ++i)
        {
            osSqlPre << ",value" + TC_Common::tostr(i);
        }
        osSqlPre << ") values ";

        for (PropertyMsg::const_iterator it = mPropMsg.begin(); it != mPropMsg.end(); ++it)
        {
            if(_terminate)
            {
                return -1;
            }

            if(iHasWriteNum > 0)
            {
                --iHasWriteNum;
                continue;
            }

            ++iTotalCount;
            
            string sValues = createDBValue(it->first, it->second, sDate, sFlag);
            if (sValues == "")
            {
                continue;
            }

            if (iValidCount == 0)
            {
                osSql.str("");
                osSql << osSqlPre.str() + sValues;
            }
            else
            {
                osSql << "," + sValues;
            }

            if ((++iValidCount) >= _maxInsertCount)
            {
                usleep(100);
                _mysql.execute(osSql.str());    
                iNowWriteNum += iTotalCount;
                iTotalCount = iValidCount = 0;
            }
        }

        if (iValidCount != 0)
        {
            _mysql.execute(osSql.str());
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        string err = string(ex.what());
        if (std::string::npos == err.find("Duplicate"))
        {
            createTable(sTbName);
        }
        //因为会输出1千条记录，这里做截取
        TLOGERROR("PropertyDbManager::insert2Db exception: " << err.substr(0,64) << endl);
        return 1;
    }
    catch (exception &ex)
    {
        TLOGERROR("PropertyDbManager::insert2Db exception: " << ex.what() << endl);
        return 1;
    }

    return 0;
}

bool PropertyDbManager::hasTableExist(const string &sTbName)
{
    try
    {
        TC_Mysql::MysqlData tTotalRecord = _mysql.queryRecord("show tables like '%" + sTbName + "%'");
        TLOGINFO("PropertyDbManager::hasTableExist|show tables like '%" << sTbName << "%|affected:" << tTotalRecord.size() << endl);
        if (tTotalRecord.size() > 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    catch(TC_Mysql_Exception &ex)
    {
        TLOGERROR("PropertyDbManager::hasTableExist exception:" << ex.what() << endl);
    }

    return false;
}

int PropertyDbManager::createTable(const string &sTbName)
{
    try
    {
        if (!hasTableExist(sTbName))
        {
            string sSql = TC_Common::replace(_sql, "${TABLE}", sTbName);
            TLOGINFO("PropertyDbManager::createTable " << sSql << endl);
            _mysql.execute(sSql);
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR("PropertyDbManager::createTable exception: " << ex.what() << endl);
        return 1;
    }

    return 0;	
}

int PropertyDbManager::createEcsTable(const string &sTbName, const string &sSql)
{
    try
    {
        if (!hasTableExist(sTbName))
        {
            TLOGDEBUG("PropertyDbManager::createEcsTable " << sSql << endl);
            _mysql.execute(sSql);
        }
    }
    catch (TC_Mysql_Exception& ex)
    {
        TLOGERROR("PropertyDbManager::createEcsTable exception: " << ex.what() << endl);
        return 1;
    }

    return 0;
}

int PropertyDbManager::updateEcsStatus(const string &sLastTime, const string &sTbNamePre)
{
    try
    {
        for (size_t i = 0; i < _appNames.size(); ++i)
        {
            string sCondition = "where appname='" + _appNames[i] + "' and action=0";
            
            TC_Mysql::RECORD_DATA rd;
            rd["lasttime"] = make_pair(TC_Mysql::DB_STR, sLastTime);
            
            int iRet = _mysql.updateRecord("t_ecstatus", rd, sCondition);
            
            TLOGDEBUG("PropertyDbManager::updateEcsStatus iRet: " <<iRet <<"|" << _mysql.getLastSQL() << endl);

            if (iRet == 0 )
            {
                rd["appname"]  = make_pair(TC_Mysql::DB_STR, _appNames[i]);
                rd["checkint"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(g_app.getInsertInterval()));
                rd["lasttime"] = make_pair(TC_Mysql::DB_STR, sLastTime);
                iRet = _mysql.replaceRecord("t_ecstatus", rd);
            }

            if (iRet != 1)
            {
                TLOGERROR("PropertyDbManager::updateEcsStatus erro: ret:" << iRet << "\n" << _mysql.getLastSQL() << endl);
            }
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR("PropertyDbManager::updateEcsStatus exception: " << ex.what() << endl);
        createEcsTable("t_ecstatus", _sqlStatus);
        return 1;
    }

    return 0;
}

int PropertyDbManager::getLastTimestamp(string &date, string &time)
{
    int ret = 0;
    try
    {
        stringstream sql_stream;
        sql_stream << "select * from t_ecstatus";

        TC_Mysql::MysqlData recordSet;
        recordSet = _mysql.queryRecord(sql_stream.str());
        size_t recordNum = recordSet.size();
		vector<map<string, string> > &recordData = recordSet.data();
        if (recordNum > 0)
        {
            map<string, string>& record = recordData[0];
            vector<string> vtmp = TC_Common::sepstr<string>(TC_Common::trim(record["lasttime"]), " ");
            if (vtmp.size() != 2)
            {
                TLOGERROR(__FUNCTION__ << "|parse lasttime error: lasttime:" << record["lasttime"] << endl);
                return -1;
            }

            date = TC_Common::trim(vtmp[0]);
            time = TC_Common::trim(vtmp[1]);
        }
        else
        {
            TLOGERROR(__FUNCTION__ << "|no data" << endl);
            return -1;
        }

        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR(__FUNCTION__ << "|exception: " << ex.what() << endl);
        ret = -1;
    }
    catch (std::exception &ex)
    {
        TLOGERROR(__FUNCTION__ << "|exception: " << ex.what() << endl);
        ret = -1;
    }
    catch (...)
    {
        TLOGERROR(__FUNCTION__ << "|unknow exception" << endl);
        ret = -1;
    }
    

    return ret;
}

string PropertyDbManager::createDBValue(const PropKey &key, const PropValue &value, const string &sDate, const string &sFlag)
{
    stringstream os;

    vector<string> v = TC_Common::sepstr<string>(key.moduleName, ".");
    size_t iSize = v.size();
    if (iSize < 2)
    {
        TLOGERROR("PropertyDbManager::createDBValue invalid server name:" << key.moduleName << endl);
        return "";
    }

    string sServerName;
    for(size_t i = 1; i < (iSize - 1); ++i)
    {
        sServerName += v[i] + ".";
    }
    sServerName += v[iSize - 1];
    
    CacheModuleInfo stInfo;
    if (CacheInfoManager::getInstance()->getModuleInfo(sServerName, stInfo))
    {
        os << "('" << sDate
            << "','" << sFlag
            << "','" << _mysql.escapeString(stInfo._appName)	  
            << "','" << _mysql.escapeString(stInfo._moduleName)
            << "','" << _mysql.escapeString(stInfo._groupName)
            << "','" << _mysql.escapeString(stInfo._idcArea) 
            << "','" << _mysql.escapeString(stInfo._serverStatus)
            << "','" << _mysql.escapeString(key.moduleName)
            << "','" << _mysql.escapeString(key.ip)
            << "','" << _mysql.escapeString(key.setName)
            << "','" << _mysql.escapeString(key.setArea) 
            << "','" << _mysql.escapeString(key.setID)
            << "'";
    }
    else
    {
        os<<"('" << sDate
            << "','" << sFlag
            << "',"  << "NULL"
            << ","	 << "NULL"
            << ","	 << "NULL"
            << ","	 << "NULL"
            << ","	 << "NULL"
            << ",'"  << _mysql.escapeString(key.moduleName)
            << "','" << _mysql.escapeString(key.ip)
            << "','" << _mysql.escapeString(key.setName)
            << "','" << _mysql.escapeString(key.setArea)
            << "','" << _mysql.escapeString(key.setID)
            << "'";
        TLOGERROR("PropertyDbManager::createDBValue no module info for server:" << key.moduleName << endl);
    }

    size_t iPropertyNum = g_app.getPropertyNameMap().size(); 
    for (size_t i = 1; i <= iPropertyNum; ++i)
    {
        string sPropertyName = "property" + TC_Common::tostr<size_t>(i);
        map<std::string, DCache::StatPropInfo>::const_iterator it = value.statInfo.find(sPropertyName);
        if (it == value.statInfo.end())
        {
            os << ",NULL";
            continue;
        }

        if (it->second.policy == "Avg")
        {
            vector<string> vTmp = TC_Common::sepstr<string>(it->second.value, "=");
            double avg = 0;
            if(2 == vTmp.size() && TC_Common::strto<long>(vTmp[1]) != 0)
            {
                avg = TC_Common::strto<double>(vTmp[0]) / TC_Common::strto<long>(vTmp[1]);
            }
            else
            {
                avg = TC_Common::strto<double>(vTmp[0]);
            }


            os << ",'" << _mysql.escapeString(TC_Common::tostr(avg)) << "'";
        }
        else
        {
            os << ",'" << _mysql.escapeString(it->second.value) << "'";
        }
    }
    os << ")";
    
    TLOGDEBUG("PropertyDbManager::creatDBValue " << os.str() << endl);
    return os.str();
}

string PropertyDbManager::getIpAndPort()
{
    return _ipAndPort;
}

int PropertyDbManager::sendAlarmSMS(const string &sMsg)
{
    string errInfo = "ERROR:" + ServerConfig::LocalIp + "_" + sMsg +  ":统计入库失败，请及时处理!";
    TARS_NOTIFY_ERROR(errInfo);

    TLOGERROR("TARS_NOTIFY_ERROR " << errInfo << endl);

    return 0;
}
