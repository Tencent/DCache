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
#ifndef __MOCK_ROUTER_SERVER_CONFIG_H__
#define __MOCK_ROUTER_SERVER_CONFIG_H__

#include "RouterServerConfig.h"

class MockRouterServerConfig : public RouterServerConfig
{
public:
    virtual std::map<std::string, std::string> getDbConnInfo() const override
    {
        return std::map<std::string, std::string>();
    }

    virtual std::map<std::string, std::string> getDbRelationInfo() const override
    {
        return std::map<std::string, std::string>();
    }

    // 获取当前APP的名称
    virtual std::string getAppName(const std::string &defaultName) const override
    {
        return "Dcache.unittest";
    }

    // 获取所有ETCD主机的IP地址列表
    virtual std::vector<std::string> getEtcdHosts(const std::string &defaultHost) const override
    {
        return tars::TC_Common::sepstr<std::string>(defaultHost, ";");
    }

    // 检查是否开启了ETCD
    virtual bool checkEnableEtcd() const { return true; }

    // 获取ETCD的HTTP请求的超时时间(单位:秒)
    virtual int getEtcdReqTimeout(int defaultTime) const override { return defaultTime; }

    // 获取ETCD心跳Key的超时时间(单位:秒)
    virtual int getETcdHeartBeatTTL(int defaultTime) const override { return defaultTime; }

    // 获取Proxy最大的访问时间间隔(单位:秒)
    virtual int getProxyMaxSilentTime(int defaultTime) const override { return defaultTime; }

    // 获取数据库重新加载路由的最小间隔时间(单位：微妙)
    virtual int64_t getDbReloadTime(int64_t defaultTime) const override { return defaultTime; }

    // 获取迁移的超时时间
    virtual int getTransferTimeout(int defaultTime) const override { return defaultTime; }

    // 获取对数据库和内存中的路由信息进行整理的最小值(单位：页数)
    virtual int getTransferDefragDbInterval(int defaultVal) const override { return defaultVal; }

    // 获取迁移重试的间隔(单位：秒)
    virtual int getRetryTransferInterval(int defaultTime) const override { return defaultTime; }

    // 获取迁移重试的最大次数
    virtual int getRetryTransferMaxTimes(int defaultVal) const override { return defaultVal; }

    // 获取获取一次迁移的最大页数
    virtual int getTransferPagesOnce(int defaultVal) const override { return defaultVal; }

    // 获取轮休迁移数据库的时间间隔(单位：秒)
    virtual int getTransferInterval(int defaultTime) const override { return defaultTime; }

    // 获取清理代理的间隔时间(单位：秒)
    virtual int getClearProxyInterval(int defaultTime) const override { return defaultTime; }

    // 获取TimerThread线程池大小
    virtual int getTimerThreadSize(int defaultVal) const override { return defaultVal; }

    // 获取每个组的最小迁移线程数
    virtual int getMinTransferThreadEachGroup(int defaultVal) const override { return defaultVal; }

    // 获取每个组的最大迁移线程数
    virtual int getMaxTransferThreadEachGroup(int defaultVal) const override { return defaultVal; }

    // 获取检查自动切换的间隔时间(单位:秒)
    virtual int getSwitchCheckInterval(int defaultTime) const override { return defaultTime; }

    // 获取发起自动切换的超时时间（单位：秒）
    virtual int getSwitchTimeOut(int defaultTime) const override { return defaultTime; }

    // 获取备机不可读的超时时间（单位：秒）
    virtual int getSlaveTimeOut(int defaultTime) const override { return defaultTime; }

    // 主备切换时，主备机binlog差异的阈值(单位:毫秒)
    virtual int getSwitchBinLogDiffLimit(int defaultTime) const override { return defaultTime; }

    // 获取每天切换的最大次数
    virtual int getSwitchMaxTimes(int defaultVal) const override { return defaultVal; }

    virtual std::string getAdminRegObj(const std::string &defaultVal) const override
    {
        return defaultVal;
    }

    // 获取主备切换时，主机降级的等待时间(单位：秒)
    virtual int getDowngradeTimeout(int defaultTime) const override { return defaultTime; }
};

#endif  // __MOCK_ROUTER_SERVER_CONFIG_H__