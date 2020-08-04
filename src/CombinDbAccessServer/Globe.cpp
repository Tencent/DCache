#include "Globe.h"

//cdb主备选择原子标记
int g_cdbMS;

void DefaultTableNameGen::initialize(const string &sConf)
{
	TC_Config tcConf;
	tcConf.parseFile(sConf);
	_iPostfixLen = TC_Common::strto<unsigned int>(tcConf["/tars/table/<PostfixLen>"]);
	_div = TC_Common::strto<int>(tcConf["/tars/table/<Div>"]);
	_sTablePrefix = tcConf["/tars/table/<TableNamePrefix>"];
	string sHashForLocateDB = tcConf.get("/tars/<isHashForLocateDB>", "N");
	_isHashForLocateDB = (sHashForLocateDB=="Y" || sHashForLocateDB=="y")?true:false;
}


string DefaultTableNameGen::getTableName(const string &k)
{
	size_t iPostfix;
	//key是否经过hash再进行表匹配
	if(!_isHashForLocateDB)
	{
		string sPostfix;
		if( k.length() <= _iPostfixLen )
		{
			sPostfix = k;
		}
		else
		{
			int iBegin = k.length() - _iPostfixLen;
			sPostfix = k.substr(iBegin, _iPostfixLen);
		}
		if(!IsDigit(sPostfix))
		{
			throw runtime_error("DefaultTableNameGen::getTableName sPostfix is not digit");
		}
		iPostfix = TC_Common::strto<size_t>(sPostfix);
	}
	else
	{
		iPostfix = HashRawString(k);
	    LOG->debug() <<int(iPostfix)<< endl;
	}
	string sTablePostfix = getTablePostfix(iPostfix);
	return _sTablePrefix + sTablePostfix;
}

string DefaultTableNameGen::getTablePostfix(size_t postfix)
{
	size_t iTablePostfix = postfix % _div;
	string sTablePostfix = TC_Common::tostr(iTablePostfix);
	if(sTablePostfix.length() < _iPostfixLen)
	{
		string sTmp;
		sTmp.assign(_iPostfixLen - sTablePostfix.length(), '0');
		sTablePostfix = sTmp + sTablePostfix;
	}
	return sTablePostfix;
}

void DefaultDbConnGen::initialize(const string &sConf)
{
	TC_Config tcConf;
	tcConf.parseFile(sConf);
	map<string,string> connInfo = tcConf.getDomainMap("/tars/Connection");
	_conTimeOut = TC_Common::strto<int>(tcConf.get("/tars/db_conn/<conTimeOut>","3"));
	_queryTimeOut = TC_Common::strto<int>(tcConf.get("/tars/db_conn/<queryTimeOut>","3"));

    string sTimeoutRetry = tcConf.get("/tars/<timeoutRetry>","N");
    bool bTimeoutRetry;
    if((sTimeoutRetry == "y") || (sTimeoutRetry == "Y"))
        bTimeoutRetry = true;
    else
        bTimeoutRetry = false;

    //主从cdb读写标记
    _writeFlag = _readFlag = 0;
    string sReadFlag = tcConf.get("/tars/<readAble>","m");
    string sWriteFlag = tcConf.get("/tars/<writeAble>","m");

    vector<string> vFlag = TC_Common::sepstr<string>(sReadFlag, "|");
    for(unsigned int i = 0; i < vFlag.size(); i++)
    {
        string s = TC_Common::trim(vFlag[i]);
        if(s == "m")
            _readFlag += CDB_MASTER;
        else if(s == "s")
            _readFlag += CDB_SLAVE;
    }

    vFlag = TC_Common::sepstr<string>(sWriteFlag, "|");
    for(unsigned int i = 0; i < vFlag.size(); i++)
    {
        //只能允许有一个写
        string s = TC_Common::trim(vFlag[i]);
        if(s == "m")
            _writeFlag = CDB_MASTER;
        else if((s == "s") && (_writeFlag == 0))
            _writeFlag = CDB_SLAVE;
    }

    _mpMysql.push_back(map<string, TC_Mysql*>());
	for(map<string,string>::iterator it=connInfo.begin(); it != connInfo.end(); it++)
	{
		vector<string> vtDB = TC_Common::sepstr<string>(it->second, ";", true);
		TC_Mysql* pMysql = new TC_Mysql();
		pMysql->init(TC_Common::trim(vtDB[0]), TC_Common::trim(vtDB[1]), TC_Common::trim(vtDB[2]), "", TC_Common::trim(vtDB[3]), TC_Common::strto<int>(TC_Common::trim(vtDB[4])),0,_conTimeOut,_queryTimeOut);
        pMysql->setTimeoutRetry(bTimeoutRetry);
        _mpMysql[0][it->first] = pMysql;
		LOG->debug()<<it->second<< endl;
	}

    _mpMysql.push_back(map<string, TC_Mysql*>());
    connInfo = tcConf.getDomainMap("/tars/ConnectionSlave");
    for(map<string,string>::iterator it=connInfo.begin(); it != connInfo.end(); it++)
	{
		vector<string> vtDB = TC_Common::sepstr<string>(it->second, ";", true);
		TC_Mysql* pMysql = new TC_Mysql();
		pMysql->init(TC_Common::trim(vtDB[0]), TC_Common::trim(vtDB[1]), TC_Common::trim(vtDB[2]), "", TC_Common::trim(vtDB[3]), TC_Common::strto<int>(TC_Common::trim(vtDB[4])),0,_conTimeOut,_queryTimeOut);
        pMysql->setTimeoutRetry(bTimeoutRetry);
        _mpMysql[1][it->first] = pMysql;
		LOG->debug()<<it->second<< endl;
	}
	
	map<string,string> _mpConnTmp = tcConf.getDomainMap("/tars/DataBase");
	for(map<string,string>::iterator it=_mpConnTmp.begin(); it != _mpConnTmp.end(); it++)
	{
		vector<string> vtNum = TC_Common::sepstr<string>(it->first, "-", true);
		if(vtNum.size()==1)
		{
		   _mpConn[it->first] = it->second;
		   LOG->debug() <<it->first<< endl;
		}
		else if(vtNum.size() ==2)
		{
			int iBey =  TC_Common::strto<int>(vtNum[0].substr(1));
			size_t tmpLength = vtNum[0].substr(1).length();
			int iEnd =  TC_Common::strto<int>(vtNum[1].substr(0,vtNum[1].size()-1));
			for(int i=iBey;i<=iEnd;i++)
			{
				string sMapKey = TC_Common::tostr(i);
				if(sMapKey.length() < tmpLength)
				{
					string sTmp;
					sTmp.assign(tmpLength -sMapKey.length(), '0');
					sMapKey = sTmp + sMapKey;
				}
				_mpConn[sMapKey] = it->second;
				LOG->debug() <<sMapKey<< endl;
			}
		}
		else
		{
			LOG->error() <<"/tars/DataBase type error  [0-9] or 0"<< endl;
		    assert(false);
		}
	}
	_iPostfixLen = TC_Common::strto<unsigned int>(tcConf["/tars/db_conn/<PostfixLen>"]);
	_iPos = TC_Common::strto<unsigned int>(tcConf["/tars/db_conn/<PosInKey>"]);
	_div = TC_Common::strto<int>(tcConf["/tars/db_conn/<Div>"]);
	_sDbPrefix = tcConf["/tars/db_conn/<DbPrefix>"];
	string sHashForLocateDB = tcConf.get("/tars/<isHashForLocateDB>", "N");
	_isHashForLocateDB = (sHashForLocateDB=="Y" || sHashForLocateDB=="y")?true:false;
	_retryDBInterval = TC_Common::strto<unsigned int>(tcConf.get("/tars/db_conn/<retryDBInterval>","300"));
	_failTimeFobid = TC_Common::strto<unsigned int>(tcConf.get("/tars/db_conn/<failTimeFobid>","10"));
}

TC_Mysql* DefaultDbConnGen::getDbConn(const string &k, const string &opType, string &sDbName,string &mysqlNum)
{
	size_t iPostfix;
	TLOGDEBUG("key:" << k << "type:"<< opType <<endl);
	//key是否经过hash再进行库匹配
	if(!_isHashForLocateDB)
	{
		string sPostfix;
		if( k.length() <= _iPostfixLen )
		{
			sPostfix = k;
		}
		else
		{
			if(int(k.size()-_iPos)<0)
			{
				sPostfix = k.substr(k.size()-_iPostfixLen, _iPostfixLen);;
				//throw runtime_error("DefaultDbConnGen::getDbConn _iPos too long");
			}
			else if((k.size()-_iPos+_iPostfixLen)>(k.size()))
			{
				throw runtime_error("DefaultDbConnGen::getDbConn k.size()-_iPos+_iPostfixLen too long");
			}
			else
			{
			   sPostfix = k.substr(k.size()-_iPos, _iPostfixLen);
			}
		}
	
		if(!IsDigit(sPostfix))
		{
			string tmpKey = k;
			tmpKey += "DefaultDbConnGen::getDbConn sPostfix is not digit";
			throw runtime_error(tmpKey.c_str());
		}

		iPostfix = TC_Common::strto<size_t>(sPostfix);
	}
	else
	{
		string sPostfix;
		size_t iHashKey = HashRawString(k);
		string sHashKey = TC_Common::tostr(iHashKey);
		int tmpPos = _iPos;
		int tmpFixLen = _iPostfixLen;
		int length = sHashKey.size();
		LOG->debug() <<"key:"<<k<<"| hashkey:"<<sHashKey<<"|"<<"hashkeylen:"<<length<<"|pos:"<<tmpPos<<"|FixLen:"<<tmpFixLen<<endl;
		if( sHashKey.length() <= _iPostfixLen )
		{
			sPostfix = sHashKey;
		}
		else
		{
			if(int(sHashKey.size()-_iPos)<0)
			{
				//throw runtime_error("DefaultDbConnGen::getDbConn _iPos too long");
				sPostfix = sHashKey.substr(sHashKey.size()-_iPostfixLen, _iPostfixLen);
			}
			else if((sHashKey.size()-_iPos+_iPostfixLen)>sHashKey.size())
			{
				throw runtime_error("DefaultDbConnGen::getDbConn k.size()-_iPos+_iPostfixLen too long");
			}
			else
			{
			    sPostfix = sHashKey.substr(sHashKey.size()-_iPos, _iPostfixLen);
				//LOG->debug() <<"key:"<<k<<"|"<<"hashkeylen:"<<length<<"|pos:"<<tmpPos<<"|FixLen:"<<tmpFixLen<<endl;
			}
		}
		iPostfix = TC_Common::strto<size_t>(sPostfix);
	}
	string sDbPostfix = getDbPostfix(iPostfix);
	sDbName = _sDbPrefix + sDbPostfix;
	map<string, string>::iterator iter = _mpConn.find(sDbPostfix);
	if(iter == _mpConn.end())
		return NULL;

    //选择cdb主备
    int opFlag;//这次可以操作主从标记
    if(opType == "r")
        opFlag = _readFlag;
    else if(opType == "w")
        opFlag = _writeFlag;
    else 
        return NULL;

    switch(opFlag)
    {
    case CDB_MASTER://只操作主机
        {
            map<string, TC_Mysql*>::iterator iter2 = _mpMysql[0].find(iter->second);
        	if(iter2 == _mpMysql[0].end())
        		return NULL;
        	mysqlNum = "m"+iter2->first;
        	if((mysqlErrCounter[mysqlNum])->checkActive())
        	{
        		return iter2->second;
        	}
        	else
        	{
        		return NULL;
        	}
        }
        break;
    case CDB_SLAVE://只操作从机
        {
            map<string, TC_Mysql*>::iterator iter2 = _mpMysql[1].find(iter->second);
        	if(iter2 == _mpMysql[1].end())
        		return NULL;
        	mysqlNum = "s"+iter2->first;
        	if((mysqlErrCounter[mysqlNum])->checkActive())
        	{
        		return iter2->second;
        	}
        	else
        	{
        		return NULL;
        	}
        }
        break;
    case CDB_ALL:
        {
            int realFlag = 0;//实际可操作的主从标记

            map<string, TC_Mysql*>::iterator mit = _mpMysql[0].find(iter->second);
            if((mit != _mpMysql[0].end()) && ((mysqlErrCounter["m"+mit->first])->checkActive() == true))
                realFlag += CDB_MASTER;

            map<string, TC_Mysql*>::iterator sit = _mpMysql[1].find(iter->second);
            if((sit != _mpMysql[1].end()) && ((mysqlErrCounter["s"+sit->first])->checkActive() == true))
                realFlag += CDB_SLAVE;

            switch(realFlag)
            {
            case CDB_MASTER:
                {
                    mysqlNum = "m" + mit->first;
                    return mit->second;
                }
                break;
            case CDB_SLAVE:
                {
                    mysqlNum = "s" + sit->first;
                    return sit->second;
                }
                break;
            case CDB_ALL:
                {
                    bool b = __sync_fetch_and_xor(&g_cdbMS, 1);

                    if(b)
                    {
                        mysqlNum = "m" + mit->first;
                        return mit->second;
                    }
                    else
                    {
                        mysqlNum = "s" + sit->first;
                        return sit->second;
                    }
                }
                break;
            default:return NULL;
            }
        }
    default:return NULL;
    }

    return NULL;
}

string DefaultDbConnGen::getDbPostfix(size_t postfix)
{
	size_t iDbPostfix = postfix / _div;
	string sDbPostfix = TC_Common::tostr(iDbPostfix);
	LOG->debug()<<"last: "<<sDbPostfix<< endl;
	if(sDbPostfix.length() < _iPostfixLen)
	{
		string sTmp;
		sTmp.assign(_iPostfixLen - sDbPostfix.length(), '0');
		sDbPostfix = sTmp + sDbPostfix;
	}
	else if(sDbPostfix.length() > _iPostfixLen)
	{
		sDbPostfix = sDbPostfix.substr(0,_iPostfixLen);
	}
	return sDbPostfix;
}
bool sortByTag(const FieldInfo &v1,const FieldInfo &v2)
{
   return v1.tag<v2.tag;
}
size_t HashRawString(const string &key)
{
    const char *ptr= key.c_str();
    size_t key_length = key.length();
    uint32_t value= 0;

    while (key_length--) 
    {
        value += *ptr++;
        value += (value << 10);
        value ^= (value >> 6);
    }

    value += (value << 3);
    value ^= (value >> 11);
    value += (value << 15); 

    return value == 0 ? 1 : value;
}
bool IsDigit(const string &key)
{
	string::const_iterator iter = key.begin();

	

    if(key.empty())

    {

        return false;

    }

	size_t length = key.size();

	size_t count = 0;

	//是否找到小数点 
	bool bFindNode = false;

	bool bFirst = true;

    while(iter != key.end())

    {

		if(bFirst)

		{

		   if(!::isdigit(*iter))

		   {

			   if(*iter!=45)

			   {

				   return false;

			   }

			   else if(length==1)

			   {

				   return false;

			   }

			   

		   }

		}

		else

		{

			if (!::isdigit(*iter))

			{

				if(*iter==46)

				{

					if( (!bFindNode) && (count!=(length-1)) )

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

