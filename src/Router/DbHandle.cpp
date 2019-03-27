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
#include "RouterServer.h"
#include "SwitchThread.h"
#include "util/tc_encoder.h"

extern RouterServer g_app;

DbHandle::DbHandle()
    : _mapPackTables(NULL), _mapPackTables1(NULL), _lastLoadTime(0), _reloadTime(100000)
{
}

DbHandle::~DbHandle()
{
    delete _mapPackTables;
    _mapPackTables = NULL;

    delete _mapPackTables1;
    _mapPackTables1 = NULL;
}

void DbHandle::init(int64_t reloadTime,
                    std::unique_ptr<MySqlHandlerWrapper> mysql,
                    std::unique_ptr<MySqlHandlerWrapper> mysqlDBRelation,
                    std::unique_ptr<MySqlHandlerWrapper> mysqlMigrate,
                    PropertyReportPtr report)
{
    _reloadTime = reloadTime;
    // 初始化数据库连接
    _mysql = std::move(mysql);
    _mysqlDBRelation = std::move(mysqlDBRelation);
    _mysqlMigrate = std::move(mysqlMigrate);
    _exRep = report;
}

void DbHandle::initCheck()
{
    int iRet = 0;

    TC_ThreadLock::Lock lock(_transLock);
    if (!hasTransfering())  // 确保当前没有迁移在进行
    {
        iRet = checkRoute();

        if (iRet < 0)
        {
            RUNTIME_ERROR("check router config from db error!");
        }

        iRet = loadRouteToMem();
    }
    else
    {
        iRet = 1;
    }

    if (iRet < 0)
    {
        RUNTIME_ERROR("load router config from db error!");
    }
    else if (iRet > 0)
    {
        RUNTIME_ERROR("tranfering, don't load router config from db!");
    }
}

int DbHandle::reloadRouter()
{
    int iRet;
    TC_ThreadLock::Lock lock(_transLock);

    if (!hasTransfering())  //此处判断是应该限制所有模块  ??????
    {
        vector<RecordInfo> vRecords = getDBRecordList();
        for (size_t i = 0; i < vRecords.size(); ++i)
        {
            if ((vRecords.size() - 1) == i)
            {
                if (vRecords[i].toPageNo != 429496)
                {
                    TLOGERROR("[DbHandle::reloadRouter] error! "
                              << vRecords[i].moduleName << " is not end with 429496!" << endl);
                    return -1;
                }
            }

            if (0 == i)
            {
                if (vRecords[i].fromPageNo != 0)
                {
                    TLOGERROR("[DbHandle::reloadRouter] error! "
                              << vRecords[i].moduleName << " is not start with 0!" << endl);
                    return -1;
                    ;
                }
                continue;
            }

            if (vRecords[i - 1].moduleName != vRecords[i].moduleName)
            {  //如果不相等，说明是另外一个模块了，检查是否结束了
                if (vRecords[i - 1].toPageNo != 429496)
                {
                    TLOGERROR("[DbHandle::reloadRouter] error! "
                              << vRecords[i - 1].moduleName << " is not end with 429496!" << endl);
                    return -1;
                }

                if (vRecords[i].fromPageNo != 0)
                {
                    TLOGERROR("[DbHandle::reloadRouter] error! "
                              << vRecords[i].moduleName << " is not start with 0!" << endl);
                    return -1;
                }

                continue;
            }

            if ((vRecords[i - 1].toPageNo + 1) != vRecords[i].fromPageNo)
            {
                TLOGERROR("[DbHandle::reloadRouter] error! "
                          << vRecords[i].moduleName << " " << vRecords[i - 1].toPageNo
                          << "+1 != " << vRecords[i].fromPageNo << endl);
                return -1;
            }
        }

        if (g_app.switchingModuleNum() != 0)
        {
            TLOGDEBUG(FILE_FUN << "DbHandle::reloadRouter hasSwitching" << endl);
            return 1;
        }

        iRet = loadRouteToMem();
    }
    else
    {
        TLOGDEBUG(FILE_FUN << "DbHandle::reloadRouter hasTransfering" << endl);
        iRet = 1;
    }

    return iRet;
}

int DbHandle::reloadRouter(const string &moduleName)
{
    int iRet;
    TC_ThreadLock::Lock lock(_transLock);
    TLOGDEBUG(FILE_FUN << "DbHandle::reloadRouter moduleName:" << moduleName << endl);
    if (!hasTransfering(moduleName))  //此处判断是应该限制所有模块  ??????
    {
        //检查路由是否完整
        vector<RecordInfo> vRecords = getDBRecordList(moduleName);
        for (size_t i = 0; i < vRecords.size(); ++i)
        {
            if ((0 == i) && (vRecords[i].fromPageNo != 0))
            {
                TLOGERROR("[DbHandle::reloadRouter] error! " << vRecords[i].moduleName
                                                             << " is not start with 0!" << endl);
                return -1;
            }

            if (((vRecords.size() - 1) == i) && (vRecords[i].toPageNo != 429496))
            {
                TLOGERROR("[DbHandle::reloadRouter] error! " << vRecords[i].moduleName
                                                             << " is not end with 429496!" << endl);
                return -1;
            }

            if (i > 0)
            {
                if ((vRecords[i - 1].toPageNo + 1) != vRecords[i].fromPageNo)
                {
                    TLOGERROR("[DbHandle::reloadRouter] error! "
                              << vRecords[i].moduleName << " " << vRecords[i - 1].toPageNo
                              << "+1 != " << vRecords[i].fromPageNo << endl);
                    return -1;
                }
            }
        }

        if (g_app.switchingModuleNum() != 0)
        {
            TLOGDEBUG(FILE_FUN << "DbHandle::reloadRouter hasSwitching" << endl);
            return 1;
        }

        iRet = loadRouteToMem(moduleName);
        TLOGDEBUG(FILE_FUN << "DbHandle::reloadRouter moduleName:" << moduleName << " iRet:" << iRet
                           << endl);
    }
    else
    {
        TLOGDEBUG(FILE_FUN << "DbHandle::reloadRouter hasTransfering moduleName:" << moduleName
                           << endl);
        iRet = 1;
    }

    return iRet;
}

void DbHandle::reloadConf(const RouterServerConfig &conf)
{
    TLOGDEBUG(FILE_FUN << "DbHandle reload config ..." << endl);
    _reloadTime = conf.getDbReloadTime(100000);
}

int DbHandle::checkRoute()
{
    try
    {
        vector<RecordInfo> vRecords = getDBRecordList();
        RecordInfo ri;
        vector<RecordInfo> deleteRecord;
        for (size_t i = 0; i < vRecords.size(); ++i)
        {
            if ((vRecords.size() - 1) == i)
            {
                if (vRecords[i].toPageNo != 429496)
                {
                    TLOGERROR("[DbHandle::checkRoute] error! "
                              << vRecords[i].moduleName << " is not end with 429496!" << endl);
                    return -1;
                }
            }

            if (0 == i)
            {
                ri = vRecords[i];
                continue;
            }

            if (ri.moduleName != vRecords[i].moduleName)
            {  //如果不相等，说明是另外一个模块了，检查是否结束了
                if (ri.toPageNo != 429496)
                {
                    TLOGERROR("[DbHandle::checkRoute] error! "
                              << ri.moduleName << " is not end with 429496!" << endl);
                    return -1;
                }

                ri = vRecords[i];

                if (ri.fromPageNo != 0)
                {
                    TLOGERROR("[DbHandle::checkRoute] error! " << ri.moduleName
                                                               << " is not start with 0!" << endl);
                    return -1;
                }
            }
            else
            {
                if ((ri.toPageNo + 1) != vRecords[i].fromPageNo)
                {  //如果接不上，正常是有重复的路由
                    if (ri.groupName != vRecords[i].groupName)
                    {
                        TLOGERROR("[DbHandle::checkRoute] router error! "
                                  << vRecords[i].moduleName << " from:" << vRecords[i].fromPageNo
                                  << " topage:" << vRecords[i].toPageNo
                                  << "|ri.groupName:" << ri.groupName
                                  << "|vRecords[i].groupName:" << vRecords[i].groupName << endl);
                        return -1;
                    }

                    if (ri.fromPageNo == vRecords[i].fromPageNo)
                    {
                        if (ri.toPageNo < vRecords[i].toPageNo)
                        {  //表示vRecords[i]是覆盖ri里的路由信息
                            deleteRecord.push_back(ri);
                            ri = vRecords[i];
                        }
                        else
                        {
                            deleteRecord.push_back(vRecords[i]);
                        }
                    }
                    else
                    {
                        if ((vRecords[i].toPageNo > ri.toPageNo) ||
                            (vRecords[i].fromPageNo < ri.fromPageNo))
                        {
                            TLOGERROR("[DbHandle::checkRoute] router error! "
                                      << vRecords[i].moduleName
                                      << " from:" << vRecords[i].fromPageNo
                                      << " topage:" << vRecords[i].toPageNo << endl);
                            TLOGERROR("[DbHandle::checkRoute] router error! "
                                      << ri.moduleName << " from:" << ri.fromPageNo
                                      << " topage:" << ri.toPageNo << endl);
                            return -1;
                        }
                        else
                            deleteRecord.push_back(vRecords[i]);
                    }
                }
                else
                {
                    //可以删除数据了
                    if (deleteRecord.size() != 0)
                    {
                        for (unsigned int j = 0; j < deleteRecord.size(); j++)
                        {
                            string sSql = "where module_name = '" + deleteRecord[j].moduleName +
                                          "' and id = " + TC_Common::tostr(deleteRecord[j].id);
                            FDLOG("recover")
                                << "delete router record! module:" << deleteRecord[j].moduleName
                                << " frompage:" << deleteRecord[j].fromPageNo
                                << " topage:" << deleteRecord[j].toPageNo
                                << " groupName:" << deleteRecord[j].groupName << endl;
                            int affect = _mysql->deleteRecord("t_router_record", sSql);
                            if (affect <= 0)
                            {
                                TLOGERROR("[DbHandle::checkRoute] delete error! "
                                          << deleteRecord[j].moduleName
                                          << " id:" << deleteRecord[j].id << endl);
                                return -1;
                            }
                        }
                        deleteRecord.clear();
                    }

                    ri = vRecords[i];
                }
            }
        }
    }
    catch (exception &ex)
    {
        TLOGERROR("[DbHandle::checkRoute] exception:" << ex.what() << endl);
        return -1;
    }

    return 0;
}

int DbHandle::loadRouteToMem()
{
    try
    {
        map<string, PackTable> *tmpPackTables = new map<string, PackTable>();

        //载入服务器组及服务器信息
        vector<GroupInfo> vGroups = getDBGroupList();
        for (size_t i = 0; i < vGroups.size(); ++i)
        {
            (*tmpPackTables)[vGroups[i].moduleName].groupList[vGroups[i].groupName] = vGroups[i];
        }

        map<string, ServerInfo> mServers = getDBServerList();
        for (map<string, ServerInfo>::iterator it = mServers.begin(); it != mServers.end(); ++it)
        {
            (*tmpPackTables)[it->second.moduleName].serverList[it->second.serverName] = it->second;
        }
        // opt下线cache服务时，不会清理模块信息和record信息，这里如果该模块下不存在服务，则不再加载路由到内存
        // 载入业务模块信息
        map<string, PackTable>::iterator itr;

        vector<ModuleInfo> vModules = getDBModuleList();
        for (size_t i = 0; i < vModules.size(); ++i)
        {
            itr = tmpPackTables->find(vModules[i].moduleName);
            if (itr != tmpPackTables->end())
            {
                (*tmpPackTables)[vModules[i].moduleName].info = vModules[i];
            }
        }

        // 载入路由记录信息
        vector<RecordInfo> vRecords = getDBRecordList();
        for (size_t i = 0; i < vRecords.size(); ++i)
        {
            itr = tmpPackTables->find(vRecords[i].moduleName);
            if (itr != tmpPackTables->end())
            {
                (*tmpPackTables)[vRecords[i].moduleName].recordList.push_back(vRecords[i]);
            }
        }

        TC_ThreadLock::Lock lock(_lock);

        // 防止切换频率太快，必须间隔_reloadTime时间才切换
        int64_t nowus = TC_Common::now2us();
        if (nowus - _lastLoadTime < _reloadTime)
        {
            TLOGDEBUG(FILE_FUN << __FUNCTION__ << " load frequently, sleep a little: "
                               << _reloadTime - nowus + _lastLoadTime << "us ..." << endl);
            usleep(_reloadTime - nowus + _lastLoadTime);
        }

        // 切换重载后的配置为正式
        map<string, PackTable> *delPackTables = _mapPackTables1;
        _mapPackTables1 = _mapPackTables;
        _mapPackTables = tmpPackTables;
        delete delPackTables;

        _lastLoadTime = TC_Common::now2us();
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::loadRouteToMem exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::loadRouteToMem|") + ex.what());
        return -1;
    }

    return 0;
}

/*按模块载入路由，注意服务器组及服务器没有载入，只载入了模块信息及路由记录*/
int DbHandle::loadRouteToMem(const string &moduleName)
{
    try
    {
        // 载入业务模块信息
        ModuleInfo module = getDBModuleList(moduleName);
        // 载入路由记录信息
        vector<RecordInfo> vRecords = getDBRecordList(moduleName);
        map<string, GroupInfo> groupList;
        map<string, ServerInfo> serverList;
        // 载入服务器组及服务器信息
        bool findGroup = false;
        vector<GroupInfo> vGroups = getDBGroupList(moduleName);
        for (size_t i = 0; i < vGroups.size(); ++i)
        {
            findGroup = true;
            groupList[vGroups[i].groupName] = vGroups[i];
        }
        if (!findGroup)
        {
            TC_ThreadLock::Lock lock(_lock);
            (*_mapPackTables).erase(moduleName);
            TLOGERROR(FILE_FUN << "DbHandle::loadRouteToMem no group find moduleName " << moduleName
                               << endl);
            return 0;
        }
        map<string, ServerInfo> mServers = getDBServerList(moduleName);
        for (map<string, ServerInfo>::iterator it = mServers.begin(); it != mServers.end(); ++it)
        {
            serverList[it->second.serverName] = it->second;
        }
        TC_ThreadLock::Lock lock(_lock);
        (*_mapPackTables)[moduleName].info = module;
        (*_mapPackTables)[moduleName].recordList = vRecords;
        (*_mapPackTables)[moduleName].groupList = groupList;
        (*_mapPackTables)[moduleName].serverList = serverList;
        _lastLoadTime = TC_Common::now2us();
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::loadRouteToMem exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::loadRouteToMem|") + ex.what());
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }

    return 0;
}

vector<ModuleInfo> DbHandle::getDBModuleList()
{
    TC_ThreadLock::Lock lock(_dbLock);

    string sSql = "select id, module_name, version, switch_status from t_router_module";
    TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);

    vector<ModuleInfo> vModules;
    for (size_t i = 0; i < sqlData.size(); i++)
    {
        ModuleInfo info;
        info.id = S2I(sqlData[i]["id"]);
        info.moduleName = sqlData[i]["module_name"];
        info.version = S2I(sqlData[i]["version"]);
        info.switch_status = S2I(sqlData[i]["switch_status"]);
        vModules.push_back(info);
    }

    return vModules;
}

ModuleInfo DbHandle::getDBModuleList(const string &moduleName)
{
    TC_ThreadLock::Lock lock(_dbLock);

    string sSql =
        "select id, module_name, version, switch_status from t_router_module where "
        "module_name = '" +
        moduleName + "' order by id asc";
    TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);

    ModuleInfo info;
    if (sqlData.size() > 0)
    {
        info.id = S2I(sqlData[0]["id"]);
        info.moduleName = moduleName;
        info.version = S2I(sqlData[0]["version"]);
        info.switch_status = S2I(sqlData[0]["switch_status"]);
    }

    return info;
}

vector<RecordInfo> DbHandle::getDBRecordList()
{
    TC_ThreadLock::Lock lock(_dbLock);

    string sSql =
        "select id, module_name, from_page_no, to_page_no, group_name"
        " from t_router_record order by module_name asc, from_page_no asc";
    TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);

    vector<RecordInfo> vRecords;
    for (size_t i = 0; i < sqlData.size(); i++)
    {
        RecordInfo info;
        info.id = S2I(sqlData[i]["id"]);
        info.moduleName = sqlData[i]["module_name"];
        info.fromPageNo = S2I(sqlData[i]["from_page_no"]);
        info.toPageNo = S2I(sqlData[i]["to_page_no"]);
        info.groupName = sqlData[i]["group_name"];

        vRecords.push_back(info);
    }

    return vRecords;
}

vector<RecordInfo> DbHandle::getDBRecordList(const string &moduleName)
{
    TC_ThreadLock::Lock lock(_dbLock);

    string sSql =
        "select id, from_page_no, to_page_no, group_name"
        " from t_router_record where module_name = '" +
        moduleName + "' order by from_page_no asc";
    TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);

    vector<RecordInfo> vRecords;
    for (size_t i = 0; i < sqlData.size(); i++)
    {
        RecordInfo info;
        info.id = S2I(sqlData[i]["id"]);
        info.moduleName = moduleName;
        info.fromPageNo = S2I(sqlData[i]["from_page_no"]);
        info.toPageNo = S2I(sqlData[i]["to_page_no"]);
        info.groupName = sqlData[i]["group_name"];

        vRecords.push_back(info);
    }

    return vRecords;
}

vector<GroupInfo> DbHandle::getDBGroupList()
{
    TC_ThreadLock::Lock lock(_dbLock);
    string sSql =
        "select module_name, group_name, access_status from t_router_group group by "
        "module_name, group_name, access_status";
    TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);

    vector<GroupInfo> vGroups;
    for (size_t i = 0; i < sqlData.size(); ++i)
    {
        GroupInfo info;
        info.id = i;
        info.moduleName = sqlData[i]["module_name"];
        info.groupName = sqlData[i]["group_name"];
        info.accessStatus = TC_Common::strto<int>(sqlData[i]["access_status"]);
        if (info.accessStatus == 1)
        {
            string errMsg =
                "reload router find error accessStatus=1 moduleName:" + info.moduleName +
                " groupName:" + info.groupName;
            TLOGERROR(FILE_FUN << errMsg << endl);
            TARS_NOTIFY_ERROR(errMsg);
        }
        string sSql2 =
            "select a.server_name, a.idc_area, b.server_status, b.priority, b.source_server_name, "
            "case when b.server_status in ('Z') then 1 when b.server_status in ('M') then 2 else 3 "
            "end level from t_router_server as a, t_router_group as b where a.server_name = "
            "b.server_name and b.module_name = '" +
            info.moduleName + "' and b.group_name = '" + info.groupName +
            "' order by level, priority";
        TC_Mysql::MysqlData sqlData2 = _mysql->queryRecord(sSql2);
        for (size_t j = 0; j < sqlData2.size(); j++)
        {
            if (sqlData2[j]["server_status"] == "M")
            {
                info.masterServer = sqlData2[j]["server_name"];
            }
            info.bakList[sqlData2[j]["server_name"]] = sqlData2[j]["source_server_name"];
            info.idcList[sqlData2[j]["idc_area"]].push_back(sqlData2[j]["server_name"]);
        }
        vGroups.push_back(info);
    }
    return vGroups;
}

vector<GroupInfo> DbHandle::getDBGroupList(const string &moduleName)
{
    TC_ThreadLock::Lock lock(_dbLock);
    string sSql =
        "select module_name, group_name, access_status from t_router_group where module_name = '" +
        moduleName + "' group by module_name, group_name, access_status";
    TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);

    vector<GroupInfo> vGroups;
    for (size_t i = 0; i < sqlData.size(); ++i)
    {
        GroupInfo info;
        info.id = i;
        info.moduleName = sqlData[i]["module_name"];
        info.groupName = sqlData[i]["group_name"];
        info.accessStatus = TC_Common::strto<int>(sqlData[i]["access_status"]);
        if (info.accessStatus == 1)
        {
            string errMsg =
                "reload router find error accessStatus=1 moduleName:" + info.moduleName +
                " groupName:" + info.groupName;
            TLOGERROR(FILE_FUN << errMsg << endl);
            TARS_NOTIFY_ERROR(errMsg);
        }
        string sSql2 =
            "select a.server_name, a.idc_area, b.server_status, b.priority, b.source_server_name, "
            "case when b.server_status in ('Z') then 1 when b.server_status in ('M') then 2 else 3 "
            "end level from t_router_server as a, t_router_group as b where a.server_name = "
            "b.server_name and b.module_name = '" +
            info.moduleName + "' and b.group_name = '" + info.groupName +
            "' order by level, priority";
        TC_Mysql::MysqlData sqlData2 = _mysql->queryRecord(sSql2);
        for (size_t j = 0; j < sqlData2.size(); j++)
        {
            if (sqlData2[j]["server_status"] == "M")
            {
                info.masterServer = sqlData2[j]["server_name"];
            }
            info.bakList[sqlData2[j]["server_name"]] = sqlData2[j]["source_server_name"];
            info.idcList[sqlData2[j]["idc_area"]].push_back(sqlData2[j]["server_name"]);
        }
        vGroups.push_back(info);
    }
    return vGroups;
}

map<string, ServerInfo> DbHandle::getDBServerList()
{
    TC_ThreadLock::Lock lock(_dbLock);

    string sSql =
        "select a.id, a.server_name, a.ip, a.binlog_port, a.cache_port, "
        "a.routerclient_port, a.idc_area, a.status, b.module_name, b.server_status, b.group_name "
        "from t_router_server as a, t_router_group as b where a.server_name = b.server_name";
    TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);
    map<string, ServerInfo> mServers;

    for (size_t i = 0; i < sqlData.size(); i++)
    {
        string sServerName = sqlData[i]["server_name"];
        ServerInfo &info = mServers[sServerName];
        info.id = S2I(sqlData[i]["id"]);
        info.serverName = sServerName;
        info.ip = sqlData[i]["ip"];
        info.BinLogServant = info.serverName + ".BinLogObj";
        info.CacheServant = info.serverName + ".CacheObj";
        info.WCacheServant = info.serverName + ".WCacheObj";
        info.RouteClientServant = info.serverName + ".RouterClientObj";
        info.idc = sqlData[i]["idc_area"];
        info.ServerStatus = sqlData[i]["server_status"];
        info.moduleName = sqlData[i]["module_name"];
        info.groupName = sqlData[i]["group_name"];
        info.status = S2I(sqlData[i]["status"]);
    }
    return mServers;
}

map<string, ServerInfo> DbHandle::getDBServerList(const string &moduleName)
{
    TC_ThreadLock::Lock lock(_dbLock);

    string sSql =
        "select a.id, a.server_name, a.ip, a.binlog_port, a.cache_port, "
        "a.routerclient_port, a.idc_area, a.status, b.module_name, b.server_status, "
        "b.group_name from t_router_server as a, t_router_group as b where a.server_name "
        "= b.server_name and b.module_name = '" +
        moduleName + "'";
    TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);

    map<string, ServerInfo> mServers;
    for (size_t i = 0; i < sqlData.size(); i++)
    {
        string sServerName = sqlData[i]["server_name"];
        ServerInfo &info = mServers[sServerName];
        info.id = S2I(sqlData[i]["id"]);
        info.serverName = sServerName;
        info.ip = sqlData[i]["ip"];
        info.BinLogServant = info.serverName + ".BinLogObj";
        info.CacheServant = info.serverName + ".CacheObj";
        info.WCacheServant = info.serverName + ".WCacheObj";
        info.RouteClientServant = info.serverName + ".RouterClientObj";
        info.idc = sqlData[i]["idc_area"];
        info.ServerStatus = sqlData[i]["server_status"];
        info.moduleName = sqlData[i]["module_name"];
        info.groupName = sqlData[i]["group_name"];
        info.status = S2I(sqlData[i]["status"]);
    }

    return mServers;
}

int DbHandle::getIdcMap(map<string, string> &idcMap)
{
    try
    {
        TC_ThreadLock::Lock lock(_relationLock);
        string sSql = "select city, idc from t_idc_map";
        TC_Mysql::MysqlData sqlData = _mysqlDBRelation->queryRecord(sSql);
        for (size_t i = 0; i < sqlData.size(); ++i)
        {
            string city = TC_Encoder::gbk2utf8(sqlData[i]["city"]);
            idcMap[city] = sqlData[i]["idc"];
        }
        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR("DbHandle::getIdcMap exception: " << ex.what() << endl);
    }

    return -1;
}

int DbHandle::getRouterInfo(const string &sModuleName,
                            int &version,
                            vector<TransferInfo> &transferingInfoList,
                            PackTable &packTable)
{
    int iRet = -1;

    version = TRANSFER_CLEAN_VERSION;
    map<string, PackTable>::const_iterator it;

    TC_ThreadLock::Lock lock(_transLock);
    TC_ThreadLock::Lock lock1(_lock);

    it = _mapPackTables->find(sModuleName);
    if (it != _mapPackTables->end())
    {
        packTable = it->second;
        map<string, ModuleTransferingInfo>::iterator it2 = _mapTransferingInfos.find(sModuleName);
        if (it2 != _mapTransferingInfos.end())
        {
            if (!(it2->second).transferingInfoList.empty())
            {
                version = (it2->second).version;
                transferingInfoList = (it2->second).transferingInfoList;
            }
        }
        iRet = 0;
    }

    return iRet;
}

int DbHandle::getVersion(const string &sModuleName)
{
    int iRet = -1;  // 业务模块不存在

    TC_ThreadLock::Lock lock(_lock);
    map<string, PackTable>::const_iterator it = _mapPackTables->find(sModuleName);
    if (it != _mapPackTables->end())
    {
        iRet = (it->second).info.version;
    }

    return iRet;
}
int DbHandle::getSwitchType(const string &sModuleName)
{
    int iRet = -1;  // 业务模块不存在

    TC_ThreadLock::Lock lock(_lock);
    map<string, PackTable>::const_iterator it = _mapPackTables->find(sModuleName);
    if (it != _mapPackTables->end())
    {
        iRet = (it->second).info.switch_status;
    }
    else
    {
        FDLOG("switch") << "DbHandle::getSwitchType module:" << sModuleName << " not found!"
                        << endl;
    }

    return iRet;
}

int DbHandle::getPackTable(const string &sModuleName, PackTable &packTable)
{
    int iRet = -1;  // 业务模块不存在
    TC_ThreadLock::Lock lock(_lock);
    map<string, PackTable>::const_iterator it = _mapPackTables->find(sModuleName);
    if (it != _mapPackTables->end())
    {
        packTable = it->second;
        iRet = 0;
    }

    return iRet;
}

int DbHandle::getMasterIdc(const string &sModuleName, string &idc)
{
    int iRet = -1;  // 业务模块不存在

    TC_ThreadLock::Lock lock(_lock);
    map<string, PackTable>::iterator it = _mapPackTables->find(sModuleName);
    if (it != _mapPackTables->end())
    {
        map<string, GroupInfo>::const_iterator itgroup = it->second.groupList.begin();
        idc = it->second.serverList[itgroup->second.masterServer].idc;
        iRet = 0;
    }

    return iRet;
}

int DbHandle::loadSwitchInfo()
{
    TC_ThreadLock::Lock lock(_lock);
    g_app.loadSwitchInfo(_mapPackTables);
    return 0;
}

int DbHandle::loadSwitchInfo(const string &moduleName)
{
    TC_ThreadLock::Lock lock(_lock);
    //上报心跳时间在刚开始加载的时候 设置为0 而不设置为当前时间 防止升级过程中的误切换
    g_app.loadSwitchInfo(_mapPackTables, moduleName);
    return 0;
}

int DbHandle::getTransferingInfos(const string &sModuleName, ModuleTransferingInfo &info)
{
    TC_ThreadLock::Lock lock(_transLock);

    map<string, ModuleTransferingInfo>::iterator _it = _mapTransferingInfos.find(sModuleName);
    if (_it != _mapTransferingInfos.end())
    {
        info = _it->second;
    }
    return 0;
}

int DbHandle::getTransferInfos(map<string, map<string, TransferInfo>> &info)
{
    TC_ThreadLock::Lock lock(_transLock);
    info = _mapTransferInfos;
    return 0;
}

bool DbHandle::clearTransferInfos()
{
    TC_ThreadLock::Lock lock(_transLock);
    _mapTransferInfos.clear();
    _mapTransferingInfos.clear();
    return (_mapTransferInfos.size() == 0 && _mapTransferingInfos.size() == 0);
}

int DbHandle::getModuleInfos(map<string, ModuleInfo> &info)
{
    TC_ThreadLock::Lock lock(_lock);
    map<string, PackTable>::const_iterator it;
    for (it = _mapPackTables->begin(); it != _mapPackTables->end(); it++)
    {
        info[it->first] = it->second.info;
    }
    return 0;
}

int DbHandle::getModuleList(vector<string> &moduleList)
{
    TC_ThreadLock::Lock lock(_lock);
    map<string, PackTable>::const_iterator it;
    for (it = _mapPackTables->begin(); it != _mapPackTables->end(); it++)
    {
        if (it->second.info.switch_status != 3)
        {
            moduleList.push_back(it->first);
        }
    }
    return 0;
}

bool DbHandle::hasTransfering() { return (_mapTransferInfos.size() != 0); }

bool DbHandle::hasTransfering(const string &sModuleName)
{
    map<string, map<string, TransferInfo>>::iterator it = _mapTransferInfos.find(sModuleName);
    if (it != _mapTransferInfos.end()) return true;

    return false;
}

bool DbHandle::hasTransferingLoc(const string &sModuleName, const string &sGroupName)
{
    //判断当前模块中的当前组是否正在被迁移
    FDLOG("TransferSwitch") << __LINE__ << "|" << __FUNCTION__ << "|"
                            << " pthread_id: " << pthread_self() << " module:" << sModuleName
                            << " group: " << sGroupName << endl;

    TC_ThreadLock::Lock lock(_lock);
    map<string, map<string, TransferInfo>>::iterator it = _mapTransferInfos.find(sModuleName);
    if (it != _mapTransferInfos.end())
    {
        map<string, TransferInfo>::iterator iter = it->second.begin();
        FDLOG("TransferSwitch") << __LINE__ << "|" << __FUNCTION__ << "|"
                                << " pthread_id: " << pthread_self() << " module:" << sModuleName
                                << " sourcegroup:" << iter->second.groupName
                                << " destgroup:" << iter->second.transGroupName
                                << " is transfering."
                                << " group: " << sGroupName << " need to switch" << endl;

        while (iter != it->second.end())
        {
            if (iter->second.groupName == sGroupName ||
                iter->second.transGroupName == sGroupName)  //切换组在源或目的组内
            {
                FDLOG("TransferSwitch")
                    << __LINE__ << "|" << __FUNCTION__ << "|"
                    << " pthread_id: " << pthread_self() << " module:" << sModuleName
                    << " sourcegroup:" << iter->second.groupName
                    << " destgroup:" << iter->second.transGroupName << " is transfering."
                    << " group: " << sGroupName << " need to switch with collision" << endl;

                return true;
            }
            ++iter;
        }
    }

    FDLOG("TransferSwitch") << __LINE__ << "|" << __FUNCTION__ << "|"
                            << " pthread_id: " << pthread_self() << " module:" << sModuleName
                            << " group: " << sGroupName << " need to switch, func end" << endl;
    return false;
}

bool DbHandle::hasTransfering(const TransferInfo &transferInfo)
{
    // 判断当前页是否正在被迁移
    map<string, map<string, TransferInfo>>::iterator it =
        _mapTransferInfos.find(transferInfo.moduleName);
    if (it != _mapTransferInfos.end())
    {
        map<string, TransferInfo> &m = it->second;
        map<string, TransferInfo>::iterator it1 = m.begin();
        for (; it1 != m.end(); ++it1)
        {
            if (((it1->second).fromPageNo <= transferInfo.fromPageNo &&
                 (it1->second).toPageNo >= transferInfo.fromPageNo) ||
                ((it1->second).fromPageNo <= transferInfo.toPageNo &&
                 (it1->second).toPageNo >= transferInfo.toPageNo))
            {
                return true;
            }
        }
    }

    return false;
}

bool DbHandle::hasTransfering(const string &sModuleName, pthread_t threadId)
{
    map<string, map<string, TransferInfo>>::iterator it = _mapTransferInfos.find(sModuleName);

    if (it != _mapTransferInfos.end())
    {
        if (it->second.find(TC_Common::tostr<pthread_t>(threadId)) != it->second.end())
        {
            return true;
        }
    }

    return false;
}

// 数据库操作，返回-1比较严重，一般需要手工处理了
int DbHandle::changeDbRecord(const TransferInfo &transferInfo)
{
    TC_ThreadLock::Lock lock(_dbLock);

    try
    {
        string sSql =
            "select id, module_name, from_page_no, to_page_no, group_name"
            " from t_router_record where from_page_no <= " +
            I2S(transferInfo.fromPageNo) + " and to_page_no >= " + I2S(transferInfo.fromPageNo) +
            " and module_name = '" + transferInfo.moduleName + "'";
        TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);
        if (sqlData.size() != 1)
        {
            return -1;
        }

        RecordInfo dbRecord;
        dbRecord.fromPageNo = S2I(sqlData[0]["from_page_no"]);
        dbRecord.toPageNo = S2I(sqlData[0]["to_page_no"]);
        dbRecord.groupName = sqlData[0]["group_name"];
        if ((dbRecord.fromPageNo == transferInfo.fromPageNo) &&
            (dbRecord.groupName == transferInfo.groupName) &&
            (dbRecord.toPageNo >= transferInfo.toPageNo))
        {
            vector<string> vSql;
            if (dbRecord.toPageNo == transferInfo.toPageNo)
            {
                sSql = "delete from t_router_record where id = " + sqlData[0]["id"];
                vSql.push_back(sSql);
            }
            else
            {
                sSql = "where id = " + sqlData[0]["id"];

                TC_Mysql::RECORD_DATA updateData;
                updateData["from_page_no"] =
                    make_pair(TC_Mysql::DB_INT, I2S(transferInfo.toPageNo + 1));

                string s = _mysql->buildUpdateSQL("t_router_record", updateData, sSql);
                vSql.push_back(s);
            }

            RecordInfo record;
            record.id = 0;
            record.fromPageNo = transferInfo.fromPageNo;
            record.toPageNo = transferInfo.toPageNo;
            record.moduleName = transferInfo.moduleName;
            record.groupName = transferInfo.transGroupName;

            // 插入拆分后的记录

            TC_Mysql::RECORD_DATA insertData;
            insertData["from_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(record.fromPageNo));
            insertData["to_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(record.toPageNo));
            insertData["module_name"] = make_pair(TC_Mysql::DB_STR, record.moduleName);
            insertData["group_name"] = make_pair(TC_Mysql::DB_STR, record.groupName);
            string s = _mysql->buildInsertSQL("t_router_record", insertData);
            vSql.push_back(s);

            string sErrMsg;
            bool bSucc = doTransaction(vSql, sErrMsg);

            if (bSucc)
                return updateVersion(transferInfo.moduleName);
            else
            {
                DAY_ERROR << "DbHandle::changeDbRecord doTransaction err: " << sErrMsg << endl;
                return -1;
            }
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::changeDbRecord exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::changeDbRecord|") + ex.what());
        if (_exRep)
        {
            _exRep->report(1);
        }
    }

    return -1;
}

int DbHandle::changeDbRecord2(const TransferInfo &transferInfo)
{
    TC_ThreadLock::Lock lock(_dbLock);

    int iFromPageNo = transferInfo.fromPageNo;
    try
    {
        vector<RecordInfo> addRecords;
        vector<string> delRecords;
        while (1)
        {
            string sSql =
                "select id, module_name, from_page_no, to_page_no, group_name"
                " from t_router_record where from_page_no <= " +
                I2S(iFromPageNo) + " and to_page_no >= " + I2S(iFromPageNo) +
                " and module_name = '" + transferInfo.moduleName + "'";
            TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sSql);
            if (sqlData.size() != 1)
            {
                return -1;
            }

            delRecords.push_back(sqlData[0]["id"]);
            RecordInfo dbRecord;
            dbRecord.id = S2I(sqlData[0]["id"]);
            dbRecord.fromPageNo = S2I(sqlData[0]["from_page_no"]);
            dbRecord.toPageNo = S2I(sqlData[0]["to_page_no"]);
            dbRecord.moduleName = sqlData[0]["module_name"];
            dbRecord.groupName = sqlData[0]["group_name"];

            // 开始页不相等，则需要增加一条记录，但只限于transferInfo(其实应该是整个迁移过程，这个地方可以进一步增加限制性)的起始页，否则的话数据库中的路由记录肯定有重叠
            if (dbRecord.fromPageNo != iFromPageNo)
            {
                if (iFromPageNo != transferInfo.fromPageNo)
                {
                    return -1;
                }
                RecordInfo record = dbRecord;
                record.id = 0;
                record.toPageNo = iFromPageNo - 1;
                addRecords.push_back(record);
            }

            // 如果dbRecord的结束页>=transferInfo的结束页，则结束循环
            if (dbRecord.toPageNo >= transferInfo.toPageNo)
            {
                if (dbRecord.toPageNo != transferInfo.toPageNo)
                {
                    RecordInfo record = dbRecord;
                    record.id = 0;
                    record.fromPageNo = transferInfo.toPageNo + 1;
                    addRecords.push_back(record);
                }
                break;
            }

            iFromPageNo = dbRecord.toPageNo + 1;
        }

        RecordInfo record;
        record.id = 0;
        record.fromPageNo = transferInfo.fromPageNo;
        record.toPageNo = transferInfo.toPageNo;
        record.moduleName = transferInfo.moduleName;
        record.groupName = transferInfo.groupName;
        addRecords.push_back(record);

        // 删除数据库原记录
        string sSql = "delete from t_router_record where id = " + delRecords[0];
        for (size_t i = 1; i < delRecords.size(); ++i)
        {
            sSql += " or id = " + delRecords[i];
        }

        vector<string> vSql;
        vSql.push_back(sSql);

        // 插入拆分后的记录
        for (size_t k = 0; k < addRecords.size(); k++)
        {
            TC_Mysql::RECORD_DATA insertData;
            insertData["from_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(addRecords[k].fromPageNo));
            insertData["to_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(addRecords[k].toPageNo));
            insertData["module_name"] = make_pair(TC_Mysql::DB_STR, addRecords[k].moduleName);
            insertData["group_name"] = make_pair(TC_Mysql::DB_STR, addRecords[k].groupName);

            string s = _mysql->buildInsertSQL("t_router_record", insertData);
            vSql.push_back(s);
        }

        //执行事务
        string sErrMsg;
        bool bSucc = doTransaction(vSql, sErrMsg);

        if (bSucc)
            return updateVersion(transferInfo.moduleName);
        else
        {
            DAY_ERROR << "DbHandle::changeDbRecord2 doTransaction error: " << sErrMsg << endl;
            return -1;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::changeDbRecord exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::changeDbRecord|") + ex.what());
        if (_exRep)
        {
            _exRep->report(1);
        }
    }

    return -1;
}

// 数据库操作，返回-1比较严重，一般需要手工处理了
int DbHandle::insertRouterRecord(const vector<RecordInfo> &addRecords)
{
    // 插入记录
    for (size_t k = 0; k < addRecords.size(); k++)
    {
        TC_Mysql::RECORD_DATA insertData;
        insertData["from_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(addRecords[k].fromPageNo));
        insertData["to_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(addRecords[k].toPageNo));
        insertData["module_name"] = make_pair(TC_Mysql::DB_STR, addRecords[k].moduleName);
        insertData["group_name"] = make_pair(TC_Mysql::DB_STR, addRecords[k].groupName);
        int affect = _mysql->insertRecord("t_router_record", insertData);
        if (affect != 1)
        {
            DAY_ERROR << __FUNCTION__ << " _mysql->insertRecord return: " << affect << endl;
            return -1;
        }
    }

    return 0;
}

int DbHandle::insertRouterRecord(const RecordInfo &record)
{
    TC_Mysql::RECORD_DATA insertData;
    insertData["from_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(record.fromPageNo));
    insertData["to_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(record.toPageNo));
    insertData["module_name"] = make_pair(TC_Mysql::DB_STR, record.moduleName);
    insertData["group_name"] = make_pair(TC_Mysql::DB_STR, record.groupName);
    int affect = _mysql->insertRecord("t_router_record", insertData);
    if (affect != 1)
    {
        DAY_ERROR << __FUNCTION__ << " _mysql->insertRecord return: " << affect << endl;
        return -1;
    }

    return 0;
}

int DbHandle::modifyMemRecord(const TransferInfo &transferInfo)
{
    TC_ThreadLock::Lock lock(_lock);

    map<string, PackTable>::iterator it = _mapPackTables->find(transferInfo.moduleName);

    if (it != _mapPackTables->end())
    {
        vector<RecordInfo> &recordList = (it->second).recordList;
        size_t size = recordList.size();
        for (size_t i = 0; i < size; ++i)
        {
            if (recordList[i].fromPageNo == transferInfo.fromPageNo)
            {
                if (recordList[i].groupName == transferInfo.groupName)
                {
                    if (recordList[i].toPageNo >= transferInfo.toPageNo)
                    {
                        RecordInfo record;
                        record.id = 0;
                        record.moduleName = transferInfo.moduleName;
                        record.fromPageNo = transferInfo.fromPageNo;
                        record.toPageNo = transferInfo.toPageNo;
                        record.groupName = transferInfo.transGroupName;

                        if (recordList[i].toPageNo == transferInfo.toPageNo)
                        {
                            recordList.erase(recordList.begin() + i);
                        }
                        else
                        {
                            recordList[i].fromPageNo = transferInfo.toPageNo + 1;
                        }

                        recordList.push_back(record);

                        ++(it->second.info.version);

                        return 0;
                    }
                }
                break;
            }
        }
    }

    return -1;
}

int DbHandle::modifyMemRecord2(const TransferInfo &transferInfo)
{
    TC_ThreadLock::Lock lock(_lock);

    map<string, PackTable>::iterator it = _mapPackTables->find(transferInfo.moduleName);

    if (it != _mapPackTables->end())
    {
        int iPageNo = transferInfo.fromPageNo;
        vector<RecordInfo> newRecords;

        RecordInfo record;
        record.id = 0;
        record.moduleName = transferInfo.moduleName;
        record.fromPageNo = transferInfo.fromPageNo;
        record.toPageNo = transferInfo.toPageNo;
        record.groupName = transferInfo.groupName;
        newRecords.push_back(record);

        bool bFinish = false;
        vector<RecordInfo> recordList = (it->second).recordList;
        while (!bFinish)
        {
            size_t i = 0, size = recordList.size();
            for (; i < size; ++i)
            {
                if (recordList[i].fromPageNo <= iPageNo && recordList[i].toPageNo >= iPageNo)
                {
                    if (recordList[i].groupName == transferInfo.groupName)
                    {
                        // 开始页不相等，则需要增加一条记录
                        if (recordList[i].fromPageNo != iPageNo)
                        {
                            if (iPageNo != transferInfo.fromPageNo)
                            {
                                return -1;
                            }
                            RecordInfo record = recordList[i];
                            record.id = 0;
                            record.toPageNo = iPageNo - 1;
                            newRecords.push_back(record);
                        }

                        // 结束页不相等，则需要增加一条记录
                        if (recordList[i].toPageNo >= transferInfo.toPageNo)
                        {
                            if (recordList[i].toPageNo != transferInfo.toPageNo)
                            {
                                RecordInfo record = recordList[i];
                                record.id = 0;
                                record.fromPageNo = transferInfo.toPageNo + 1;
                                newRecords.push_back(record);
                            }
                            bFinish = true;
                        }

                        iPageNo = recordList[i].toPageNo + 1;
                        recordList.erase(recordList.begin() + i);
                    }
                    else
                    {
                        return -1;
                    }

                    break;
                }
            }
            if (i == size)
            {
                return -1;
            }
        }

        newRecords.insert(newRecords.end(), recordList.begin(), recordList.end());

        it->second.recordList = newRecords;

        ++(it->second.info.version);

        return 0;
    }

    return -1;
}

// 数据库操作，返回-1比较严重，一般需要手工处理了
int DbHandle::defragDbRecord(const TransferInfo &transferInfo)
{
    TC_ThreadLock::Lock lock(_dbLock);

    try
    {
        vector<string> vSql;

        // 删除数据库分散的记录
        string sSql =
            "delete from t_router_record where from_page_no >= " + I2S(transferInfo.fromPageNo) +
            " and to_page_no <= " + I2S(transferInfo.toPageNo) + " and module_name = '" +
            transferInfo.moduleName + "'";
        vSql.push_back(sSql);

        // 插入整理后的记录
        TC_Mysql::RECORD_DATA insertData;
        insertData["from_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(transferInfo.fromPageNo));
        insertData["to_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(transferInfo.toPageNo));
        insertData["module_name"] = make_pair(TC_Mysql::DB_STR, transferInfo.moduleName);
        insertData["group_name"] = make_pair(TC_Mysql::DB_STR, transferInfo.transGroupName);
        sSql.clear();
        sSql = _mysql->buildInsertSQL("t_router_record", insertData);
        vSql.push_back(sSql);

        string sErrMsg;
        bool bSucc = doTransaction(vSql, sErrMsg);
        if (bSucc)
            return updateVersion(transferInfo.moduleName);
        else
        {
            DAY_ERROR << "DbHandle::defragDbRecord doTransaction error: " << sErrMsg << endl;
            return -1;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::defragDbRecord exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::defragDbRecord|") + ex.what());
        if (_exRep)
        {
            _exRep->report(1);
        }
    }

    return -1;
}

// 数据库操作，返回-1比较严重，一般需要手工处理了
int DbHandle::defragDbRecord(const string &sModuleName,
                             vector<RecordInfo> &vOldRecord,
                             vector<RecordInfo> &vNewRecord)
{
    try
    {
        PackTable packTable;
        if (getPackTable(sModuleName, packTable) < 0)
        {
            DAY_ERROR << "DbHandle::defragDbRecord cant find moduleName: " << sModuleName << endl;
            TARS_NOTIFY_WARN(string("DbHandle::defragDbRecord|cant find moduleName: ") +
                             sModuleName);
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -2;  // 找不着业务模块
        }

        vector<RecordInfo> newRecords;
        vector<RecordInfo> &oldRecords = packTable.recordList;
        int iBegin = 0, iEnd = 0;
        for (size_t i = 0; i < oldRecords.size(); i++)
        {
            // 比较相邻记录页号是否连续
            if (i != oldRecords.size() - 1 &&
                oldRecords[i].toPageNo + 1 == oldRecords[i + 1].fromPageNo &&
                oldRecords[i].groupName == oldRecords[i + 1].groupName &&
                oldRecords[i].moduleName == oldRecords[i + 1].moduleName)
            {
                iEnd = i + 1;
            }
            else
            {
                iEnd = i;
                // 合并
                if (iBegin < iEnd)
                {
                    RecordInfo tmp = oldRecords[iBegin];
                    tmp.id = 0;
                    tmp.toPageNo = oldRecords[iEnd].toPageNo;
                    {
                        TC_ThreadLock::Lock lock(_dbLock);
                        //先插入，再删除
                        if (insertRouterRecord(tmp) != 0)
                        {
                            TLOGERROR("[DbHandle::defragDbRecord] insert record error!" << endl);
                            ;
                            return -1;
                        }

                        string sSql = "where module_name = '" + sModuleName + "' and id in (" +
                                      TC_Common::tostr(oldRecords[iBegin].id);
                        ;
                        for (int j = iBegin + 1; j <= iEnd; j++)
                        {
                            sSql += "," + TC_Common::tostr(oldRecords[j].id);
                        }
                        sSql += ")";
                        int affect = _mysql->deleteRecord("t_router_record", sSql);
                        if (affect != iEnd - iBegin + 1)
                        {
                            DAY_ERROR
                                << "DbHandle::defragDbRecord mysql deleteRecord return: " << affect
                                << " " << sSql << endl;
                            TARS_NOTIFY_WARN(
                                string("DbHandle::defragDbRecord|mysql deleteRecord return: ") +
                                I2S(affect) + " " + sSql);
                            if (_exRep)
                            {
                                _exRep->report(1);
                            }
                            return -1;
                        }
                    }
                }
                else
                {
                    newRecords.push_back(oldRecords[iBegin]);
                }
                iBegin = i + 1;
            }
        }

        PackTable newPackTable;
        if (getPackTable(sModuleName, newPackTable) < 0)
        {
            DAY_ERROR << "DbHandle::defragDbRecord cant find moduleName: " << sModuleName << endl;
            TARS_NOTIFY_WARN(string("DbHandle::defragDbRecord|cant find moduleName: ") +
                             sModuleName);
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -2;  // 找不着业务模块
        }

        vOldRecord = oldRecords;
        vNewRecord = newPackTable.recordList;
        return updateVersion(sModuleName);
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::defragDbRecord exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::defragDbRecord|") + ex.what());
        if (_exRep)
        {
            _exRep->report(1);
        }
    }

    return -1;
}

// 整理某个模块的路由碎片，返回整理前的记录及整理后的记录
int DbHandle::defragDbRecord(string &moduleNames,
                             map<string, vector<RecordInfo>> &mOldRecord,
                             map<string, vector<RecordInfo>> &mNewRecord)
{
    TC_ThreadLock::Lock lock(_transLock);
    if (!hasTransfering())
    {
        vector<string> names = SEPSTR(moduleNames, " ");
        moduleNames.clear();
        vector<string>::const_iterator vit;
        vit = find(names.begin(), names.end(), "all");
        if (vit != names.end())
        {
            map<string, PackTable>::const_iterator it;
            for (it = _mapPackTables->begin(); it != _mapPackTables->end(); it++)
            {
                int iRet = defragDbRecord(it->first, mOldRecord[it->first], mNewRecord[it->first]);
                if (iRet == -1)
                {
                    DAY_ERROR << "DbHandle::defragDbRecord defrag route fail: " << it->first
                              << endl;
                    TARS_NOTIFY_WARN(string("DbHandle::defragDbRecord|defrag route fail: ") +
                                     it->first);
                    if (_exRep)
                    {
                        _exRep->report(1);
                    }
                    moduleNames = it->first;
                    return -1;
                }
                else if (iRet == -2)
                {
                    moduleNames += it->first + " ";
                }
            }
        }
        else
        {
            map<string, PackTable>::const_iterator it;
            for (unsigned i = 0; i < names.size(); i++)
            {
                it = _mapPackTables->find(names[i]);
                if (it != _mapPackTables->end())
                {
                    int iRet =
                        defragDbRecord(it->first, mOldRecord[it->first], mNewRecord[it->first]);
                    if (iRet == -1)
                    {
                        DAY_ERROR << "DbHandle::defragDbRecord defrag route fail: " << it->first
                                  << endl;
                        TARS_NOTIFY_WARN(string("DbHandle::defragDbRecord|defrag route fail: ") +
                                         it->first);
                        if (_exRep)
                        {
                            _exRep->report(1);
                        }
                        moduleNames = it->first;
                        return -1;
                    }
                    else if (iRet == -2)
                    {
                        moduleNames += it->first + " ";
                    }
                }
                else
                {
                    moduleNames += names[i] + " ";
                }
            }
        }

        // 重新加载路由
        // if (loadToNormal() == 0 && loadToTransfer() == 0)
        return loadRouteToMem();
        /*
        if(loadRouteToMem() == 0)
            return 0;
        else
            return -2;
        */
    }

    return 1;  // 正在迁移，不能整理路由表
}

int DbHandle::defragMemRecord(const TransferInfo &transferInfo)
{
    TC_ThreadLock::Lock lock(_lock);

    map<string, PackTable>::iterator it = _mapPackTables->find(transferInfo.moduleName);

    if (it != _mapPackTables->end())
    {
        vector<RecordInfo> &recordList = (it->second).recordList;
        vector<RecordInfo>::reverse_iterator revIt = recordList.rbegin();
        for (; revIt != recordList.rend();)
        {
            if (revIt->fromPageNo >= transferInfo.fromPageNo &&
                revIt->toPageNo <= transferInfo.toPageNo)
            {
                recordList.erase((++revIt).base());
            }
            else
            {
                ++revIt;
            }
        }

        RecordInfo record;
        record.id = 0;
        record.fromPageNo = transferInfo.fromPageNo;
        record.toPageNo = transferInfo.toPageNo;
        record.moduleName = transferInfo.moduleName;
        record.groupName = transferInfo.transGroupName;
        recordList.push_back(record);

        ++(it->second.info.version);  //是否需要增加版本号呢？  ？  ？ add by smitchzhao@ 04-29

        return 0;
    }

    return -1;
}

int DbHandle::addMemTransferRecord(const TransferInfo &transferInfo)
{
    TC_ThreadLock::Lock lock(_transLock);
    if (g_app.checkModuleSwitching(transferInfo) != 0)
    {
        return -1;
    }
    FDLOG("TransferSwitch2") << __LINE__ << "|" << __FUNCTION__ << "|"
                             << "tranid:" << transferInfo.id << "|"
                             << " pthread_id: " << pthread_self()
                             << "module without tranfer or switch collision " << endl;
    map<string, map<string, TransferInfo>>::iterator _it =
        _mapTransferInfos.find(transferInfo.moduleName);
    if (_it != _mapTransferInfos.end())
    {
        map<string, TransferInfo> &_m = _it->second;
        map<string, TransferInfo>::iterator _it1 = _m.begin();
        for (; _it1 != _m.end(); ++_it1)
        {
            //不允许同时做源和目的迁移服务器
            if (transferInfo.groupName == (_it1->second).transGroupName ||
                transferInfo.transGroupName == (_it1->second).groupName)
            {
                FDLOG("TransferSwitch") << __LINE__ << "|" << __FUNCTION__ << "|"
                                        << "tranid:" << transferInfo.id << "|"
                                        << " pthread_id: " << pthread_self()
                                        << "same source and dest, return -1" << endl;
                return -1;
            }

            //不允许迁移页交错
            if (transferInfo.fromPageNo <= (_it1->second).toPageNo &&
                transferInfo.toPageNo >= (_it1->second).fromPageNo)
            {
                FDLOG("TransferSwitch")
                    << __LINE__ << "|" << __FUNCTION__ << "|"
                    << "tranid:" << transferInfo.id << "|"
                    << " pthread_id: " << pthread_self() << "Page overlap, return -1" << endl;
                return -1;
            }
        }
        _m[TC_Common::tostr<pthread_t>(pthread_self())] = transferInfo;
    }
    else
    {
        //模块迁移时的打包路由信息和迁移打包路由信息的同步
        _mapTransferInfos[transferInfo.moduleName][TC_Common::tostr<pthread_t>(pthread_self())] =
            transferInfo;
    }
    FDLOG("TransferSwitch") << __LINE__ << "|" << __FUNCTION__ << "|"
                            << "tranid:" << transferInfo.id << "|"
                            << " pthread_id: " << pthread_self() << "addMemTransferRecord return 0"
                            << endl;
    return 0;
}

int DbHandle::updateTransferingRecord(const TransferInfo *const pTransInfoNew,
                                      const TransferInfo *const pTransInfoComplete,
                                      bool bCleanSrc,
                                      bool bCleanDest,
                                      bool &bSrcHasInc,
                                      bool &bDestHasInc,
                                      int iReturn)
{
    TC_ThreadLock::Lock lock(_transLock);

    if (pTransInfoComplete)
    {
        if (!hasTransfering(pTransInfoComplete->moduleName, pthread_self()))
        {
            return -1;
        }
        map<string, ModuleTransferingInfo>::iterator _it =
            _mapTransferingInfos.find(pTransInfoComplete->moduleName);
        if (_it != _mapTransferingInfos.end())
        {
            ++(_it->second.version);
            vector<TransferInfo> &_v = _it->second.transferingInfoList;
            unsigned int i = 0;
            for (; i < _v.size(); ++i)
            {
                if (_v[i] == *pTransInfoComplete)
                {
                    break;
                }
            }
            if (i == _v.size())
            {
                return -1;
            }

            map<string, int> &_m = _it->second.transferingGroup;
            map<string, int>::iterator _it1 = _m.find(pTransInfoComplete->groupName);
            map<string, int>::iterator _it2 = _m.find(pTransInfoComplete->transGroupName);
            if ((_it1 == _m.end() || _it1->second == 0) || (_it2 == _m.end() || _it2->second == 0))
            {
                _v.erase(_v.begin() + i);
                if (_v.empty() && _m.empty())
                {
                    _mapTransferingInfos.erase(_it);
                }
                return -1;
            }

            if (pTransInfoNew)
            {
                _v[i] = *pTransInfoNew;
            }
            else
            {
                _v.erase(_v.begin() + i);

                if (--(_it1->second) == 0)
                {
                    _m.erase(_it1);
                }
                if (--(_it2->second) == 0)
                {
                    _m.erase(_it2);
                }
                if (_v.empty() && _m.empty())
                {
                    _mapTransferingInfos.erase(_it);
                }
            }
        }
        else
        {
            return -1;
        }
    }
    else if (pTransInfoNew)
    {
        if (!hasTransfering(pTransInfoNew->moduleName, pthread_self()))
        {
            return -1;
        }

        ModuleTransferingInfo &_info = _mapTransferingInfos[pTransInfoNew->moduleName];

        map<string, int>::iterator _it1 = _info.transferingGroup.find(pTransInfoNew->groupName);
        map<string, int>::iterator _it2 =
            _info.transferingGroup.find(pTransInfoNew->transGroupName);
        if (_it1 == _info.transferingGroup.end())
        {
            _info.transferingGroup[pTransInfoNew->groupName] = 0;

            if (_it2 == _info.transferingGroup.end())
            {
                _info.transferingGroup[pTransInfoNew->transGroupName] = 0;
                return 2;
            }
            return 1;
        }
        else if (_it2 == _info.transferingGroup.end())
        {
            _info.transferingGroup[pTransInfoNew->transGroupName] = 0;
            return 3;
        }
        else
        {
            if (iReturn == 0)
            {
                if ((_it1->second == 0 && !bCleanSrc) || (_it2->second == 0 && !bCleanDest))
                {
                    TLOGERROR(FILE_FUN << "return 4:" << _it1->second << " " << bCleanSrc << " "
                                       << _it2->second << " " << bCleanDest << endl);
                    return 4;
                }
                else
                {
                    _info.transferingInfoList.push_back(*pTransInfoNew);
                    ++(_it1->second);
                    ++(_it2->second);
                    ++(_info.version);
                }
            }
            else if (iReturn == 1)
            {
                if (_it1->second == 0 && bCleanSrc)
                {
                    _it1->second++;
                    bSrcHasInc = true;
                    TLOGERROR(FILE_FUN << "return 4 src++ iReturn==1:" << _it1->second << " "
                                       << bCleanSrc << " " << _it2->second << " " << bCleanDest
                                       << endl);
                    return 4;
                }

                if (_it2->second == 0 && !bCleanDest)
                {
                    TLOGERROR(FILE_FUN << "return 4:" << _it1->second << " " << bCleanSrc << " "
                                       << _it2->second << " " << bCleanDest << endl);
                    return 4;
                }
                _info.transferingInfoList.push_back(*pTransInfoNew);
                if (!bSrcHasInc) ++(_it1->second);
                if (!bDestHasInc) ++(_it2->second);
                ++(_info.version);
            }
            else if (iReturn == 2)
            {
                if (_it1->second == 0 && bCleanSrc)
                {
                    _it1->second++;
                    bSrcHasInc = true;
                    TLOGERROR(FILE_FUN << "return 4 src++:" << _it1->second << " " << bCleanSrc
                                       << " " << _it2->second << " " << bCleanDest << endl);
                    return 4;
                }
                if (_it2->second == 0 && bCleanDest)
                {
                    _it2->second++;
                    bDestHasInc = true;
                    TLOGERROR(FILE_FUN << "return 4:" << _it1->second << " " << bCleanSrc << " "
                                       << _it2->second << " " << bCleanDest << endl);
                    return 4;
                }

                _info.transferingInfoList.push_back(*pTransInfoNew);
                if (!bSrcHasInc) ++(_it1->second);
                if (!bDestHasInc) ++(_it2->second);
                ++(_info.version);
            }
            else if (iReturn == 3)
            {
                if (_it2->second == 0 && bCleanDest)
                {
                    _it2->second++;
                    bDestHasInc = true;
                    TLOGERROR(FILE_FUN << "return 4:" << _it1->second << " " << bCleanSrc << " "
                                       << _it2->second << " " << bCleanDest << endl);
                    return 4;
                }
                if (_it1->second == 0 && !bCleanSrc)
                {
                    TLOGERROR(FILE_FUN << "return 4 src++ iReturn==1:" << _it1->second << " "
                                       << bCleanSrc << " " << _it2->second << " " << bCleanDest
                                       << endl);
                    return 4;
                }
                _info.transferingInfoList.push_back(*pTransInfoNew);
                if (!bSrcHasInc) ++(_it1->second);
                if (!bDestHasInc) ++(_it2->second);
                ++(_info.version);
            }
            else if (iReturn == 4)
            {
                if ((_it1->second == 0 && !bCleanSrc) || (_it2->second == 0 && !bCleanDest))
                {
                    TLOGERROR(FILE_FUN << "return 4:" << _it1->second << " " << bCleanSrc << " "
                                       << _it2->second << " " << bCleanDest << endl);
                    return 4;
                }
                else
                {
                    _info.transferingInfoList.push_back(*pTransInfoNew);
                    if (!bSrcHasInc) ++(_it1->second);
                    if (!bDestHasInc) ++(_it2->second);
                    ++(_info.version);
                }
            }
        }
    }

    return 0;
}

int DbHandle::updateTransferingRecord(const string &moduleName,
                                      const string &groupName1,
                                      const string &groupName2)
{
    int ret = 0;
    TC_ThreadLock::Lock lock(_transLock);
    map<string, ModuleTransferingInfo>::iterator it = _mapTransferingInfos.find(moduleName);

    if (it != _mapTransferingInfos.end())
    {
        ModuleTransferingInfo &info = it->second;
        map<string, int>::iterator it1 = info.transferingGroup.find(groupName1);
        if (it1 != info.transferingGroup.end())
        {
            if (it1->second == 0)
            {
                info.transferingGroup.erase(it1);
            }
            else
            {
                if (--(it1->second) == 0)
                {
                    info.transferingGroup.erase(it1);
                }
            }
        }
        else
        {
            ret = -1;
        }

        if (!groupName2.empty())
        {
            it1 = info.transferingGroup.find(groupName2);
            if (it1 != info.transferingGroup.end())
            {
                if (it1->second == 0)
                {
                    info.transferingGroup.erase(it1);
                }
                else
                {
                    if (--(it1->second) == 0)
                    {
                        info.transferingGroup.erase(it1);
                    }
                }
            }
            else
            {
                ret = -1;
            }
        }

        if (info.transferingInfoList.empty() && info.transferingGroup.empty())
        {
            _mapTransferingInfos.erase(it);
        }
        return 0;
    }
    else
    {
        ret = -1;
    }

    return ret;
}

int DbHandle::writeDbTransferRecord(TransferInfo &transferInfo)
{
    if (transferInfo.id > 0) return 0;

    try
    {
        TC_Mysql::RECORD_DATA insertData;
        insertData["from_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(transferInfo.fromPageNo));
        insertData["to_page_no"] = make_pair(TC_Mysql::DB_INT, I2S(transferInfo.toPageNo));
        insertData["module_name"] = make_pair(TC_Mysql::DB_STR, transferInfo.moduleName);
        insertData["group_name"] = make_pair(TC_Mysql::DB_STR, transferInfo.groupName);
        insertData["trans_group_name"] = make_pair(TC_Mysql::DB_STR, transferInfo.transGroupName);
        insertData["state"] = make_pair(TC_Mysql::DB_INT, I2S(TRANSFERING));
        TC_ThreadLock::Lock lock(_dbLock);
        int affect = _mysql->insertRecord("t_router_transfer", insertData);
        if (affect != 1)
        {
            DAY_ERROR << "DbHandle::writeDbTransferRecord mysql insertRecord return: " << affect
                      << endl;
            TARS_NOTIFY_WARN(string("DbHandle::writeDbTransferRecord|mysql insertRecord return: ") +
                             I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        transferInfo.id = _mysql->lastInsertID();
        TLOGDEBUG(FILE_FUN << __FUNCTION__ << " _mysql->lastInsertID return: " << transferInfo.id
                           << endl);
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::writeDbTransferRecord exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::writeDbTransferRecord|") + ex.what());
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }

    return 0;
}

int DbHandle::removeMemTransferRecord(const TransferInfo &transferInfo, bool &bModuleComplete)
{
    TC_ThreadLock::Lock lock(_transLock);
    map<string, map<string, TransferInfo>>::iterator it =
        _mapTransferInfos.find(transferInfo.moduleName);
    if (it != _mapTransferInfos.end())
    {
        map<string, TransferInfo> &m = it->second;
        map<string, TransferInfo>::iterator it1 =
            m.find(TC_Common::tostr<pthread_t>(pthread_self()));
        if (it1 != m.end())
        {
            m.erase(it1);
            if (m.empty())
            {
                bModuleComplete = true;
                _mapTransferInfos.erase(it);
                _mapTransferingInfos.erase(transferInfo.moduleName);
            }

            FDLOG("TransferSwitch") << __LINE__ << "|" << __FUNCTION__ << "|"
                                    << "tranid:" << transferInfo.id << "|"
                                    << "pthread_id: " << pthread_self() << "|"
                                    << " removeMemTransferRecord success" << endl;
            return 0;
        }
        FDLOG("TransferSwitch") << __LINE__ << "|" << __FUNCTION__ << "|"
                                << "tranid:" << transferInfo.id << "|"
                                << "pthread_id: " << pthread_self() << "|"
                                << " removeMemTransferRecord fail1" << endl;
    }
    else
    {
        FDLOG("TransferSwitch") << __LINE__ << "|" << __FUNCTION__ << "|"
                                << "tranid:" << transferInfo.id << "|"
                                << "pthread_id: " << pthread_self() << "|"
                                << " removeMemTransferRecord fail2" << endl;
        bModuleComplete = true;
    }

    return -1;
}

int DbHandle::updateVersion(const string &sModuleName)
{
    try
    {
        string sSql =
            "update t_router_module set version = version + 1 where "
            "module_name = '" +
            sModuleName + "'";
        _mysql->execute(sSql);
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::updateVersion " << sModuleName << " exception: " << ex.what()
                  << endl;
        TARS_NOTIFY_WARN(string("DbHandle::updateVersion|") + ex.what() + ": " + sModuleName);
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }

    return 0;
}

int DbHandle::setTransferStatus(const TransferInfo &transferInfo,
                                eTransferStatus eway,
                                const string &remark)
{
    if (transferInfo.id <= 0) return -1;
    try
    {
        TC_Mysql::RECORD_DATA updateData;
        updateData["remark"] = make_pair(TC_Mysql::DB_STR, remark);
        string sSql;

        if (eway == TRANSFERING || transferInfo.toPageNo == -1)
        {
            updateData["state"] = make_pair(TC_Mysql::DB_INT, I2S(eway));
            sSql = "where id = " + I2S(transferInfo.id);
        }
        else if (eway == SET_TRANSFER_PAGE)  // 设置当前迁移成功页
        {
            updateData["state"] = make_pair(TC_Mysql::DB_INT, I2S(TRANSFERING));
            updateData["transfered_page_no"] =
                make_pair(TC_Mysql::DB_INT, I2S(transferInfo.toPageNo));
            sSql = "where id = " + I2S(transferInfo.id) + " and state = " + I2S(TRANSFERING);
        }
        else
        {
            updateData["transfered_page_no"] =
                make_pair(TC_Mysql::DB_INT, I2S(transferInfo.toPageNo));
            updateData["state"] = make_pair(TC_Mysql::DB_INT, I2S(eway));
            updateData["endTime"] = make_pair(TC_Mysql::DB_INT, "now()");
            sSql = "where id = " + I2S(transferInfo.id);
        }

        TC_ThreadLock::Lock lock(_dbLock);
        int affect = _mysql->updateRecord("t_router_transfer", updateData, sSql);
        if (affect != 1)
        {
            DAY_ERROR << "DbHandle::setTransferStatus mysql updateRecord return: " << affect
                      << endl;
            TARS_NOTIFY_WARN(string("DbHandle::setTransferStatus|mysql updateRecord return: ") +
                             I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            FDLOG("TransferSwitch")
                << __LINE__ << "|" << __FUNCTION__ << "|"
                << "tranid:" << transferInfo.id << "|"
                << " pthread_id: " << pthread_self() << " affect: " << affect << endl;
            return -1;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::setTransferStatus exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::setTransferStatus|") + ex.what());
        if (_exRep)
        {
            _exRep->report(1);
        }
        FDLOG("TransferSwitch") << __LINE__ << "|" << __FUNCTION__ << "|"
                                << "tranid:" << transferInfo.id << "|"
                                << " pthread_id: " << pthread_self()
                                << "DbHandle::setTransferStatus exception: " << ex.what() << endl;
        return -1;
    }

    return 0;
}

int DbHandle::setStartTransferStatus(const TransferInfo &transferInfo)
{
    if (transferInfo.id <= 0) return -1;
    try
    {
        TC_Mysql::RECORD_DATA updateData;
        updateData["remark"] = make_pair(TC_Mysql::DB_STR, "start");
        string sSql;

        updateData["state"] = make_pair(TC_Mysql::DB_INT, I2S(TRANSFERING));

        sSql = "where id = " + I2S(transferInfo.id) + " and state = " + I2S(TRANSFERING);

        TC_ThreadLock::Lock lock(_dbLock);
        int affect = _mysql->updateRecord("t_router_transfer", updateData, sSql);
        if (affect != 1)
        {
            DAY_ERROR << "DbHandle::setStartTransferStatus mysql updateRecord return: " << affect
                      << endl;
            FDLOG("TransferSwitch2")
                << __LINE__ << "|" << __FUNCTION__ << "|"
                << "tranid:" << transferInfo.id << "|"
                << "pthread_id: " << pthread_self()
                << "DbHandle::setStartTransferStatus mysql updateRecord return: " << affect << endl;
            TARS_NOTIFY_WARN(
                string("DbHandle::setStartTransferStatus|mysql updateRecord return: ") +
                I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::setStartTransferStatus exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::setStartTransferStatus|") + ex.what());
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }

    return 0;
}

int DbHandle::getTransferTask(TransferInfo &transferInfo)
{
    transferInfo.id = -1;
    transferInfo.fromPageNo = -1;
    transferInfo.toPageNo = -1;
    transferInfo.moduleName.clear();
    transferInfo.groupName.clear();
    transferInfo.transGroupName.clear();

    try
    {
        TC_ThreadLock::Lock lock(_dbLock);
        string sql =
            "select id, module_name, from_page_no, to_page_no, "
            "group_name, trans_group_name from t_router_transfer where "
            "state = " +
            I2S(UNTRANSFER) + " order by id asc limit 1";
        TC_Mysql::MysqlData sqlData = _mysql->queryRecord(sql);
        if (sqlData.size() > 0)
        {
            transferInfo.id = S2I(sqlData[0]["id"]);
            transferInfo.fromPageNo = S2I(sqlData[0]["from_page_no"]);
            transferInfo.toPageNo = S2I(sqlData[0]["to_page_no"]);
            transferInfo.moduleName = sqlData[0]["module_name"];
            transferInfo.groupName = sqlData[0]["group_name"];
            transferInfo.transGroupName = sqlData[0]["trans_group_name"];

            ostringstream os;
            transferInfo.displaySimple(os);
            TLOGDEBUG(FILE_FUN << __FUNCTION__ << ": " << os.str() << endl);

            TC_Mysql::RECORD_DATA updateData;
            updateData["state"] = make_pair(TC_Mysql::DB_INT, I2S(TRANSFERING));
            updateData["startTime"] = make_pair(TC_Mysql::DB_INT, "now()");
            sql = "where id = " + I2S(transferInfo.id) + " and state = " + I2S(UNTRANSFER);
            int affect = _mysql->updateRecord("t_router_transfer", updateData, sql);
            if (affect != 1)
            {
                DAY_ERROR << "DbHandle::getTransferTask mysql updateRecord return: " << affect
                          << endl;
                TARS_NOTIFY_WARN(string("DbHandle::getTransferTask|mysql updateRecord return: ") +
                                 I2S(affect));
                if (_exRep)
                {
                    _exRep->report(1);
                }
                return -1;
            }
            return 0;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::getTransferTask exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::getTransferTask|") + ex.what());
        if (_exRep)
        {
            _exRep->report(1);
        }
    }

    return -1;
}

int DbHandle::switchMasterAndSlaveInDbAndMem(const string &moduleName,
                                             const string &groupName,
                                             const string &lastMaster,
                                             const string &lastSlave,
                                             PackTable &packTable)
{
    string sSql = "where module_name='" + moduleName + "' and group_name='" + groupName + "'";

    try
    {
        TC_ThreadLock::Lock lock(_dbLock);
        TC_ThreadLock::Lock lock1(_lock);
        TC_Mysql::RECORD_DATA updateData;

        updateData["server_status"] = make_pair(TC_Mysql::DB_STR, "S");
        updateData["source_server_name"] = make_pair(TC_Mysql::DB_STR, lastSlave);
        updateData["access_status"] = make_pair(TC_Mysql::DB_INT, "0");
        updateData["priority"] = make_pair(TC_Mysql::DB_INT, "3");
        int affect = _mysql->updateRecord(
            "t_router_group", updateData, sSql + " and server_name='" + lastMaster + "'");
        if (affect != 1)
        {
            DAY_ERROR << "DbHandle::switchMasterAndSlaveInDbAndMem " << affect << endl;
            TARS_NOTIFY_WARN(
                string("DbHandle::switchMasterAndSlaveInDbAndMem|mysql updateRecord return: ") +
                I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        updateData["server_status"] = make_pair(TC_Mysql::DB_STR, "M");
        updateData["source_server_name"] = make_pair(TC_Mysql::DB_STR, "");
        updateData["access_status"] = make_pair(TC_Mysql::DB_INT, "0");
        updateData["priority"] = make_pair(TC_Mysql::DB_INT, "1");
        affect = _mysql->updateRecord(
            "t_router_group", updateData, sSql + " and server_name='" + lastSlave + "'");
        if (affect != 1)
        {
            DAY_ERROR << "DbHandle::switchMasterAndSlaveInDbAndMem " << affect << endl;
            TARS_NOTIFY_WARN(
                string("DbHandle::switchMasterAndSlaveInDbAndMem|mysql updateRecord return: ") +
                I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        updateData.clear();
        updateData["source_server_name"] = make_pair(TC_Mysql::DB_STR, lastSlave);
        updateData["access_status"] = make_pair(TC_Mysql::DB_INT, "0");
        //如果是带镜像备机的 镜像备机的同步源不变
        _mysql->updateRecord(
            "t_router_group",
            updateData,
            sSql + " and  server_name!='" + lastSlave + "'" + " and  server_status!='B'");
        updateVersion(moduleName);
        // Changed by tutuli 2017-7-18 修复同一个模块切换时，路由信息更新只更新有变更的组
        packTable.recordList = (*_mapPackTables)[moduleName].recordList;
        // 更新全局路由信息时，仅更新相关切换的组的信息

        TLOGDEBUG(FILE_FUN << "moduleName :" << moduleName << " groupName : " << groupName << endl);
        map<string, GroupInfo>::iterator groupIt =
            (*_mapPackTables)[moduleName].groupList.find(groupName);
        // 没有找到旧的组名
        if (groupIt == (*_mapPackTables)[moduleName].groupList.end())
        {
            TLOGERROR(FILE_FUN << "can not find groupName in global packTable,groupName:"
                               << groupName << endl);
            return -1;
        }
        map<string, GroupInfo>::iterator newGroupIt = packTable.groupList.find(groupName);
        if (newGroupIt == packTable.groupList.end())
        {
            TLOGERROR(FILE_FUN << "can not find groupName in new packTable,groupName:" << groupName
                               << endl);
            return -1;
        }
        // 给全局的结构赋值
        groupIt->second = newGroupIt->second;

        // 修改serverInfo,这个真
        string sIdc("");
        // 找到旧的信息
        map<string, ServerInfo>::iterator odlServerInfoIt =
                                              (*_mapPackTables)[moduleName].serverList.begin(),
                                          odlServerInfoItEnd =
                                              (*_mapPackTables)[moduleName].serverList.end();
        for (; odlServerInfoIt != odlServerInfoItEnd; ++odlServerInfoIt)
        {
            if (odlServerInfoIt->second.ServerStatus == "M" &&
                odlServerInfoIt->second.groupName == groupName)
            {
                sIdc = odlServerInfoIt->second.idc;
                break;
            }
        }
        map<string, ServerInfo>::iterator odlServerInfoBakIt =
                                              (*_mapPackTables)[moduleName].serverList.begin(),
                                          odlServerInfoBakItEnd =
                                              (*_mapPackTables)[moduleName].serverList.end();
        for (; odlServerInfoBakIt != odlServerInfoBakItEnd; ++odlServerInfoBakIt)
        {
            if (odlServerInfoBakIt->second.ServerStatus == "S" &&
                odlServerInfoBakIt->second.groupName == groupName &&
                odlServerInfoBakIt->second.idc == sIdc)
            {
                break;
            }
        }
        if (odlServerInfoIt == odlServerInfoItEnd || odlServerInfoBakIt == odlServerInfoBakItEnd)
        {
            TLOGERROR(
                FILE_FUN << "[swicth_by_group_fail]no masterServer or slaveServer find module: "
                         << moduleName << " group: " << groupName << endl);
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }
        // 修改状态
        odlServerInfoIt->second.ServerStatus = "S";
        odlServerInfoBakIt->second.ServerStatus = "M";
        // 版本号+1
        (*_mapPackTables)[moduleName].info.version++;

        // (*_mapPackTables)[moduleName] = packTable;
        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::switchMasterAndSlaveInDbAndMem " << moduleName << "|" << groupName
                  << " exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::switchMasterAndSlaveInDbAndMem|") + ex.what() + ": " +
                         moduleName + "|" + groupName);
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }
}

//生成临时的路由信息
int DbHandle::getPackTable4SwitchRW(const string &moduleName,
                                    const string &groupName,
                                    const string &lastMaster,
                                    const string &lastSlave,
                                    PackTable &tmpPackTable)
{
    try
    {
        PackTable packTable;
        map<string, PackTable>::const_iterator it = _mapPackTables->find(moduleName);
        if (it != _mapPackTables->end())
        {
            packTable = it->second;
        }
        else
        {
            DAY_ERROR << "DbHandle::getPackTable4SwitchRW "
                      << "modify router in mem no find module:" << moduleName << endl;
            TARS_NOTIFY_WARN(string("DbHandle::getPackTable4SwitchRW|no find module:") +
                             moduleName);
            return -1;
        }
        map<string, GroupInfo>::iterator itGroupInfo = packTable.groupList.find(groupName);
        if (itGroupInfo == packTable.groupList.end())
        {
            DAY_ERROR << "DbHandle::getPackTable4SwitchRW "
                      << "modify router in mem no find group:" << groupName << endl;
            TARS_NOTIFY_WARN(
                string("DbHandle::getPackTable4SwitchRW|modify router in mem no find group:") +
                groupName);
            return -1;
        }
        itGroupInfo->second.accessStatus = 0;
        string sIdc;
        //生成临时的切换后的路由
        map<string, ServerInfo>::iterator it1 = packTable.serverList.begin(),
                                          it1End = packTable.serverList.end();
        for (; it1 != it1End; ++it1)
        {
            if (it1->second.ServerStatus == "M" && it1->second.groupName == groupName)
            {
                sIdc = it1->second.idc;
                break;
            }
        }
        if (it1 == it1End)
        {
            DAY_ERROR << "DbHandle::getPackTable4SwitchRW "
                      << "modify router in mem no find master groupName:" << groupName << endl;
            TARS_NOTIFY_WARN(string("DbHandle::getPackTable4SwitchRW|modify router in mem no find "
                                    "master groupName:") +
                             groupName);
            return -1;
        }
        //找备机
        map<string, ServerInfo>::iterator it2 = packTable.serverList.begin(),
                                          it2End = packTable.serverList.end();
        for (; it2 != it2End; ++it2)
        {
            if (it2->second.ServerStatus == "S" && it2->second.groupName == groupName &&
                it2->second.idc == sIdc)
            {
                break;
            }
        }
        if (it2 == it2End)
        {
            DAY_ERROR << "DbHandle::getPackTable4SwitchRW "
                      << "modify router in mem no find slave groupName:" << groupName << endl;
            TARS_NOTIFY_WARN(string("DbHandle::getPackTable4SwitchRW|modify router in mem no find "
                                    "slave  groupName:") +
                             groupName);
            return -1;
        }
        TLOGDEBUG(FILE_FUN << "curMasterServer" << lastMaster << " "
                           << "curSlaveServer" << lastSlave << endl);
        // it指向组信息 it1指向主机，it2指向要切合的备机 修改路由
        it1->second.ServerStatus = "S";
        it2->second.ServerStatus = "M";
        itGroupInfo->second.masterServer = lastSlave;
        itGroupInfo->second.bakList[lastSlave] = "";
        map<string, string>::iterator it3 = itGroupInfo->second.bakList.begin(),
                                      it3End = itGroupInfo->second.bakList.end();
        for (; it3 != it3End; ++it3)
        {
            if (it3->first != lastSlave) it3->second = lastSlave;
        }
        vector<string> &vIdcList = itGroupInfo->second.idcList[sIdc];
        vector<string>::iterator vIt = vIdcList.begin(), vItEnd = vIdcList.end();
        while (vIt != vItEnd)
        {
            if (*vIt == lastSlave)
            {
                vIdcList.erase(vIt);
                break;
            }
            ++vIt;
        }

        //修改服务可读状态
        map<string, ServerInfo>::iterator its = packTable.serverList.find(lastMaster);
        if (its != packTable.serverList.end())
        {
            its->second.status = -1;
        }

        vector<string> vIdcListNew;
        vIdcListNew.push_back(lastSlave);
        vIdcListNew.insert(vIdcListNew.end(), vIdcList.begin(), vIdcList.end());
        itGroupInfo->second.idcList[sIdc] = vIdcListNew;
        // FIXME, 如果在此次下发临时路由和修改内存、DB中路由之间，
        //有变更发生，Cache再次同步路由是否会漏掉该变更
        ++packTable.info.version;

        tmpPackTable = packTable;
        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::DbHandle::getPackTable4SwitchRW " << moduleName << "|" << groupName
                  << " exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::DbHandle::getPackTable4SwitchRW|") + ex.what() + ": " +
                         moduleName + "|" + groupName);
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }
}

int DbHandle::switchRWDbAndMem(const string &moduleName,
                               const string &groupName,
                               const string &lastMaster,
                               const string &lastSlave)
{
    string sSql = "where module_name='" + moduleName + "' and group_name='" + groupName + "'";

    try
    {
        TC_ThreadLock::Lock lock(_dbLock);
        TC_ThreadLock::Lock lock1(_lock);
        TC_Mysql::RECORD_DATA updateData;

        updateData["server_status"] = make_pair(TC_Mysql::DB_STR, "S");
        updateData["source_server_name"] = make_pair(TC_Mysql::DB_STR, lastSlave);
        updateData["access_status"] = make_pair(TC_Mysql::DB_INT, "0");
        updateData["priority"] = make_pair(TC_Mysql::DB_INT, "3");
        int affect = _mysql->updateRecord(
            "t_router_group", updateData, sSql + " and server_name='" + lastMaster + "'");
        if (affect != 1)
        {
            DAY_ERROR << "DbHandle::switchRWDbAndMem " << affect << endl;
            TARS_NOTIFY_WARN(string("DbHandle::switchRWDbAndMem|mysql updateRecord return: ") +
                             I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        updateData["server_status"] = make_pair(TC_Mysql::DB_STR, "M");
        updateData["source_server_name"] = make_pair(TC_Mysql::DB_STR, "");
        updateData["access_status"] = make_pair(TC_Mysql::DB_INT, "0");
        updateData["priority"] = make_pair(TC_Mysql::DB_INT, "1");
        affect = _mysql->updateRecord(
            "t_router_group", updateData, sSql + " and server_name='" + lastSlave + "'");
        if (affect != 1)
        {
            DAY_ERROR << "DbHandle::switchRWDbAndMem " << affect << endl;
            TARS_NOTIFY_WARN(string("DbHandle::switchRWDbAndMem|mysql updateRecord return: ") +
                             I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        //修改服务可读状态
        string sTmpSql = "where server_name = '" + lastMaster + "'";
        updateData.clear();
        updateData["status"] = make_pair(TC_Mysql::DB_INT, "-1");
        affect = _mysql->updateRecord("t_router_server", updateData, sTmpSql);
        updateData.clear();
        updateData["source_server_name"] = make_pair(TC_Mysql::DB_STR, lastSlave);
        updateData["access_status"] = make_pair(TC_Mysql::DB_INT, "0");
        _mysql->updateRecord(
            "t_router_group", updateData, sSql + " and  server_name!='" + lastSlave + "'");
        updateVersion(moduleName);
        PackTable packTable;
        map<string, PackTable>::const_iterator it = _mapPackTables->find(moduleName);
        if (it != _mapPackTables->end())
        {
            packTable = it->second;
        }
        else
        {
            DAY_ERROR << "DbHandle::switchRWDbAndMem "
                      << "modify router in mem no find module:" << moduleName << endl;
            TARS_NOTIFY_WARN(
                string("DbHandle::switchRWDbAndMem|modify router in mem no find module:") +
                moduleName);
            return -1;
        }
        map<string, GroupInfo>::iterator itGroupInfo = packTable.groupList.find(groupName);
        if (itGroupInfo == packTable.groupList.end())
        {
            DAY_ERROR << "DbHandle::switchRWDbAndMem "
                      << "modify router in mem no find group:" << groupName << endl;
            TARS_NOTIFY_WARN(
                string("DbHandle::switchRWDbAndMem|modify router in mem no find group:") +
                groupName);
            return -1;
        }
        itGroupInfo->second.accessStatus = 0;
        string sIdc;
        //生成临时的切换后的路由
        map<string, ServerInfo>::iterator it1 = packTable.serverList.begin(),
                                          it1End = packTable.serverList.end();
        for (; it1 != it1End; ++it1)
        {
            if (it1->second.ServerStatus == "M" && it1->second.groupName == groupName)
            {
                sIdc = it1->second.idc;
                break;
            }
        }
        if (it1 == it1End)
        {
            DAY_ERROR << "DbHandle::switchRWDbAndMem "
                      << "modify router in mem no find master groupName:" << groupName << endl;
            TARS_NOTIFY_WARN(
                string(
                    "DbHandle::switchRWDbAndMem|modify router in mem no find master groupName:") +
                groupName);
            return -1;
        }
        //找备机
        map<string, ServerInfo>::iterator it2 = packTable.serverList.begin(),
                                          it2End = packTable.serverList.end();
        for (; it2 != it2End; ++it2)
        {
            if (it2->second.ServerStatus == "S" && it2->second.groupName == groupName &&
                it2->second.idc == sIdc)
            {
                break;
            }
        }
        if (it2 == it2End)
        {
            DAY_ERROR << "DbHandle::switchRWDbAndMem "
                      << "modify router in mem no find slave groupName:" << groupName << endl;
            TARS_NOTIFY_WARN(
                string(
                    "DbHandle::switchRWDbAndMem|modify router in mem no find slave  groupName:") +
                groupName);
            return -1;
        }
        TLOGDEBUG(FILE_FUN << "curMasterServer" << lastMaster << " "
                           << "curSlaveServer" << lastSlave << endl);
        // it指向组信息 it1指向主机，it2指向要切合的备机 修改路由
        it1->second.ServerStatus = "S";
        it2->second.ServerStatus = "M";
        itGroupInfo->second.masterServer = lastSlave;
        itGroupInfo->second.bakList[lastSlave] = "";
        map<string, string>::iterator it3 = itGroupInfo->second.bakList.begin(),
                                      it3End = itGroupInfo->second.bakList.end();
        for (; it3 != it3End; ++it3)
        {
            if (it3->first != lastSlave) it3->second = lastSlave;
        }
        vector<string> &vIdcList = itGroupInfo->second.idcList[sIdc];
        vector<string>::iterator vIt = vIdcList.begin(), vItEnd = vIdcList.end();
        while (vIt != vItEnd)
        {
            if (*vIt == lastSlave)
            {
                vIdcList.erase(vIt);
                break;
            }
            ++vIt;
        }

        //修改服务可读状态
        map<string, ServerInfo>::iterator its = packTable.serverList.find(lastMaster);
        if (its != packTable.serverList.end())
        {
            its->second.status = -1;
        }

        vector<string> vIdcListNew;
        vIdcListNew.push_back(lastSlave);
        vIdcListNew.insert(vIdcListNew.end(), vIdcList.begin(), vIdcList.end());
        itGroupInfo->second.idcList[sIdc] = vIdcListNew;
        ++packTable.info.version;
        (*_mapPackTables)[moduleName] = packTable;
        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::DbHandle::switchRWDbAndMem " << moduleName << "|" << groupName
                  << " exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::DbHandle::switchRWDbAndMem|") + ex.what() + ": " +
                         moduleName + "|" + groupName);
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }
}
int DbHandle::switchReadOnlyInDbAndMem(const string &moduleName, const string &groupName)
{
    string sSql = "where module_name='" + moduleName + "' and group_name='" + groupName + "'";

    try
    {
        TC_ThreadLock::Lock lock(_dbLock);
        TC_ThreadLock::Lock lock1(_lock);
        TC_Mysql::RECORD_DATA updateData;

        updateData["access_status"] = make_pair(TC_Mysql::DB_INT, "1");
        int affect = _mysql->updateRecord("t_router_group", updateData, sSql);
        if (affect <= 0)
        {
            DAY_ERROR << "DbHandle::switchReadOnlyInDbAndMem " << affect << endl;
            TARS_NOTIFY_WARN(
                string("DbHandle::switchReadOnlyInDbAndMemmysql updateRecord return: ") +
                I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }
        updateVersion(moduleName);
        map<string, PackTable>::iterator it = _mapPackTables->find(moduleName);
        if (it != _mapPackTables->end())
        {
            map<string, GroupInfo>::iterator itr = it->second.groupList.find(groupName);
            if (itr != it->second.groupList.end())
            {
                itr->second.accessStatus = 1;
                it->second.info.version++;
            }
            else
            {
                DAY_ERROR << "DbHandle::switchReadOnlyInDbAndMem groupName nofind" << moduleName
                          << "|" << groupName << endl;
                TARS_NOTIFY_WARN(string("DbHandle::switchReadOnlyInDbAndMem groupName nofind|") +
                                 ": " + moduleName + "|" + groupName);
                return -1;
            }
        }
        else
        {
            DAY_ERROR << "DbHandle::switchReadOnlyInDbAndMem moduleName nofind" << moduleName << "|"
                      << groupName << endl;
            TARS_NOTIFY_WARN(string("DbHandle::switchReadOnlyInDbAndMem moduleName nofind|") +
                             ": " + moduleName + "|" + groupName);
            return -1;
        }
        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::switchReadOnlyInDbAndMem " << moduleName << "|" << groupName
                  << " exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::switchReadOnlyInDbAndMem|") + ex.what() + ": " +
                         moduleName + "|" + groupName);
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }
}

int DbHandle::switchMirrorInDbAndMem(const string &moduleName,
                                     const string &groupName,
                                     const string &serverName)
{
    string sSql = "where module_name='" + moduleName + "' and group_name='" + groupName + "'";

    try
    {
        TC_ThreadLock::Lock lock(_dbLock);
        TC_ThreadLock::Lock lock1(_lock);
        TC_Mysql::RECORD_DATA updateData;

        //更新服务组状态
        updateData["access_status"] = make_pair(TC_Mysql::DB_INT, "2");
        int affect = _mysql->updateRecord("t_router_group", updateData, sSql);
        if (affect <= 0)
        {
            DAY_ERROR << "DbHandle::switchMirrorInDbAndMem t_router_group access_status " << affect
                      << endl;
            TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMem updateRecord t_router_group "
                                    "access_status return: ") +
                             I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        string sTmpSql = "where server_name = '" + serverName + "'";
        updateData.clear();
        updateData["status"] = make_pair(TC_Mysql::DB_INT, "-1");
        affect = _mysql->updateRecord("t_router_server", updateData, sTmpSql);
        if (affect <= 0)
        {
            DAY_ERROR << "DbHandle::switchMirrorInDbAndMem t_router_server server_name " << affect
                      << endl;
            TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMem updateRecord t_router_server "
                                    "server_name return: ") +
                             I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        updateVersion(moduleName);
        map<string, PackTable>::iterator it = _mapPackTables->find(moduleName);
        if (it != _mapPackTables->end())
        {
            map<string, GroupInfo>::iterator itr = it->second.groupList.find(groupName);
            if (itr != it->second.groupList.end())
            {
                itr->second.accessStatus = 2;
                it->second.info.version++;
            }
            else
            {
                DAY_ERROR << "DbHandle::switchMirrorInDbAndMem groupName nofind" << moduleName
                          << "|" << groupName << endl;
                TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMem groupName nofind|") +
                                 ": " + moduleName + "|" + groupName);
                return -1;
            }

            map<string, ServerInfo>::iterator its = it->second.serverList.find(serverName);
            if (its != it->second.serverList.end())
            {
                its->second.status = -1;
            }
            else
            {
                DAY_ERROR << "DbHandle::switchMirrorInDbAndMem serverName nofind " << moduleName
                          << "|" << groupName << "|" << serverName << endl;
                TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMem serverName nofind |") +
                                 ": " + moduleName + "|" + groupName + "|" + serverName);
                return -1;
            }
        }
        else
        {
            DAY_ERROR << "DbHandle::switchMirrorInDbAndMem moduleName nofind" << moduleName << "|"
                      << groupName << endl;
            TARS_NOTIFY_WARN(string("DbHandle::switchReadOnlyInDbAndMem moduleName nofind|") +
                             ": " + moduleName + "|" + groupName);
            return -1;
        }
        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::switchMirrorInDbAndMem " << moduleName << "|" << groupName
                  << " exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMem|") + ex.what() + ": " +
                         moduleName + "|" + groupName);
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }
}
/*镜像切换流程*/

int DbHandle::recoverMirrorInDbAndMem(const string &moduleName,
                                      const string &groupName,
                                      const string &serverName)
{
    string sSql = "where module_name='" + moduleName + "' and group_name='" + groupName + "'";

    try
    {
        TC_ThreadLock::Lock lock(_dbLock);
        TC_ThreadLock::Lock lock1(_lock);
        TC_Mysql::RECORD_DATA updateData;

        updateData["access_status"] = make_pair(TC_Mysql::DB_INT, "0");
        int affect = _mysql->updateRecord("t_router_group", updateData, sSql);
        if (affect <= 0)
        {
            DAY_ERROR << "DbHandle::recoverMirrorInDbAndMem " << affect << endl;
            TARS_NOTIFY_WARN(string("DbHandle::recoverMirrorInDbAndMem updateRecord return: ") +
                             I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        string sTmpSql = "where server_name = '" + serverName + "'";
        updateData.clear();
        updateData["status"] = make_pair(TC_Mysql::DB_INT, "0");
        affect = _mysql->updateRecord("t_router_server", updateData, sTmpSql);
        if (affect <= 0)
        {
            DAY_ERROR << "DbHandle::switchMirrorInDbAndMem t_router_server server_name " << affect
                      << endl;
            TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMem updateRecord t_router_server "
                                    "server_name return: ") +
                             I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        updateVersion(moduleName);
        map<string, PackTable>::iterator it = _mapPackTables->find(moduleName);
        if (it != _mapPackTables->end())
        {
            map<string, GroupInfo>::iterator itr = it->second.groupList.find(groupName);
            if (itr != it->second.groupList.end())
            {
                itr->second.accessStatus = 0;
                it->second.info.version++;
            }
            else
            {
                DAY_ERROR << "DbHandle::recoverMirrorInDbAndMem groupName nofind" << moduleName
                          << "|" << groupName << endl;
                TARS_NOTIFY_WARN(string("DbHandle::recoverMirrorInDbAndMem groupName nofind|") +
                                 ": " + moduleName + "|" + groupName);
                return -1;
            }

            map<string, ServerInfo>::iterator its = it->second.serverList.find(serverName);
            if (its != it->second.serverList.end())
            {
                its->second.status = 0;
            }
            else
            {
                DAY_ERROR << "DbHandle::switchMirrorInDbAndMem serverName nofind " << moduleName
                          << "|" << groupName << "|" << serverName << endl;
                TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMem serverName nofind |") +
                                 ": " + moduleName + "|" + groupName + "|" + serverName);
                return -1;
            }
        }
        else
        {
            DAY_ERROR << "DbHandle::recoverMirrorInDbAndMem moduleName nofind" << moduleName << "|"
                      << groupName << endl;
            TARS_NOTIFY_WARN(string("DbHandle::recoverMirrorInDbAndMem moduleName nofind|") + ": " +
                             moduleName + "|" + groupName);
            return -1;
        }
        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::recoverMirrorInDbAndMem " << moduleName << "|" << groupName
                  << " exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::recoverMirrorInDbAndMem|") + ex.what() + ": " +
                         moduleName + "|" + groupName);
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }
}

int DbHandle::setServerstatus(const string &moduleName,
                              const string &groupName,
                              const string &serverName,
                              const int iStatus)
{
    try
    {
        string sSql;

        TC_ThreadLock::Lock lock(_dbLock);
        TC_ThreadLock::Lock lock1(_lock);
        TC_Mysql::RECORD_DATA updateData;

        sSql = "where server_name = '" + serverName + "'";
        updateData.clear();
        updateData["status"] = make_pair(TC_Mysql::DB_INT, TC_Common::tostr(iStatus));
        int affect = _mysql->updateRecord("t_router_server", updateData, sSql);
        if (affect <= 0)
        {
            DAY_ERROR << "DbHandle::setServerstatus t_router_server server_name " << affect << endl;
            TARS_NOTIFY_WARN(
                string(
                    "DbHandle::setServerstatus updateRecord t_router_server server_name return: ") +
                I2S(affect));
            if (_exRep)
            {
                _exRep->report(1);
            }
            return -1;
        }

        updateVersion(moduleName);
        map<string, PackTable>::iterator it = _mapPackTables->find(moduleName);
        if (it != _mapPackTables->end())
        {
            map<string, GroupInfo>::iterator itr = it->second.groupList.find(groupName);
            if (itr != it->second.groupList.end())
            {
                it->second.info.version++;
            }
            else
            {
                DAY_ERROR << "DbHandle::setServerstatus groupName nofind" << moduleName << "|"
                          << groupName << endl;
                TARS_NOTIFY_WARN(string("DbHandle::setServerstatus groupName nofind|") + ": " +
                                 moduleName + "|" + groupName);
                return -1;
            }

            map<string, ServerInfo>::iterator its = it->second.serverList.find(serverName);
            if (its != it->second.serverList.end())
            {
                its->second.status = iStatus;
            }
            else
            {
                DAY_ERROR << "DbHandle::setServerstatus serverName nofind " << moduleName << "|"
                          << groupName << "|" << serverName << endl;
                TARS_NOTIFY_WARN(string("DbHandle::setServerstatus serverName nofind |") + ": " +
                                 moduleName + "|" + groupName + "|" + serverName);
                return -1;
            }
        }
        else
        {
            DAY_ERROR << "DbHandle::setServerstatus moduleName nofind" << moduleName << "|"
                      << groupName << endl;
            TARS_NOTIFY_WARN(string("DbHandle::setServerstatus moduleName nofind|") + ": " +
                             moduleName + "|" + groupName);
            return -1;
        }

        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::setServerstatus " << moduleName << "|" << groupName
                  << " exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::setServerstatus|") + ex.what() + ": " + moduleName +
                         "|" + groupName);
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }

    return 0;
}

int DbHandle::switchMirrorInDbAndMemByIdc(const string &moduleName,
                                          const string &groupName,
                                          const string &idc,
                                          bool autoSwitch,
                                          string &masterImage,
                                          string &slaveImage)
{
    string sSql = "where module_name='" + moduleName + "' and group_name='" + groupName + "'";
    try
    {
        TC_ThreadLock::Lock lock(_dbLock);
        TC_ThreadLock::Lock lock1(_lock);
        string mirrorMaster, mirrorSlave, masterServer;
        map<string, PackTable>::iterator it = _mapPackTables->find(moduleName);
        if (it != _mapPackTables->end())
        {
            map<string, GroupInfo>::iterator itr = it->second.groupList.find(groupName);
            if (itr != it->second.groupList.end())
            {
                if (itr->second.accessStatus != 0)
                {
                    DAY_ERROR << "DbHandle::switchMirrorInDbAndMemByIdc groupName accessStatus!=0|"
                              << moduleName << "|" << groupName << "|" << idc << endl;
                    TARS_NOTIFY_ERROR(
                        string("DbHandle::switchMirrorInDbAndMemByIdc groupName accessStatus!=0|") +
                        ": " + moduleName + "|" + groupName + "|" + idc);
                    return -1;
                }
                map<string, vector<string>>::iterator idcItr = itr->second.idcList.find(idc);
                if (idcItr != itr->second.idcList.end())
                {
                    if (idcItr->second.size() == 2)
                    {
                        masterServer = itr->second.masterServer;
                        mirrorMaster = idcItr->second[0];
                        mirrorSlave = idcItr->second[1];
                        if (mirrorMaster == masterServer || mirrorSlave == masterServer)
                        {
                            DAY_ERROR << "DbHandle::switchMirrorInDbAndMemByIdc groupName has "
                                         "masterServer|"
                                      << moduleName << "|" << groupName << "|" << idc << endl;
                            TARS_NOTIFY_ERROR(string("DbHandle::switchMirrorInDbAndMemByIdc "
                                                     "groupName has masterServer|") +
                                              ": " + moduleName + "|" + groupName + "|" + idc);
                            return -1;
                        }
                        //手动切换不再设置这个状态
                        ////所有校验成功 开始修改数据库
                        ////修改组访问状态为2，镜像切换状态，修改完成后这个组状态后，这个组不能再进行切换
                        TC_Mysql::RECORD_DATA updateData;
                        int affect;
                        //修改镜像主机为镜像备机
                        updateData["server_status"] = make_pair(TC_Mysql::DB_STR, "B");
                        updateData["source_server_name"] = make_pair(TC_Mysql::DB_STR, mirrorSlave);
                        updateData["priority"] = make_pair(TC_Mysql::DB_INT, "2");
                        affect =
                            _mysql->updateRecord("t_router_group",
                                                 updateData,
                                                 sSql + " and server_name='" + mirrorMaster + "'");
                        if (affect != 1)
                        {
                            DAY_ERROR << "DbHandle::switchMirrorInDbAndMemByIdc " << affect << endl;
                            TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMemByIdc|mysql "
                                                    "updateRecord return: ") +
                                             I2S(affect));
                            if (_exRep)
                            {
                                _exRep->report(1);
                            }
                            return -1;
                        }
                        //修改镜像备机为镜像主机
                        updateData["server_status"] = make_pair(TC_Mysql::DB_STR, "I");
                        updateData["source_server_name"] =
                            make_pair(TC_Mysql::DB_STR, masterServer);
                        updateData["priority"] = make_pair(TC_Mysql::DB_INT, "1");
                        affect =
                            _mysql->updateRecord("t_router_group",
                                                 updateData,
                                                 sSql + " and server_name='" + mirrorSlave + "'");
                        if (affect != 1)
                        {
                            DAY_ERROR << "DbHandle::switchMirrorInDbAndMemByIdc " << affect << endl;
                            TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMemByIdc|mysql "
                                                    "updateRecord return: ") +
                                             I2S(affect));
                            if (_exRep)
                            {
                                _exRep->report(1);
                            }
                            return -1;
                        }

                        //修改服务可读状态
                        sSql = "where server_name = '" + mirrorMaster + "'";
                        updateData.clear();
                        updateData["status"] = make_pair(TC_Mysql::DB_INT, "-1");
                        affect = _mysql->updateRecord("t_router_server", updateData, sSql);

                        map<string, ServerInfo>::iterator its =
                            it->second.serverList.find(mirrorMaster);
                        if (its != it->second.serverList.end())
                        {
                            its->second.status = -1;
                        }
                        itr->second.accessStatus = 0;
                        idcItr->second[0] = mirrorSlave;
                        idcItr->second[1] = mirrorMaster;
                        itr->second.bakList[mirrorSlave] = itr->second.masterServer;
                        itr->second.bakList[mirrorMaster] = mirrorSlave;
                        it->second.info.version++;
                        masterImage = mirrorSlave;
                        slaveImage = mirrorMaster;
                    }
                    else
                    {
                        DAY_ERROR << "DbHandle::switchMirrorInDbAndMemByIdc groupName idc servers "
                                     "size!=2|"
                                  << moduleName << "|" << groupName << "|" << idc << endl;
                        TARS_NOTIFY_ERROR(string("DbHandle::switchMirrorInDbAndMemByIdc groupName "
                                                 "idc servers size!=2|") +
                                          ": " + moduleName + "|" + groupName + "|" + idc);
                        return -1;
                    }
                }
                else
                {
                    DAY_ERROR << "DbHandle::switchMirrorInDbAndMemByIdc groupName idc nofind "
                              << moduleName << "|" << groupName << "|" << idc << endl;
                    TARS_NOTIFY_ERROR(
                        string("DbHandle::switchMirrorInDbAndMemByIdc groupName nofind|") + ": " +
                        moduleName + "|" + groupName + "|" + idc);
                    return -1;
                }
            }
            else
            {
                DAY_ERROR << "DbHandle::switchMirrorInDbAndMemByIdc groupName nofind" << moduleName
                          << "|" << groupName << endl;
                TARS_NOTIFY_ERROR(
                    string("DbHandle::switchMirrorInDbAndMemByIdc groupName nofind|") + ": " +
                    moduleName + "|" + groupName);
                return -1;
            }
        }
        else
        {
            DAY_ERROR << "DbHandle::switchMirrorInDbAndMemByIdc moduleName nofind" << moduleName
                      << "|" << groupName << endl;
            TARS_NOTIFY_ERROR(string("DbHandle::switchMirrorInDbAndMemByIdc moduleName nofind|") +
                              ": " + moduleName + "|" + groupName);
            return -1;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        DAY_ERROR << "DbHandle::switchMirrorInDbAndMemByIdc " << moduleName << "|" << groupName
                  << " exception: " << ex.what() << endl;
        TARS_NOTIFY_WARN(string("DbHandle::switchMirrorInDbAndMemByIdc|") + ex.what() + ": " +
                         moduleName + "|" + groupName);
        if (_exRep)
        {
            _exRep->report(1);
        }
        return -1;
    }
    return 0;
}

/*切换完成后更新服务主备最新状态到三者关系数据库*/
void DbHandle::updateStatusToRelationDB(const string &serverName, const string &status)
{
    vector<string> tmpVec = TC_Common::sepstr<string>(serverName, ".");
    int sizeVec = int(tmpVec.size());
    string lastServerName;
    if (sizeVec < 2)
    {
        lastServerName = serverName;
    }
    else
    {
        for (int i = 1; i < sizeVec - 1; ++i)
        {
            lastServerName += tmpVec[i] + ".";
        }
        lastServerName += tmpVec[sizeVec - 1];
    }

    try
    {
        TC_ThreadLock::Lock lock(_relationLock);
        string sSql = "update t_cache_router set server_status='" + status +
                      "' where cache_name='" + lastServerName + "'";
        _mysqlDBRelation->execute(sSql);
    }
    catch (TC_Mysql_Exception &ex)
    {
        FDLOG("switch") << "DbHandle::updateStatusToRelationDB exception: " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("DbHandle::updateStatusToRelationDB|") + ex.what());
    }
}

/*开始切换前将模块，服务组，状态等信息写入数据库*/
int DbHandle::insertSwitchInfo(const string &moduleName,
                               const string &groupName,
                               const string &masterServer,
                               const string &slaveServer,
                               const string &mirrorIdc,
                               const string &mirrorServer,
                               const int switchType,
                               long &id)
{
    try
    {
        TC_ThreadLock::Lock lock(_relationLock);

        string sSql =
            "insert into `t_router_switch` (`app_name`,`module_name`,`group_name`,"
            "`master_server`,`slave_server`,`mirror_idc`,`master_mirror`,`switch_type`,`"
            "switch_time`) values("
            "(select app_name from t_router_app where router_name='DCache." +
            ServerConfig::ServerName + "'),'" + moduleName + "','" + groupName + "','" +
            masterServer + "','" + slaveServer + "','" + mirrorIdc + "','" + mirrorServer + "'," +
            I2S(switchType) + ",now())";

        _mysqlDBRelation->execute(sSql);

        int affect = _mysqlDBRelation->getAffectedRows();

        if (affect != 1)
        {
            TLOGERROR(FILE_FUN << "run sql affect:" << affect << "|sql:" << sSql << endl);
            return -1;
        }

        id = _mysqlDBRelation->lastInsertID();
        TLOGDEBUG(FILE_FUN << "run sql affect:" << affect << "|lastInsertID:" << id
                           << "|sql:" << sSql << endl);
        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        FDLOG("switch") << "DbHandle::insertSwitchInfo exception: " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("DbHandle::insertSwitchInfo|") + ex.what());
    }
    return -1;
}

int DbHandle::insertSwitchInfo(const string &moduleName,
                               const string &groupName,
                               const string &masterServer,
                               const string &slaveServer,
                               const string &mirrorIdc,
                               const string &masterMirror,
                               const string &slaveMirror,
                               const string &switchProperty,
                               const string &comment,
                               const int switchType,
                               const int switchResult,
                               const int groupSatus,
                               const time_t switchBeginTime)
{
    try
    {
        TC_ThreadLock::Lock lock(_relationLock);

        string sSql =
            "insert into `t_router_switch` (`app_name`,`module_name`,`group_name`,"
            "`master_server`,`slave_server`,`mirror_idc`,`master_mirror`,`slave_mirror`,"
            "`switch_type`,`switch_result`,`access_status`,`comment`,`switch_property`,`"
            "switch_time`,`modify_time`) values("
            "(select app_name from t_router_app where router_name='DCache." +
            ServerConfig::ServerName + "'),'" + moduleName + "','" + groupName + "','" +
            masterServer + "','" + slaveServer + "','" + mirrorIdc + "','" + masterMirror + "','" +
            slaveMirror + "'," + I2S(switchType) + "," + I2S(switchResult) + "," + I2S(groupSatus) +
            ",'" + comment + "','" + switchProperty + "',FROM_UNIXTIME(" +
            TC_Common::tostr(switchBeginTime) + "), now())";

        _mysqlDBRelation->execute(sSql);

        int affect = _mysqlDBRelation->getAffectedRows();

        if (affect != 1)
        {
            TLOGERROR(FILE_FUN << "run sql affect:" << affect << "|sql:" << sSql << endl);
            return -1;
        }

        long id = _mysqlDBRelation->lastInsertID();
        TLOGDEBUG(FILE_FUN << "run sql affect:" << affect << "|lastInsertID:" << id
                           << "|sql:" << sSql << endl);
        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        FDLOG("switch") << "DbHandle::insertSwitchInfo exception: " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("DbHandle::insertSwitchInfo|") + ex.what());
    }
    return -1;
}

void DbHandle::updateSwitchInfo(long iID,
                                int iResult,
                                const string &sComment,
                                int iGroupStatus,
                                const string &sCurMasterServer,
                                const string &sCurSlaveServer)
{
    try
    {
        TC_ThreadLock::Lock lock(_relationLock);

        string sSql =
            "update `t_router_switch` set `switch_result`=" + tars::TC_Common::tostr(iResult) +
            ", `comment`='" + sComment +
            "', `access_status`=" + tars::TC_Common::tostr(iGroupStatus);
        if (sCurMasterServer != "") sSql += (", `master_server`='" + sCurMasterServer + "'");
        if (sCurSlaveServer != "") sSql += (", `slave_server`='" + sCurSlaveServer + "'");

        sSql += (" where `id`=" + tars::TC_Common::tostr(iID));

        _mysqlDBRelation->execute(sSql);
        int affect = _mysqlDBRelation->getAffectedRows();
        TLOGDEBUG(FILE_FUN << "run sql affect:" << affect << "|sql:" << sSql << endl);
        if (affect != 1)
        {
            DAY_ERROR << __FUNCTION__ << " _mysql->updateSwitchInfo return: " << affect << endl;
            return;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        FDLOG("switch") << "DbHandle::updateSwitchInfo exception: " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("DbHandle::updateSwitchInfo|") + ex.what());
    }
}

void DbHandle::updateSwitchMirrorInfo(long iID,
                                      int iResult,
                                      const string &sComment,
                                      int iGroupStatus,
                                      const string &sCurMasterImage,
                                      const string &sCurSlaveImage)
{
    try
    {
        TC_ThreadLock::Lock lock(_relationLock);

        string sSql =
            "update `t_router_switch` set `switch_result`=" + tars::TC_Common::tostr(iResult) +
            ", `comment`='" + sComment +
            "', `access_status`=" + tars::TC_Common::tostr(iGroupStatus);
        if (sCurMasterImage != "") sSql += (", `master_mirror`='" + sCurMasterImage + "'");
        if (sCurSlaveImage != "") sSql += (", `slave_mirror`='" + sCurSlaveImage + "'");

        sSql += (" where `id`=" + tars::TC_Common::tostr(iID));

        _mysqlDBRelation->execute(sSql);
        int affect = _mysqlDBRelation->getAffectedRows();
        TLOGDEBUG(FILE_FUN << "run sql affect:" << affect << "|sql:" << sSql << endl);
        if (affect != 1)
        {
            DAY_ERROR << __FUNCTION__ << " _mysql->updateSwitchInfo return: " << affect << endl;
            return;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        FDLOG("switch") << "DbHandle::updateSwitchInfo exception: " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("DbHandle::updateSwitchInfo|") + ex.what());
    }
}

void DbHandle::updateSwitchGroupStatus(const string &moduleName,
                                       const string &groupName,
                                       const string &sIdc,
                                       const string &comment,
                                       int iGroupStatus)
{
    try
    {
        TC_ThreadLock::Lock lock(_relationLock);

        string sSql =
            "update `t_router_switch` set `access_status`=" + tars::TC_Common::tostr(iGroupStatus) +
            ", `comment`='" + comment + "' where `module_name`='" + moduleName +
            "' and `group_name`='" + groupName + "' and `mirror_idc`='" + sIdc +
            "' and `access_status`=2 and `switch_result`=1";

        _mysqlDBRelation->execute(sSql);
        int affect = _mysqlDBRelation->getAffectedRows();
        TLOGDEBUG(FILE_FUN << "run sql affect:" << affect << "|sql:" << sSql << endl);
        if (affect != 1)
        {
            DAY_ERROR << __FUNCTION__ << " _mysql->updateSwitchInfo return: " << affect << endl;
            return;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        FDLOG("switch") << "DbHandle::updateSwitchInfo exception: " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("DbHandle::updateSwitchInfo|") + ex.what());
    }
}

void DbHandle::updateSwitchGroupStatus(long iID, int iGroupStatus)
{
    try
    {
        TC_ThreadLock::Lock lock(_relationLock);

        string sSql =
            "update `t_router_switch` set `access_status`=" + tars::TC_Common::tostr(iGroupStatus) +
            " where `id`=" + tars::TC_Common::tostr(iID);

        _mysqlDBRelation->execute(sSql);
        int affect = _mysqlDBRelation->getAffectedRows();
        TLOGDEBUG(FILE_FUN << "run sql affect:" << affect << "|sql:" << sSql << endl);
        if (affect != 1)
        {
            DAY_ERROR << __FUNCTION__ << " _mysql->updateSwitchInfo return: " << affect << endl;
            return;
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        FDLOG("switch") << "DbHandle::updateSwitchGroupStatus exception: " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("DbHandle::updateSwitchInfo|") + ex.what());
    }
}

int DbHandle::addMirgrateInfo(const string &strIp, const string &strReason)
{
    try
    {
        TC_ThreadLock::Lock lock(_migrateLock);

        string sSql = "select status from db_dcache_relation.t_migrate_info where ip='" + strIp +
                      "' and (unix_timestamp(now())-unix_timestamp(last_modify_time))<259200;";
        TC_Mysql::MysqlData sqlData = _mysqlMigrate->queryRecord(sSql);
        FDLOG("migrate") << __LINE__ << "|sql:[" << sSql << "]executed. size:" << sqlData.size()
                         << "\n";
        if (sqlData.size() == 0)
        {
            sSql =
                "insert into db_dcache_relation.t_migrate_info (ip,status,reason, "
                "last_modify_time) values('" +
                strIp + "', 0, '" + strReason + "', now());";
            _mysqlMigrate->execute(sSql);
            int affect = _mysqlMigrate->getAffectedRows();
            if (affect != 1)
            {
                DAY_ERROR << __FUNCTION__ << " _mysql->insertRecord return: " << affect << endl;
                TLOGERROR(FILE_FUN << "run sql affect:" << affect << "|sql:" << sSql << endl);
                return -1;
            }
            FDLOG("migrate") << __LINE__ << "|sql:[" << sSql << "]executed.\n";
        }
        else
        {
            FDLOG("migrate") << __LINE__ << "|ip:[" << strIp << "]existed inf t_migrate_info.\n";
        }
    }
    catch (TC_Mysql_Exception &ex)
    {
        FDLOG("migrate") << "DbHandle::addMirgrateInfo exception: " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("DbHandle::addMirgrateInfo|") + ex.what());
        TLOGERROR(FILE_FUN << "DbHandle::addMirgrateInfo exception: " << ex.what() << endl);
        return -1;
    }
    return 0;
}

int DbHandle::getAllServerInIp(const string &strIp, vector<string> &vServerName)
{
    try
    {
        TC_ThreadLock::Lock lock(_migrateLock);

        string sSql = "select cache_name from db_dcache_relation.t_cache_router where cache_ip='" +
                      strIp + "';";
        TC_Mysql::MysqlData sqlData = _mysqlMigrate->queryRecord(sSql);
        FDLOG("migrate") << __LINE__ << "[DbHandle::getAllServerInIp]|sql:[" << sSql
                         << "]executed. size:" << sqlData.size() << "\n";

        for (size_t ii = 0; ii < sqlData.size(); ii++)
        {
            vServerName.push_back(sqlData[ii]["cache_name"]);
        }

        if (vServerName.size() == 0)
        {
            FDLOG("migrate") << __LINE__
                             << "[DbHandle::getAllServerInIp]|error: vServerName.size() == 0\n";
            return -1;
        }

        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        FDLOG("migrate") << "DbHandle::getAllServerInIp exception: " << ex.what() << endl;
        TARS_NOTIFY_ERROR(string("DbHandle::getAllServerInIp|") + ex.what());
        TLOGERROR(FILE_FUN << "DbHandle::getAllServerInIp exception: " << ex.what() << endl);
        return -1;
    }
    return 0;
}

// 完成一组事务的处理，保证全部成功或者全部失败
bool DbHandle::doTransaction(const vector<string> &vSqlSet, string &sErrMsg)
{
    ostringstream os;

    if (0 == vSqlSet.size())
    {
        os << "[DbHandle::doTransaction] sqlSet param is empty!" << endl;
        sErrMsg = os.str();
        return false;
    }
    // Start transaction
    const string startTransaction("START TRANSACTION;");
    // Commit result
    const string commit("COMMIT;");
    // rollback result
    const string rollBack("ROLLBACK;");
    // set autocommit=false ;
    const string autoCommitFalse("set autocommit=false;");
    // set autocommit=true ;
    const string autoCommitTrue("set autocommit=true;");
    // 关闭自动提交
    try
    {
        _mysql->execute(autoCommitFalse);
    }
    catch (const exception &e)
    {
        // 关闭自动提交失败
        os << "[DbHandle::doTransaction] Do SQL: [" << autoCommitFalse << "]"
           << " Exception:" << e.what() << endl;
        sErrMsg = os.str();
        return false;
    }
    catch (...)
    {
        os << "[DbHandle::doTransaction] Do SQL: [" << autoCommitFalse << "]"
           << " unknow Exception." << endl;
        sErrMsg = os.str();
        return false;
    }

    // 启动事务
    try
    {
        _mysql->execute(startTransaction);
    }
    catch (const exception &e)
    {
        os << "[DbHandle::doTransaction] Start transaction exception : " << e.what() << endl;
        sErrMsg = os.str();
        return false;
    }
    catch (...)
    {
        os << "[DbHandle::doTransaction] Do SQL: [" << startTransaction << "]"
           << " Exception" << endl;
        sErrMsg = os.str();
        return false;
    }
    // 执行sql
    bool except = false;
    for (size_t i = 0; i < vSqlSet.size(); ++i)
    {
        try
        {
            _mysql->execute(vSqlSet[i]);
        }
        catch (const exception &e)
        {
            os << "[DbHandle::doTransaction] Do SQL: [" << vSqlSet[i] << "]"
               << " Exception:" << e.what() << endl;
            sErrMsg = os.str();
            except = true;
            break;
        }
        catch (...)
        {
            os << "[DbHandle::doTransaction] Do SQL: [" << vSqlSet[i] << "]"
               << " unknow Exception." << endl;
            sErrMsg = os.str();
            except = true;
            break;
        }
    }

    os.str("");
    // 如果存在异常，进行回滚操作
    if (true == except)
    {
        try
        {
            _mysql->execute(rollBack);
        }
        catch (const exception &e)
        {
            // rollback 失败，断开链接保证事务不生效！
            os << "[DbHandle::doTransaction] rollBack exception : " << e.what() << endl;
            sErrMsg = os.str();
            _mysql->disconnect();
            // connect();
            return false;
        }
        catch (...)
        {
            os << "[DbHandle::doTransaction] rollBack unknow exception." << endl;
            sErrMsg = os.str();
            _mysql->disconnect();
            // connect
            return false;
        }
        // 如果不存在异常，则直接commit
    }
    else
    {
        try
        {
            _mysql->execute(commit);
        }
        catch (const exception &e)
        {
            // commit 失败，断开链接保证事务不生效！
            os << "[DbHandle::doTransaction] commit exception : " << e.what() << endl;
            sErrMsg = os.str();
            _mysql->disconnect();
            // connect();
            return false;
        }
        catch (...)
        {
            os << "[DbHandle::doTransaction] commit unknow exception." << endl;
            sErrMsg = os.str();
            _mysql->disconnect();
            // connect
            return false;
        }
    }

    // commit成功了，打开自动提交
    try
    {
        _mysql->execute(autoCommitTrue);
    }
    catch (const exception &e)
    {
        // 打开自动提交失败
        os << "[DbHandle::doTransaction] Do SQL: [" << autoCommitTrue << "]"
           << " Exception:" << e.what() << " but not serious!" << endl;
        sErrMsg = os.str();

        // 这里不能 return false。由于结果已经commit，即实际上更新操作已经完成了。
        // 如果这里出现异常，暂时无法处理，当前链接会保持在 autocommit = false状态。
        // 断开链接，重新连接即可
        _mysql->disconnect();
        return true;
    }
    catch (...)
    {
        os << "[DbHandle::doTransaction] Do SQL: [" << autoCommitTrue << "]"
           << " unknow Exception , but not serious!" << endl;
        sErrMsg = os.str();
        _mysql->disconnect();
        return true;
    }

    return true;
}

int DbHandle::addMemTransferMutexCond(const TransferInfo &transferInfo)
{
    map<string, map<string, TransferMutexCondPtr>>::iterator _it =
        _mapTransferMutexCond.find(transferInfo.moduleName);
    if (_it != _mapTransferMutexCond.end())
    {
        map<string, TransferMutexCondPtr> &_m = _it->second;

        TransferMutexCondPtr pMutexCond = new TransferMutexCond();

        _m[TC_Common::tostr<pthread_t>(pthread_self())] = pMutexCond;
    }
    else
    {
        TransferMutexCondPtr pMutexCond = new TransferMutexCond();
        _mapTransferMutexCond[transferInfo.moduleName]
                             [TC_Common::tostr<pthread_t>(pthread_self())] = pMutexCond;
    }
    return 0;
}

int DbHandle::removeMemTransferMutexCond(const TransferInfo &transferInfo)
{
    TC_ThreadLock::Lock lock(_transLock);
    map<string, map<string, TransferMutexCondPtr>>::iterator _it =
        _mapTransferMutexCond.find(transferInfo.moduleName);
    if (_it != _mapTransferMutexCond.end())
    {
        map<string, TransferMutexCondPtr> &_m = _it->second;
        map<string, TransferMutexCondPtr>::iterator _itPtr =
            _m.find(TC_Common::tostr<pthread_t>(pthread_self()));
        if (_itPtr != _m.end())
        {
            _m.erase(_itPtr);
            if (_m.empty())
            {
                _mapTransferMutexCond.erase(_it);
            }

            FDLOG("TransferSwitch") << __FUNCTION__ << "|" << __LINE__ << "|"
                                    << "tranid:" << transferInfo.id << "|"
                                    << "pthread_id: " << pthread_self() << "|"
                                    << " removeMemTransferMutexCond success" << endl;
            return 0;
        }
        FDLOG("TransferSwitch") << __FUNCTION__ << "|" << __LINE__ << "|"
                                << "tranid:" << transferInfo.id << "|"
                                << "pthread_id: " << pthread_self() << "|"
                                << " removeMemTransferMutexCond fail1" << endl;
    }
    else
    {
        FDLOG("TransferSwitch") << __FUNCTION__ << "|" << __LINE__ << "|"
                                << "tranid:" << transferInfo.id << "|"
                                << "pthread_id: " << pthread_self() << "|"
                                << " removeMemTransferMutexCond fail2" << endl;
    }

    return -1;
}

int DbHandle::getTransferMutexCond(const string &moduleName,
                                   const string &threadID,
                                   pthread_mutex_t **pMutex,
                                   pthread_cond_t **pCond)
{
    TC_ThreadLock::Lock lock(_transLock);
    map<string, map<string, TransferMutexCondPtr>>::iterator _it =
        _mapTransferMutexCond.find(moduleName);
    if (_it != _mapTransferMutexCond.end())
    {
        map<string, TransferMutexCondPtr> &_m = _it->second;
        map<string, TransferMutexCondPtr>::iterator _itPtr = _m.find(threadID);
        if (_itPtr != _m.end())
        {
            *pMutex = &(_itPtr->second->_mutex);
            *pCond = &(_itPtr->second->_cond);
            return 0;
        }
    }

    return -1;
}

int DbHandle::getTransferMutexCond(map<string, map<string, TransferMutexCondPtr>> &mMutexCond)
{
    TC_ThreadLock::Lock lock(_transLock);
    mMutexCond = _mapTransferMutexCond;
    return 0;
}

int DbHandle::checkServerOffline(const string &serverName, bool &bOffline)
{
    try
    {
        TC_ThreadLock::Lock lock(_dbLock);

        string sSql = "select * from t_router_server where server_name = '" + serverName + "'";

        TC_Mysql::MysqlData serverData = _mysql->queryRecord(sSql);
        if (serverData.size() <= 0)
        {
            bOffline = true;
        }
        else
        {
            bOffline = false;
        }

        TLOGDEBUG(FILE_FUN << "DbHandle::checkServerOffline serverName:" << serverName
                           << "|bOffline: " << bOffline << "|sql:" << sSql << endl);

        return 0;
    }
    catch (TC_Mysql_Exception &ex)
    {
        TLOGERROR(FILE_FUN << "DbHandle::checkServerOffline serverName:" << serverName
                           << "|exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR(FILE_FUN << "DbHandle::checkServerOffline serverName:" << serverName
                           << "|unknown exception." << endl);
    }

    return -1;
}

void DbHandle::downgrade()
{
    // _mapPackTables不需要清理，在备机升级为主机时会通过reloadRoute重新加载
    clearTransferInfos();
}

