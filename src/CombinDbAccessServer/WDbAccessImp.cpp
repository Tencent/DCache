#include "WDbAccessImp.h"
#include "JceOperator.h"





////////////////////////////////////////////////////////
void WDbAccessImp::initialize()
{
	_tablef.initialize(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");
	_dbf.initialize(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");
	TC_Config tcConf;
	tcConf.parseFile(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");
    int CacheType = TC_Common::strto<int>(tcConf["/taf/<CacheType>"]);
	size_t iSelectLimit = TC_Common::strto<size_t>(tcConf.get("/taf/<SelectLimit>","100000"));
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
		_sigKeyNameInDB = tcConf["/taf/sigKey/<KeyNameInDB>"];
		string sIsSerializated = tcConf.get("/taf/sigKey/<isSerializated>", "Y");
	    _isSerializated = (sIsSerializated=="Y" || sIsSerializated=="y")?true:false;
		if(_isSerializated)
		{
			LOG->debug()<<"Serializated true"<<endl;
		}
		else
		{
			LOG->debug()<<"Serializated false"<<endl;
		}
		map<string, string> mpFiedInfoTmp = tcConf.getDomainMap("/taf/sigKey/record");
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
	typedef string (DefaultTableNameGen::*GetTableMem)(const string &);


	//table_functor table_cmd(&_tablef, static_cast<GetTableMem>(&DefaultTableNameGen::getTableName));
	table_functor table_cmd = std::bind(&DefaultTableNameGen::getTableName, &_tablef, std::placeholders::_1);
	_tableFunctor = table_cmd;
	_mkDbopt.set_table_functor(table_cmd);

	typedef TC_Mysql* (DefaultDbConnGen::*GetDbConnMem)(const string &, const string &, string &,string &);

	//db_functor dbconn_cmd(&_dbf, static_cast<GetDbConnMem>(&DefaultDbConnGen::getDbConn));
	db_functor dbconn_cmd = std::bind(&DefaultDbConnGen::getDbConn, &_dbf, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
	_dbFunctor = dbconn_cmd;
	_mkDbopt.set_db_functor(dbconn_cmd);
	_mkDbopt.set_select_limit(iSelectLimit);
}

void WDbAccessImp::destroy()
{
}

taf::Int32 WDbAccessImp::delString(const std::string & keyItem,taf::JceCurrentPtr current)
{
	return delValue<string>(keyItem,true);
}
      

taf::Int32 WDbAccessImp::WDbAccessImp::delInt(taf::Int32 keyItem,taf::JceCurrentPtr current)
{
	return delValue<taf::Int32>(keyItem,false);
}
      

taf::Int32 WDbAccessImp::delLong(taf::Int64 keyItem,taf::JceCurrentPtr current)
{
	return delValue<taf::Int64>(keyItem,false);
}

taf::Int32 WDbAccessImp::setString(const std::string & keyItem,const std::string & value,taf::JceCurrentPtr current)
{
	LOG->debug() <<"setString:"<<keyItem<<endl;
    return setValue<string>(keyItem,value,true);
}
taf::Int32 WDbAccessImp::setStringExp(const std::string & keyItem, const std::string & value, taf::Int32 expireTime, taf::JceCurrentPtr current)
{
	LOG->debug() << "setStringExp:" << keyItem << endl;
	return setValue<string>(keyItem, value, expireTime, true);
}

taf::Int32 WDbAccessImp::setInt(taf::Int32 keyItem,const std::string & value,taf::JceCurrentPtr current)
{
	LOG->debug() <<"setInt:"<<keyItem<<endl;
    return setValue<taf::Int32>(keyItem,value,false);
}
taf::Int32 WDbAccessImp::setIntExp(taf::Int32 keyItem, const std::string & value, taf::Int32 expireTime, taf::JceCurrentPtr current)
{
	LOG->debug() << "setInt:" << keyItem << endl;
	return setValue<taf::Int32>(keyItem, value, expireTime, false);
}   

taf::Int32 WDbAccessImp::setLong(taf::Int64 keyItem,const std::string & value,taf::JceCurrentPtr current)
{
	LOG->debug() <<"setLong:"<<keyItem<<endl;
	return setValue<taf::Int64>(keyItem,value,false);
}
taf::Int32 WDbAccessImp::setLongExp(taf::Int64 keyItem, const std::string & value, taf::Int32 expireTime, taf::JceCurrentPtr current)
{
	LOG->debug() << "setLong:" << keyItem << endl;
	return setValue<taf::Int64>(keyItem, value, expireTime, false);
}
    
taf::Int32 WDbAccessImp::setStringBS(const vector<taf::Char> & keyItem, const vector<taf::Char> & value, taf::JceCurrentPtr current)
{
	return eDbNotImplement;
}
taf::Int32 WDbAccessImp::setStringBSExp(const vector<taf::Char> & keyItem, const vector<taf::Char> & value, taf::Int32 expireTime, taf::JceCurrentPtr current)
{
	return eDbNotImplement;
}
       
taf::Int32 WDbAccessImp::setIntBS(taf::Int32 keyItem, const vector<taf::Char> & value, taf::JceCurrentPtr current)
{
	return eDbNotImplement;
}
taf::Int32 WDbAccessImp::setIntBSExp(taf::Int32 keyItem, const vector<taf::Char> & value, taf::Int32 expireTime, taf::JceCurrentPtr current)
{
	return eDbNotImplement;
}
       
taf::Int32 WDbAccessImp::setLongBS(taf::Int64 keyItem, const vector<taf::Char> & value, taf::JceCurrentPtr current)
{
	return eDbNotImplement;
}
taf::Int32 WDbAccessImp::setLongBSExp(taf::Int64 keyItem, const vector<taf::Char> & value, taf::Int32 expireTime, taf::JceCurrentPtr current)
{
	return eDbNotImplement;
}


taf::Int32 WDbAccessImp::delStringBS(const vector<taf::Char> & keyItem,taf::JceCurrentPtr current)
{
	return eDbNotImplement;
}
template<class T> taf::Int32 WDbAccessImp::delValue(const T &keyItem,bool isStr)
{
	LOG->debug() <<"WDbAccessImp::delValue:"<<keyItem<<endl;
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
        try
        {
            _mysql = _dbFunctor(TC_Common::tostr(keyItem), "w", sDbName,mysqlNum);
            if(_mysql == NULL)
            {
                iRet = eDbTableNameError;
                LOG->error() << "mysql conn is NULL" << endl;
                return iRet;
            }
            sTableName = _tableFunctor(TC_Common::tostr(keyItem));
        }
        catch(std::exception &ex)
        {
            iRet = eDbTableNameError;
            LOG->error() << ex.what() << endl;
            return iRet;
        }
		int affect = 0;
        try
        {
			string sCondition;
			if(isStr)
			{
               sCondition ="where `"+_sigKeyNameInDB+"`='"+ _mysql->escapeString(TC_Common::tostr(keyItem)) + "'";
			}
			else
			{
			   sCondition ="where `"+_sigKeyNameInDB+"`="+ TC_Common::tostr(keyItem);
			}

			beginTime = TC_TimeProvider::getInstance()->getNowMs();
			optDBTpye = "delete";

            affect = _mysql->deleteRecord(sDbName + "." +sTableName,sCondition);

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);

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
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_EXCE,endTime-beginTime);

			mysqlErrCounter[mysqlNum]->finishInvoke(false);
        }
		catch(TC_Mysql_TimeOut_Exception &ex)
		{
			LOG->error() << _mysql->getLastSQL() << endl;
            LOG->error() << ex.what() << endl;

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_TIMEOUT,endTime-beginTime);

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
template<class T> taf::Int32 WDbAccessImp::setValue(const T &keyItem,const std::string & value,bool isStr)
{
	LOG->debug() <<"WDbAccessImp::setValue:"<<keyItem<<endl;
	if(_isMKDBAccess)
	{
		return eDbNotImplement;
	}
	else
	{
		int affect = 0 ;
        int iRet = eDbUnknownError;
        TC_Mysql* _mysql;
        string sTableName;
        string sDbName;
        string mysqlNum;
		int64_t beginTime,endTime;
		string optDBTpye;
        try
        {
            _mysql = _dbFunctor(TC_Common::tostr(keyItem), "w", sDbName,mysqlNum);
            if(_mysql == NULL)
            {
                iRet = eDbTableNameError;
                LOG->error() << "mysql conn is NULL" << endl;
                return iRet;
            }
            sTableName = _tableFunctor(TC_Common::tostr(keyItem));
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
			/*if(isStr)
			{
                sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`='"+ TC_Common::tostr(keyItem) + "'";
				updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_STR,TC_Common::tostr(keyItem));
			}
			else
			{
				sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`="+ TC_Common::tostr(keyItem);
				updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_INT,TC_Common::tostr(keyItem));
			}
			beginTime = TC_TimeProvider::getInstance()->getNowMs();
			optDBTpye = "select";

            TC_Mysql::MysqlData recordSet = _mysql->queryRecord(sSql);

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,"DCache."+sDbName+"_"+sTableName,mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);*/
			if(isStr)
			{
                //sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`='"+ TC_Common::tostr(keyItem) + "'";
				updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_STR,TC_Common::tostr(keyItem));
			}
			else
			{
				//sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`="+ TC_Common::tostr(keyItem);
				updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_INT,TC_Common::tostr(keyItem));
			}
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
           /* if(recordSet.size() == 0)
            {
				beginTime = TC_TimeProvider::getInstance()->getNowMs();
			    optDBTpye = "insert";
                affect = _mysql->insertRecord(sDbName +"." + sTableName,updateData);

				endTime = TC_TimeProvider::getInstance()->getNowMs();
			    Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,"DCache."+sDbName+"_"+sTableName,mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
				mysqlErrCounter[mysqlNum]->finishInvoke(false);
            }
            else
            {
				string sCondition;
				if(isStr)
			    {
                     sCondition ="where `"+_sigKeyNameInDB+"`='"+ TC_Common::tostr(keyItem) + "'";
				}
				else
				{
					 sCondition ="where `"+_sigKeyNameInDB+"`="+ TC_Common::tostr(keyItem);
				}

				beginTime = TC_TimeProvider::getInstance()->getNowMs();
			    optDBTpye = "update";
                affect = _mysql->updateRecord(sDbName + "." + sTableName,updateData,sCondition);

				endTime = TC_TimeProvider::getInstance()->getNowMs();
			    Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,"DCache."+sDbName+"_"+sTableName,mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
				mysqlErrCounter[mysqlNum]->finishInvoke(false);
            }*/
            beginTime = TC_TimeProvider::getInstance()->getNowMs();
			optDBTpye = "replace";
            affect = _mysql->replaceRecord(sDbName +"." + sTableName,updateData);
			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
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
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_EXCE,endTime-beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
        }
		catch(TC_Mysql_TimeOut_Exception &ex)
		{
			LOG->error() << _mysql->getLastSQL() << endl;
            LOG->error() << ex.what() << endl;
			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db",mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_TIMEOUT,endTime-beginTime);
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
template<class T> taf::Int32 WDbAccessImp::setValue(const T &keyItem, const std::string & value, taf::Int32 expireTime, bool isStr)
{
	LOG->debug() << "WDbAccessImp::setValue:" << keyItem << endl;
	if (_isMKDBAccess)
	{
		return eDbNotImplement;
	}
	else
	{
		int affect = 0;
		int iRet = eDbUnknownError;
		TC_Mysql* _mysql;
		string sTableName;
		string sDbName;
		string mysqlNum;
		int64_t beginTime, endTime;
		string optDBTpye;
		try
		{
			_mysql = _dbFunctor(TC_Common::tostr(keyItem), "w", sDbName, mysqlNum);
			if (_mysql == NULL)
			{
				iRet = eDbTableNameError;
				LOG->error() << "mysql conn is NULL" << endl;
				return iRet;
			}
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
			TC_Mysql::RECORD_DATA updateData;
			/*if(isStr)
			{
			sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`='"+ TC_Common::tostr(keyItem) + "'";
			updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_STR,TC_Common::tostr(keyItem));
			}
			else
			{
			sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`="+ TC_Common::tostr(keyItem);
			updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_INT,TC_Common::tostr(keyItem));
			}
			beginTime = TC_TimeProvider::getInstance()->getNowMs();
			optDBTpye = "select";

			TC_Mysql::MysqlData recordSet = _mysql->queryRecord(sSql);

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,"DCache."+sDbName+"_"+sTableName,mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);*/
			if (isStr)
			{
				//sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`='"+ TC_Common::tostr(keyItem) + "'";
				updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_STR, TC_Common::tostr(keyItem));
			}
			else
			{
				//sSql = "select * from " + sDbName + "." + sTableName + " where `" + _sigKeyNameInDB + "`="+ TC_Common::tostr(keyItem);
				updateData[_sigKeyNameInDB] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(keyItem));
			}
			if (_isSerializated)
			{
				JceDecode vDecode;
				vDecode.setBuffer(value);
				for (vector<FieldInfo>::iterator it = _vtFieldInfo.begin(); it != _vtFieldInfo.end(); it++)
				{
					LOG->debug() << it->FieldName << endl;
					string tmp;
					tmp = vDecode.read(it->tag, it->type, it->defValue, it->bRequire);
					if (it->type == "string" || it->type == "byte")
					{
						updateData[it->FieldName] = make_pair(TC_Mysql::DB_STR, tmp);
					}
					else
					{
						updateData[it->FieldName] = make_pair(TC_Mysql::DB_INT, tmp);
					}
					LOG->debug() << tmp << endl;
				}
			}
			else
			{
				vector<FieldInfo>::iterator it = _vtFieldInfo.begin();
				if (it->type == "string")
				{
					updateData[it->FieldName] = make_pair(TC_Mysql::DB_STR, value);
				}
				else
				{
					updateData[it->FieldName] = make_pair(TC_Mysql::DB_INT, value);
				}
			}
			updateData["sDCacheExpireTime"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(expireTime)); //@2016.3.18
			/* if(recordSet.size() == 0)
			{
			beginTime = TC_TimeProvider::getInstance()->getNowMs();
			optDBTpye = "insert";
			affect = _mysql->insertRecord(sDbName +"." + sTableName,updateData);

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,"DCache."+sDbName+"_"+sTableName,mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
			}
			else
			{
			string sCondition;
			if(isStr)
			{
			sCondition ="where `"+_sigKeyNameInDB+"`='"+ TC_Common::tostr(keyItem) + "'";
			}
			else
			{
			sCondition ="where `"+_sigKeyNameInDB+"`="+ TC_Common::tostr(keyItem);
			}

			beginTime = TC_TimeProvider::getInstance()->getNowMs();
			optDBTpye = "update";
			affect = _mysql->updateRecord(sDbName + "." + sTableName,updateData,sCondition);

			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache."+ServerConfig::ServerName,ServerConfig::LocalIp,"DCache."+sDbName+"_"+sTableName,mapDBInfo[mysqlNum].ip+"_"+TC_Common::tostr(mapDBInfo[mysqlNum].port),0,optDBTpye, taf::StatReport::STAT_SUCC,endTime-beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
			}*/
			beginTime = TC_TimeProvider::getInstance()->getNowMs();
			optDBTpye = "replace";
			affect = _mysql->replaceRecord(sDbName + "." + sTableName, updateData);
			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache." + ServerConfig::ServerName, ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db", mapDBInfo[mysqlNum].ip + "_" + TC_Common::tostr(mapDBInfo[mysqlNum].port), 0, optDBTpye, taf::StatReport::STAT_SUCC, endTime - beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
			iRet = eDbSucc;

		}
		catch (TC_Mysql_Exception &ex)
		{
			LOG->error() << _mysql->getLastSQL() << endl;
			LOG->error() << ex.what() << endl;
			//db_error->report(1);
			iRet = eDbError;
			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache." + ServerConfig::ServerName, ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db", mapDBInfo[mysqlNum].ip + "_" + TC_Common::tostr(mapDBInfo[mysqlNum].port), 0, optDBTpye, taf::StatReport::STAT_EXCE, endTime - beginTime);
			mysqlErrCounter[mysqlNum]->finishInvoke(false);
		}
		catch (TC_Mysql_TimeOut_Exception &ex)
		{
			LOG->error() << _mysql->getLastSQL() << endl;
			LOG->error() << ex.what() << endl;
			endTime = TC_TimeProvider::getInstance()->getNowMs();
			Application::getCommunicator()->getStatReport()->report("DCache." + ServerConfig::ServerName, ServerConfig::LocalIp,/*"DCDB."+sDbName+"_"+sTableName*/"DCDB.db", mapDBInfo[mysqlNum].ip + "_" + TC_Common::tostr(mapDBInfo[mysqlNum].port), 0, optDBTpye, taf::StatReport::STAT_TIMEOUT, endTime - beginTime);
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
taf::Int32 WDbAccessImp::replaceBS(const vector<taf::Char> & mainKey,const map<std::string, DCache::DbUpdateValueBS> & mpValue,const vector<DCache::DbConditionBS> & vtCond,taf::JceCurrentPtr current)
{
	return eDbNotImplement;
}
taf::Int32 WDbAccessImp::delBS(const vector<taf::Char> & mainKey,const vector<DCache::DbConditionBS> & vtCond,taf::JceCurrentPtr current)
{
	return eDbNotImplement;
}
taf::Int32 WDbAccessImp::replace(const string &mainKey, const map<string, DbUpdateValue> &mpValue, 
								  const vector<DbCondition> &vtCond, taf::JceCurrentPtr current)
{
	//LOG->debug() <<"WDbAccessImp::replace:"<<mainKey<<endl;
	if(_isMKDBAccess)
	{
		return _mkDbopt.replace(mainKey, mpValue, vtCond);
	}
	else
	{
		return eDbNotImplement;
	}
}

taf::Int32 WDbAccessImp::del(const string &mainKey, const vector<DbCondition> &vtCond, taf::JceCurrentPtr current)
{
	//LOG->debug() <<"WDbAccessImp::del:"<<mainKey<<endl;
	if(_isMKDBAccess)
	{
		return _mkDbopt.del(mainKey, vtCond);
	}
	else
	{
		return eDbNotImplement;
	}
}

