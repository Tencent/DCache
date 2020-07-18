#include "DbAccessImp.h"
#include "JceOperator.h"


////////////////////////////////////////////////////////
void DbAccessImp::initialize()
{
	_tablef.initialize(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");
	_dbf.initialize(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");
	TC_Config tcConf;
	tcConf.parseFile(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");
    int CacheType = TC_Common::strto<int>(tcConf["/tars/<CacheType>"]);
	size_t iSelectLimit = TC_Common::strto<size_t>(tcConf.get("/tars/<SelectLimit>","100000"));
	if(CacheType==1)
	{
		_isMKDBAccess = false;
	    LOG->debug() <<" CacheType =1"<< endl;
	}
	else if(CacheType==2)
	{
		_isMKDBAccess = true;
		LOG->debug() <<" CacheType =2"<< endl;
	}
	else
	{
		LOG->error() <<" CacheType in conf must =1 or =2"<< endl;
		assert(false);
	}
	if(!_isMKDBAccess)
	{
		_sigKeyNameInDB = tcConf["/tars/sigKey/<KeyNameInDB>"];
		string sIsSerializated = tcConf.get("/tars/sigKey/<isSerializated>", "Y");
	    _isSerializated = (sIsSerializated=="Y" || sIsSerializated=="y")?true:false;
		if(_isSerializated)
		{
			LOG->debug()<<"Serializated true"<<endl;
		}
		else
		{
			LOG->debug()<<"Serializated false"<<endl;
		}
		map<string, string> mpFiedInfoTmp = tcConf.getDomainMap("/tars/sigKey/record");
		for(map<string, string>::iterator it=mpFiedInfoTmp.begin(); it!=mpFiedInfoTmp.end(); it++)
		{
			vector<string> vtInfo = TC_Common::sepstr<string>(it->second, "|", true);
			struct FieldInfo info;
			info.tag = TC_Common::strto<uint32_t>(TC_Common::trim(vtInfo[0]));
			info.type = TC_Common::trim(vtInfo[1]);
			info.bRequire = (TC_Common::trim(vtInfo[2]) == "require") ? true:false;
			info.defValue = TC_Common::trim(vtInfo[3]);
			info.FieldName = it->first;
			_vtFieldInfo.push_back(info);
			LOG->debug()<<it->first<<endl;
		}
		std::sort(_vtFieldInfo.begin(),_vtFieldInfo.end(),sortByTag);  
		LOG->debug()<<_vtFieldInfo.size()<<endl;
	}
//	typedef string (DefaultTableNameGen::*GetTableMem)(const string &);


	//table_functor table_cmd(&_tablef, static_cast<GetTableMem>(&DefaultTableNameGen::getTableName));
	table_functor table_cmd = std::bind(&DefaultTableNameGen::getTableName, &_tablef, std::placeholders::_1);
	_tableFunctor = table_cmd;
	_mkDbopt.set_table_functor(table_cmd);

//	typedef TC_Mysql* (DefaultDbConnGen::*GetDbConnMem)(const string &, const string &, string &,string &);

	//db_functor dbconn_cmd(&_dbf, static_cast<GetDbConnMem>(&DefaultDbConnGen::getDbConn));
	db_functor dbconn_cmd = std::bind(&DefaultDbConnGen::getDbConn, &_dbf, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
	_dbFunctor = dbconn_cmd;
	_mkDbopt.set_db_functor(dbconn_cmd);
	_mkDbopt.set_select_limit(iSelectLimit);
}

void DbAccessImp::destroy()
{
}

int DbAccessImp::get( const string &keyItem, string &value, int &expireTime , tars::TarsCurrentPtr current )
{
	if (_isMKDBAccess)
	{
		return eDbNotImplement;
	}
	else
	{
		int iRet = eDbUnknownError;
		TC_Mysql* _mysql;
		string sTableName;
		string sDbName;
		string mysqlNum;
		int64_t beginTime = 0, endTime = 0;
		string optDBTpye;
		try
		{
			_mysql = _dbFunctor(TC_Common::tostr(keyItem), "r", sDbName, mysqlNum);
			if (_mysql == NULL)
			{
				iRet = eDbTableNameError;
				LOG->error() << "mysql conn is NULL or forbided" << endl;
				return iRet;
			}
			setMysqlTimeoutRetry(_mysql);
			sTableName = _tableFunctor(TC_Common::tostr(keyItem));
		}
		catch (std::exception &ex)
		{
			iRet = eDbTableNameError;
			LOG->error() << ex.what() << endl;
			return iRet;
		}
		try
		{
			string sSql;
			// if (isStr)
			// {
				sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`='" + _mysql->escapeString(keyItem) + "'";
			// }
			// else
			// {
			// 	sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`=" + TC_Common::tostr(keyItem);
			// }
			beginTime = TC_TimeProvider::getInstance()->getNowMs();
			optDBTpye = "select";
			LOG->debug() << "sql:" << sSql << endl;
			TC_Mysql::MysqlData recordSet = _mysql->queryRecord(sSql);

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache." + ServerConfig::ServerName, ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db", mapDBInfo[mysqlNum].ip + "_" + TC_Common::tostr(mapDBInfo[mysqlNum].port), 0, optDBTpye, tars::StatReport::STAT_SUCC, endTime - beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);


			if (recordSet.size() == 0)
			{
				return eDbRecordNotExist;
			}
			TC_Mysql::MysqlRecord record = recordSet[0];
			if (_isSerializated)
			{
				JceEncode vEncode;
				for (vector<FieldInfo>::iterator it = _vtFieldInfo.begin(); it != _vtFieldInfo.end(); it++)
				{
					vEncode.write(record[it->FieldName], it->tag, it->type);
				}
				value.assign(vEncode.getBuffer(), vEncode.getLength());
				iRet = eDbSucc;
			}
			else
			{
				vector<FieldInfo>::iterator it = _vtFieldInfo.begin();
				value = record[it->FieldName];
				iRet = eDbSucc;
			}
			expireTime = TC_Common::strto<tars::Int32>(record["sDCacheExpireTime"]);

		}
		catch (TC_Mysql_Exception &ex)
		{
			LOG->error() << _mysql->getLastSQL() << endl;
			LOG->error() << ex.what() << endl;
			//db_error->report(1);
			iRet = eDbError;

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache." + ServerConfig::ServerName, ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db", mapDBInfo[mysqlNum].ip + "_" + TC_Common::tostr(mapDBInfo[mysqlNum].port), 0, optDBTpye, tars::StatReport::STAT_EXCE, endTime - beginTime);

			mysqlErrCounter[mysqlNum]->finishInvoke(false);
		}
		catch (TC_Mysql_TimeOut_Exception &ex)
		{
			LOG->error() << _mysql->getLastSQL() << endl;
			LOG->error() << ex.what() << endl;

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache." + ServerConfig::ServerName, ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db", mapDBInfo[mysqlNum].ip + "_" + TC_Common::tostr(mapDBInfo[mysqlNum].port), 0, optDBTpye, tars::StatReport::STAT_TIMEOUT, endTime - beginTime);

			mysqlErrCounter[mysqlNum]->finishInvoke(true);
			//db_error_timeout->report(1);
			iRet = eDbError;
		}
		catch (std::exception &ex)
		{
			LOG->error() << ex.what() << endl;
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
			iRet = eDbUnknownError;
		}
		return iRet;
	}
}

/**
* 设置key-value。
*/
int DbAccessImp::set( const string & keyItem, const string &value, int expireTime, tars::TarsCurrentPtr current)
{
 	if(_isMKDBAccess)
	{
		return eDbNotImplement;
	}
	else
	{
		// int affect = 0 ;
        int iRet = eDbUnknownError;
        TC_Mysql* _mysql;
        string sTableName;
        string sDbName;
        string mysqlNum;
		int64_t beginTime,endTime;
		string optDBTpye;

		beginTime = TC_TimeProvider::getInstance()->getNowMs();

        try
        {
            _mysql = _dbFunctor(keyItem, "w", sDbName,mysqlNum);
            if(_mysql == NULL)
            {
                iRet = eDbTableNameError;
                LOG->error() << "mysql conn is NULL" << endl;
                return iRet;
            }
			setMysqlTimeoutRetry(_mysql);
            sTableName = _tableFunctor(keyItem);
        }
        catch(std::exception &ex)
        {
            iRet = eDbTableNameError;
            LOG->error() << ex.what() << endl;
            return iRet;
        }
        try
        {
			string sSql;
			TC_Mysql::RECORD_DATA updateData;

			// if(isStr)
			// {
                //sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`='"+ TC_Common::tostr(keyItem) + "'";
				updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_STR, keyItem);
			// }
			// else
			// {
				// sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`="+ TC_Common::tostr(keyItem);
				// updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_INT, keyItem);
			// }
			if(_isSerializated)
			{
				JceDecode vDecode;
				vDecode.setBuffer(value);
				for(vector<FieldInfo>::iterator it=_vtFieldInfo.begin(); it!=_vtFieldInfo.end(); it++)
				{
					LOG->debug() <<it->FieldName<< endl;
					string tmp;
					tmp = vDecode.read(it->tag,it->type,it->defValue,it->bRequire);
					if(it->type == "string"||it->type == "byte")
					{
						updateData[it->FieldName] = make_pair(TC_Mysql::DB_STR,tmp);
					}
					else
					{
						updateData[it->FieldName] = make_pair(TC_Mysql::DB_INT,tmp);
					}
					LOG->debug() <<tmp<< endl;
				}
			}
			else
			{
				vector<FieldInfo>::iterator it=_vtFieldInfo.begin();
				if(it->type == "string")
				{
				     updateData[it->FieldName] = make_pair(TC_Mysql::DB_STR,value);
				}
				else
				{
					 updateData[it->FieldName] = make_pair(TC_Mysql::DB_INT,value);
				}
			}      

			optDBTpye = "replace";
            _mysql->replaceRecord(sDbName +"." + sTableName,updateData);
            LOG->debug() << "set, key:" << TC_Common::tostr(keyItem) << ", table:" << sDbName +"." + sTableName << endl;
			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_SUCC,endTime-beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
            iRet = eDbSucc;
			
        }
        catch(TC_Mysql_Exception &ex)
        {
            LOG->error() << _mysql->getLastSQL() << endl;
            LOG->error() << ex.what() << endl;
			//db_error->report(1);
            iRet = eDbError;
			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_EXCE,endTime-beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
        }
		catch(TC_Mysql_TimeOut_Exception &ex)
		{
			LOG->error() << _mysql->getLastSQL() << endl;
            LOG->error() << ex.what() << endl;
			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_TIMEOUT,endTime-beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(true);
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
}

int DbAccessImp::del(const string & keyItem , tars::TarsCurrentPtr current )
{
 	if(_isMKDBAccess)
	{
		return eDbNotImplement;
	}
	else
	{
		int iRet = eDbUnknownError;
        TC_Mysql* _mysql;
        string sTableName;
        string sDbName;
        string mysqlNum;
		int64_t beginTime,endTime;
		string optDBTpye;

		beginTime = TC_TimeProvider::getInstance()->getNowMs();

        try
        {
            _mysql = _dbFunctor(keyItem, "w", sDbName,mysqlNum);
            if(_mysql == NULL)
            {
                iRet = eDbTableNameError;
                LOG->error() << "mysql conn is NULL" << endl;
                return iRet;
            }
			setMysqlTimeoutRetry(_mysql);
            sTableName = _tableFunctor(keyItem);
        }
        catch(std::exception &ex)
        {
            iRet = eDbTableNameError;
            LOG->error() << ex.what() << endl;
            return iRet;
        }
		// int affect = 0;
        try
        {
			string sCondition;
			// if(isStr)
			// {
               sCondition ="where `"+_sigKeyNameInDB+"`='"+ _mysql->escapeString(keyItem) + "'";
			// }
			// else
			// {
			//    sCondition ="where `"+_sigKeyNameInDB+"`="+ TC_Common::tostr(keyItem);
			// }

			optDBTpye = "delete";

            _mysql->deleteRecord(sDbName + "." +sTableName,sCondition);

            LOG->debug() << "sql:" << _mysql->getLastSQL() << endl;

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_SUCC,endTime-beginTime);

            iRet = eDbSucc;
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
        }
        catch(TC_Mysql_Exception &ex)
        {
            LOG->error() << _mysql->getLastSQL() << endl;
            LOG->error() << ex.what() << endl;
			//db_error->report(1);
            iRet = eDbError;

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_EXCE,endTime-beginTime);

			mysqlErrCounter[mysqlNum]->finishInvoke(false);
        }
		catch(TC_Mysql_TimeOut_Exception &ex)
		{
			LOG->error() << _mysql->getLastSQL() << endl;
            LOG->error() << ex.what() << endl;

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_TIMEOUT,endTime-beginTime);

			mysqlErrCounter[mysqlNum]->finishInvoke(true);
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
}

/***********************
******for MKVCache *****
**********************/

int DbAccessImp::select( const string & mainKey, const string & field, const vector<DbCondition> &vtCond, vector<map<string, string>> &vtData , tars::TarsCurrentPtr current )
{
	if(_isMKDBAccess)
	{
		return _mkDbopt.select(mainKey, field, vtCond, vtData);
	}
	else
	{
		return eDbNotImplement;
	}
}

int DbAccessImp::replace( const string & mainKey, const map<string, DbUpdateValue> &mpValue, const vector<DbCondition> &vtCond , tars::TarsCurrentPtr current )
{
	if(_isMKDBAccess)
	{
		return _mkDbopt.replace(mainKey, mpValue, vtCond);
	}
	else
	{
		return eDbNotImplement;
	}
}

int DbAccessImp::delCond( const string & mainKey, const vector<DbCondition> &vtCond, tars::TarsCurrentPtr current )
{
	if(_isMKDBAccess)
	{
		return _mkDbopt.del(mainKey, vtCond);
	}
	else
	{
		return eDbNotImplement;
	}
}

// tars::Int32 DbAccessImp::getString(const std::string & keyItem,std::string &value,tars::JceCurrentPtr current)
// {
//    return getValue<string>(keyItem,value,true);
// }

// tars::Int32 DbAccessImp::getStringExp(const std::string & keyItem, std::string &value, tars::Int32 &expireTime, tars::JceCurrentPtr current)
// {
// 	return getValue<string>(keyItem, value, expireTime, true);
// }

// tars::Int32 DbAccessImp::getInt(tars::Int32 keyItem,std::string &value,tars::JceCurrentPtr current)
// {
//    return getValue<tars::Int32>(keyItem,value,false);
// }

// tars::Int32 DbAccessImp::getIntExp(tars::Int32 keyItem, std::string &value, tars::Int32 &expireTime, tars::JceCurrentPtr current)
// {
// 	return getValue<tars::Int32>(keyItem, value, expireTime, false);
// }

// tars::Int32 DbAccessImp::getLong(tars::Int64 keyItem, std::string &value, tars::JceCurrentPtr current)
// {
// 	return getValue<tars::Int64>(keyItem, value, false);
// }

// tars::Int32 DbAccessImp::getLongExp(tars::Int64 keyItem, std::string &value, tars::Int32 &expireTime, tars::JceCurrentPtr current)
// {
// 	return getValue<tars::Int64>(keyItem, value, expireTime, false);
// }

// tars::Int32 DbAccessImp::delString(const std::string & keyItem,tars::JceCurrentPtr current)
// {
// 	return delValue<string>(keyItem,true);
// }
      
// tars::Int32 DbAccessImp::DbAccessImp::delInt(tars::Int32 keyItem,tars::JceCurrentPtr current)
// {
// 	return delValue<tars::Int32>(keyItem,false);
// }
      

// tars::Int32 DbAccessImp::delLong(tars::Int64 keyItem,tars::JceCurrentPtr current)
// {
// 	return delValue<tars::Int64>(keyItem,false);
// }
// tars::Int32 DbAccessImp::setString(const std::string & keyItem,const std::string & value,tars::JceCurrentPtr current)
// {
// 	LOG->debug() <<"setString:"<<keyItem<<endl;
//     return setValue<string>(keyItem,value,true);
// } 

// tars::Int32 DbAccessImp::setInt(tars::Int32 keyItem,const std::string & value,tars::JceCurrentPtr current)
// {
// 	LOG->debug() <<"setInt:"<<keyItem<<endl;
//     return setValue<tars::Int32>(keyItem,value,false);
// }
      

// tars::Int32 DbAccessImp::setLong(tars::Int64 keyItem,const std::string & value,tars::JceCurrentPtr current)
// {
// 	LOG->debug() <<"setLong:"<<keyItem<<endl;
// 	return setValue<tars::Int64>(keyItem,value,false);
// }
    
// tars::Int32 DbAccessImp::setStringBS(const vector<tars::Char> & keyItem,const vector<tars::Char> & value,tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
       
// tars::Int32 DbAccessImp::setIntBS(tars::Int32 keyItem,const vector<tars::Char> & value,tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
       
// tars::Int32 DbAccessImp::setLongBS(tars::Int64 keyItem,const vector<tars::Char> & value,tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
// tars::Int32 DbAccessImp::delStringBS(const vector<tars::Char> & keyItem,tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
// tars::Int32 DbAccessImp::getStringBS(const std::vector<tars::Char> & keyItem, std::vector<tars::Char> &value, tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
// tars::Int32 DbAccessImp::getStringBSExp(const std::vector<tars::Char> & keyItem, std::vector<tars::Char> &value, tars::Int32 &expireTime, tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }

// tars::Int32 DbAccessImp::getIntBS(tars::Int32 keyItem, std::vector<tars::Char> &value, tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
// tars::Int32 DbAccessImp::getIntBSExp(tars::Int32 keyItem, std::vector<tars::Char> &value, tars::Int32 &expireTime, tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }

// tars::Int32 DbAccessImp::getLongBS(tars::Int64 keyItem, std::vector<tars::Char> &value, tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
// tars::Int32 DbAccessImp::getLongBSExp(tars::Int64 keyItem, std::vector<tars::Char> &value, tars::Int32 &expireTime, tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }

// template<class T> tars::Int32 DbAccessImp::getValue(const T &keyItem,std::string &value,bool isStr)
// {
// 	if(_isMKDBAccess)
// 	{
// 		return eDbNotImplement;
// 	}
// 	else
// 	{
// 		int iRet = eDbUnknownError;
//         TC_Mysql* _mysql;
//         string sTableName;
//         string sDbName;
// 		string mysqlNum;
// 		int64_t beginTime,endTime;
// 		string optDBTpye;
//         try
//         {
//             _mysql = _dbFunctor(TC_Common::tostr(keyItem), "r", sDbName,mysqlNum);
//             if(_mysql == NULL)
//             {
//                 iRet = eDbTableNameError;
//                 LOG->error() << "mysql conn is NULL or forbided" << endl;
//                 return iRet;
//             }
// 			setMysqlTimeoutRetry(_mysql);
//             sTableName = _tableFunctor(TC_Common::tostr(keyItem));
//         }
//         catch(std::exception &ex)
//         {
//             iRet = eDbTableNameError;
//             LOG->error() << ex.what() << endl;
//             return iRet;
//         }
//         try
//         {
// 			string sSql;
// 			if(isStr)
// 			{
//                 sSql = "select * from " + sDbName + "." + sTableName + " where `"+_sigKeyNameInDB+"`='"+ _mysql->escapeString(TC_Common::tostr(keyItem)) + "'";
// 			}
// 			else
// 			{
// 				sSql = "select * from " + sDbName + "." + sTableName + " where `"+_sigKeyNameInDB+"`="+TC_Common::tostr(keyItem);
// 			}
// 			beginTime = TC_TimeProvider::getInstance()->getNowMs();
// 			optDBTpye = "select";
// 			LOG->debug() << "sql:" << sSql << endl;
//             TC_Mysql::MysqlData recordSet = _mysql->queryRecord(sSql);

// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_SUCC,endTime-beginTime);
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
			

//             if(recordSet.size() == 0)
//             {
//                 return eDbRecordNotExist;
//             }
//             TC_Mysql::MysqlRecord record = recordSet[0];
// 			if(_isSerializated)
// 			{
// 				JceEncode vEncode;
// 				for(vector<FieldInfo>::iterator it=_vtFieldInfo.begin(); it!=_vtFieldInfo.end(); it++)
// 				{
// 					vEncode.write(record[it->FieldName],it->tag,it->type);
// 				}
// 				value.assign(vEncode.getBuffer(),vEncode.getLength());
// 				iRet = eDbSucc;
// 			}
// 			else
// 			{
// 				vector<FieldInfo>::iterator it=_vtFieldInfo.begin();
// 				value = record[it->FieldName];
// 			    iRet = eDbSucc;
// 			}
			
//         }
//         catch(TC_Mysql_Exception &ex)
//         {
//             LOG->error() << _mysql->getLastSQL() << endl;
//             LOG->error() << ex.what() << endl;
// 			//db_error->report(1);
//             iRet = eDbError;

// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_EXCE,endTime-beginTime);
			
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
//         }
// 		catch(TC_Mysql_TimeOut_Exception &ex)
// 		{
// 			LOG->error() << _mysql->getLastSQL() << endl;
//             LOG->error() << ex.what() << endl;

// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_TIMEOUT,endTime-beginTime);
			
// 			mysqlErrCounter[mysqlNum]->finishInvoke(true);
// 			//db_error_timeout->report(1);
//             iRet = eDbError;
// 		}
//         catch(std::exception &ex)
//         {
//             LOG->error() << ex.what() << endl;
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
//             iRet = eDbUnknownError;
//         }
//         return iRet;
// 	}
// }
// template<class T> tars::Int32 DbAccessImp::getValue(const T &keyItem, std::string&value, tars::Int32 &expireTime, bool isStr)
// {
// 	if (_isMKDBAccess)
// 	{
// 		return eDbNotImplement;
// 	}
// 	else
// 	{
// 		int iRet = eDbUnknownError;
// 		TC_Mysql* _mysql;
// 		string sTableName;
// 		string sDbName;
// 		string mysqlNum;
// 		int64_t beginTime = 0, endTime = 0;
// 		string optDBTpye;
// 		try
// 		{
// 			_mysql = _dbFunctor(TC_Common::tostr(keyItem), "r", sDbName, mysqlNum);
// 			if (_mysql == NULL)
// 			{
// 				iRet = eDbTableNameError;
// 				LOG->error() << "mysql conn is NULL or forbided" << endl;
// 				return iRet;
// 			}
// 			setMysqlTimeoutRetry(_mysql);
// 			sTableName = _tableFunctor(TC_Common::tostr(keyItem));
// 		}
// 		catch (std::exception &ex)
// 		{
// 			iRet = eDbTableNameError;
// 			LOG->error() << ex.what() << endl;
// 			return iRet;
// 		}
// 		try
// 		{
// 			string sSql;
// 			if (isStr)
// 			{
// 				sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`='" + _mysql->escapeString(TC_Common::tostr(keyItem)) + "'";
// 			}
// 			else
// 			{
// 				sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`=" + TC_Common::tostr(keyItem);
// 			}
// 			beginTime = TC_TimeProvider::getInstance()->getNowMs();
// 			optDBTpye = "select";
// 			LOG->debug() << "sql:" << sSql << endl;
// 			TC_Mysql::MysqlData recordSet = _mysql->queryRecord(sSql);

// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache." + ServerConfig::ServerName, ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db", mapDBInfo[mysqlNum].ip + "_" + TC_Common::tostr(mapDBInfo[mysqlNum].port), 0, optDBTpye, tars::StatReport::STAT_SUCC, endTime - beginTime);
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);


// 			if (recordSet.size() == 0)
// 			{
// 				return eDbRecordNotExist;
// 			}
// 			TC_Mysql::MysqlRecord record = recordSet[0];
// 			if (_isSerializated)
// 			{
// 				JceEncode vEncode;
// 				for (vector<FieldInfo>::iterator it = _vtFieldInfo.begin(); it != _vtFieldInfo.end(); it++)
// 				{
// 					vEncode.write(record[it->FieldName], it->tag, it->type);
// 				}
// 				value.assign(vEncode.getBuffer(), vEncode.getLength());
// 				iRet = eDbSucc;
// 			}
// 			else
// 			{
// 				vector<FieldInfo>::iterator it = _vtFieldInfo.begin();
// 				value = record[it->FieldName];
// 				iRet = eDbSucc;
// 			}
// 			expireTime = TC_Common::strto<tars::Int32>(record["sDCacheExpireTime"]);

// 		}
// 		catch (TC_Mysql_Exception &ex)
// 		{
// 			LOG->error() << _mysql->getLastSQL() << endl;
// 			LOG->error() << ex.what() << endl;
// 			//db_error->report(1);
// 			iRet = eDbError;

// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache." + ServerConfig::ServerName, ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db", mapDBInfo[mysqlNum].ip + "_" + TC_Common::tostr(mapDBInfo[mysqlNum].port), 0, optDBTpye, tars::StatReport::STAT_EXCE, endTime - beginTime);

// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
// 		}
// 		catch (TC_Mysql_TimeOut_Exception &ex)
// 		{
// 			LOG->error() << _mysql->getLastSQL() << endl;
// 			LOG->error() << ex.what() << endl;

// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache." + ServerConfig::ServerName, ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db", mapDBInfo[mysqlNum].ip + "_" + TC_Common::tostr(mapDBInfo[mysqlNum].port), 0, optDBTpye, tars::StatReport::STAT_TIMEOUT, endTime - beginTime);

// 			mysqlErrCounter[mysqlNum]->finishInvoke(true);
// 			//db_error_timeout->report(1);
// 			iRet = eDbError;
// 		}
// 		catch (std::exception &ex)
// 		{
// 			LOG->error() << ex.what() << endl;
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
// 			iRet = eDbUnknownError;
// 		}
// 		return iRet;
// 	}
// }
// template<class T> tars::Int32 DbAccessImp::delValue(const T &keyItem,bool isStr)
// {
// 	if(_isMKDBAccess)
// 	{
// 		return eDbNotImplement;
// 	}
// 	else
// 	{
// 		int iRet = eDbUnknownError;
//         TC_Mysql* _mysql;
//         string sTableName;
//         string sDbName;
//         string mysqlNum;
// 		int64_t beginTime,endTime;
// 		string optDBTpye;
//         try
//         {
//             _mysql = _dbFunctor(TC_Common::tostr(keyItem), "w", sDbName,mysqlNum);
//             if(_mysql == NULL)
//             {
//                 iRet = eDbTableNameError;
//                 LOG->error() << "mysql conn is NULL" << endl;
//                 return iRet;
//             }
// 			setMysqlTimeoutRetry(_mysql);
//             sTableName = _tableFunctor(TC_Common::tostr(keyItem));
//         }
//         catch(std::exception &ex)
//         {
//             iRet = eDbTableNameError;
//             LOG->error() << ex.what() << endl;
//             return iRet;
//         }
// 		int affect = 0;
//         try
//         {
// 			string sCondition;
// 			if(isStr)
// 			{
//                sCondition ="where `"+_sigKeyNameInDB+"`='"+ _mysql->escapeString(TC_Common::tostr(keyItem)) + "'";
// 			}
// 			else
// 			{
// 			   sCondition ="where `"+_sigKeyNameInDB+"`="+ TC_Common::tostr(keyItem);
// 			}

// 			beginTime = TC_TimeProvider::getInstance()->getNowMs();
// 			optDBTpye = "delete";

//             affect = _mysql->deleteRecord(sDbName + "." +sTableName,sCondition);

//             LOG->debug() << "sql:" << _mysql->getLastSQL() << endl;

// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_SUCC,endTime-beginTime);

//             iRet = eDbSucc;
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
//         }
//         catch(TC_Mysql_Exception &ex)
//         {
//             LOG->error() << _mysql->getLastSQL() << endl;
//             LOG->error() << ex.what() << endl;
// 			//db_error->report(1);
//             iRet = eDbError;

// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_EXCE,endTime-beginTime);

// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
//         }
// 		catch(TC_Mysql_TimeOut_Exception &ex)
// 		{
// 			LOG->error() << _mysql->getLastSQL() << endl;
//             LOG->error() << ex.what() << endl;

// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_TIMEOUT,endTime-beginTime);

// 			mysqlErrCounter[mysqlNum]->finishInvoke(true);
// 			//db_error_timeout->report(1);
//             iRet = eDbError;
// 		}
//         catch(std::exception &ex)
//         {
//             LOG->error() << ex.what() << endl;
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
//             iRet = eDbUnknownError;
//         }
//         return iRet;
// 	}
// }
// template<class T> tars::Int32 DbAccessImp::setValue(const T &keyItem,const std::string & value,bool isStr)
// {
// 	if(_isMKDBAccess)
// 	{
// 		return eDbNotImplement;
// 	}
// 	else
// 	{
// 		int affect = 0 ;
//         int iRet = eDbUnknownError;
//         TC_Mysql* _mysql;
//         string sTableName;
//         string sDbName;
//         string mysqlNum;
// 		int64_t beginTime,endTime;
// 		string optDBTpye;
//         try
//         {
//             _mysql = _dbFunctor(TC_Common::tostr(keyItem), "w", sDbName,mysqlNum);
//             if(_mysql == NULL)
//             {
//                 iRet = eDbTableNameError;
//                 LOG->error() << "mysql conn is NULL" << endl;
//                 return iRet;
//             }
// 			setMysqlTimeoutRetry(_mysql);
//             sTableName = _tableFunctor(TC_Common::tostr(keyItem));
//         }
//         catch(std::exception &ex)
//         {
//             iRet = eDbTableNameError;
//             LOG->error() << ex.what() << endl;
//             return iRet;
//         }
//         try
//         {
// 			string sSql;
// 			TC_Mysql::RECORD_DATA updateData;

// 			if(isStr)
// 			{
//                 //sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`='"+ TC_Common::tostr(keyItem) + "'";
// 				updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_STR,TC_Common::tostr(keyItem));
// 			}
// 			else
// 			{
// 				//sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`="+ TC_Common::tostr(keyItem);
// 				updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_INT,TC_Common::tostr(keyItem));
// 			}
// 			if(_isSerializated)
// 			{
// 				JceDecode vDecode;
// 				vDecode.setBuffer(value);
// 				for(vector<FieldInfo>::iterator it=_vtFieldInfo.begin(); it!=_vtFieldInfo.end(); it++)
// 				{
// 					LOG->debug() <<it->FieldName<< endl;
// 					string tmp;
// 					tmp = vDecode.read(it->tag,it->type,it->defValue,it->bRequire);
// 					if(it->type == "string"||it->type == "byte")
// 					{
// 						updateData[it->FieldName] = make_pair(TC_Mysql::DB_STR,tmp);
// 					}
// 					else
// 					{
// 						updateData[it->FieldName] = make_pair(TC_Mysql::DB_INT,tmp);
// 					}
// 					LOG->debug() <<tmp<< endl;
// 				}
// 			}
// 			else
// 			{
// 				vector<FieldInfo>::iterator it=_vtFieldInfo.begin();
// 				if(it->type == "string")
// 				{
// 				     updateData[it->FieldName] = make_pair(TC_Mysql::DB_STR,value);
// 				}
// 				else
// 				{
// 					 updateData[it->FieldName] = make_pair(TC_Mysql::DB_INT,value);
// 				}
// 			}      

// 			beginTime = TC_TimeProvider::getInstance()->getNowMs();
// 			optDBTpye = "replace";
//             affect = _mysql->replaceRecord(sDbName +"." + sTableName,updateData);
//             LOG->debug() << "set, key:" << TC_Common::tostr(keyItem) << ", table:" << sDbName +"." + sTableName << endl;
// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_SUCC,endTime-beginTime);
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
//             iRet = eDbSucc;
			
//         }
//         catch(TC_Mysql_Exception &ex)
//         {
//             LOG->error() << _mysql->getLastSQL() << endl;
//             LOG->error() << ex.what() << endl;
// 			//db_error->report(1);
//             iRet = eDbError;
// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_EXCE,endTime-beginTime);
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
//         }
// 		catch(TC_Mysql_TimeOut_Exception &ex)
// 		{
// 			LOG->error() << _mysql->getLastSQL() << endl;
//             LOG->error() << ex.what() << endl;
// 			endTime = TC_TimeProvider::getInstance()->getNowMs();
// 			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, tars::StatReport::STAT_TIMEOUT,endTime-beginTime);
// 			mysqlErrCounter[mysqlNum]->finishInvoke(true);
// 			//db_error_timeout->report(1);
//             iRet = eDbError;
// 		}
//         catch(std::exception &ex)
//         {
//             LOG->error() << ex.what() << endl;
// 			mysqlErrCounter[mysqlNum]->finishInvoke(false);
//             iRet = eDbUnknownError;
//         }
//         return iRet;
// 	}
// }

// tars::Int32 DbAccessImp::select(const string &mainKey, const string &field, const vector<DbCondition> &vtCond, 
// 								 vector<map<string, string> > &vtData, tars::JceCurrentPtr current)
// {
// 	if(_isMKDBAccess)
// 	{
// 		return _mkDbopt.select(mainKey, field, vtCond, vtData);
// 	}
// 	else
// 	{
// 		return eDbNotImplement;
// 	}
// }
// tars::Int32 DbAccessImp::selectBS(const vector<tars::Char> & mainKey,const std::string & field,const vector<DCache::DbConditionBS> & vtCond,vector<map<std::string, vector<tars::Char> > > &vtData,tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
// tars::Int32 DbAccessImp::replaceBS(const vector<tars::Char> & mainKey,const map<std::string, DCache::DbUpdateValueBS> & mpValue,const vector<DCache::DbConditionBS> & vtCond,tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
// tars::Int32 DbAccessImp::delBS(const vector<tars::Char> & mainKey,const vector<DCache::DbConditionBS> & vtCond,tars::JceCurrentPtr current)
// {
// 	return eDbNotImplement;
// }
// tars::Int32 DbAccessImp::replace(const string &mainKey, const map<string, DbUpdateValue> &mpValue, 
// 								  const vector<DbCondition> &vtCond, tars::JceCurrentPtr current)
// {
// 	if(_isMKDBAccess)
// 	{
// 		return _mkDbopt.replace(mainKey, mpValue, vtCond);
// 	}
// 	else
// 	{
// 		return eDbNotImplement;
// 	}
// }

// tars::Int32 DbAccessImp::del(const string &mainKey, const vector<DbCondition> &vtCond, tars::JceCurrentPtr current)
// {
// 	if(_isMKDBAccess)
// 	{
// 		return _mkDbopt.del(mainKey, vtCond);
// 	}
// 	else
// 	{
// 		return eDbNotImplement;
// 	}
// }

