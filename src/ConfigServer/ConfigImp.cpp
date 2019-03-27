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
#include "ConfigImp.h"
#include "ConfigServer.h"

using namespace std;
using namespace tars;
using namespace DCache;


void ConfigImp::initialize()
{
    _conf.parseFile(ServerConfig::BasePath + "ConfigServer.conf");

    // 初始化数据库连接
    TC_DBConf tcDBConf;
    tcDBConf.loadFromMap(_conf.getDomainMap("/Main/DB"));
    _mysqlConfig.init(tcDBConf);
}

int ConfigImp::readConfig(const string &appName, const string &serverName, string &config, tars::TarsCurrentPtr current)
{
    if(!serverName.empty())
    {
        return readConfigByHost(appName + "." + serverName, current->getIp(), config, current);
    }
    else
    {
        return readAppConfig(appName, config, current);
    }
}

int ConfigImp::readConfigByHost(const string &appServerName, const string &host, string &config, tars::TarsCurrentPtr current)
{
    TLOGDEBUG("ConfigImp::readConfigByHost appServerName:" << appServerName << "|host:" << host << endl);
    
    config = "";

    //加载配置文件的所有配置项
    loadItems();

    //多配置文件的分割符
    string sSep = "\r\n\r\n";
    try
    {
        string sIpServerConfig = "";

        string sSql =
            "select item_id,config_value from t_config_table "
            "where server_name = '" + _mysqlConfig.escapeString(appServerName) + "' "
            "and host = '" + _mysqlConfig.escapeString(host) + "' "
            "and level = " + TC_Common::tostr<int>(eLevelAllServer);

        TC_Mysql::MysqlData res = _mysqlConfig.queryRecord(sSql);

        TLOGDEBUG("ConfigImp::readConfigByHost sql:" << sSql << "|res.size:" << res.size() << endl);

        if(res.size() != 0)
        {
            map<string, string> content;
            content.clear();
            for(size_t i = 0; i < res.size(); ++i)
            {
                content.insert(map<string, string>::value_type(res[i]["item_id"], res[i]["config_value"]));
            }

            Rigup rigup;
            if(rigup.enter(_items, content) != 0)
            {
                return -1;
            }

            sIpServerConfig = rigup.getString();
        }

        vector<int> referenceIds;

        int iRet = getReferenceIds(appServerName, host, referenceIds);

        for(size_t i = 0; (iRet == 0) && (i < referenceIds.size()); ++i)
        {
            string refConfig;
            iRet = readConfigById(referenceIds[i], refConfig);

            if(iRet == 0 && (!refConfig.empty()))
            {
                if(config.empty())
                {
                    config += refConfig;
                }
                else
                {
                    config += sSep + refConfig;
                }
            }
        }

        if(iRet != 0)
        {
            return -1;
        }

        //添加配置本身
        if(!sIpServerConfig.empty())
        {
            if(config.empty())
            {
                config += sIpServerConfig;
            }
            else
            {
                config += sSep + sIpServerConfig;
            }
        }

    }
    catch(TC_Mysql_Exception & ex)
    {
        config = ex.what();
        TLOGERROR("ConfigImp::readConfigByHost exception:" << ex.what() << endl);
        return -1;
    }
    catch(...)
    {
        TLOGERROR("ConfigImp::readConfigByHost unknown exception." << endl);
        return  -1;
    }

    TLOGDEBUG("ConfigImp::readConfigByHost config:\n" << config << endl);

    return 0;
}

int ConfigImp::readAppConfig(const string &appName, string &config, tars::TarsCurrentPtr current)
{
    TLOGDEBUG("ConfigImp::readAppConfig app:" << appName << endl);

    config = "";

    // 加载配置文件的所有配置项
    loadItems();
    
    try
    {
        //查公有配置
        string sSql =
            "select item_id,config_value from t_config_table "
            "where server_name = '" + _mysqlConfig.escapeString(appName) + "' "
            "and level=" + TC_Common::tostr<int>(eLevelApp);

        TC_Mysql::MysqlData res = _mysqlConfig.queryRecord(sSql);

        TLOGDEBUG("ConfigImp::readAppConfig sql:" << sSql << "|res.size:" << res.size() << endl);

        map<string, string> content;
        content.clear();
        for(size_t i = 0; i < res.size(); ++i)
        {
            content.insert(map<string, string>::value_type(res[i]["item_id"], res[i]["config_value"]));
        }

        Rigup rigup;
        int iRet = rigup.enter(_items, content);
        if(iRet != 0)
        {
            return -1;
        }

        config = rigup.getString();
    }
    catch(TC_Mysql_Exception & ex)
    {
        config = ex.what();
        TLOGERROR("ConfigImp::readAppConfig exception:" << ex.what() << endl);
        return -1;
    }
    catch(...)
    {
        TLOGERROR("ConfigImp::readAppConfig unknown exception." << endl);
        return -1;
    }

    return 0;
}

int ConfigImp::loadItems()
{
    _items.clear();

    try
    {
        //直接遍历映射表，取出所有字段
        string sSql = "select id, item, path from t_config_item";
    
        TC_Mysql::MysqlData res = _mysqlConfig.queryRecord(sSql);
        for (size_t i = 0; i < res.size(); ++i)
        {
            ConfigItem item;
            item._id   = res[i]["id"];
            item._item = res[i]["item"];
            item._path = res[i]["path"];
            _items.push_back(item);
        }
    }
    catch(TC_Mysql_Exception & ex)
    {
        TLOGERROR("ConfigImp::readMapTable exception:" << ex.what() << endl);
        return -1;
    }
    catch(...)
    {
        TLOGERROR("ConfigImp::readMapTable unknown exception." << endl);
        return -1;
    }

    return 0;
}

int ConfigImp::getReferenceIds(const string &appServerName, const string &host, vector<int> &referenceIds)
{
    referenceIds.clear();

    try
    {
        string sql =
            "select reference_id from t_config_reference "
            "where server_name = '" + _mysqlConfig.escapeString(appServerName) + "' " 
            "and host = '" + _mysqlConfig.escapeString(host) + "'";

        TC_Mysql::MysqlData res = _mysqlConfig.queryRecord(sql);
        
        TLOGDEBUG("ConfigImp::loadConfigByHost sql:" << sql << "|res.size:" << res.size() << endl);

        for(size_t i = 0 ; i < res.size(); ++i)
        {
            int iRefId = TC_Common::strto<int>(res[i]["reference_id"]);
            referenceIds.push_back(iRefId);
        }
    }
    catch(TC_Mysql_Exception & ex)
    {
        TLOGERROR("ConfigImp::getReferenceIds exception:" << ex.what() << endl);
        return -1;
    }
    catch(...)
    {
        TLOGERROR("ConfigImp::getReferenceIds unknown exception" << endl);
        return -1;
    }

    return 0;
}

int ConfigImp::readConfigById(const int configId, string& config)
{
    config = "";

    //按照建立引用的先后返回
    try
    {
        string sSql = 
            "select item_id,config_value from t_config_table where config_id = '" + TC_Common::tostr(configId) + "'";

        TC_Mysql::MysqlData res = _mysqlConfig.queryRecord(sSql);

        TLOGDEBUG("ConfigImp::readConfigById sql:" << sSql << "|res.size:" << res.size() << endl);

        if(res.size() != 0) 
        {
            map<string, string> content;
            content.clear();
            for(size_t i = 0; i < res.size(); ++i)
            {
                content.insert(map<string, string>::value_type(res[i]["item_id"], res[i]["config_value"]));
            }

            Rigup rigup;
            int iRet = rigup.enter(_items, content);
            if(iRet != 0)
            {
                return -1;
            }

            config = rigup.getString();
        }
    }
    catch(TC_Mysql_Exception & ex)
    {
        TLOGERROR("ConfigImp::readConfigById exception:" << ex.what() << endl);
        return -1;
    }
    catch(...)
    {
        TLOGERROR("ConfigImp::readConfigById unknown exception" << endl);
        return -1;
    }

    return 0;
}
