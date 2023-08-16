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
#ifndef __CACHE_MODULE_H_
#define __CACHE_MODULE_H_

#include "util/tc_thread_rwlock.h"

using namespace tars;

struct CacheInfo
{
    string module_name;
    string app_name;
    string idc_area;
    string group_name;
    string server_status;
};

class CacheInfoList
{
public:
    void init(std::map<string, CacheInfo> &mapCacheInfo)
    {
        modules[0] = mapCacheInfo;
        _index = 0;
    }

    void reload(std::map<string, CacheInfo> &mapCacheInfo)
    {
        int iNew = _index == 1 ? 0 : 1;
        modules[iNew] = mapCacheInfo;

        TC_ThreadWLock wlock(_rwl);
        _index = iNew;
    }

    bool get(const std::string &cacheName, CacheInfo &cacheInfo)
    {
        TC_ThreadRLock rlock(_rwl);
        if(modules[_index].find(cacheName) != modules[_index].end())
        {
            cacheInfo = modules[_index][cacheName];
            return true;
        }
        return false;
    }

private:
    TC_ThreadRWLocker  _rwl;
    tars::Int32 _index;
    std::map<string, CacheInfo> modules[2];
};

#endif

