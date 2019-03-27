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
#include "MKCacheServer.h"

void ExpireThread::init(const string &sConf)
{
    _config = sConf;
    _tcConf.parseFile(_config);

    _expireInterval = TC_Common::strto<int>(_tcConf["/Main/Cache<ExpireInterval>"]);

    _existDB = (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y") ? true : false;
    if ((_tcConf["/Main/Cache<ExpireDb>"] == "Y" || _tcConf["/Main/Cache<ExpireDb>"] == "y") && (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y"))
        _delDB = true;
    else
        _delDB = false;

    _expireSpeed = TC_Common::strto<int>(_tcConf["/Main/Cache<ExpireSpeed>"]);

    TLOGDEBUG("ExpireThread::init succ" << endl);
}

void ExpireThread::reload()
{
    _tcConf.parseFile(_config);

    _expireInterval = TC_Common::strto<int>(_tcConf["/Main/Cache<ExpireInterval>"]);

    if ((_tcConf["/Main/Cache<ExpireDb>"] == "Y" || _tcConf["/Main/Cache<ExpireDb>"] == "y") && (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y"))
        _delDB = true;
    else
        _delDB = false;

    _expireSpeed = TC_Common::strto<int>(_tcConf["/Main/Cache<ExpireSpeed>"]);

    TLOGDEBUG("ExpireThread::reload succ" << endl);
}

void ExpireThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (!_isStart)
    {
        _isStart = true;
        if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
        {
            throw runtime_error("Create ExpireThread fail");
        }
    }
}

void* ExpireThread::Run(void* arg)
{
    pthread_detach(pthread_self());
    ExpireThread* pthis = (ExpireThread*)arg;
    pthis->setRuning(true);

    TC_Multi_HashMap_Malloc::MainKey::KEYTYPE keyType;
    g_HashMap.getMainKeyType(keyType);

    time_t tLastExpire = TC_TimeProvider::getInstance()->getNow();
    while (pthis->isStart())
    {
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        if (tNow - tLastExpire >= pthis->_expireInterval)
        {
            if (TC_Multi_HashMap_Malloc::MainKey::LIST_TYPE != keyType)
                pthis->eraseData();
            else
                pthis->eraseListData();
            tLastExpire = tNow;
        }

        sleep(1);
    }
    pthis->setRuning(false);
    pthis->setStart(false);
    return NULL;
}

void ExpireThread::eraseData()
{
    time_t tBegin = TC_TimeProvider::getInstance()->getNow();

    TC_Multi_HashMap_Malloc::MainKey::KEYTYPE keyType;
    g_HashMap.getMainKeyType(keyType);

    size_t iCount = 0;
    MultiHashMap::hash_iterator it = g_HashMap.hashBegin();
    while (isStart() && it != g_HashMap.hashEnd())
    {
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        if (_expireSpeed > 0 && tBegin == tNow && iCount > _expireSpeed)
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

            if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == keyType)
            {
                vector<MultiHashMap::Expiretime> vv;
                it->getExpireTime(vv);
                ++it;

                for (size_t i = 0; i < vv.size(); i++)
                {
                    try
                    {
                        if (vv[i]._iExpireTime != 0 && tNow > vv[i]._iExpireTime)
                        {
                            int iRet;
                            if ((g_app.gstat()->serverType() == MASTER) && _delDB)
                            {
                                iRet = g_HashMap.delSetBit(vv[i]._mkey, vv[i]._ukey, time(NULL));
                            }
                            else
                            {
                                iRet = g_HashMap.erase(vv[i]._mkey, vv[i]._ukey);
                                if (_existDB && !_delDB)
                                {
                                    g_HashMap.setFullData(vv[i]._mkey, false);
                                }
                            }
                            iCount++;
                            if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                g_app.ppReport(PPReport::SRP_EXPIRE_CNT, 1);
                            }
                            else
                            {
                                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            }
                        }
                    }
                    catch (const std::exception &ex)
                    {
                        TLOGERROR("ExpireThread::eraseData exception: " << ex.what() << ", mkey = " << vv[i]._mkey << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                    }
                }
            }
            else
            {
                it->eraseExpireData(tNow);
                ++it;
            }
        }
    }
    if (!isStart())
    {
        TLOGDEBUG("ExpireThread by stop" << endl);
    }
    else
    {
        TLOGDEBUG("expire data finish" << endl);
    }
}

void ExpireThread::eraseListData()
{
    MultiHashMap::mk_hash_iterator it = g_HashMap.mHashBegin();
    while (it != g_HashMap.mHashEnd())
    {
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        it->eraseExpireData(tNow);

        ++it;

    }
}

