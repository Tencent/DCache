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
#ifndef __MOCK_CACHE_PROXY_H__
#define __MOCK_CACHE_PROXY_H__

#include <gmock/gmock.h>
#include "servant/Global.h"

#include "Proxy.h"
#include "ProxyShare.h"


class InterfaceOfProxy
{
  public:
    virtual void async_response_getKV(tars::TarsCurrentPtr current, tars::Int32 _ret, const DCache::GetKVRsp &rsp)
    {

    }
};

class MockProxy : public InterfaceOfProxy
{
  public:
    MOCK_METHOD3(async_response_getKV, void(tars::TarsCurrentPtr current, tars::Int32 _ret, const DCache::GetKVRsp &rsp));
};

class FakeProxy
{
  public:
    static void async_response_getKV(tars::TarsCurrentPtr current, tars::Int32 _ret, const DCache::GetKVRsp &rsp)
    {
        if (!proxy) { cout << "you should set FakeProxy::proxy before" << endl; return; }
        proxy->async_response_getKV(current, _ret, rsp);
    }

    static void setProxy(InterfaceOfProxy *proxy1)
    {
        proxy = proxy1;
    }
  public:
    static InterfaceOfProxy *proxy;
};

InterfaceOfProxy *FakeProxy::proxy = nullptr;


#endif