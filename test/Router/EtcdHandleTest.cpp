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
#include "EtcdHandle.h"
#include "EtcdHost.h"
#include "MockEtcdHttp.h"
#include "MockRouterServerConfig.h"
#include "RouterServer.h"


extern RouterServer g_app;

class MockEtcdHost : public EtcdHost
{
public:
    virtual int init(const RouterServerConfig &conf) override { return 0; }

    // 因为TC_Thread::start方法不是virtual，所以只能覆盖run方法。
    virtual void run() override { return; }

    virtual std::string getRouterHost() override
    {
        return "http://100.100.100.100:2379/v2/keys/Dcache.unittest/router/router_endpoint";
    }
};

class MockEtcdHandle : public EtcdHandle
{
public:
    virtual std::unique_ptr<EtcdHttpRequest> makeEtcdHttpRequest(const std::string &url,
                                                                 enum HttpMethod method) const
    {
        return std::unique_ptr<MockEtcdHttpRequest>(new MockEtcdHttpRequest(url, method));
    }
};

class TestEtcdHandle : public ::testing::Test
{
protected:
    void SetUp() override
    {
        etcdHost = std::make_shared<MockEtcdHost>();
        EXPECT_EQ(handle.init(cfg, etcdHost), 0);
    }
    void TearDown() override
    {
        handle.destroy();
        restoreEtcdHttp();
    }

    MockRouterServerConfig cfg;
    MockEtcdHandle handle;
    std::shared_ptr<MockEtcdHost> etcdHost;
};

TEST_F(TestEtcdHandle, get_router_obj)
{
    g_etcdHttpRequestRc = 0;
    g_etcdHttpResponse = R"({
    "action": "get",
        "node": {
            "createdIndex": 2,
            "key": "/message",
            "modifiedIndex": 2,
            "value": "DCache.unittest.RouterObj@127.0.0.1:2379"
        }
    })";
    std::string obj;
    EXPECT_EQ(handle.getRouterObj(&obj), 0);
    EXPECT_EQ(obj, "DCache.unittest.RouterObj@127.0.0.1:2379");
}

TEST_F(TestEtcdHandle, create_cas)
{
    g_etcdHttpRequestRc = 0;
    g_etcdHttpResponse = R"({
    "action": "create",
        "node": {
            "createdIndex": 6,
            "key": "/queue/00000000000000000006",
            "modifiedIndex": 6,
            "value": "Job1"
        }
    })";
    EXPECT_EQ(handle.createCAS(60, "test_val"), 0);
}

// TODO : Tars的异步HTTP请求代码有BUG，待Tars更新代码后再测试。
// TEST_F(TestEtcdHandle, refresh_cas)
// {
//     g_etcdHttpRequestRc = 0;
//     g_etcdHttpResponse = R"({
//     "action": "create",
//         "node": {
//             "createdIndex": 6,
//             "key": "/queue/00000000000000000006",
//             "modifiedIndex": 6,
//             "value": "Job1"
//         }
//     })";
//     EXPECT_EQ(handle.refreshCAS(60, "test_val"), 0);
// }

TEST_F(TestEtcdHandle, watch_router_master)
{
    g_etcdHttpRequestRc = 0;
    g_etcdHttpResponse = R"({
        "action": "expire",
        "node": {
            "createdIndex": 8,
            "key": "/dir",
            "modifiedIndex": 15
        },
        "prevNode": {
            "createdIndex": 8,
            "key": "/dir",
            "dir":true,
            "modifiedIndex": 17,
            "expiration": "2013-12-11T10:39:35.689275857-08:00"
        }
    })";
    EXPECT_EQ(handle.watchRouterMaster(), 0);
}

TEST_F(TestEtcdHandle, watch_router_master_time_out)
{
    g_etcdHttpRequestRc = -1;
    EXPECT_EQ(handle.watchRouterMaster(), -1);
}

TEST(RspHandler, handleEtcdRsp)
{
    // 先设置成master，待处理错误响应后检查是否降级为slave
    g_app.setRouterType(ROUTER_MASTER);
    RspHandler rsp;
    TC_ThreadControl ctl = rsp.start();

    std::shared_ptr<EtcdRspItem> item1 = std::make_shared<EtcdRspItem>();
    item1->ret = RET_SUCC;
    item1->reqInfo.action = ETCD_CAS_REFRESH;
    item1->respContent = R"({
        "action": "create",
            "node": {
                "createdIndex": 6,
                "key": "/queue/00000000000000000006",
                "modifiedIndex": 6,
                "value": "Job1"
            }
        })";

    rsp.addRspItem(item1);
    ctl.sleep(1);

    EXPECT_EQ(g_app.getRouterType(), ROUTER_MASTER);

    std::shared_ptr<EtcdRspItem> item2 = std::make_shared<EtcdRspItem>();
    item2->reqInfo.action = ETCD_CAS_REFRESH;
    item2->respContent = R"({
        "action": "create",
            "node": {
                "createdIndex": 6,
                "key": "/queue/00000000000000000006",
                "modifiedIndex": 6,
                "value": "Job1"
            }
        })";
    item2->ret = RET_ERROR_CODE;
    rsp.addRspItem(item2);
    ctl.sleep(1000);
    EXPECT_EQ(g_app.getRouterType(), ROUTER_SLAVE);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}