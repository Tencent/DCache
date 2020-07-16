#ifndef _GLOBE_H
#define _GLOBE_H
#include "servant/Application.h"
#include "MysqlTimeOutHandle.h"
#include "CombinDbAccessServer.h"
#include "tc_mysql.h"
#include <map>

using namespace std;

#ifndef DB_LOG
#define DB_LOG  FDLOG("DB_TIME_INFO")
#endif

#define CDB_MASTER 1
#define CDB_SLAVE 2
#define CDB_ALL 3

struct CheckMysqlTimeoutInfo
{
    unsigned int minTimeoutInvoke;

    unsigned int checkTimeoutInterval;

    unsigned int frequenceFailInvoke;

    unsigned int radio;

    unsigned int tryTimeInterval;

    CheckMysqlTimeoutInfo() : minTimeoutInvoke(5), checkTimeoutInterval(60), frequenceFailInvoke(10), radio(30), tryTimeInterval(300) {}

	void check()
	{
		if(minTimeoutInvoke<5)
			minTimeoutInvoke = 5;
		if(checkTimeoutInterval<60)
			checkTimeoutInterval = 60;
		if(frequenceFailInvoke<5)
			frequenceFailInvoke = 10;
		if(tryTimeInterval<200)
			tryTimeInterval = 300;
	}
};
//上报db stat用 实例的ip和port
struct DBInfo
{
	string ip;
	unsigned int port;
};

struct FieldInfo
{
	string FieldName;
	uint8_t tag;
	string type;
	bool bRequire;
	string defValue;
};
/**
 * 
 * 默认生成的分表代码，可以自己重写
 */
class DefaultTableNameGen
{
public:
	DefaultTableNameGen(){}
	~DefaultTableNameGen(){}
	void initialize(const string &sConf);
    string getTableName(const string &k);
	/*
    string getTableName(taf::Int64 k);
    string getTableName(taf::Int32 k);
	*/
	string getTablePostfix(size_t postfix);

private:
	unsigned int _iPostfixLen;
	int _div;
	string _sTablePrefix;
    bool _isHashForLocateDB;
};

/**
*默认生成的获取数据库连接代码，可以自己重写
*/
class DefaultDbConnGen
{
public:
	DefaultDbConnGen(){}
	~DefaultDbConnGen()
	{
        for(unsigned int i = 0; i < 2; i++)
		for(map<string, TC_Mysql*>::iterator it=_mpMysql[i].begin(); it!=_mpMysql[i].end(); it++)
		{
			delete it->second;
		}
	}
	void initialize(const string &sConf);
    TC_Mysql* getDbConn(const string &k, const string &opType, string &sDbName,string &mysqlNum);
	/*int timeOutSet(const string &mysqlNum);
	int timeOutClean(const string &mysqlNum);*/
	string getDbPostfix(size_t postfix);

private:
	vector<map<string, TC_Mysql*> > _mpMysql;
	map<string, string> _mpConn;
	unsigned int _iPostfixLen;
	unsigned int _iPos;
	int _div;
	unsigned int _retryDBInterval;
	unsigned int _failTimeFobid;
	int _conTimeOut;
	int _queryTimeOut;
	string _sDbPrefix;
	bool _isHashForLocateDB;

    int _readFlag;//标识cdb主从是否可读
    int _writeFlag;//标识cdb主从是否可写
};
bool sortByTag(const FieldInfo &v1,const FieldInfo &v2);
size_t HashRawString(const string &key);
bool IsDigit(const string &key);
class MysqlTimeOutHandle;
extern map<string,MysqlTimeOutHandle * > mysqlErrCounter;
extern map<string,DBInfo> mapDBInfo;
//extern PropertyReportPtr db_error;
//extern PropertyReportPtr db_error_timeout;
extern PropertyReportPtr g_largemk_count;
extern CheckMysqlTimeoutInfo * checkTimeoutInfoPtr;
extern unsigned int g_mkSizelimit;
extern bool g_bTypeErrorRepair;
#endif

