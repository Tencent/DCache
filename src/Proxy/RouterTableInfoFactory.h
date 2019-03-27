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
#ifndef _ROUTERTABLE_FACTORY_H
#define _ROUTERTABLE_FACTORY_H

#include <map>
#include <vector>

#include "util/tc_common.h"
#include "servant/Application.h"
#include "util/tc_timeprovider.h"

#include "Router.h"
#include "UnpackTable.h"
#include "RouterHandle.h"

using namespace DCache;
using namespace tars;
using namespace std;

typedef UnpackTable RouterTable;

class RouterTableInfo
{
  public:
    RouterTableInfo(const string &moduleName, const string &localRouterFile, shared_ptr<RouterHandle> pRouterHandle);

    ~RouterTableInfo() {}

    static void setMaxUpdateFrequency(int maxUpdateFrequency)
    {
        _maxUpdateFrequency = maxUpdateFrequency;
    }

    static string showStatus();

    int initRouterTable();

    int update();

    int updateRouterTable();

    int updateRouterTableByVersion(int iVersion);

    RouterTable &getRouterTable()
    {
        return _routerTable;
    }

    void setRouterTableToFile()
    {
        _routerTable.toFile(_localRouterFile);
    }

  private:
    string _moduleName; // 模块名

    string _localRouterFile; // 本地路由文件

    time_t _lastUpdateBeginTime; // 本轮更新的起始时间

    int _updateTimes; // 本轮更新已经更新次数

    RouterTable _routerTable; // 路由表

    TC_ThreadLock _threadLock; // 线程锁

    shared_ptr<RouterHandle> _pRouterHandle; // 负责和路由服务(通过网络)通信的单例类

    static int _maxUpdateFrequency; // 主动更新路由表的最大频率，不包括定时更新
};


class RouterTableInfoFactory
{
  public:
    RouterTableInfoFactory(RouterHandle *pRouterHanle);

    ~RouterTableInfoFactory() {}

    // 下载当前应用所支持的模块的路由数据，一个模块对应一个RouterTableInfo对象(存储路由数据)
    int init(TC_Config &conf);

    string reloadConf(TC_Config &conf);

    // 主动去Router拉取路由信息数据，为ProxyServer当前支持的每个模块更新对应的路由表
    void synRouterTableInfo();

    RouterTableInfo *getRouterTableInfo(const string &moduleName);

    // 根据最新的模块信息(有新增模块或者有下线模块)从而判断是否需要更新路由工厂
    bool isNeedUpdateFactory(const vector<string> &moduleList);

    string showStatus();

    // 是否支持模块modulName
    bool supportModule(const string &moduleName);

    // 检查是否有新增模块或者已经下线的模块，从而新增或删除路由表
    void checkModuleChange();
    

  private:
    volatile int _flag;

    vector<map<string, RouterTableInfo *>> _routerTableInfo;

    TC_ThreadLock _threadLock; //线程锁

    string _baseLocalRouterFile; //路由文件前缀

    time_t _lastUpdateFactoryTime; //最近一次更新RouterTableInfoFactory所支持模块的时间

    time_t _minUpdateFactoryInterval; //更新RouterTableInfoFactory所支持模块的最小时间间隔

    shared_ptr<RouterHandle> _pRouterHandle; // 负责和路由服务(通过网络)通信的单例类
};

#endif
