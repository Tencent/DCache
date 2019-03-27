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
#ifndef __CACHEINFO_MANAGER_H_
#define __CACHEINFO_MANAGER_H_

#include "util/tc_mysql.h"
#include "util/tc_config.h"
#include "util/tc_singleton.h"
#include "util/tc_thread_rwlock.h"

using namespace tars;

struct CacheModuleInfo
{
    string _moduleName;
    string _appName;
    string _idcArea;
    string _groupName;
    string _serverStatus;
};

class CacheInfoManager : public TC_Singleton<CacheInfoManager>
{
public:
    /**
     * 构造函数
     */
    CacheInfoManager() : _moduleInfoIndex(0)
    {}
    
    /**
     * 析构函数
     */
    ~CacheInfoManager() {}

    /**
     * 初始化, 只会进程调用一次
     */
    void init(const TC_Config& conf);

    /**
     * 重新加载所有Cache信息
     */
    void reload();

    /**
     * 获取指定Cache服务的模块信息
     */
    bool getModuleInfo(const string &cacheName, CacheModuleInfo &moduleInfo) const;

private:

    TC_Mysql _mysqlRelation;

    mutable TC_ThreadRWLocker  _rwl;
    
    size_t _moduleInfoIndex;
    
    map<string, CacheModuleInfo> _moduleInfos[2];
};

#endif

