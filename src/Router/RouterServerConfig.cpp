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
#include "RouterServerConfig.h"

void RouterServerConfig::init(const string &fileName) { _conf.parseFile(fileName); }

std::string RouterServerConfig::getAppName(const std::string &defaultName) const
{
    return _conf.get("/Main<AppName>", defaultName);
}

std::vector<std::string> RouterServerConfig::getEtcdHosts(const std::string &defaultHost) const
{
    std::string s = _conf.get("/Main/ETCD<host>", defaultHost);
    return tars::TC_Common::sepstr<std::string>(s, ";");
}

bool RouterServerConfig::checkEnableEtcd() const
{
    string s = _conf.get("/Main/ETCD<enable>", "N");
    return (s == "Y" || s == "y");
}

int RouterServerConfig::getEtcdReqTimeout(int defaultTime) const
{
    return getConfig("/Main/ETCD<RequestTimeout>", defaultTime);
}

int RouterServerConfig::getETcdHeartBeatTTL(int defaultTime) const
{
    return getConfig("/Main/ETCD<EtcdHbTTL>", defaultTime);
}

int RouterServerConfig::getProxyMaxSilentTime(int defaultTime) const
{
    return getConfig("/Main/Transfer<ProxyMaxSilentTime>", defaultTime);
}

int64_t RouterServerConfig::getDbReloadTime(int64_t defaultTime) const
{
    return getConfig("/Main<DbReloadTime>", defaultTime);
}

std::map<std::string, std::string> RouterServerConfig::getDbConnInfo() const
{
    return _conf.getDomainMap("/Main/DB/conn");
}

std::map<std::string, std::string> RouterServerConfig::getDbRelationInfo() const
{
    return _conf.getDomainMap("/Main/DB/relation");
}

int RouterServerConfig::getTransferTimeout(int defaultTime) const
{
    return getConfig("/Main/Transfer<TransferTimeOut>", defaultTime);
}

int RouterServerConfig::getTransferDefragDbInterval(int defaultVal) const
{
    return getConfig("/Main/Transfer<TransferDefragDbInterval>", defaultVal);
}

int RouterServerConfig::getRetryTransferInterval(int defaultTime) const
{
    return getConfig("/Main/Transfer<RetryTransferInterval>", defaultTime);
}

int RouterServerConfig::getRetryTransferMaxTimes(int defaultVal) const
{
    return getConfig("/Main/Transfer<RetryTransferMaxTimes>", defaultVal);
}

int RouterServerConfig::getTransferPagesOnce(int defaultVal) const
{
    return getConfig("/Main/Transfer<TransferPagesOnce>", defaultVal);
}

int RouterServerConfig::getTransferInterval(int defaultTime) const
{
    return getConfig("/Main/Transfer<TransferInterval>", defaultTime);
}

int RouterServerConfig::getClearProxyInterval(int defaultTime) const
{
    return getConfig("/Main/Transfer<ClearProxyInterval>", defaultTime);
}

int RouterServerConfig::getTimerThreadSize(int defaultVal) const
{
    return getConfig("/Main/Transfer<TimerThreadSize>", defaultVal);
}

int RouterServerConfig::getMinTransferThreadEachGroup(int defaultVal) const
{
    return getConfig("/Main/Transfer<MinTransferThreadEachGroup>", defaultVal);
}

int RouterServerConfig::getMaxTransferThreadEachGroup(int defaultVal) const
{
    return getConfig("/Main/Transfer<MaxTransferThreadEachGroup>", defaultVal);
}

int RouterServerConfig::getSwitchCheckInterval(int defaultTime) const
{
    return getConfig("/Main/Switch<SwitchCheckInterval>", defaultTime);
}

int RouterServerConfig::getSwitchTimeOut(int defaultTime) const
{
    return getConfig("/Main/Switch<SwitchTimeOut>", defaultTime);
}

int RouterServerConfig::getSlaveTimeOut(int defaultTime) const
{
    return getConfig("/Main/Switch<SlaveTimeOut>", defaultTime);
}

std::string RouterServerConfig::getAdminRegObj(const std::string &defaultVal) const
{
    return _conf.get("/Main<AdminRegObj>", defaultVal);
}

int RouterServerConfig::getSwitchBinLogDiffLimit(int defaultTime) const
{
    return getConfig("/Main/Switch<SwitchBinLogDiffLimit>", defaultTime);
}

int RouterServerConfig::getSwitchMaxTimes(int defaultVal) const
{
    return getConfig("/Main/Switch<SwitchMaxTimes>", defaultVal);
}

int RouterServerConfig::getDowngradeTimeout(int defaultTime) const
{
    return getConfig("/Main/Switch<DowngradeTimeout>", defaultTime);
}
