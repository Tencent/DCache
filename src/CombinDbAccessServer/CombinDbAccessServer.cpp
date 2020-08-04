#include "CombinDbAccessServer.h"
#include "DbAccessImp.h"
// #include "WDbAccessImp.h"
//#include "util/tc_atomic.h"
#include "Globe.h"
#include "MysqlTimeOutHandle.h"

using namespace std;
using namespace DCache;
using namespace tars;

//PropertyReportPtr db_error;
//PropertyReportPtr db_error_timeout;
PropertyReportPtr g_largemk_count;
//一个变量对应一个连接示例
map<string,MysqlTimeOutHandle * > mysqlErrCounter;
map<string,DBInfo> mapDBInfo;
CheckMysqlTimeoutInfo * checkTimeoutInfoPtr;
unsigned int g_mkSizelimit; //二期的select主key下面条数的域值 如果超过则告警
bool g_bTypeErrorRepair; //二期当发现int类型但value非整型时，直接返回成功。
CombinDbAccessServer g_app;

/////////////////////////////////////////////////////////////////
void
CombinDbAccessServer::initialize()
{
	//initialize application here:
	//...
	addConfig(ServerConfig::ServerName + ".conf");
	addServant<DbAccessImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".DbAccessObj");
	// addServant<WDbAccessImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".WDbAccessObj");
	
	TARS_ADD_ADMIN_CMD_NORMAL("ver", CombinDbAccessServer::showVer);
	
	TC_Config tcConf;
	tcConf.parseFile(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");

	g_mkSizelimit = TC_Common::strto<unsigned int>(tcConf.get("/tars/<mkSizeLimit>","20000"));
	string strTypeErrorRepair = tcConf.get("/tars/<typeErrorRepair>","N");
	g_bTypeErrorRepair = (strTypeErrorRepair == "Y" || strTypeErrorRepair == "y")? true : false;
	checkTimeoutInfoPtr = new CheckMysqlTimeoutInfo();
	checkTimeoutInfoPtr->checkTimeoutInterval = TC_Common::strto<unsigned int>(tcConf.get("/tars/db_conn/<checkTimeoutInterval>","30"));
	checkTimeoutInfoPtr->frequenceFailInvoke = TC_Common::strto<unsigned int>(tcConf.get("/tars/db_conn/<frequenceFailInvoke>","10"));
	checkTimeoutInfoPtr->minTimeoutInvoke = TC_Common::strto<unsigned int>(tcConf.get("/tars/db_conn/<minTimeoutInvoke>","5"));
	checkTimeoutInfoPtr->radio = TC_Common::strto<unsigned int>(tcConf.get("/tars/db_conn/<radio>","30"));
	checkTimeoutInfoPtr->tryTimeInterval = TC_Common::strto<unsigned int>(tcConf.get("/tars/db_conn/<tryTimeInterval>","30"));
	checkTimeoutInfoPtr->check();

    //读取主db
	map<string,string> connInfo = tcConf.getDomainMap("/tars/Connection");
	for(map<string,string>::iterator itr=connInfo.begin();itr!=connInfo.end();itr++)
	{
		vector<string> tmpVec = TC_Common::sepstr<string>(itr->second,";");
		DBInfo tmpDBInfo;
		tmpDBInfo.ip=tmpVec[0];
		tmpDBInfo.port=TC_Common::strto<unsigned int>(tmpVec[4]);
		mysqlErrCounter.insert(make_pair("m"+itr->first,new MysqlTimeOutHandle(checkTimeoutInfoPtr,tmpVec[0],tmpVec[4])));
		mapDBInfo.insert(make_pair("m"+itr->first,tmpDBInfo));
	}

    //读取从db
    connInfo = tcConf.getDomainMap("/tars/ConnectionSlave");
	for(map<string,string>::iterator itr=connInfo.begin();itr!=connInfo.end();itr++)
	{
		vector<string> tmpVec = TC_Common::sepstr<string>(itr->second,";");
		DBInfo tmpDBInfo;
		tmpDBInfo.ip=tmpVec[0];
		tmpDBInfo.port=TC_Common::strto<unsigned int>(tmpVec[4]);
		mysqlErrCounter.insert(make_pair("s"+itr->first,new MysqlTimeOutHandle(checkTimeoutInfoPtr,tmpVec[0],tmpVec[4])));
		mapDBInfo.insert(make_pair("s"+itr->first,tmpDBInfo));
	}

	/*db_error = Application::getCommunicator()->getStatReport()->createPropertyReport("DbAccessServer.DBERROR", PropertyReport::sum());
	db_error_timeout = Application::getCommunicator()->getStatReport()->createPropertyReport("DbAccessServer.DBTIMEOUT", PropertyReport::sum());*/
	g_largemk_count = Application::getCommunicator()->getStatReport()->createPropertyReport("DbAccessServer.LARGEMK_COUNT", PropertyReport::sum());
}
/////////////////////////////////////////////////////////////////

bool CombinDbAccessServer::showVer(const string& command, const string& params, string& result)
{
	result = "DBAccess_3.8.1_Normal";
	return true;
}

void
CombinDbAccessServer::destroyApp()
{
	//destroy application here:
	//...
}
/////////////////////////////////////////////////////////////////
int
main(int argc, char* argv[])
{
	try
	{
		g_app.main(argc, argv);
		g_app.waitForShutdown();
	}
	catch (std::exception& e)
	{
		cerr << "std::exception:" << e.what() << std::endl;
	}
	catch (...)
	{
		cerr << "unknown exception." << std::endl;
	}
	return -1;
}
/////////////////////////////////////////////////////////////////
