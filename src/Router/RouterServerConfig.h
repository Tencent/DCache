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
// Router服务的配置文件统一接口。所有需要获取配置文件的地方都应该通过RouterServerConfig类来读取。

#ifndef __ROUTERSERVERCONFIG_H__
#define __ROUTERSERVERCONFIG_H__

#include <string>
#include <vector>
#include "util/tc_common.h"
#include "util/tc_config.h"

class RouterServerConfig
{
public:
    void init(const std::string &fileName);

    // 获取当前APP的名称
    virtual std::string getAppName(const std::string &defaultName) const;

    // 获取所有ETCD主机的IP地址列表
    virtual std::vector<std::string> getEtcdHosts(const std::string &defaultHost) const;

    // 检查是否开启了ETCD
    virtual bool checkEnableEtcd() const;

    // 检查是否开启自动Cache主备自动切换
    virtual bool checkEnableSwitch() const;

    // 获取ETCD的HTTP请求的超时时间(单位:秒)
    virtual int getEtcdReqTimeout(int defaultTime) const;

    // 获取ETCD心跳Key的超时时间(单位:秒)
    virtual int getETcdHeartBeatTTL(int defaultTime) const;

    // 获取Proxy最大的访问时间间隔(单位:秒)
    virtual int getProxyMaxSilentTime(int defaultTime) const;

    // 获取数据库重新加载路由的最小间隔时间(单位：微妙)
    virtual int64_t getDbReloadTime(int64_t defaultTime) const;

    // 获取迁移的超时时间
    virtual int getTransferTimeout(int defaultTime) const;

    // 获取对数据库和内存中的路由信息进行整理的最小值(单位：页数)
    virtual int getTransferDefragDbInterval(int defaultVal) const;

    // 获取迁移重试的间隔(单位：秒)
    virtual int getRetryTransferInterval(int defaultTime) const;

    // 获取迁移重试的最大次数
    virtual int getRetryTransferMaxTimes(int defaultVal) const;

    // 获取获取一次迁移的最大页数
    virtual int getTransferPagesOnce(int defaultVal) const;

    // 获取轮休迁移数据库的时间间隔(单位：秒)
    virtual int getTransferInterval(int defaultTime) const;

    // 获取清理代理的间隔时间(单位：秒)
    virtual int getClearProxyInterval(int defaultTime) const;

    // 获取TimerThread线程池大小
    virtual int getTimerThreadSize(int defaultVal) const;

    // 获取每个组的最小迁移线程数
    virtual int getMinTransferThreadEachGroup(int defaultVal) const;

    // 获取每个组的最大迁移线程数
    virtual int getMaxTransferThreadEachGroup(int defaultVal) const;

    // 获取检查自动切换的间隔时间(单位:秒)
    virtual int getSwitchCheckInterval(int defaultTime) const;

    // 获取发起自动切换的超时时间（单位：秒）
    virtual int getSwitchTimeOut(int defaultTime) const;

    // 获取备机不可读的超时时间（单位：秒）
    virtual int getSlaveTimeOut(int defaultTime) const;

    // 主备切换时，主备机binlog差异的阈值(单位:毫秒)
    virtual int getSwitchBinLogDiffLimit(int defaultTime) const;

    // 获取每天切换的最大次数
    virtual int getSwitchMaxTimes(int defaultVal) const;

    virtual std::string getAdminRegObj(const std::string &defaultVal) const;

    // 获取主备切换时，主机降级的等待时间(单位：秒)
    virtual int getDowngradeTimeout(int defaultTime) const;

    // 获取数据库连接信息
    virtual std::map<std::string, std::string> getDbConnInfo() const;

    virtual std::map<std::string, std::string> getDbRelationInfo() const;

private:
    tars::TC_Config _conf;  // 配置文件

private:
    template <typename T>
    T getConfig(std::string tagName, T defaultVal) const
    {
        string s = _conf.get(tagName, "");
        if (s == "")
        {
            return defaultVal;
        }
        T t = tars::TC_Common::strto<T>(s);
        return t > 0 ? t : defaultVal;
    }
};

#endif  // __ROUTERSERVERCONFIG_H__
