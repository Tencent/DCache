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
#include "ExpireThread.h"
#include "CacheServer.h"

void ExpireThread::init(const string &sConf)
{
    _config = sConf;
    _tcConf.parseFile(_config);

    _expireInterval = TC_Common::strto<int>(_tcConf["/Main/Cache<ExpireInterval>"]);

    _existDB = (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y") ? true : false;
    if (_existDB && (_tcConf["/Main/Cache<ExpireDb>"] == "Y" || _tcConf["/Main/Cache<ExpireDb>"] == "y"))
        _delDB = true;
    else
        _delDB = false;

    _expireSpeed = TC_Common::strto<size_t>(_tcConf["/Main/Cache<ExpireSpeed>"]);

    TLOG_DEBUG("ExpireThread::init succ" << endl);
}

void ExpireThread::reload()
{
    _tcConf.parseFile(_config);

    _expireInterval = TC_Common::strto<int>(_tcConf["/Main/Cache<ExpireInterval>"]);

    _existDB = (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y") ? true : false;
    if (_existDB && (_tcConf["/Main/Cache<ExpireDb>"] == "Y" || _tcConf["/Main/Cache<ExpireDb>"] == "y"))
        _delDB = true;
    else
        _delDB = false;

    _expireSpeed = TC_Common::strto<size_t>(_tcConf["/Main/Cache<ExpireSpeed>"]);

    TLOG_DEBUG("ExpireThread::reload succ" << endl);
}

tars::Int32 ExpireThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (!_isRuning)
    {
        if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
        {
            TLOG_ERROR("Create ExpireThread fail" << endl);
            return -1;
        }
    }
    else
    {
        TLOG_DEBUG("ExpireThread is running, can not be create again" << endl);
        return -1;
    }

    return 0;
}

void* ExpireThread::Run(void* arg)
{
    pthread_detach(pthread_self());
    ExpireThread* pthis = (ExpireThread*)arg;
    pthis->setRuning(true);
    pthis->setStart(true);


    time_t tLastExpire = TC_TimeProvider::getInstance()->getNow();
    while (pthis->isStart())
    {
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        if (tNow - tLastExpire >= pthis->_expireInterval)
        {
            pthis->eraseData();
            tLastExpire = tNow;
        }

        sleep(1);
    }
    pthis->setRuning(false);
    return NULL;
}

void ExpireThread::eraseData()
{
    time_t tBegin = TC_TimeProvider::getInstance()->getNow();

    size_t iCount = 0;
    SHashMap::dcache_hash_iterator it = g_sHashMap.hashBegin();
    TLOG_DEBUG("expire data start" << endl);
    while (isStart() && it != g_sHashMap.hashEnd())
    {
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        if (_expireSpeed > 0 && tBegin == tNow && iCount >= _expireSpeed)
        {
            usleep(10000);
        }
        else
        {
            if (tBegin < tNow)
            {
                iCount = 0;
                tBegin = tNow;
            }
            vector<pair<string, string> > vExpireData;
            it->getExpire(tNow, vExpireData);
            ++it;
            for (size_t i = 0; i < vExpireData.size(); ++i)
            {
                try
                {
                    int iRet;
                    if (g_app.gstat()->serverType() == MASTER && _delDB)
                    {
                        iRet = g_sHashMap.delExpire(vExpireData[i].first);
                    }
                    else
                    {
                        iRet = g_sHashMap.erase(vExpireData[i].first);
                    }

                    ++iCount;

                    if (iRet == TC_HashMapMalloc::RT_OK)
                    {
                        g_app.ppReport(PPReport::SRP_EXPIRE_CNT, 1);
                    }
                    else
                    {
                        g_app.ppReport(PPReport::SRP_EX, 1);
                    }
                }
                catch (std::exception& ex)
                {
                    TLOG_ERROR("ExpireThread::eraseData exception: " << ex.what() << ", key = " << vExpireData[i].first << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
            }
        }
    }
    if (!isStart())
    {
        TLOG_DEBUG("ExpireThread by stop" << endl);
    }
    else
    {
        TLOG_DEBUG("expire data finish" << endl);
    }
}
