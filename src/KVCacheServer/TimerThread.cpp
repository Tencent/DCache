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
#include "TimerThread.h"
#include "RouterHandle.h"
#include "CacheServer.h"
#include "ControlAck.h"

void TimerThread::init(const string &sConf)
{
    _config = sConf;
    _tcConf.parseFile(_config);

    _syncRouteInterval = TC_Common::strto<int>(_tcConf["/Main/Router<SyncInterval>"]);
    _binlogHeartbeatInterval = TC_Common::strto<int>(_tcConf.get("/Main/BinLog<HeartbeatInterval>", "600"));
    _binlogFile = _tcConf["/Main/BinLog<LogFile>"];
    _keyBinlogFile = _tcConf["/Main/BinLog<LogFile>"] + "key";
    _moduleName = _tcConf["/Main<ModuleName>"];
    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _shmNum = TC_Common::strto<unsigned int>(_tcConf.get("/Main/Cache<JmemNum>", "10"));

    _srp_dirtyCnt = Application::getCommunicator()->getStatReport()->createPropertyReport("CountOfDirtyRecords", PropertyReport::avg());
    _srp_hitcount = Application::getCommunicator()->getStatReport()->createPropertyReport("CacheHitRatio", PropertyReport::avg());

    _srp_memSize = Application::getCommunicator()->getStatReport()->createPropertyReport("CacheMemSize_MB", PropertyReport::avg());
    _srp_memInUse = Application::getCommunicator()->getStatReport()->createPropertyReport("DataMemUsage", PropertyReport::avg());
    _srp_dirtyRatio = Application::getCommunicator()->getStatReport()->createPropertyReport("ProportionOfDirtyRecords", PropertyReport::avg());
    _srp_chunksOnceEle = Application::getCommunicator()->getStatReport()->createPropertyReport("ChunkUsedPerRecord", PropertyReport::avg());
    _srp_elementCount = Application::getCommunicator()->getStatReport()->createPropertyReport("TotalCountOfRecords", PropertyReport::avg());
    _srp_onlykeyCount = Application::getCommunicator()->getStatReport()->createPropertyReport("CountOfOnlyKey", PropertyReport::avg());
    _srp_maxJmemUsage = Application::getCommunicator()->getStatReport()->createPropertyReport("MaxMemUsageOfJmem", PropertyReport::avg());
    if (_srp_dirtyCnt == 0 || _srp_hitcount == 0 || _srp_memSize == 0 || _srp_memInUse == 0 || _srp_dirtyRatio == 0 ||
        _srp_chunksOnceEle == 0 || _srp_elementCount == 0 || _srp_onlykeyCount == 0 || _srp_maxJmemUsage == 0)
    {
        TLOG_ERROR("TimerThread::init createPropertyReport error" << endl);
        assert(false);
    }

    _downgradeTimeout = TC_Common::strto<int>(_tcConf.get("/Main/Cache<DowngradeTimeout>", "30"));

    TLOG_DEBUG("TimerThread::init succ" << endl);
}

void TimerThread::reload()
{
    _tcConf.parseFile(_config);

    _syncRouteInterval = TC_Common::strto<int>(_tcConf["/Main/Router<SyncInterval>"]);
    _downgradeTimeout = TC_Common::strto<int>(_tcConf.get("/Main/Cache<DowngradeTimeout>", "30"));
    TLOG_DEBUG("TimerThread::reload succ" << endl);
}

void TimerThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (!_isStart)
    {
        _isStart = true;
        if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
        {
            throw runtime_error("Create TimerThread fail");
        }
    }
}

void* TimerThread::Run(void* arg)
{
    pthread_detach(pthread_self());
    TimerThread* pthis = (TimerThread*)arg;
    pthis->setRuning(true);

    time_t tLastRoute = 0;
    time_t tLastReport = 0;
    time_t tLastBinlogHeartbeat = 0;
    time_t tLastCheckConnectHb = TC_TimeProvider::getInstance()->getNow();
    uint64_t connectHbCount = RouterHandle::getInstance()->getSlaveHbCount();

    while (pthis->isStart())
    {
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        if (tNow - tLastRoute >= pthis->_syncRouteInterval)
        {
            if (g_app.gstat()->isSlaveCreating() == true)
            {
                pthis->_isInSlaveCreating = true;
                if (g_app.gstat()->serverType() == MASTER)
                {
                    ostringstream os;
                    os << "server changed from SLAVE to MASTER when in data creating status";
                    TARS_NOTIFY_ERROR(os.str());
                    TLOG_ERROR(os.str() << endl);
                }
            }
            else
            {
                pthis->_isInSlaveCreating = false;
                pthis->syncRoute();
                tLastRoute = tNow;
            }
        }
        if (tNow - tLastBinlogHeartbeat >= pthis->_binlogHeartbeatInterval)
        {
            TBinLogEncode logEncode;
            string sActive = logEncode.EncodeActive();
            sActive += "\n";

            if (pthis->_recordBinLog)
            {
                CacheServer::WriteToFile(sActive, pthis->_binlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
                
            if (pthis->_recordKeyBinLog)
            {
            
                CacheServer::WriteToFile(sActive, pthis->_keyBinlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
            tLastBinlogHeartbeat = tNow;
        }
        for (int i = 1; i <= g_app.gstat()->hitIndex(); i++)
        {
            HitCount count;
            g_app.gstat()->getHitCnt(i, count);
            if (tNow - count.t >= 60 && count.totalCount != 0)
            {
                pthis->_srp_hitcount->report((int)((float)count.hitCount / count.totalCount * 100));
                g_app.gstat()->resetHitCnt(i);
            }
        }
        if (tNow - tLastReport >= 60)
        {
            int biggest = 0;
            for (unsigned int i = 0; i < pthis->_shmNum; i++)
            {
                int tmp = 0;
                g_sHashMap.getUseRatio(i, tmp);
                if (tmp > biggest)
                    biggest = tmp;
            }
            pthis->_srp_maxJmemUsage->report(biggest);

            pthis->_srp_memSize->report((int)((float)g_sHashMap.getMemSize() / 1024 / 1024));
            pthis->_srp_memInUse->report((int)((float)g_sHashMap.getUsedDataMem() / g_sHashMap.getDataMemSize() * 100));
            if (g_sHashMap.size() == 0)
            {
                pthis->_srp_dirtyRatio->report(0);
                pthis->_srp_chunksOnceEle->report(0);
            }
            else
            {
                pthis->_srp_dirtyRatio->report((int)((float)g_sHashMap.dirtyCount() / g_sHashMap.size() * 100));
                pthis->_srp_chunksOnceEle->report((int)((float)g_sHashMap.usedChunkCount() / g_sHashMap.size()));
            }
            pthis->_srp_dirtyCnt->report(g_sHashMap.dirtyCount());
            pthis->_srp_elementCount->report(g_sHashMap.size() - g_sHashMap.onlyKeyCount());
            pthis->_srp_onlykeyCount->report(g_sHashMap.onlyKeyCount());
            tLastReport = tNow;
        }

        //check slave connect heartbeat
        if (pthis->_downgradeTimeout > 0 && (tNow - tLastCheckConnectHb) >= pthis->_downgradeTimeout)
        {
            if (g_app.gstat()->serverType() == MASTER)
            {
                uint64_t iCount = RouterHandle::getInstance()->getSlaveHbCount();
                if (iCount == connectHbCount)
                {
                    pthis->handleConnectHbTimeout();
                }

                connectHbCount = iCount;

            }

            tLastCheckConnectHb = tNow;
        }
        sleep(1);
    }
    pthis->setRuning(false);
    pthis->setStart(false);
    return NULL;
}

void TimerThread::syncRoute()
{

    RouterHandle::getInstance()->syncRoute();
}

size_t TimerThread::getDirtyNum()
{
    size_t iDirty = g_sHashMap.dirtyCount();
    return iDirty;
}

enum ServerType TimerThread::getType()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    GroupInfo group;
    g_route_table.getGroup(sServerName, group);

    if (group.masterServer == sServerName)
    {
        return MASTER;
    }
    return SLAVE;
}

void TimerThread::handleConnectHbTimeout()
{
    int ret = 0;
    try
    {
        //检测是否能正常访问router
        bool succ = false;
        string sRouteObj = _tcConf["/Main/Router<ObjName>"];
        RouterPrx routePrx;
        vector<TC_Endpoint> vtRouterEndpoint;
        int reTry = 2;
        while (reTry-- > 0)
        {
            try
            {
                vtRouterEndpoint = Application::getCommunicator()->getEndpoint4All(sRouteObj);
                break;
            }
            catch (const exception & ex)
            {
                TLOG_ERROR("TimerThread::" << __FUNCTION__ << " getEndpoint4All exception: " << ex.what() << endl);
            }
        }

        string serverName = ServerConfig::Application + "." + ServerConfig::ServerName;
        for (size_t i = 0; i < vtRouterEndpoint.size(); i++)
        {
            string sRouteProxy = sRouteObj + "@tcp -h " + vtRouterEndpoint[i].getHost() + " -p " + TC_Common::tostr(vtRouterEndpoint[i].getPort());
            reTry = 2;
            while (reTry-- > 0)
            {
                try
                {
                    routePrx = Application::getCommunicator()->stringToProxy<RouterPrx>(sRouteProxy);
                    ret = routePrx->heartBeatReport(_moduleName, g_app.gstat()->groupName(), serverName);
                    succ = true;
                    FDLOG("switch") << "TimerThread::" << __FUNCTION__ << " heartBeatReport succ to " << sRouteProxy << endl;
                    break;
                }
                catch (const exception & ex)
                {
                    TLOG_ERROR("TimerThread::" << __FUNCTION__ << " heartBeatReport exception again: " << ex.what() << endl);
                }
            }

            if (succ)
                break;
        }

        FDLOG("switch") << "TimerThread::handleConnectHbTimeout connect timeout, vtRouterEndpoint size:"
                        << vtRouterEndpoint.size() << "|invoke router:" << (succ ? "succ" : "fail") << endl;

        if (!succ)
        {
            ret = RouterHandle::getInstance()->masterDowngrade();
            if (ret != 0)
            {
                TLOG_ERROR("TimerThread::handleConnectHbTimeout masterDowngrade error:" << ret << endl);
                assert(false);
            }

            FDLOG("switch") << "TimerThread::handleConnectHbTimeout masterDowngrade succ." << endl;
        }
    }
    catch (exception &ex)
    {
        TLOG_ERROR("[TimerThread::handleConnectHbTimeout] exception:" << ex.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("[TimerThread::handleConnectHbTimeout] unknown exception." << endl);
    }
}


void CreateBinlogFileThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
    {
        throw runtime_error("Create CreateBinlogFileThread fail");
    }
}

void* CreateBinlogFileThread::Run(void* arg)
{
    TLOG_DEBUG("[CreateBinlogFileThread::Run] start!" << endl);

    pthread_detach(pthread_self());

    //读取配置文件
    TC_Config config;
    config.parseFile(ServerConfig::BasePath + "CacheServer.conf");

    string sBinlogFile = config["/Main/BinLog<LogFile>"];
    string sKeyBinlogFile = config["/Main/BinLog<LogFile>"] + "key";

    string sRecordBinLog = config.get("/Main/BinLog<Record>", "Y");
    bool bRecordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = config.get("/Main/BinLog<KeyRecord>", "N");
    bool bKeyRecordBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    //既不打普通binlog，也不打keybinlog，就直接退出
    if (!bRecordBinLog && !bKeyRecordBinLog)
        return NULL;

    string path = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + ServerConfig::Application + "." + ServerConfig::ServerName + "_";

    char str[128];
    struct tm ptr;
    time_t nowSec;
    string normalBinlogPath, keyBinlogPath;

    static string sTimeStr;
    while (!((CreateBinlogFileThread*)arg)->_stop)
    {
        memset(str, '\0', 128);
        nowSec = time(NULL);
        ptr = *(localtime(&nowSec));
        strftime(str, 80, "%Y%m%d%H", &ptr);
        string tmpStr = str;

        //时间跳到下一个小时了，就要加锁生成文件
        if (tmpStr != sTimeStr)
        {
            TLOG_DEBUG("[CreateBinlogFileThread::Run] create file!" << endl);

            if (bRecordBinLog)
            {
                normalBinlogPath = path + sBinlogFile + "_" + str + ".log";
                int ret = g_app.createBinLogFile(normalBinlogPath, false);
                assert(ret == 0);
            }

            if (bKeyRecordBinLog)
            {
                keyBinlogPath = path + sKeyBinlogFile + "_" + str + ".log";
                int ret = g_app.createBinLogFile(keyBinlogPath, true);
                assert(ret == 0);
            }

            sTimeStr = str;
        }

        sleep(1);
    }

    return NULL;
}

void HeartBeatThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
    {
        throw runtime_error("Create HeartBeatThread fail");
    }
}

void* HeartBeatThread::Run(void* arg)
{
    TLOG_DEBUG("[HeartBeatThread::Run] start!" << endl);

    pthread_detach(pthread_self());

    HeartBeatThread* pThis = (HeartBeatThread*)arg;
    //读取配置文件
    TC_Config config;
    config.parseFile(ServerConfig::BasePath + "CacheServer.conf");

    int interval = TC_Common::strto<int>(config.get("/Main<RouterHeartbeatInterval>", "1000"));

    string masterAddr = RouterHandle::getInstance()->getConnectHbAddr();
    ControlAckPrx connectPrx = NULL;
    if (masterAddr.length() > 0)
    {
        connectPrx = Application::getCommunicator()->stringToProxy<ControlAckPrx>(masterAddr);
        TLOG_DEBUG("HeartBeatThread::Run, connect hb master obj:" << masterAddr << endl);
    }

    while (!pThis->_stop)
    {
        //report heartBeat to Router.
        RouterHandle::getInstance()->heartBeat();

        //report connect heartbeat to master
        if (pThis->_enableConHb)
        {
            try
            {
                string tmpAddr = RouterHandle::getInstance()->getConnectHbAddr();
                if (!tmpAddr.empty() && masterAddr != tmpAddr)
                {
                    connectPrx = Application::getCommunicator()->stringToProxy<ControlAckPrx>(tmpAddr);
                    masterAddr = tmpAddr;
                    TLOG_DEBUG("HeartBeatThread::Run, connect hb master change to obj:" << masterAddr << endl);
                }

                if (connectPrx)
                    connectPrx->connectHb();
            }
            catch (const exception & ex)
            {
                TLOG_ERROR("HeartBeatThread::" << __FUNCTION__ << " connect hb exception: " << ex.what() << endl);
            }
        }

        usleep(interval * 1000);
    }

    return NULL;
}


