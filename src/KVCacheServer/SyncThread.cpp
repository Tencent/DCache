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
#include "SyncThread.h"
#include "CacheServer.h"


void SyncThread::init(const string &sConf)
{
    _config = sConf;
    _tcConf.parseFile(_config);

    _syncDbInterval = TC_Common::strto<int>(_tcConf["/Main/Cache<SyncInterval>"]);
    _threadNum = TC_Common::strto<int>(_tcConf["/Main/Cache<SyncThreadNum>"]);
    _syncSpeed = TC_Common::strto<size_t>(_tcConf["/Main/Cache<SyncSpeed>"]);
    _syncCount = 0;
    _syncTime = 0;
    string sBlockTime = _tcConf.get("/Main/Cache<SyncBlockTime>", "0000-0000");

    vector<string> vTime = TC_Common::sepstr<string>(sBlockTime, ";", false);

    for (unsigned int i = 0; i < vTime.size(); i++)
    {
        vector<string> vTival = TC_Common::sepstr<string>(vTime[i], "-", false);
        if (vTival.size() != 2)
        {
            TLOGERROR("[SyncThread::init] block time error! | " << vTime[i] << endl);
            assert(false);
        }
        //检查获取的时间是否正确
        if ((vTival[0].size() != 4) || (vTival[1].size() != 4))
        {
            TLOGERROR("[SyncThread::init] block time error! | " << vTime[i] << endl);
            assert(false);
        }

        for (unsigned int j = 0; j < 4; j++)
        {
            if ((!isdigit(vTival[0][j])) || (!isdigit(vTival[1][j])))
            {
                TLOGERROR("[SyncThread::init] block time error! | " << vTime[i] << endl);
                assert(false);
            }
        }

        int iHour, iMin;
        sscanf(vTival[0].c_str(), "%2d%2d", &iHour, &iMin);
        time_t m_iSyncBlockBegin = iHour * 60 * 60 + iMin * 60;

        sscanf(vTival[1].c_str(), "%2d%2d", &iHour, &iMin);
        time_t m_iSyncBlockEnd = iHour * 60 * 60 + iMin * 60;

        if (m_iSyncBlockBegin > m_iSyncBlockEnd)
        {
            TLOGERROR("[SyncThread::init] block time error! | " << vTime[i] << endl);
            assert(false);
        }

        if (m_iSyncBlockBegin == m_iSyncBlockEnd)
            continue;

        _blockTime.push_back(make_pair(m_iSyncBlockBegin, m_iSyncBlockEnd));

        TLOGDEBUG("[SyncThread::init] " << m_iSyncBlockBegin << " " << m_iSyncBlockEnd << endl);
    }

    TLOGDEBUG("SyncThread::init succ" << endl);
}

void SyncThread::reload()
{
    _tcConf.parseFile(_config);

    _syncDbInterval = TC_Common::strto<int>(_tcConf["/Main/Cache<SyncInterval>"]);
    _threadNum = TC_Common::strto<int>(_tcConf["/Main/Cache<SyncThreadNum>"]);
    _syncSpeed = TC_Common::strto<size_t>(_tcConf["/Main/Cache<SyncSpeed>"]);

    TLOGDEBUG("SyncThread::reload succ" << endl);
}

void SyncThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (!_isStart)
    {
        _isStart = true;
        if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
        {
            throw runtime_error("Create SyncThread fail");
        }
    }
}

void* SyncThread::Run(void* arg)
{
    pthread_detach(pthread_self());
    SyncThread* pthis = (SyncThread*)arg;

    pthis->setRuning(true);

    CachePrx pMasterCachePrx;
    string sBakSourceAddr = "";

    time_t tLastDb = 0;
    while (pthis->isStart())
    {
        try
        {
            TC_ThreadPool twpool;
            twpool.init(pthis->getThreadNum());
            twpool.start();
//            TC_Functor<void, TL::TLMaker<time_t>::Result> cmd(pthis, &SyncThread::syncData);

            time_t tNow = TC_TimeProvider::getInstance()->getNow();
            if (tNow - tLastDb >= pthis->_syncDbInterval)
            {
                if (g_app.gstat()->serverType() == MASTER)
                {
                    pthis->sync();
//                    TC_Functor<void, TL::TLMaker<time_t>::Result>::wrapper_type fw(cmd, tNow);
                    for (size_t i = 0; i < twpool.getThreadNum(); i++)
                    {
                        twpool.exec(std::bind(&SyncThread::syncData, pthis, tNow));
                    }
                    twpool.waitForAllDone();
                    pthis->_syncTime = tNow;
                    TLOGDEBUG("SyncThread::Run, master sync data, t= " << TC_Common::tm2str(pthis->_syncTime) << endl);
                }
                else if (g_app.gstat()->serverType() == SLAVE)
                {
                    string sTmpCacheAddr = pthis->geBakSourceAddr();
                    if (sTmpCacheAddr.length() > 0)
                    {
                        if (sTmpCacheAddr != sBakSourceAddr)
                        {
                            TLOGDEBUG("MasterCacheAddr changed from " << sBakSourceAddr << " to " << sTmpCacheAddr << endl);
                            sBakSourceAddr = sTmpCacheAddr;
                            pMasterCachePrx = Application::getCommunicator()->stringToProxy<CachePrx>(sBakSourceAddr);
                        }
                        time_t tSync = pMasterCachePrx->getSyncTime();
                        pthis->_syncTime = tSync;

                        if (tSync > 60)
                        {
                            time_t tSyncSlave = tSync - 60;
                            pthis->sync();
//                            TC_Functor<void, TL::TLMaker<time_t>::Result>::wrapper_type fw(cmd, tSyncSlave);
                            for (size_t i = 0; i < twpool.getThreadNum(); i++)
                            {
                                twpool.exec(std::bind(&SyncThread::syncData, pthis, tSyncSlave));
                            }
                            twpool.waitForAllDone();
                        }

                        TLOGDEBUG("slave sync data, t= " << TC_Common::tm2str(tSync) << " - 60" << endl);
                    }
                }
                tLastDb = tNow;
            }
            sleep(1);
        }
        catch (const TarsException & ex)
        {
            TLOGERROR("SyncThread::Run: exception: " << ex.what() << endl);
            usleep(100000);
        }
        catch (const std::exception &ex)
        {
            TLOGERROR("SyncThread::Run: exception: " << ex.what() << endl);
            usleep(100000);
        }
        catch (...)
        {
            TLOGERROR("SyncThread::Run: unkown exception: " << endl);
            usleep(100000);
        }
    }
    pthis->setRuning(false);
    pthis->setStart(false);
    return NULL;
}

void SyncThread::sync()
{
    g_sHashMap.sync();
}

void SyncThread::syncData(time_t t)
{
    time_t tBegin = TC_TimeProvider::getInstance()->getNow();

    CanSync& canSync = g_app.gstat()->getCanSync();
    while (isStart())
    {
        int iRet;
        time_t tNow = TC_TimeProvider::getInstance()->getNow();

        //今天凌晨开始的秒数
        time_t nows = (tNow + 28800) % 86400;

        //检查是否屏蔽回写
        if (_blockTime.size() > 0)
        {
            vector<pair<time_t, time_t> >::iterator it = _blockTime.begin();
            while (it != _blockTime.end())
            {
                if (it->first <= nows && nows <= it->second)
                {
                    TLOGDEBUG("[SyncThread::syncData] block sync data! " << nows << endl);
                    sleep(30);
                    break;
                }
                it++;
            }

            if (it != _blockTime.end())
                continue;
        }

        if (_syncSpeed > 0 && tBegin == tNow && _syncCount > _syncSpeed)
        {
            usleep(10000);
        }
        else
        {
            if (tBegin < tNow)
            {
                _syncCount = 0;
                tBegin = tNow;
            }

            iRet = g_sHashMap.syncOnce(t, canSync);

            if (iRet == TC_HashMapMalloc::RT_OK)
            {
                break;
            }
            else if (iRet == TC_HashMapMalloc::RT_NEED_SYNC)
            {
                _syncCount++;
            }
            else if (iRet != TC_HashMapMalloc::RT_NONEED_SYNC && iRet != TC_HashMapMalloc::RT_ONLY_KEY)
            {
                TLOGERROR("SyncThread::syncData sync data error:" << iRet << endl);
                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                break;
            }
        }

    }
    if (!isStart())
    {
        TLOGDEBUG("SyncThread by stop" << endl);
    }
    else
    {
        TLOGDEBUG("syncData finish" << endl);
    }
}

string SyncThread::geBakSourceAddr()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    ServerInfo server;
    int iRet = g_route_table.getBakSource(sServerName, server);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOGERROR("SyncThread::geBakSourceAddr getBakSource error, iRet = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return "";
    }

    string sBakSourceAddr;
    sBakSourceAddr = server.CacheServant;
    return sBakSourceAddr;
}

