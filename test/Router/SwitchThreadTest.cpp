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
#include <gtest/gtest.h>
#include "DbHandle.h"
#include "ProxyWrapper.h"
#include "SwitchThread.h"

using std::string;
using std::vector;

const string active_active = "192.168.0.1";
const string active_inactive = "192.168.0.2";
const string access_timeout = "192.168.0.3";
const string access_error = "192.168.0.4";
const string error_state = "192.168.0.5";

class TestAdminRegProxyWrapper : public AdminRegProxyWrapper
{
public:
    explicit TestAdminRegProxyWrapper(const string &obj) {}

    virtual void tars_timeout(int msecond) override {}

    virtual tars::Int32 getServerState(const string &application,
                                       const string &serverName,
                                       const string &nodeName,
                                       tars::ServerStateDesc &state,
                                       std::string &result,
                                       const map<string, string> &context = TARS_CONTEXT(),
                                       map<string, string> *pResponseContext = NULL) override
    {
        if (serverName == access_timeout)
        {
            result = "TimeOut";
            return -1;
        }

        if (serverName == access_error)
        {
            result = "error";
            return -1;
        }

        if (serverName == active_active)
        {
            state.settingStateInReg = "active";
            state.presentStateInReg = "active";
            return 0;
        }

        if (serverName == active_inactive)
        {
            state.settingStateInReg = "active";
            state.presentStateInReg = "inactive";
            return 0;
        }

        if (serverName == error_state)
        {
            state.settingStateInReg = "active";
            state.presentStateInReg = "inactive";
            return 0;
        }

        return -1;
    }
};

class TestDbHandle : public DbHandle
{
public:
    int getAllServerInIp(const string &strIp, vector<string> &vServerName) override
    {
        (void)strIp;
        vServerName = server_names;
        return 0;
    }

    vector<string> server_names;
};

TEST(DoCheckTransThread, checkMachineDown)
{
    DoCheckTransThread t;
    std::shared_ptr<AdminRegProxyWrapper> p = std::make_shared<TestAdminRegProxyWrapper>("test");
    std::shared_ptr<TestDbHandle> d = std::make_shared<TestDbHandle>();
    t.init(p, "", "", "", d);

    d->server_names.clear();
    d->server_names.push_back(active_inactive);
    EXPECT_EQ(t.checkMachineDown(""), 1);
    d->server_names.push_back(active_inactive);
    EXPECT_EQ(t.checkMachineDown(""), 1);
    d->server_names.push_back(active_active);
    EXPECT_EQ(t.checkMachineDown(""), 0);

    d->server_names.clear();
    d->server_names.push_back(access_timeout);
    EXPECT_EQ(t.checkMachineDown(""), 1);
    d->server_names.push_back(access_error);
    EXPECT_EQ(t.checkMachineDown(""), -1);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}