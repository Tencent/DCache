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
#ifndef _global_H_
#define _global_H_

#include <sys/time.h>
#include <iostream>
#include <map>
#include "Router.h"
#include "RouterClient.h"
#include "servant/Application.h"
#include "util/tc_common.h"
#include "util/tc_config.h"

using namespace tars;
using namespace std;
using namespace DCache;

#define __INT_MAX 0xffffffff
#define __PAGE_SIZE 10000
#define __ALL_PAGE_COUNT (__INT_MAX / __PAGE_SIZE + (__INT_MAX % __PAGE_SIZE > 0 ? 1 : 0))
#define DAY_ERROR FDLOG("error")
#define FILE_FUN __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << "|"

//过程控制
#define PROC_BEGIN \
    do             \
    {
#define PROC_END \
    }            \
    while (0)    \
        ;

#define RUNTIME_ERROR(__err)                                                                       \
    do                                                                                             \
    {                                                                                              \
        throw std::runtime_error(std::string(__FILE__) + ", " + std::string(__FUNCTION__) + ", " + \
                                 TC_Common::tostr<int>(__LINE__) + ": " + __err);                  \
    } while (0)

// 通用函数的缩写宏
#define S2I(s) TC_Common::strto<int>(s)
#define I2S(i) TC_Common::tostr<int>(i)
#define S2I64(s) TC_Common::strto<int64_t>(s)
#define SEPSTR(s1, s2) TC_Common::sepstr<string>(s1, s2)
#define RT2STR(i) etos((TransferReturn)i)

#endif
