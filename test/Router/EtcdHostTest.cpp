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
#include <unistd.h>
#include "EtcdHost.h"
#include "EtcdHttp.h"
#include "MockEtcdHttp.h"
#include "MockRouterServerConfig.h"


namespace
{
std::string etcdHostsInConfig("127.0.0.1:2379;127.0.0.2:2379;127.0.0.3:2379");
}  // namespace

class TestRouterServerConfig : public MockRouterServerConfig
{
public:
    virtual std::vector<std::string> getEtcdHosts(const std::string &defaultHost) const override
    {
        return tars::TC_Common::sepstr<std::string>(etcdHostsInConfig, ";");
    }
};

class TestEtcdHost : public EtcdHost
{
public:
    virtual std::unique_ptr<EtcdHttpRequest> makeEtcdHttpRequest(
        const std::string &url, enum HttpMethod method) const override
    {
        return std::unique_ptr<EtcdHttpRequest>(new MockEtcdHttpRequest(url, method));
    }

    virtual std::string createRandomTestValue() const override { return "test_val"; }
};

TEST(EtcdHost, choseHost_sanity)
{
    TestEtcdHost host;
    TestRouterServerConfig cfg;
    int rc = host.init(cfg);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(host.choseHost(), "http://127.0.0.1:2379/v2/keys");
    EXPECT_EQ(host.choseHost(), "http://127.0.0.2:2379/v2/keys");
    EXPECT_EQ(host.choseHost(), "http://127.0.0.3:2379/v2/keys");
    EXPECT_EQ(host.getRouterHost(),
              "http://127.0.0.1:2379/v2/keys/Dcache.unittest/router/router_endpoint");
    EXPECT_EQ(host.getRouterHost(),
              "http://127.0.0.2:2379/v2/keys/Dcache.unittest/router/router_endpoint");
    EXPECT_EQ(host.getRouterHost(),
              "http://127.0.0.3:2379/v2/keys/Dcache.unittest/router/router_endpoint");
}

TEST(EtcdHost, choseHost_failed)
{
    TestEtcdHost host;
    TestRouterServerConfig cfg;
    g_etcdHttpRequestRc = -1;
    int rc = host.init(cfg);
    EXPECT_EQ(rc, -1);
    g_etcdHttpRequestRc = 0;
}

TEST(EtcdHost, run_and_terminate)
{
    TestEtcdHost host;
    TestRouterServerConfig cfg;
    int rc = host.init(cfg);
    EXPECT_EQ(rc, 0);
    host.start();
    // 休眠3秒，等待EtcdHost线程获取主机信息
    ::sleep(2);
    EXPECT_EQ(host.choseHost(), "http://127.0.0.1:2379/v2/keys");
    EXPECT_EQ(host.choseHost(), "http://127.0.0.2:2379/v2/keys");
    EXPECT_EQ(host.choseHost(), "http://127.0.0.3:2379/v2/keys");
    EXPECT_EQ(host.getRouterHost(),
              "http://127.0.0.1:2379/v2/keys/Dcache.unittest/router/router_endpoint");
    EXPECT_EQ(host.getRouterHost(),
              "http://127.0.0.2:2379/v2/keys/Dcache.unittest/router/router_endpoint");
    EXPECT_EQ(host.getRouterHost(),
              "http://127.0.0.3:2379/v2/keys/Dcache.unittest/router/router_endpoint");
    host.terminate();
}

TEST(EtcdHost, chose_empty_host)
{
    TestEtcdHost host;
    TestRouterServerConfig cfg;
    std::string cfg_bak = etcdHostsInConfig;
    etcdHostsInConfig = "";
    EXPECT_EQ(host.init(cfg), -1);
    etcdHostsInConfig = cfg_bak;
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}