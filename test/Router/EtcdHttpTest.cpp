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
#include <string>
#include "EtcdHttp.h"


namespace
{
const std::string test_url("www.qq.com");
const std::string test_key("test_key");
const std::string test_value("test_value");
const int ttl = 10;

const std::string put_value_resp = R"(
        {
            "action": "set",
            "node": {
                "createdIndex": 2,
                "key": "/message",
                "modifiedIndex": 2,
                "value": "Hello world"
            }
        })";

const std::string error_content = R"({
    "cause": "/foo",
    "errorCode": 105,
    "index": 39776,
    "message": "Key already exists"
})";
}  // namespace

TEST(EtcdHttpRequest, etcd_create_watch_key)
{
    EtcdHttpRequest httpReq(test_url, HTTP_GET);
    httpReq.setDefaultHeader();

    httpReq.setKey(test_key);
    EXPECT_EQ(httpReq.dumpURL(), "www.qq.com/test_key");

    httpReq.setWatchKey();
    EXPECT_EQ(httpReq.dumpURL(), "www.qq.com/test_key?wait=true");
}

TEST(EtcdHttpRequest, etcd_create_and_refresh_key)
{
    EtcdHttpRequest httpReq(test_url, HTTP_PUT);
    httpReq.setDefaultHeader();
    httpReq.setKey(test_key);
    EXPECT_EQ(httpReq.dumpURL(), "www.qq.com/test_key");

    httpReq.setPrevExist(false);
    EXPECT_EQ(httpReq.dumpRequestBody(), "prevExist=false");
    httpReq.setKeyTTL(ttl);
    EXPECT_EQ(httpReq.dumpRequestBody(), "prevExist=false&ttl=10");
    httpReq.setValue(test_value);
    EXPECT_EQ(httpReq.dumpRequestBody(), "prevExist=false&ttl=10&value=test_value");

    httpReq = EtcdHttpRequest(test_url, HTTP_PUT);
    httpReq.setDefaultHeader();
    httpReq.setKey(test_key);
    httpReq.setRefresh(true);
    httpReq.setPrevExist(true);
    httpReq.setKeyTTL(ttl);
    httpReq.setPrevValue(test_value);
    EXPECT_EQ(httpReq.dumpRequestBody(), "refresh=true&prevExist=true&ttl=10&prevValue=test_value");

    std::string domain;
    uint32_t port = 0;
    httpReq.getHostPort(domain, port);
    EXPECT_EQ(domain, "www.qq.com");
    EXPECT_EQ(port, 80);
}

TEST(EtcdHttpRequest, do_request_timeout)
{
    EtcdHttpRequest putReq(test_url, HTTP_PUT);
    std::string resp;
    int rc = putReq.doRequest(1, &resp);
    EXPECT_NE(rc, 0);

    EtcdHttpRequest getReq(test_url, HTTP_GET);
    rc = putReq.doRequest(1, &resp);
    EXPECT_NE(rc, 0);
}

class TestEtcdHttpParser : public ::testing::Test
{
protected:
    virtual void SetUp() override {}

    void parse(const std::string &content)
    {
        EXPECT_EQ(0, parser_.parseContent(content)) << " parsed error :" << content;
    }
    EtcdHttpParser parser_;
};

TEST_F(TestEtcdHttpParser, regular)
{
    parse(put_value_resp);

    std::string action;
    parser_.getAction(&action);
    EXPECT_EQ(action, "set");

    std::string key;
    parser_.getCurrentNodeKey(&key);
    EXPECT_EQ(key, "/message");

    std::string value;
    parser_.getCurrentNodeValue(&value);
    EXPECT_EQ(value, "Hello world");

    int errCode;
    std::string message;
    int rc = parser_.getEtcdError(&errCode, &message);
    EXPECT_EQ(rc, 0);
}

TEST_F(TestEtcdHttpParser, parse_etcd_error)
{
    parse(error_content);

    int errCode;
    std::string message;
    int rc = parser_.getEtcdError(&errCode, &message);
    EXPECT_EQ(rc, -1);
    EXPECT_EQ(errCode, 105);
    EXPECT_EQ(message, "Key already exists");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}