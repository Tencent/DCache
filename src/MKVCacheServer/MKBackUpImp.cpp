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
#include "MKBackUpImp.h"
#include "MKCacheServer.h"


void MKBackUpImp::initialize()
{
    _config = ServerConfig::BasePath + "MKCacheServer.conf";
    _tcConf.parseFile(_config);

    TLOG_DEBUG("[BackUpImp::initialize] initialize config succ" << endl);
}

tars::Int32 MKBackUpImp::dump(const std::string &dumpPath, const std::string &mirrorName, std::string &errmsg, tars::TarsCurrentPtr current)
{
    try
    {
        int iRet = g_app.dumpThread()->init(dumpPath, mirrorName, _config);
        if (iRet != 0)
        {
            errmsg = "[BackUpImp::dump] dumpThread has started";
            TLOG_ERROR(errmsg << endl);
            return 0;
        }
        g_app.dumpThread()->start();
        return 0;
    }
    catch (const TarsException & ex)
    {
        errmsg = ex.what();
        TLOG_ERROR("[BackUpImp::dump] catch exception: " << errmsg << endl);
    }
    catch (const std::exception & ex)
    {
        errmsg = ex.what();
        TLOG_ERROR("[BackUpImp::dump] catch exception: " << errmsg << endl);
    }
    return -1;
}

tars::Int32 MKBackUpImp::restore(const std::string & mirrorPath, const vector<std::string> & binlogPath, tars::Int32 lastTime, tars::Bool normal, std::string &errmsg, tars::TarsCurrentPtr current)
{
    try
    {
        int iRet = g_app.slaveCreateThread()->init(mirrorPath, binlogPath, lastTime, normal, _config);
        if (iRet != 0)
        {
            errmsg = "[BackUpImp::restore] slave create thread has started";
            TLOG_ERROR(errmsg << endl);
            return -1;
        }
        g_app.slaveCreateThread()->createThread();
        return 0;
    }
    catch (const TarsException & ex)
    {
        errmsg = ex.what();
        TLOG_ERROR("[BackUpImp::restore] catch exception: " << errmsg << endl);
    }
    catch (const std::exception & ex)
    {
        errmsg = ex.what();
        TLOG_ERROR("[BackUpImp::restore] catch exception: " << errmsg << endl);
    }
    return -1;
}

