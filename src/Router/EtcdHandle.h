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
// 提供了一些通过ETCD来进行操作的接口。
#ifndef __ETCDHANDLE_H_
#define __ETCDHANDLE_H_

#include <string>
#include "EtcdHost.h"
#include "EtcdRequestCallback.h"
#include "RouterServerConfig.h"
#include "util/tc_http_async.h"
#include "util/tc_singleton.h"

class RspHandler : public TC_Thread, public TC_ThreadLock
{
public:
    RspHandler() : _terminate(false) {}

    virtual ~RspHandler();

    void handleEtcdRsp(const std::shared_ptr<EtcdRspItem> &item) const;

    virtual void run() override;

    void terminate();

    void addRspItem(std::shared_ptr<EtcdRspItem> item);

private:
    void handleCommon(const std::string &respContent) const;

private:
    bool _terminate;  // 线程是否终止
    tars::TC_ThreadLock _queue_lock;
    std::queue<std::shared_ptr<EtcdRspItem>> _queue;
};

class EtcdHandle
{
public:
    EtcdHandle() = default;

    int init(const RouterServerConfig &config, std::shared_ptr<EtcdHost> etcdHost);

    // 通过ETCD获取RouterObj
    int getRouterObj(std::string *obj) const;

    // 原子地创建一个键
    int createCAS(int ttl, const std::string &keyvalue) const;

    // 原子地为一个键保活
    int refreshCAS(int ttl, const std::string &prevValue);

    // 监听Router主机
    int watchRouterMaster() const;

    void getModuleInfo(std::string &app,
                       std::string &module,
                       std::string &group,
                       std::string &server) const
    {
        app = _appName;
        module = _moduleName;
        group = _groupName;
        server = ServerConfig::Application + "." + ServerConfig::ServerName;
    }

    void destroy();

protected:
    virtual std::unique_ptr<EtcdHttpRequest> makeEtcdHttpRequest(const std::string &url,
                                                                 enum HttpMethod method) const
    {
        return std::unique_ptr<EtcdHttpRequest>(new EtcdHttpRequest(url, method));
    }

private:
    // 同步GET请求ETCD
    int doGetEtcd(const std::string &url, std::string *keyvalue) const;

private:
    std::string _appName;                 // APP名称
    std::string _moduleName;              // 模块名称
    std::string _groupName;               // 分组名称
    tars::TC_HttpAsync _asyncHttp;        // 异步HTTP通信句柄
    std::shared_ptr<RspHandler> _rspHandler;            // 处理ETCD响应的线程句柄
    std::shared_ptr<EtcdHost> _etcdHost;  // EtcdHost线程
};

#endif  // __ETCDHANDLE_H_
