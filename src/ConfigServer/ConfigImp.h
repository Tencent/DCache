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
#ifndef _Config_Imp_H_
#define _Config_Imp_H_

#include "util/tc_mysql.h"
#include "util/tc_common.h"
#include "util/tc_config.h"
#include "servant/Application.h"
#include "RigUp.h"
#include "Config.h"

using namespace tars;

enum Level
{
    eLevelApp = 1,
    eLevelAllServer
};

/**
 *
 *
 */
class ConfigImp : public DCache::Config
{
public:
    /**
     * 构造函数
     */
    ConfigImp() {};

    /**
     * 析构函数
     */
    virtual ~ConfigImp() {};
 
    /**
     * 初始化
     */
    virtual void initialize();

    /**
     * 退出
     */
    virtual void destroy() {};

    /**
     * 读取配置文件 
     * @param appName: 应用名
     * @param serverName: 服务名 
     * @param config: 读取到的配置 
     * @return int: 成功返回0，否则返回-1 
     */
    virtual int readConfig(const string &appName, const string &serverName, string &config, tars::TarsCurrentPtr current);

    /**
     * 读取服务的配置文件，这是指在服务名存在的情况下，根据服务的ip来读取服务的配置
     * @param appServerName: 服务名称，由应用名称和服务名称合并构成 
     * @param host: 节点ip 
     * @param config: 读取到的配置  
     * @return int: 成功返回0，否则返回-1 
     */
    virtual int readConfigByHost(const string &appServerName, const string &host, string &config, tars::TarsCurrentPtr current);

    /**
     * 读取应用的配置文件
     * @param appName: 应用名 
     * @param config: 读取到的配置 
     * @return int: 成功返回0，否则返回-1 
     */
    int readAppConfig(const string &appName, string &config, tars::TarsCurrentPtr current);
protected:

    /**
     * 读取服务的引用配置文件
     * @param appServerName: 服务名称
     * @param host: 节点ip 
     * @param referenceId: 该服务的引用文件的id，引用文件可能不止一个
     * @return int: 成功返回0，否则返回-1 
     */
    int getReferenceIds(const string &appServerName, const string &host, vector<int> &referenceIds);

    /**
     * 根据配置文件id来读取配置
     * @param configId: 配置文件的id
     * @param config: 读取到的配置 
     * @return int: 成功返回0，否则返回-1 
     */
    int readConfigById(const int configId, string& config);

    /**
     * 读取所有的配置项，服务通过与其结果中的配置项id比较来匹配到配置项的路径
     */
    int loadItems();

private:
    /**
     * 存放配置项的数组，配置项id、配置项和配置项的路径一一对应
     */
    vector<ConfigItem> _items;

    TC_Config _conf;

    TC_Mysql _mysqlConfig;
};

#endif
