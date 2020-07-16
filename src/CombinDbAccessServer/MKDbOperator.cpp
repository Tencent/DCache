#include "MKDbOperator.h"
#include "servant/taf_logger.h"

string MKDbOperator::buildConditionSQL(const vector<DbCondition> &vtCond, TC_Mysql *pMysql)
{
	string sWhere;
	if(vtCond.size() == 0)
	{
		return sWhere;
	}
	sWhere += " where ";
	for(size_t i = 0; i < vtCond.size(); i ++)
	{
		sWhere += "`" + vtCond[i].fieldName + "`" + OP2STR(vtCond[i].op);
		if(vtCond[i].type == INT)
		{
			sWhere += pMysql->escapeString(vtCond[i].value);
		}
		else
		{
			sWhere += "'" + pMysql->escapeString(vtCond[i].value) + "'";
		}
		if(i != vtCond.size() - 1)
		{
			sWhere += " and ";
		}
	}
	return sWhere;
}

string MKDbOperator::buildUpdateSQL(const map<string, DbUpdateValue> &mpValue, TC_Mysql *pMysql)
{
	string sUpdate;
	if(mpValue.size() == 0)
	{
		return sUpdate;
	}

	map<string, DbUpdateValue>::const_iterator it = mpValue.begin();
	while(it != mpValue.end())
	{
		if(!sUpdate.empty())
		{
			sUpdate += ", ";
		}
		sUpdate += "`" + it->first + "`=";
		if(it->second.op == ADD)
		{
			// 自增
			if(it->second.type == INT)
			{
				sUpdate += "`" + it->first + "`+" + pMysql->escapeString(it->second.value);
			}
			else
			{
				sUpdate += "`" + it->first + "`+'" + pMysql->escapeString(it->second.value) + "'";
			}
		}
		else if(it->second.op == SUB)
		{
			// 自减
			if(it->second.type == INT)
			{
				sUpdate += "`" + it->first + "`-" + pMysql->escapeString(it->second.value);
			}
			else
			{
				sUpdate += "`" + it->first + "`-'" + pMysql->escapeString(it->second.value) + "'";
			}
		}
		else
		{
			// SET
			if(it->second.type == INT)
			{
				sUpdate += pMysql->escapeString(it->second.value);
			}
			else
			{
				sUpdate += "'" + pMysql->escapeString(it->second.value) + "'";
			}
		}
		it ++;
	}

	return sUpdate;
}

string MKDbOperator::OP2STR(Op op)
{
	switch(op)
	{
	case EQ:
		return "=";
	case NE:
		return "!=";
	case GT:
		return ">";
	case LT:
		return "<";
	case LE:
		return "<=";
	case GE:
		return ">=";
	default:
		throw runtime_error("invalid op: " + etos(op));
		break;
	}
}

TC_Mysql::FT MKDbOperator::DT2FT(DataType type)
{
	switch(type)
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

TC_Mysql* MKDbOperator::getDb(string &sDbName, string &sTableName, string &mysqlNum,const string &mainKey, const string &op, string &result)
{
	TC_Mysql* mysql = NULL;
	try
	{
		mysql = _dbf(mainKey, op, sDbName,mysqlNum);
		if(mysql == NULL)
		{
			LOG->error() << "mysql conn is NULL or forbid" << endl;
			return NULL;
		}
		mysql->setTimeoutRetry(true);
		sTableName = _tablef(mainKey);
	}
	catch(std::exception &ex)
	{
		string strTmp = ex.what();
		size_t iFound = strTmp.find("sPostfix is not digit");
		if (iFound!=std::string::npos)
		{
			if(g_bTypeErrorRepair)
			       result = "repair";
		}
		LOG->error() << ex.what() << endl;
		return NULL;
	}

	return mysql;
}

taf::Int32 MKDbOperator::select(const string &mainKey, const string &field, const vector<DbCondition> &vtCond, vector<map<string, string> > &vtData)
{
	int iRet = eDbUnknownError;
	string sTableName;
	string sDbName;
	string mysqlNum;
	int64_t beginTime,endTime;
	string optDBTpye;
	string strResult;
	TC_Mysql* mysql = getDb(sDbName, sTableName, mysqlNum, mainKey, "r", strResult);
	if(mysql == NULL)
	{	
		return eDbTableNameError;
	}
	mysql->setTimeoutRetry(true);

	try
	{
		string sSql = "select ";
		vector<string> vtFields = TC_Common::sepstr<string>(field, ",", false);
		if(vtFields.size() == 0 || (vtFields.size() == 1 && vtFields[0] == "*"))
		{
			// 查询所有字段
			sSql += "*";
		}
		else
		{
			for(size_t i = 0; i < vtFields.size(); i ++)
			{
				sSql += "`" + vtFields[i] + "`";
				if(i != vtFields.size() - 1)
				{
					sSql += ", ";
				}
			}
		}
        if(!_bOrder)
            sSql += " from " + sDbName + "." + sTableName + buildConditionSQL(vtCond, mysql) + " limit " +TC_Common::tostr(_iSelectLimit);
        else if(_basc)
            sSql += " from " + sDbName + "." + sTableName + buildConditionSQL(vtCond, mysql) + " order by " + _orderItem + " asc limit " +TC_Common::tostr(_iSelectLimit);
        else
            sSql += " from " + sDbName + "." + sTableName + buildConditionSQL(vtCond, mysql) + " order by " + _orderItem + " desc limit " +TC_Common::tostr(_iSelectLimit);

        LOG->debug() << sSql << endl;

		beginTime = TC_TimeProvider::getInstance()->getNowMs();
		optDBTpye = "select";

		TC_Mysql::MysqlData recordSet = mysql->queryRecord(sSql);

		endTime = TC_TimeProvider::getInstance()->getNowMs();
		Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
		mysqlErrCounter[mysqlNum]->finishInvoke(false);
	
		size_t iRecordSize = recordSet.size();
		if(iRecordSize == 0)
		{
			return 0;
		}
		if(iRecordSize>g_mkSizelimit)
		{
			string errorMsg = string("mkSize to large  size:")+TC_Common::tostr(iRecordSize)+". warnning limit size:"+TC_Common::tostr(g_mkSizelimit)+" sql:"+sSql + " mainKey:" + mainKey;
			LOG->error() << errorMsg << endl;
			g_largemk_count->report(1);
		}
		if(iRecordSize == _iSelectLimit)
		{
			string errorMsg = string("mkSize to large  refused size:")+TC_Common::tostr(iRecordSize) + " mainKey:" + mainKey;
			LOG->error() << errorMsg << endl;
			//TAF_NOTIFY_ERROR(errorMsg);
			g_largemk_count->report(1);
			//return eDbError;
		}
		vtData = recordSet.data();
		LOG->debug()<<mysqlNum<<"|"<<sDbName<<"|"<<sTableName<<"|select|"<<endTime-beginTime<<"|"<<int(vtData.size())<<endl;
		
		return vtData.size();
	}
	catch(TC_Mysql_Exception &ex)
	{
		LOG->error() << mysql->getLastSQL() << endl;
		LOG->error() << ex.what() << endl;
		//db_error->report(1);

		endTime = TC_TimeProvider::getInstance()->getNowMs();
		Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_EXCE,endTime-beginTime);
		mysqlErrCounter[mysqlNum]->finishInvoke(false);
		iRet = eDbError;
	}
	catch(TC_Mysql_TimeOut_Exception & ex)
	{
		LOG->error() << mysql->getLastSQL() << endl;
        LOG->error() << ex.what() << endl;

		endTime = TC_TimeProvider::getInstance()->getNowMs();
		Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_TIMEOUT,endTime-beginTime);
		mysqlErrCounter[mysqlNum]->finishInvoke(true);
		//db_error_timeout->report(1);
		endTime = TC_TimeProvider::getInstance()->getNowMs();
		LOG->debug()<<mysqlNum<<"|"<<sDbName<<"|"<<sTableName<<"|select timeout|"<<endTime-beginTime<<endl;;
        iRet = eDbError;
	}
	catch(std::exception &ex)
	{
		LOG->error() << ex.what() << endl;
		mysqlErrCounter[mysqlNum]->finishInvoke(false);
		iRet = eDbUnknownError;
	}

	return iRet;
}
taf::Int32 MKDbOperator::replace(const string &mainKey, const map<string, DbUpdateValue> &mpValue, const vector<DbCondition> &vtCond)
{
	int iRet = eDbUnknownError;
	string sTableName;
	string sDbName;
	string mysqlNum;
	int64_t beginTime,endTime;
	string optDBTpye;

	string strResult;
	TC_Mysql* mysql = getDb(sDbName, sTableName, mysqlNum, mainKey, "w", strResult);
	if(mysql == NULL)
	{
		if(strResult!="repair")
			return eDbTableNameError;
		else
			return eDbSucc;
	}
	mysql->setTimeoutRetry(true);

	try
	{
		//// 先查询是否存在记录
		//string sSql = "select count(*) as n from " + sDbName + "." + sTableName + buildConditionSQL(vtCond, mysql);
		//LOG->info() << sSql << endl;

		//beginTime = TC_TimeProvider::getInstance()->getNowMs();
		//optDBTpye = "select";

		//TC_Mysql::MysqlData recordSet = mysql->queryRecord(sSql);

		//endTime = TC_TimeProvider::getInstance()->getNowMs();
		//Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,"DCache."+sDbName+"_"+sTableName,mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
		//mysqlErrCounter[mysqlNum]->finishInvoke(false);
		//if(recordSet.size() == 0)
		//{
		//	return eDbError;
		//}
		//if(TC_Common::strto<int>(recordSet[0]["n"]) > 0)
		//{
		//	// 记录已经存在，更新已有记录
		//	string sUpdate = buildUpdateSQL(mpValue, mysql);
		//	if(sUpdate.empty())
		//	{
		//		return 0;
		//	}
		//	sSql = "update " + sDbName + "." + sTableName + " set " + sUpdate + buildConditionSQL(vtCond, mysql);
		//	LOG->info() << sSql << endl;
		//	beginTime = TC_TimeProvider::getInstance()->getNowMs();
		//	optDBTpye = "update";
		//	mysql->execute(sSql);
		//	endTime = TC_TimeProvider::getInstance()->getNowMs();
		//	Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,"DCache."+sDbName+"_"+sTableName,mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
		//	DB_LOG<<mysqlNum<<"|"<<sDbName<<"|"<<sTableName<<"|update|"<<endTime-beginTime<<endl;
		//	mysqlErrCounter[mysqlNum]->finishInvoke(false);
		//	return mysql_affected_rows(mysql->getMysql());
		//}
		//else
		//{
		//	// 记录不存在，插入新记录
		//	map<string, pair<TC_Mysql::FT, string> > mpColumns;
		//	map<string, DbUpdateValue>::const_iterator it = mpValue.begin();
		//	while(it != mpValue.end())
		//	{
		//		mpColumns[it->first] = make_pair(DT2FT(it->second.type), it->second.value);
		//		it ++;
		//	}
		//	beginTime = TC_TimeProvider::getInstance()->getNowMs();
		//	optDBTpye = "insert";
		//	iRet = mysql->insertRecord(sDbName + "." + sTableName, mpColumns);
		//	endTime = TC_TimeProvider::getInstance()->getNowMs();
		//	Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,"DCache."+sDbName+"_"+sTableName,mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
		//	DB_LOG<<mysqlNum<<"|"<<sDbName<<"|"<<sTableName<<"|insert|"<<endTime-beginTime<<endl;
		//	mysqlErrCounter[mysqlNum]->finishInvoke(false);
		//	return iRet;
		//}
			map<string, pair<TC_Mysql::FT, string> > mpColumns;
			map<string, DbUpdateValue>::const_iterator it = mpValue.begin();
			while(it != mpValue.end())
			{
				if(it->second.type==INT)
				{
				   if(!IsDigit(it->second.value))
				   {
					   if(g_bTypeErrorRepair)
						   return eDbSucc;
					   LOG->error() <<"MKDbOperator::replace type error INT but value=" << it->second.value << endl;
					   return eDbJceDecodeError;
				   }
				}
				mpColumns[it->first] = make_pair(DT2FT(it->second.type), it->second.value);
				it ++;

			}
			beginTime = TC_TimeProvider::getInstance()->getNowMs();
			optDBTpye = "replace";
			iRet = mysql->replaceRecord(sDbName + "." + sTableName, mpColumns);
			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
			LOG->debug()<<mysqlNum<<"|"<<sDbName<<"|"<<sTableName<<"|replace|"<<endTime-beginTime<<endl;
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
			return iRet;
		
	}
	catch(TC_Mysql_Exception &ex)
	{
		LOG->error() << mysql->getLastSQL() << endl;
		LOG->error() << ex.what() << endl;
		//db_error->report(1);
		endTime = TC_TimeProvider::getInstance()->getNowMs();
		Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_EXCE,endTime-beginTime);
		mysqlErrCounter[mysqlNum]->finishInvoke(false);
		iRet = eDbError;
		string strTmp = ex.what();
		size_t iFound = strTmp.find("Data too long for column");
		if (iFound!=std::string::npos)
		{
			if(g_bTypeErrorRepair)
			       iRet = eDbSucc;
		}
	}
	catch(TC_Mysql_TimeOut_Exception & ex)
	{
		LOG->error() << mysql->getLastSQL() << endl;
        LOG->error() << ex.what() << endl;

		endTime = TC_TimeProvider::getInstance()->getNowMs();
		Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_TIMEOUT,endTime-beginTime);
		mysqlErrCounter[mysqlNum]->finishInvoke(true);
		endTime = TC_TimeProvider::getInstance()->getNowMs();
		LOG->debug()<<mysqlNum<<"|"<<sDbName<<"|"<<sTableName<<"|replace|"<<endTime-beginTime<<endl;
		//db_error_timeout->report(1);
        iRet = eDbError;
	}
	catch(std::exception &ex)
	{
		LOG->error() << ex.what() << endl;
		mysqlErrCounter[mysqlNum]->finishInvoke(false);
		iRet = eDbUnknownError;
	}

	return iRet;
}

taf::Int32 MKDbOperator::del(const string &mainKey, const vector<DbCondition> &vtCond)
{
	int iRet = eDbUnknownError;
	string sTableName;
	string sDbName;
	string mysqlNum;
	int64_t beginTime,endTime;
	string optDBTpye;
	string strResult;
	TC_Mysql* mysql = getDb(sDbName, sTableName, mysqlNum, mainKey, "w", strResult);
	if(mysql == NULL)
	{
		return eDbTableNameError;
	}
	mysql->setTimeoutRetry(true);
	try
	{
		beginTime = TC_TimeProvider::getInstance()->getNowMs();
		optDBTpye = "delete";
		string tmp = buildConditionSQL(vtCond, mysql);
		LOG->debug() <<"delete sql:" <<tmp << endl;
		iRet = mysql->deleteRecord(sDbName + "." + sTableName, tmp);
		endTime = TC_TimeProvider::getInstance()->getNowMs();
		Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
		LOG->debug()<<mysqlNum<<"|"<<sDbName<<"|"<<sTableName<<"|delete|"<<endTime-beginTime<<endl;
		mysqlErrCounter[mysqlNum]->finishInvoke(false);
		return iRet;
	}
	catch(TC_Mysql_Exception &ex)
	{
		LOG->error() << mysql->getLastSQL() << endl;
		LOG->error() << ex.what() << endl;
		endTime = TC_TimeProvider::getInstance()->getNowMs();
		Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_EXCE,endTime-beginTime);
		mysqlErrCounter[mysqlNum]->finishInvoke(false);
		//db_error->report(1);
		iRet = eDbError;
	}
	catch(TC_Mysql_TimeOut_Exception & ex)
	{
		LOG->error() << mysql->getLastSQL() << endl;
        LOG->error() << ex.what() << endl;
		mysqlErrCounter[mysqlNum]->finishInvoke(true);
		//db_error_timeout->report(1);
		endTime = TC_TimeProvider::getInstance()->getNowMs();
		Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_TIMEOUT,endTime-beginTime);
		LOG->debug()<<mysqlNum<<"|"<<sDbName<<"|"<<sTableName<<"|delete|"<<endTime-beginTime<<endl;
        iRet = eDbError;
	}
	catch(std::exception &ex)
	{
		LOG->error() << ex.what() << endl;
		mysqlErrCounter[mysqlNum]->finishInvoke(false);
		iRet = eDbUnknownError;
	}	
	return iRet;
}
