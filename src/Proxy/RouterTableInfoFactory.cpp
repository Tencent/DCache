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
#include "Global.h"
#include <algorithm>

#include "RouterTableInfoFactory.h"
#include "ProxyShare.h"

int RouterTableInfo::_maxUpdateFrequency = 0;

string RouterTableInfo::showStatus()
{
    string sResult = "";
    sResult = "---------------------RouterTableInfo----------------------\n";
    sResult += "the RouterTableMaxUpdateFrequency: " + TC_Common::tostr<int>(_maxUpdateFrequency) + "\n";
    return sResult;
}

RouterTableInfo::RouterTableInfo(const string &moduleName, const string &localRouterFile, shared_ptr<RouterHandle> pRouterHandle)
    : _pRouterHandle(pRouterHandle)
{
    _updateTimes = 1;
    _lastUpdateBeginTime = TC_TimeProvider::getInstance()->getNow();
    _moduleName = moduleName;
    _localRouterFile = localRouterFile;
}

int RouterTableInfo::update()
{
    TC_ThreadLock::Lock lock(_threadLock);

    if (TC_TimeProvider::getInstance()->getNow() - _lastUpdateBeginTime >= 1)
    {
        _lastUpdateBeginTime = TC_TimeProvider::getInstance()->getNow();
        _updateTimes = 1;
    }
    else if (_updateTimes < _maxUpdateFrequency)
    {
        _updateTimes++;
    }
    else
    {
        TLOG_ERROR("in RouterTableInfo::update(), update too frequently!" << endl);
        return ET_KEY_AREA_ERR;
    }
    return updateRouterTable();
}

int RouterTableInfo::initRouterTable()
{
    string strErr;
    try
    {
        PackTable packTable;
        int iRet = _pRouterHandle->getRouterInfo(_moduleName, packTable, true);
        if (iRet == 0)
        {
            //从RouterServer拉取路由成功后，初始化本地路由表
            iRet = _routerTable.init(packTable, "");
            if (iRet == RouterTable::RET_SUCC)
            {
                // 加载成功后写到本地文件
                _routerTable.toFile(_localRouterFile);
                TLOG_DEBUG("load router table for module: " << _moduleName << " from RouterServer succ" << endl);
                return 0;
            }
            else
            {
                TLOG_ERROR("[RouterTableInfo::initRouterTable] load router table for module: " << _moduleName << " from RouterServer succ, but initialize failed, ret="
                                                                                              << iRet << "\ntry from local file: " << _localRouterFile << endl);
            }
        }
        else
        {
            TLOG_ERROR("[RouterTableInfo::initRouterTable] load router table for module: " << _moduleName << " from RouterServer failed \ntry from local file: " << _localRouterFile << endl);
        }

        iRet = _routerTable.fromFile(_localRouterFile, "");
        if (iRet == RouterTable::RET_SUCC)
        {
            TLOG_DEBUG("load router table for module: " << _moduleName << " from local file succ" << endl);
            return 0;
        }
        else
        {
            TLOG_ERROR("[RouterTableInfo::initRouterTable] load router table for module: " << _moduleName << " from local file: " << _localRouterFile << " failed, ret=" << iRet << endl);
        }
    }
    catch (exception &e)
    {
        strErr = string("[RouterTableInfo::initRouterTable] exception:") + e.what();
    }
    catch (...)
    {
        strErr = "[RouterTableInfo::initRouterTable] UnkownException";
    }

    TARS_NOTIFY_WARN(strErr);
    TLOG_ERROR(strErr << endl);
    return -1;
}

int RouterTableInfo::updateRouterTable()
{
    //将本地路由表版本号和RouterServer中该模块的路由表版本号相比较，如果不同则重新拉取路由表
    int remoteRouteTableVersion;
    int iRet = _pRouterHandle->getRouterVersion(_moduleName, remoteRouteTableVersion);
    if (iRet != 0)
    {
        TLOG_ERROR("[RouterTableInfo::updateRouterTable] get route version for module: " << _moduleName << " failed" << endl);
        return ET_SYS_ERR;
    }

    return updateRouterTableByVersion(remoteRouteTableVersion);
}

int RouterTableInfo::updateRouterTableByVersion(int iVersion)
{
    string strErr;
    try
    {
        //将本地路由表版本号和RouterServer中该模块的路由表版本号相比较，如果不同则重新拉取路由表
        int iCurVersion = _routerTable.getVersion();

        if (iCurVersion == iVersion)
        {
            return ET_KEY_AREA_ERR;
        }
        else
        {
            PackTable packTable;
            int iRet = _pRouterHandle->getRouterInfo(_moduleName, packTable);
            if (iRet != 0)
            {
                TLOG_ERROR("[RouterTableInfo::updateRouterTable] get router info for module: " << _moduleName << " failed" << endl);
                return ET_SYS_ERR;
            }
            else
            {
                iRet = _routerTable.reload(packTable);
                if (iRet != 0)
                {
                    TLOG_ERROR("[RouterTableInfo::updateRouterTable] reload router table for module: " << _moduleName << " failed" << endl);
                    return ET_SYS_ERR;
                }
                else
                {
                    _routerTable.toFile(_localRouterFile); // 将路由表写到本地文件
                    // 上报自动切换状态
                    map<string, GroupInfo>::iterator itr = packTable.groupList.begin();
                    map<string, GroupInfo>::iterator itrEnd = packTable.groupList.end();
                    while (itr != itrEnd)
                    {
                        if (itr->second.accessStatus == 1)
                        {
                            int iRet = RouterHandle::getInstance()->reportSwitchGroup(_moduleName, itr->second.groupName, ServerConfig::ServerName);
                            if (iRet != 0)
                            {
                                string errMsg = "proxy RouterHandle::getInstance()->reportSwitchGroup error _moduleName:" + _moduleName + " groupName:" + itr->second.groupName;
                                TLOG_ERROR(errMsg << endl);
                                TARS_NOTIFY_ERROR(errMsg);
                            }
                        }
                        itr++;
                    }
                    return ET_SUCC;
                }
            }
        }
    }
    catch (exception &e)
    {
        strErr = string("[RouterTableInfo::updateRouterTable] exception:") + e.what();
    }
    catch (...)
    {
        strErr = "[RouterTableInfo::updateRouterTable] UnkownException";
    }

    TARS_NOTIFY_WARN(strErr);
    TLOG_ERROR(strErr << endl);
    return ET_SYS_ERR;
}

RouterTableInfoFactory::RouterTableInfoFactory(RouterHandle *pRouterHanle) : _pRouterHandle(pRouterHanle)
{
    _flag = 0;
    map<string, RouterTableInfo *> mapRouterTableInfo1;
    mapRouterTableInfo1.clear();
    _routerTableInfo.push_back(mapRouterTableInfo1);
    map<string, RouterTableInfo *> mapRouterTableInfo2;
    mapRouterTableInfo2.clear();
    _routerTableInfo.push_back(mapRouterTableInfo2);
}

int RouterTableInfoFactory::init(TC_Config &conf)
{
    // 设置路由表更新最大频率
    RouterTableInfo::setMaxUpdateFrequency(TC_Common::strto<int>(conf["/Main<RouterTableMaxUpdateFrequency>"]));

    /*以下根据配置文件提供的模块列表，为每个模块创建路由表信息*/
    _baseLocalRouterFile = ServerConfig::DataPath + conf["/Main<BaseLocalRouterFile>"];

    string sFailedModuleNameList = ""; // 路由表信息创建失败的模块列表
    string sSuccModuleNameList = "";   // 路由表信息创建成功的模块列表

    vector<string> vModuleNameListTmp;
    int iRet = _pRouterHandle->getModuleList(vModuleNameListTmp);
    if (iRet != 0)
    {
        TLOG_DEBUG("RouterTableInfoFactory::init RouterHandle::getInstance()->getModuleList error" << endl);
    }
    vector<string>::const_iterator it = vModuleNameListTmp.begin();
    for (; it != vModuleNameListTmp.end(); ++it)
    {
        // 如果配置文件的模块列表中有重复的模块，且之前该模块的路由表信息已经成功创建，则跳过
        if (_routerTableInfo[0].find(*it) != _routerTableInfo[0].end())
        {
            continue;
        }

        //为模块创建路由表信息
        string sLocalRouterFile = _baseLocalRouterFile + "." + *it;
        RouterTableInfo *pRouterTableInfo = new RouterTableInfo(*it, sLocalRouterFile, _pRouterHandle);

        int iRet = pRouterTableInfo->initRouterTable();
        if (iRet != 0)
        {
            TLOG_ERROR("[RouterTableInfoFactory::init] init router table for " << *it << " failed" << endl);
            sFailedModuleNameList = sFailedModuleNameList + "|" + *it;
            delete pRouterTableInfo;
        }
        else
        {
            TLOG_DEBUG("[RouterTableInfoFactory::init] init router table for " << *it << " succ" << endl);
            _routerTableInfo[0][*it] = pRouterTableInfo;
            sSuccModuleNameList = sSuccModuleNameList + *it + ";";
        }
    }

    //之前间隔太长,导致业务连续在一个应用下安装几个模块时,导致只有第一个能reload成功，其他都不满足reload时间
    _minUpdateFactoryInterval = 2;
    _lastUpdateFactoryTime = TC_TimeProvider::getInstance()->getNow();

    if (sSuccModuleNameList.empty())
    {
        TLOG_ERROR("[RouterTableInfoFactory::init] no router table can be used" << endl);
        return ET_SYS_ERR;
    }
    if (!sFailedModuleNameList.empty())
    {
        TARS_NOTIFY_ERROR("[RouterTableInfoFactory::init] module:" + sFailedModuleNameList + " load router table failed");
    }
    sSuccModuleNameList = TC_Common::trimright(sSuccModuleNameList, ";", false);
    TLOG_DEBUG("init RouterTableInfoFactory succ, this Proxy provide service for: " << sSuccModuleNameList << endl);
    return ET_SUCC;
}

string RouterTableInfoFactory::reloadConf(TC_Config &conf)
{
    TLOG_DEBUG("RouterTableInfoFactory reload config ..." << endl);

    // 重置路由表更新最大频率
    RouterTableInfo::setMaxUpdateFrequency(TC_Common::strto<int>(conf["/Main<RouterTableMaxUpdateFrequency>"]));

    return "";
}

string RouterTableInfoFactory::showStatus()
{
    string sResult = "";
    string sModuleNameList = "";
    int i = 0;
    map<string, RouterTableInfo *>::const_iterator it = _routerTableInfo[_flag].begin();
    for (; it != _routerTableInfo[_flag].end(); ++it)
    {
        sModuleNameList = sModuleNameList + it->first;
        sModuleNameList += (++i % 3 == 0) ? "\n" : ";";
    }
    sResult = "-----------------RouterTableInfoFactory-----------------\n";
    sResult += "ModuleNameList that ProxyServer supported: " + ((i > 0 && i % 3 == 0) ? sModuleNameList : (TC_Common::trimright(sModuleNameList, ";", false) + "\n"));
    sResult += "the MinUpdateModuleNameListInterval: " + TC_Common::tostr<time_t>(_minUpdateFactoryInterval) + "\n";
    return sResult;
}

bool RouterTableInfoFactory::supportModule(const string &moduleName)
{
    int indexInUse = _flag;
    if (_routerTableInfo[indexInUse].count(moduleName))
        return true;
    return false;
}

RouterTableInfo *RouterTableInfoFactory::getRouterTableInfo(const string &moduleName)
{
    int indexInUse = _flag ? 1 : 0;
    if (_routerTableInfo[indexInUse].find(moduleName) == _routerTableInfo[indexInUse].end())
        return NULL;
    return _routerTableInfo[indexInUse][moduleName];
}


void RouterTableInfoFactory::synRouterTableInfo()
{
    TC_ThreadLock::Lock lock(_threadLock);
    int64_t beginTime = TC_TimeProvider::getInstance()->getNowMs();

    // 查看之前是否初始化成功
    if (_routerTableInfo[_flag].begin() == _routerTableInfo[_flag].end())
    {
        TC_Config conf;
        conf.parseFile(ServerConfig::BasePath + "ProxyServer.conf");

        int ret = init(conf);
        if (ret)
        {
            TLOG_DEBUG("synRouterTableInfo get router failed, no RouterTable can be used" << endl);
        }
        return;
    }

    vector<string> moduleNameList;
    map<string, RouterTableInfo *>::const_iterator it = _routerTableInfo[_flag].begin();
    for (; it != _routerTableInfo[_flag].end(); ++it)
    {
        moduleNameList.push_back(it->first);
    }

    map<string, int> mapModuleVersion;

    int iRet = _pRouterHandle->getRouterVersionBatch(moduleNameList, mapModuleVersion);
    if (iRet != 0)
    {
        TLOG_ERROR("RouterHandle::getInstance()->getRouterVersionBatch error return==" << iRet << endl);
        return;
    }

    // 成功更新路由表数据的模块名集合，形如："modulaName1|moduleName2";
    string synSucc = "";
    // 无需更新路由表数据的模块名集合(即本地路由表数据的版本号和路由服务器的最新路由数据版本号一致，因此无需更新)
    string synNeedless = "";
    // 路由表数据更新失败的模块名的集合
    vector<string> modulesFail2SyncRouterTable;

    
    for (it = _routerTableInfo[_flag].begin(); it != _routerTableInfo[_flag].end(); ++it)
    {
        map<string, int>::iterator itr = mapModuleVersion.find(it->first);
        if (itr != mapModuleVersion.end())
        {
            if (itr->second != -1)
            {
                int iRet = it->second->updateRouterTableByVersion(itr->second);
                if (iRet == ET_SUCC)
                    synSucc = synSucc + "|" + it->first;
                else if (iRet == ET_KEY_AREA_ERR)
                    synNeedless = synNeedless + "|" + it->first;
                else
                    modulesFail2SyncRouterTable.push_back(it->first);
            }
            else
            {
                modulesFail2SyncRouterTable.push_back(it->first);
            }
        }
    }
    
    string synFailed = TC_Common::tostr(modulesFail2SyncRouterTable);
    if (synSucc.size() > 0)
    {
        TLOG_DEBUG("synchronize succ: " << synSucc << endl);
    }
    if (synFailed.size() > 0)
    {
        TLOG_DEBUG("synchronize failed: " << synFailed << endl);
    }
    if (synNeedless.size() > 0)
    {
        TLOG_DEBUG("synchronize needless: " << synNeedless << endl);
    }

    int64_t endTime = TC_TimeProvider::getInstance()->getNowMs();
    TLOG_DEBUG("RouterTableInfoFactory::synRouterTableInfo timecount：" << endTime - beginTime << endl);
}


void RouterTableInfoFactory::checkModuleChange()
{   
    // 从Router拉取最新的模块信息
    vector<string> latestModuleList;
    int ret = _pRouterHandle->getModuleList(latestModuleList);
    if (ret != 0)
    {
        string errMsg = "[RouterTableInfoFactory::checkModuleChange] RouterHandle::getInstance()->getModuleList error!";
        TLOG_ERROR(errMsg << endl);
        TARS_NOTIFY_ERROR(errMsg);
        return;
    }

    TLOG_DEBUG("[RouterTableInfoFactory::checkModuleChange], got " << latestModuleList.size() 
                << " modules from Router, they are: " << TC_Common::tostr(latestModuleList) << endl);

    // 判断是否有新增的模块或者下线的模块
    if (isNeedUpdateFactory(latestModuleList))
    {

        int iNew = 1, iCur = 0;
        if (_flag)
        {
            iNew = 0;
            iCur = 1;
        }

        // 释放上次下线的模块的路由对象所占内存
        map<string, RouterTableInfo *>::const_iterator mIt = _routerTableInfo[iNew].begin();
        for (; mIt != _routerTableInfo[iNew].end(); mIt++)
        {
            if (_routerTableInfo[iCur].find(mIt->first) != _routerTableInfo[iCur].end())
            {
                continue;
            }
            else
            {
                RouterTableInfo *pRouterTableInfo = mIt->second;
                if (pRouterTableInfo != NULL)
                {
                    delete pRouterTableInfo;
                }
            }
        }
        
        _routerTableInfo[iNew].clear();

        vector<string>::const_iterator vIt = latestModuleList.begin();
        for (; vIt != latestModuleList.end(); ++vIt)
        {
            // 如果在更新之前ProxyServer已经支持这个模块，那么使用这个模块已有的路由表信息
            if (_routerTableInfo[iCur].find(*vIt) != _routerTableInfo[iCur].end())
            {
                _routerTableInfo[iNew][*vIt] = _routerTableInfo[iCur][*vIt];
            }
            // 创建新增模块的路由表信息
            else
            {
                string sLocalRouterFile = _baseLocalRouterFile + "." + *vIt;
                RouterTableInfo *pRouterTableInfo = new RouterTableInfo(*vIt, sLocalRouterFile, _pRouterHandle);
                int iRet = pRouterTableInfo->initRouterTable();

                if (iRet != 0)
                {
                    TLOG_ERROR("[RouterTableInfoFactory::checkModuleChange] init router table for module(" << *vIt << ") failed" << endl);
                    delete pRouterTableInfo;
                }
                else
                {
                    TLOG_DEBUG("[RouterTableInfoFactory::checkModuleChange] init router table for module( " << *vIt << ") succ" << endl);
                    _routerTableInfo[iNew][*vIt] = pRouterTableInfo;
                }
            }
        }

        // 路由表信息map切换
        TC_ThreadLock::Lock lock(_threadLock);
        _flag = iNew;
    }
}


bool RouterTableInfoFactory::isNeedUpdateFactory(const vector<string> &moduleList)
{
    // 判断是否有新增模块，如果是返回true
    vector<string>::const_iterator vIt = moduleList.begin();
    for (; vIt != moduleList.end(); ++vIt)
    {
        if (_routerTableInfo[_flag].find(*vIt) == _routerTableInfo[_flag].end())
        {
            return true;
        }
    }

    // 判断是否需要删除某个模块，如果是返回true
    map<string, RouterTableInfo *>::const_iterator mIt = _routerTableInfo[_flag].begin();
    for (; mIt != _routerTableInfo[_flag].end(); ++mIt)
    {
        if (find(moduleList.begin(), moduleList.end(), mIt->first) == moduleList.end())
            return true;
    }

    return false;
}
