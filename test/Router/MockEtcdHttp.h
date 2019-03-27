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
#ifndef __TEST_ETCD_HTTP_REQUEST_H__
#define __TEST_ETCD_HTTP_REQUEST_H__

#include <string>
#include "EtcdHttp.h"

static int g_etcdHttpRequestRc = 0;
static std::string g_etcdHttpResponse = R"(
        {
            "action": "set",
            "node": {
                "createdIndex": 2,
                "key": "/message",
                "modifiedIndex": 2,
                "value": "test_val"
            }
        })";

void restoreEtcdHttp()
{
    g_etcdHttpRequestRc = 0;
    g_etcdHttpResponse = R"(
        {
            "action": "set",
            "node": {
                "createdIndex": 2,
                "key": "/message",
                "modifiedIndex": 2,
                "value": "test_val"
            }
        })";
}

class MockEtcdHttpRequest : public EtcdHttpRequest
{
public:
    MockEtcdHttpRequest(const std::string &url, enum HttpMethod method)
        : EtcdHttpRequest(url, method)
    {
    }

    virtual int doRequest(int timeout, std::string *respContent) override
    {
        *respContent = g_etcdHttpResponse;
        return g_etcdHttpRequestRc;
    }
};
#endif  // __TEST_ETCD_HTTP_REQUEST_H__