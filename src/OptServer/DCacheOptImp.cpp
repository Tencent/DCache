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
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <set>

#include "servant/Application.h"
#include "util/tc_common.h"
#include "util/tc_md5.h"

#include "DCacheOptImp.h"
#include "DCacheOptServer.h"
#include "Assistance.h"
#include "ReleaseThread.h"
#include "UninstallThread.h"

using namespace std;

#define MAX_ROUTER_PAGE_NUM 429496
#define TOTAL_ROUTER_PAGE_NUM 429497

std::function<void(const string, const string, const string, const int, const string, const string, const int, bool)> dbaccessFun = std::bind(DCacheOptImp::creatDBAccessTableThread, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8);

//由于dbaccess安装数据库表改成异步，通过这个全局变量获取安装错误信息
string g_dbaccessError;
//dbaccess安装数据库表的互斥锁
TC_ThreadLock g_dbaccessLock;

//升级opt服务
#define CREATE_DBACCESS_CONF "CREATE TABLE `t_dbaccess_app` (   \
  `id` int(11) NOT NULL AUTO_INCREMENT, \
  `dbaccess_name` varchar(128) NOT NULL DEFAULT '', \
  `app_name` varchar(64) NOT NULL DEFAULT '',  \
  PRIMARY KEY (`id`),   \
  UNIQUE KEY `ro_app` (`dbaccess_name`,`app_name`)  \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4; "

extern DCacheOptServer g_app;

void DCacheOptImp::initialize()
{
    // DCacheOpt initialize
    _tcConf.parseFile(ServerConfig::BasePath + "DCacheOptServer.conf");

    //tars db: save router,proxy,cache server info, include: server conf, adapter conf
    map<string, string> tarsConnInfo = _tcConf.getDomainMap("/Main/DB/tars");
    TC_DBConf tarsDbConf;
    tarsDbConf.loadFromMap(tarsConnInfo);
    _mysqlTarsDb.init(tarsDbConf);

    //relation db: save app-router-proxy-cache relation info
    _relationDBInfo = _tcConf.getDomainMap("/Main/DB/relation");
    TC_DBConf relationDbConf;
    relationDbConf.loadFromMap(_relationDBInfo);
    _mysqlRelationDB.init(relationDbConf);

    _communicator = Application::getCommunicator();

    _adminproxy = _communicator->stringToProxy<AdminRegPrx>(_tcConf.get("/Main/<AdminRegObj>", "tars.tarsAdminRegistry.AdminRegObj"));

    _propertyPrx = _communicator->stringToProxy<PropertyPrx>(_tcConf.get("/Main/<PropertyObj>", "DCache.PropertyServer.PropertyObj"));

	_tpool.init(2);
	_tpool.start();

    TLOGDEBUG(FUN_LOG << "initialize succ" << endl);
}

//////////////////////////////////////////////////////
void DCacheOptImp::destroy()
{
    _mysqlTarsDb.disconnect();
    TLOGDEBUG("DCacheOptImp::destroyApp ok" << endl);
}

/*
*  interface function
*/
tars::Int32 DCacheOptImp::installApp(const InstallAppReq & installReq, InstallAppRsp & instalRsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG("begin installApp and app_name=" << installReq.appName << endl);

    try
    {
        const RouterParam & stRouter = installReq.routerParam;
        const ProxyParam & stProxy = installReq.proxyParam;
        string & err = instalRsp.errMsg;

        //创建路由数据库
        if (createRouterDBConf(stRouter, err) != 0)
        {
            err = string("failed to create router DB, errmsg:") + err;
            return -1;
        }

        string::size_type posDot = stRouter.serverName.find_first_of(".");
        if (string::npos == posDot)
        {
            err = "router server name error, eg: DCache.AbcRouterServer";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        //创建router server的配置
        if (createRouterConf(stRouter, false, installReq.replace, err) != 0)
        {
            err = "failed to create router conf";
            return -1;
        }

        posDot = stProxy.serverName.find_first_of(".");
        if (string::npos == posDot)
        {
            err = "proxy server name error, eg: DCache.AbcProxyServer";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        bool bProxyServerExist = hasServer("DCache", stProxy.serverName.substr(posDot + 1));
        //创建proxy的配置 模块名填'';
        if (!bProxyServerExist && createProxyConf("", stProxy, stRouter, installReq.replace, err) != 0)
        {
            err = string("failed to create proxy conf, errmsg:") + err;
            return -1;
        }

        // 这里enableGroup 默认 为true, 采用分组
        if (insertTarsDb(stProxy, stRouter, installReq.version, true, installReq.replace, err) !=0)
        {
            err = string("failed to insert tars server, errmsg:") + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 保存 proxy-app 和 router-app的对应关系
        insertRelationAppTable(installReq.appName, stProxy.serverName, stRouter.serverName, stRouter, installReq.replace, err);

        //把三者关系生成放到最后面 保证配额查询接口的准确性
        try
        {
            insertProxyRouter(stProxy.serverName.substr(7), stRouter.serverName + ".RouterObj", stRouter.dbName, stRouter.dbIp, "", installReq.replace, err);
        }
        catch (exception &ex)
        {
            //这里捕捉异常以不影响安装结果
            err = string("insert relation info catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << err << endl);

            return 0;
        }

        TLOGDEBUG(FUN_LOG << "install finish" << endl);
    }
    catch (const std::exception &ex)
    {
        instalRsp.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch execption:") + ex.what();
        TLOGERROR(FUN_LOG << instalRsp.errMsg << endl);
        return -1;
    }

    return 0;
}


int DCacheOptImp::tarsServerConf(const string &appName, const string &serverName, const vector<string> &vServerIp, const string &templateName, const string &templateDetail, bool bReplace, string &err)
{
	try
	{
		int iHostConfigId;
		//插入配置文件信息
		if (insertTarsConfigFilesTable(serverName, serverName.substr(7) + ".conf", "", templateDetail, 2, iHostConfigId, bReplace, err) != 0 )
			return -1;

		if (insertTarsHistoryConfigFilesTable(iHostConfigId, templateDetail, bReplace, err) != 0)
			return -1;

		for (unsigned int i = 0; i < vServerIp.size(); i++)
		{
			string serverIp = vServerIp[i];

			//配置obj信息
			LOG->debug() << "get DBAccess Server port" << endl;
			uint16_t iDbAccessObjPort = getPort(serverIp);
			uint16_t iWDbAccessObjPort = getPort(serverIp);
			if (0 == iDbAccessObjPort || 0 == iWDbAccessObjPort)
			{
				LOG->error() << "failed to get port for " << serverName << "_" << serverIp << "|" << iDbAccessObjPort << "|" << iWDbAccessObjPort << endl;
				//return -1; 非致命错误,不需要停止
			} //获取PORT失败
			LOG->debug() << "success for getting DBAccess Server. DbAccessObjport:" << iDbAccessObjPort << " WDbAccessObjPort:" << iWDbAccessObjPort << endl;

			if (insertTarsServerTable("DCache", serverName, serverIp, templateName, "CombinDbAccessServer", "", false, bReplace, err) != 0)
				return -1;

			string sDBAccessServant = serverName + ".DbAccessObj";
			string sDBAccessEndpoint = "tcp -h " + serverIp + " -t 60000 -p " + TC_Common::tostr(getPort(serverIp, iDbAccessObjPort));
			if (insertServantTable("DCache", serverName.substr(7), serverIp, sDBAccessServant, sDBAccessEndpoint, "5", bReplace, err) != 0)
				return -1;

			string sWDBAccessServant = serverName + ".WDbAccessObj";
			string sWDBAccessEndpoint = "tcp -h " + serverIp + " -t 60000 -p " + TC_Common::tostr(getPort(serverIp, iWDbAccessObjPort));
			if (insertServantTable("DCache", serverName.substr(7), serverIp, sWDBAccessServant, sWDBAccessEndpoint, "5", bReplace, err) != 0)
				return -1;

			//insertServerGroupRelation(serverName, string("DbAccessServer"), bReplace);
			LOG->debug() << "insert DBAccess info to taf table success" << endl;
		}
	}
	catch (exception &ex)
	{
		LOG->error() << "[DCacheOptImp::tafServerConf] error! " << ex.what() << endl;
		err = ex.what();
		return -2;
	}

	return 0;

}

int DCacheOptImp::createDBAccessConf(int type, bool isSerializatedconst, const vector<DCache::RecordParam> & vtModuleRecord, const DCache::DBAccessConf & conf, string &result, string &err)
{
	//判断主key是否数字
	string isHashForLocateDB;
	if (conf.isDigital == true)
		isHashForLocateDB = "N";
	else
		isHashForLocateDB = "Y";


	string configFile = "";
	//增加taf标签
	configFile += "<taf>\n";
	configFile += "    #主从机读写配置 m|s\n";
	configFile += "    #readAble = m\n";
	configFile += "    #writeAble = m\n";
	configFile += "    #dcache是一期还是二期的\n";
	configFile += "    CacheType = " + TC_Common::tostr(type) + "\n";
	configFile += "    #如果key为非数字，可以通过hash算法把字符串转批为数字\n";
	configFile += "    isHashForLocateDB = " + isHashForLocateDB + "\n";

	//二期这个参数可以不写
	if (KVCACHE == type)
	{
		string isSerializated;
		if (isSerializatedconst)
			isSerializated = "Y";
		else
			isSerializated = "N";

		string mKey = "";
		string record = "";

		int recordIndex = 0;
		for (unsigned int i = 0; i < vtModuleRecord.size(); i++)
		{
			if (vtModuleRecord[i].keyType == "mkey")
				mKey = vtModuleRecord[i].fieldName;
			else if (vtModuleRecord[i].keyType == "value")
			{
				record += "            " + vtModuleRecord[i].fieldName + "=" + TC_Common::tostr(recordIndex) + "|" + vtModuleRecord[i].dataType + "|" + vtModuleRecord[i].property + "|" + vtModuleRecord[i].defaultValue + "\n";
				recordIndex++;
			}
		}

		if (mKey.size() == 0)
		{
			err =  "[DCacheOptImp::createDBAccessConf] error, main key miss!";
			LOG->error() << err << endl;
			return -1;
		}

		configFile += "    #一期需要配置，二期请忽略<sigKey>配置\n";
		configFile += "    <sigKey>\n";
		configFile += "        #主key字段名\n";
		configFile += "        KeyNameInDB = " + mKey + "\n";
		configFile += "        #value是否有序列化存储\n";
		configFile += "        isSerializated = " + isSerializated + "\n";
		configFile += "        #序列化存储的字段结构定义，如果非序列化，此处写vale的字段名即可\n";
		configFile += "        <record>\n";
		configFile += record;
		configFile += "        </record>\n";
		configFile += "    </sigKey>\n";
	}

	/**
	 * 分表
	 * part_table 1  10 100 1000
	 * PostfixLen 1  1  2   3
	 * Div        1  10 100 1000
	 */
	int part_table = conf.tableNum;
	int tablePostfixLen = 1;
	if (part_table == 1) {
		tablePostfixLen = 1;
	} else if (part_table == 10) {
		tablePostfixLen = 1;
	} else if (part_table == 100) {
		tablePostfixLen = 2;
	} else if (part_table == 1000) {
		tablePostfixLen = 3;
	}
	configFile += "    <table>\n";
	configFile += "        #判断表名后缀的key尾的长度，表名后缀=key.substr(key.length() - PostfixLen) % Div\n";
	configFile += "        PostfixLen = " + TC_Common::tostr(tablePostfixLen) + "\n";
	configFile += "        #表名前缀\n";
	configFile += "        TableNamePrefix  =   " + conf.tablePrefix + "\n";
	configFile += "        #判断表名后缀命名的模数\n";
	configFile += "        Div = " + TC_Common::tostr(part_table) + "\n";
	configFile += "    </table>\n";

	/**
	 * 分库
	 * part_table 1  10 100 1000
	 * PosInKey   (tablePostfixLen + dbPostfixLen)
	 * PostfixLen 1  1  2   3
	 * Div        10 1  1   1
	 */

	int part_db = conf.DBNum;
	unsigned int dbPostfixLen = 1;
	int Div = 10;
	if (part_db == 1) {
		dbPostfixLen = 1;
		Div = 10;
	} else if (part_db == 10) {
		dbPostfixLen = 1;
		Div = 1;
	} else if (part_db == 100) {
		dbPostfixLen = 2;
		Div = 1;
	} else if (part_db == 1000) {
		dbPostfixLen = 3;
		Div = 1;
	}

	configFile += "    <db_conn>\n";
	configFile += "        #判断数据连接配置\n";
	configFile += "        #从key倒数第几位开始\n";
	configFile += "        PosInKey = " + TC_Common::tostr(tablePostfixLen + dbPostfixLen) + "\n";
	configFile += "        #从开始位向右截取key一定长度的数字，库名后缀=key.substr(key.length() - PostfixLen) / Div\n";
	configFile += "        PostfixLen = " + TC_Common::tostr(dbPostfixLen) + "\n";
	configFile += "        #数据连接配置节名称前缀\n";
	configFile += "        DbPrefix  =  " + conf.DBPrefix + "\n";
	configFile += "        #判断数据库后缀命名的除数\n";
	configFile += "        Div = " + TC_Common::tostr(Div) + "\n";
	configFile += "        #连接超时时间,单位s，默认1秒\n";
	configFile += "        #conTimeOut=\n";
	configFile += "        #查询超时时间,单位s，默认1秒\n";
	configFile += "        #queryTimeOut=\n";
	configFile += "        #屏蔽后多久尝试重连,单位s，默认300秒\n";
	configFile += "        #retryDBInterval=\n";
	configFile += "        #检查超时的时间片，单位s,默认60\n";
	configFile += "        #checkTimeoutInterval=\n";
	configFile += "        #连续超时次数，默认10\n";
	configFile += "        #frequenceFailInvoke=\n";
	configFile += "        #最小失败次数  到达该值后 才开始检查radio，默认5\n";
	configFile += "        #minTimeoutInvoke=\n";
	configFile += "        #失败比例到达多少会屏蔽,默认30,单位%\n";
	configFile += "        #radio=\n";
	configFile += "    </db_conn>\n";


	//计算出每个实例下对应几个DB
	int tableInstance = conf.vDBInfo.size();
	int db_size = part_db / tableInstance;

	string connectionStr;
	string dataBaseStr;

	for (int j = 0; j < tableInstance; j++)
	{
		connectionStr += string("        ") + TC_Common::tostr(j) + "	=	" + conf.vDBInfo[j].ip + "; " + conf.vDBInfo[j].user + "; " + conf.vDBInfo[j].pwd + "; " + conf.vDBInfo[j].charset + "; " + conf.vDBInfo[j].port + "\n";

		if (j == 0) {
			string db_base_size = "";
			if (part_db == 1) {
				db_base_size = "0";
			} else if (part_db == 10) {
				db_base_size = "0";
			} else if (part_db == 100) {
				db_base_size = "00";
			} else if (part_db == 1000) {
				db_base_size = "000";
			}
			string sEnd = TC_Common::tostr(db_size - 1);
			while (sEnd.size() < dbPostfixLen)
			{
				sEnd = string("0").append(sEnd);
			}

			dataBaseStr += string("        [") + TC_Common::tostr(db_base_size) + "-" +  sEnd + "] = " + TC_Common::tostr(j) + "\n";
		} else {
			string sBegin = TC_Common::tostr(db_size * j);
			while (sBegin.size() < dbPostfixLen)
			{
				sBegin = string("0").append(sBegin);
			}

			string sEnd = TC_Common::tostr(db_size * (j + 1) - 1 );
			while (sEnd.size() < dbPostfixLen)
			{
				sEnd = string("0").append(sEnd);
			}
			dataBaseStr += string("        [") + sBegin + "-" +  sEnd + "] = " + TC_Common::tostr(j) + "\n";
		}
	}

	configFile += "    <Connection>\n";
	configFile += "        # 连接编号; host; user; pass; charset; dbport\n";
	configFile += connectionStr;
	configFile += "    </Connection>\n";
	configFile += "    <DataBase>\n";
	configFile += "        # DataBase编号; DataBase所在的连接编号\n";
	configFile += dataBaseStr;
	configFile += "    </DataBase>\n";
	configFile += "</taf>";

	result = configFile;

	return 0;
}


int DCacheOptImp::creatDBAccessTable(const string &serverName, const vector<DCache::RecordParam> & vtModuleRecord, const DCache::DBAccessConf & conf, bool bReplace, string &err)
{
	try
	{
		//用于插入t_dbaccess_cdb表的信息
		string tableName;
		string dbIP;

		//生成表信息
		string tableSql = "create table `" + conf.tablePrefix + "${NUM}` (";

		for (unsigned int i = 0; i < vtModuleRecord.size(); i++)
		{
			tableSql += "`" + vtModuleRecord[i].fieldName + "` " + vtModuleRecord[i].DBType + " ";

			if (vtModuleRecord[i].keyType == "mkey") //主键
				tableSql += " NOT NULL ";

			/*if(vtModuleRecord[i].dataType == "string")
			    tableSql += " DEFAULT '" + vtModuleRecord[i].defaultValue + "' ";
			else if(vtModuleRecord[i].defaultValue.size() == 0)
			    tableSql += " DEFAULT 0";
			else
			    tableSql += " DEFAULT " + vtModuleRecord[i].defaultValue;*/

			// tableSql += " COMMENT '" + vtModuleRecord[i].remark + "' ,";
			tableSql += ", ";
		}
		//加入淘汰时间字段
		tableSql += "`sDCacheExpireTime` int(16),";
		//加入时间戳
		tableSql += "`auto_updatetime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,";

		tableSql += "PRIMARY KEY (";
		bool first = true;
		for (unsigned int i = 0; i < vtModuleRecord.size(); i++)
		{
			if ((vtModuleRecord[i].keyType == "mkey") || (vtModuleRecord[i].keyType == "ukey"))
			{
				if (first)
				{
					tableSql += "`" + vtModuleRecord[i].fieldName + "`";
					first = false;
				}
				else
					tableSql += ",`" + vtModuleRecord[i].fieldName + "`";
			}
		}
		tableSql += ")";

		tableSql += ") ENGINE=" + conf.tableEngine + " CHARSET=" + conf.tableCharset;

		unsigned int numPerDb = conf.DBNum / conf.vDBInfo.size();

		unsigned int dbIndex = 0;

		unsigned int dbIndexLen = 0;
		switch (conf.DBNum)
		{
		case 1: dbIndexLen = 1; break;
		case 10: dbIndexLen = 1; break;
		case 100: dbIndexLen = 2; break;
		case 1000: dbIndexLen = 3; break;
		default: LOG->error() << "[DCacheOptImp::creatDBAccessTable] db number to much " << conf.DBNum << endl; return -1;
		}

		TC_LockT<TC_ThreadLock> lock(g_dbaccessLock);
		g_dbaccessError.clear();

		//开始创建数据库表
		for (unsigned int i = 0; i < conf.vDBInfo.size(); i++)
		{
			for (unsigned int j = 0; j < numPerDb; j++)
			{
				string sNum = TC_Common::tostr(dbIndex);
				dbIndex++;

				//不够位数，前面补0
				for (unsigned int k = sNum.size(); k < dbIndexLen; k++)
					sNum = "0" + sNum;

				string dbName = conf.DBPrefix + sNum;

				//异步创建表
				_tpool.exec(dbaccessFun, conf.vDBInfo[i].ip, conf.vDBInfo[i].user, conf.vDBInfo[i].pwd, TC_Common::strto<int>(conf.vDBInfo[i].port), dbName, tableSql, conf.tableNum, bReplace);

				// dbIP = conf.vDBInfo[i].ip + "_" + conf.vDBInfo[i].port;

				// tableName = dbName + "_";

				// unsigned int tableIndexLen = 0;
				// switch (conf.tableNum)
				// {
				// case 1: tableIndexLen = 1; break;
				// case 10: tableIndexLen = 1; break;
				// case 100: tableIndexLen = 2; break;
				// case 1000: tableIndexLen = 3; break;
				// default: LOG->error() << "[DCacheOptImp::creatDBAccessTable] table number to much " << conf.tableNum << endl; return -1;
				// }

				// //创建表
				// for (int k = 0; k < conf.tableNum; k++)
				// {
				// 	string sTableIndex = TC_Common::tostr(k);

				// 	//表的位数不够，前面补0
				// 	for (unsigned int n = sTableIndex.size(); n < tableIndexLen; n++)
				// 		sTableIndex = "0" + sTableIndex;

				// 	tableName += conf.tablePrefix + sTableIndex;

				// 	//插入t_dbaccess_cdb
				// 	if (insertDBAccessCDB(serverName, tableName, dbIP, bReplace) < 0)
				// 	{
				// 		err = "[DCacheOptImp::creatDBAccessTable] error! insert into t_dbaccess_cdb error!";
				// 		LOG->error() << err << endl;
				// 	}
				// }
			}
		}

		_tpool.waitForAllDone();
		if (g_dbaccessError.size() != 0)
		{
			err = g_dbaccessError;
			g_dbaccessError.clear();
			LOG->error() << err << endl;
			return -1;
		}
	}
	catch (exception &ex)
	{
		LOG->error() << "[DCacheOptImp::creatDBAccessTable] exception: " << ex.what() << endl;
		err = string("[creatDBAccessTable] exception: ") + ex.what();
		return -1;
	}
	return 0;
}

void DCacheOptImp::creatDBAccessTableThread(const string &dbIp, const string &dbUser, const string &dbPwd, const int dbPort, const string &dbName, const string &tableSql, const int number, bool bReplace)
{
	TC_Mysql dbConn(dbIp, dbUser, dbPwd, "", "", dbPort);
	dbConn.connect();

	string sql;
	string err;
	try
	{
		//创建数据库
		sql = "create database " + dbName;
		dbConn.execute(sql);
	}
	catch (exception &ex) //数据库已经存在
	{
		err = ex.what();
		LOG->error() << "[creatDBAccessTable] exception: " << err << endl;
		if (bReplace && (err.find("database exists") != string::npos))
			LOG->debug() << "dbName is exist! " << dbName << endl;
		else
		{
			g_dbaccessError = string("[creatDBAccessTable] exception: ") + ex.what();
			LOG->error() << g_dbaccessError << endl;
			return;
		}
	}

	sql = "use " + dbName;
	dbConn.execute(sql);

	unsigned int tableIndexLen = 0;
	switch (number)
	{
	case 1: tableIndexLen = 1; break;
	case 10: tableIndexLen = 1; break;
	case 100: tableIndexLen = 2; break;
	case 1000: tableIndexLen = 3; break;
	default: g_dbaccessError = "[DCacheOptImp::creatDBAccessTable] table number to much!"; LOG->error() << g_dbaccessError << number << endl; return;
	}

	//创建表
	for (int k = 0; k < number; k++)
	{
		string sTableIndex = TC_Common::tostr(k);

		//表的位数不过，前面补0
		for (unsigned int n = sTableIndex.size(); n < tableIndexLen; n++)
			sTableIndex = "0" + sTableIndex;

		sql = tableSql;

		sql = TC_Common::replace(sql, "${NUM}", sTableIndex);

		try
		{
			dbConn.execute(sql);
		}
		catch (exception &ex)
		{
			err = ex.what();
			LOG->error() << "[creatDBAccessTable] exception: " << err << endl;
			if (bReplace && (err.find("already exists") != string::npos))
			{
				LOG->debug() << "table is exist! " << sTableIndex << endl;
				continue;
			}
			else
			{
				g_dbaccessError = string("[creatDBAccessTable] exception: ") + ex.what();
				LOG->error() << g_dbaccessError << endl;
				return;
			}
		}
	}
}

tars::Int32 DCacheOptImp::installDBAccess(const InstallDbAccessReq &req, InstallDbAccessRsp &rsp, tars::CurrentPtr current)
{
	LOG->debug() << __FUNCTION__ << ":" << __LINE__ << "|" << req.appName << "|" << req.serverName << endl;

	try
	{
		string serverConf;

		//生成配置文件信息
		if (createDBAccessConf(req.cacheType, req.isSerializated, req.vtModuleRecord, req.conf, serverConf, rsp.errMsg) != 0)
		{
			LOG->error() <<  "[DCacheOptImp::installDBAccess] create config data error!" << endl;
			rsp.errMsg = "create config data error" + rsp.errMsg;
			return -1;
		}

		LOG->debug() << serverConf << endl;

		//插入tars数据库
		if (tarsServerConf(req.appName, req.serverName, req.serverIp, req.serverTemplate, serverConf, req.replace, rsp.errMsg) < 0)
		{
			LOG->error() << "[DCacheOptImp::installDBAccess] insert tars db error!" << endl;
			rsp.errMsg = "insert tars db error!" + rsp.errMsg;
			return -2;
		}

		//插入dcache数据库
		map<string, pair<TC_Mysql::FT, string> > m;

		m["dbaccess_name"]     = make_pair(TC_Mysql::DB_STR, req.serverName);
		m["app_name"]     = make_pair(TC_Mysql::DB_STR, req.appName);

		_mysqlRelationDB.replaceRecord("t_dbaccess_app", m);

		LOG->debug() << "[DCacheOptImp::installDBAccess] begin creatDBAccessTable!" << endl;
		//创建数据库表
		if (creatDBAccessTable(req.serverName.substr(7), req.vtModuleRecord, req.conf, req.replace, rsp.errMsg) != 0)
		{
			LOG->error() << "[DCacheOptImp::installDBAccess] insert create db table error!" << endl;
			rsp.errMsg = "insert create db table error!" + rsp.errMsg;
			return -3;
		}
	}
	catch (const std::exception &ex)
	{
		rsp.errMsg = string("[DCacheOptImp::installDBAccess] ERROR :") + ex.what();
		LOG->error() << rsp.errMsg << endl;
		return -3;
	}
	LOG->debug() << "[DCacheOptImp::installDBAccess] complete!" << endl;
	return 0;
}

int DCacheOptImp::getShmKey(size_t &shmKey)
{
    try
    {
        for(;;)
        {
            shmKey = 1141161986 + rand()%10000000;

            string sql = "select t1.config_value as config_value from t_config_table as t1, t_map_table as t2 where t1.`item_id` = t2.`id` and t2.item='ShmKey' and t1.config_value = " + TC_Common::tostr(shmKey);

            auto data = _mysqlRelationDB.queryRecord(sql);

            if(data.size() == 0)
            {
                return 0;
            }
        }
    }
    catch(const std::exception &ex)
    {
        string errmsg = string("getShmKey catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::installKVCacheModule(const InstallKVCacheReq & kvCacheReq, InstallKVCacheRsp & kvCacheRsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name: " << kvCacheReq.appName << "|module name: " << kvCacheReq.moduleName << endl);

    try
    {
        const vector<DCache::CacheHostParam> & vtCacheHost = kvCacheReq.kvCacheHost;
        const SingleKeyConfParam & stSingleKeyConf = kvCacheReq.kvCacheConf;
        const string & sTarsVersion = kvCacheReq.version;

        RouterConsistRes &rcRes = kvCacheRsp.rcRes;
        std::string &err = kvCacheRsp.errMsg;

        //初始化out变量
        rcRes.iFlag = 0;
        rcRes.sInfo = "success";
        err = "success";

        //安装前先检查路由数据库与内存是否一致
        TC_DBConf routerDbInfo;
        vector<string> vsProxyName;
        string strRouterName;

        int iRet = getRouterDBFromAppTable(kvCacheReq.appName, routerDbInfo, vsProxyName, strRouterName, err);
        if (0 != iRet)
        {
            return -1;
        }

        rcRes.iFlag = iRet;
        rcRes.sInfo = err;

        //生成cache router关系,防止多次调用时无关系可查
        if (vtCacheHost.size() == 0 || vtCacheHost[0].serverName.empty())
        {
            err = "install cache server, but cache server name is empty";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        if (createKVCacheConf(kvCacheReq.appName, kvCacheReq.moduleName, strRouterName, vtCacheHost, stSingleKeyConf, kvCacheReq.replace, err) != 0)
        {
            err = "failed to create kvcache server conf";
            return -1;
        }

        if (insertCache2RouterDb(kvCacheReq.moduleName, "", routerDbInfo, vtCacheHost, kvCacheReq.replace, err) != 0)
        {
            if (err.empty())
            {
                err = "failed to insert module and cache info to router db";
            }
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 通知router加载新模块的信息
        (void)reloadRouterByModule("DCache", kvCacheReq.moduleName, strRouterName, err);

        if (!checkRouterLoadModule("DCache", kvCacheReq.moduleName, strRouterName, err))
        {
            err = "router check routing error, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 保存cache信息到tars db中
        iRet = insertCache2TarsDb(routerDbInfo, vtCacheHost, DCache::KVCACHE, sTarsVersion, kvCacheReq.replace, err);
        if (iRet != 0)
        {
            err = "failed to insert catch info to tars db, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        //把三者关系生成放到最后面 保证配额查询接口的准确性
        try
        {
            for (size_t i = 0; i < vtCacheHost.size(); ++i)
            {
                for (size_t j = 0; j < vsProxyName.size(); j++)
                {
                    // 保存 proxy server和cache server的对应关系
                    insertProxyCache(vsProxyName[j].substr(7), vtCacheHost[i].serverName, kvCacheReq.replace);
                }

                // 保存cache serve和router server的对应关系
                insertCacheRouter(vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, int(TC_Common::strto<float>(vtCacheHost[i].shmSize)*1024), strRouterName + ".RouterObj", routerDbInfo, kvCacheReq.moduleName, kvCacheReq.replace, err);
            }
        }
        catch(exception &ex)
        {
            //这里捕捉异常以不影响安装结果
            err = string("option relation db catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << err << endl);
            return 0;
        }

        TLOGDEBUG(FUN_LOG << "finish" << endl);
    }
    catch(const std::exception &ex)
    {
        kvCacheRsp.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << kvCacheRsp.errMsg << endl);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::installMKVCacheModule(const InstallMKVCacheReq & mkvCacheReq, InstallMKVCacheRsp & mkvCacheRsp, tars::TarsCurrentPtr current)
{
    try
    {
        const vector<DCache::CacheHostParam> & vtCacheHost = mkvCacheReq.mkvCacheHost;
        const MultiKeyConfParam & stMultiKeyConf = mkvCacheReq.mkvCacheConf;
        const vector<DCache::RecordParam> & vtRecord = mkvCacheReq.fieldParam;
        const string & sTarsVersion = mkvCacheReq.version;

        RouterConsistRes &rcRes = mkvCacheRsp.rcRes;
        std::string &err = mkvCacheRsp.errMsg;

        //初始化out变量
        rcRes.iFlag = 0;
        rcRes.sInfo = "success";
        err = "success";

        //安装前先检查路由数据库与内存是否一致
        TC_DBConf routerDbInfo;
        vector<string> vtProxyName;
        string strRouterName;
        int iRet = getRouterDBFromAppTable(mkvCacheReq.appName, routerDbInfo, vtProxyName, strRouterName, err);
        if (0 != iRet)
        {
            return -1;
        }

        rcRes.iFlag = iRet;
        rcRes.sInfo = err;

        //生成cache router关系,防止多次调用时无关系可查
        if (vtCacheHost.size() == 0 || vtCacheHost[0].serverName.empty())
        {
            err = "cache server is empty";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        if (createMKVCacheConf(mkvCacheReq.appName, mkvCacheReq.moduleName, strRouterName, vtCacheHost, stMultiKeyConf, vtRecord, mkvCacheReq.replace, err) != 0)
        {
            err = "failed to create mkvcache server conf";
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        if (insertCache2RouterDb(mkvCacheReq.moduleName, "", routerDbInfo, vtCacheHost, mkvCacheReq.replace, err) != 0)
        {
            if (err.empty())
            {
                err = "failed to insert module and cache info to router db";
            }
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 通知router加载新模块的信息
        (void)reloadRouterByModule("DCache", mkvCacheReq.moduleName, strRouterName, err);

        if (!checkRouterLoadModule("DCache", mkvCacheReq.moduleName, strRouterName, err))
        {
            err = "router check routing error, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        // 保存cache信息到tarsDB中
        iRet = insertCache2TarsDb(routerDbInfo, vtCacheHost, MKVCACHE, sTarsVersion, mkvCacheReq.replace, err);
        if (iRet != 0)
        {
            err = "failed to insert catch info to tars db, errmsg:" + err;
            TLOGERROR(FUN_LOG << err << endl);
            return -1;
        }

        //把三者关系生成放到最后面 保证配额查询接口的准确性
        try
        {
            for (size_t i = 0; i < vtCacheHost.size(); ++i)
            {
                for (unsigned int j = 0; j < vtProxyName.size(); j++)
                {
                    // 保存 proxy server和cache server的对应关系
                    insertProxyCache(vtProxyName[j].substr(7), vtCacheHost[i].serverName, mkvCacheReq.replace);
                }

                // 保存cache serve和router server的对应关系
                insertCacheRouter(vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, int(TC_Common::strto<float>(vtCacheHost[i].shmSize) * 1024), strRouterName + ".RouterObj", routerDbInfo, mkvCacheReq.moduleName, mkvCacheReq.replace, err);
            }
        }
        catch(exception &ex)
        {
            //这里捕捉异常以不影响安装结果
            err = string("option relation db catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << err << endl);
            return 0;
        }

        TLOGDEBUG(FUN_LOG << "finish" << endl);
    }
    catch(const std::exception &ex)
    {
        mkvCacheRsp.errMsg = TC_Common::tostr(__FUNCTION__) + string(" catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << mkvCacheRsp.errMsg << endl);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::releaseServer(const vector<DCache::ReleaseInfo> & releaseInfo, ReleaseRsp & releaseRsp, tars::TarsCurrentPtr current)
{
    try
    {
        if (releaseInfo.empty())
        {
            releaseRsp.errMsg = "released server info is null, please cheak";
            TLOGERROR(FUN_LOG << releaseRsp.errMsg << endl);
            return -1;
        }

        ReleaseRequest* request = new ReleaseRequest();
        request->requestId      = g_app.getReleaseID();
        request->releaseInfo    = releaseInfo;
        releaseRsp.releaseId    = request->requestId;
        g_app.releaseRequestQueueManager()->push_back(request);

        releaseRsp.errMsg = "sucess to release server";

        TLOGDEBUG(FUN_LOG << "sucess to release server|server info size:" << releaseInfo.size() << "|release id:" << request->requestId << endl);

        return 0;
    }
    catch (exception &e)
    {
        releaseRsp.errMsg = string("release dcache server catch exception:") + e.what();
    }
    catch (...)
    {
        releaseRsp.errMsg = "release dcahce server unkown exception";
    }

    TLOGERROR(FUN_LOG << releaseRsp.errMsg << endl);

    return -1;
}

tars::Int32 DCacheOptImp::getReleaseProgress(const ReleaseProgressReq & progressReq, ReleaseProgressRsp & progressRsp, tars::TarsCurrentPtr current)
{
    try
    {
        vector<DCache::ReleaseInfo> &vtReleaseInfo = progressRsp.releaseInfo;

        ReleaseStatus releaseStatus = g_app.releaseRequestQueueManager()->getProgressRecord(progressReq.releaseId, vtReleaseInfo);
        TLOGDEBUG(FUN_LOG << "release id:" << progressReq.releaseId << "|release status:" << releaseStatus.status << "|release percent:" << releaseStatus.percent << "%" << endl);

        if (releaseStatus.status != RELEASE_FAILED)
        {
            progressRsp.releaseId   = progressReq.releaseId;
            progressRsp.percent     = releaseStatus.percent;
            if (releaseStatus.status == RELEASE_FINISH && 100 == progressRsp.percent)
            {
                g_app.releaseRequestQueueManager()->deleteProgressRecord(progressReq.releaseId);

                TLOGDEBUG(FUN_LOG << "release finish|delete release progress record id:" << progressReq.releaseId << endl);
            }

            return 0;
        }
        else
        {
            progressRsp.releaseId   = progressReq.releaseId;
            progressRsp.errMsg      = releaseStatus.errmsg;

            g_app.releaseRequestQueueManager()->deleteProgressRecord(progressReq.releaseId);

            TLOGERROR(FUN_LOG << "release failed|errmsg:" << progressRsp.errMsg << "|release id:" << progressReq.releaseId << endl);
        }
    }
    catch (exception &ex)
    {
        progressRsp.errMsg = string("get the percent of releasing dcache server catch exception:") + ex.what();
    }
    catch (...)
    {
        progressRsp.errMsg = "get the percent of releasing dcache server catch unknown exception";
    }

    TLOGERROR(FUN_LOG << progressRsp.errMsg << endl);

    return -1;
}

tars::Int32 DCacheOptImp::uninstall4DCache(const UninstallReq & uninstallInfo, UninstallRsp & uninstallRsp, tars::TarsCurrentPtr current)
{
    try
    {
        UninstallRequest request;
        request.info = uninstallInfo;

        if (DCache::MODULE == uninstallInfo.unType)
        {
            request.requestId =  uninstallInfo.moduleName;
            TLOGDEBUG(FUN_LOG << "uninstall by module|module name:" << uninstallInfo.moduleName << endl);
        }
        else if (DCache::GROUP ==  uninstallInfo.unType)
        {
            //按组
            request.requestId = uninstallInfo.groupName;
            TLOGDEBUG(FUN_LOG << "uninstall by group|group name:" << uninstallInfo.groupName << endl);
        }
        else if (DCache::SERVER == uninstallInfo.unType)
        {
            //按服务
            request.requestId = uninstallInfo.serverName;
            TLOGDEBUG(FUN_LOG << "uninstall by cache server|cache server name:" << uninstallInfo.serverName << endl);
        }
        else if (DCache::QUOTA_SERVER == uninstallInfo.unType)
        {
            //按服务
            request.requestId = uninstallInfo.serverName;
            TLOGDEBUG(FUN_LOG << "uninstall by quota cache server|cache server name:" << uninstallInfo.serverName << endl);
        }
        else
        {
            throw DCacheOptException("uninstall type invalid: " + TC_Common::tostr(uninstallInfo.unType) + ", should be 0(server), 1(group), 2(module)");
            TLOGERROR(FUN_LOG << "uninstall type invalid:" << uninstallInfo.unType << ", should be 0(server), 1(group), 2(module)" << endl);
        }

        g_app.uninstallRequestQueueManager()->addUninstallRecord(request.requestId);
        g_app.uninstallRequestQueueManager()->push_back(request);

        TLOGDEBUG(FUN_LOG << "sucess to uninstall cache|request id:" << request.requestId << "|uninstall type:" << uninstallInfo.unType << endl);

        return 0;
    }
    catch (exception &e)
    {
        uninstallRsp.errMsg = string("uninstall dcache server catch exception:") + e.what();
    }
    catch (...)
    {
        uninstallRsp.errMsg = "uninstall dcache server catch unkown exception";
    }

    TLOGERROR(FUN_LOG << uninstallRsp.errMsg << endl);

    return -1;
}

tars::Int32 DCacheOptImp::getUninstallPercent(const UninstallReq & uninstallInfo, UninstallProgressRsp & progressRsp, tars::TarsCurrentPtr current)
{
    string sRequestId("");
    progressRsp.errMsg = "";

    try
    {
        if (DCache::MODULE == uninstallInfo.unType)
        {
            sRequestId = uninstallInfo.moduleName;
        }
        else if (DCache::GROUP == uninstallInfo.unType)
        {
            sRequestId = uninstallInfo.groupName;
        }
        else if (DCache::SERVER == uninstallInfo.unType || DCache::QUOTA_SERVER == uninstallInfo.unType)
        {
            sRequestId = uninstallInfo.serverName;
        }

        UninstallStatus currentStatus = g_app.uninstallRequestQueueManager()->getUninstallRecord(sRequestId);
        TLOGDEBUG(FUN_LOG << "uninstall status:" << currentStatus.status << "|uninstall percent:" << currentStatus.percent << "|request id:" << sRequestId << endl);

        if (currentStatus.status != UNINSTALL_FAILED)
        {
            progressRsp.percent = currentStatus.percent;
            if (currentStatus.status == UNINSTALL_FINISH && 100 == progressRsp.percent)
            {
                g_app.uninstallRequestQueueManager()->deleteUninstallRecord(sRequestId);
            }

            return 0;
        }
        else
        {
            progressRsp.percent = 0;
            progressRsp.errMsg  = currentStatus.errmsg;
            g_app.uninstallRequestQueueManager()->deleteUninstallRecord(sRequestId);

            TLOGDEBUG(FUN_LOG << "uninstall failed|errmsg:" << progressRsp.errMsg << "|request id:" << sRequestId << endl);
        }
    }
    catch(exception &ex)
    {
        progressRsp.errMsg = string("get the percent of uninstalling dcache server catch exception:") + ex.what();
    }
    catch (...)
    {
        progressRsp.errMsg = "get the percent of uninstalling dcache server catch unknown exception";
    }

    TLOGERROR(FUN_LOG << progressRsp.errMsg << endl);

    return -1;
}

tars::Int32 DCacheOptImp::transferDCache(const TransferReq & req, TransferRsp & rsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name:" << req.appName << "|module name:" << req.moduleName << "|src group name:" << req.srcGroupName << endl);

    string &errmsg = rsp.errMsg;
    try
    {
        int iRet = insertTransferStatusRecord(req.appName, req.moduleName, req.srcGroupName, req.cacheHost[0].groupName, false, errmsg);
        if (iRet != 0)
        {
            return iRet;
        }

        iRet = expandDCacheServer(req.appName, req.moduleName, req.cacheHost, req.version, req.cacheType, true, errmsg);
        if (iRet == 0)
        {
            //根据appName查询 router obj
            string routerObj("");
            iRet = getRouterObj(req.appName, routerObj, errmsg);
            if (iRet != 0)
            {
                errmsg = "get router obj info failed|appName:" + req.appName + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            vector<string> tmp = TC_Common::sepstr<string>(routerObj, ".");
            iRet = reloadRouterByModule("DCache", req.moduleName, tmp[0] + "." + tmp[1], errmsg);
            if (iRet == 0)
            {
                map<string, pair<TC_Mysql::FT, string> > m_update;
                m_update["status"]  = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(CONFIG_SERVER)); // 配置阶段完成

                string condition = "where module_name='"    + req.moduleName
                                 + "' and src_group='"      + req.srcGroupName
                                 + "' and app_name='"       + req.appName
                                 + "' and dst_group='"      + req.cacheHost[0].groupName + "'";

                _mysqlRelationDB.updateRecord("t_transfer_status", m_update, condition);

                TLOGDEBUG(FUN_LOG << "configure transfer record succ and expand cache server succ, module name:" << req.moduleName
                                  << "|src group name:" << req.srcGroupName << "|dst group name:" << req.cacheHost[0].groupName << endl);

                return 0;
            }
            else
            {
                errmsg = "reload router conf from DB failed, router server name:" + tmp[1] + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
            }
        }
        else
        {
           errmsg = "configure transfer record failed and transfer cache server failed, module Name:" + req.moduleName + "|src group name:" + req.srcGroupName + "|dst group name:" + req.cacheHost[0].groupName + "|errmsg:" + errmsg;
           TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch (const std::exception &ex)
    {
        errmsg = string("configure transfer record catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

tars::Int32 DCacheOptImp::transferDCacheGroup(const TransferGroupReq & req, TransferGroupRsp & rsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name:" << req.appName << "|module name:" << req.moduleName << "|src group name:" << req.srcGroupName << "|dst group name:" << req.dstGroupName << endl);

    string & errmsg = rsp.errMsg;
    try
    {
        int iRet = insertTransferStatusRecord(req.appName, req.moduleName, req.srcGroupName, req.dstGroupName, true, errmsg);
        if (iRet != 0)
        {
            return iRet;
        }

        iRet = insertTransfer2RouterDb(req.appName, req.moduleName, req.srcGroupName, req.dstGroupName, req.transferData, rsp.errMsg);

        return iRet;
    }
    catch (exception &ex)
    {
        errmsg = string("transfer to existed gourp catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

/*
* 扩容
*/
tars::Int32 DCacheOptImp::expandDCache(const ExpandReq & expandReq, ExpandRsp & expandRsp, tars::TarsCurrentPtr current)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "new expand request|appName:" << expandReq.appName << "|moduleName:" << expandReq.moduleName << "|cacheType:" << etos(expandReq.cacheType) << endl);

        int iRet = insertExpandReduceStatusRecord(expandReq.appName, expandReq.moduleName, DCache::EXPAND_TYPE, vector<string>(), expandRsp.errMsg);
        if (iRet == 0)
        {
            iRet = expandDCacheServer(expandReq.appName, expandReq.moduleName, expandReq.cacheHost, expandReq.version, expandReq.cacheType, expandReq.replace, expandRsp.errMsg);
            if (iRet == 0)
            {
                //根据appName查询 router obj
                string routerObj("");
                iRet = getRouterObj(expandReq.appName, routerObj, expandRsp.errMsg);
                if (iRet != 0)
                {
                    expandRsp.errMsg = "get router obj info failed|appName:" + expandReq.appName + "|errmsg:" + expandRsp.errMsg;
                    TLOGERROR(FUN_LOG << expandRsp.errMsg << endl);
                    return -1;
                }

                vector<string> tmp = TC_Common::sepstr<string>(routerObj, ".");
                string routerName = tmp[0] + "." + tmp[1];

                iRet = reloadRouterByModule("DCache", expandReq.moduleName, routerName, expandRsp.errMsg);
                if (iRet == 0)
                {
                    map<string, pair<TC_Mysql::FT, string> > m_update;
                    m_update["status"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(CONFIG_SERVER));

                    string condition = "where module_name='" + expandReq.moduleName + "' and app_name='" + expandReq.appName + "' and type=" + TC_Common::tostr(DCache::EXPAND_TYPE);

                    _mysqlRelationDB.updateRecord("t_expand_status", m_update, condition);

                    return 0;
                }
                else
                {
                    expandRsp.errMsg = "reload router conf from DB failed, router server name:" + tmp[1];
                    TLOGERROR(FUN_LOG << expandRsp.errMsg << endl);
                }
            }
            else
            {
               expandRsp.errMsg = "configure expand record failed and expand cache server failed, app name:" + expandReq.appName + "|module name:" + expandReq.moduleName + "|errmsg:" + expandRsp.errMsg;
               TLOGERROR(FUN_LOG << expandRsp.errMsg << endl);
            }
        }
        else
        {
            expandRsp.errMsg = "insert expand or reduce to db failed|errmsg:" + expandRsp.errMsg;
            TLOGERROR(FUN_LOG << expandRsp.errMsg << endl);
        }
    }
    catch (const std::exception &ex)
    {
        expandRsp.errMsg = string("expand dcache server catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << expandRsp.errMsg << endl);
    }

    return -1;
}

/*
* 缩容
*/
tars::Int32 DCacheOptImp::reduceDCache(const ReduceReq & reduceReq, ReduceRsp & reduceRsp, tars::TarsCurrentPtr current)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "appName:" << reduceReq.appName << "|moduleName:" << reduceReq.moduleName << endl);

        int iRet = insertExpandReduceStatusRecord(reduceReq.appName, reduceReq.moduleName, DCache::REDUCE_TYPE, reduceReq.srcGroupName, reduceRsp.errMsg);

        return iRet;

    }
    catch(const std::exception &ex)
    {
        reduceRsp.errMsg = string("reduce dcache server catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << reduceRsp.errMsg << endl);
    }

    return -1;
}


/*
* 配置router迁移任务
*/
tars::Int32 DCacheOptImp::configTransfer(const ConfigTransferReq & req, ConfigTransferRsp & rsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name:" << req.appName << "|module name:" << req.moduleName << "|transfer type:" << etos(req.type) << endl);

    try
    {
        int iRet = 0;

        if (req.type == DCache::TRANSFER_TYPE)
        {
            // transfer
            if (req.srcGroupName.size() < 1 || req.dstGroupName.size() < 1)
            {
                rsp.errMsg = string("transfer info wrong, src group or dst group is empty|src group size:")
                           + TC_Common::tostr(req.srcGroupName.size()) + "|dst group size:" + TC_Common::tostr(req.dstGroupName.size());
                TLOGERROR(FUN_LOG << rsp.errMsg << endl);
                return -1;
            }

            iRet = insertTransfer2RouterDb(req.appName, req.moduleName, req.srcGroupName[0], req.dstGroupName[0], req.transferData, rsp.errMsg);
        }
        else if (req.type == DCache::EXPAND_TYPE)
        {
            // expand
            if (req.dstGroupName.size() < 1)
            {
                rsp.errMsg = string("expand info wrong, dst group is empty|dst group size:") + TC_Common::tostr(req.dstGroupName.size());
                TLOGERROR(FUN_LOG << rsp.errMsg << endl);
                return -1;
            }

            iRet = insertExpandTransfer2RouterDb(req.appName, req.moduleName, req.dstGroupName, rsp.errMsg);
        }
        else if (req.type == DCache::REDUCE_TYPE)
        {
            // reduce
            if (req.srcGroupName.size() < 1)
            {
                rsp.errMsg = string("reduce info wrong, src group is empty|src group size:") + TC_Common::tostr(req.srcGroupName.size());
                TLOGERROR(FUN_LOG << rsp.errMsg << endl);
                return -1;
            }

            iRet = insertReduceTransfer2RouterDb(req.appName, req.moduleName, req.srcGroupName, rsp.errMsg);
        }

        return iRet;
    }
    catch(const std::exception &ex)
    {
        rsp.errMsg = string("config transfer to router db table t_router_transfer catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << rsp.errMsg << endl);
    }

    return -1;
}

tars::Int32 DCacheOptImp::getModuleStruct(const ModuleStructReq & req,ModuleStructRsp & rsp,tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name:" << req.appName << "|module name:" << req.moduleName << endl);
    try
    {
        string & errmsg = rsp.errMsg;
        string sql("");

        //根据appName查询路由数据库信息
        TC_DBConf routerDbInfo;
        int iRet = getRouterDBInfo(req.appName, routerDbInfo, errmsg);
        if (iRet == -1)
        {
            errmsg = "get router db info failed|app name:" + req.appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        TC_Mysql tcMysql;
        tcMysql.init(routerDbInfo);
        tcMysql.connect(); //连不上时抛出异常,加上此句方便捕捉异常

        //找出服务组中服务节点数最多的组
        sql = "select group_name,count(*) as num from t_router_group where module_name='" + req.moduleName + "' group by group_name order by num desc";
        TC_Mysql::MysqlData groupInfo = tcMysql.queryRecord(sql);

        if (groupInfo.size() <= 0)
        {
            errmsg = "can not find module group info|app name:" + req.appName + "|module name:" + req.moduleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        //查找务组中服务节点最多的组的服务结构信息
        sql = "select * from t_router_group where module_name='" + req.moduleName + "' and group_name='" + groupInfo[0]["group_name"] + "'";
        TC_Mysql::MysqlData groupStruct = tcMysql.queryRecord(sql);

        if (groupStruct.size() < 0)
        {
            errmsg = "can not find module struct|appn ame:" + req.appName + "|module name:" + req.moduleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        //插入模块结构信息
        vector<ModuleServer> & serverInfo = rsp.serverInfo;
        serverInfo.clear();

        for (size_t i = 0; i < groupStruct.size(); i++)
        {
            ModuleServer tmp;
            tmp.serverName  = groupStruct[i]["server_name"];
            tmp.type        = groupStruct[i]["server_status"];

            //找出服务的idc信息
            sql = " select * from t_router_server where server_name='" + tmp.serverName + "'";
            TC_Mysql::MysqlData serverInfoData = tcMysql.queryRecord(sql);
            if (serverInfoData.size() <= 0)
            {
                errmsg = "can not find server info|app name:" + req.appName + "|module name:" + req.moduleName + "|server name:" + tmp.serverName;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            tmp.idc = serverInfoData[0]["idc_area"];

            serverInfo.push_back(tmp);
        }

        // 找出主机的idc地区
        string masterIdc("");
        vector<ModuleServer>::iterator it = serverInfo.begin();
        for (; it != serverInfo.end(); it++)
        {
            if (it->type == "M")
            {
                masterIdc = it->idc;
                break;
            }
        }

        if (masterIdc.size() == 0)
        {
            errmsg = "can not find master idc|app name:" + req.appName + "|module name:" + req.moduleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        // 返回主机所在地区
        rsp.idc = masterIdc;


        if (serverInfo[0].serverName.find("MKVCache") != string::npos)
        {
            rsp.cacheType = DCache::MKVCACHE; // mkv cache
        }
        else
        {
            rsp.cacheType = DCache::KVCACHE; // kv cache
        }

        //查询主机信息
        sql = "select * from t_router_group where module_name='" + req.moduleName + "' and server_status='M'";
        TC_Mysql::MysqlData masterInfo = tcMysql.queryRecord(sql);
        if (masterInfo.size() == 0)
        {
            errmsg = "can not find master server info|app name:" + req.appName + "|module name:" + req.moduleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        float totalMemSize = 0;

        // 查询主机的内存大小
        string sQuerySql = "select id from t_config_item where item='ShmSize' and path='Main/Cache'";
        TC_Mysql::MysqlData shmSizeId;
        shmSizeId = _mysqlRelationDB.queryRecord(sQuerySql);

        for (size_t i = 0; i < masterInfo.size(); i++)
        {
            float memSize = 0;
            sQuerySql = "select config_value from t_config_table where server_name='" + masterInfo[i]["server_name"] + "' and item_id=" + shmSizeId[0]["id"];

            TC_Mysql::MysqlData shmSizeData;
            shmSizeData = _mysqlRelationDB.queryRecord(sQuerySql);

            if (shmSizeData.size() == 1)
            {
                string tmp = shmSizeData[0]["config_value"];
                size_t j = 0;
                //找出配置中的内存大小数字
                while (j < tmp.size())
                {
                    if (isdigit(tmp[j]) || (tmp[j] == '.'))
                    {
                        j++;
                        continue;
                    }
                    else if ((tmp[j] == 'M') || (tmp[j] == 'm'))
                    {
                        tmp = tmp.substr(0, j);
                        memSize = TC_Common::strto<float>(tmp);
                        break;
                    }
                    else if ((tmp[j] == 'G') || (tmp[j] == 'g'))
                    {
                        tmp = tmp.substr(0, j);
                        memSize = TC_Common::strto<float>(tmp) * 1024;
                        break;
                    }
                    else
                    {
                        errmsg = "mem size config value wrong|app name:" + req.appName + "|module name:" + req.moduleName;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        break;
                    }

                    ++j;
                }

                if (memSize == 0)
                {
                    errmsg = "mem size config value wrong|app name:" + req.appName + "|module name:" + req.moduleName + "|master server name:" + masterInfo[i]["server_name"];
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    continue;
                }
            }
            else
            {
                //如果没有，就找公有的配置
                sQuerySql = "select reference_id from t_config_reference where server_name='" + masterInfo[i]["server_name"] + "' limit 1";
                TC_Mysql::MysqlData reference_idData;
                reference_idData = _mysqlRelationDB.queryRecord(sQuerySql);
                if (reference_idData.size() == 0)
                {
                    errmsg = "can not find reference_id|app name:" + req.appName + "|module name:" + req.moduleName + "|master server name:" + masterInfo[i]["server_name"] + "|sql:" + sQuerySql;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }

                sQuerySql = "select config_value from t_config_table where config_id=" + reference_idData[0]["reference_id"] + " and item_id=" + shmSizeId[0]["id"];
                TC_Mysql::MysqlData shmSizeData;
                shmSizeData = _mysqlRelationDB.queryRecord(sQuerySql);

                if (shmSizeData.size() == 1)
                {
                    string tmp=shmSizeData[0]["config_value"];
                    size_t i = 0;

                    while (i < tmp.size())
                    {
                        if (isdigit(tmp[i]) || (tmp[i] == '.'))
                        {
                            i++;
                            continue;
                        }
                        else if ((tmp[i] == 'M') || (tmp[i] == 'm'))
                        {
                            tmp = tmp.substr(0,i);
                            memSize = TC_Common::strto<float>(tmp);
                            break;
                        }
                        else if ((tmp[i] == 'G') || (tmp[i] == 'g'))
                        {
                            tmp = tmp.substr(0,i);
                            memSize = TC_Common::strto<float>(tmp) * 1024;
                            break;
                        }
                        else
                        {
                            errmsg = "mem size config value wrong|app name:" + req.appName + "|module name:" + req.moduleName;
                            TLOGERROR(FUN_LOG << errmsg << endl);
                            break;
                        }

                        ++i;
                    }

                    if (memSize == 0)
                    {
                        errmsg = "mem size config value wrong|app name:" + req.appName + "|module name:" + req.moduleName + "|master server name:" + masterInfo[i]["server_name"];
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        continue;
                    }
                }
                else
                {
                    errmsg = "not find shm size config item|app name:" + req.appName + "|module name:" + req.moduleName + "|master server name:" + masterInfo[i]["server_name"];
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    continue;
                }
            }

            totalMemSize += memSize;
        }

        rsp.avgMemSize      = TC_Common::tostr(totalMemSize / masterInfo.size());   // 主机平均内存大小
        rsp.totalMemSize    = TC_Common::tostr(totalMemSize);                       // 主机总内存
        rsp.nodeNum         = masterInfo.size();                                    // 主机节点数

    }
    catch (exception &ex)
    {
        rsp.errMsg = string("get module struct catch exception:") + ex.what() + "|app name:" + req.appName + "|module name:" + req.moduleName;
        TLOGERROR(FUN_LOG << rsp.errMsg << endl);
    }
    catch (...)
    {
        rsp.errMsg = string("get module struct catch unknown exception|app name:") + req.appName + "|module name:" + req.moduleName;
        TLOGERROR(FUN_LOG << rsp.errMsg << endl);
    }

	return 0;
}

/*
* 获取路由变更信息(包括迁移，扩容，缩容)
* cond 中的字段要和 TransferRecord 字段保持一致
  web 传的 type: 0:迁移，1:扩容，2:缩容
  t_transfer_status 默认type为 0
  t_expand_status 中的type: 1: 扩容, 2: 缩容 (新建任务时要对应)

  t_transfer_status status：0: 新建任务，1：配置阶段，2：发布阶段，3：迁移中，4：完成，5：停止
  t_expand_status   status：0: 新建任务，1：配置阶段，2：发布阶段，3：迁移中，4：完成，5：停止
  保持一致
*/
tars::Int32 DCacheOptImp::getRouterChange(const RouterChangeReq & req,RouterChangeRsp & rsp,tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "select condition:" << TC_Common::tostr(req.cond) <<  "|index:" << req.index << "|number:" << req.number << endl);

    string &errmsg = rsp.errMsg;
    try
    {
        const map<std::string, std::string> & cond = req.cond;

        //返回的结果集
        vector<DCache::TransferRecord> vTmpInfo;

        //分别是迁移和扩容的结果集
        vector<pair<size_t, DCache::TransferRecord> > vTInfo;
        vector<pair<size_t, DCache::TransferRecord> > vEInfo;

        map<std::string, std::string>::const_iterator it = cond.find("type");

        int type;
        if (it != cond.end())
        {
            type = TC_Common::strto<int>(it->second);
        }
        else
        {
            type = -1;
        }

        string condition("");
        string sSql("");

        // 没有 设置type，或者查询 迁移任务，查询 t_transfer_status 表
        if ((type == DCache::UNSPECIFIED_TYPE) || (type == DCache::TRANSFER_TYPE))
        {
            //生成条件语句
            if (cond.size() != 0)
            {
                bool isFirstCond = true;
                for (map<std::string, std::string>::const_iterator it = cond.begin(); it != cond.end(); ++it)
                {
                    if (it->first == "type")
                    {
                        // t_transfer_status 没有type字段，跳过
                        continue;
                    }

                    if (isFirstCond)
                    {
                        condition = " where ";
                        isFirstCond = false;
                    }
                    else
                    {
                        condition += " and ";
                    }

                    if (it->first == "appName")
                    {
                        condition += "app_name like '%" + it->second + "%'";
                    }
                    else if (it->first == "moduleName")
                    {
                        condition += "module_name like '%" + it->second + "%'";
                    }
                    else if (it->first == "srcGroupName")
                    {
                        condition += "src_group like '%" + it->second + "%'";
                    }
                    else if (it->first == "dstGroupName")
                    {
                        condition += "dst_group like '%" + it->second + "%'";
                    }
                    else if (it->first == "status")
                    {
                        condition += it->first + "=" + it->second;
                    }
                    else
                    {
                        errmsg = string("invalid field in select condition|field:") + it->first;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                }
            }

            //获取迁移信息
            sSql = "select *,UNIX_TIMESTAMP(auto_updatetime) as unixTime from t_transfer_status " + condition + " order by auto_updatetime desc";
            TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);

            for (size_t i = 0; i < data.size(); ++i)
            {
                DCache::TransferRecord tmpInfo;
                tmpInfo.appName         = data[i]["app_name"];
                tmpInfo.moduleName      = data[i]["module_name"];
                tmpInfo.srcGroupName    = data[i]["src_group"];
                tmpInfo.dstGroupName    = data[i]["dst_group"];
                tmpInfo.status          = (TransferStatus)TC_Common::strto<int>(data[i]["status"]);
                tmpInfo.beginTime       = data[i]["transfer_start_time"];
                tmpInfo.endTime         = data[i]["auto_updatetime"] ;
                tmpInfo.type            = DCache::TRANSFER_TYPE; // 0 迁移

                if (tmpInfo.status == TRANSFER_FINISH) // 迁移完成
                {
                    tmpInfo.progress = 100;
                }
                else
                {
                    tmpInfo.progress = 0;
                }

                //正在迁移,查询迁移进度
                if (tmpInfo.status == TRANSFERRING)
                {
                    //根据appName查询路由数据库信息
                    TC_DBConf routerDbInfo;
                    int iRet = getRouterDBInfo(tmpInfo.appName, routerDbInfo, errmsg);
                    if (iRet == -1)
                    {
                        errmsg = string("get router db info failed|appName:") + tmpInfo.appName + "|errmsg:" + errmsg;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        // 如果该应用下的所有模块全部下线，这里找不到router信息，则跳过
                        //return -1;
                        continue;
                    }

                    TC_Mysql tcMysql;
                    tcMysql.init(routerDbInfo);
                    tcMysql.connect(); //连不上时抛出异常,加上此句方便捕捉异常

                    vector<string> vPageInfo = TC_Common::sepstr<string>(data[i]["router_transfer_id"], "|");

                    sSql = "select * from t_router_transfer where module_name='" + tmpInfo.moduleName
                         + "' and group_name='" + tmpInfo.srcGroupName
                         + "' and trans_group_name='" + tmpInfo.dstGroupName
                         + "' and id in (";

                    for (size_t i = 0; i < vPageInfo.size(); ++i)
                    {
                        if (i != 0)
                        {
                            sSql += ",";
                        }

                        sSql += vPageInfo[i];
                    }
                    sSql += ")";

                    TC_Mysql::MysqlData transferData = tcMysql.queryRecord(sSql);
                    if (transferData.size() > 0)
                    {
                        int totalPageNum = 0, succPageNum = 0;
                        for (size_t i = 0; i < transferData.size(); ++i)
                        {
                            totalPageNum += TC_Common::strto<int>(transferData[i]["to_page_no"]) - TC_Common::strto<int>(transferData[i]["from_page_no"]) + 1;
                            if (transferData[i]["transfered_page_no"].size() != 0)
                            {
                                succPageNum += TC_Common::strto<int>(transferData[i]["transfered_page_no"]) - TC_Common::strto<int>(transferData[i]["from_page_no"]) + 1;
                            }
                        }

                        TLOGDEBUG(FUN_LOG << "succ num:" << succPageNum << "|totalPageNum:" << totalPageNum << "|moduleName:" << tmpInfo.moduleName << endl);
                        tmpInfo.progress = int(float(succPageNum) / float(totalPageNum) * 100);
                    }
                    else
                    {
                        TLOGERROR(FUN_LOG << "not find transfer record in t_router_transfer|module mame:" << tmpInfo.moduleName << "|src group name:" << tmpInfo.srcGroupName << "|dst group name:" << tmpInfo.dstGroupName << endl);
                    }
                }

                vTInfo.push_back(make_pair(TC_Common::strto<size_t>(data[i]["unixTime"]), tmpInfo));
            }
        }

        // 设置type，或者查询 扩容或缩容任务，查询 t_expand_status 表
        if ((type == UNSPECIFIED_TYPE) || (type == EXPAND_TYPE) || (type == REDUCE_TYPE))
        {
            //保存源组、目的组和状态条件
            string sCondSrcGroup, sCondDstGroup;
            int iStatus = -1;

            //获取扩容信息
            //生成条件语句
            condition = "";

            if (cond.size() != 0)
            {
                bool isFirstCond = true;
                for (map<std::string, std::string>::const_iterator it = cond.begin(); it != cond.end(); it++)
                {
                    //剔除源组和目的组条件
                    if (it->first == "srcGroupName")
                    {
                        sCondSrcGroup = it->second;
                        continue;
                    }

                    if (it->first == "dstGroupName")
                    {
                        sCondDstGroup = it->second;
                        continue;
                    }

                    if (isFirstCond)
                    {
                        condition = " where ";
                        isFirstCond = false;
                    }
                    else
                    {
                        condition += " and ";
                    }

                    if (it->first == "appName")
                    {
                        condition += "app_name like '%" + it->second + "%'";
                    }
                    else if (it->first == "moduleName")
                    {
                        condition += "module_name like '%" + it->second + "%'";
                    }
                    else if (it->first == "type")
                    {
                        condition += it->first + "=" + it->second;
                    }
                    else if (it->first == "status")
                    {
                        iStatus = TC_Common::strto<int>(it->second);
                        condition += it->first + "=" + it->second;
                    }
                    else
                    {
                        errmsg = string("invalid field in select condition|field:") + it->first;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                }
            }

            sSql = "select *,UNIX_TIMESTAMP(auto_updatetime) as unixTime from t_expand_status " + condition + " order by auto_updatetime desc";;

            TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
            for (size_t i = 0; i < data.size(); ++i)
            {
                DCache::TransferRecord tmpInfo;
                tmpInfo.appName     = data[i]["app_name"];
                tmpInfo.moduleName  = data[i]["module_name"];
                tmpInfo.beginTime   = data[i]["auto_updatetime"];
                tmpInfo.status      = (TransferStatus)TC_Common::strto<int>(data[i]["status"]);
                tmpInfo.type        = (TransferType)TC_Common::strto<int>(data[i]["type"]);
                tmpInfo.beginTime   = data[i]["expand_start_time"];
                tmpInfo.endTime     = data[i]["auto_updatetime"];

                // tmpInfo.type 1:扩容，2：缩容，3：路由整理

                if(tmpInfo.status == TRANSFER_FINISH)
                {
                    // status 4：完成
                    tmpInfo.progress = 100;
                }
                else
                {
                    tmpInfo.progress = 0;
                }

                if (tmpInfo.status == TRANSFERRING)
                {
                    // status 3: 迁移中，需要计算迁移进度
                    TC_DBConf routerDbInfo;
                    if (getRouterDBInfo(data[i]["app_name"], routerDbInfo, errmsg) != 0)
                    {
                       errmsg = string("get router db info failed|appName:") + tmpInfo.appName + "|errmsg:" + errmsg;
                       TLOGERROR(FUN_LOG << errmsg << endl);
                       continue;
                    }

                    //用于保存迁移组的完成情况: map<srcgroupname, map<dstgroupname, pair<totalNum, succNum> > >
                    map<string, map<string, pair<int, int> > > mGroup;

                    vector<string> tmp = TC_Common::sepstr<string>(data[i]["router_transfer_id"], "|");
                    if (tmp.size() == 0)
                    {
                       errmsg = string("not find transfer record info in t_expand_status table|app name:") + tmpInfo.appName + "|module name:" + tmpInfo.moduleName + "|type:" + etos(tmpInfo.type);
                       TLOGERROR(FUN_LOG << errmsg << endl);
                       continue;
                    }

                    TC_Mysql tcMysql;
                    tcMysql.init(routerDbInfo);
                    tcMysql.connect();

                    for (size_t j = 0; j < tmp.size(); ++j)
                    {
                        sSql = "select * from t_router_transfer where id=" + tmp[j];

                        TC_Mysql::MysqlData transferData = tcMysql.queryRecord(sSql);
                        if (transferData.size() == 0)
                        {
                           errmsg = string("not find transfer record in t_router_transfer table|app name:") + tmpInfo.appName + "|module name:" + tmpInfo.moduleName + "|id:" + tmp[j];
                           TLOGERROR(FUN_LOG << errmsg << endl);
                           continue;
                        }

                        //找符合过滤条件的组
                        if (!sCondSrcGroup.empty())
                        {
                            // 源组和提供的查询条件不一致则跳过
                            if (transferData[0]["group_name"].find(sCondSrcGroup) == string::npos)
                            {
                                continue;
                            }
                        }

                        if (!sCondDstGroup.empty())
                        {
                            // 目的组和提供的查询条件不一致则跳过
                            if (transferData[0]["trans_group_name"].find(sCondDstGroup) == string::npos)
                            {
                                continue;
                            }
                        }

                        mGroup[transferData[0]["group_name"]][transferData[0]["trans_group_name"]].first += TC_Common::strto<int>(transferData[0]["to_page_no"]) - TC_Common::strto<int>(transferData[0]["from_page_no"]) + 1;
                        if (transferData[0]["transfered_page_no"].size() != 0)
                        {
                            mGroup[transferData[0]["group_name"]][transferData[0]["trans_group_name"]].second += TC_Common::strto<int>(transferData[0]["transfered_page_no"]) - TC_Common::strto<int>(transferData[0]["from_page_no"]) + 1;
                        }

                    }

                    for (map<string, map<string, pair<int, int> > >::iterator it1 = mGroup.begin(); it1 != mGroup.end(); it1++)
                    {
                        for (map<string, pair<int, int> >::iterator it2 = it1->second.begin(); it2 != it1->second.end(); it2++)
                        {
                            DCache::TransferRecord insertInfo = tmpInfo;
                            insertInfo.srcGroupName = it1->first;
                            insertInfo.dstGroupName = it2->first;

                            insertInfo.progress = int(float(it2->second.second) / float(it2->second.first) * 100);

                            //有过滤条件
                            if ((iStatus == TRANSFERRING) && (insertInfo.progress == 100))
                            {
                               continue;
                            }

                            if (insertInfo.progress == 100)
                            {
                                insertInfo.status = TRANSFER_FINISH; // 该组迁移完成
                            }

                            vEInfo.push_back(make_pair(TC_Common::strto<size_t>(data[i]["unixTime"]), insertInfo));
                        }
                    }
                }
                else
                {
                    vEInfo.push_back(make_pair(TC_Common::strto<size_t>(data[i]["unixTime"]), tmpInfo));
                }
            }
        }

        //合并任务
        vector<pair<size_t,DCache::TransferRecord> >::iterator tIt = vTInfo.begin();
        vector<pair<size_t,DCache::TransferRecord> >::iterator eIt = vEInfo.begin();

        //合并排序
        while (true)
        {
            if (tIt == vTInfo.end())
            {
                for (; eIt != vEInfo.end(); ++eIt)
                {
                    vTmpInfo.push_back(eIt->second);
                }
                break;
            }
            else if (eIt == vEInfo.end())
            {
                for (;tIt != vTInfo.end(); tIt++)
                {
                    vTmpInfo.push_back(tIt->second);
                }
                break;
            }

            if (tIt->first > eIt->first)
            {
                vTmpInfo.push_back(tIt->second);
                tIt++;
            }
            else
            {
                vTmpInfo.push_back(eIt->second);
                eIt++;
            }
        }

        rsp.totalNum = vTmpInfo.size();

        if (rsp.totalNum == 0)
        {
            return 0;
        }

        if ((req.index < 0) || (req.index > rsp.totalNum))
        {
            errmsg = "the index of request is wrong, less than 0 or greater than total number|index:" + TC_Common::tostr(req.index);
            TLOGERROR(FUN_LOG << errmsg << endl);
            return 0;
        }

        if ((req.number == -1) || ((req.index + req.number) > rsp.totalNum))
        {
            //获取到结束的全部数据
            rsp.transferRecord.assign(vTmpInfo.begin() + req.index, vTmpInfo.end());
        }
        else if (req.number == 0)
        {
            //不获取数据
            return 0;
        }
        else
        {
            rsp.transferRecord.assign(vTmpInfo.begin() + req.index, vTmpInfo.begin() + req.index + req.number);
        }

        return 0;
    }
    catch(exception &ex)
    {
        errmsg = string("get router change info catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = string("get router change info catch unkown exception");
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

tars::Int32 DCacheOptImp::switchServer(const SwitchReq & req, SwitchRsp & rsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "new switch request|app name:" << req.appName << "|module name:"
                      << req.moduleName << "|group name:" << req.groupName << "|force switch:"
                      << req.forceSwitch << "|binlog diff time limit:" << req.diffBinlogTime << endl);

    string &errmsg = rsp.errMsg;
    try
    {
        //根据appName查询 router obj
        string routerObj;
        int iRet = getRouterObj(req.appName, routerObj, errmsg);
        if (iRet != 0)
        {
            errmsg = "get router obj info failed|appName:" + req.appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        RouterPrx routerPrx = Application::getCommunicator()->stringToProxy<RouterPrx>(routerObj);
        routerPrx->tars_timeout(3000);

        // 通知router发起切换
        iRet = routerPrx->switchByGroup(req.moduleName, req.groupName, req.forceSwitch, req.diffBinlogTime, errmsg);

        return iRet;
    }
    catch (exception &ex)
    {
        errmsg = string("active-standby switch catch an exception:") + string(ex.what()) + "|module name:" + req.moduleName + "|group name:" + req.groupName;
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = string("active-standby switch catch unkown exception") + "|module name:" + req.moduleName + "|group name:" + req.groupName;
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

tars::Int32 DCacheOptImp::getSwitchInfo(const SwitchInfoReq & req, SwitchInfoRsp & rsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "select condition:" << TC_Common::tostr(req.cond) <<  "|index:" << req.index << "|number:" << req.number << endl);

    string &errmsg = rsp.errMsg;

    try
    {
        if ((req.index < 0) || (req.number < -1))
        {
            errmsg = "the index or number of request is wrong |index:" + TC_Common::tostr(req.index) + "|number:" + TC_Common::tostr(req.number);
            TLOGERROR(FUN_LOG << errmsg << endl);
            return 0;
        }

        const map<std::string, std::string> & cond = req.cond;

        //根据查询条件cond的内容构造sql, t_router_switch表在 db_dcache_relation库中
        string sql = "select * from t_router_switch ";

        for (map<string, string>::const_iterator iter = cond.begin(); iter != cond.end(); ++iter)
        {
            if (iter == cond.begin())
            {
                sql += " where ";
            }
            else
            {
                sql += " and ";
            }

            if (iter->first == "appName")
            {
                sql += "app_name like '%" + iter->second + "%'";
            }
            else if (iter->first == "moduleName")
            {
                sql += "module_name like'%" + iter->second + "%'";
            }
            else if (iter->first == "groupName")
            {
                sql += "group_name like '%" + iter->second + "%'";
            }
            else if (iter->first == "masterServer")
            {
                sql += "master_server like '%" + iter->second + "%'";
            }
            else if (iter->first == "slaveServer")
            {
                sql += "slave_server like '%" + iter->second + "%'";
            }
            else if (iter->first == "mirrorIdc")
            {
                sql += "mirror_idc like '%" + iter->second + "%'";
            }
            else if (iter->first == "masterMirror")
            {
                sql += "master_mirror like '%" + iter->second + "%'";
            }
            else if (iter->first == "slaveMirror")
            {
                sql += "slave_mirror like '%" + iter->second + "%'";
            }
            else if (iter->first == "switchType")
            {
                sql += "switch_type=" + iter->second;
            }
            else if(iter->first == "switchResult")
            {
                sql += "switch_result=" + iter->second;
            }
            else if(iter->first == "groupStatus")
            {
                sql += "access_status=" + iter->second;
            }
            else if (iter->first == "switch_time")
            {
                vector<string> vTimes;
                vTimes = TC_Common::sepstr<string>(iter->second, "|", false);
                if (vTimes.size() != 2)
                {
                    errmsg = "switch time condition set wrongly:" + iter->second;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }

                sql += "(switch_time>='" + vTimes[0] + "' and switch_time<='" + vTimes[1] + "')";
            }
            else
            {
                errmsg = "unknown condition in switch info request:" + iter->first + "|" + iter->second;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }
        }

        sql += " order by id desc ";

        TC_Mysql::MysqlData switchInfoData = _mysqlRelationDB.queryRecord(sql);

        // 返回记录总数
        rsp.totalNum = switchInfoData.size();

        size_t end = (req.number == -1) ? switchInfoData.size() : (req.number + req.index);

        for (size_t i = req.index; i < switchInfoData.size() && i < end; ++i)
        {
            SwitchRecord info;
            info.appName        = switchInfoData[i]["app_name"];
            info.moduleName     = switchInfoData[i]["module_name"];
            info.groupName      = switchInfoData[i]["group_name"];
            info.masterServer   = switchInfoData[i]["master_server"];
            info.slaveServer    = switchInfoData[i]["slave_server"];
            info.mirrorIdc      = switchInfoData[i]["mirror_idc"];
            info.masterMirror   = switchInfoData[i]["master_mirror"];
            info.slaveMirror    = switchInfoData[i]["slave_mirror"];
            info.switchTime     = switchInfoData[i]["switch_time"];
            info.modifyTime     = switchInfoData[i]["modify_time"];
            info.comment        = switchInfoData[i]["comment"];
            info.switchProperty = switchInfoData[i]["switch_property"];
            info.switchType     = TC_Common::strto<int>(switchInfoData[i]["switch_type"]);
            info.switchResult   = TC_Common::strto<int>(switchInfoData[i]["switch_result"]);
            info.groupStatus    = TC_Common::strto<int>(switchInfoData[i]["access_status"]);

            string serverName = info.masterServer != "" ? info.masterServer : info.masterMirror;

            int iRet = getCacheConfigFromDB(serverName, info.appName, info.moduleName, info.groupName, info.enableErase, info.dbFlag, info.memSize, errmsg);
            if (iRet != 0)
            {
                continue;
            }

            rsp.switchRecord.push_back(info);
        }

        errmsg = "success";

        return 0;
    }
    catch(exception &ex)
    {
        errmsg = string("get switch info catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = string("get switch info catch unkown exception");
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

tars::Int32 DCacheOptImp::stopTransfer(const StopTransferReq& req, StopTransferRsp &rsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name:" << req.appName << "|module name:" << req.moduleName << "|transfer type:" << etos(req.type) << endl);

    string & errmsg = rsp.errMsg;
    try
    {
        int iRet = 0;

        if (req.type == DCache::TRANSFER_TYPE)
        {
            // transfer
            if (req.srcGroupName.size() < 1 || req.dstGroupName.size() < 1)
            {
                rsp.errMsg = string("stop transfer request wrong, src group or dst group is empty|src group size:")
                           + TC_Common::tostr(req.srcGroupName.size()) + "|dst group size:" + TC_Common::tostr(req.dstGroupName.size());

                return -1;
            }

            iRet = stopTransferForTransfer(req.appName, req.moduleName, req.srcGroupName, req.dstGroupName, rsp.errMsg);
        }
        else if (req.type == DCache::EXPAND_TYPE || req.type == DCache::REDUCE_TYPE)
        {
            // expand or reduce

            iRet = stopTransferForExpandReduce(req.appName, req.moduleName, req.type, rsp.errMsg);
        }

        return iRet;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("stop transfer catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

tars::Int32 DCacheOptImp::restartTransfer(const RestartTransferReq& req, RestartTransferRsp &rsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name:" << req.appName << "|module name:" << req.moduleName << "|transfer type:" << etos(req.type) << endl);

    string & errmsg = rsp.errMsg;
    try
    {
        int iRet = 0;

        if (req.type == DCache::TRANSFER_TYPE)
        {
            // transfer
            if (req.srcGroupName.size() < 1 || req.dstGroupName.size() < 1)
            {
                rsp.errMsg = string("restart transfer request wrong, src group or dst group is empty|src group size:")
                           + TC_Common::tostr(req.srcGroupName.size()) + "|dst group size:" + TC_Common::tostr(req.dstGroupName.size());

                return -1;
            }

            // 重启迁移任务，则是重新创建迁移任务
            iRet = insertTransfer2RouterDb(req.appName, req.moduleName, req.srcGroupName, req.dstGroupName, true, rsp.errMsg);
        }
        else if (req.type == DCache::EXPAND_TYPE || req.type == DCache::REDUCE_TYPE)
        {
            // expand or reduce
            iRet = restartTransferForExpandReduce(req.appName, req.moduleName, req.type, rsp.errMsg);
        }

        return iRet;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("restart transfer catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}


tars::Int32 DCacheOptImp::deleteTransfer(const DeleteTransferReq& req, DeleteTransferRsp& rsp,tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name:" << req.appName << "|module name:" << req.moduleName << "|transfer type:" << etos(req.type) << endl);

    string & errmsg = rsp.errMsg;
    try
    {
        int iRet = 0;

        //删除迁移任务记录
        if (req.type == DCache::TRANSFER_TYPE)
        {
            iRet = deleteTransferForTransfer(req.appName, req.moduleName, req.srcGroupName, req.dstGroupName, errmsg);
        }
        else if (req.type == DCache::EXPAND_TYPE || req.type == DCache::REDUCE_TYPE)
        {
            iRet = deleteTransferForExpandReduce(req.appName, req.moduleName, req.type, errmsg);
        }
        else
        {
            errmsg = "delete transfer record request has unknow type:" + TC_Common::tostr(req.type);
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        return iRet;
    }
    catch(exception &ex)
    {
        errmsg = string("delete transfer record catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

tars::Int32 DCacheOptImp::recoverMirrorStatus(const RecoverMirrorReq& req, RecoverMirrorRsp & rsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name:" << req.appName << "|module name:" << req.moduleName << "|group name:" << req.groupName << "|mirror idc:" << req.mirrorIdc << endl);

    string & errmsg = rsp.errMsg;
    try
    {
        //根据appName查询 router obj
        string routerObj;
        int iRet = getRouterObj(req.appName, routerObj, errmsg);
        if (iRet != 0)
        {
            errmsg = "get router obj info failed|appName:" + req.appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        RouterPrx routerPrx = Application::getCommunicator()->stringToProxy<RouterPrx>(routerObj);
        routerPrx->tars_timeout(3000);

        iRet = routerPrx->recoverMirrorStat(req.moduleName, req.groupName, req.mirrorIdc, errmsg);

        return iRet;
    }
    catch (exception &ex)
    {
        errmsg = string("recover mirrot status catch an exception:") + string(ex.what()) + "|module name:" + req.moduleName + "|group name:" + req.groupName + "|mirror idc:" + req.mirrorIdc;
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = string("recover mirrot status catch unkown exception") + "|module name:" + req.moduleName + "|group name:" + req.groupName + "|mirror idc:" + req.mirrorIdc;
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

/*
* 获取服务列表
*/
tars::Int32 DCacheOptImp::getCacheServerList(const CacheServerListReq& req, CacheServerListRsp& rsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG(FUN_LOG << "app name:" << req.appName << "|module name:" << req.moduleName << endl);
    std::string &errmsg = rsp.errMsg;

    try
    {
        TC_DBConf routerDbInfo;
        int iRet = getRouterDBInfo(req.appName, routerDbInfo, errmsg);
        if (iRet == -1)
        {
            errmsg = "get router db info failed, maybe the selected app is not existed|app name:" + req.appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        TC_Mysql tcMysql;
        tcMysql.init(routerDbInfo);
        tcMysql.connect(); //连不上时抛出异常,加上此句方便捕捉异常


        string sSql = "select cacheType from t_config_appMod where appName='" + req.appName + "' and moduleName='" + req.moduleName + "'";
        TC_Mysql::MysqlData dataCacheType = _mysqlRelationDB.queryRecord(sSql);
        if (dataCacheType.size() > 0)
        {
            stoe(dataCacheType[0]["cacheType"], rsp.cacheType);
        }
        else
        {
            TLOGDEBUG(FUN_LOG << "the record of the selected module is not existed, app name:" << req.appName << "|module name:" << req.moduleName << endl);
            return 0;
        }

        sSql = "select * from t_cache_router where app_name='" + req.appName + "' and module_name='" + req.moduleName + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);

        for (size_t i = 0; i < data.size(); ++i)
        {
            rsp.cacheServerList.push_back(CacheServerInfo());
            CacheServerInfo& cacheServer = rsp.cacheServerList.back();
            cacheServer.serverName  = data[i]["cache_name"];
            cacheServer.serverIp    = data[i]["cache_ip"];
            cacheServer.serverType  = data[i]["server_status"];
            cacheServer.groupName   = data[i]["group_name"];
            cacheServer.idcArea     = data[i]["idc_area"];
            cacheServer.memSize     = data[i]["mem_size"];

            iRet = getCacheGroupRouterPageNo(tcMysql, cacheServer.groupName, cacheServer.routerPageNo, errmsg);
            if (iRet != 0)
            {
                errmsg = string("get group router page number failed|errmsg:") + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
            }
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("get cache server list catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

/*
* 配置中心操作接口
*/
tars::Int32 DCacheOptImp::addCacheConfigItem(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        string sSql = "select id from t_config_item where item='" + configReq.item + "' and path='" + configReq.path + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            errmsg = string("the record of t_config_item is already exists, item:") + configReq.item + ", path:" + configReq.path;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        map<string, pair<TC_Mysql::FT, string> > m;
        m["remark"] = make_pair(TC_Mysql::DB_STR, configReq.remark);
        m["item"]   = make_pair(TC_Mysql::DB_STR, configReq.item);
        m["path"]   = make_pair(TC_Mysql::DB_STR, configReq.path);
        m["reload"] = make_pair(TC_Mysql::DB_STR, configReq.reload);
        m["period"] = make_pair(TC_Mysql::DB_STR, configReq.period);

        _mysqlRelationDB.insertRecord("t_config_item", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("add config item to table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::updateCacheConfigItem(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m_update;
        m_update["remark"] = make_pair(TC_Mysql::DB_STR, configReq.remark);
        m_update["reload"] = make_pair(TC_Mysql::DB_STR, configReq.reload);
        m_update["period"] = make_pair(TC_Mysql::DB_STR, configReq.period);

        string condition = "where id=" + configReq.id;

        _mysqlRelationDB.updateRecord("t_config_item", m_update, condition);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("update config item on table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::deleteCacheConfigItem(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        string condition = "where id=" + configReq.id;

        _mysqlRelationDB.deleteRecord("t_config_item", condition);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("delete config item from table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::getCacheConfigItemList(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;

    try
    {
        string selectSql = "select * from t_config_item order by path, item";

        TC_Mysql::MysqlData itemData = _mysqlRelationDB.queryRecord(selectSql);
        configRsp.configItemList = itemData.data();
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get config item from table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::addServerConfigItem(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        string configId("");
        string level("");
        string serverName("");
        string nodeName("");
        if (configReq.serverName.size() > 0 && configReq.nodeName.size() > 0)
        {
            // serverName和nodeName不为空，则表示增加单个服务节点的配置项
            // configId 设置为0，level设置为2
            configId = "0";
            level = "2";
            serverName = configReq.serverName;
            nodeName = configReq.nodeName;
        }
        else
        {
            // serverName或nodeName为空，则表示增加模块的公共配置项
            // serverName 设置为模块名，nodeName设置为空，level设置为1
            int appModConfigId = 0;
            if (0 != getAppModConfigId(configReq.appName, configReq.moduleName, appModConfigId, errmsg))
            {
                errmsg = string("get app-mod config id failed, errmsg:") + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }
            configId = TC_Common::tostr(appModConfigId);
            level = "1";
            serverName = configReq.moduleName;
            nodeName = "";
        }

        map<string, pair<TC_Mysql::FT, string> > m;
        m["config_id"]    = make_pair(TC_Mysql::DB_INT, configId);
        m["server_name"]  = make_pair(TC_Mysql::DB_STR, serverName);
        m["host"]         = make_pair(TC_Mysql::DB_STR, nodeName);
        m["item_id"]      = make_pair(TC_Mysql::DB_INT, configReq.itemId);
        m["config_value"] = make_pair(TC_Mysql::DB_STR, configReq.configValue);
        m["level"]        = make_pair(TC_Mysql::DB_INT, level);
        m["posttime"]     = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]     = make_pair(TC_Mysql::DB_STR, configReq.lastUser);
        m["config_flag"]  = make_pair(TC_Mysql::DB_INT, configReq.configFlag); // 这个配置没啥用，建议删除，建表语句把这个字段也删除

        _mysqlRelationDB.insertRecord("t_config_table", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("add module or server config item to table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::updateServerConfigItem(const ServerConfigReq& configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m_update;
        m_update["config_value"] = make_pair(TC_Mysql::DB_STR, configReq.configValue);
        m_update["lastuser"]     = make_pair(TC_Mysql::DB_STR, configReq.lastUser);

        string condition = "where id=" + configReq.indexId;

        _mysqlRelationDB.updateRecord("t_config_table", m_update, condition);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("update config item on table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::updateServerConfigItemBatch(const vector<ServerConfigReq> & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        for (size_t i = 0; i < configReq.size(); ++i)
        {
            if (0 != updateServerConfigItem(configReq[i], configRsp, current))
            {
                return -1;
            }
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("update config item on table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::deleteServerConfigItem(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        string condition = "where id=" + configReq.indexId;

        _mysqlRelationDB.deleteRecord("t_config_table", condition);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("delete config item from table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::deleteServerConfigItemBatch(const vector<ServerConfigReq> & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;
    try
    {
        for (size_t i = 0; i < configReq.size(); ++i)
        {
            if (0 != deleteServerConfigItem(configReq[i], configRsp, current))
            {
                return -1;
            }
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("delete config item from table t_config_table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::getServerNodeConfigItemList(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;

    try
    {
        string selectSql = "select ct.*, ci.remark, ci.item, ci.path, ci.reload, ci.period from t_config_table ct join t_config_item ci on ct.item_id = ci.id where server_name='" +  configReq.serverName + "' and host='" + configReq.nodeName + "'";

        TC_Mysql::MysqlData itemListData = _mysqlRelationDB.queryRecord(selectSql);
        configRsp.configItemList = itemListData.data();
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get config item from table t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::getServerConfigItemList(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current)
{
    std::string &errmsg = configRsp.errMsg;

    try
    {
        int appModConfigId = 0;
        if (0 != getAppModConfigId(configReq.appName, configReq.moduleName, appModConfigId, errmsg))
        {
            errmsg = string("get app-mod config id failed, errmsg:") + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        // 查询模块的公共配置
        string selectSql = "select ct.*, ci.remark, ci.item, ci.path, ci.reload, ci.period from t_config_table ct join t_config_item ci on ct.item_id = ci.id where ct.config_id=" + TC_Common::tostr(appModConfigId);

        TC_Mysql::MysqlData itemListData = _mysqlRelationDB.queryRecord(selectSql);
        configRsp.configItemList = itemListData.data();

        if (configReq.serverName.size() > 0 && configReq.nodeName.size() > 0)
        {
            // serverName 和 nodeName，则需要查询服务节点的配置项
            ConfigRsp serverConfigRsq;
            int iRet = getServerNodeConfigItemList(configReq, serverConfigRsq, current);
            if (iRet != 0)
            {
                errmsg = string("get server config item failed, errmsg:" + serverConfigRsq.errMsg);
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            // 合并 模块公共配置和服务节点配置
            configRsp.configItemList.insert(configRsp.configItemList.end(), serverConfigRsq.configItemList.begin(), serverConfigRsq.configItemList.end());
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get module or server config item list from table t_config_table and t_config_item catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

tars::Int32 DCacheOptImp::queryProperptyData(const DCache::QueryPropReq & req,vector<DCache::QueryResult> &rsp,tars::TarsCurrentPtr current)
{
    ostringstream os("");
    req.displaySimple(os);
    TLOGDEBUG(FUN_LOG << "request content:" << os.str() << endl);

    string errmsg("");
    try
    {
        const QueryPropCond& cond = reinterpret_cast<const QueryPropCond &>(req);
        vector<QueriedResult> data;

        int iRet = _propertyPrx->queryPropData(cond, data);
        if (iRet == 0)
        {
            char* pData = (char *)&data;
            rsp = *(reinterpret_cast<vector<DCache::QueryResult> *>(pData));

            return 0;
        }
    }
    catch (const std::exception &ex)
    {
        errmsg = string("query cache properpty data catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return -1;
}

/*
*------------------------------
*      private function
*------------------------------
*/
bool DCacheOptImp::checkRouterLoadModule(const std::string & sApp, const std::string & sModuleName, const std::string & sRouterServerName, std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "app:" << sApp << "|module name:" << sModuleName << "|router server name:" << sRouterServerName << endl);

    try
    {
        int times = 0;
        while(times < 2)
        {
            if (sApp.empty() || sRouterServerName.empty())
            {
                errmsg = "app name and router server name can not be empty";
                TLOGERROR(FUN_LOG << errmsg << endl);
                return false;
            }

            RouterPrx routerPrx;
            routerPrx = _communicator->stringToProxy<RouterPrx>(sRouterServerName + string(".RouterObj"));
            vector<TC_Endpoint> vEndPoints = routerPrx->getEndpoint4All();

            set<string> nodeIPset;
            for (vector<TC_Endpoint>::size_type i = 0; i < vEndPoints.size(); i++)
            {
                //获取所有节点ip
                nodeIPset.insert(vEndPoints[i].getHost());
            }

            set<string> reloadNodeIP;
            string info;
            set<string>::iterator pos;
            for (pos = nodeIPset.begin(); pos != nodeIPset.end(); ++pos)
            {
                _adminproxy->notifyServer(sApp, sRouterServerName.substr(7), *pos, "router.checkModule " + sModuleName, info);

                if (info.find("not support") != string::npos)
                {
                    TLOGERROR(FUN_LOG << "module is not support, need reload router|router server name:" << sRouterServerName << "|moduleName:" << sModuleName << "|notify server result:" << info << endl);
                    reloadNodeIP.insert(*pos);
                }
            }

            if (reloadNodeIP.empty())
            {
                break;
            }

            if (times == 0)
            {
                reloadRouterByModule(sApp, sModuleName, sRouterServerName, reloadNodeIP, errmsg);
                times++;
                sleep(10);
                continue;
            }
            else
            {
                errmsg = string("router load module:") + sModuleName + "'s routing failed, please check!";
                TLOGERROR(FUN_LOG << errmsg << endl);
                return false;
            }
        }
    }
    catch(const TarsException& ex)
    {
        errmsg = string("get server endpoint catch exception|server name:") + sRouterServerName + "|exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        return false;
    }

    errmsg = "";
    return true;
}

bool DCacheOptImp::reloadRouterByModule(const std::string & sApp, const std::string & sModuleName, const std::string & sRouterServerName, const set<string>& nodeIPset, std::string &errmsg)
{
    try
    {
        if (sApp.empty() || sRouterServerName.empty())
        {
            errmsg = "app name and router server name can not be empty";
            TLOGERROR(FUN_LOG << errmsg << endl);
            return false;
        }

        set<string>::iterator pos;
        for (pos = nodeIPset.begin(); pos != nodeIPset.end(); ++pos)
        {
            //通知router重新加载配置
            _adminproxy->notifyServer(sApp, sRouterServerName.substr(7), *pos, "router.reloadRouterByModule " + sModuleName, errmsg);
        }
    }
    catch(const TarsException& ex)
    {
        errmsg = string("reload router by module name catch TarsException:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        return false;
    }

    errmsg = "";
    return true;
}

tars::Bool DCacheOptImp::reloadRouterConfByModuleFromDB(const std::string & sApp, const std::string & sModuleName, const std::string & sRouterServerName, std::string &errmsg, tars::TarsCurrentPtr current)
{
    try
    {
        if (sApp.empty() || sRouterServerName.empty())
        {
            errmsg = "app name and router server name can not be empty";
            TLOGERROR(FUN_LOG << errmsg << endl);
            return false;
        }

        RouterPrx routerPrx;
        routerPrx = _communicator->stringToProxy<RouterPrx>(sRouterServerName + string(".RouterObj"));
        vector<TC_Endpoint> vEndPoints = routerPrx->getEndpoint4All();

        set<string> nodeIPset;
        for (vector<TC_Endpoint>::size_type i = 0; i < vEndPoints.size(); i++)
        {
            //获取所有节点ip
            nodeIPset.insert(vEndPoints[i].getHost());
        }

        set<string>::iterator pos;
        for (pos = nodeIPset.begin(); pos != nodeIPset.end(); ++pos)
        {
            //通知router重新加载配置
            _adminproxy->notifyServer(sApp, sRouterServerName.substr(7), *pos, "router.reloadRouterByModule " + sModuleName, errmsg);
        }
    }
    catch(const TarsException& ex)
    {
        errmsg = string("reload router by module name catch TarsException:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        return false;
    }

    errmsg = "";
    return true;
}

int DCacheOptImp::reloadRouterByModule(const std::string & app, const std::string & moduleName, const std::string & routerName, std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "app:" << app << "|module name:" << moduleName << "|router server name:" << routerName << endl);

    try
    {
        if (app.empty() || routerName.empty())
        {
            errmsg = "app and router server name can not be empty|app:" + app + "|router name:" + routerName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        RouterPrx routerPrx;
        routerPrx = _communicator->stringToProxy<RouterPrx>(routerName + string(".RouterObj"));
        vector<TC_Endpoint> vEndPoints = routerPrx->getEndpoint4All();

        set<string> nodeIPset;
        for (vector<TC_Endpoint>::size_type i = 0; i < vEndPoints.size(); i++)
        {
            //获取所有节点ip
            nodeIPset.insert(vEndPoints[i].getHost());
        }

        set<string>::iterator pos;
        for (pos = nodeIPset.begin(); pos != nodeIPset.end(); ++pos)
        {
            //通知router重新加载配置
            _adminproxy->notifyServer(app, routerName.substr(7), *pos, "router.reloadRouterByModule " + moduleName, errmsg);
        }

        return 0;
    }
    catch (const TarsException& ex)
    {
        errmsg = string("reload router by module name catch TarsException:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::createRouterDBConf(const RouterParam &param, string &errmsg)
{
    //string t_sOperation = "replace";

    try
    {
        /*string t_sdbConnectFlag = param.dbIp + string("_") + param.dbName; //"ip_数据库名"
        string t_sPostTime = TC_Common::now2str("%Y-%m-%d %H:%M:%S");

        map<string, pair<TC_Mysql::FT, string> > m;
        TC_Mysql::MysqlData t_tcConnData = _mysqlRouterConfDb.queryRecord(string("select * from t_config_dbconnects where name='") + t_sdbConnectFlag + string("'"));
        if (t_tcConnData.size() == 0)
        {
            m["NAME"]       = make_pair(TC_Mysql::DB_STR, t_sdbConnectFlag);
            m["HOST"]       = make_pair(TC_Mysql::DB_STR, param.dbIp);
            m["PORT"]       = make_pair(TC_Mysql::DB_STR, param.dbPort);
            m["USER"]       = make_pair(TC_Mysql::DB_STR, param.dbUser);
            m["PASSWORD"]   = make_pair(TC_Mysql::DB_STR, param.dbPwd);
            m["CHARSET"]    = make_pair(TC_Mysql::DB_STR, "utf8");
            m["DBNAME"]     = make_pair(TC_Mysql::DB_STR, param.dbName);
            m["ABOUT"]      = make_pair(TC_Mysql::DB_STR, param.remark);
            m["POSTTIME"]   = make_pair(TC_Mysql::DB_STR, t_sPostTime);
            m["LASTUSER"]   = make_pair(TC_Mysql::DB_STR, "system");

            _mysqlRouterConfDb.insertRecord("t_config_dbconnects", m);
        }
        else
        {
            m["NAME"]       = make_pair(TC_Mysql::DB_STR, t_sdbConnectFlag);
            m["HOST"]       = make_pair(TC_Mysql::DB_STR, param.dbIp);
            m["PORT"]       = make_pair(TC_Mysql::DB_STR, param.dbPort);
            m["USER"]       = make_pair(TC_Mysql::DB_STR, param.dbUser);
            m["PASSWORD"]   = make_pair(TC_Mysql::DB_STR, param.dbPwd);
            m["CHARSET"]    = make_pair(TC_Mysql::DB_STR, "utf8");
            m["DBNAME"]     = make_pair(TC_Mysql::DB_STR, param.dbName );
            m["ABOUT"]      = make_pair(TC_Mysql::DB_STR, param.remark);
            m["POSTTIME"]   = make_pair(TC_Mysql::DB_STR, t_sPostTime);
            m["LASTUSER"]   = make_pair(TC_Mysql::DB_STR, "system");

            _mysqlRouterConfDb.replaceRecord("t_config_dbconnects", m);
        }*/

        //创建路由数据库
        createRouterDB(param, errmsg);

        /*string sql = string("select ID from t_config_dbconnects where name='") + t_sdbConnectFlag + "' limit 1 ";
        TC_Mysql::MysqlData data = _mysqlRouterConfDb.queryRecord(sql);

        if (data.size() == 0)
        {
            errmsg = string("select ID from t_config_dbconnects size=0 [t_sdbConnectFlag=") + t_sdbConnectFlag + string("]");
            TLOGERROR(errmsg << endl);
            return -1;
        }
        else
        {
            string ID = data[0]["ID"];
            string t_sqlContent("select * from t_router_group order by id asc ");
            string columns(" OPNAME,QUERYKEY, QUERYPOSTTIME,QUERYLASTUSER,SERVERNAME, DBCONNECTID,PAGESIZE,`EXECUTESQL`,ORDERSQL,TABLENAME,OTHERCALL,POSTTIME,LASTUSER ");
            string t_sLastUser = "system";
            sql = t_sOperation + string(" into t_config_querys (")      + columns + string(")")
                + string(" values('DCache.RouterServer.RouterModule.")  + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'select * from  t_router_module order by id asc ','','t_router_module','','") + t_sPostTime + string("','") + t_sLastUser + string("'),")
                + string(" ('DCache.RouterServer.RouterTransfer.")      + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'select * from t_router_transfer order by id asc ','','t_router_transfer','','") + t_sPostTime + string("','") + t_sLastUser + string("'),")
                + string(" ('DCache.RouterServer.RouterRecord.")        + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'select * from t_router_record order by id asc ','','t_router_record','','") + t_sPostTime + string("','") + t_sLastUser + string("'),")
                + string(" ('DCache.RouterServer.RouterGroup.")         + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'") + t_sqlContent + string("','','t_router_group','','") +t_sPostTime + string("','") + t_sLastUser + string("'),")
                + string(" ('DCache.RouterServer.RouterServer.")        + param.appName + string("','id','POSTTIME','LASTUSER','',") + ID + string(",20,'select * from t_router_server order by id asc ','','t_router_server','','") + t_sPostTime + string("','") + t_sLastUser + string("') ");

            string sql4Module("select * from t_config_querys where opname in ('DCache.GetModulesNew')");
            if (_mysqlRouterConfDb.queryRecord(sql4Module).size() == 0) //数据库中没有，插入
            {
                sql += string(" ,('DCache.GetModulesNew','id','POSTTIME','LASTUSER','',0,100,'select concat(\\'DCache.\\',module_name) module_name,server_name from t_router_group ','','t_router_group','','") + t_sPostTime + string("','system')")   ;
            }

            _mysqlRouterConfDb.execute(sql);
            MYSQL *pMysql = _mysqlRouterConfDb.getMysql();
            MYSQL_RES *result = mysql_store_result(pMysql);
            mysql_free_result(result);

            string t_sOpNames = string("'DCache.RouterServer.RouterModule.")  + param.appName+ string("', ")
                              + string("'DCache.RouterServer.RouterTransfer.")+ param.appName + string("', ")
                              + string("'DCache.RouterServer.RouterRecord.")  + param.appName + string("', ")
                              + string("'DCache.RouterServer.RouterGroup.")   + param.appName + string("', ")
                              + string("'DCache.RouterServer.RouterServer.")  + param.appName + string("' ");
            sql = string(" select id, opname from t_config_querys where opname in (") + t_sOpNames+ string(") ") ;

            TC_Mysql::MysqlData data5 = _mysqlRouterConfDb.queryRecord(sql);
            if (5 == data5.size())
            {
                string t_sFields = "QUERYID,NAME,SEARCHNAME,ABOUT,ISMODIFY,ISSEARCH,ISVISIBLE,ISEQUAL,ISNULL,ORDERBY,EDITWIDTH,EDITHEIGHT,SEARCHSIZE,DATATYPE,RAWVALUE,OPTIONS,POSTTIME,LASTUSER";
                for (size_t i=0; i<data5.size(); i++)
                {
                    string t_ID =  data5[i]["id"];
                    string t_OPNAME = data5[i]["opname"];

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterModule"))
                    {
                        sql = t_sOperation + string("  into t_config_fields (") + t_sFields + string(") ")
                            + string(" values (") + t_ID + string(",'module_name','module_name','业务模块名','1','1','1','0','0',1,400,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'version','version','版本号','1','1','1','0','0',2,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'switch_status','status','模块状态','1','1','1','0','0',2,100,0,10,4,'','0|读写自动切换;1|只读自动切换;2|不支持自动切换;3|无效模块','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'remark','remark','备注','1','1','1','0','1',3,400,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','1','0','0','1',4,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterTransfer"))
                    {
                        sql = t_sOperation + string("  into t_config_fields (") + t_sFields + string(") ")
                            + string(" values (") + t_ID +string(",'from_page_no','from_page_no','迁移开始页','1','1','1','0','0',2,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'group_name','group_name','迁移原服务器组名','1','1','1','0','0',4,400,60,10,4,'','select group_name as S1, group_name as S2 from t_router_group','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','1','0','0','1',9,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'startTime','startTime','开始时间','1','1','1','0','1',9,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'endTime','endTime','结束时间','1','1','1','0','1',9,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'module_name','module_name','业务模块名','1','1','1','0','0',1,400,0,10,4,'','select module_name as S1, module_name as S2 from t_router_module','")  + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'remark','remark','迁移结果描述','1','1','1','0','1',7,400,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'state','state','迁移状态','1','1','1','0','0',8,100,0,10,4,'','0|未迁移;1|正在迁移;2|迁移结束;4|停止迁移','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'to_page_no','to_page_no','迁移结束页','1','1','1','0','0',3,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'transfered_page_no','transfered_page_no','已迁移成功页','1','1','1','0','1',6,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'trans_group_name','trans_group_name','迁移目的服务器组名','1','1','1','0','0',5,400,60,10,4,'','select group_name as S1, group_name as S2 from t_router_group','") + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterRecord"))
                    {
                        sql = t_sOperation + string(" into t_config_fields (") + t_sFields + string(") ")
                            + string(" values (") + t_ID + string(",'from_page_no','from_page_no','开始页','1','1','1','0','0',2,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'group_name','group_name','路由目的服务器组名','1','1','1','0','0',4,400,60,10,4,'','select group_name as S1, group_name as S2 from t_router_group','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'module_name','module_name','业务模块名','1','1','1','0','0',1,400,0,10,4,'','select module_name as S1, module_name as S2 from t_router_module','")  + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','1','0','0','1',5,100,0,10,9,'','','" ) + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'to_page_no','to_page_no','结束页','1','1','1','0','0',3,100,0,10,1,'','','")  + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterGroup"))
                    {
                        sql = t_sOperation + string("  into t_config_fields (") + t_sFields + string(") ")
                            + string(" values(") + t_ID + string(",'group_name','group_name','服务器组名','1','1','1','0','0',2,400,0,10,1,'','','")  + t_sPostTime + string("','dcache_init_fields.sh') ,")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','0','0','0','0',5,100,0,10,9,'','','")  + t_sPostTime +string( "','dcache_init_fields.sh') ,")
                            + string(" (") + t_ID + string(",'module_name','module_name','业务模块名','1','1','1','0','0',1,400,60,10,4,'','select module_name as S1, module_name as S2 from t_router_module','")  + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'pri','pri','优先级','1','1','1','0','1',5,150,0,15,1,'','','")  + t_sPostTime + string("','dcache_init_fields.sh' ),")
                            + string(" (") + t_ID + string(",'server_name','server_name','服务器名','1','1','1','0','0',3,400,60,10,4,'','select server_name as S1, server_name as S2 from t_router_server union all select \\'NULL\\' as S1, \\'NULL\\' as S2 from t_router_server','")  + t_sPostTime + string("','dcache_init_fields.sh' ),")
                            + string(" (") + t_ID + string(",'server_status','server_status','服务类型','1','1','1','0','0',4,400,60,10,4,'','M|主机;S|备机;I|镜像主机;B|镜像备机','")  + t_sPostTime + string("','dcache_init_fields.sh' ),")
                            + string(" (") + t_ID + string(",'access_status','access_status','访问状态','1','1','1','0','0',2,100,0,10,4,'','0|正常状态;1|只读状态;2|镜像切换状态','")  + t_sPostTime + string("','dcache_init_fields.sh' ),")
                            + string(" (") + t_ID + string(",'source_server_name','source_server_name','备份源Server','1','1','1','0','1',6,150,0,15,1,'','','")  + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    if (string::npos !=  t_OPNAME.find("DCache.RouterServer.RouterServer"))
                    {
                        sql = t_sOperation + string(" into t_config_fields (") + t_sFields + string(") ")
                            + string(" values(") + t_ID + string(",'binlog_port','binlog_port','BinLogObj服务端口','1','1','1','0','0',3,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'cache_port','cache_port','cache_port服务端口','1','1','1','0','0',4,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'wcache_port','cache_port','wcache_port服务端口','1','1','1','0','0',4,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'backup_port','backup_port','backup_port服务端口','1','1','1','0','0',4,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'ip','ip','服务器IP','1','1','1','0','0',2,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'modify_time','modify_time','最后修改时间','0','1','0','0','1',7,100,0,10,9,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'remark','remark','备注','1','1','1','0','1',6,400,0,10,1,'','','" )+ t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'routeclient_port','routeclient_port','RouterClientObj服务端口','1','1','1','0','0',5,100,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'server_name','server_name','服务器名','1','1','1','0','0',1,400,0,10,1,'','','") + t_sPostTime + string("','dcache_init_fields.sh'),")
                            + string(" (") + t_ID + string(",'idc_area','idc_area','IDC地区','1','1','1','0','0',0,150,0,15,4,'','sz|深圳;bj|北京;sh|上海;nj|南京;hk|香港;cd|成都','") + t_sPostTime + string("','dcache_init_fields.sh')");
                    }

                    _mysqlRouterConfDb.execute(sql);

                    pMysql = _mysqlRouterConfDb.getMysql();
                    result = mysql_store_result(pMysql);
                    mysql_free_result(result);
                }
            }
        }*/
    }
    catch(const std::exception &ex)
    {
        errmsg = string("create router DB conf catch expcetion:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }
    catch (...)
    {
        errmsg = "create router DB conf catch unknown expcetion";
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::createRouterDB(const RouterParam &param, string &errmsg)
{
    TC_Mysql tcMysql;
    int iRet = 0;

    try
    {
        tcMysql.init(param.dbIp, param.dbUser, param.dbPwd, "", "utf8", TC_Common::strto<int>(param.dbPort));

        tcMysql.connect();
        MYSQL *pMysql = tcMysql.getMysql();

        //检查DB是否已经存在
        iRet = checkDb(tcMysql, param.dbName);
        if (iRet == 0)
        {
            // 不存在，则新建
            string sSql = "create database " + param.dbName;
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create database:") + param.dbName + "@" + param.dbIp + " error:" + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        string sSql2 = "use " + param.dbName;
        mysql_real_query(pMysql, sSql2.c_str(), sSql2.length());

        // 检查routerDB中各个表是否存在，不存在则新建
        iRet = checkTable(tcMysql, param.dbName, "t_router_module");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_module` (") +
                                "`id` int(11) NOT NULL auto_increment," +
                                "`module_name` varchar(255) NOT NULL default ''," +
                                "`version` int(11) NOT NULL default '0'," +
                                "`remark` varchar(255) default NULL," +
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP," +
                                "`POSTTIME` datetime default NULL," +
                                "`LASTUSER` varchar(60) default NULL," +
                                "`switch_status` int(11) default '0'," +
                                "PRIMARY KEY  (`id`)," +
                                "UNIQUE KEY `module_name` (`module_name`)" +
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table:") + param.dbName + ".t_router_module error:" + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        iRet = checkTable(tcMysql, param.dbName, "t_router_server");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_server` (")+
                                "`id` int(11) NOT NULL auto_increment,"+
                                "`server_name` varchar(255) NOT NULL default '',"+
                                "`ip` varchar(15) NOT NULL default '',"+
                                "`binlog_port` int(11) NOT NULL default '0',"+
                                "`cache_port` int(11) NOT NULL default '0',"+
                                "`routerclient_port` int(11) NOT NULL default '0',"+
                                "`backup_port` int(11) NOT NULL default '0',"+
                                "`wcache_port` int(11) NOT NULL default '0',"+
                                "`controlack_port` int(11) NOT NULL default '0',"+
                                "`idc_area` varchar(10) NOT NULL default 'sz',"+
                                "`status` int(11) NOT NULL default '0',"+
                                "`remark` varchar(255) default NULL,"+
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,"+
                                "`POSTTIME` datetime default NULL,"+
                                "`LASTUSER` varchar(60) default NULL,"+
                                "PRIMARY KEY  (`id`),"+
                                "UNIQUE KEY `server_name` (`server_name`)"+
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";

            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table: ") + param.dbName + ".t_router_server error: " + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        iRet = checkTable(tcMysql, param.dbName, "t_router_group");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_group` (")+
                                "`id` int(11) NOT NULL auto_increment,"+
                                "`module_name` varchar(255) NOT NULL default '',"+
                                "`group_name` varchar(255) NOT NULL default '',"+
                                "`server_name` varchar(255) NOT NULL default '',"+
                                "`server_status` char(1) NOT NULL default '',"+
                                "`priority` tinyint(4) NOT NULL default '1',"+
                                "`source_server_name` varchar(255) default NULL,"+
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,"+
                                "`POSTTIME` datetime default NULL,"+
                                "`LASTUSER` varchar(60) default NULL,"+
                                "`access_status` int(11) default '0'," +
                                "PRIMARY KEY  (`id`),"+
                                "UNIQUE KEY `server_name` (`server_name`)"+
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table: ") + param.dbName + ".t_router_group error: " + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        iRet = checkTable(tcMysql, param.dbName, "t_router_record");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_record` (")+
                                "`id` int(11) NOT NULL auto_increment,"+
                                "`module_name` varchar(255) NOT NULL default '',"+
                                "`from_page_no` int(11) NOT NULL default '0',"+
                                "`to_page_no` int(11) NOT NULL default '0',"+
                                "`group_name` varchar(255) NOT NULL default '',"+
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,"+
                                "`POSTTIME` datetime default NULL,"+
                                "`LASTUSER` varchar(60) default NULL,"+
                                "PRIMARY KEY  (`id`)"+
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table: ") + param.dbName + ".t_router_record error: " + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }

        iRet = checkTable(tcMysql, param.dbName, "t_router_transfer");
        if (iRet == 0)
        {
            string sSql = string("CREATE TABLE `t_router_transfer` (")+
                                "`id` int(11) NOT NULL auto_increment,"+
                                "`module_name` varchar(255) NOT NULL default '',"+
                                "`from_page_no` int(11) NOT NULL default '0',"+
                                "`to_page_no` int(11) NOT NULL default '0',"+
                                "`group_name` varchar(255) NOT NULL default '',"+
                                "`trans_group_name` varchar(255) NOT NULL default '',"+
                                "`transfered_page_no` int(11) default NULL,"+
                                "`remark` varchar(255) default NULL,"+
                                "`state` tinyint(4) NOT NULL default '0',"+
                                "`modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,"+
                                "`startTime` datetime NOT NULL default '1970-01-01 00:00:01',"+
                                "`endTime` datetime NOT NULL default '1970-01-01 00:00:01',"+
                                "`POSTTIME` datetime default NULL,"+
                                "`LASTUSER` varchar(60) default NULL,"+
                                "PRIMARY KEY  (`id`)"+
                                ") ENGINE=MyISAM DEFAULT CHARSET=utf8";
            iRet = mysql_real_query(pMysql, sSql.c_str(), sSql.length());
            if (iRet != 0)
            {
                errmsg = string("create table: ") + param.dbName + ".t_router_transfer error: " + mysql_error(pMysql);
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
        }
    }
    catch (exception &ex)
    {
        tcMysql.disconnect();
        errmsg = string("create router db and table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::createRouterConf(const RouterParam &param, bool bRouterServerExist, bool bReplace, string & errmsg)
{
    if (!bRouterServerExist)
    {
        string sRouterConf = string("<Main>\n")+
                                    "   # 应用名称\n" +
                                    "   AppName=${AppName}\n" +
                                    "   # 数据库配置重新装载最小间隔时间\n" +
                                    "   DbReloadTime=300000\n" +
                                    "   # 管理接口的Obj\n"+
                                    "   AdminRegObj=tars.tarsregistry.AdminRegObj\n" +

                                    "   <ETCD>\n" +
                                    "       # 是否开启ETCD为Router集群(Router集群需要利用ETCD来做选举)\n" +
                                    "       enable=N\n" +
                                    "       # 所有ETCD机器的IP地址，以分号分割\n" +
                                    "       host=x.x.x.x:port;x.x.x.x:port\n" +
                                    "       # ETCD通信请求的超时时间(秒)\n" +
                                    "       RequestTimeout=3\n" +
                                    "       # Router主机心跳的维持时间(秒)\n" +
                                    "       EtcdHbTTL=60\n" +
                                    "   </ETCD>\n" +

                                    "   <Transfer>\n" +
                                    "       # 清理代理的最近未访问时间\n" +
                                    "       ProxyMaxSilentTime=1800\n" +
                                    "       # 清理代理的间隔时间\n" +
                                    "       ClearProxyInterval=1800\n" +
                                    "       # 轮询迁移数据库的时间\n" +
                                    "       TransferInterval=3\n" +
                                    "       # 轮询线程数\n" +
                                    "       TimerThreadSize=20\n" +
                                    "       # 等待页迁移的超时时间(毫秒)\n" +
                                    "       TransferTimeOut=3000\n" +
                                    "       # 迁移时隔多少页整理一下数据库记录\n" +
                                    "       TransferDefragDbInterval=50\n" +
                                    "       # 重新执行迁移指令的时间间隔(秒)\n" +
                                    "       RetryTransferInterval=1\n" +
                                    "       # 迁移失败时的最大重试次数\n" +
                                    "       RetryTransferMaxTimes=3\n" +
                                    "       # 一次迁移页数\n" +
                                    "       TransferPagesOnce=10\n" +
                                    "       # 每个组分配的最小迁移线程数\n" +
                                    "       MinTransferThreadEachGroup=5\n" +
                                    "       # 每个组分配的最大迁移线程数\n" +
                                    "       MaxTransferThreadEachGroup=8\n" +
                                    "   </Transfer>\n" +

                                    "   <Switch>\n" +
                                    "       # 是否开启cache主备自动切换\n" +
                                    "       enable=N\n" +
                                    "       # 自动切换超时的检测间隔(秒)\n" +
                                    "       SwitchCheckInterval= 10\n" +
                                    "       # 自动切换的超时时间(秒)\n" +
                                    "       SwitchTimeOut=60\n" +
                                    "       # 自动切换执行的线程数(默认1个)\n" +
                                    "       SwitchThreadSize=50\n" +
                                    "       # 备机不可读的超时时间(秒)\n" +
                                    "       SlaveTimeOut=60\n" +
                                    "       # 主备切换时，主备机binlog差异的阈值(毫秒)\n" +
                                    "       SwitchBinLogDiffLimit=300\n" +
                                    "       # 一天当中主备切换的最大次数\n" +
                                    "       SwitchMaxTimes=3\n" +
                                    "       # 主备切换时等待主机降级的时间(秒)\n" +
                                    "       DowngradeTimeout=30\n" +
                                    "   </Switch>\n" +

                                    "   <DB>\n" +
                                    "       <conn>\n" +
                                    "           charset=utf8\n" +
                                    "           dbname=${DbName}\n" +
                                    "           dbhost=${DbIp}\n" +
                                    "           dbpass=${DbPwd}\n" +
                                    "           dbport=${DbPort}\n" +
                                    "           dbuser=${DbUser}\n" +
                                    "       </conn>\n" +

                                    "       <relation>\n" +
                                    "           charset=${RelationDBCharset}\n" +
                                    "           dbname=${RelationDBName}\n" +
                                    "           dbhost=${RelationDBIp}\n" +
                                    "           dbpass=${RelationDBPass}\n" +
                                    "           dbport=${RelationDBPort}\n" +
                                    "           dbuser=${RelationDBUser}\n" +
                                    "       </relation>\n" +
                                    "   </DB>\n" +
                                    "</Main>\n";

        sRouterConf = TC_Common::replace(sRouterConf, "${AppName}", param.appName);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbName}", param.dbName);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbIp}", param.dbIp);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbPort}", param.dbPort);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbUser}", param.dbUser);
        sRouterConf = TC_Common::replace(sRouterConf, "${DbPwd}", param.dbPwd);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBCharset}", _relationDBInfo["charset"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBName}", _relationDBInfo["dbname"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBIp}", _relationDBInfo["dbhost"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBPass}", _relationDBInfo["dbpass"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBPort}", _relationDBInfo["dbport"]);
        sRouterConf = TC_Common::replace(sRouterConf, "${RelationDBUser}", _relationDBInfo["dbuser"]);

        int iConfigId;
        if (insertTarsConfigFilesTable(param.serverName, param.serverName.substr(7) + ".conf", "", sRouterConf, 2, iConfigId, bReplace, errmsg) != 0)
            return -1;

        if (insertTarsHistoryConfigFilesTable(iConfigId, sRouterConf, bReplace, errmsg) != 0)
            return -1;
    }

    int iHostConfigId;
    for (size_t i = 0; i < param.serverIp.size(); ++i)
    {
        if (insertTarsConfigFilesTable(param.serverName, param.serverName.substr(7) + ".conf", param.serverIp[i], "", 3, iHostConfigId, bReplace, errmsg) != 0)
            return -1;

        if (insertTarsHistoryConfigFilesTable(iHostConfigId, "", bReplace, errmsg) != 0)
            return -1;
    }

    return 0;
}

int DCacheOptImp::createProxyConf(const string &sModuleName, const ProxyParam &stProxyParam, const RouterParam &stRouterParam, bool bReplace, string & errmsg)
{
    string sProxyConf = string("<Main>\n")+
                        "   #同步路由表的时间间隔(秒)\n" +
                        "   SynRouterTableInterval = 1\n" +
                        "   #同步模块变化(模块新增或者下线)的时间间隔(秒)\n" +
                        "   SynRouterTableFactoryInterval = 5\n" +
                        "   #保存在本地的路由表文件名前缀\n" +
                        "   BaseLocalRouterFile = Router.dat\n" +
                        "   #模块路由表每秒钟的最大更新频率\n" +
                        "   RouterTableMaxUpdateFrequency = 3\n" +
                        "   RouterObj = ${RouterServant}\n" +
                        "</Main>\n";

    sProxyConf = TC_Common::replace(sProxyConf, "${RouterServant}", stRouterParam.serverName + ".RouterObj");

    int iConfigId;
    if (insertTarsConfigFilesTable(stProxyParam.serverName, "ProxyServer.conf", "", sProxyConf, 2, iConfigId, bReplace, errmsg) != 0)
        return -1;

    if (insertTarsHistoryConfigFilesTable(iConfigId, sProxyConf, bReplace, errmsg) != 0)
        return -1;

    string sHostProxyConf = string("<Main>\n") +
                            "   #sz-深圳; bj-北京; sh-上海; tj-天津\n" +
                            "   IdcArea = ${IdcArea}\n" +
                            "</Main>\n";
    for (size_t i = 0; i < stProxyParam.serverIp.size(); i++)
    {
        string stmpHostProxyConf = TC_Common::replace(sHostProxyConf, "${IdcArea}", stProxyParam.serverIp[i].idcArea);
        int iHostConfigId;
        if (insertTarsConfigFilesTable(stProxyParam.serverName, "ProxyServer.conf", stProxyParam.serverIp[i].ip, stmpHostProxyConf, 3, iHostConfigId, bReplace, errmsg) != 0)
            return -1;

        if (insertTarsHistoryConfigFilesTable(iHostConfigId, stmpHostProxyConf, bReplace, errmsg) != 0)
            return -1;
    }

    return 0;
}

int DCacheOptImp::matchItemId(const string &item, const string &path, string &item_id)
{
    string sql = "select id from t_config_item where item = '" + item + "' and path = '" + path + "'";
    TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sql);
    if (data.size() != 1)
    {
        string sErr = string("select from table t_config_item result error, the sql is:") + sql;
        TLOGDEBUG(FUN_LOG << sErr << endl);
        throw DCacheOptException(sErr);
    }
    item_id = data[0]["id"];

    return 0;
}

int DCacheOptImp::insertAppModTable(const string &appName, const string &moduleName, int &id, bool bReplace, string & errmsg)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "app name:" << appName << "|module name:" << moduleName << endl);
        map<string, pair<TC_Mysql::FT, string> > m;

        m["appName"]    = make_pair(TC_Mysql::DB_STR, appName);
        m["moduleName"] = make_pair(TC_Mysql::DB_STR, moduleName);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_config_appMod", m);
        else
            _mysqlRelationDB.insertRecord("t_config_appMod", m);

        string sSql = "select id from t_config_appMod where appName='" + appName + "' and moduleName='" + moduleName + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() != 1)
        {
            errmsg = string("select id from t_config_appMod result count not equal to 1, app name:") + appName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
        }

        id = TC_Common::strto<int>(data[0]["id"]);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("operate t_config_appMod error, app name:") + appName + "|module name:" + moduleName + "|catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertAppModTable(const string &appName, const string &moduleName, DCache::DCacheType eCacheType, int &id, bool bReplace, string & errmsg)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "app name:" << appName << "|module name:" << moduleName << "|cache type:" << etos(eCacheType) << endl);
        map<string, pair<TC_Mysql::FT, string> > m;

        m["appName"]    = make_pair(TC_Mysql::DB_STR, appName);
        m["moduleName"] = make_pair(TC_Mysql::DB_STR, moduleName);
        m["cacheType"]  = make_pair(TC_Mysql::DB_STR, etos(eCacheType));

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_config_appMod", m);
        else
            _mysqlRelationDB.insertRecord("t_config_appMod", m);

        string sSql = "select id from t_config_appMod where appName='" + appName + "' and moduleName='" + moduleName + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() != 1)
        {
            errmsg = string("select id from t_config_appMod result count not equal to 1, app name:") + appName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
        }

        id = TC_Common::strto<int>(data[0]["id"]);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("operate t_config_appMod error, app name:") + appName + "|module name:" + moduleName + "|catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertConfigFilesTable(const string &sFullServerName, const string &sHost, const int config_id,  vector< map<string,string> > &myVec, const int level, bool bReplace, string & errmsg)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "server name:" << sFullServerName << "|host:" << sHost << endl);

        map<string, pair<TC_Mysql::FT, string> > m;
        for (size_t i = 0; i < myVec.size(); i++)
        {
            m["server_name"]    = make_pair(TC_Mysql::DB_STR, sFullServerName);
            m["host"]           = make_pair(TC_Mysql::DB_STR, sHost);
            m["posttime"]       = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            m["lastuser"]       = make_pair(TC_Mysql::DB_STR, "system");
            m["level"]          = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(level));
            m["config_flag"]    = make_pair(TC_Mysql::DB_INT, "0");
            m["config_id"]      = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(config_id));
            m["item_id"]        = make_pair(TC_Mysql::DB_STR, myVec[i]["item_id"]);
            m["config_value"]   = make_pair(TC_Mysql::DB_STR, myVec[i]["config_value"]);

            if (bReplace)
                _mysqlRelationDB.replaceRecord("t_config_table", m);
            else
                _mysqlRelationDB.insertRecord("t_config_table", m);
        }
    }
    catch (const std::exception &ex)
    {
        errmsg = string("config table t_config_table servevr name:") + sFullServerName + "|catch exception:" + ex.what();
        TLOGDEBUG(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

int DCacheOptImp::insertReferenceTable(const int appConfigID, const string& server_name, const string& host, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["reference_id"]   = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(appConfigID));
        m["server_name"]    = make_pair(TC_Mysql::DB_STR, server_name);
        m["host"]           = make_pair(TC_Mysql::DB_STR, host);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_config_reference", m);
        else
            _mysqlRelationDB.insertRecord("t_config_reference", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("operate table t_config_reference catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

int DCacheOptImp::createKVCacheConf(const string &appName, const string &moduleName, const string &routerName, const vector<DCache::CacheHostParam> & cacheHost, const DCache::SingleKeyConfParam & kvConf, bool bReplace, string & errmsg)
{
    vector < map <string, string> > vtConfig;

    insertConfigItem2Vector("ModuleName", "Main", moduleName, vtConfig);
    insertConfigItem2Vector("CoreSizeLimit", "Main", "-1", vtConfig);
    insertConfigItem2Vector("BackupDayLog", "Main", "dumpAndRecover", vtConfig);
    insertConfigItem2Vector("RouterHeartbeatInterval", "Main", "1000", vtConfig);

    // <Cache Config>
    insertConfigItem2Vector("AvgDataSize", "Main/Cache", kvConf.avgDataSize, vtConfig);
    insertConfigItem2Vector("HashRadio", "Main/Cache", "2", vtConfig);

    insertConfigItem2Vector("EnableErase", "Main/Cache", kvConf.enableErase, vtConfig);
    insertConfigItem2Vector("EraseInterval", "Main/Cache", "5", vtConfig);
    insertConfigItem2Vector("EraseRadio", "Main/Cache", kvConf.eraseRadio, vtConfig);
    insertConfigItem2Vector("EraseThreadCount", "Main/Cache", "2", vtConfig);
    insertConfigItem2Vector("MaxEraseCountOneTime", "Main/Cache", "500", vtConfig);

    insertConfigItem2Vector("StartExpireThread", "Main/Cache", kvConf.startExpireThread, vtConfig);
    insertConfigItem2Vector("ExpireDb", "Main/Cache", kvConf.expireDb, vtConfig);
    insertConfigItem2Vector("ExpireInterval", "Main/Cache", "3600", vtConfig);
    insertConfigItem2Vector("ExpireSpeed", "Main/Cache", "0", vtConfig);

    insertConfigItem2Vector("SyncInterval", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("SyncSpeed", "Main/Cache", "0", vtConfig);
    insertConfigItem2Vector("SyncThreadNum", "Main/Cache", "1", vtConfig);
    insertConfigItem2Vector("SyncTime", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("SyncBlockTime", "Main/Cache", "0000-0000", vtConfig);

    insertConfigItem2Vector("SaveOnlyKey", "Main/Cache", kvConf.saveOnlyKey, vtConfig);
    insertConfigItem2Vector("DowngradeTimeout", "Main/Cache", "30", vtConfig);

    insertConfigItem2Vector("JmemNum", "Main/Cache", "10", vtConfig);
    insertConfigItem2Vector("coldDataCalEnable", "Main/Cache", "Y", vtConfig);
    insertConfigItem2Vector("coldDataCalCycle", "Main/Cache", "7", vtConfig);
    insertConfigItem2Vector("MaxKeyLengthInDB", "Main/Cache", "767", vtConfig);

    // <Log Config>
    insertConfigItem2Vector("DbDayLog", "Main/Log", "db", vtConfig);

    // <DbAccess Config>
    insertConfigItem2Vector("DBFlag", "Main/DbAccess", kvConf.dbFlag, vtConfig);
    insertConfigItem2Vector("ObjName", "Main/DbAccess", kvConf.dbAccessServant, vtConfig);
    if ((kvConf.enableErase == "Y") && (kvConf.dbFlag == "Y"))
    {
        insertConfigItem2Vector("ReadDbFlag", "Main/DbAccess", "Y", vtConfig);
    }
    else
    {
        insertConfigItem2Vector("ReadDbFlag", "Main/DbAccess", kvConf.readDbFlag, vtConfig);
    }

    // <Binlog Config>
    insertConfigItem2Vector("LogFile", "Main/BinLog", "binlog", vtConfig);
    insertConfigItem2Vector("Record", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("KeyRecord", "Main/BinLog", "N", vtConfig);
    insertConfigItem2Vector("MaxLine", "Main/BinLog", "10000", vtConfig);
    insertConfigItem2Vector("SyncCompress", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("IsGzip", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("KeySyncMode", "Main/BinLog", "N", vtConfig);
    insertConfigItem2Vector("BuffSize", "Main/BinLog", "10", vtConfig);
    insertConfigItem2Vector("SaveSyncTimeInterval", "Main/BinLog", "10", vtConfig);
    insertConfigItem2Vector("HeartbeatInterval", "Main/BinLog", "600", vtConfig);

    // <Router Config>
    insertConfigItem2Vector("ObjName", "Main/Router", routerName + ".RouterObj", vtConfig);
    insertConfigItem2Vector("PageSize", "Main/Router", "10000", vtConfig);
    insertConfigItem2Vector("RouteFile", "Main/Router", "Route.dat", vtConfig);
    insertConfigItem2Vector("SyncInterval", "Main/Router", "1", vtConfig);

    // shm size
    string shmSize;
    for (size_t i = 0; i < cacheHost.size(); i++)
    {
        if (cacheHost[i].type == "M")
        {
            shmSize = cacheHost[i].shmSize;
            break;
        }
    }
    insertConfigItem2Vector("ShmSize", "Main/Cache", shmSize + "G", vtConfig);

    // 保存appname和modulename间的关系
    int iAppConfigId;
    //if (insertAppModTable(appName, moduleName, iAppConfigId, bReplace, errmsg) != 0)
    //    return -1;

    if (insertAppModTable(appName, moduleName, DCache::KVCACHE, iAppConfigId, bReplace, errmsg) != 0)
        return -1;

    // 模块公共配置
    if (insertConfigFilesTable(appName, "", iAppConfigId, vtConfig, 1, bReplace, errmsg) != 0)
        return -1;

    // 节点服务配置
    for (size_t i = 0; i < cacheHost.size(); i++)
    {
        vector < map <string, string> > level2Vec;

        string hostShmSize = TC_Common::tostr(cacheHost[i].shmSize);
        insertConfigItem2Vector("ShmSize", "Main/Cache", hostShmSize + "G", level2Vec);

        size_t shmKey = TC_Common::strto<size_t>(cacheHost[i].shmKey);
        if(shmKey == 0)
        {
            if(getShmKey(shmKey) != 0)
                return -1;
        }

        insertConfigItem2Vector("ShmKey", "Main/Cache", TC_Common::tostr(shmKey), level2Vec);

        // if (!cacheHost[i].shmKey.empty())
        // {
        //     insertConfigItem2Vector("ShmKey", "Main/Cache", TC_Common::tostr(cacheHost[i].shmKey), level2Vec);
        // }

        if (insertConfigFilesTable(cacheHost[i].serverName, cacheHost[i].serverIp, 0, level2Vec, 2, bReplace, errmsg) != 0)
            return -1;

        // 保存节点配置对模块公共配置的引用关系
        if (insertReferenceTable(iAppConfigId, cacheHost[i].serverName, cacheHost[i].serverIp, bReplace, errmsg) != 0)
            return -1;
    }

    return 0;
}

int DCacheOptImp::createMKVCacheConf(const string &appName, const string &moduleName, const string &routerName,const vector<DCache::CacheHostParam> & vtCacheHost,const DCache::MultiKeyConfParam & mkvConf, const vector<RecordParam> &vtRecordParam, bool bReplace, string & errmsg)
{
    TLOGDEBUG(FUN_LOG << "router name:" << routerName  << "|cache type:" << mkvConf.cacheType << endl);

    vector< map<string, string> > vtConfig;

    insertConfigItem2Vector("ModuleName", "Main", moduleName, vtConfig);
    insertConfigItem2Vector("CoreSizeLimit", "Main", "-1", vtConfig);
    insertConfigItem2Vector("MKeyMaxBlockCount", "Main", "20000", vtConfig);
    insertConfigItem2Vector("BackupDayLog", "Main", "dumpAndRecover", vtConfig);
    insertConfigItem2Vector("RouterHeartbeatInterval", "Main", "1000", vtConfig);

    // <Cache Config>
    insertConfigItem2Vector("AvgDataSize", "Main/Cache", mkvConf.avgDataSize, vtConfig);
    insertConfigItem2Vector("HashRadio", "Main/Cache", "2", vtConfig);
    insertConfigItem2Vector("MKHashRadio", "Main/Cache", "1", vtConfig);
    insertConfigItem2Vector("MainKeyType", "Main/Cache", mkvConf.cacheType, vtConfig);

    insertConfigItem2Vector("StartDeleteThread", "Main/Cache", "Y", vtConfig);
    insertConfigItem2Vector("DeleteInterval", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("DeleteSpeed", "Main/Cache", "0", vtConfig);

    insertConfigItem2Vector("EnableErase", "Main/Cache", mkvConf.enableErase, vtConfig);
    insertConfigItem2Vector("EraseInterval", "Main/Cache", "5", vtConfig);
    insertConfigItem2Vector("EraseRadio", "Main/Cache", mkvConf.eraseRadio, vtConfig);
    insertConfigItem2Vector("EraseThreadCount", "Main/Cache", "2", vtConfig);
    insertConfigItem2Vector("MaxEraseCountOneTime", "Main/Cache", "500", vtConfig);

    insertConfigItem2Vector("StartExpireThread", "Main/Cache", mkvConf.startExpireThread, vtConfig);
    insertConfigItem2Vector("ExpireDb", "Main/Cache", mkvConf.expireDb, vtConfig);
    insertConfigItem2Vector("ExpireInterval", "Main/Cache", "3600", vtConfig);
    insertConfigItem2Vector("ExpireSpeed", "Main/Cache", "0", vtConfig);

    insertConfigItem2Vector("SyncInterval", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("SyncSpeed", "Main/Cache", "0", vtConfig);
    insertConfigItem2Vector("SyncThreadNum", "Main/Cache", "1", vtConfig);
    insertConfigItem2Vector("SyncTime", "Main/Cache", "300", vtConfig);
    insertConfigItem2Vector("SyncBlockTime", "Main/Cache", "0000-0000", vtConfig);
    insertConfigItem2Vector("SyncUNBlockPercent", "Main/Cache", "60", vtConfig);

    insertConfigItem2Vector("InsertOrder", "Main/Cache", "N", vtConfig);
    insertConfigItem2Vector("UpdateOrder", "Main/Cache", "N", vtConfig);
    insertConfigItem2Vector("MkeyMaxDataCount", "Main/Cache", "0", vtConfig);
    insertConfigItem2Vector("SaveOnlyKey", "Main/Cache", mkvConf.saveOnlyKey, vtConfig);
    insertConfigItem2Vector("OrderItem", "Main/Cache", "", vtConfig);
    insertConfigItem2Vector("OrderDesc", "Main/Cache", "Y", vtConfig);

    insertConfigItem2Vector("JmemNum", "Main/Cache", "2", vtConfig);
    insertConfigItem2Vector("DowngradeTimeout", "Main/Cache", "20", vtConfig);
    insertConfigItem2Vector("coldDataCalEnable", "Main/Cache", "Y", vtConfig);
    insertConfigItem2Vector("coldDataCalCycle", "Main/Cache", "7", vtConfig);
    insertConfigItem2Vector("MaxKeyLengthInDB", "Main/Cache", "767", vtConfig);

    insertConfigItem2Vector("transferCompress", "Main/Cache", "Y", vtConfig);

    // <Log Config>
    insertConfigItem2Vector("DbDayLog", "Main/Log", "db", vtConfig);

    // <DbAccess Config>
    insertConfigItem2Vector("DBFlag", "Main/DbAccess", mkvConf.dbFlag, vtConfig);
    insertConfigItem2Vector("ObjName", "Main/DbAccess", mkvConf.dbAccessServant, vtConfig);
    if ((mkvConf.enableErase == "Y") && (mkvConf.dbFlag == "Y"))
    {
        insertConfigItem2Vector("ReadDbFlag", "Main/DbAccess", "Y", vtConfig);
    }
    else
    {
        insertConfigItem2Vector("ReadDbFlag", "Main/DbAccess", mkvConf.readDbFlag, vtConfig);
    }

    // <Binlog Config>
    insertConfigItem2Vector("LogFile", "Main/BinLog", "binlog", vtConfig);
    insertConfigItem2Vector("Record", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("KeyRecord", "Main/BinLog", "N", vtConfig);
    insertConfigItem2Vector("KeySyncMode", "Main/BinLog", "N", vtConfig);
    insertConfigItem2Vector("MaxLine", "Main/BinLog", "10000", vtConfig);
    insertConfigItem2Vector("SyncCompress", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("IsGzip", "Main/BinLog", "Y", vtConfig);
    insertConfigItem2Vector("BuffSize", "Main/BinLog", "10", vtConfig);
    insertConfigItem2Vector("SaveSyncTimeInterval", "Main/BinLog", "10", vtConfig);
    insertConfigItem2Vector("HeartbeatInterval", "Main/BinLog", "600", vtConfig);

    // <Router Config>
    insertConfigItem2Vector("ObjName", "Main/Router", routerName + ".RouterObj", vtConfig);
    insertConfigItem2Vector("PageSize", "Main/Router", "10000", vtConfig);
    insertConfigItem2Vector("RouteFile", "Main/Router", "Route.dat", vtConfig);
    insertConfigItem2Vector("SyncInterval", "Main/Router", "1", vtConfig);

    // <Record Config>
    string sMKey, sUKey, sValue, sRecords;
    int iMKeyNum = 0;
    int iTag = 0;
    for (size_t i = 0; i < vtRecordParam.size(); i++)
    {
        if (vtRecordParam[i].keyType == "mkey")
        {
            iMKeyNum++;
            if (iMKeyNum > 1)
            {
                errmsg = string("cache config format error, mkey number:") + TC_Common::tostr(iMKeyNum) + "no equal to 1|module name:" + moduleName;
                TLOGERROR(FUN_LOG << errmsg << endl);
                throw DCacheOptException(errmsg);
            }
            sRecords += "\t\t\t" + vtRecordParam[i].fieldName + " = " + TC_Common::tostr(iTag) + "|" + vtRecordParam[i].dataType + "|" + vtRecordParam[i].property + "|" + vtRecordParam[i].defaultValue + "|" + TC_Common::tostr(vtRecordParam[i].maxLen) + "\n";
            iTag++;
            sMKey = vtRecordParam[i].fieldName;
        }
    }

    for (size_t i=0; i<vtRecordParam.size(); i++)
    {
        if (vtRecordParam[i].keyType == "ukey")
        {
            sRecords += "\t\t\t" + vtRecordParam[i].fieldName + " = " + TC_Common::tostr(iTag) + "|" + vtRecordParam[i].dataType + "|" + vtRecordParam[i].property + "|" + vtRecordParam[i].defaultValue + "|" + TC_Common::tostr(vtRecordParam[i].maxLen) + "\n";
            iTag++;

            if (sUKey.length() == 0)
                sUKey = vtRecordParam[i].fieldName;
            else
                sUKey = sUKey + "|" + vtRecordParam[i].fieldName;
        }
    }

    if ((mkvConf.cacheType == "hash") && (sUKey.length() <= 0))
    {
        errmsg = string("module:") + moduleName + "'s cache config format error, not exist ukey";
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    for (size_t i = 0; i < vtRecordParam.size(); i++)
    {
        if (vtRecordParam[i].keyType == "value")
        {
            sRecords += "\t\t\t" + vtRecordParam[i].fieldName + " = " + TC_Common::tostr(iTag) + "|" + vtRecordParam[i].dataType + "|" + vtRecordParam[i].property + "|" + vtRecordParam[i].defaultValue + "|" + TC_Common::tostr(vtRecordParam[i].maxLen) + "\n";
            iTag++;
            if (sValue.length() == 0)
                sValue = vtRecordParam[i].fieldName;
            else
                sValue = sValue + "|" + vtRecordParam[i].fieldName;
        }
    }

    insertConfigItem2Vector("Virtualrecord", "Main/Record/Field", sRecords, vtConfig);
    insertConfigItem2Vector("MKey", "Main/Record", sMKey, vtConfig);
    insertConfigItem2Vector("UKey", "Main/Record", sUKey, vtConfig);
    insertConfigItem2Vector("VKey", "Main/Record", sValue, vtConfig);

    // 保存 应用和模块之间的对应关系
    int iAppConfigId;
    //if (insertAppModTable(appName, moduleName, iAppConfigId, bReplace, errmsg) != 0)
    //    return -1;

    if (insertAppModTable(appName, moduleName, DCache::MKVCACHE, iAppConfigId, bReplace, errmsg) != 0)
        return -1;

    if (insertConfigFilesTable(appName, "", iAppConfigId, vtConfig, 1, bReplace, errmsg) != 0)
        return -1;

    for (size_t i = 0; i < vtCacheHost.size(); ++i)
    {

        vector < map <string, string> > level2Vec;
        map<string , string> level3Map;
        string level2_item;
        string level2_path;
        string level2_item_id;

        string hostShmSize = TC_Common::tostr(vtCacheHost[i].shmSize);
        level3Map["config_value"] = hostShmSize + "G";

        level2_item = "ShmSize";
        level2_path = "Main/Cache";
        matchItemId(level2_item, level2_path, level2_item_id);
        level3Map["item_id"] = level2_item_id;

        level2Vec.push_back(level3Map);

        size_t shmKey = TC_Common::strto<size_t>(vtCacheHost[i].shmKey);
        if(shmKey == 0)
        {
            if(getShmKey(shmKey) != 0)
                return -1;
        }

        insertConfigItem2Vector("ShmKey", "Main/Cache", TC_Common::tostr(shmKey), level2Vec);

        // if (!vtCacheHost[i].shmKey.empty())
        // {
        //     insertConfigItem2Vector("ShmKey", "Main/Cache", TC_Common::tostr(vtCacheHost[i].shmKey), level2Vec);
        // }

        if (insertConfigFilesTable(vtCacheHost[i].serverName, vtCacheHost[i].serverIp, 0, level2Vec, 2, bReplace, errmsg) != 0)
            return -1;

        if (insertReferenceTable(iAppConfigId, vtCacheHost[i].serverName, vtCacheHost[i].serverIp, bReplace, errmsg) != 0)
            return -1;
    }

    return 0;
}

void DCacheOptImp::insertConfigItem2Vector(const string& sItem, const string &sPath, const string &sConfigValue, vector < map <string, string> > &vtConfig)
{
    map<string , string> mMap;
    string sItemId("");
    matchItemId(sItem, sPath, sItemId);

    mMap["item_id"]      = sItemId;
    mMap["config_value"] = sConfigValue;

    vtConfig.push_back(mMap);
}

int DCacheOptImp::insertCache2RouterDb(const string& sModuleName, const string &sRemark, const TC_DBConf &routerDbInfo, const vector<DCache::CacheHostParam> & vtCacheHost, bool bReplace, string& errmsg)
{
    TLOGDEBUG(FUN_LOG << "insert module name and cache server info to router db table|module name:" << sModuleName << endl);

    TC_Mysql tcMysql;
    try
    {
        tcMysql.init(routerDbInfo);

        if (tcMysql.getRecordCount("t_router_module", "where module_name='" + sModuleName + "'") == 0)
        {
            map<string, pair<TC_Mysql::FT, string> > mpModule;
            mpModule["module_name"] = make_pair(TC_Mysql::DB_STR, sModuleName);
            mpModule["version"]     = make_pair(TC_Mysql::DB_INT, "1");
            mpModule["remark"]      = make_pair(TC_Mysql::DB_STR, sRemark);
            mpModule["POSTTIME"]    = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpModule["LASTUSER"]    = make_pair(TC_Mysql::DB_STR, "sys");

            if (bReplace)
                tcMysql.replaceRecord("t_router_module", mpModule);
            else
                tcMysql.insertRecord("t_router_module", mpModule);
        }
        else
        {
            // 已经存在，则更新版本号
            string updateVersion = string("update t_router_module set version = version+1 where module_name='") + sModuleName + "';";
            TLOGDEBUG(FUN_LOG << "module name:" << sModuleName << " already exist, update router version, do SQL:" << updateVersion << endl);
            tcMysql.execute(updateVersion);
        }

        uint16_t iPort = getPort(vtCacheHost[0].serverIp);
        if (0 == iPort)
        {
            errmsg = "failed to get port for cache server ip:" + vtCacheHost[0].serverIp;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        set<string> setAddr;
        for (size_t i = 0; i < vtCacheHost.size(); i++)
        {
            map<string, pair<TC_Mysql::FT, string> > mpServer;
            if (vtCacheHost[i].isContainer == "true")
            {
                mpServer["server_name"]       = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
                mpServer["ip"]                = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverIp);
                mpServer["binlog_port"]       = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].binlogPort);
                mpServer["cache_port"]        = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].cachePort);
                mpServer["routerclient_port"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].routerPort);
                mpServer["backup_port"]       = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].backupPort);
                mpServer["wcache_port"]       = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].wcachePort);
                mpServer["controlack_port"]   = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].controlackPort);
            }
            else
            {
                mpServer["server_name"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
                mpServer["ip"]          = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverIp);

                uint16_t iBinLogPort = iPort;
                while(true)
                {
                    iBinLogPort = getPort(vtCacheHost[i].serverIp, iBinLogPort);
                    if (iBinLogPort == 0)
                    {
                        errmsg = "failed to get port for binlog obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBinLogPort)) != setAddr.end())
                    {
                        iBinLogPort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBinLogPort));
                mpServer["binlog_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iBinLogPort));

                uint16_t iCachePort = iPort + 1;
                while(true)
                {
                    iCachePort = getPort(vtCacheHost[i].serverIp, iCachePort);
                    if (iCachePort == 0)
                    {
                        errmsg = "failed to get port for cache obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iCachePort)) != setAddr.end())
                    {
                        iCachePort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iCachePort));
                mpServer["cache_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iCachePort));

                uint16_t iRouterPort = iPort + 2;
                while(true)
                {
                    iRouterPort = getPort(vtCacheHost[i].serverIp, iRouterPort);
                    if (iRouterPort == 0)
                    {
                        errmsg = "failed to get port for router client obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iRouterPort)) != setAddr.end())
                    {
                        iRouterPort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iRouterPort));
                mpServer["routerclient_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iRouterPort));

                uint16_t iBackUpPort = iPort + 3;
                while(true)
                {
                    iBackUpPort = getPort(vtCacheHost[i].serverIp, iBackUpPort);
                    if (iBackUpPort == 0)
                    {
                        errmsg = "failed to get port for backup obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBackUpPort)) != setAddr.end())
                    {
                        iBackUpPort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBackUpPort));
                mpServer["backup_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iBackUpPort));

                uint16_t iWCachePort = iPort + 4;
                while(true)
                {
                    iWCachePort = getPort(vtCacheHost[i].serverIp, iWCachePort);
                    if (iWCachePort == 0)
                    {
                        errmsg = "failed to get port for wcache obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iWCachePort)) != setAddr.end())
                    {
                        iWCachePort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iWCachePort));
                mpServer["wcache_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iWCachePort));

                uint16_t iControlAckPort = iPort + 5;
                while(true)
                {
                    iControlAckPort = getPort(vtCacheHost[i].serverIp, iControlAckPort);
                    if (iControlAckPort == 0)
                    {
                        errmsg = "failed to get port for control ack obj|server ip:" + vtCacheHost[i].serverIp;
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                    if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iControlAckPort)) != setAddr.end())
                    {
                        iControlAckPort++;
                        continue;
                    }
                    break;
                }
                setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iControlAckPort));
                mpServer["controlack_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iControlAckPort));
            }
            mpServer["idc_area"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].idc);
            mpServer["POSTTIME"] = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpServer["LASTUSER"] = make_pair(TC_Mysql::DB_STR, "sys");

            if (tcMysql.getRecordCount("t_router_server", "where server_name='" + vtCacheHost[i].serverName + "'") == 0)
            {
                tcMysql.insertRecord("t_router_server", mpServer);
            }

            //避免主机重复
            if (!bReplace && (vtCacheHost[i].type == "M") && tcMysql.getRecordCount("t_router_group", "where server_status='M' AND module_name='" + sModuleName + "' AND group_name='" + vtCacheHost[i].groupName + "'") > 0)
            {
                errmsg = string("module_name:") + sModuleName + ", group_name:" + vtCacheHost[i].groupName + " has master cache server|server name:" + vtCacheHost[i].serverName;
                TLOGERROR(FUN_LOG << errmsg << endl);
                continue;
            }

            string groupsql = "SELECT * FROM t_router_group WHERE module_name='" + sModuleName + "' AND group_name='" + vtCacheHost[i].groupName + "' AND server_name='" + vtCacheHost[i].serverName + "'";
            TC_Mysql::MysqlData existRouterGroup = tcMysql.queryRecord(groupsql);
            if (existRouterGroup.size() > 0)
            {
                if (existRouterGroup[0]["source_server_name"] != vtCacheHost[i].bakSrcServerName)
                {
                    errmsg = string("existed record's source_server_name incosistent with param:") + vtCacheHost[i].bakSrcServerName + "|existed record in database is :" + existRouterGroup[0]["source_server_name"];
                    TLOGERROR(FUN_LOG << errmsg<< endl);
                    return -1;
                }
                else
                {
                    continue;
                }
            }

            map<string, pair<TC_Mysql::FT, string> > mpGroup;
            mpGroup["module_name"]        = make_pair(TC_Mysql::DB_STR, sModuleName);
            mpGroup["group_name"]         = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].groupName);
            mpGroup["server_name"]        = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
            mpGroup["server_status"]      = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].type);
            mpGroup["priority"]           = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].priority);
            mpGroup["source_server_name"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].bakSrcServerName);
            mpGroup["POSTTIME"]           = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpGroup["LASTUSER"]           = make_pair(TC_Mysql::DB_STR, "sys");

            if (bReplace)
                tcMysql.replaceRecord("t_router_group", mpGroup);
            else
                tcMysql.insertRecord("t_router_group", mpGroup);
        }

        // 删除可能存在的模块路由信息
        tcMysql.deleteRecord("t_router_record", "where module_name = '" + sModuleName + "'");

        string sSql = "select group_name from t_router_group where module_name = '" + sModuleName + "' group by group_name order by group_name";
        TC_Mysql::MysqlData data = tcMysql.queryRecord(sSql);
        if (data.size() <= 0)
        {
            errmsg = string("select group_name from t_router_group error|module_name:") + sModuleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
        }

        // 根据组的个数均分路由页
        int num    = TOTAL_ROUTER_PAGE_NUM / data.size();
        int iBegin = 0;
        int iEnd   = MAX_ROUTER_PAGE_NUM;
        for (size_t i = 0; i < data.size(); i++)
        {
            int iToPage;
            if ( (i + 1) == data.size() )
            {
                iToPage = iEnd;
            }
            else
            {
                iToPage = iBegin + num;
            }

            map<string, pair<TC_Mysql::FT, string> > mpRecord;
            mpRecord["module_name"]     = make_pair(TC_Mysql::DB_STR, sModuleName);
            mpRecord["from_page_no"]    = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(iBegin));
            mpRecord["to_page_no"]      = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(iToPage));
            mpRecord["group_name"]      = make_pair(TC_Mysql::DB_STR, data[i]["group_name"]);
            mpRecord["POSTTIME"]        = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpRecord["LASTUSER"]        = make_pair(TC_Mysql::DB_STR, "sys");

            if (bReplace)
                tcMysql.replaceRecord("t_router_record", mpRecord);
            else
                tcMysql.insertRecord("t_router_record", mpRecord);

            iBegin = iToPage + 1;
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("insert module name:") + sModuleName + " to router db's table catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        tcMysql.disconnect();
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertTarsDb(const ProxyParam &stProxyParam, const RouterParam &stRouterParam, const std::string &sTarsVersion, const bool enableGroup, bool bReplace, string & errmsg)
{
    if (stProxyParam.installProxy)
    {
        for (size_t i = 0; i < stProxyParam.serverIp.size(); i++)
        {
            uint16_t iProxyObjPort = getPort(stProxyParam.serverIp[i].ip);
            uint16_t iRouterClientObjPort = getPort(stProxyParam.serverIp[i].ip);
            if (0 == iProxyObjPort || 0 == iRouterClientObjPort)
            {
                //获取PORT失败
                TLOGERROR(FUN_LOG << "failed to get port for " << stProxyParam.serverName << "_" << stProxyParam.serverIp[i].ip << "|" << iProxyObjPort << "|" << iRouterClientObjPort << endl);
            }

            if (insertTarsServerTableWithIdcGroup("DCache", stProxyParam.serverName, stProxyParam.serverIp[i], stProxyParam.serverIp[i].ip, stProxyParam.templateFile, "", sTarsVersion, enableGroup, bReplace, errmsg) != 0)
                return -1;

            string sProxyServant = stProxyParam.serverName + ".ProxyObj";
            string sProxyEndpoint = "tcp -h " + stProxyParam.serverIp[i].ip + " -t 60000 -p "+ TC_Common::tostr(getPort(stProxyParam.serverIp[i].ip, iProxyObjPort));
            if (insertServantTable("DCache", stProxyParam.serverName.substr(7), stProxyParam.serverIp[i].ip, sProxyServant, sProxyEndpoint, "10", bReplace, errmsg) != 0)
                return -1;

            string sRouteClientServant = stProxyParam.serverName + ".RouterClientObj";
            string sRouteClientEndpoint = "tcp -h " + stProxyParam.serverIp[i].ip + " -t 60000 -p " + TC_Common::tostr(getPort(stProxyParam.serverIp[i].ip, iRouterClientObjPort));
            if (insertServantTable("DCache", stProxyParam.serverName.substr(7), stProxyParam.serverIp[i].ip, sRouteClientServant, sRouteClientEndpoint, "10", bReplace, errmsg) != 0)
                return -1;
        }
    }

    if (stRouterParam.installRouter)
    {
        for (size_t i = 0; i < stRouterParam.serverIp.size(); ++i)
        {
            if (insertTarsServerTable("DCache", stRouterParam.serverName, stRouterParam.serverIp[i], stRouterParam.templateFile, "", sTarsVersion, false, bReplace, errmsg) != 0)
                return -1;

            string sRouterServant = stRouterParam.serverName + ".RouterObj";
            uint16_t iRouterObjPort = getPort(stRouterParam.serverIp[i]);
            if (0 == iRouterObjPort)
            {
                TLOGERROR(FUN_LOG << "failed to get port for " << stRouterParam.serverName << "_" << stRouterParam.serverIp[i]<< "|" << iRouterObjPort << endl);
            }

            string sRouterEndpoint = "tcp -h " + stRouterParam.serverIp[i] + " -t 60000 -p " + TC_Common::tostr(iRouterObjPort);
            if (insertServantTable("DCache", stRouterParam.serverName.substr(7),  stRouterParam.serverIp[i], sRouterServant, sRouterEndpoint, "10", bReplace, errmsg) != 0)
                return -1;
        }
    }

    return 0;
}

int DCacheOptImp::insertCache2TarsDb(const TC_DBConf &routerDbInfo, const vector<DCache::CacheHostParam> & vtCacheHost, DCache::DCacheType eCacheType, const string &sTarsVersion, bool bReplace, std::string &errmsg)
{
    string sExePath;
    if (eCacheType == DCache::KVCACHE)
        sExePath = "KVCacheServer";
    else
        sExePath = "MKVCacheServer";

    TC_Mysql tcMysql;
    try
    {
        tcMysql.init(routerDbInfo);

        for (size_t i = 0; i < vtCacheHost.size(); i++)
        {
            if (insertTarsServerTable("DCache", vtCacheHost[i].serverName, vtCacheHost[i].serverIp, vtCacheHost[i].templateFile, sExePath, sTarsVersion, false, bReplace, errmsg) != 0)
            {
                errmsg = string("insert cache info to tars server conf table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            string sBinLogPort, sCachePort,sRouterPort,sBackUpPort,sWCachePort,sControlAckPort;

            if (selectPort(tcMysql, vtCacheHost[i].serverName, vtCacheHost[i].serverIp, sBinLogPort, sCachePort, sRouterPort, sBackUpPort, sWCachePort, sControlAckPort, errmsg) != 0)
            {
                errmsg = string("select cache servant info from releation cache router table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            string sBinLogServant = vtCacheHost[i].serverName + ".BinLogObj";
            string sBinLogEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sBinLogPort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sBinLogServant, sBinLogEndpoint, "3", bReplace, errmsg) != 0)
            {
                errmsg = string("insert cache binlog servant info to tars adapter conf table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            string sCacheServant = vtCacheHost[i].serverName + ".CacheObj";
            string sCacheEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sCachePort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sCacheServant, sCacheEndpoint, "8", bReplace, errmsg) != 0)
            {
                errmsg = string("insert cache cache servant info to tars adapter conf table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            string sRouterServant = vtCacheHost[i].serverName + ".RouterClientObj";
            string sRouterEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 180000 -p " + sRouterPort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sRouterServant, sRouterEndpoint, "5", bReplace, errmsg) != 0)
            {
                errmsg = string("insert cache router client servant info to tars adapter conf table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            string sBackUpServant = vtCacheHost[i].serverName + ".BackUpObj";
            string sBackupEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sBackUpPort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sBackUpServant, sBackupEndpoint, "1", bReplace, errmsg) != 0)
            {
                errmsg = string("insert cache backup servant info to tars adapter conf table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            string sWCacheServant = vtCacheHost[i].serverName + ".WCacheObj";
            string sWCacheEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sWCachePort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sWCacheServant, sWCacheEndpoint, "8", bReplace, errmsg) != 0)
            {
                errmsg = string("insert cache wcache servant info to tars adapter conf table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            string sControlAckServant = vtCacheHost[i].serverName + ".ControlAckObj";
            string sControlAckEndpoint = "tcp -h "+ vtCacheHost[i].serverIp + " -t 60000 -p " + sControlAckPort;
            if (insertServantTable("DCache", vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, sControlAckServant, sControlAckEndpoint, "1", bReplace, errmsg) != 0)
            {
                errmsg = string("insert cache control ack servant info to tars adapter conf table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }
        }

        return 0;
    }
    catch(exception &ex)
    {
        tcMysql.disconnect();
        errmsg = string("insert cache info to tars db catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }
    catch (...)
    {
        tcMysql.disconnect();
        errmsg = string("insert cache info to tars db catch unknown exception");
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return -1;
}

int DCacheOptImp::insertTarsConfigFilesTable(const string &sFullServerName, const string &sConfName, const string &sHost, const string &sConfig, const int level, int &id, bool bReplace, string & errmsg)
{
    string sSql;
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;

        m["server_name"]    = make_pair(TC_Mysql::DB_STR, sFullServerName);
        m["host"]           = make_pair(TC_Mysql::DB_STR, sHost);
        m["filename"]       = make_pair(TC_Mysql::DB_STR, sConfName);
        m["config"]         = make_pair(TC_Mysql::DB_STR, sConfig);
        m["posttime"]       = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]       = make_pair(TC_Mysql::DB_STR, "system");
        m["level"]          = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(level));
        m["config_flag"]    = make_pair(TC_Mysql::DB_INT, "0");

        if(bReplace)
            _mysqlTarsDb.replaceRecord("t_config_files", m);
        else
            _mysqlTarsDb.insertRecord("t_config_files", m);

        sSql= "select id from t_config_files where server_name = '" + sFullServerName + "' and filename = '" + sConfName + "' and host = '" + sHost + "' and level = " + TC_Common::tostr(level);
        TC_Mysql::MysqlData data = _mysqlTarsDb.queryRecord(sSql);
        if (data.size() != 1)
        {
            errmsg = string("get id from t_config_files error, serverName = ") + sFullServerName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
        }

        id = TC_Common::strto<int>(data[0]["id"]);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("insert server name:") + sFullServerName + " catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertTarsHistoryConfigFilesTable(const int iConfigId, const string &sConfig, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["configid"]       = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(iConfigId));
        m["reason"]         = make_pair(TC_Mysql::DB_STR, "新建立配置文件");
        m["reason_select"]  = make_pair(TC_Mysql::DB_STR, "");
        m["content"]        = make_pair(TC_Mysql::DB_STR, sConfig);
        m["posttime"]       = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]       = make_pair(TC_Mysql::DB_STR, "system");

        if(bReplace)
            _mysqlTarsDb.replaceRecord("t_config_history_files", m);
        else
            _mysqlTarsDb.insertRecord("t_config_history_files", m);
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert server:") + TC_Common::tostr(iConfigId) + " history config error, catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertTarsServerTableWithIdcGroup(const string &sApp, const string& sServerName , const ProxyAddr &stProxyAddr, const string &sIp, const string &sTemplateName, const string &sExePath,const std::string &sTarsVersion, const bool enableGroup, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["application"]    = make_pair(TC_Mysql::DB_STR, sApp);
        m["server_name"]    = make_pair(TC_Mysql::DB_STR, sServerName.substr(7));//去掉前缀DCache.
        m["node_name"]      = make_pair(TC_Mysql::DB_STR, sIp);
        m["exe_path"]       = make_pair(TC_Mysql::DB_STR, sExePath);
        m["template_name"]  = make_pair(TC_Mysql::DB_STR, sTemplateName);
        m["posttime"]       = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]       = make_pair(TC_Mysql::DB_STR, "sys");
        m["tars_version"]   = make_pair(TC_Mysql::DB_STR, sTarsVersion);

        // 这里默认开启IDC分组
        m["enable_group"] = make_pair(TC_Mysql::DB_STR,(enableGroup)?"Y":"N");//是否启用IDC分组

        if (stProxyAddr.idcArea == "sz")
        {
            m["ip_group_name"] = make_pair(TC_Mysql::DB_STR, "sz");
        }
        else if (stProxyAddr.idcArea == "tj")
        {
            m["ip_group_name"] = make_pair(TC_Mysql::DB_STR, "tj");
        }
        else if (stProxyAddr.idcArea == "sh")
        {
            m["ip_group_name"] = make_pair(TC_Mysql::DB_STR, "sh");
        }
        else if (stProxyAddr.idcArea == "cd")
        {
            m["ip_group_name"] = make_pair(TC_Mysql::DB_STR, "cd");
        }
        else
        {
            //启用IDC分组改为No
            m["enable_group"] = make_pair(TC_Mysql::DB_STR, "N");
        }

        if(bReplace)
            _mysqlTarsDb.replaceRecord("t_server_conf", m);
        else
            _mysqlTarsDb.insertRecord("t_server_conf", m);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("insert appName:") + sApp + ", serverName:" + sServerName + "@" + sIp +"'s tars server config error, " + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::insertTarsServerTable(const string &sApp, const string &sServerName, const string &sIp, const string &sTemplateName, const string &sExePath,const std::string &sTarsVersion, const bool enableGroup, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["application"]    = make_pair(TC_Mysql::DB_STR, sApp);
        m["server_name"]    = make_pair(TC_Mysql::DB_STR, sServerName.substr(7));
        m["node_name"]      = make_pair(TC_Mysql::DB_STR, sIp);
        m["exe_path"]       = make_pair(TC_Mysql::DB_STR, sExePath);
        m["template_name"]  = make_pair(TC_Mysql::DB_STR, sTemplateName);
        m["posttime"]       = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]       = make_pair(TC_Mysql::DB_STR, "sys");
        m["tars_version"]   = make_pair(TC_Mysql::DB_STR, sTarsVersion);
        m["enable_group"]   = make_pair(TC_Mysql::DB_STR, (enableGroup)?"Y":"N");//是否启用IDC分组

        if (bReplace)
            _mysqlTarsDb.replaceRecord("t_server_conf", m);
        else
            _mysqlTarsDb.insertRecord("t_server_conf", m);

        return 0;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert") + sApp + "."+ sServerName + "@" + sIp + "'s tars server config error, catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return -1;
}

int DCacheOptImp::insertServantTable(const string &sApp, const string &sServerName, const string &sIp, const string &sServantName, const string &sEndpoint, const string &sThreadNum, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["application"]        = make_pair(TC_Mysql::DB_STR, sApp);
        m["server_name"]        = make_pair(TC_Mysql::DB_STR, sServerName);
        m["node_name"]          = make_pair(TC_Mysql::DB_STR, sIp);
        m["adapter_name"]       = make_pair(TC_Mysql::DB_STR, sServantName + "Adapter");
        m["thread_num"]         = make_pair(TC_Mysql::DB_INT, sThreadNum);
        m["endpoint"]           = make_pair(TC_Mysql::DB_STR, sEndpoint);
        m["max_connections"]    = make_pair(TC_Mysql::DB_INT, "10240");
        m["servant"]            = make_pair(TC_Mysql::DB_STR, sServantName);
        m["queuecap"]           = make_pair(TC_Mysql::DB_INT, "100000");
        m["queuetimeout"]       = make_pair(TC_Mysql::DB_INT, "60000");
        m["posttime"]           = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
        m["lastuser"]           = make_pair(TC_Mysql::DB_STR, "sys");
        m["protocol"]           = make_pair(TC_Mysql::DB_STR, "tars");

        if (bReplace)
            _mysqlTarsDb.replaceRecord("t_adapter_conf", m);
        else
            _mysqlTarsDb.insertRecord("t_adapter_conf", m);

        return 0;
    }
    catch(const std::exception &ex)
    {
        errmsg = string("insert ") + sApp + "." + sServerName + "@" + sIp + ":" + sServantName + " config catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return -1;
}

bool DCacheOptImp::hasServer(const string&sApp, const string &sServerName)
{
    string sSql = "select id from t_server_conf where application='" + sApp + "' and server_name='" + sServerName + "'";
    return _mysqlTarsDb.existRecord(sSql);
}

int DCacheOptImp::selectPort(TC_Mysql &tcMysql, const string &sServerName, const string &sIp, string &sBinLogPort, string &sCachePort, string &sRouterPort,string &sBackUpPort,string &sWCachePort,string &sControlAckPort, std::string &errmsg)
{
    try
    {
        string sSql = "select binlog_port,cache_port,routerclient_port,backup_port,wcache_port,controlack_port from t_router_server where server_name = '" + sServerName +"' and ip = '"+ sIp + "'";
        TC_Mysql::MysqlData data = tcMysql.queryRecord(sSql);
        if (data.size() != 1)
        {
            errmsg = string("get port from t_router_server error|server name:") + sServerName + "|ip:" + sIp;
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
        }

        sBinLogPort     = data[0]["binlog_port"];
        sCachePort      = data[0]["cache_port"];
        sRouterPort     = data[0]["routerclient_port"];
        sBackUpPort     = data[0]["backup_port"];
        sWCachePort     = data[0]["wcache_port"];
        sControlAckPort = data[0]["controlack_port"];
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get port from t_router_server catch exception|server name:") + sServerName + "|ip:" + sIp + "|exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

int DCacheOptImp::checkDb(TC_Mysql &tcMysql, const string &sDbName)
{
    try
    {
        string sSql = "show databases";

        TC_Mysql::MysqlData data = tcMysql.queryRecord(sSql);
        for (size_t i = 0; i < data.size(); i++)
        {
            if (data[i]["Database"] == sDbName)
            {
                return 1;
            }
        }
    }
    catch(const std::exception &ex)
    {
        string sErr = string("check db: ") + sDbName + " error, catch exception: " + ex.what();
        TLOGERROR(sErr << endl);
        throw DCacheOptException(sErr);
    }

    return 0;
}

int DCacheOptImp::checkTable(TC_Mysql &tcMysql, const string &sDbName, const string &sTableName)
{
    try
    {
        string sSql = "show tables from " + sDbName;
        TC_Mysql::MysqlData data = tcMysql.queryRecord(sSql);

        string sField = "Tables_in_" + sDbName;
        for (size_t i = 0; i < data.size(); i++)
        {
            if (data[i][sField] == sTableName)
            {
                return 1;
            }
        }
    }
    catch(const std::exception &ex)
    {
        string sErr = string("check table: ") + sTableName + " from db:" + sDbName + " error, exception: " + ex.what();
        TLOGERROR(FUN_LOG << sErr << endl);
        throw DCacheOptException(sErr);
    }

    return 0;
}

bool DCacheOptImp::checkIP(const string &sIp)
{
    //检查ip是否合法
    vector<string> vDigit;
    vDigit = TC_Common::sepstr<string>(sIp, ".",false);
    if ( 4 == vDigit.size())
    {
        //检查是否为数字
        if (!(TC_Common::isdigit(vDigit[0])
            && TC_Common::isdigit(vDigit[1])
            && TC_Common::isdigit(vDigit[2])
            && TC_Common::isdigit(vDigit[3])))
        {
            TLOGERROR(FUN_LOG << "ip format error in GetPort(),some nondigit exists|" <<sIp << endl);
            return false;
        }

        //检测每段数据范围是否合法
        unsigned iOne    = TC_Common::strto<unsigned>(vDigit[0]);
        unsigned iTwo    = TC_Common::strto<unsigned>(vDigit[1]);
        unsigned iThree  = TC_Common::strto<unsigned>(vDigit[2]);
        unsigned iFour   = TC_Common::strto<unsigned>(vDigit[3]);
        if (!((iOne <= 255 && iOne >=0)
            && (iTwo <= 255 && iTwo >=0)
            && (iThree <= 255 && iThree >=0)
            && (iFour <= 255 && iFour >=0)))
        {
            TLOGERROR(FUN_LOG << "ip format error in GetPort(), digit scope error" << endl);
            return false;
        }
    }
    else
    {
        TLOGERROR(FUN_LOG << "ip format error in GetPort(), the size of splited ip is  " << vDigit.size() << endl);
        return false;
    }

    return true;
}

uint16_t DCacheOptImp::getPort(const string &sIp, uint16_t iPort)
{
    if (iPort == 0)
    {
        iPort = getRand();
    }

    while(true)
    {
        if (!checkIP(sIp))
        {
            return 0;
        }

        int s,flags,retcode;
        struct sockaddr_in  peer;

        s = socket(AF_INET, SOCK_STREAM, 0);
        if ((flags = fcntl(s, F_GETFL, 0)) < 0)
        {
            continue;
        }

        if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            continue;
        }

        peer.sin_family      = AF_INET;
        peer.sin_port        = htons(iPort);
        peer.sin_addr.s_addr=inet_addr(sIp.c_str());

        retcode = connect(s, (struct sockaddr*)&peer, sizeof(peer));
        if (retcode == 0)
        {
            close(s);
            continue;
        }

        fd_set rdevents,wrevents,exevents;

        FD_ZERO(&rdevents);
        FD_SET(s, &rdevents);

        wrevents = rdevents;
        exevents = rdevents;
        struct timeval tv;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        retcode = select(s+1, &rdevents, &wrevents, &exevents, &tv);

        if (retcode < 0)
        {
            close(s);
            break;
        }
        else if (retcode == 0)
        {
            close(s);
            break;
        }
        else
        {
            int err;
            socklen_t errlen = sizeof(err);
            getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &errlen);
            if (err)
            {
                close(s);
                break;
            }
        }
        close(s);

        iPort = getRand();
    }

    return iPort;
}

uint16_t DCacheOptImp::getRand()
{
    struct timeval tv;
    TC_Common::gettimeofday(tv);

    srand(tv.tv_usec);
    return (rand() % 20000) + 10000;
}

void DCacheOptImp::insertProxyRouter(const string &sProxyName, const string &sRouterName, const string &sDbName, const string &sIp, const string& sModuleNameList, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > mpProxyRouter;
        mpProxyRouter["proxy_name"]     = make_pair(TC_Mysql::DB_STR, sProxyName);
        mpProxyRouter["router_name"]    = make_pair(TC_Mysql::DB_STR, sRouterName);
        mpProxyRouter["db_name"]        = make_pair(TC_Mysql::DB_STR, sDbName);
        mpProxyRouter["db_ip"]          = make_pair(TC_Mysql::DB_STR, sIp);

        string sql = "select * from t_proxy_router where proxy_name='" + sProxyName + "' and router_name='" + sRouterName + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sql);
        string mnlist = sModuleNameList;
        if (data.size() == 1)
        {
            vector<string> vtModuleNameList;
            vtModuleNameList = TC_Common::sepstr<string>(data[0]["modulename_list"] + ";" + sModuleNameList, ";");
            mnlist = "";
            for (size_t i = 0; i < vtModuleNameList.size(); ++i)
            {
                string tmp = vtModuleNameList[i];
                mnlist += tmp + string(";");
                vtModuleNameList.erase(vtModuleNameList.begin() + i);

                // 删除相同的模块名
                while(true)
                {
                    vector<string>::iterator it = find(vtModuleNameList.begin(), vtModuleNameList.end(), tmp);
                    if (it == vtModuleNameList.end())
                    {
                        break;
                    }
                    vtModuleNameList.erase(it);
                }
            }
        }

        mpProxyRouter["modulename_list"] = make_pair(TC_Mysql::DB_STR, mnlist);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_proxy_router", mpProxyRouter);
        else
            _mysqlRelationDB.insertRecord("t_proxy_router", mpProxyRouter);
    }
    catch (exception &ex)
    {
        errmsg = string("insert proxy-router info catch exception:") + string(ex.what());
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }
}

int DCacheOptImp::insertCacheRouter(const string &sCacheName, const string &sCacheIp, int memSize, const string &sRouterName, const TC_DBConf &routerDbInfo, const string& sModuleName, bool bReplace, string & errmsg)
{
    TLOGDEBUG(FUN_LOG << "cache server name:" << sCacheName << "|router server name:" << sRouterName << endl);

    vector<string> vRouterName = TC_Common::sepstr<string>(sRouterName, ".");
    if (vRouterName.size() != 3)
    {
        errmsg = "router server name is invalid, router name:" + sRouterName;
        TLOGERROR(FUN_LOG << errmsg << endl);
        return -1;
    }

    string sSql = "select * from t_router_app where router_name='" + vRouterName[0] + "." + vRouterName[1] + "'";
    TC_Mysql::MysqlData appNameData = _mysqlRelationDB.queryRecord(sSql);
    if (appNameData.size() != 1)
    {
        errmsg = string("select from t_router_app result data size:") + TC_Common::tostr(appNameData.size()) + "no equal to 1.|router name:" + vRouterName[0] + "." + vRouterName[1];
        TLOGERROR(FUN_LOG << errmsg << endl);
        return -1;
    }

    map<string, pair<TC_Mysql::FT, string> > mpProxyRouter;
    try
    {
        TC_Mysql mysqlRouterDb(routerDbInfo);
        sSql = "select * from t_router_group where server_name='DCache." + sCacheName + "'";
        TC_Mysql::MysqlData groupInfo = mysqlRouterDb.queryRecord(sSql);
        if (groupInfo.size() != 1)
        {
            errmsg = string("select from t_router_group result data size:") + TC_Common::tostr(groupInfo.size()) + " no equal to 1.|cache server name:" + "DCache." + sCacheName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }


        sSql = "select idc_area from t_router_server where server_name='DCache." + sCacheName + "'";
        TC_Mysql::MysqlData serverInfo = mysqlRouterDb.queryRecord(sSql);
        if (serverInfo.size() != 1)
        {
            errmsg = string("select from t_router_server result data size:") + TC_Common::tostr(serverInfo.size()) + " no equal to 1.|cache server name:" + "DCache." + sCacheName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        mpProxyRouter["cache_name"]     = make_pair(TC_Mysql::DB_STR, sCacheName);
        mpProxyRouter["cache_ip"]       = make_pair(TC_Mysql::DB_STR, sCacheIp);
        mpProxyRouter["router_name"]    = make_pair(TC_Mysql::DB_STR, sRouterName);
        mpProxyRouter["db_name"]        = make_pair(TC_Mysql::DB_STR, routerDbInfo._database);
        mpProxyRouter["db_ip"]          = make_pair(TC_Mysql::DB_STR, routerDbInfo._host);
        mpProxyRouter["db_port"]        = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(routerDbInfo._port));
        mpProxyRouter["db_user"]        = make_pair(TC_Mysql::DB_STR, routerDbInfo._user);
        mpProxyRouter["db_pwd"]         = make_pair(TC_Mysql::DB_STR, routerDbInfo._password);
        mpProxyRouter["db_charset"]     = make_pair(TC_Mysql::DB_STR, routerDbInfo._charset);
        mpProxyRouter["module_name"]    = make_pair(TC_Mysql::DB_STR, sModuleName);
        mpProxyRouter["app_name"]       = make_pair(TC_Mysql::DB_STR, appNameData[0]["app_name"]);
        mpProxyRouter["idc_area"]       = make_pair(TC_Mysql::DB_STR, serverInfo[0]["idc_area"]);
        mpProxyRouter["group_name"]     = make_pair(TC_Mysql::DB_STR, groupInfo[0]["group_name"]);
        mpProxyRouter["server_status"]  = make_pair(TC_Mysql::DB_STR, groupInfo[0]["server_status"]);
        mpProxyRouter["priority"]       = make_pair(TC_Mysql::DB_STR, groupInfo[0]["priority"]);
        mpProxyRouter["mem_size"]       = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(memSize));

        _mysqlRelationDB.replaceRecord("t_cache_router", mpProxyRouter);

        return 0;
    }
    catch (exception &ex)
    {
        errmsg = string("query router db catch exception|db name:") + routerDbInfo._database + "|db host:" + routerDbInfo._host + "|port:" + TC_Common::tostr(routerDbInfo._port) + "|exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return -1;
}

void DCacheOptImp::insertProxyCache(const string &sProxyObj, const string & sCacheName, bool bReplace)
{
    map<string, pair<TC_Mysql::FT, string> > mpProxyCache;
    mpProxyCache["proxy_name"]     = make_pair(TC_Mysql::DB_STR, sProxyObj);
    mpProxyCache["cache_name"]     = make_pair(TC_Mysql::DB_STR, sCacheName);

    if (bReplace)
        _mysqlRelationDB.replaceRecord("t_proxy_cache", mpProxyCache);
    else
        _mysqlRelationDB.insertRecord("t_proxy_cache", mpProxyCache);
}

int DCacheOptImp::getRouterDBFromAppTable(const string &appName, TC_DBConf &routerDbInfo, vector<string> & sProxyName, string & sRouterName, string &errmsg)
{
    try
    {
        errmsg = "";
        string sSql("");

        sSql = "select * from t_proxy_app where app_name='" + appName + "'";

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);

        if (data.size() > 0)
        {
            for (size_t i = 0; i < data.size(); i++)
            {
                sProxyName.push_back(data[i]["proxy_name"]);
            }

            sSql = "select * from t_router_app where app_name='" + appName + "'";
            data = _mysqlRelationDB.queryRecord(sSql);
            if (data.size() > 0)
            {
                sRouterName             = data[0]["router_name"];
                routerDbInfo._host      = data[0]["host"];
                routerDbInfo._database  = data[0]["db_name"];
                routerDbInfo._port      = TC_Common::strto<int>(data[0]["port"]);
                routerDbInfo._user      = data[0]["user"];
                routerDbInfo._password  = data[0]["password"];
                routerDbInfo._charset   = data[0]["charset"];

                return 0;
            }
            else
            {
                errmsg = string("select from table t_router_app no find app name:") + appName;
                TLOGERROR(FUN_LOG << errmsg << endl);
            }
        }
        else
        {
            errmsg = string("select from table t_proxy_app no find app name:") + appName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch(exception &ex)
    {
        errmsg = string("select from relation db catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = "select from relation db catch unknown exception";
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::insertRelationAppTable(const string &appName, const string &proxyServer,const string &routerServer,const DCache::RouterParam & stRouter, bool bReplace, string & errmsg)
{
    try
    {
        map<string, pair<TC_Mysql::FT, string> > m;
        m["proxy_name"] = make_pair(TC_Mysql::DB_STR, proxyServer);
        m["app_name"]   = make_pair(TC_Mysql::DB_STR, appName);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_proxy_app", m);
        else
            _mysqlRelationDB.insertRecord("t_proxy_app", m);

        map<string, pair<TC_Mysql::FT, string> > n;
        n["router_name"] = make_pair(TC_Mysql::DB_STR, routerServer);
        n["app_name"]    = make_pair(TC_Mysql::DB_STR, appName);
        n["host"]        = make_pair(TC_Mysql::DB_STR, stRouter.dbIp);
        n["port"]        = make_pair(TC_Mysql::DB_STR, stRouter.dbPort);
        n["user"]        = make_pair(TC_Mysql::DB_STR, stRouter.dbUser);
        n["password"]    = make_pair(TC_Mysql::DB_STR, stRouter.dbPwd);
        n["db_name"]     = make_pair(TC_Mysql::DB_STR, stRouter.dbName);

        if (bReplace)
            _mysqlRelationDB.replaceRecord("t_router_app", n);
        else
            _mysqlRelationDB.insertRecord("t_router_app", n);
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert relation info catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
    }

    return 0;
}

/*
*   expand/transfer
*/
int DCacheOptImp::expandCacheConf(const string &appName, const string &moduleName, const vector<DCache::CacheHostParam> & vtCacheHost, bool bReplace, string & errmsg)
{
    int iAppConfigId = 0;

    getAppModConfigId(appName, moduleName, iAppConfigId, errmsg);

    for (size_t i = 0; i < vtCacheHost.size(); ++i)
    {
        vector < map <string, string> > level3Vec;

        string shmSize = TC_Common::tostr(vtCacheHost[i].shmSize);

        insertConfigItem2Vector("ShmSize", "Main/Cache",  shmSize + "G", level3Vec);

        size_t shmKey = TC_Common::strto<size_t>(vtCacheHost[i].shmKey);
        if(shmKey == 0)
        {
            if(getShmKey(shmKey) != 0)
                return -1;
        }

        insertConfigItem2Vector("ShmKey", "Main/Cache", TC_Common::tostr(shmKey), level3Vec);

        // if (!vtCacheHost[i].shmKey.empty())
        // {
        //     insertConfigItem2Vector("ShmKey", "Main/Cache", TC_Common::tostr(vtCacheHost[i].shmKey), level3Vec);
        // }

        if (insertConfigFilesTable(vtCacheHost[i].serverName, vtCacheHost[i].serverIp, 0, level3Vec, 2, bReplace, errmsg) != 0)
        {
            errmsg = string("insert config file table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        if (insertReferenceTable(iAppConfigId, vtCacheHost[i].serverName, vtCacheHost[i].serverIp, bReplace, errmsg) != 0)
        {
            errmsg = string("insert reference table failed|server name:") + vtCacheHost[i].serverName + "|server ip:" + vtCacheHost[i].serverIp + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }
    }

    return 0;
}

int DCacheOptImp::getAppModConfigId(const string &appName, const string &moduleName, int &id, string & errmsg)
{
    try
    {
        string sSql = "select id from t_config_appMod where appName = '" + appName + "' and moduleName = '" + moduleName + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() != 1)
        {
            errmsg = string("get id from t_config_appMod error, appName:") + appName + ", moduleName:" + moduleName + ", size:" + TC_Common::tostr<int>(data.size()) + " not equal to 1.";
            TLOGERROR(FUN_LOG << errmsg << endl);
            throw DCacheOptException(errmsg);
            return -1;
        }

        id = TC_Common::strto<int>(data[0]["id"]);
    }
    catch(const std::exception &ex)
    {
        errmsg = string("get id from t_config_appMod appName:") + appName + ", moduleName:" + moduleName + ", catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        throw DCacheOptException(errmsg);
        return -1;
    }

    return 0;
}

int DCacheOptImp::expandCache2RouterDb(const string &sModuleName, const TC_DBConf &routerDbInfo, const vector<DCache::CacheHostParam> & vtCacheHost, bool bReplace, string& errmsg)
{
    TC_Mysql tcMysql;
    try
    {
        TLOGDEBUG(FUN_LOG << "router db name:" << routerDbInfo._database << "|host:" << routerDbInfo._host << "|port:" << routerDbInfo._port << endl);

        tcMysql.init(routerDbInfo);

        uint16_t iPort = getPort(vtCacheHost[0].serverIp);
        if (0 == iPort)
        {
            //获取PORT失败
            errmsg = "failed to get port for cache server ip:" + vtCacheHost[0].serverIp;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        set<string> setAddr;
        for (size_t i = 0; i < vtCacheHost.size(); ++i)
        {
            //save cache server info to routerdb.t_router_server
            map<string, pair<TC_Mysql::FT, string> > mpServer;
            mpServer["server_name"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
            mpServer["ip"]          = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverIp);

            uint16_t iBinLogPort = iPort;
            while (true)
            {
                iBinLogPort = getPort(vtCacheHost[i].serverIp, iBinLogPort);
                if (iBinLogPort == 0)
                {
                    errmsg = "failed to get port for binlog obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBinLogPort)) != setAddr.end())
                {
                    iBinLogPort++;
                    continue;
                }

                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBinLogPort));
            mpServer["binlog_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iBinLogPort));

            uint16_t iCachePort = iPort + 1;
            while (true)
            {
                iCachePort = getPort(vtCacheHost[i].serverIp, iCachePort);
                if (iCachePort == 0)
                {
                    errmsg = "failed to get port for cache obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iCachePort)) != setAddr.end())
                {
                    iCachePort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iCachePort));
            mpServer["cache_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iCachePort));

            uint16_t iRouterPort = iPort + 2;
            while (true)
            {
                iRouterPort = getPort(vtCacheHost[i].serverIp, iRouterPort);
                if (iRouterPort == 0)
                {
                    errmsg = "failed to get port for router client obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iRouterPort)) != setAddr.end())
                {
                    iRouterPort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iRouterPort));
            mpServer["routerclient_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iRouterPort));

            uint16_t iBackUpPort = iPort + 3;
            while (true)
            {
                iBackUpPort = getPort(vtCacheHost[i].serverIp, iBackUpPort);
                if (iBackUpPort == 0)
                {
                    errmsg = "failed to get port for backup obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBackUpPort)) != setAddr.end())
                {
                    iBackUpPort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iBackUpPort));
            mpServer["backup_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iBackUpPort));

            uint16_t iWCachePort = iPort + 4;
            while (true)
            {
                iWCachePort = getPort(vtCacheHost[i].serverIp, iWCachePort);
                if (iWCachePort == 0)
                {
                    errmsg = "failed to get port for wcache obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iWCachePort)) != setAddr.end())
                {
                    iWCachePort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iWCachePort));
            mpServer["wcache_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iWCachePort));

            uint16_t iControlAckPort = iPort + 5;
            while (true)
            {
                iControlAckPort = getPort(vtCacheHost[i].serverIp, iControlAckPort);
                if (iControlAckPort == 0)
                {
                    errmsg = "failed to get port for control ack obj|server ip:" + vtCacheHost[i].serverIp;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                if (setAddr.find(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iControlAckPort)) != setAddr.end())
                {
                    iControlAckPort++;
                    continue;
                }
                break;
            }
            setAddr.insert(vtCacheHost[i].serverIp + ":" +TC_Common::tostr(iControlAckPort));
            mpServer["controlack_port"] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(iControlAckPort));

            mpServer["idc_area"] = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].idc);
            mpServer["POSTTIME"] = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpServer["LASTUSER"] = make_pair(TC_Mysql::DB_STR, "sys");

            if (tcMysql.getRecordCount("t_router_server", "where server_name='" + vtCacheHost[i].serverName + "'") == 0)
            {
                if (bReplace)
                    tcMysql.replaceRecord("t_router_server", mpServer);
                else
                    tcMysql.insertRecord("t_router_server", mpServer);
            }

            //save cache server info to routerdb.t_router_group
            //避免主机重复
            if (!bReplace && vtCacheHost[i].type == "M" && tcMysql.getRecordCount("t_router_group", "where server_status='M' AND module_name='" + sModuleName + "' AND group_name='" + vtCacheHost[i].groupName + "' AND server_status='M'") > 0)
            {
                errmsg = string("module_name:") + sModuleName + ", group_name:" + vtCacheHost[i].groupName + " has master cache server|server name:" + vtCacheHost[i].serverName;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            string sSql = "select * from t_router_group where module_name='" + sModuleName + "' AND group_name='" + vtCacheHost[i].groupName + "' AND server_name='" + vtCacheHost[i].serverName + "'";
            TC_Mysql::MysqlData existRouterGroup = tcMysql.queryRecord(sSql);
            if (existRouterGroup.size() > 0)
            {
                if (existRouterGroup[0]["source_server_name"] != vtCacheHost[i].bakSrcServerName)
                {
                    errmsg = string("source_server_name incosistent-param:") + vtCacheHost[i].bakSrcServerName + "|database:" + existRouterGroup[0]["source_server_name"];
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                else
                {
                    continue;
                }
            }

            map<string, pair<TC_Mysql::FT, string> > mpGroup;
            mpGroup["module_name"]          = make_pair(TC_Mysql::DB_STR, sModuleName);
            mpGroup["group_name"]           = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].groupName);
            mpGroup["server_name"]          = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].serverName);
            mpGroup["server_status"]        = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].type);
            mpGroup["priority"]             = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].priority);
            mpGroup["source_server_name"]   = make_pair(TC_Mysql::DB_STR, vtCacheHost[i].bakSrcServerName);
            mpGroup["POSTTIME"]             = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
            mpGroup["LASTUSER"]             = make_pair(TC_Mysql::DB_STR, "sys");

            if (bReplace)
                tcMysql.replaceRecord("t_router_group", mpGroup);
            else
                tcMysql.insertRecord("t_router_group", mpGroup);
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("expand module name:") + sModuleName + " to router db's table catch exception:" + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
        tcMysql.disconnect();
        throw DCacheOptException(errmsg);
    }

    return -1;
}

int DCacheOptImp::getRouterObj(const string & sWhereName, string & routerObj, string & errmsg, int type)
{
    try
    {
        errmsg = "";
        string sSql ("");
        if (type == 1)
        {
            sSql = "select * from t_cache_router where app_name='" + sWhereName + "' limit 1";
        }
        else
        {
            sSql = "select * from t_cache_router where cache_name='" + sWhereName + "' limit 1";
        }

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            routerObj = data[0]["router_name"];

            return 0;
        }
        else
        {
            errmsg = "not find router info in t_cache_router";
            TLOGERROR(FUN_LOG << errmsg << "|sql:" << sSql << endl);
        }
    }
    catch(exception &ex)
    {
        errmsg = string("get router obj from relation db t_cache_router table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = "get router obj from relation db t_cache_router table catch unknown exception";
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::getRouterDBInfo(const string &appName, TC_DBConf &routerDbInfo, string& errmsg)
{
    try
    {
        string sSql("");
        sSql = "select * from t_cache_router where app_name='" + appName + "'";

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            routerDbInfo._host      = data[0]["db_ip"];
            routerDbInfo._database  = data[0]["db_name"];
            routerDbInfo._port      = TC_Common::strto<int>(data[0]["db_port"]);
            routerDbInfo._user      = data[0]["db_user"];
            routerDbInfo._password  = data[0]["db_pwd"];
            routerDbInfo._charset   = data[0]["db_charset"];

            return 0;
        }
        else
        {
            errmsg = string("not find router db config in relation db table t_cache_router|app name:") + appName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch(exception &ex)
    {
        errmsg = string("get router db info from relation db t_cache_router table catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = "get router db info from relation db t_cache_router table catch unknown exception";
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::expandDCacheServer(const std::string & appName,const std::string & moduleName,const vector<DCache::CacheHostParam> & vtCacheHost,const std::string & sTarsVersion,DCache::DCacheType cacheType,bool bReplace,std::string &errmsg)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "appName:" << appName << "|moduleName:" << moduleName << "|cacheType:" << etos(cacheType) << "|replace:" << bReplace << endl);

        if (cacheType != DCache::KVCACHE && cacheType != DCache::MKVCACHE)
        {
            errmsg = "expand type error: KVCACHE-1 or MKVCACHE-2";
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        //初始化out变量
        errmsg = "success";

        if (vtCacheHost.size() == 0)
        {
            errmsg = "no cache server specified, please make sure params is right";
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        TC_DBConf routerDbInfo;
        vector<string> vtProxyName;
        string routerServerName;

        if (0 != getRouterDBFromAppTable(appName, routerDbInfo, vtProxyName, routerServerName, errmsg))
        {
            errmsg = string("get router db and router server name failed|errmsg:") + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        if (expandCacheConf(appName, moduleName, vtCacheHost, bReplace, errmsg) != 0)
        {
            errmsg = string("failed to expand cache server conf|errmsg:") + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        if (expandCache2RouterDb(moduleName, routerDbInfo, vtCacheHost, bReplace, errmsg) != 0)
        {
            errmsg = string("failed to insert cache server info to router db table|errmsg:") + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        if (insertCache2TarsDb(routerDbInfo, vtCacheHost, cacheType, sTarsVersion, bReplace, errmsg) != 0)
        {
            errmsg = string("failed to insert cache server info to tars db table|errmsg:") + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        try
        {
            for (size_t i = 0; i < vtCacheHost.size(); ++i)
            {
                for (size_t j = 0; j < vtProxyName.size(); j++)
                {
                    // 保存 proxy server和cache server的对应关系
                    insertProxyCache(vtProxyName[j].substr(7), vtCacheHost[i].serverName, bReplace);
                }

                // 保存cache serve和router server的对应关系
                insertCacheRouter(vtCacheHost[i].serverName.substr(7), vtCacheHost[i].serverIp, int(TC_Common::strto<float>(vtCacheHost[i].shmSize)*1024), routerServerName + ".RouterObj", routerDbInfo, moduleName, bReplace, errmsg);
            }
        }
        catch(exception &ex)
        {
            //这里捕捉异常以不影响安装结果
            errmsg = string("option relation db catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << errmsg << endl);
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("expand cache server catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::insertTransferStatusRecord(const std::string & appName,const std::string & moduleName,const std::string & srcGroupName,const std::string & dstGroupName,bool transferExisted,std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "|app name:" << appName << "|module name:" << moduleName << "|src group name:" << srcGroupName << "|dst group name:" << dstGroupName << endl);
    try
    {
        //往已存在服务迁移
        if (transferExisted)
        {
            string sSql = "select * from t_transfer_status where module_name='" + moduleName
                        + "' and src_group='"   + srcGroupName
                        + "' and app_name='"    + appName
                        + "' and dst_group='"   + dstGroupName + "'";

            TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
            if (data.size() == 0)
            {
                map<string, pair<TC_Mysql::FT, string> > m;
                m["module_name"]            = make_pair(TC_Mysql::DB_STR, moduleName);
                m["app_name"]               = make_pair(TC_Mysql::DB_STR, appName);
                m["src_group"]              = make_pair(TC_Mysql::DB_STR, srcGroupName);
                m["dst_group"]              = make_pair(TC_Mysql::DB_STR, dstGroupName);
                m["status"]                 = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(CONFIG_SERVER));  // 1: 配置阶段完成
                m["transfer_start_time"]    = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));

                _mysqlRelationDB.insertRecord("t_transfer_status", m);
            }
            else if (data[0]["status"] == TC_Common::tostr(TRANSFER_FINISH))
            {
                //status: 4: 迁移完成
                //以前有成功记录，就覆盖
                map<string, pair<TC_Mysql::FT, string> > m;
                m["status"]                 = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(CONFIG_SERVER)); // 1: 配置阶段完成
                m["transfer_start_time"]    = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));

                string cond = "where module_name='" + moduleName
                            + "' and src_group='"   + srcGroupName
                            + "' and app_name='"    + appName
                            + "' and dst_group='"   + dstGroupName + "'";

                _mysqlRelationDB.updateRecord("t_transfer_status", m, cond);
            }

            //改为往数据库中插入相关信息
            TLOGDEBUG(FUN_LOG << "insert transfer record succ for exist dst group|module name:" << moduleName << "|src group name:" << srcGroupName << "|dst group name:" << dstGroupName << endl);

            return 0;
        }
        else
        {
            //往新部署服务迁移
            string sSql = "select * from t_transfer_status where module_name='" + moduleName
                        + "' and src_group='"   + srcGroupName
                        + "' and app_name='"    + appName
                        + "' and dst_group='"   + dstGroupName + "'";

            TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
            if ((data.size() > 0) && (data[0]["status"] != TC_Common::tostr(TRANSFER_FINISH)))
            {
                //此阶段的配置已成功
                if (data[0]["status"] != TC_Common::tostr(NEW_TASK))
                {
                    TLOGDEBUG(FUN_LOG << "configure transfer succ, app name:" << appName << "|module name:" << moduleName << "|src group name:" << srcGroupName << "|dst group name:" << dstGroupName << endl);
                    return 0;
                }
                else if (data[0]["status"] == TC_Common::tostr(NEW_TASK))
                {
                    // 0: 新建任务，如果配置阶段失败，则状态一直是0
                    //上次在此阶段失败，需要清除下，再发起迁移
                    errmsg = "configure transfer failed need clean, app name:" + appName + "|module name:" + moduleName + "|src group name:" + srcGroupName + "|dst group name:" + dstGroupName;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                }
            }
            else
            {
                //记录迁移信息并准备开始迁移
                if (data.size() == 0)
                {
                    map<string, pair<TC_Mysql::FT, string> > m;
                    m["module_name"]            = make_pair(TC_Mysql::DB_STR, moduleName);
                    m["app_name"]               = make_pair(TC_Mysql::DB_STR, appName);
                    m["src_group"]              = make_pair(TC_Mysql::DB_STR, srcGroupName);
                    m["dst_group"]              = make_pair(TC_Mysql::DB_STR, dstGroupName);
                    m["status"]                 = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(NEW_TASK)); // 0: 新建迁移任务
                    m["transfer_start_time"]    = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));

                    _mysqlRelationDB.insertRecord("t_transfer_status", m);
                }
                else
                {
                    //以前有成功记录，就覆盖
                    map<string, pair<TC_Mysql::FT, string> > m;
                    m["status"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(NEW_TASK)); // 0: 新建迁移任务
                    m["transfer_start_time"] = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));

                    string cond = "where module_name='" + moduleName
                                + "' and src_group='"   + srcGroupName
                                + "' and app_name='"    + appName
                                + "' and dst_group='"   + dstGroupName + "'";

                    _mysqlRelationDB.updateRecord("t_transfer_status", m, cond);
                }
            }
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert transfer status record catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::insertExpandReduceStatusRecord(const std::string& appName, const std::string& moduleName, TransferType type, const vector<string> & groupName, std::string& errmsg)
{
    TLOGDEBUG(FUN_LOG << "|app name:" << appName << "|module name:" << moduleName << "|type:" << etos(type) << endl);
    try
    {
        string sSql = "select * from t_expand_status where module_name='" + moduleName + "' and app_name='" + appName + "' and type=" + TC_Common::tostr(type);

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        {
            //记录扩容或者缩容信息并准备开始迁移
            if (data.size() == 0)
            {
                map<string, pair<TC_Mysql::FT, string> > m;
                m["module_name"]        = make_pair(TC_Mysql::DB_STR, moduleName);
                m["app_name"]           = make_pair(TC_Mysql::DB_STR, appName);
                m["status"]             = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(NEW_TASK)); // 0: 新建任务
                m["type"]               = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(type)); // type: 1:expand, 2:reduce
                m["expand_start_time"]  = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
                m["modify_group_name"]  = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(groupName.begin(), groupName.end())); // 多个组名以"|"隔开
                // modify_group_name: 对于扩容是新扩的组名，对于缩容是要缩掉的组名

                _mysqlRelationDB.insertRecord("t_expand_status", m);
            }
            else
            {
                map<string, pair<TC_Mysql::FT, string> > m;
                m["status"]             = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(CONFIG_SERVER)); // 1: 配置阶段完成
                m["type"]               = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(type));
                m["expand_start_time"]  = make_pair(TC_Mysql::DB_STR, TC_Common::now2str("%Y-%m-%d %H:%M:%S"));
                m["modify_group_name"]  = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(groupName.begin(), groupName.end())); // 多个组名以"|"隔开

                string cond = "where module_name='" + moduleName + "' and app_name='" + appName + "' and type=" + TC_Common::tostr(type);

                _mysqlRelationDB.updateRecord("t_expand_status", m, cond);
            }
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert expand or reduce status record catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}


int DCacheOptImp::insertTransferRecord2RouterDb(TC_Mysql &tcMysql, const string &module, const string &srcGroup, const string &destGroup, unsigned int fromPage, unsigned int toPage, string &ids, std::string& errmsg)
{
    try
    {
        TLOGDEBUG(FUN_LOG << "|dstGroup:" << destGroup << "|srcGroup:" << srcGroup << "|fromPage:" << fromPage << "|toPage:" << toPage << endl);
        if ((toPage - fromPage) > 10000)
        {
            for (unsigned int begin = fromPage; begin <= toPage; begin += 10000)
            {
                map<string, pair<TC_Mysql::FT, string> > m;
                m["module_name"]        = make_pair(TC_Mysql::DB_STR, module);
                m["from_page_no"]       = make_pair(TC_Mysql::DB_INT, TC_Common::tostr<unsigned int>(begin));

                unsigned int end = 0;
                if ((begin + 9999) >= toPage)
                {
                    end = toPage;
                }
                else
                {
                    end = begin + 9999;
                }

                m["to_page_no"]         = make_pair(TC_Mysql::DB_INT, TC_Common::tostr<unsigned int>(end));
                m["group_name"]         = make_pair(TC_Mysql::DB_STR, srcGroup);
                m["trans_group_name"]   = make_pair(TC_Mysql::DB_STR, destGroup);
                m["state"]              = make_pair(TC_Mysql::DB_INT, "3"); // 3-设置迁移页

                tcMysql.insertRecord("t_router_transfer", m);

                string sql = string("select * from t_router_transfer where module_name='") + module
                           + string("' and from_page_no=") + TC_Common::tostr<int>(begin)
                           + string(" and to_page_no=") + TC_Common::tostr<int>(end)
                           + string(" and group_name='") + srcGroup
                           + string("' and trans_group_name='") + destGroup
                           + string("' order by id desc limit 1");

                TC_Mysql::MysqlData transferData = tcMysql.queryRecord(sql);

                // 记录 迁移任务的 id
                ids += transferData[0]["id"] + "|";
            }
        }
        else
        {
            map<string, pair<TC_Mysql::FT, string> > m;
            m["module_name"]        = make_pair(TC_Mysql::DB_STR, module);
            m["from_page_no"]       = make_pair(TC_Mysql::DB_INT, TC_Common::tostr<unsigned int>(fromPage));
            m["to_page_no"]         = make_pair(TC_Mysql::DB_INT, TC_Common::tostr<unsigned int>(toPage));
            m["group_name"]         = make_pair(TC_Mysql::DB_STR, srcGroup);
            m["trans_group_name"]   = make_pair(TC_Mysql::DB_STR, destGroup);
            m["state"]              = make_pair(TC_Mysql::DB_INT, "3"); // 3-设置迁移页
            tcMysql.insertRecord("t_router_transfer", m);

            string sql = string("select * from t_router_transfer where module_name='") + module
                       + string("' and from_page_no=") + TC_Common::tostr<int>(fromPage)
                       + string(" and to_page_no=") + TC_Common::tostr<int>(toPage)
                       + string(" and group_name='") + srcGroup
                       + string("' and trans_group_name='") + destGroup
                       + string("' order by id desc limit 1");

            TC_Mysql::MysqlData transferData = tcMysql.queryRecord(sql);

            // 记录 迁移任务的 id
            ids += transferData[0]["id"] + "|";
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert transfer record to router db table t_router_transfer catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::insertTransfer2RouterDb(const std::string & appName,const std::string & moduleName,const std::string & srcGroupName,const std::string & dstGroupName,bool transferData,std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "app:" << appName << "|module:" << moduleName<< "|srcGroup:" << srcGroupName << "|dstGroupName:" << dstGroupName << "|transferData:" << transferData << endl);
    try
    {
        //根据appName查询路由数据库信息
        TC_DBConf routerDbInfo;
        int iRet = getRouterDBInfo(appName, routerDbInfo, errmsg);
        if (iRet == -1)
        {
            errmsg = "get router db info failed|app name:" + appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        string sSql = "select * from t_transfer_status where module_name='" + moduleName
                    + "' and src_group='"   + srcGroupName
                    + "' and dst_group='"   + dstGroupName
                    + "' and app_name='"    + appName + "'";

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            TC_Mysql tcMysql;
            tcMysql.init(routerDbInfo);
            tcMysql.connect(); //连不上时抛出异常,加上此句方便捕捉异常

            string sql = string("select * from t_router_record where module_name='") + moduleName + string("' and group_name='") + srcGroupName + string("'");

            TC_Mysql::MysqlData recordData = tcMysql.queryRecord(sql);
            if (recordData.size() == 0)
            {
                errmsg = "transfer page not find in router record table|moduleName:" + moduleName + "|srcGroupName:" + srcGroupName;
                TLOGERROR(FUN_LOG << errmsg << endl);
                return -1;
            }

            if (transferData)
            {
                // 迁移数据，需要插入迁移记录
                string id_str = "";
                for (size_t i = 0; i < recordData.size(); ++i)
                {
                    insertTransferRecord2RouterDb(tcMysql, recordData[i]["module_name"], recordData[i]["group_name"], data[0]["dst_group"], TC_Common::strto<unsigned int>(recordData[i]["from_page_no"]), TC_Common::strto<unsigned int>(recordData[i]["to_page_no"]), id_str, errmsg);
                }

                map<string, pair<TC_Mysql::FT, string> > m_update;
                m_update["status"]              = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(TRANSFERRING)); // 设置迁移状态为 3: 迁移中
                m_update["router_transfer_id"]  = make_pair(TC_Mysql::DB_STR, id_str);

                string condition = "where id=" + data[0]["id"];

                _mysqlRelationDB.updateRecord("t_transfer_status", m_update, condition);

                vector<string> tmp = TC_Common::sepstr<string>(id_str, "|");
                for (size_t j = 0; j < tmp.size(); ++j)
                {
                    map<string, pair<TC_Mysql::FT, string> > m_update;
                    m_update["state"] = make_pair(TC_Mysql::DB_INT, "0"); // 设置路由迁移任务状态为 0: 未迁移

                    string condition = "where id=" + tmp[j];

                    tcMysql.updateRecord("t_router_transfer", m_update, condition);
                }
            }
            else
            {
                //无数据迁移，直接把路由信息由源组改成目的组
                for (size_t i = 0; i < recordData.size(); i++)
                {
                    map<string, pair<TC_Mysql::FT, string> > m;
                    m["group_name"] = make_pair(TC_Mysql::DB_STR, dstGroupName);

                    tcMysql.updateRecord("t_router_record", m, "where id=" + recordData[i]["id"]);
                }

                //修改模块版本号
                sql = "select * from t_router_module where module_name='" +  moduleName + "'";
                TC_Mysql::MysqlData moduleData = tcMysql.queryRecord(sql);
                if (moduleData.size() == 0)
                {
                    errmsg = "router module info not find in router module table|module name:" + moduleName + "|src group name:" + srcGroupName;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                map<string, pair<TC_Mysql::FT, string> > m;
                m["version"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(TC_Common::strto<int>(moduleData[0]["version"]) + 1));

                tcMysql.updateRecord("t_router_module", m, "where id=" + moduleData[0]["id"]);

                //根据appName查询 router obj
                string routerObj("");
                iRet = getRouterObj(appName, routerObj, errmsg);
                if (iRet != 0)
                {
                    errmsg = "get router obj info failed|appName:" + appName + "|errmsg:" + errmsg;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }

                vector<string> tmp = TC_Common::sepstr<string>(routerObj, ".");
                iRet = reloadRouterByModule("DCache", moduleName, tmp[0] + "." + tmp[1], errmsg);
                if (iRet != 0)
                {
                    errmsg = "reload router by module failed, router server name:" + tmp[1] + "|module name:" + moduleName + "|errmsg:" + errmsg;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }

                // 更新 迁移状态为 迁移完成
                map<string, pair<TC_Mysql::FT, string> > m_update;
                m_update["status"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(TRANSFER_FINISH));

                _mysqlRelationDB.updateRecord("t_transfer_status", m_update, "where id=" + data[0]["id"]);
            }

            return 0;
        }
        else
        {
            errmsg = "no record in transgfer status table|moduleName:" + moduleName + "|srcGroupName:" + srcGroupName + "|dstGroupName:" + dstGroupName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert tarnsfer record catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::insertExpandTransfer2RouterDb(const std::string & appName,const std::string & moduleName,const vector<std::string> & dstGroupNames,std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "app:" << appName << "|module:" << moduleName << "|dstGroupNames:" << TC_Common::tostr(dstGroupNames) << endl);
    try
    {
        //根据appName查询路由数据库信息
        TC_DBConf routerDbInfo;
        int iRet = getRouterDBInfo(appName, routerDbInfo, errmsg);
        if (iRet == -1)
        {
            errmsg = "get router db info failed|app name:" + appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        string sSql = "select * from t_expand_status where module_name='" + moduleName + "' and app_name='" + appName + "' and type=" + TC_Common::tostr(DCache::EXPAND_TYPE);
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            {
                TC_Mysql tcMysql;
                tcMysql.init(routerDbInfo);
                tcMysql.connect(); //连不上时抛出异常,加上此句方便捕捉异常

                sSql = string("select *,to_page_no-from_page_no+1 as num from t_router_record where module_name='") + moduleName + string("' order by num asc");

                TC_Mysql::MysqlData recordData = tcMysql.queryRecord(sSql);
                if (recordData.size() == 0)
                {
                    errmsg = string("transfer page not find in router record table|appName:") + appName + "|moduleName:" + moduleName;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }

                //记录现有组的路由信息
                vector<RouterInfoPtr> groupRouter;

                //记录以开始页为索引的路由信息
                map<int, RouterInfoPtr> fromPageRouter;

                //记录以结束页为索引的路由信息
                map<int, RouterInfoPtr> toPageRouter;

                //记录源组可分配出去的路由页数
                map<string, int> srcGroupPages;

                //用于校验路由是否完整
                int pageCount = 0;

                for (size_t i = 0; i < recordData.size(); i++)
                {
                    RouterInfoPtr tmpR = new RouterInfo;
                    tmpR->fromPage  = TC_Common::strto<unsigned int>(recordData[i]["from_page_no"]);
                    tmpR->toPage    = TC_Common::strto<unsigned int>(recordData[i]["to_page_no"]);
                    tmpR->groupName = recordData[i]["group_name"];
                    tmpR->flags     = true;

                    groupRouter.push_back(tmpR);

                    fromPageRouter[tmpR->fromPage] = tmpR;
                    toPageRouter[tmpR->toPage] = tmpR;

                    srcGroupPages[tmpR->groupName] += TC_Common::strto<int>(recordData[i]["num"]);

                    pageCount += TC_Common::strto<int>(recordData[i]["num"]);
                }

                if (pageCount != TOTAL_ROUTER_PAGE_NUM)
                {
                    errmsg = string("router page is not complete|appName:") + appName + "|moduleName:" + moduleName;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                else
                {
                    TLOGDEBUG(FUN_LOG << "router record group size:" << groupRouter.size() << endl);
                }

                unsigned int totalGroupSize = srcGroupPages.size() + dstGroupNames.size();//扩容后总的服务组数
                unsigned int pageNumPer = TOTAL_ROUTER_PAGE_NUM / totalGroupSize;//扩容后每组拥有的页数

                map<string, int> dstGroupPages;//记录目的组需要分配的剩余路由页数
                for (size_t i = 0; i < dstGroupNames.size(); i++)
                {
                    dstGroupPages[dstGroupNames[i]] = pageNumPer;
                }

                //计算源组可分配的路由页数
                for (map<string, int>::iterator it = srcGroupPages.begin(); it != srcGroupPages.end(); it++)
                {
                    it->second -= pageNumPer;
                }

                string id_str = "";

                for (size_t i = 0; (i < groupRouter.size()) && (dstGroupPages.size() > 0); )
                {
                    if ((groupRouter[i]->flags == false) || (srcGroupPages[groupRouter[i]->groupName] <= 0))
                    {
                        //这个路由已经分配了
                        i++;
                        continue;
                    }

                    map<string, int>::iterator destIt = dstGroupPages.begin();

                    //这个记录的路由页数
                    int tmpNum = groupRouter[i]->toPage - groupRouter[i]->fromPage + 1;

                    //本次可分配的页数
                    int needPageNum = (srcGroupPages[groupRouter[i]->groupName] < destIt->second) ? srcGroupPages[groupRouter[i]->groupName] : destIt->second;

                    int tmpBeginPage = groupRouter[i]->fromPage;
                    int tmpEndPage = 0;

                    if (tmpNum <= needPageNum)
                    {
                        tmpEndPage = groupRouter[i]->toPage;
                    }
                    else
                    {
                        tmpEndPage = tmpBeginPage + needPageNum - 1;

                        if(tmpEndPage > groupRouter[i]->toPage)
                        {
                            tmpEndPage = groupRouter[i]->toPage;
                        }
                    }

                    if (tmpEndPage == groupRouter[i]->toPage)
                    {
                        //这个记录已经分配了，标记为不可用
                        groupRouter[i]->flags = false;
                    }
                    else
                    {
                        groupRouter[i]->fromPage = tmpEndPage + 1;
                    }

                    //插入记录
                    insertTransferRecord2RouterDb(tcMysql, moduleName, groupRouter[i]->groupName, destIt->first, tmpBeginPage, tmpEndPage, id_str, errmsg);

                    srcGroupPages[groupRouter[i]->groupName] -= tmpEndPage - tmpBeginPage + 1;

                    //记录目的组剩余需要迁移的页数
                    destIt->second -= tmpEndPage - tmpBeginPage + 1;
                    if (destIt->second <= 0)
                    {
                        //这个目的组完成了
                        dstGroupPages.erase(destIt);

                        TLOGDEBUG(FUN_LOG << "dst group:" << destIt->first << " done! " << destIt->second << " left dst group size:" << dstGroupPages.size() << endl);
                        continue;
                    }

                    //找下一个连续的块
                    int cBeginPage = tmpBeginPage;
                    int cEndPage = tmpEndPage;
                    while (destIt->second > 0)
                    {
                        map<int, RouterInfoPtr>::iterator fromPageIt = fromPageRouter.find(cEndPage + 1);//接尾的块
                        map<int, RouterInfoPtr>::iterator toPageIt = toPageRouter.find(cBeginPage - 1);//接头的块

                        RouterInfoPtr routerPtr = NULL;

                        bool isfromPage;
                        if (fromPageIt == fromPageRouter.end())
                        {
                            if (toPageIt == toPageRouter.end())
                            {
                                break;
                            }
                            else
                            {
                                routerPtr = toPageIt->second;
                                isfromPage = false;
                            }
                        }
                        else if (toPageIt == toPageRouter.end())
                        {
                            if (fromPageIt == fromPageRouter.end())
                            {
                                break;
                            }
                            else
                            {
                                routerPtr = fromPageIt->second;
                                isfromPage = true;
                            }
                        }
                        else
                        {
                            if ((fromPageIt->second->flags == false) || (fromPageIt->second->fromPage != fromPageIt->first) || (srcGroupPages[fromPageIt->second->groupName] <= 0))
                            {
                                routerPtr = toPageIt->second;
                                isfromPage = false;
                            }
                            else if ((toPageIt->second->flags == false) || (toPageIt->second->toPage != toPageIt->first) || (srcGroupPages[toPageIt->second->groupName] <= 0))
                            {
                                routerPtr = fromPageIt->second;
                                isfromPage = true;
                            }
                            else
                            {
                                //找一个最小的而且是完整的块
                                unsigned int fromPageNum = fromPageIt->second->toPage - fromPageIt->second->fromPage + 1;
                                unsigned int toPageNum = toPageIt->second->toPage - toPageIt->second->fromPage + 1;

                                unsigned int fromAbleNum = (srcGroupPages[fromPageIt->second->groupName] < destIt->second)?srcGroupPages[fromPageIt->second->groupName]:destIt->second;
                                unsigned int toAbleNum = (srcGroupPages[toPageIt->second->groupName] < destIt->second)?srcGroupPages[toPageIt->second->groupName]:destIt->second;

                                if (fromAbleNum < fromPageNum)
                                {
                                    routerPtr = toPageIt->second;
                                    isfromPage = false;
                                }
                                else if (toAbleNum < toPageNum)
                                {
                                    routerPtr = fromPageIt->second;
                                    isfromPage = true;
                                }
                                else
                                {
                                    if (fromPageNum <= toPageNum)
                                    {
                                        routerPtr = fromPageIt->second;
                                        isfromPage = true;
                                    }
                                    else
                                    {
                                        routerPtr = toPageIt->second;
                                        isfromPage = false;
                                    }
                                }
                            }
                        }

                        if ((routerPtr->flags == false) || ((srcGroupPages[routerPtr->groupName] <= 0)))
                        {
                            break;
                        }

                        int tmpBeginPage = 0;
                        int tmpEndPage = 0;
                        if (isfromPage)
                        {
                            if (routerPtr->fromPage != (cEndPage + 1))
                            {
                                TLOGERROR(FUN_LOG << "router page is discontinuous, next begin page:" << routerPtr->fromPage << " is not equal to last end page increase one:" << (cEndPage + 1) << endl);
                                break;//接不上，直接退出
                            }

                            int tmpNum = (srcGroupPages[routerPtr->groupName] < destIt->second)?srcGroupPages[routerPtr->groupName]:destIt->second;

                            tmpBeginPage = routerPtr->fromPage;

                            if (tmpNum < (routerPtr->toPage - routerPtr->fromPage + 1))
                            {
                                tmpEndPage = tmpBeginPage + tmpNum - 1;
                                routerPtr->fromPage = tmpEndPage + 1;

                                if (tmpEndPage > routerPtr->toPage)
                                {
                                    tmpEndPage = routerPtr->toPage;
                                }
                            }
                            else
                            {
                                tmpEndPage = routerPtr->toPage;
                                routerPtr->flags = false;
                            }

                            cEndPage = tmpEndPage;
                        }
                        else
                        {
                            if (routerPtr->toPage != (cBeginPage - 1))
                            {
                                TLOGERROR(FUN_LOG << "router page is discontinuous, last end page:" << routerPtr->toPage << " is not equal to next begin page decrease one:" << (cBeginPage - 1) << endl);
                                break;//接不上，直接退出
                            }

                            int tmpNum = (srcGroupPages[routerPtr->groupName] < destIt->second)?srcGroupPages[routerPtr->groupName]:destIt->second;

                            tmpEndPage = routerPtr->toPage;

                            if (tmpNum < (routerPtr->toPage - routerPtr->fromPage + 1))
                            {
                                tmpBeginPage = tmpEndPage - tmpNum - 1;
                                routerPtr->toPage = tmpBeginPage - 1;

                                if(tmpBeginPage < routerPtr->fromPage)
                                {
                                    tmpBeginPage = routerPtr->fromPage;
                                }
                            }
                            else
                            {
                                tmpBeginPage = routerPtr->fromPage;
                                routerPtr->flags = false;
                            }

                            cBeginPage = tmpBeginPage;
                        }

                        //插入记录
                        insertTransferRecord2RouterDb(tcMysql, moduleName, routerPtr->groupName, destIt->first, tmpBeginPage, tmpEndPage, id_str, errmsg);

                        srcGroupPages[routerPtr->groupName] -= tmpEndPage - tmpBeginPage + 1;

                        //记录目的组剩余需要迁移的页数
                        destIt->second -= tmpEndPage - tmpBeginPage + 1;
                    }

                    if (destIt->second <= 0)
                    {
                        //这个目的组完成了
                        dstGroupPages.erase(destIt);
                        continue;
                    }
                }

                map<string, pair<TC_Mysql::FT, string> > m_update;
                m_update["status"]              = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(TRANSFERRING));
                m_update["router_transfer_id"]  = make_pair(TC_Mysql::DB_STR, id_str);

                string condition = "where id=" + data[0]["id"];

                _mysqlRelationDB.updateRecord("t_expand_status", m_update, condition);

                vector<string> tmp = TC_Common::sepstr<string>(id_str, "|");
                for (size_t j = 0; j < tmp.size(); j++)
                {
                    map<string, pair<TC_Mysql::FT, string> > m_update;
                    m_update["state"] = make_pair(TC_Mysql::DB_INT, "0");

                    string condition = "where id=" + tmp[j];
                    tcMysql.updateRecord("t_router_transfer", m_update, condition);
                }

                return 0;
            }
        }
        else
        {
            errmsg = string("no record in transgfer db expand status table|appName:") + appName + "|moduleName:" + moduleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert expand transfer record catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::insertReduceTransfer2RouterDb(const std::string& appName, const std::string& moduleName, const std::vector<std::string> &srcGroupNames, std::string& errmsg)
{
    TLOGDEBUG(FUN_LOG << "app:" << appName << "|module:" << moduleName << "|srcGroupNames:" << TC_Common::tostr(srcGroupNames) << endl);

    std::vector<std::string> vtSrcGroupNames = srcGroupNames;
    size_t oldSize = vtSrcGroupNames.size();

    //先对参数排重
    std::sort(vtSrcGroupNames.begin(), vtSrcGroupNames.end());
    vtSrcGroupNames.erase(std::unique(vtSrcGroupNames.begin(), vtSrcGroupNames.end()), vtSrcGroupNames.end());

    if (vtSrcGroupNames.size() != oldSize)
    {
        errmsg = string("src group name is not unique|app:") + appName + "|module:" + moduleName;
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    try
    {
        //根据appName查询路由数据库信息
        TC_DBConf routerDbInfo;
        int iRet = getRouterDBInfo(appName, routerDbInfo, errmsg);
        if (iRet == -1)
        {
            errmsg = "get router db info failed|app name:" + appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        string sSql = "select * from t_expand_status where module_name='" + moduleName + "' and app_name='" + appName + "' and type=" + TC_Common::tostr(DCache::REDUCE_TYPE);
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            {
                string ids;

                TC_Mysql tcMysql;
                tcMysql.init(routerDbInfo);
                tcMysql.connect(); //连不上时抛出异常,加上此句方便捕捉异常

                string sql = string("select *,to_page_no-from_page_no+1 as num from t_router_record where module_name='") + moduleName + string("' order by num asc");
                TC_Mysql::MysqlData recordData = tcMysql.queryRecord(sql);
                if (recordData.size() == 0)
                {
                    errmsg = string("transfer page not find in router record table|appName:") + appName + "|moduleName:" + moduleName;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }

                //记录现有组的路由信息
                vector<RouterInfoPtr> groupRouter;

                //记录以开始页为索引的路由信息
                map<int, RouterInfoPtr> fromPageRouter;

                //记录以结束页为索引的路由信息
                map<int, RouterInfoPtr> toPageRouter;

                //记录原有组的路由页数
                map<string, int> dstGroupPages; //原所有组的全部页数

                //用于校验路由是否完整
                int pageCount = 0;

                for (size_t i = 0; i < recordData.size(); i++)
                {
                    RouterInfoPtr tmpR = new RouterInfo;
                    tmpR->fromPage  = TC_Common::strto<unsigned int>(recordData[i]["from_page_no"]);
                    tmpR->toPage    = TC_Common::strto<unsigned int>(recordData[i]["to_page_no"]);
                    tmpR->groupName = recordData[i]["group_name"];
                    tmpR->flags     = true;

                    groupRouter.push_back(tmpR);

                    fromPageRouter[tmpR->fromPage]  = tmpR;
                    toPageRouter[tmpR->toPage]      = tmpR;

                    dstGroupPages[tmpR->groupName] += TC_Common::strto<int>(recordData[i]["num"]);

                    pageCount += TC_Common::strto<int>(recordData[i]["num"]);
                }

                if (pageCount != TOTAL_ROUTER_PAGE_NUM)
                {
                    errmsg = string("router page is not complete|appName:") + appName + "|moduleName:" + moduleName;
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }
                else
                {
                    TLOGDEBUG(FUN_LOG << "router record size:" << groupRouter.size() << endl);
                }

                //把路由页数为0的去掉
                map<string, int>::iterator dstGroupPagesIt = dstGroupPages.begin();
                for (; dstGroupPagesIt != dstGroupPages.end(); )
                {
                    if (dstGroupPagesIt->second == 0)
                    {
                        dstGroupPages.erase(dstGroupPagesIt++);
                    }
                    else
                    {
                        dstGroupPagesIt++;
                    }
                }

                //校验传入迁移源组是否存在
                for (size_t i = 0; i < vtSrcGroupNames.size(); i++)
                {
                    if (dstGroupPages.find(vtSrcGroupNames[i]) == dstGroupPages.end())
                    {
                        errmsg = "src group name not existed:" + vtSrcGroupNames[i];
                        TLOGERROR(FUN_LOG << errmsg << endl);
                        return -1;
                    }
                }

                if (dstGroupPages.size() <= vtSrcGroupNames.size())
                {
                    errmsg = "src group num exceed the total group count";
                    TLOGERROR(FUN_LOG << errmsg << endl);
                    return -1;
                }

                // vtSrcGroupNames 记录的是需要缩容的组名
                unsigned int totalGroupSize = dstGroupPages.size() - vtSrcGroupNames.size();//缩容后总的服务组数
                unsigned int pageNumPer = TOTAL_ROUTER_PAGE_NUM / totalGroupSize + 1;//缩容后每组拥有的页数


                map<string, int> srcGroupPages;//记录源组需要迁移走的剩余路由页数
                for (size_t i = 0; i < vtSrcGroupNames.size(); i++)
                {
                    srcGroupPages[vtSrcGroupNames[i]] = dstGroupPages[vtSrcGroupNames[i]];
                    dstGroupPages.erase(vtSrcGroupNames[i]);
                }

                // 需要把源组的路由页全部迁走 srcGroupPages

                map<string, int>::iterator it = dstGroupPages.begin();
                for (; it != dstGroupPages.end(); ++it)
                {
                    it->second = pageNumPer - it->second;
                }

                //开始做迁移逻辑
                for (size_t i = 0; i < recordData.size(); ++i)
                {
                    string groupName = recordData[i]["group_name"];
                    unsigned int toPage     = TC_Common::strto<unsigned int >(recordData[i]["to_page_no"]);
                    unsigned int fromPage   = TC_Common::strto<unsigned int >(recordData[i]["from_page_no"]);

                    //检查是否是目的组
                    map<string, int>::iterator groupIt = dstGroupPages.find(groupName);
                    if (groupIt == dstGroupPages.end())
                    {
                        continue;
                    }
                    else
                    {
                        groupRouter[i]->flags = false;

                        if (groupIt->second <= 0)
                        {
                            continue;
                        }
                    }

                    while (true)
                    {
                        //先查找路由范围后面的能不能迁移
                        map<int, RouterInfoPtr>::iterator it = fromPageRouter.find(toPage + 1);
                        if (it != fromPageRouter.end() && it->second->flags == true)
                        {
                            map<string, int>::iterator srcIt = srcGroupPages.find(it->second->groupName);
                            if (srcIt != srcGroupPages.end())
                            {

                                int num = it->second->toPage - it->second->fromPage + 1; // 源组上的一个路由页范围

                                if (dstGroupPages[groupName] > 0)
                                {
                                    if (dstGroupPages[groupName] >= num)
                                    {
                                        toPage = it->second->toPage;

                                        srcIt->second -= num;
                                        dstGroupPages[groupName] -= num;

                                        //插入记录
                                        insertTransferRecord2RouterDb(tcMysql, moduleName, srcIt->first, groupName, it->second->fromPage, it->second->toPage, ids, errmsg);

                                        it->second->flags = false;

                                        continue;
                                    }
                                    else
                                    {
                                        toPage = it->second->fromPage+dstGroupPages[groupName]-1;

                                        //插入记录
                                        insertTransferRecord2RouterDb(tcMysql, moduleName, srcIt->first, groupName, it->second->fromPage, toPage, ids, errmsg);

                                        it->second->fromPage = toPage + 1;

                                        srcIt->second -= dstGroupPages[groupName];
                                        dstGroupPages[groupName] = 0;

                                        break;
                                    }
                                }
                            }
                        }

                        //再找路由范围前的能不能迁移
                        if (fromPage == 0)
                        {
                            break;
                        }

                        it = toPageRouter.find(fromPage - 1);
                        if (it != toPageRouter.end() && it->second->flags == true)
                        {
                            map<string, int>::iterator srcIt = srcGroupPages.find(it->second->groupName);
                            if (srcIt != srcGroupPages.end())
                            {
                                //找到可以迁移的
                                int num = it->second->toPage - it->second->fromPage + 1;

                                if (dstGroupPages[groupName] > 0)
                                {
                                    if (dstGroupPages[groupName] >= num)
                                    {
                                        fromPage = it->second->fromPage;

                                        srcIt->second -= num;
                                        dstGroupPages[groupName] -= num;

                                        //插入记录
                                        insertTransferRecord2RouterDb(tcMysql, moduleName, srcIt->first, groupName, it->second->fromPage, it->second->toPage, ids, errmsg);

                                        it->second->flags = false;

                                        continue;
                                    }
                                    else
                                    {
                                        fromPage = it->second->toPage - dstGroupPages[groupName] + 1;

                                        //插入记录
                                        insertTransferRecord2RouterDb(tcMysql, moduleName, srcIt->first, groupName, fromPage, it->second->toPage, ids, errmsg);

                                        it->second->toPage = fromPage - 1;

                                        srcIt->second -= dstGroupPages[groupName];
                                        dstGroupPages[groupName] = 0;

                                        break;
                                    }
                                }
                            }
                        }

                        break;
                    }
                }

                //到这里迁移能路由范围连续的都已经做了，这里再处理剩下的
                for (size_t i = 0; i < groupRouter.size(); i++)
                {
                    if (groupRouter[i]->flags == true)
                    {
                        map<string, int>::iterator srcIt = srcGroupPages.find(groupRouter[i]->groupName);
                        if (srcIt != srcGroupPages.end())
                        {
                            unsigned int fromPage   = groupRouter[i]->fromPage;
                            unsigned int toPage     = groupRouter[i]->toPage;

                            int num = toPage - fromPage + 1;

                            map<string, int>::iterator destIt = dstGroupPages.begin();
                            for (; destIt != dstGroupPages.end(); destIt++)
                            {
                                if (destIt->second > 0)
                                {
                                    if (destIt->second > num)
                                    {
                                        groupRouter[i]->flags = false;
                                        destIt->second -= num;

                                        //插入记录
                                        insertTransferRecord2RouterDb(tcMysql, moduleName, srcIt->first, destIt->first, fromPage,toPage, ids, errmsg);

                                        break;
                                    }
                                    else
                                    {
                                        //插入记录
                                        insertTransferRecord2RouterDb(tcMysql, moduleName, srcIt->first, destIt->first, fromPage, fromPage + destIt->second -1, ids, errmsg);

                                        num -= destIt->second;
                                        fromPage = fromPage + destIt->second;
                                        destIt->second = 0;

                                        continue;
                                    }
                                }
                            }
                        }
                    }
                }

                map<string, pair<TC_Mysql::FT, string> > m_update;
                m_update["status"]              = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(TRANSFERRING));
                m_update["router_transfer_id"]  = make_pair(TC_Mysql::DB_STR, ids);

                string condition = "where id=" + data[0]["id"];

                _mysqlRelationDB.updateRecord("t_expand_status", m_update, condition);
                TLOGDEBUG("upadte t_expand_status sql:" << _mysqlRelationDB.getLastSQL() << endl);

                vector<string> tmp = TC_Common::sepstr<string>(ids, "|");
                for (size_t j = 0; j < tmp.size(); j++)
                {
                    map<string, pair<TC_Mysql::FT, string> > m_update;
                    m_update["state"] = make_pair(TC_Mysql::DB_INT, "0");

                    string condition = "where id=" + tmp[j];
                    tcMysql.updateRecord("t_router_transfer", m_update, condition);
                }

                return 0;
            }
        }
        else
        {
            errmsg = string("no record in transgfer db expand status|appName:") + appName + "|moduleName:" + moduleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch (const std::exception &ex)
    {
        errmsg = string("insert reduce transfer record catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::getCacheConfigFromDB(const string &serverName, const string &appName, const string &moduleName, const string &groupName, string &sEnableErase, string &sDBFlag, string &sMemSize, string & errmsg)
{
    try
    {
        string sSql = "select id from t_config_item where item='EnableErase' and path='Main/Cache'";
        TC_Mysql::MysqlData itemData = _mysqlRelationDB.queryRecord(sSql);
        if (itemData.size() != 1)
        {
            errmsg = "cache config item wrong, item='EnableErase' and path='Main/Cache' not existed";
            return -1;
        }

        string enableEraseId = itemData[0]["id"];

        sSql = "select id from t_config_item where item='DBFlag' and path='Main/DbAccess'";
        itemData = _mysqlRelationDB.queryRecord(sSql);
        if (itemData.size() != 1)
        {
            errmsg = "cache config item wrong, item='DBFlag' and path='Main/DbAccess' not existed";
            return -1;
        }

        string dbFlagId = itemData[0]["id"];

        string referenceID;
        sSql = "select reference_id from t_config_reference where server_name='" + serverName + "'";

        TC_Mysql::MysqlData referData = _mysqlRelationDB.queryRecord(sSql);

        for (size_t i = 0; i < referData.size(); ++i)
        {
            referenceID = referData[i]["reference_id"];

            if (referenceID != "")
            {
                sSql = "select item_id,config_value from t_config_table where config_id='" + referenceID + "' and (item_id=" + enableEraseId + " or item_id=" + dbFlagId + ")";

                TC_Mysql::MysqlData configData = _mysqlRelationDB.queryRecord(sSql);

                for (size_t j = 0; j < configData.size(); ++j)
                {
                    if (configData[j]["item_id"] == enableEraseId)
                    {
                        sEnableErase = configData[j]["config_value"];
                    }
                    else if (configData[j]["item_id"] == dbFlagId)
                    {
                        sDBFlag= configData[j]["config_value"];
                    }
                    else
                    {
                        continue;
                    }

                    if (sEnableErase != "" && sDBFlag != "")
                    {
                        break;
                    }
                }
            }
        }

        sSql = "select mem_size from t_cache_router where app_name='" + appName + "' and module_name='" + moduleName + "' and group_name='" + groupName + "'";

        TC_Mysql::MysqlData memData = _mysqlRelationDB.queryRecord(sSql);
        if (memData.size() > 0)
        {
            sMemSize = memData[0]["mem_size"];
        }

        return 0;
    }
    catch (exception &ex)
    {
        errmsg = string("get cache server config from relation db catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch (...)
    {
        errmsg = "get cache server config from relation db catch unknown exception";
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::stopTransferForTransfer(const std::string & appName,const std::string & moduleName,const std::string & srcGroupName,const std::string & dstGroupName,std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "app name:" << appName << "|module name:" << moduleName << "|src group name:" << srcGroupName << "|dst group name:" << dstGroupName << endl);

    try
    {
        //根据appName查询路由数据库信息
        TC_DBConf routerDbInfo;
        int iRet = getRouterDBInfo(appName, routerDbInfo, errmsg);
        if (iRet == -1)
        {
            errmsg = "get router db info failed|app name:" + appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        string sSql = "select * from t_transfer_status where module_name='" + moduleName + "' and src_group='" + srcGroupName + "' and dst_group='" + dstGroupName + "' and app_name='" + appName + "'";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            TransferStatus status = (TransferStatus)TC_Common::strto<int>(data[0]["status"]);
            if (status != TRANSFERRING)
            {
                errmsg = "transfer status is not transferring, can not stop transfer|app name:" + appName + "|module name:" + moduleName + "|srcGroupName:" + srcGroupName;
                TLOGERROR(FUN_LOG << errmsg << endl);
            }
            else
            {
                {
                    TC_ThreadLock::Lock lock(TransferThread::_lock);
                    TC_Mysql tcMysql;
                    tcMysql.init(routerDbInfo);
                    tcMysql.connect();

                    vector<string> transferId = TC_Common::sepstr<string>(data[0]["router_transfer_id"], "|");
                    for (size_t j = 0; j < transferId.size(); ++j)
                    {
                        map<string, pair<TC_Mysql::FT, string> > m;
                        m["state"] = make_pair(TC_Mysql::DB_INT, "3"); // stop

                        tcMysql.updateRecord("t_router_transfer", m, "where id = " + transferId[j]);
                    }

                    map<string, pair<TC_Mysql::FT, string> > m;
                    m["status"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(TRANSFER_STOP));

                    _mysqlRelationDB.updateRecord("t_transfer_status", m, "where id = " + data[0]["id"]);

                    return 0;
                }
            }
        }
        else
        {
            errmsg = "there is no transfer record for app name:" + appName + "|module name:" + moduleName + "|src group name:" + srcGroupName + "|dst group name:" + dstGroupName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch (const std::exception &ex)
    {
        errmsg = string("stop transfer for transfer catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::stopTransferForExpandReduce(const std::string & appName,const std::string & moduleName,TransferType type,std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "app name:" << appName << "|module name:" << moduleName << "|type:" << etos(type) << endl);
    try
    {
        //根据appName查询路由数据库信息
        TC_DBConf routerDbInfo;
        int iRet = getRouterDBInfo(appName, routerDbInfo, errmsg);
        if (iRet == -1)
        {
            errmsg = "get router db info failed|app name:" + appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        string sSql = "select * from t_expand_status where module_name='" + moduleName + "' and app_name='" + appName + "' and type=" + TC_Common::tostr(type);
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            TransferStatus status = (TransferStatus)TC_Common::strto<int>(data[0]["status"]);
            if (status != TRANSFERRING)
            {
                errmsg = "expand status is not transferring, can not stop transfer|app name:" + appName + "|module name:" + moduleName;
                TLOGERROR(FUN_LOG << errmsg << endl);
            }
            else
            {
                {
                    TC_ThreadLock::Lock lock(ExpandThread::_lock);
                    TC_Mysql tcMysql;
                    tcMysql.init(routerDbInfo);
                    tcMysql.connect();

                    vector<string> transferId = TC_Common::sepstr<string>(data[0]["router_transfer_id"], "|");
                    for (size_t j = 0; j < transferId.size(); j++)
                    {
                        map<string, pair<TC_Mysql::FT, string> > m;
                        m["state"] = make_pair(TC_Mysql::DB_INT, "3"); // 3-stop

                        tcMysql.updateRecord("t_router_transfer", m, "where id = " + transferId[j]);
                    }

                    int count = 3;
                    while (count-- > 0)
                    {
                        sleep(1);

                        //再把迁移状态读出来
                        string sql = "select * from t_router_transfer where id in(";
                        for (size_t j = 0; j < transferId.size(); j++)
                        {
                            if (j == 0)
                            {
                                sql += transferId[j];
                            }
                            else
                            {
                                sql += string(",") + transferId[j];
                            }
                        }
                        sql += ")";

                        bool bRunning = false;
                        TC_Mysql::MysqlData statusData = tcMysql.queryRecord(sql);

                        for (size_t j = 0; j < statusData.size(); j++)
                        {
                            if (statusData[j]["state"] == "1")
                            {
                                bRunning = true;
                                break;
                            }
                        }

                        if (bRunning)
                        {
                            //还在跑就再停止一次
                            for (size_t j = 0; j < transferId.size(); j++)
                            {
                                map<string, pair<TC_Mysql::FT, string> > m;
                                m["state"] = make_pair(TC_Mysql::DB_INT, "3");

                                tcMysql.updateRecord("t_router_transfer", m, "where id = " + transferId[j] + " and state = 1");
                            }

                            continue;
                        }
                        else
                        {
                            break;
                        }
                    }

                    map<string, pair<TC_Mysql::FT, string> > m;
                    m["status"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(TRANSFER_STOP));

                    _mysqlRelationDB.updateRecord("t_expand_status", m, "where id = " + data[0]["id"]);

                    return 0;
                }
            }
        }
        else
        {
            errmsg = "there is no expand record for app name:" + appName + "|module name:" + moduleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("stop transfer for expand catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::restartTransferForExpandReduce(const std::string & appName,const std::string & moduleName,TransferType type,std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "app name:" << appName << "|module name:" << moduleName << "|type:" << etos(type) << endl);
    try
    {
        //根据appName查询路由数据库信息
        TC_DBConf routerDbInfo;
        int iRet = getRouterDBInfo(appName, routerDbInfo, errmsg);
        if (iRet == -1)
        {
            errmsg = "get router db info failed|app name:" + appName + "|errmsg:" + errmsg;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        string sSql = "select * from t_expand_status where module_name='" + moduleName + "' and app_name='" + appName + "' and type=" + TC_Common::tostr(type);
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sSql);
        if (data.size() > 0)
        {
            TransferStatus status = (TransferStatus)TC_Common::strto<int>(data[0]["status"]);
            if (status != TRANSFER_STOP)
            {
                errmsg = "expand status is not stop, can not restart transfer|app name:" + appName + "|module name:" + moduleName;
                TLOGERROR(FUN_LOG << errmsg << endl);
            }
            else
            {
                {
                    TC_ThreadLock::Lock lock(ExpandThread::_lock);
                    TC_Mysql tcMysql;
                    tcMysql.init(routerDbInfo);
                    tcMysql.connect();

                    vector<string> transferId = TC_Common::sepstr<string>(data[0]["router_transfer_id"], "|");
                    for (size_t j = 0; j < transferId.size(); j++)
                    {
                        string sSql = "select * from t_router_transfer where id=" + transferId[j];

                        TC_Mysql::MysqlData transferData = tcMysql.queryRecord(sSql);
                        if (transferData.size() > 0)
                        {
                            if (transferData[0]["remark"] != "e_Succ" && transferData[0]["state"] != "1")
                            {
                                map<string, pair<TC_Mysql::FT, string> > m_update;
                                m_update["state"]   = make_pair(TC_Mysql::DB_INT, "0"); // 重新设置状态为未迁移
                                m_update["remark"]  = make_pair(TC_Mysql::DB_STR, "");

                                if (TC_Common::strto<int>(transferData[0]["transfered_page_no"]) != 0)
                                {
                                    m_update["from_page_no"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr<int>(TC_Common::strto<int>(transferData[0]["transfered_page_no"]) + 1));
                                }

                                string condition = "where id=" + transferId[j];
                                tcMysql.updateRecord("t_router_transfer", m_update, condition);
                            }
                        }
                        else
                        {
                            errmsg = "transfer record or expand not existed, transfer id:" + transferId[j];
                            TLOGERROR(FUN_LOG << errmsg << endl);
                        }
                    }

                    map<string, pair<TC_Mysql::FT, string> > m;
                    m["status"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(TRANSFERRING)); // 重启迁移后，设置状态为 3-迁移中

                    _mysqlRelationDB.updateRecord("t_expand_status", m, "where id=" + data[0]["id"]);

                    return 0;
                }
            }
        }
        else
        {
            errmsg = "there is no expand record for app name:" + appName + "|module name:" + moduleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
        }
    }
    catch(const std::exception &ex)
    {
        errmsg = string("restart transfer for expand or reduce catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::deleteTransferForTransfer(const std::string & appName,const std::string & moduleName,const std::string & srcGroupName,const std::string & dstGroupName,std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "app name:" << appName << "|module name:" << moduleName << "|src group name:" << srcGroupName << "|dst group name:" << dstGroupName << endl);

    try
    {
        string sql = "select * from t_transfer_status where app_name='" + appName + "' and module_name='" + moduleName + "' and src_group='" + srcGroupName + "' and dst_group='" + dstGroupName + "'";

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sql);
        if (data.size() == 0)
        {
            errmsg = "not find transfer record|app name:" + appName + "|module name:" + moduleName + "|src group name:" + srcGroupName + "dst group nane:" + dstGroupName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        for (size_t i = 0; i < data.size(); i++)
        {
            //正在迁移，不应该删除
            if (data[i]["status"] == TC_Common::tostr(TRANSFERRING))
            {
                errmsg = "current transfer record is transfering, can not delete|app name:" + data[i]["app_name"]
                       + "|module name:" + data[i]["module_name"]
                       + "|src group name:" + data[i]["src_group"]
                       + "|dst group name:" + data[i]["dst_group"];
                TLOGERROR(FUN_LOG << errmsg << endl);

                return -1;
            }

            /*string cond = "where app_name='" + data[i]["app_name"]
                        + "' and module_name='" + data[i]["module_name"]
                        + "' and src_group='" + data[i]["src_group"]
                        + "' and dst_group='" + data[i]["dst_group"] + "'";*/

            string cond = "where id=" + data[i]["id"];

            _mysqlRelationDB.deleteRecord("t_transfer_status", cond);
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("stop transfer record for transfer catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

int DCacheOptImp::deleteTransferForExpandReduce(const std::string & appName,const std::string & moduleName,TransferType type,std::string &errmsg)
{
    TLOGDEBUG(FUN_LOG << "app name:" << appName << "|module name:" << moduleName << endl);
    try
    {
        string sql = "select * from t_expand_status where app_name='" + appName + "' and module_name='" + moduleName + "' and type=" + TC_Common::tostr(type);

        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sql);
        if (data.size() == 0)
        {
            errmsg = "not find expand or reduce transfer record|app name:" + appName + "|module name:" + moduleName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        for (size_t i = 0; i < data.size(); i++)
        {
            //正在迁移，不应该删除
            if (data[i]["status"] == TC_Common::tostr(TRANSFERRING))
            {
                errmsg = "current expand or reduce transfer record is transfering, can not delete|app name:" + data[i]["app_name"]
                       + "|module name:" + data[i]["module_name"];
                TLOGERROR(FUN_LOG << errmsg << endl);

                return -1;
            }

            // string cond = "where app_name='" + data[i]["app_name"] + "' and module_name='" + data[i]["module_name"] + "'";
            string cond = "where id=" + data[i]["id"];

            _mysqlRelationDB.deleteRecord("t_expand_status", cond);
        }

        return 0;
    }
    catch(const std::exception &ex)
    {
        errmsg = string("delete transfer record for expand or reduce catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}

/*
* 按组获取路由页页数
*/
int DCacheOptImp::getCacheGroupRouterPageNo(TC_Mysql &tcMysql, const string& groupName, long& groupPageNo, string& errmsg)
{
    try
    {
        string sql = "select sum(to_page_no-from_page_no+1) as router_page_no from t_router_record where group_name='" + groupName + "';";

        TC_Mysql::MysqlData groupPageInfo = tcMysql.queryRecord(sql);

        if (groupPageInfo.size() <= 0)
        {
            errmsg = "can not find group router page info|group name:" + groupName;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        groupPageNo = 0;

        if (groupPageInfo[0]["router_page_no"].size() > 0)
        {
            groupPageNo = TC_Common::strto<long>(groupPageInfo[0]["router_page_no"]);
        }


        tcMysql.disconnect();

        return 0;
    }
    catch (const std::exception &ex)
    {
        errmsg = string("get cache group router page number catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}


int DCacheOptImp::loadCacheApp(vector<CacheApp> &cacheApp, tars::CurrentPtr current)
{
    try
    {
        map<string, CacheApp> mCacheApp;
        string sql = "select router_name, app_name from t_router_app";
        TC_Mysql::MysqlData data = _mysqlRelationDB.queryRecord(sql);
        for(size_t i = 0; i < data.size(); i++ )
        {
            mCacheApp[data[i]["app_name"]].name = data[i]["app_name"];
            mCacheApp[data[i]["app_name"]].routerName = data[i]["router_name"].substr(7);
        }

        sql = "select proxy_name, app_name from t_proxy_app";
        data = _mysqlRelationDB.queryRecord(sql);
        for(size_t i = 0; i < data.size(); i++ )
        {
            mCacheApp[data[i]["app_name"]].name = data[i]["app_name"];
            mCacheApp[data[i]["app_name"]].proxyName = data[i]["proxy_name"].substr(7);
        }

        sql = "select cache_name, router_name, module_name, app_name, group_name from t_cache_router";

        data = _mysqlRelationDB.queryRecord(sql);
        for(size_t i = 0; i < data.size(); i++ )
        {
            mCacheApp[data[i]["app_name"]].name = data[i]["app_name"];
            mCacheApp[data[i]["app_name"]].cacheModules[data[i]["module_name"]].moduleName = data[i]["module_name"];

            string cacheNode = data[i]["cache_name"];
            if(cacheNode.find("-") != string::npos)
            {
                cacheNode = cacheNode.substr(0, cacheNode.find("-"));
            }
            else if(cacheNode.find("ServerBak") != string::npos)
            {
                cacheNode = TC_Common::replace(cacheNode, "ServerBak", "Server"); 
            }

            mCacheApp[data[i]["app_name"]].cacheModules[data[i]["module_name"]].cacheNode = cacheNode;
            mCacheApp[data[i]["app_name"]].cacheModules[data[i]["module_name"]].cacheServer.push_back(data[i]["cache_name"]);
        }

        //如果表不存在, 则创建之, 兼容老版本
		sql = "select * from information_schema.tables where table_schema ='" + _relationDBInfo["dbname"] + "' and table_name = 't_dbaccess_app' limit 1";
		data = _mysqlRelationDB.queryRecord(sql);
		if(data.size() == 0)
		{
            _mysqlRelationDB.execute(CREATE_DBACCESS_CONF);
        }

        sql = "select dbaccess_name, app_name from t_dbaccess_app";

        data = _mysqlRelationDB.queryRecord(sql);
        for(size_t i = 0; i < data.size(); i++ )
        {
            string dbAccessServer = data[i]["dbaccess_name"].substr(7);
            string moduleName = dbAccessServer.substr(0, dbAccessServer.size() - string("DbAccessServer").size());

            mCacheApp[data[i]["app_name"]].cacheModules[moduleName].dbAccessServer = dbAccessServer;
        }

        for(auto e : mCacheApp)
        {
            cacheApp.push_back(e.second); 
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        string errmsg = string("get cache app catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    return -1;
}
