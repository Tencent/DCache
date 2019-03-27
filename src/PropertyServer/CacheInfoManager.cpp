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
#include "CacheInfoManager.h"
#include "servant/Application.h"

void CacheInfoManager::init(const TC_Config& conf)
{
    TC_DBConf tcDBConf;
    tcDBConf.loadFromMap(conf.getDomainMap("/Main/DB/relation"));
    _mysqlRelation.init(tcDBConf);

    reload();
}

void CacheInfoManager::reload()
{
    string sSql = "select * from t_cache_router";
    try
    {
        TC_Mysql::MysqlData res = _mysqlRelation.queryRecord(sSql);
        
        TLOGDEBUG("CacheInfoManager::reload sql:" << sSql << "|res.size:" << res.size() << endl);

        int iUpdateIndex = _moduleInfoIndex == 1 ? 0 : 1;
        _moduleInfos[iUpdateIndex].clear();
        for(size_t i = 0; i < res.size(); ++i)
        {
            CacheModuleInfo info;
            info._appName	   = res[i]["app_name"];
            info._moduleName   = res[i]["module_name"];
            info._idcArea	   = res[i]["idc_area"];
            info._groupName    = res[i]["group_name"];
            info._serverStatus = res[i]["server_status"];
            _moduleInfos[iUpdateIndex][res[i]["cache_name"]] = info;
        }

        if(!_moduleInfos[iUpdateIndex].empty())
        {
            TC_ThreadWLock wlock(_rwl);
            _moduleInfoIndex = iUpdateIndex;
        }
    }
    catch(TC_Mysql_Exception & ex)
    {
        TLOGERROR("CacheInfoManager::reload exception:" << ex.what() << endl);
    }
    catch(...)
    {
        TLOGERROR("CacheInfoManager::reload unknown exception." << endl);
    }
}

bool CacheInfoManager::getModuleInfo(const string &cacheName, CacheModuleInfo &moduleInfo) const
{
    TC_ThreadWLock rlock(_rwl);
    
    const map<string, CacheModuleInfo> &infos = _moduleInfos[_moduleInfoIndex];
    map<string, CacheModuleInfo>::const_iterator it = infos.find(cacheName);
    if(it != infos.end())
    {
        moduleInfo = it->second;
        
        return true;
    }

    return false;
}