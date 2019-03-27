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
#ifndef __MOCK_ROUTER_HANDLE_H__
#define __MOCK_ROUTER_HANDLE_H__

#include <gmock/gmock.h>

#include "RouterHandle.h"

using namespace std;

// 假设当前应用只支持下列两个模块
static const string MODULE1 = "modoule1";
static const string MODULE2 = "modoule2";
static const string NEW_MODULE = "module3";
 
static const vector<string> SUPPORT_MODULES = { MODULE1, MODULE2 };
static const vector<string> SUPPORT_MODULES_AFTER_ADD_NEW_MODULE = { MODULE1, MODULE2, NEW_MODULE }; // 有新模块上线
static const vector<string> SUPPORT_MODULES_AFTER_UPLOAD_MODULE = { MODULE1 }; // 有模块下线


class MockRouterHandle : public RouterHandle
{
public:
    MOCK_METHOD3(getRouterInfo, int(const string &moduleName, PackTable &packTable, bool needRetry));

    MOCK_METHOD3(getRouterVersion, int(const string &moduleName, int &version, bool needRetry));

    MOCK_METHOD3(getRouterVersionBatch, int(const vector<string> &moduleList, map<string, int> &mapModuleVersion, bool needRetry));

    MOCK_METHOD3(reportSwitchGroup, int(const string &moduleName, const string &groupName, const string &serverName));

    MOCK_METHOD1(getModuleList, int(vector<string> &moduleList));    
};

class FakeRouterHandle1 : public RouterHandle
{
public:
    virtual int getModuleList(vector<string> &moduleList) override
    {
        moduleList = SUPPORT_MODULES;
        return 0;   
    }
};

class FakeRouterHandle2 : public RouterHandle
{
public:
    virtual int getModuleList(vector<string> &moduleList) override
    {
        moduleList = SUPPORT_MODULES_AFTER_ADD_NEW_MODULE;
        return 0;   
    }
};

class FakeRouterHandle3 : public RouterHandle
{
public:
    virtual int getModuleList(vector<string> &moduleList) override
    {
        moduleList = SUPPORT_MODULES_AFTER_UPLOAD_MODULE;
        return 0;   
    }
};
/*
class MockRouterHandle : public RouterHandle
{
public:
    MockRouterHandle() {}

    virtual ~MockRouterHandle() {}

    virtual int getRouterInfo(const string &moduleName, PackTable &packTable, bool needRetry = false) override
    {
        return 0;
    }

    virtual int getRouterVersion(const string &moduleName, int &version, bool needRetry = false) override
    {
        return 0;
        
    }

    virtual int getRouterVersionBatch(const vector<string> &moduleList, map<string, int> &mapModuleVersion, bool needRetry = false) override
    { 
        return 0;
    }

    virtual int reportSwitchGroup(const string &moduleName, const string &groupName, const string &serverName) override
    { 
        return 0;
    }

    virtual int getModuleList(vector<string> &moduleList) override
    {
        moduleList.insert(moduleList.end(), { MODULE1, MODULE2 });
        return 0;   
    }
    
};
*/

#endif

