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
#include "policy_multi_hashmap_malloc.h"
#include "dcache_policy_multi_hashmap_malloc.h"
#include "CacheShare.h"
#include "UnpackTable.h"
#include "StringUtil.h"
#include "Common.h"

using namespace std;
using namespace tars;
using namespace DCache;

typedef MultiHashMapMallocDCache<SemLockPolicyDCache, ShmStorePolicyDCache> MultiHashMap;
#define CACHE_VER  "Shm_MKVCache_1.0.0"

extern MultiHashMap g_HashMap;
extern UnpackTable g_route_table;

struct FieldInfo
{
    uint8_t tag;
    string type;
    bool bRequire;
    string defValue;
    int lengthInDB;
};

struct FieldConf
{
    string sMKeyName;
    vector<string> vtUKeyName;
    vector<string> vtValueName;
    map<string, int> mpFieldType;//用于快速查找字段类型，0表示主key，1表示联合key，2表示value字段
    map<string, FieldInfo> mpFieldInfo;
};

struct Limit
{
    bool bLimit;
    size_t iIndex;
    size_t iCount;
};

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

    bool operator()(const string &mk)
    {
        TC_ThreadLock::Lock lock(_lock);
        uint32_t hash = g_route_table.hashKey(mk);
        for (size_t i = 0; i < _limitKeyRange.size(); ++i)
        {
            if (hash >= _limitKeyRange[i].lower && hash <= _limitKeyRange[i].upper)
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

    void setDirtyNum(size_t num);
    size_t dirtyNum();
    void setDelNum(size_t num);
    size_t delNum();
    time_t deleteTime();
    void setDeleteTime(time_t time);
    const FieldConf& fieldconfig();
    const FieldConf* fieldconfigPtr();
    void getFieldConfig(FieldConf& field);
    void setFieldConfig(const FieldConf& field);
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
    //用于记录脏数据个数和del数据个数
    size_t _dirtyNum;
    size_t _delNum;
    //主机删除del缓存数据时间点
    time_t _deleteTime;
    //数据结构配置信息
    FieldConf _fieldConf;

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
inline bool IsDigit(const string &key)
{
    string::const_iterator iter = key.begin();

    if (key.empty())
    {
        return false;
    }
    size_t length = key.size();
    size_t count = 0;
    //是否找到小数点
    bool bFindNode = false;
    bool bFirst = true;
    while (iter != key.end())
    {
        if (bFirst)
        {
            if (!::isdigit(*iter))
            {
                if (*iter != 45)
                {
                    return false;
                }
                else if (length == 1)
                {
                    return false;
                }

            }
        }
        else
        {
            if (!::isdigit(*iter))
            {
                if (*iter == 46)
                {
                    if ((!bFindNode) && (count != (length - 1)))
                    {
                        bFindNode = true;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
        }
        ++iter;
        count++;
        bFirst = false;
    }
    return true;
}
#endif

