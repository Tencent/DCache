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
#ifndef _CACHEGLOBE_H
#define _CACHEGLOBE_H

#include "servant/Application.h"
#include "dcache_jmem_hashmap_malloc.h"
#include "CacheShare.h"
#include "UnpackTable.h"
#include "StringUtil.h"
#include "Common.h"

using namespace std;
using namespace tars;
using namespace DCache;

extern UnpackTable g_route_table;

typedef HashMapMallocDCache<SemLockPolicyDCache, ShmStorePolicyDCache> SHashMap;
#define CACHE_VER  "Shm_KVCache_1.0.0"

extern SHashMap g_sHashMap;

namespace DCache
{

class CanSync
{
public:
    CanSync() :_canSyncCount(0) {}
    ~CanSync() {}

    struct KeyLimit
    {
        unsigned int upper;
        unsigned int lower;
    };

    void Set(const vector<KeyLimit>& vtKeyLimit)
    {
        TC_ThreadLock::Lock lock(_lock);
        _limitKeyRange = vtKeyLimit;
    }

    bool operator()(const string &k)
    {
        TC_ThreadLock::Lock lock(_lock);
        size_t uCount = _limitKeyRange.size();
        if (uCount > 0)
        {
            unsigned int hashVal = hashCounter.HashRawString(k);
            for (size_t i = 0; i < uCount; ++i)
            {
                if (hashVal >= _limitKeyRange[i].lower && hashVal <= _limitKeyRange[i].upper)
                    return false;
            }
        }

        ++_canSyncCount;
        return true;
    }

    bool operator()(const struct SKey &k)
    {
        TC_ThreadLock::Lock lock(_lock);
        size_t uCount = _limitKeyRange.size();
        for (size_t i = 0; i < uCount; ++i)
        {
            if ((uint32_t)k.hash >= _limitKeyRange[i].lower && (uint32_t)k.hash <= _limitKeyRange[i].upper)
                return false;
        }

        ++_canSyncCount;
        return true;
    }

    size_t getCanSyncCount()
    {
        TC_ThreadLock::Lock lock(_lock);

        return _canSyncCount;
    }
private:
    NormalHash hashCounter;
    TC_ThreadLock _lock;
    vector<KeyLimit> _limitKeyRange;
    volatile size_t _canSyncCount;
};

//property report
class PPReport
{
public:
    enum PropertyType
    {
        SRP_EX,
        SRP_DB_EX,
        SRP_DB_ERR,
        SRP_CACHE_ERR,
        SRP_BINLOG_ERR,
        SRP_BAKCENTER_ERR,
        SRP_ERASE_CNT,
        SRP_ERASE_UNEXPIRE_CNT,
        SRP_GET_CNT,
        SRP_SET_CNT,
        SRP_COLD_RATIO,
        SRP_EXPIRE_CNT

    };

    int init();
    void report(enum PropertyType type, int value);
private:

    PropertyReportPtr _srp_ex;
    PropertyReportPtr _srp_dbex;
    PropertyReportPtr _srp_dberr;
    PropertyReportPtr _srp_cache_err;
    PropertyReportPtr _srp_binlogerr;
    PropertyReportPtr _srp_bakcenter_err;

    //淘汰线程淘汰计数
    PropertyReportPtr _srp_erasecount;
    //淘汰线程淘汰非过期数据的计数
    PropertyReportPtr _srp_erasecount_unexpire;
    //读请求计数，用于统计性能瓶颈
    PropertyReportPtr _srp_getCount;
    //写请求计数，用于统计性能瓶颈
    PropertyReportPtr _srp_setCount;
    //计算非热点数据比例
    PropertyReportPtr _srp_coldDataRatio;
    //过期淘汰数据计数
    PropertyReportPtr _srp_expirecount;
};

class GlobalStat
{
public:
    GlobalStat();
    ~GlobalStat();
    int init();
    enum DCache::ServerType serverType();
    void setServerType(enum DCache::ServerType type);
    const string& groupName();
    void setGroupName(string &name);
    unsigned int getBinlogTimeSync();
    unsigned int getBinlogTimeLast();
    void setBinlogTime(unsigned int sync, unsigned int last);
    int genHitIndex();
    int hitIndex();
    void tryHit(int index);
    void hit(int index);
    void getHitCnt(int index, HitCount &count);
    void resetHitCnt(int index);
    bool isSlaveCreating();
    void setSlaveCreating(bool stat);
    bool isExpireEnabled();
    void enableExpire(bool enable);
    CanSync& getCanSync();
private:
    enum DCache::ServerType _serverType;
    string _groupName;
    //BinLog同步时间点
    BinLogSyncTime _binLogSyncTime;
    //get命中率
    TC_Atomic _hitIndex;
    HitCount _hitCount[50];
    bool _slaveCreating;
    bool _enableExpire;
    CanSync _canSync;

};

typedef void(*sighandler_t)(int);
inline pid_t pox_system(const char *cmd_line)
{
    pid_t ret = 0;
    sighandler_t old_handler;
    old_handler = signal(SIGCHLD, SIG_DFL);
    ret = system(cmd_line);
    signal(SIGCHLD, old_handler);

    return ret;
}

}

#endif

