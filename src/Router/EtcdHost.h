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
// EtcdHost会开启一个线程，不断去更新所有存活的Etcd主机列表，将挂掉的主机剔除。
// 并对外提供接口去获取可用的ETCD主机信息。
#ifndef __ETCDHOST_H_
#define __ETCDHOST_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "EtcdHttp.h"
#include "RouterServerConfig.h"
#include "util/tc_http.h"
#include "util/tc_singleton.h"
#include "util/tc_thread.h"
#include "util/tc_thread_rwlock.h"

class EtcdHost : public tars::TC_Thread, public tars::TC_ThreadLock
{
public:
    EtcdHost() : _terminate(false), _count(0) {}

    virtual ~EtcdHost();

    virtual int init(const RouterServerConfig &conf);

    // 挑选一台ETCD主机，返回它的ETCD URL
    std::string choseHost();

    // 获取router对应的ETCD URL
    virtual std::string getRouterHost();

    void terminate();

protected:
    virtual std::unique_ptr<EtcdHttpRequest> makeEtcdHttpRequest(const std::string &url,
                                                                 enum HttpMethod method) const
    {
        return std::unique_ptr<EtcdHttpRequest>(new EtcdHttpRequest(url, method));
    }

    // 生成随机的测试值
    virtual std::string createRandomTestValue() const;

private:
    struct HostInfo
    {
        std::string _ip;
        std::string _port;
        std::string _url;
        HostInfo() {}
        HostInfo(const std::string &ip, const std::string &port) : _ip(ip), _port(port)
        {
            _url = "http://" + _ip + ":" + _port + "/v2/keys";
        }
    };

private:
    typedef std::map<std::string, HostInfo> HostMap;

    void timeWait();

    virtual void run();

    // 轮流检查ETCD主机的健康状况
    void checkHostHealth();

    // 检查ETCD主机是否可以正常读
    bool checkHostReadable(const std::string &url,
                           const std::string &key,
                           const std::string &val,
                           bool checkValue);

    // 检查ETCD主机是否可以正常写
    bool checkHostWritable(const std::string &url, const std::string &key, const std::string &val);

    // 从主机列表中加载ETCD主机信息
    int loadHostsInfo(const std::vector<std::string> &hosts);

private:
    bool _terminate;                        // 线程是否停止
    std::string _appName;                   // APP名称
    HostMap _configHosts;                   // 从配置文件中加载出的主机信息
    std::vector<std::string> _activeHosts;  // 存活的主机
    uint32_t _count;                        // 用于选择主机的计数

    // 多线程中对_activeHosts的读写锁
    static tars::TC_ThreadRWLocker _activeHostsLock;
};

#endif  // __ETCDHOST_H_
