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
#ifndef _ROUTERHANDLE_H_
#define _ROUTERHANDLE_H_

#include <vector>
#include <map>

#include "servant/Application.h"

#include "Router.h"

using namespace std;
using namespace tars;
using namespace DCache;

class RouterHandle : public TC_Singleton<RouterHandle>
{
  public:
    RouterHandle();

    virtual ~RouterHandle();

    void init(TC_Config &conf);

    // 从路由服务获取某模块的路由表(压缩后)
    virtual int getRouterInfo(const string &moduleName, PackTable &packTable, bool needRetry = false);

    // 从路由服务获取某模块的路由表的最新版本号
    virtual int getRouterVersion(const string &moduleName, int &version, bool needRetry = false);

    // 从路由服务批量获取多个模块的路由表的最新版本号
    virtual int getRouterVersionBatch(const vector<string> &moduleList, map<string, int> &mapModuleVersion, bool needRetry = false);

    virtual int reportSwitchGroup(const string &moduleName, const string &groupName, const string &serverName);

    // 从路由服务获取其所支持的模块的名字
    virtual int getModuleList(vector<string> &moduleList);

  protected:
    // 根据配置文件提供的Router Obj获取路由代理指针
    int getRouterPrx(RouterPrx &routerPrx);

    // 更新路由代理指针
    int updateRouterPrx(RouterPrx &routerPrx);

  private:
    TC_ThreadLock _lock; // 线程锁

    string _routeObj;

    string _routerProxy;

    vector<TC_Endpoint> _routerEndpoints;

    size_t _endpointIndex;

    RouterPrx _routerPrx;

    time_t _lastGetEndpointTime;

    TC_Endpoint _curEndpoint;
};

#endif
