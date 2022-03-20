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
#include "ControlAckImp.h"
#include "RouterHandle.h"
#include "CacheServer.h"

void ControlAckImp::initialize()
{
    try
    {
        _tcConf.parseFile(ServerConfig::BasePath + "CacheServer.conf");
    }
    catch (exception &ex)
    {
        TLOG_ERROR("ControlAckImp::initialize exception:" << ex.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("ControlAckImp::initialize unknown exception." << endl);
    }

}

tars::Int32 ControlAckImp::connectHb(tars::TarsCurrentPtr current)
{
    if (g_app.gstat()->serverType() != MASTER)
    {
        TLOG_ERROR("ControlAckImp::connectHb, server is not master. maybe downgrade." << endl);
        return -1;
    }

    RouterHandle::getInstance()->incSlaveHbCount();

    return 0;
}


