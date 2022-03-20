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
#include "CacheInfoUpdateThread.h"
#include "util/tc_config.h"
#include "CacheInfoManager.h"

CacheInfoUpdateThread::~CacheInfoUpdateThread()
{
    if (isAlive())
    {
        terminate();

        getThreadControl().join();
    }
}

void CacheInfoUpdateThread::terminate()
{
    TLOG_DEBUG("CacheInfoUpdateThread::terminate");

    _terminate = true;

    TC_ThreadLock::Lock lock(*this);

    notifyAll();
}

void CacheInfoUpdateThread::run()
{
    TLOG_DEBUG("CacheInfoUpdateThread::run once in " << _reloadInterval << " ms" << endl);
    
    while(!_terminate)
    {	
        {
            TC_ThreadLock::Lock lock(*this);
            timedWait(_reloadInterval * 1000);
        }

        CacheInfoManager::getInstance()->reload();
    }
}
