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
#ifndef _BACKUP_IMP_H_
#define _BACKUP_IMP_H_

#include "CacheGlobe.h"
#include "BackUp.h"
#include "servant/Application.h"
#include "DumpThread.h"
#include "SlaveCreateThread.h"

using namespace tars;
using namespace std;

class BackUpImp : public DCache::BackUp
{
public:

    BackUpImp() {}

    /**
     *
     */
    virtual ~BackUpImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy() {}

    /**
     *
     */
    tars::Int32 dump(const std::string &dumpPath, const std::string &mirrorName, std::string &errmsg, tars::TarsCurrentPtr current);

    tars::Int32 restore(const std::string & mirrorPath, const vector<std::string> & binlogpath, tars::Int32 lastTime, tars::Bool normal, std::string &errmsg, tars::TarsCurrentPtr current);

private:
    string _config;
    TC_Config _tcConf;

};

#endif

