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
#ifndef _DCacheOptImp_H_
#define _DCacheOptImp_H_

#include "servant/Application.h"
#include "util/tc_common.h"
#include "util/tc_mysql.h"
#include "servant/AdminF.h"
#include "servant/Communicator.h"
#include "servant/CommunicatorFactory.h"

#include "Proxy.h"
#include "Router.h"
#include "Node.h"
#include "Property.h"
#include "DCacheOpt.h"
#include "ReleaseThread.h"

using namespace std;
using namespace tars;
using namespace DCache;


class DCacheOptImp;

struct DCacheOptException : public TC_Exception
{
    DCacheOptException(const std::string& s):TC_Exception(s){}
};


class DCacheOptImp : public DCache::DCacheOpt
{
public:

    //临时路由结构
    struct RouterInfo : public tars::TC_HandleBase
    {
        bool flags;//是否可用标记
        int fromPage;
        int toPage;
        string groupName;
    };
    typedef tars::TC_AutoPtr<RouterInfo> RouterInfoPtr;

    static void creatDBAccessTableThread(const string &dbIp, const string &dbUser, const string &dbPwd, const int dbPort, const string &dbName, const string &tableSql, const int number, bool bReplace);
public:
    DCacheOptImp(){}

    /**
    *
    */
    virtual ~DCacheOptImp() {}

    /**
    * 初始化
    */
    virtual void initialize();

    /**
    * 退出
    */
    virtual void destroy();

    /*
    * 安装新应用
    */
    virtual tars::Int32 installApp(const InstallAppReq & installReq, InstallAppRsp & instalRsp, tars::TarsCurrentPtr current);

    /*
    * 安装新模块
    */
    virtual tars::Int32 installKVCacheModule(const InstallKVCacheReq & kvCacheReq, InstallKVCacheRsp & kvCacheRsp, tars::TarsCurrentPtr current);
    virtual tars::Int32 installMKVCacheModule(const InstallMKVCacheReq & mkvCacheReq, InstallMKVCacheRsp & mkvCacheRsp, tars::TarsCurrentPtr current);

    /*
    * 发布服务
    */
    virtual tars::Int32 releaseServer(const vector<DCache::ReleaseInfo> & releaseInfo, ReleaseRsp & releaseRsp, tars::TarsCurrentPtr current);

    /*
    * 获取发布服务进度
    */
    virtual tars::Int32 getReleaseProgress(const ReleaseProgressReq & progressReq, ReleaseProgressRsp & progressRsp, tars::TarsCurrentPtr current);

    /*
    * 下线服务
    */
    virtual tars::Int32 uninstall4DCache(const UninstallReq & uninstallInfo, UninstallRsp & uninstallRsp, tars::TarsCurrentPtr current);

    /*
    * 获取下线服务进度
    */
    virtual tars::Int32 getUninstallPercent(const UninstallReq & uninstallInfo, UninstallProgressRsp & progressRsp, tars::TarsCurrentPtr current);

    /*
    * reload router
    */
    virtual tars::Bool reloadRouterConfByModuleFromDB(const std::string & sApp, const std::string & sModuleName, const std::string & sRouterServerName, std::string &errmsg,tars::TarsCurrentPtr current);

    /*
    * transfer cache
    */
    virtual tars::Int32 transferDCache(const TransferReq & req, TransferRsp & rsp, tars::TarsCurrentPtr current);

    /*
    * transfer to existed group
    */
    virtual tars::Int32 transferDCacheGroup(const TransferGroupReq & req, TransferGroupRsp & rsp, tars::TarsCurrentPtr current);

    /*
    * expand cache
    */
    virtual tars::Int32 expandDCache(const ExpandReq & expandReq, ExpandRsp & expandRsp, tars::TarsCurrentPtr current);

    /*
    * reduce cache
    */
    virtual tars::Int32 reduceDCache(const ReduceReq & reduceReq, ReduceRsp & reduceRsp, tars::TarsCurrentPtr current);

    /*
    * config transfer task
    */
    virtual tars::Int32 configTransfer(const ConfigTransferReq & req, ConfigTransferRsp & rsp, tars::TarsCurrentPtr current);

    /*
    * get module struct info
    */
    virtual tars::Int32 getModuleStruct(const ModuleStructReq & req,ModuleStructRsp & rsp,tars::TarsCurrentPtr current);

    /*
    * get router change(inlcude tarnsfer, expand and reduce)
    */
    virtual tars::Int32 getRouterChange(const RouterChangeReq & req,RouterChangeRsp & rsp,tars::TarsCurrentPtr current);

    /*
    * active-standby switch
    */
    virtual tars::Int32 switchServer(const SwitchReq & req, SwitchRsp & rsp, tars::TarsCurrentPtr current);

    /*
    * get switch info
    */
    virtual tars::Int32 getSwitchInfo(const SwitchInfoReq & req, SwitchInfoRsp & rsp, tars::TarsCurrentPtr current);

    /*
    * stop transfer
    */
    virtual tars::Int32 stopTransfer(const StopTransferReq& req, StopTransferRsp &rsp, tars::TarsCurrentPtr current);

    /*
    * restart transfer
    */
    virtual tars::Int32 restartTransfer(const RestartTransferReq& req, RestartTransferRsp &rsp, tars::TarsCurrentPtr current);

    /*
    * delete transfer record
    */
    virtual tars::Int32 deleteTransfer(const DeleteTransferReq& req, DeleteTransferRsp& rsp,tars::TarsCurrentPtr current);

    /*
    * recover mirror status
    */
    virtual tars::Int32 recoverMirrorStatus(const RecoverMirrorReq& req, RecoverMirrorRsp & rsp, tars::TarsCurrentPtr current);

    /*
    * get cache server list
    */
    virtual tars::Int32 getCacheServerList(const CacheServerListReq& req, CacheServerListRsp& rsp, tars::TarsCurrentPtr current);

    /*
    * 配置中心操作接口
    */
    virtual tars::Int32 addCacheConfigItem(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 updateCacheConfigItem(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 deleteCacheConfigItem(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getCacheConfigItemList(const CacheConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 addServerConfigItem(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 updateServerConfigItem(const ServerConfigReq& configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 updateServerConfigItemBatch(const vector<ServerConfigReq> & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 deleteServerConfigItem(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 deleteServerConfigItemBatch(const vector<ServerConfigReq> & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getServerNodeConfigItemList(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getServerConfigItemList(const ServerConfigReq & configReq, ConfigRsp & configRsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 queryProperptyData(const DCache::QueryPropReq & req,vector<DCache::QueryResult> &rsp,tars::TarsCurrentPtr current);

    int creatDBAccessTable(const string &serverName, const vector<DCache::RecordParam> & vtModuleRecord, const DCache::DBAccessConf & conf, bool bReplace, string &err);

    virtual tars::Int32 installDBAccess(const InstallDbAccessReq &req, InstallDbAccessRsp &rsp, tars::CurrentPtr current);

    virtual int loadCacheApp(vector<CacheApp> &cacheApps, tars::CurrentPtr current);

private:

    /**
    * 创建router库表及配置,子菜单等
    */
    int createRouterDBConf(const RouterParam &param, string &errmsg);

    /**
    * 创建router库表
    */
    int createRouterDB(const RouterParam &param, string &errmsg);

    /**
    * 创建router配置文件
    */
    int createRouterConf(const RouterParam &param, bool bRouterServerExist, bool bReplace, string & errmsg);

    /**
    * 服务是否已存在
    */
    bool hasServer(const string&sApp, const string &sServerName);

    /**
    * 创建Proxy配置文件
    */
    int createProxyConf(const string &sModuleName, const ProxyParam &stProxyParam, const RouterParam &stRouterParam, bool bReplace, string & errmsg);

    /**
    * 保存配置文件
    */
    int insertTarsConfigFilesTable(const string &sFullServerName, const string &sConfName, const string &sHost, const string &sConfig, const int level, int &id, bool bReplace, string & errmsg);

    /**
    * 保存配置文件
    */
    int insertTarsHistoryConfigFilesTable(const int iConfigId, const string &sConfig, bool bReplace, string & errmsg);

    /*
    * 服务信息保存到tars 库表
    */
    int insertTarsDb(const ProxyParam &stProxyParam, const RouterParam &stRouterParam, const std::string &sTarsVersion, const bool enableGroup, bool bReplace, string & errmsg);

    /*
    * 保存服务对应关系
    */
    int insertRelationAppTable(const string & appName, const string &proxyServer,const string &routerServer,const DCache::RouterParam & stRouter, bool bReplace, string & errmsg);

    void insertProxyRouter(const string &sProxyName,  const string &sRouterName, const string &sDbName, const string &sIp, const string& sModuleNameList, bool bReplace, string & errmsg);

    /*
    * 创建 kvcache配置
    */
    int createKVCacheConf(const string &appName, const string &moduleName, const string &routerName, const vector<DCache::CacheHostParam> & cacheHost, const DCache::SingleKeyConfParam & kvConf, bool bReplace, string & errmsg);

    /*
    * 创建 mkvcache配置
    */
    int createMKVCacheConf(const string &appName, const string &moduleName, const string &routerName,const vector<DCache::CacheHostParam> & vtCacheHost,const DCache::MultiKeyConfParam & mkvConf, const vector<RecordParam> &vtRecordParam, bool bReplace, string & errmsg);

    /**
    * 生成cache在router表的记录
    */
    int insertCache2RouterDb(const string& sModuleName, const string &sRemark, const TC_DBConf &routerDbInfo, const vector<DCache::CacheHostParam> & vtCacheHost, bool bReplace, string& errmsg);

    /**
    * 保存cache信息到tars db
    */
    int insertCache2TarsDb(const TC_DBConf &routerDbInfo, const vector<DCache::CacheHostParam> & vtCacheHost, DCache::DCacheType eCacheType, const string &sTarsVersion, bool bReplace, std::string &errmsg);

    /**
    * 保存cache信息到tars server 表
    */
    int insertTarsServerTable(const string &sApp, const string &sServerName, const string &sIp, const string &sTemplateName, const string &sExePath,const std::string &sTarsVersion, const bool enableGroup, bool bReplace, string & errmsg);

    /**
    * 保存cache信息到tars servant 表
    */
    int insertServantTable(const string &sApp, const string &sServerName, const string &sIp, const string &sServantName, const string &sEndpoint, const string &sThreadNum, bool bReplace, string & errmsg);

    void insertConfigItem2Vector(const string& sItem, const string &sPath,const string &sConfigValue, vector < map <string, string> > &vtConfig);

    /**
    *查找item对应的item_id,插入config_table时需要这个值
    */
    int matchItemId(const string &item, const string &path, string &item_id);

    /**
    *应用名和模块名来确定level1的id
    */
    int insertAppModTable(const string &appName, const string &moduleName, int &id, bool bReplace, string & errmsg);
    int insertAppModTable(const string &appName, const string &moduleName, DCache::DCacheType eCacheType, int &id, bool bReplace, string & errmsg);

    /**
    *重载insertConfigFilesTable
    */
    int insertConfigFilesTable(const string &sFullServerName, const string &sHost, const int config_id, vector< map<string,string> > &myVec, const int level, bool bReplace, string & errmsg);

    /**
    *
    */
    int insertReferenceTable(const int appConfigID, const string& server_name, const string& host, bool bReplace, string & errmsg);

    /**
    * 查询cache的port信息
    */
    int selectPort(TC_Mysql &tcMysql, const string &sServerName, const string &sIp, string &sBinLogPort, string &sCachePort, string &sRouterPort,string &sBackUpPort,string &sWCachePort,string &sControlAckPort, std::string &errmsg);

    /**
    * 检查DB是否存在
    */
    int checkDb(TC_Mysql &tcMysql, const string &sDbName);

    /**
    * 检查表是否存在
    */
    int checkTable(TC_Mysql &tcMysql, const string &sDbName, const string &sTableName);

    /**
    * 保存proxy和cache的关系
    */
    void insertProxyCache(const string &sProxyObj, const string & sCacheName, bool bReplace);

    bool checkIP(const string &sIp);
    uint16_t getPort(const string &sIp, uint16_t iPort = 0);
    uint16_t getRand();


    // 检查router加载模块路由是否成功
    bool checkRouterLoadModule(const std::string & sApp, const std::string & sModuleName, const std::string & sRouterServerName, std::string &errmsg);

    // 重载路由信息
    bool reloadRouterByModule(const std::string & sApp, const std::string & sModuleName, const std::string & sRouterServerName, const set<string>& nodeIPset, std::string &errmsg);

    int reloadRouterByModule(const std::string & app, const std::string & moduleName, const std::string & routerName, std::string &errmsg);

    int insertCacheRouter(const string &sCacheName, const string &sCacheIp, int memSize, const string &sRouterName, const TC_DBConf &routerDbInfo, const string& sModuleName, bool bReplace, string & errmsg);

    // 插入到t_server_conf 里面默认IDC分组
    int insertTarsServerTableWithIdcGroup(const string &sApp, const string& sServerName ,const ProxyAddr &stProxyAddr, const string &sIp, const string &sTemplateName, const string &sExePath, const std::string &sTarsVersion, const bool enableGroup, bool bReplace, string & errmsg);

    int expandCacheConf(const string &appName, const string &moduleName, const vector<DCache::CacheHostParam> & vtCacheHost, bool bReplace, string & errmsg);

    int getAppModConfigId(const string &appName, const string &moduleName, int &id, string & errmsg);

    int expandCache2RouterDb(const string &sModuleName, const TC_DBConf &routerDbInfo, const vector<DCache::CacheHostParam> & vtCacheHost, bool bReplace, string& errmsg);

    int getRouterObj(const string &sWhereName, string &routerObj, string & errmsg, int type = 1);

    int getRouterDBInfo(const string &appName, TC_DBConf &routerDbInfo, string& errmsg);

    int getRouterDBFromAppTable(const string &appName, TC_DBConf &routerDbInfo, vector<string> & sProxyName, string & sRouterName, string &errmsg);

    int expandDCacheServer(const std::string & appName,const std::string & moduleName,const vector<DCache::CacheHostParam> & vtCacheHost,const std::string & sTarsVersion,DCache::DCacheType cacheType, bool bReplace,std::string &errmsg);

    int insertTransferStatusRecord(const std::string & appName,const std::string & moduleName,const std::string & srcGroupName,const std::string & dstGroupName,bool transferExisted,std::string &errmsg);

    int insertExpandReduceStatusRecord(const std::string& appName, const std::string& moduleName, TransferType type, const vector<string> & groupName, std::string& errmsg);

    int insertTransferRecord2RouterDb(TC_Mysql &tcMysql, const string &module, const string &srcGroup, const string &destGroup, unsigned int fromPage, unsigned int toPage, string &ids, std::string& errmsg);

    int insertTransfer2RouterDb(const std::string & appName,const std::string & moduleName,const std::string & srcGroupName,const std::string & dstGroupName,bool transferData,std::string &errmsg);

    int insertExpandTransfer2RouterDb(const std::string & appName,const std::string & moduleName,const vector<std::string> & dstGroupNames,std::string &errmsg);

    int insertReduceTransfer2RouterDb(const std::string& appName, const std::string& moduleName, const std::vector<std::string> &srcGroupNames, std::string& errmsg);

    int getCacheConfigFromDB(const string &serverName, const string &appName, const string &moduleName, const string &groupName, string &sEnableErase, string &sDBFlag, string &sMemSize, string & errmsg);

    int stopTransferForTransfer(const std::string & appName,const std::string & moduleName,const std::string & srcGroupName,const std::string & dstGroupName,std::string &errmsg);

    int stopTransferForExpandReduce(const std::string & appName,const std::string & moduleName,TransferType type,std::string &errmsg);

    int restartTransferForExpandReduce(const std::string & appName,const std::string & moduleName,TransferType type,std::string &errmsg);

    int deleteTransferForTransfer(const std::string & appName,const std::string & moduleName,const std::string & srcGroupName,const std::string & dstGroupName,std::string &errmsg);

    int deleteTransferForExpandReduce(const std::string & appName,const std::string & moduleName,TransferType type,std::string &errmsg);

    int getCacheGroupRouterPageNo(TC_Mysql &tcMysql, const string& groupName, long& groupPageNo, string& errmsg);

    int createDBAccessConf(int type, bool isSerializatedconst, const vector<DCache::RecordParam> & vtModuleRecord, const DCache::DBAccessConf & conf, string &result, string &err);

    int tarsServerConf(const string &appName, const string &serverName, const vector<string> &vServerIp, const string &templateName, const string &templateDetail, bool bReplace, string &err);

    int getShmKey(size_t &shmKey);

private:

    TC_Config _tcConf;

    TC_Mysql _mysqlTarsDb;

    CommunicatorPtr _communicator;
    AdminRegPrx     _adminproxy;

    PropertyPrx     _propertyPrx;

    //专门用于建表建库的线程池
    TC_ThreadPool _tpool;

    //dcache 关系变量
    map<string, string> _relationDBInfo;
    TC_Mysql _mysqlRelationDB;

};

/////////////////////////////////////////////////////
#endif
