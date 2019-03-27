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
#include "BinLogThread.h"
#include "util/tc_common.h"
#include "CacheServer.h"
#include <sstream>
static bool isNum(const string & ss)
{
    for (size_t i = 0; i < ss.length(); i++)
    {
        char a = ss[i];
        if (a<'0' || a>'9')
        {
            return false;
        }
    }
    return true;
}
void BinLogThread::init(const string &sConf)
{
    _configFile = sConf;
    _tcConf.parseFile(_configFile);
    _maxSyncLine = TC_Common::strto<uint32_t>(_tcConf["/Main/BinLog<MaxLine>"]);
    _binLogFile = _tcConf["/Main/BinLog<LogFile>"];
    _keyBinLogFile = _tcConf["/Main/BinLog<LogFile>"] + "key";

    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _isRecordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sSyncCompress = _tcConf.get("/Main/BinLog<SyncCompress>", "N");
    _isAferSwicth = false;
    if (sSyncCompress == "Y" || sSyncCompress == "y")
    {
        _syncCompress = true;
    }
    string sIsKeySyncMode = _tcConf.get("/Main/BinLog<KeySyncMode>", "N");
    _isKeySyncMode = (sIsKeySyncMode == "Y" || sIsKeySyncMode == "y") ? true : false;

    string sIsGzip = _tcConf.get("/Main/BinLog<IsGzip>", "Y");
    _isGzip = (sIsGzip == "Y" || sIsGzip == "y") ? true : false;

    _binlogQueueSize = TC_Common::strto<unsigned int>(_tcConf.get("/Main/BinLog<BuffSize>", "10"));

    TLOGDEBUG("BinLogThread::init Succ" << endl);
}

void BinLogThread::reload()
{
    _tcConf.parseFile(_configFile);
    _maxSyncLine = TC_Common::strto<uint32_t>(_tcConf["/Main/BinLog<MaxLine>"]);
    string sSyncCompress = _tcConf.get("/Main/BinLog<SyncCompress>", "N");
    if (sSyncCompress == "Y" || sSyncCompress == "y")
    {
        _syncCompress = true;
    }
    else
        _syncCompress = false;
    string sIsKeySyncMode = _tcConf.get("/Main/BinLog<KeySyncMode>", "N");
    _isKeySyncMode = (sIsKeySyncMode == "Y" || sIsKeySyncMode == "y") ? true : false;

    string sIsGzip = _tcConf.get("/Main/BinLog<IsGzip>", "Y");
    _isGzip = (sIsGzip == "Y" || sIsGzip == "y") ? true : false;
    TLOGDEBUG("BinLogThread::reload Succ" << endl);
}


void BinLogThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (!_isStart)
    {
        _isStart = true;
        if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
        {
            throw runtime_error("Create BinLogThread 1 fail");
        }

        if (pthread_create(&thread, NULL, WriteData, (void*)this) != 0)
        {
            throw runtime_error("Create BinLogThread 2 fail");
        }
    }
}

void* BinLogThread::Run(void* arg)
{
    pthread_detach(pthread_self());
    BinLogThread* pthis = (BinLogThread*)arg;
    pthis->setRuning(true);
    //初始化m_binLogPrx
    string sAddr = pthis->getBakSourceAddr();
    if (sAddr.length() > 0)
        pthis->_binLogPrx = Application::getCommunicator()->stringToProxy<BinLogPrx>(sAddr);
    bool firstrGetPoint = false;
    bool bHaveGetSynPoint = false;
    while (pthis->isStart())
    {
        try
        {
            {
                firstrGetPoint = false;
                if (g_app.gstat()->isSlaveCreating() == true)
                {

                    bHaveGetSynPoint = false;
                    pthis->_isInSlaveCreating = true;
                    usleep(100000);
                    continue;
                }
                pthis->_isInSlaveCreating = false;
                if (g_app.gstat()->serverType() != SLAVE)
                {
                    pthis->_isAferSwicth = true;
                    sAddr = "";
                    bHaveGetSynPoint = false;
                    string sFile = ServerConfig::DataPath + "/sync_point.data";
                    remove(sFile.c_str());
                    usleep(100000);
                    continue;
                }
                if (!bHaveGetSynPoint)
                {
                    pthis->getSyncPiont(pthis->_isAferSwicth);
                    TLOGDEBUG("[BinLogThread::Run] afterSwitch:" << pthis->_isAferSwicth << "|getSyncPiont:"
                              << pthis->_logFile << "|" << pthis->_seek << endl);
                    if (pthis->_logFile.empty())
                    {
                        TLOGERROR("[BinLogThread::Run] getSyncPiont failed, file sync_point.data may be destroyed" << endl);
                        assert(false);
                    }
                    bHaveGetSynPoint = true;
                    firstrGetPoint = true;
                }

                string sTmpAddr = pthis->getBakSourceAddr();
                if (sTmpAddr != sAddr)
                {
                    TLOGDEBUG("MasterBinLogAddr111 changed from " << sAddr << " to " << sTmpAddr << endl);
                    //切合后 重新设置同步点
                    if (!firstrGetPoint)
                    {
                        //先等待缓存的数据写完
                        while (true)
                        {
                            unsigned int size = 0;
                            {
                                TC_ThreadLock::Lock lock(pthis->_lock);

                                size = pthis->_binlogDataQueue.size();
                            }

                            if (size > 0)
                                sleep(1);
                            else
                                break;
                        }

                        TC_ThreadLock::Lock lock(pthis->_lock);

                        TLOGDEBUG("firstrGetPoint" << endl);
                        if (!pthis->_isKeySyncMode)
                        {
                            pthis->_logFile = pthis->getServerName() + "_" + pthis->_binLogFile + "_" + pthis->_logFile.substr(pthis->_logFile.size() - 14, 14);
                        }
                        else
                        {
                            pthis->_logFile = pthis->getServerName() + "_" + pthis->_binLogFile + "key_" + pthis->_logFile.substr(pthis->_logFile.size() - 14, 14);
                        }
                        pthis->_seek = 0;
                        TLOGDEBUG("last type SLAVE:m_logfile" << pthis->_logFile << endl);
                        pthis->saveSyncPiont(pthis->_logFile, pthis->_seek);
                    }
                    sAddr = sTmpAddr;
                    if (sAddr.length() > 0)
                    {
                        pthis->_binLogPrx = Application::getCommunicator()->stringToProxy<BinLogPrx>(sAddr);
                        TLOGDEBUG("sAddr" << sAddr << endl);
                    }
                }
            }
            int iRet = 0;
            if (sAddr.length() > 0)
            {
                if (pthis->_syncCompress)
                {
                    iRet = pthis->syncCompress();
                }
                else
                    iRet = pthis->syncBinLog();
            }
            if (iRet <= 0)
                usleep(100000);
        }
        catch (const std::exception &ex)
        {
            TLOGERROR("[BinLogThread::Run] exception: " << ex.what() << endl);
            usleep(100000);
        }
        catch (...)
        {
            TLOGERROR("[BinLogThread::Run] unkown exception" << endl);
            usleep(100000);
        }

    }
    pthis->setRuning(false);
    pthis->setStart(false);
    return NULL;
}

void* BinLogThread::WriteData(void* arg)
{
    pthread_detach(pthread_self());

    BinLogThread* pthis = (BinLogThread*)arg;

    TLOGDEBUG("[BinLogThread::writeData] running!" << endl);

    stBinlogDataPtr tmpBinlogData = NULL;

    while (pthis->isStart())
    {
        try
        {
            bool found = false;

            {
                TC_ThreadLock::Lock lock(pthis->_lock);

                if (pthis->_binlogDataQueue.size() > 0)
                {
                    tmpBinlogData = pthis->_binlogDataQueue.front();
                    pthis->_binlogDataQueue.pop();
                    found = true;
                }
                else
                {
                    found = false;
                }
            }

            //找不到数据要写就等待一下
            if (!found)
            {
                usleep(100000);
                continue;
            }

            //压缩同步方式
            if (pthis->_syncCompress)
            {
                vector<string> vLogContent;
                vLogContent.reserve(pthis->_maxSyncLine);
                if (tmpBinlogData->iRet == 0)
                {
                    if (pthis->_isGzip)
                    {
                        string sUncompress;
                        bool bGzipOk = StringUtil::gzipUncompress(tmpBinlogData->sLogContent.c_str(), tmpBinlogData->sLogContent.length(), sUncompress);
                        if (!bGzipOk)
                        {
                            TLOGERROR("[BinLogThread::syncCompress] logContent gzip uncompress error" << endl);
                            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                            pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
                            continue;
                        }

                        if (!StringUtil::parseString(sUncompress, vLogContent))
                        {
                            TLOGERROR("[BinLogThread::syncCompress] logContent string parse error" << endl);
                            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                            pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
                            continue;
                        }
                    }
                    else
                    {
                        if (!StringUtil::parseString(tmpBinlogData->sLogContent, vLogContent))
                        {
                            TLOGERROR("[BinLogThread::syncCompress] logContent string parse error" << endl);
                            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                            pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
                            continue;
                        }
                    }
                }
                else
                {
                    if (!StringUtil::parseString(tmpBinlogData->sLogContent, vLogContent))
                    {
                        TLOGERROR("[BinLogThread::syncCompress] logContent string parse error" << endl);
                        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                        pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
                        continue;
                    }
                }
                pthis->wirteBinLog(vLogContent);

                pthis->saveSyncPiont(tmpBinlogData->curLogfile, tmpBinlogData->curSeek);
            }
            else
            {
                pthis->wirteBinLog(tmpBinlogData->vLogContent);

                pthis->saveSyncPiont(tmpBinlogData->curLogfile, tmpBinlogData->curSeek);
            }

        }
        catch (const std::exception &ex)
        {
            TLOGERROR("[BinLogThread::writeData] exception: " << ex.what() << endl);
            if (tmpBinlogData)
                pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
        }
        catch (...)
        {
            TLOGERROR("[BinLogThread::writeData] unkown exception" << endl);
            if (tmpBinlogData)
                pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
        }
    }

    return NULL;
}

int BinLogThread::syncBinLog()
{
    stBinlogDataPtr tmpBinlogData = new stBinlogData();
    tmpBinlogData->iSyncTime = 0;
    {
        TC_ThreadLock::Lock lock(_lock);

        if (_binlogDataQueue.size() >= _binlogQueueSize)
        {
            TLOGDEBUG("[BinLogThread::syncBinLog] queue is full! queue size():" << _binlogDataQueue.size() << endl);
            return 0;
        }

        //如果是重置binlog，而且刚好是同步开始，就直接改成false就行
        if (_isBinlogReset)
            _isBinlogReset = false;

        tmpBinlogData->m_logFile = _logFile;
        tmpBinlogData->m_seek = _seek;
    }

    try
    {
        BinLogReq req;
        req.logfile = tmpBinlogData->m_logFile;
        req.seek = tmpBinlogData->m_seek;
        req.lineCount = _maxSyncLine;

        BinLogRsp rsp;
        tmpBinlogData->iRet = _binLogPrx->getLog(req, rsp);

        tmpBinlogData->vLogContent.swap(rsp.logContent);
        tmpBinlogData->curLogfile = rsp.curLogfile;
        tmpBinlogData->curSeek = rsp.curSeek;
        tmpBinlogData->iSyncTime = rsp.syncTime;
        tmpBinlogData->iLastTime = rsp.lastTime;
    }
    catch (const TarsException & ex)
    {
        TLOGERROR("[BinLogThread::syncBinLog] getLog exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }

    BinLogTimeThread::UpdateLastBinLogTime(tmpBinlogData->iLastTime, tmpBinlogData->iSyncTime);

    if (tmpBinlogData->iRet != 0)
    {
        TLOGERROR("[BinLogThread::syncBinLog] getLog error, iRet = " << tmpBinlogData->iRet << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }

    {
        TC_ThreadLock::Lock lock(_lock);

        //如果要重置同步点，就不能保存同步点和数据了
        if (!_isBinlogReset)
        {
            if (tmpBinlogData->vLogContent.size() > 0)
                _binlogDataQueue.push(tmpBinlogData);

            _logFile = tmpBinlogData->curLogfile;
            _seek = tmpBinlogData->curSeek;
        }
    }

    return 1;
}

int BinLogThread::syncCompress()
{
    stBinlogDataPtr tmpBinlogData = new stBinlogData();
    tmpBinlogData->iSyncTime = 0;
    {
        TC_ThreadLock::Lock lock(_lock);

        if (_binlogDataQueue.size() >= _binlogQueueSize)
        {
            TLOGDEBUG("[BinLogThread::syncCompress] queue is full!queue size():" << _binlogDataQueue.size() << endl);
            return 0;
        }

        //如果是重置binlog，而且刚好是同步开始，就直接改成false就行
        if (_isBinlogReset)
            _isBinlogReset = false;

        tmpBinlogData->m_logFile = _logFile;
        tmpBinlogData->m_seek = _seek;
    }

    static size_t iPacketSizeLimit = 9 * 1024 * 1024, iLogSize = iPacketSizeLimit, iLastLogSize = 0;
    static int iStepFactor = 1, iSyncCount = 0, iExCount = 0;

    //固定超时时间为10秒
    _binLogPrx->tars_timeout(10000);

    try
    {
        BinLogReq req;
        req.logfile = tmpBinlogData->m_logFile;
        req.seek = tmpBinlogData->m_seek;
        req.lineCount = _maxSyncLine;
        req.logSize = iLogSize;

        BinLogRsp rsp;
        tmpBinlogData->iRet = _binLogPrx->getLogCompress(req, rsp);

        tmpBinlogData->sLogContent.swap(rsp.compLog);
        tmpBinlogData->curLogfile = rsp.curLogfile;
        tmpBinlogData->curSeek = rsp.curSeek;
        tmpBinlogData->iSyncTime = rsp.syncTime;
        tmpBinlogData->iLastTime = rsp.lastTime;
    }
    catch (const std::exception & ex)
    {
        TLOGERROR("[BinLogThread::syncCompress] getLogCompress exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        if (++iExCount >= 10)
        {
            if (iLastLogSize == 0)
            {
                iLogSize = iLogSize / 2;
                if (iLogSize < SYNC_ADDSIZE_STEP)
                {
                    iLogSize = SYNC_ADDSIZE_STEP;
                }
            }
            else
            {
                iLogSize = iLastLogSize;
                iLastLogSize = 0;
            }
            iExCount = 0;
            iSyncCount = 0;
            iStepFactor = 1;
        }
        else if (++iSyncCount >= 1000)
        {
            if (iLogSize < iPacketSizeLimit)
            {
                iLastLogSize = iLogSize;
                iLogSize = iLastLogSize + SYNC_ADDSIZE_STEP;
                iStepFactor = 1;
                if (iLogSize > iPacketSizeLimit)
                {
                    iLogSize = iPacketSizeLimit;
                }
            }
            else
            {
                iLastLogSize = 0;
            }
            iExCount = 0;
            iSyncCount = 0;
        }
        return -1;
    }

    if (tmpBinlogData->iRet < 0)
    {
        TLOGERROR("[BinLogThread::syncCompress] getLogCompress error, iRet = " << tmpBinlogData->iRet << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }
    BinLogTimeThread::UpdateLastBinLogTime(tmpBinlogData->iLastTime, tmpBinlogData->iSyncTime);

    //校验返回的binlog同步点格式,目前发现个别因"网络问题"导致的binlog同步点错乱
    if (!checkSyncPoint(tmpBinlogData->curLogfile))
    {
        TLOGERROR("[BinLogThread::syncCompress] point error" << tmpBinlogData->curLogfile << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }

    if (++iSyncCount >= 1000)
    {
        if (iExCount == 0)
        {
            if (iLogSize < iPacketSizeLimit)
            {
                iLastLogSize = iLogSize;
                iLogSize = iLastLogSize + iStepFactor*SYNC_ADDSIZE_STEP;
                iStepFactor *= 2;
                if (iLogSize > iPacketSizeLimit)
                {
                    iLogSize = iPacketSizeLimit;
                }
            }
            else
            {
                iLastLogSize = 0;
            }
        }
        else
        {
            if (iLogSize < iPacketSizeLimit)
            {
                iLastLogSize = iLogSize;
                iLogSize = iLastLogSize + SYNC_ADDSIZE_STEP;
                iStepFactor = 1;
                if (iLogSize > iPacketSizeLimit)
                {
                    iLogSize = iPacketSizeLimit;
                }
            }
            else
            {
                iLastLogSize = 0;
            }
        }
        iExCount = 0;
        iSyncCount = 0;
    }

    if (tmpBinlogData->sLogContent.length() <= 0)
    {
        string tmpMasterServerName = getServerName();
        if (tmpBinlogData->curLogfile.substr(0, tmpMasterServerName.length()) != tmpMasterServerName)
        {
            TLOGERROR("point error" << tmpBinlogData->curLogfile << " should be " << tmpMasterServerName << endl);
        }

        {
            TC_ThreadLock::Lock lock(_lock);

            //如果要重置同步点，就不能保存同步点
            if (!_isBinlogReset)
            {
                _logFile = tmpBinlogData->curLogfile;
                _seek = tmpBinlogData->curSeek;
            }
        }
        return 0;
    }

    {
        TC_ThreadLock::Lock lock(_lock);

        if (!_isBinlogReset)
        {
            _binlogDataQueue.push(tmpBinlogData);

            _logFile = tmpBinlogData->curLogfile;
            _seek = tmpBinlogData->curSeek;
        }
    }

    return 1;
}

void BinLogThread::getSyncPiont(bool aferSwitch)
{
    if (!aferSwitch)
    {
        string sFile = ServerConfig::DataPath + "/sync_point.data";

        ifstream fin;
        fin.open(sFile.c_str(), ios::in | ios::binary);

        if (!fin)
        {
            TLOGERROR("open file: " << sFile << " error" << endl);
            _logFile = getServerName() + "_" + _tcConf["/Main/BinLog<LogFile>"];
            _seek = 0;
            return;
        }

        string line;
        if (getline(fin, line))
        {
            vector<string> vt;
            vt = TC_Common::sepstr<string>(line, "|");
            if (vt.size() != 2)
            {
                _logFile = getServerName() + "_" + _tcConf["/Main/BinLog<LogFile>"];
                _seek = 0;
                fin.close();
                return;
            }
            else
            {
                _logFile = vt[0];
                _seek = TC_Common::strto<uint64_t>(vt[1]);
            }
        }
        else
        {
            _logFile = "";
        }
        fin.close();
    }
    else
    {
        _logFile = getServerName() + "_" + _tcConf["/Main/BinLog<LogFile>"];
        _seek = 0;
        string sFile = ServerConfig::DataPath + "/sync_point.data";
        int fd = open(sFile.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fd < 0)
        {
            //要加告警
            TLOGERROR("Save SyncPoint error, open " << sFile << " failed" << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return;
        }

        //在同步点文件后面加100个空格，用于覆盖旧的内容
        string sBlank(100, ' ');
        string line;
        line = _logFile + "|" + TC_Common::tostr(_seek);
        line += sBlank;

        write(fd, line.c_str(), line.size());
        close(fd);

    }
}

void BinLogThread::saveSyncPiont(const string &logFile, const uint64_t seek)
{
    string sFile = ServerConfig::DataPath + "/sync_point.data";

    int fd = open(sFile.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd < 0)
    {
        TLOGERROR("Save SyncPoint error, open " << sFile << " failed" << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return;
    }

    //在同步点文件后面加100个空格，用于覆盖旧的内容
    string sBlank(100, ' ');
    string line;
    line = logFile + "|" + TC_Common::tostr(seek);
    line += sBlank;

    write(fd, line.c_str(), line.size());
    close(fd);

}

string BinLogThread::getBakSourceAddr()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    ServerInfo server;
    int iRet = g_route_table.getBakSource(sServerName, server);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOGERROR("[BinLogThread::getBakSourceAddr] getBakSource error, iRet = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return "";
    }

    string sBakSourceAddr;
    sBakSourceAddr = server.BinLogServant;
    return sBakSourceAddr;
}

string BinLogThread::getServerName()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    ServerInfo server;
    int iRet = g_route_table.getBakSource(sServerName, server);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOGERROR("[BinLogThread::getServerName] getBakSource error, iRet = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return "";
    }

    return server.serverName;
}

void BinLogThread::wirteBinLog(const vector<string>& logContent)
{
    int iRet;
    ostringstream fileStreamStr;
    ostringstream keyFileStreamStr;
    for (size_t i = 0; i < logContent.size(); ++i)
    {
        TBinLogEncode sEncode;
        sEncode.Decode(logContent[i]);
        if (BINLOG_ACTIVE == sEncode.GetBinLogType())
            continue;

        string sKey;
        string sValue;
        enum BinLogOpt binlogOpt = sEncode.GetOpt();
        switch (binlogOpt)
        {
        case BINLOG_SET:
            sKey = sEncode.GetStringKey();
            sValue = sEncode.GetValue();
            while (true)
            {
                iRet = g_sHashMap.set(sKey, sValue, sEncode.GetDirty(), sEncode.GetExpireTime());
                if (iRet != TC_HashMapMalloc::RT_OK)
                {
                    TLOGERROR("[BinLogThread::wirteBinLog] map set error, key = " << sKey << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_SET_ONLYKEY:
            sKey = sEncode.GetStringKey();
            g_sHashMap.set(sKey, 1);
            break;
        case BINLOG_DEL:
            sKey = sEncode.GetStringKey();
            while (true)
            {
                iRet = g_sHashMap.del(sKey);
                if (iRet != TC_HashMapMalloc::RT_OK && iRet != TC_HashMapMalloc::RT_NO_DATA && iRet != TC_HashMapMalloc::RT_ONLY_KEY)
                {
                    TLOGERROR("[BinLogThread::wirteBinLog] map del error,  key = " << sKey << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_ERASE:
            sKey = sEncode.GetStringKey();
            while (true)
            {
                iRet = g_sHashMap.eraseByForce(sKey);
                if (iRet != TC_HashMapMalloc::RT_OK && iRet != TC_HashMapMalloc::RT_NO_DATA && iRet != TC_HashMapMalloc::RT_ONLY_KEY)
                {
                    TLOGERROR("[BinLogThread::wirteBinLog] map erase error, key = " << sKey << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        default:
            TLOGERROR("[BinLogThread::wirteBinLog] Binlog format error, opt error" << endl);
            break;
        }
        fileStreamStr << logContent[i] << endl;
        if (_isKeySyncMode)
        {
            if (binlogOpt != BINLOG_SET)
            {
                keyFileStreamStr << logContent[i] << endl;
            }
            else
            {
                keyFileStreamStr << sEncode.EncodeSetKey(sKey) << endl;
            }
        }
    }
    string outStrBinlog = fileStreamStr.str();
    if (_isRecordBinLog)
    {
        if (!outStrBinlog.empty())
            CacheServer::WriteToFile(outStrBinlog, _binLogFile);
    }
    if (_isKeySyncMode)
    {
        string outStrKeyBinlog = keyFileStreamStr.str();
        if (!outStrKeyBinlog.empty())
            CacheServer::WriteToFile(outStrKeyBinlog, _keyBinLogFile);
    }
}
bool BinLogThread::checkSyncPoint(const string & strSyncPoint)
{
    string masterServeName = getServerName();
    size_t iPos = 0;
    if (strSyncPoint.substr(0, masterServeName.length()) != masterServeName)
    {
        return false;
    }
    iPos += masterServeName.length();
    if (strSyncPoint.substr(iPos, 8) != "_binlog_")
    {
        if (strSyncPoint.substr(iPos, 11) != "_binlogkey_")
        {
            return false;
        }
        else
            iPos += 11;
    }
    else
        iPos += 8;
    if (!isNum(strSyncPoint.substr(iPos, 10)))
        return false;
    iPos += 10;
    if (strSyncPoint.substr(iPos, 4) != ".log")
        return false;
    return true;
}

void BinLogThread::resetBuff(const string &logFile, const uint64_t seek)
{
    TLOGERROR("[BinLogThread::resetBuff] reset" << endl);

    TC_ThreadLock::Lock lock(_lock);

    while (_binlogDataQueue.size() > 0)
    {
        _binlogDataQueue.pop();
    }

    _logFile = logFile;
    _seek = seek;

    _isBinlogReset = true;
}

