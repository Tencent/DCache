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
#ifndef _COMMON_H_
#define _COMMON_H_

#include "util/tc_common.h"

#define SKEY_INT "int"
#define SKEY_LL "longlong"
#define SKEY_STRING "string"

//区分API类型
#define API_TYPE			"DCache::API::TYPE"		//key of map<string, string>  api的类型
#define DIRECT_CACHE_API	"directCacheAPI"		//value of map
#define GET_ROUTE			"DCache::getRoute"		//key of map 是否需要获取路由
#define VALUE_YES			"Y"						//value of map
#define API_IDC				"DCache::API::IDC"		//key of map, client的idc
#define ROUTER_UPDATED		"DCache::updatedRoute"	//key of map 路由

using namespace tars;

namespace DCache
{
    enum ServerType
    {
        MASTER = 0,
        SLAVE
    };

    struct BinLogSyncTime
    {
        unsigned int tSync;		//当前同步的时间点
        unsigned int tLast;		//最后一次BinLog时间点
    };

    struct HitCount
    {
        size_t totalCount;
        size_t hitCount;
        time_t t;
    };

    struct BinLogSyncInfo
    {
        unsigned int tLastSyn;   //当前已同步的时间点
        unsigned int tLastWrite; //最后一次写BinLog的时间点

        string sLastSynLog;
        uint64_t iLastSeek;

        string sNextSynLog;
        uint64_t iNextSeek;
    };

    inline string keyToString(string key, string type)
    {
        string sKey;

        if (type == SKEY_INT)
        {
            sKey = TC_Common::tostr(*(uint32_t *)(key.c_str()));
        }
        else if (type == SKEY_LL)
        {
            sKey = TC_Common::tostr(*(uint64_t *)(key.c_str()));
        }
        else
        {
            sKey = key;
        }
        return sKey;
    }
}

#endif
