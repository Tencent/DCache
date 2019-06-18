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
#include "MKBinLogTimeThread.h"
#include "MKCacheServer.h"

TC_ThreadLock MKBinLogTimeThread::_lock;

void MKBinLogTimeThread::init(const string& sConf)
{
    _config = sConf;
    _tcConf.parseFile(_config);

    _srp_binlogSynDiff = Application::getCommunicator()->getStatReport()->createPropertyReport("M/S_ReplicationLatency", PropertyReport::avg());
    if (_srp_binlogSynDiff == 0)
    {
        TLOGERROR("MKBinLogTimeThread::init createPropertyReport error" << endl);
        assert(false);
    }

    _saveSyncTimeInterval = TC_Common::strto<int>(_tcConf.get("/Main/BinLog<SaveSyncTimeInterval>", "30"));

    TLOGDEBUG("MKBinLogTimerThread::init succ" << endl);
}

void MKBinLogTimeThread::reload()
{
    _tcConf.parseFile(_config);
    _saveSyncTimeInterval = TC_Common::strto<int>(_tcConf.get("/Main/BinLog<SaveSyncTimeInterval>", "30"));

    TLOGDEBUG("MKBinLogTimeThread::reload Succ" << endl);
}

void MKBinLogTimeThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (!_isStart)
    {
        _isStart = true;
        if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
        {
            throw runtime_error("Create MKBinLogTimeThread fail");
        }
    }
}

void* MKBinLogTimeThread::Run(void* arg)
{
    pthread_detach(pthread_self());
    MKBinLogTimeThread* pthis = (MKBinLogTimeThread*)arg;
    pthis->setRuning(true);
    //初始化m_binLogPrx
    string sAddr = pthis->getBakSourceAddr();
    if (sAddr.length() > 0)
    {
        pthis->_binLogPrx = Application::getCommunicator()->stringToProxy<MKBinLogPrx>(sAddr);
    }

    //线程启动时从文件中获取g_binLogSyncTime.tSync和g_binLogSyncTime.tLast
    pthis->getSyncTime();

    //防止同步binlog线程还没有同步binlog 而上报同步差异
    //这个后面会改成tars线程的sleep模式
    usleep(3000000);

    time_t tLastSaveTime = 0;
    while (pthis->isStart())
    {
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        //主备机都保存内存中g_binLogSyncTime
        if (tNow - tLastSaveTime > pthis->_saveSyncTimeInterval)
        {
            pthis->saveSyncTime();
            tLastSaveTime = tNow;
        }

        if (g_app.gstat()->isSlaveCreating() == true)
        {
            pthis->_isInSlaveCreating = true;
            usleep(100000);
            continue;
        }
        pthis->_isInSlaveCreating = false;
        if (g_app.gstat()->serverType() != SLAVE)
        {
            pthis->reportBinLogDiff();
            usleep(100000);
            continue;
        }

        try
        {
            string sTmpAddr = pthis->getBakSourceAddr();
            if (sTmpAddr != sAddr)
            {
                TLOGDEBUG("MasterBinLogAddr changed from " << sAddr << " to " << sTmpAddr << endl);
                sAddr = sTmpAddr;
                if (sAddr.length() > 0)
                {
                    pthis->_binLogPrx = Application::getCommunicator()->stringToProxy<MKBinLogPrx>(sAddr);
                }
                else
                {
                    pthis->reportBinLogDiff();
                    usleep(100000);
                    continue;
                }
            }

            uint32_t tLastBinLog = 0;
            int iRet = pthis->_binLogPrx->getLastBinLogTime(tLastBinLog);
            if (iRet ==0)
                UpdateLastBinLogTime(tLastBinLog);

            pthis->_failCount = 0;
            pthis->reportBinLogDiff();

            usleep(500000);
            continue;
        }
        catch (const std::exception &ex)
        {
            TLOGERROR("[MKBinLogTimeThread::Run] exception: " << ex.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("[MKBinLogTimeThread::Run] unkown exception" << endl);
        }
        pthis->reportBinLogDiff();
        if (++pthis->_failCount >= 3)
        {
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            pthis->_failCount = 0;
            sleep(1);
        }
        else
            usleep(100000);
    }
    pthis->setRuning(false);
    pthis->setStart(false);
    return NULL;
}

void MKBinLogTimeThread::UpdateLastBinLogTime(uint32_t tLast, uint32_t tSync)
{
    TC_ThreadLock::Lock lock(_lock);

    g_app.gstat()->setBinlogTime(tSync, tLast);
}

string MKBinLogTimeThread::getBakSourceAddr()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    ServerInfo server;
    int iRet = g_route_table.getBakSource(sServerName, server);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOGERROR("[MKBinLogTimeThread::getBakSourceAddr] getBakSource error, iRet = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return "";
    }

    string sBakSourceAddr;
    sBakSourceAddr = server.BinLogServant;
    return sBakSourceAddr;
}

void MKBinLogTimeThread::reportBinLogDiff()
{
    static time_t tLastReport = 0;
    time_t tNow = TC_TimeProvider::getInstance()->getNow();
    if (tNow - tLastReport < 60)
    {
        return;
    }

    if (g_app.gstat()->getBinlogTimeSync() != 0)
    {
        _srp_binlogSynDiff->report(g_app.gstat()->getBinlogTimeLast() - g_app.gstat()->getBinlogTimeSync());
    }

    tLastReport = tNow;
}

void MKBinLogTimeThread::getSyncTime()
{
    string sFile = ServerConfig::DataPath + "/sync_time.data";

    ifstream fin;
    fin.open(sFile.c_str(), ios::in | ios::binary);
    if (!fin)
    {
        TLOGERROR("open file: " << sFile << " error" << endl);
        return;
    }

    string line;
    if (getline(fin, line))
    {
        vector<string> vt;
        vt = TC_Common::sepstr<string>(line, "|");
        if (vt.size() != 2)
        {
            TLOGERROR("sync_time.data is error" << endl);
            fin.close();
            return;
        }
        else
        {
            uint32_t tLast = TC_Common::strto<uint32_t>(vt[0]);
            uint32_t tSync = TC_Common::strto<uint32_t>(vt[1]);
            UpdateLastBinLogTime(tLast, tSync);
        }
    }
    else
    {
        TLOGERROR("sync_time.data is error" << endl);
    }
    fin.close();
}

void MKBinLogTimeThread::saveSyncTime()
{
    string sFile = ServerConfig::DataPath + "/sync_time.data";

    int fd = open(sFile.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd < 0)
    {
        TLOGERROR("Save SyncTime error, open " << sFile << " failed" << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }

    //在同步时间文件后面加10个空格，用于覆盖旧的内容
    string line;
    {
        TC_ThreadLock::Lock lock(_lock);
        string sBlank(10, ' ');
        line = TC_Common::tostr(g_app.gstat()->getBinlogTimeLast()) + "|" + TC_Common::tostr(g_app.gstat()->getBinlogTimeSync());
        line += sBlank;
    }

    write(fd, line.c_str(), line.size());
    close(fd);
}

