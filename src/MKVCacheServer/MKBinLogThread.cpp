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
#include "MKBinLogThread.h"
#include "MKCacheServer.h"

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
void MKBinLogThread::init(const string &sConf)
{
    _config = sConf;
    _tcConf.parseFile(_config);
    _maxSyncLine = TC_Common::strto<uint32_t>(_tcConf["/Main/BinLog<MaxLine>"]);
    _binlogFile = _tcConf["/Main/BinLog<LogFile>"];
    _keyBinlogFile = _tcConf["/Main/BinLog<LogFile>"] + "key";
    _insertAtHead = (_tcConf["/Main/Cache<InsertOrder>"] == "Y" || _tcConf["/Main/Cache<InsertOrder>"] == "y") ? true : false;
    string sUpdateOrder = _tcConf.get("/Main/Cache<UpdateOrder>", "N");
    _updateInOrder = (sUpdateOrder == "Y" || sUpdateOrder == "y") ? true : false;
    _isAferSwicth = false;
    string sSyncCompress = _tcConf.get("/Main/BinLog<SyncCompress>", "N");
    if (sSyncCompress == "Y" || sSyncCompress == "y")
    {
        _syncCompress = true;
    }
    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sIsKeySyncMode = _tcConf.get("/Main/BinLog<KeySyncMode>", "N");
    _isKeySyncMode = (sIsKeySyncMode == "Y" || sIsKeySyncMode == "y") ? true : false;

    _mkeyMaxDataCount = TC_Common::strto<size_t>(_tcConf.get("/Main/Cache<MkeyMaxDataCount>", "0"));

    _binlogQueueSize = TC_Common::strto<unsigned int>(_tcConf.get("/Main/BinLog<BuffSize>", "10"));

    bool bReadDb = (_tcConf["/Main/DbAccess<ReadDbFlag>"] == "Y" || _tcConf["/Main/DbAccess<ReadDbFlag>"] == "y") ? true : false;
    if (bReadDb && _mkeyMaxDataCount > 0)
    {
        TLOG_ERROR("MKBinLogThread::init error: dbflag is true and MkeyMaxDataCount>0 at the same time,,MkeyMaxDataCount's value is initialized to 0" << endl);
        //重新设置为无数量限制
        _mkeyMaxDataCount = 0;
    }

    _existDB = (_tcConf["/Main/DbAccess<DBFlag>"] == "Y" || _tcConf["/Main/DbAccess<DBFlag>"] == "y") ? true : false;

    //限制主key下最大限数据量功能开启时，如果有db，就不能删除脏数据
    if (_existDB)
        _deleteDirty = false;
    else
        _deleteDirty = true;

    TLOG_DEBUG("MKBinLogThread::init Succ" << endl);
}

void MKBinLogThread::reload()
{
    _tcConf.parseFile(_config);
    _maxSyncLine = TC_Common::strto<uint32_t>(_tcConf["/Main/BinLog<MaxLine>"]);
    _insertAtHead = (_tcConf["/Main/Cache<InsertOrder>"] == "Y" || _tcConf["/Main/Cache<InsertOrder>"] == "y") ? true : false;
    string sSyncCompress = _tcConf.get("/Main/BinLog<SyncCompress>", "N");
    if (sSyncCompress == "Y" || sSyncCompress == "y")
    {
        _syncCompress = true;
    }
    else
        _syncCompress = false;

    TLOG_DEBUG("MKBinLogThread::reload Succ" << endl);
}


void MKBinLogThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (!_isStart)
    {
        _isStart = true;
        if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
        {
            throw runtime_error("Create MKBinLogThread 1 fail");
        }

        if (pthread_create(&thread, NULL, WriteData, (void*)this) != 0)
        {
            throw runtime_error("Create MKBinLogThread 2 fail");
        }
    }
}

void* MKBinLogThread::Run(void* arg)
{
    pthread_detach(pthread_self());
    MKBinLogThread* pthis = (MKBinLogThread*)arg;
    pthis->setRuning(true);
    //初始化m_binLogPrx
    string sAddr = pthis->getBakSourceAddr();
    if (sAddr.length() > 0)
        pthis->_binLogPrx = Application::getCommunicator()->stringToProxy<MKBinLogPrx>(sAddr);
    bool firstrGetPoint = false;
    bool bHaveGetSynPoint = false;
    while (pthis->isStart())
    {
        try
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

                if (pthis->_syncBinlogFile.empty())
                {
                    TLOG_ERROR("[BinLogThread::Run] getSyncPiont failed, file sync_point.data may be destroyed" << endl);
                    assert(false);
                }
                bHaveGetSynPoint = true;
                firstrGetPoint = true;
            }

            string sTmpAddr = pthis->getBakSourceAddr();
            if (sTmpAddr != sAddr)
            {
                TLOG_DEBUG("MasterBinLogAddr changed from " << sAddr << " to " << sTmpAddr << endl);
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

                    TLOG_DEBUG("firstrGetPoint" << endl);
                    if (!pthis->_isKeySyncMode)
                    {
                        pthis->_syncBinlogFile = pthis->getServerName() + "_" + pthis->_binlogFile + "_" + pthis->_syncBinlogFile.substr(pthis->_syncBinlogFile.size() - 14, 14);
                    }
                    else
                    {
                        pthis->_syncBinlogFile = pthis->getServerName() + "_" + pthis->_binlogFile + "key_" + pthis->_syncBinlogFile.substr(pthis->_syncBinlogFile.size() - 14, 14);
                    }
                    pthis->_seek = 0;
                    TLOG_DEBUG("last type SLAVE:m_logfile" << pthis->_syncBinlogFile << endl);
                    pthis->saveSyncPiont(pthis->_syncBinlogFile, pthis->_seek);
                }
                sAddr = sTmpAddr;
                if (sAddr.length() > 0)
                    pthis->_binLogPrx = Application::getCommunicator()->stringToProxy<MKBinLogPrx>(sAddr);
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
            TLOG_ERROR("[MKBinLogThread::Run] exception: " << ex.what() << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            usleep(100000);
        }
        catch (...)
        {
            TLOG_ERROR("[MKBinLogThread::Run] unkown exception" << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            usleep(100000);
        }

    }
    pthis->setRuning(false);
    pthis->setStart(false);
    return NULL;
}

void* MKBinLogThread::WriteData(void* arg)
{
    pthread_detach(pthread_self());

    MKBinLogThread* pthis = (MKBinLogThread*)arg;

    TLOG_DEBUG("[MKBinLogThread::WriteData] running!" << endl);

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
                //TLOG_DEBUG("[MKBinLogThread::WriteData] queue empty!" << endl);
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
                    string sUncompress;
                    bool bGzipOk = StringUtil::gzipUncompress(tmpBinlogData->sLogContent.c_str(), tmpBinlogData->sLogContent.length(), sUncompress);
                    if (!bGzipOk)
                    {
                        TLOG_ERROR("[MKBinLogThread::WriteData] logContent gzip uncompress error" << endl);
                        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                        pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
                        continue;
                    }
                    if (!StringUtil::parseString(sUncompress, vLogContent))
                    {
                        TLOG_ERROR("[MKBinLogThread::WriteData] logContent string parse error" << endl);
                        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                        pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
                        continue;
                    }
                }
                else
                {
                    if (!StringUtil::parseString(tmpBinlogData->sLogContent, vLogContent))
                    {
                        TLOG_ERROR("[MKBinLogThread::WriteData] logContent string parse error" << endl);
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
            TLOG_ERROR("[MKBinLogThread::WriteData] exception: " << ex.what() << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            if (tmpBinlogData)
                pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
        }
        catch (...)
        {
            TLOG_ERROR("[MKBinLogThread::WriteData] unkown exception" << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            if (tmpBinlogData)
                pthis->resetBuff(tmpBinlogData->m_logFile, tmpBinlogData->m_seek);
        }
    }

    return NULL;
}

int MKBinLogThread::syncBinLog()
{
    stBinlogDataPtr tmpBinlogData = new stBinlogData();
    tmpBinlogData->iSyncTime = 0;
    {
        TC_ThreadLock::Lock lock(_lock);

        if (_binlogDataQueue.size() >= _binlogQueueSize)
        {
            TLOG_DEBUG("[MKBinLogThread::syncBinLog] queue is full!" << endl);
            return 0;
        }

        //如果是重置binlog，而且刚好是同步开始，就直接改成false就行
        if (_isBinlogReset)
            _isBinlogReset = false;

        tmpBinlogData->m_logFile = _syncBinlogFile;
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
        TLOG_ERROR("[MKBinLogThread::syncBinLog] getLog exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }

    MKBinLogTimeThread::UpdateLastBinLogTime(tmpBinlogData->iLastTime, tmpBinlogData->iSyncTime);

    if (tmpBinlogData->iRet != 0)
    {
        TLOG_ERROR("[MKBinLogThread::syncBinLog] getLog error, iRet = " << tmpBinlogData->iRet << endl);
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

            _syncBinlogFile = tmpBinlogData->curLogfile;
            _seek = tmpBinlogData->curSeek;
        }
    }

    return 1;
}

int MKBinLogThread::syncCompress()
{
    stBinlogDataPtr tmpBinlogData = new stBinlogData();
    tmpBinlogData->iSyncTime = 0;
    {
        TC_ThreadLock::Lock lock(_lock);

        if (_binlogDataQueue.size() >= _binlogQueueSize)
        {
            TLOG_DEBUG("[MKBinLogThread::syncCompress] queue is full!" << endl);
            return 0;
        }

        //如果是重置binlog，而且刚好是同步开始，就直接改成false就行
        if (_isBinlogReset)
            _isBinlogReset = false;

        tmpBinlogData->m_logFile = _syncBinlogFile;
        tmpBinlogData->m_seek = _seek;
    }

    static size_t iPacketSizeLimit = 8 * 1024 * 1024, iLogSize = iPacketSizeLimit, iLastLogSize = 0;
    static int iStepFactor = 1, iSyncCount = 0, iExCount = 0;

    BinLogReq req;
    req.logfile = tmpBinlogData->m_logFile;
    req.seek = tmpBinlogData->m_seek;
    req.lineCount = _maxSyncLine;
    req.logSize = iLogSize;

    BinLogCompPartRsp rsp;
    rsp.isPart = false;
    rsp.partNum = 0;
    rsp.isEnd = true;

    //固定超时时间为10秒
    _binLogPrx->tars_timeout(10000);

    try
    {
        tmpBinlogData->iRet = _binLogPrx->getLogCompressWithPart(req, rsp);

        tmpBinlogData->sLogContent.swap(rsp.data.compLog);
        tmpBinlogData->curLogfile = rsp.data.curLogfile;
        tmpBinlogData->curSeek = rsp.data.curSeek;
        tmpBinlogData->iSyncTime = rsp.data.syncTime;
        tmpBinlogData->iLastTime = rsp.data.lastTime;
    }
    catch (const TarsException & ex)
    {
        TLOG_ERROR("[MKBinLogThread::syncCompress] getLogCompress exception:" << ex.what() << endl);
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

    //校验返回的binlog同步点格式,目前发现个别因"网络问题"导致的binlog同步点错乱

    if (!checkSyncPoint(tmpBinlogData->curLogfile))
    {
        TLOG_ERROR("[MKBinLogThread::syncCompress] point error" << tmpBinlogData->curLogfile << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }

    //检测返回的数据是否分块
    if (rsp.isPart)
    {
        TLOG_ERROR("[MKBinLogThread::syncCompress] get log transfer with part!" << endl);
        int iRet;
        int getLogTimes = 5;//最大循环次数，避免死循环
        while ((false == rsp.isEnd) && (getLogTimes-- > 0))
        {
            rsp.data.compLog.clear();
            try
            {
                ++rsp.partNum;
                iRet = _binLogPrx->getLogCompressWithPart(req, rsp);
            }
            catch (const TarsException & ex)
            {
                TLOG_ERROR("[MKBinLogThread::syncCompress] getLogCompressWithPart exception:" << ex.what() << endl);
                return -1;
            }

            if (iRet < 0)
            {
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                return -1;
            }
            else
                tmpBinlogData->sLogContent += rsp.data.compLog;
        }

        if (getLogTimes <= 0)
        {
            TLOG_ERROR("[MKBinLogThread::syncCompress] getLogCompressWithPart run too much!" << endl);
            return -1;
        }
    }

    if (tmpBinlogData->iRet < 0)
    {
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }

    MKBinLogTimeThread::UpdateLastBinLogTime(tmpBinlogData->iLastTime, tmpBinlogData->iSyncTime);

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
            TLOG_ERROR("point error " << tmpBinlogData->curLogfile << " should be " << tmpMasterServerName << endl);
        }

        {
            TC_ThreadLock::Lock lock(_lock);

            //如果要重置同步点，就不能保存同步点
            if (!_isBinlogReset)
            {
                _syncBinlogFile = tmpBinlogData->curLogfile;
                _seek = tmpBinlogData->curSeek;
            }
        }
        return 0;
    }

    //最后插入队列
    {
        TC_ThreadLock::Lock lock(_lock);

        //如果要重置同步点，就不能保存同步点和数据了
        if (!_isBinlogReset)
        {
            _binlogDataQueue.push(tmpBinlogData);

            _syncBinlogFile = tmpBinlogData->curLogfile;
            _seek = tmpBinlogData->curSeek;
        }
    }

    return 1;
}

void MKBinLogThread::getSyncPiont(bool aferSwitch)
{
    TC_ThreadLock::Lock lock(_lock);

    if (!aferSwitch)
    {
        string sFile = ServerConfig::DataPath + "/sync_point.data";

        ifstream fin;
        fin.open(sFile.c_str(), ios::in | ios::binary);

        if (!fin)
        {
            TLOG_ERROR("open file: " << sFile << " error" << endl);
            _syncBinlogFile = getServerName() + "_" + _tcConf["/Main/BinLog<LogFile>"];
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
                _syncBinlogFile = getServerName() + "_" + _tcConf["/Main/BinLog<LogFile>"];
                _seek = 0;
                fin.close();
                return;
            }
            else
            {
                _syncBinlogFile = vt[0];
                _seek = TC_Common::strto<uint64_t>(vt[1]);
            }
        }
        else
        {
            _syncBinlogFile = "";
        }
        fin.close();
    }
    else
    {
        _syncBinlogFile = getServerName() + "_" + _tcConf["/Main/BinLog<LogFile>"];
        _seek = 0;
        string sFile = ServerConfig::DataPath + "/sync_point.data";
        int fd = open(sFile.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fd < 0)
        {
            //要加告警
            TLOG_ERROR("Save SyncPoint error, open " << sFile << " failed" << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return;
        }

        //在同步点文件后面加100个空格，用于覆盖旧的内容
        string sBlank(100, ' ');
        string line;
        line = _syncBinlogFile + "|" + TC_Common::tostr(_seek);
        line += sBlank;

        write(fd, line.c_str(), line.size());
        close(fd);

    }
}

void MKBinLogThread::saveSyncPiont(const string &logFile, const uint64_t seek)
{
    string sFile = ServerConfig::DataPath + "/sync_point.data";

    int fd = open(sFile.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    if (fd < 0)
    {
        TLOG_ERROR("Save SyncPoint error, open " << sFile << " failed" << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
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

string MKBinLogThread::getBakSourceAddr()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    ServerInfo server;
    int iRet = g_route_table.getBakSource(sServerName, server);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOG_ERROR("[MKBinLogThread::getBakSourceAddr] getBakSource error, iRet = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return "";
    }

    string sBakSourceAddr;
    sBakSourceAddr = server.BinLogServant;
    return sBakSourceAddr;
}

string MKBinLogThread::getServerName()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    ServerInfo server;
    int iRet = g_route_table.getBakSource(sServerName, server);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOG_ERROR("[MKBinLogThread::getServerName] getBakSource error, iRet = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return "";
    }

    return server.serverName;
}

void MKBinLogThread::wirteBinLog(const vector<string>& logContent)
{
    int iRet;
    ostringstream fileStreamStr;
    ostringstream keyFileStreamStr;

    for (size_t i = 0; i < logContent.size(); ++i)
    {
        MKBinLogEncode encode;
        encode.Decode(logContent[i]);

        if (encode.GetBinLogType() == BINLOG_ACTIVE)
            continue;

        string mk;
        string uk;
        string value;
        vector<MultiHashMap::Value> vs;
        vector<pair<uint32_t, string> > vtvalue;
        enum BinLogOpt binlogOpt = encode.GetOpt();

        switch (binlogOpt)
        {
        case BINLOG_SET:

            mk = encode.GetMK();
            uk = encode.GetUK();
            value = encode.GetValue();

            while (true)
            {
                //为了保证不带DB的cache 主key链的顺序跟主机保持一致
                bool updateOrder;
                if (_existDB)
                    updateOrder = true;
                else
                    updateOrder = _updateInOrder;

                iRet = g_HashMap.set(mk, uk, value, encode.GetExpireTime(), 0, TC_Multi_HashMap_Malloc::DELETE_FALSE, encode.GetDirty(), TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, updateOrder, _mkeyMaxDataCount, _deleteDirty);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_DEL:
        case BINLOG_ERASE:
            mk = encode.GetMK();
            uk = encode.GetUK();

            while (true)
            {
                if (binlogOpt == BINLOG_DEL)
                {
                    iRet = g_HashMap.delSetBit(mk, uk, time(NULL));

                    //带db并且没有数据，就插入一条onlykey
                    if (_existDB && (iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA))
                        iRet = g_HashMap.setForDel(mk, uk, time(NULL));
                }
                else
                    iRet = g_HashMap.erase(mk, uk);


                if (iRet != TC_Multi_HashMap_Malloc::RT_OK
                        && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY
                        && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                        && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_DEL_MK:
        case BINLOG_ERASE_MK:
            mk = encode.GetMK();

            while (true)
            {
                if (binlogOpt == BINLOG_DEL_MK)
                {
                    uint64_t delCount;
                    iRet = g_HashMap.delSetBit(mk, time(NULL), delCount);
                }
                else
                {
                    iRet = g_HashMap.erase(mk);

                    //把主key下的数据设为部分数据
                    g_HashMap.setFullData(mk, false);
                }


                if (iRet != TC_Multi_HashMap_Malloc::RT_OK
                        && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY
                        && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                        && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_SET_ONLYKEY:
            mk = encode.GetMK();
            g_HashMap.set(mk);
            break;
        case BINLOG_SET_MUTIL:
            mk = encode.GetMK();
            vs = encode.GetVs();
            while (true)
            {
                //为了同步binlog时 主key链下面的顺序 故填false
                int iRet = g_HashMap.set(vs, TC_Multi_HashMap_Malloc::AUTO_DATA, false, false, false);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    //严重错误
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKDbAccessCallback::wirteBinLog] set error, ret = " << iRet << endl);
                }
                else
                {
                    if (encode.GetFull())
                    {
                        g_HashMap.setFullData(mk, true);
                    }
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_SET_MUTIL_FROMDB:
            mk = encode.GetMK();
            vs = encode.GetVs();
            while (true)
            {
                //是从db查回来的，把删除位设为DELETE_AUTO
                for (unsigned int i = 0; i < vs.size(); i++)
                {
                    vs[i]._isDelete = TC_Multi_HashMap_Malloc::DELETE_AUTO;
                }
                int iRet = g_HashMap.set(vs, TC_Multi_HashMap_Malloc::AUTO_DATA, false, false, false, false);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    //严重错误
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] binLog_set_mutil_formdb set error, ret = " << iRet << endl);
                }
                else
                {
                    if (encode.GetFull())
                    {
                        g_HashMap.setFullData(mk, true);
                    }
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_PUSH_LIST:
            mk = encode.GetMK();
            vtvalue = encode.GetListValue();

            while (true)
            {
                iRet = g_HashMap.pushList(mk, vtvalue, encode.GetHead(), false, 0, 0);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_POP_LIST:
            mk = encode.GetMK();

            while (true)
            {
                string value;
                uint64_t delSize = 0;
                if (g_app.gstat()->isExpireEnabled())
                {
                    time_t second = MKBinLogEncode::GetTime(logContent[i]);
                    iRet = g_HashMap.trimList(mk, true, encode.GetHead(), false, 0, 0, second, value, delSize);
                }
                else
                    iRet = g_HashMap.trimList(mk, true, encode.GetHead(), false, 0, 0, 0, value, delSize);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_REPLACE_LIST:
            mk = encode.GetMK();
            vtvalue.clear();
            vtvalue.push_back(make_pair(encode.GetExpireTime(), encode.GetValue()));

            while (true)
            {
                if (g_app.gstat()->isExpireEnabled())
                {
                    time_t second = MKBinLogEncode::GetTime(logContent[i]);
                    iRet = g_HashMap.pushList(mk, vtvalue, false, true, encode.GetPos(), second);
                }
                else
                    iRet = g_HashMap.pushList(mk, vtvalue, false, true, encode.GetPos(), 0);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_TRIM_LIST:
            mk = encode.GetMK();

            while (true)
            {
                string value;
                uint64_t delSize = 0;
                if (g_app.gstat()->isExpireEnabled())
                {
                    time_t second = MKBinLogEncode::GetTime(logContent[i]);
                    iRet = g_HashMap.trimList(mk, false, false, true, encode.GetStart(), encode.GetEnd(), second, value, delSize);
                }
                else
                    iRet = g_HashMap.trimList(mk, false, false, true, encode.GetStart(), encode.GetEnd(), 0, value, delSize);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_REM_LIST:
            mk = encode.GetMK();

            while (true)
            {
                string value;
                uint64_t delSize = 0;
                if (g_app.gstat()->isExpireEnabled())
                {
                    time_t second = MKBinLogEncode::GetTime(logContent[i]);
                    iRet = g_HashMap.trimList(mk, false, encode.GetHead(), false, 0, encode.GetCount(), second, value, delSize);
                }
                else
                    iRet = g_HashMap.trimList(mk, false, encode.GetHead(), false, 0, encode.GetCount(), 0, value, delSize);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_ADD_SET:
            mk = encode.GetMK();
            value = encode.GetValue();
            while (true)
            {
                TLOG_DEBUG("[MKBinLogThread::wirteBinLog] " << encode.GetDirty() << endl);
                iRet = g_HashMap.addSet(mk, value, encode.GetExpireTime(), 0, encode.GetDirty(), TC_Multi_HashMap_Malloc::DELETE_FALSE);
                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_DEL_SET:
            mk = encode.GetMK();
            value = encode.GetValue();
            while (true)
            {
                iRet = g_HashMap.delSetSetBit(mk, value, time(NULL));
                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_DEL_SET_MK:
            mk = encode.GetMK();
            while (true)
            {
                iRet = g_HashMap.delSetSetBit(mk, time(NULL));
                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    g_HashMap.setFullData(mk, false);
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_ADD_ZSET:
            mk = encode.GetMK();
            value = encode.GetValue();
            while (true)
            {
                iRet = g_HashMap.addZSet(mk, value, encode.GetScore(), encode.GetExpireTime(), 0, encode.GetDirty(), false, TC_Multi_HashMap_Malloc::DELETE_FALSE);
                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_INC_SCORE_ZSET:
            mk = encode.GetMK();
            value = encode.GetValue();
            while (true)
            {
                iRet = g_HashMap.addZSet(mk, value, encode.GetScore(), encode.GetExpireTime(), 0, encode.GetDirty(), true, TC_Multi_HashMap_Malloc::DELETE_FALSE);
                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_DEL_ZSET:
            mk = encode.GetMK();
            value = encode.GetValue();
            while (true)
            {
                iRet = g_HashMap.delZSetSetBit(mk, value, time(NULL));
                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_DEL_ZSET_MK:
            mk = encode.GetMK();
            while (true)
            {
                iRet = g_HashMap.delZSetSetBit(mk, time(NULL));
                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    g_HashMap.setFullData(mk, false);
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_DEL_RANGE_ZSET:
            mk = encode.GetMK();
            while (true)
            {
                if (g_app.gstat()->isExpireEnabled())
                {
                    time_t second = MKBinLogEncode::GetTime(logContent[i]);
                    iRet = g_HashMap.delRangeZSetSetBit(mk, encode.GetScoreMin(), encode.GetScoreMax(), second, time(NULL));
                }
                else
                    iRet = g_HashMap.delRangeZSetSetBit(mk, encode.GetScoreMin(), encode.GetScoreMax(), 0, time(NULL));
                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_PUSH_LIST_MUTIL:
            mk = encode.GetMK();
            vs = encode.GetVs();
            while (true)
            {
                //为了同步binlog时 主key链下面的顺序 故填false
                int iRet = g_HashMap.pushList(mk, vs);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    //严重错误
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKDbAccessCallback::wirteBinLog] set error, ret = " << iRet << endl);
                }
                else
                {
                    if (encode.GetFull())
                    {
                        g_HashMap.setFullData(mk, true);
                    }
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_ADD_SET_MUTIL:
            mk = encode.GetMK();
            vs = encode.GetVs();
            while (true)
            {
                //为了同步binlog时 主key链下面的顺序 故填false
                int iRet = g_HashMap.addSet(mk, vs, encode.GetFull() ? TC_Multi_HashMap_Malloc::FULL_DATA : TC_Multi_HashMap_Malloc::AUTO_DATA);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    //严重错误
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKDbAccessCallback::wirteBinLog] set error, ret = " << iRet << endl);
                }
                else
                {
                    if (encode.GetFull())
                    {
                        g_HashMap.setFullData(mk, true);
                    }
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_ADD_SET_MUTIL_FROMDB:
            mk = encode.GetMK();
            vs = encode.GetVs();
            for (unsigned int i = 0; i < vs.size(); i++)
            {
                vs[i]._isDelete = TC_Multi_HashMap_Malloc::DELETE_AUTO;
            }
            while (true)
            {
                //为了同步binlog时 主key链下面的顺序 故填false
                int iRet = g_HashMap.addSet(mk, vs, encode.GetFull() ? TC_Multi_HashMap_Malloc::FULL_DATA : TC_Multi_HashMap_Malloc::AUTO_DATA);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    //严重错误
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKDbAccessCallback::wirteBinLog] set error, ret = " << iRet << endl);
                }
                else
                {
                    if (encode.GetFull())
                    {
                        g_HashMap.setFullData(mk, true);
                    }
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_ADD_ZSET_MUTIL:
            mk = encode.GetMK();
            vs = encode.GetVs();
            while (true)
            {
                //为了同步binlog时 主key链下面的顺序 故填false
                int iRet = g_HashMap.addZSet(mk, vs, encode.GetFull() ? TC_Multi_HashMap_Malloc::FULL_DATA : TC_Multi_HashMap_Malloc::AUTO_DATA);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    //严重错误
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKDbAccessCallback::wirteBinLog] set error, ret = " << iRet << endl);
                }
                else
                {
                    if (encode.GetFull())
                    {
                        g_HashMap.setFullData(mk, true);
                    }
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_ADD_ZSET_MUTIL_FROMDB:
            mk = encode.GetMK();
            vs = encode.GetVs();
            for (unsigned int i = 0; i < vs.size(); i++)
            {
                vs[i]._isDelete = TC_Multi_HashMap_Malloc::DELETE_AUTO;
            }
            while (true)
            {
                //为了同步binlog时 主key链下面的顺序 故填false
                int iRet = g_HashMap.addZSet(mk, vs, encode.GetFull() ? TC_Multi_HashMap_Malloc::FULL_DATA : TC_Multi_HashMap_Malloc::AUTO_DATA);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    //严重错误
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKDbAccessCallback::wirteBinLog] set error, ret = " << iRet << endl);
                }
                else
                {
                    if (encode.GetFull())
                    {
                        g_HashMap.setFullData(mk, true);
                    }
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_ADD_SET_ONLYKEY:
            mk = encode.GetMK();
            while (true)
            {
                //为了同步binlog时 主key链下面的顺序 故填false
                int iRet = g_HashMap.addSet(mk);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    //严重错误
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKDbAccessCallback::wirteBinLog] set error, ret = " << iRet << endl);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_ADD_ZSET_ONLYKEY:
            mk = encode.GetMK();
            while (true)
            {
                //为了同步binlog时 主key链下面的顺序 故填false
                int iRet = g_HashMap.addZSet(mk);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    //严重错误
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    TLOG_ERROR("[MKDbAccessCallback::wirteBinLog] set error, ret = " << iRet << endl);
                }
                else
                {
                    break;
                }
                sleep(1);
            }
            break;
        case BINLOG_UPDATE_ZSET:
            mk = encode.GetMK();
            value = encode.GetValue();

            while (true)
            {
                //先删除老数据
                iRet = g_HashMap.delZSetSetBit(mk, encode.GetOldValue(), time(NULL));
                if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("[MKBinLogThread::wirteBinLog] update zset error, key = " << mk << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                }
                else
                {
                    //设置过期时间
                    uint32_t _ExpireTime = 0;
                    if (encode.GetExpireTime() != 0)
                    {
                        _ExpireTime = encode.GetExpireTime();
                    }
                    else
                    {
                        _ExpireTime = encode.GetOldExpireTime();
                    }
                    //添加新数据
                    iRet = g_HashMap.addZSet(mk, value, encode.GetScore(), _ExpireTime, 0, encode.GetDirty(), false, TC_Multi_HashMap_Malloc::DELETE_FALSE);
                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("[MKBinLogThread::wirteBinLog] update zset error, key = " << mk << " iRet = " << iRet << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                    }
                    else
                    {
                        break;
                    }
                }
                sleep(1);
            }
            break;
        default:
            TLOG_ERROR("[MKBinLogThread::wirteBinLog] Binlog format error, opt error" << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            break;
        }

        if (_recordBinLog)
        {
            fileStreamStr << logContent[i] << endl;
        }
        if (_isKeySyncMode)
        {
            if (binlogOpt != BINLOG_SET)
            {
                keyFileStreamStr << logContent[i] << endl;
            }
            else
            {
                keyFileStreamStr << encode.EncodeSet(mk, uk) << endl;

            }
        }
    }

    if (_recordBinLog)
    {
        string outStrBinlog = fileStreamStr.str();
        if (!outStrBinlog.empty())
            WriteBinLog::writeToFile(fileStreamStr.str(), _binlogFile);
    }
    if (_isKeySyncMode)
    {
        string outStrKeyBinlog = keyFileStreamStr.str();
        if (!outStrKeyBinlog.empty())
            WriteBinLog::writeToFile(keyFileStreamStr.str(), _keyBinlogFile);
    }
}
bool MKBinLogThread::checkSyncPoint(const string & strSyncPoint)
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

void MKBinLogThread::resetBuff(const string &logFile, const uint64_t seek)
{
    TLOG_ERROR("[MKBinLogThread::resetBuff] reset" << endl);

    TC_ThreadLock::Lock lock(_lock);

    while (_binlogDataQueue.size() > 0)
    {
        _binlogDataQueue.pop();
    }

    _syncBinlogFile = logFile;
    _seek = seek;

    _isBinlogReset = true;
}

