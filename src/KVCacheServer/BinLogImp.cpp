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
#include "BinLogImp.h"

void BinLogImp::initialize()
{
    _tcConf.parseFile(ServerConfig::BasePath + "CacheServer.conf");

    _binlogFile = ServerConfig::Application + "." + ServerConfig::ServerName + "_" + _tcConf["/Main/BinLog<LogFile>"];

    string sIsKeySyncMode = _tcConf.get("/Main/BinLog<KeySyncMode>", "N");
    _isKeySyncMode = (sIsKeySyncMode == "Y" || sIsKeySyncMode == "y") ? true : false;

    string sIsGzip = _tcConf.get("/Main/BinLog<IsGzip>", "Y");
    _isGzip = (sIsGzip == "Y" || sIsGzip == "y") ? true : false;
}

tars::Int32 BinLogImp::getLog(const DCache::BinLogReq &req, DCache::BinLogRsp &rsp, tars::TarsCurrentPtr current)
{
    string logfileOpt = req.logfile;
    int64_t seek = req.seek;

    rsp.lastTime = g_app.gstat()->getBinlogTimeLast();

    try
    {
        ifstream ifs;
        rsp.logContent.clear();


        rsp.curSeek = seek;
        rsp.curLogfile = logfileOpt;

        string sTmpCurLogfile, sFile;
        if (logfileOpt == _binlogFile)
        {
            time_t t = TC_TimeProvider::getInstance()->getNow();
            sTmpCurLogfile = logfileOpt + "_" + TC_Common::tm2str(t, "%Y%m%d%H") + ".log";

            sFile = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + sTmpCurLogfile;
            if (access(sFile.c_str(), F_OK) == 0)
            {
                ifs.open(sFile.c_str(), ios::in | ios::binary);
                if (!ifs)
                {
                    g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                    TLOGERROR("BinLogImp::getLog open binlog :" << sFile << " failed" << endl);
                    return -1;
                }
                seek = 0;
                rsp.curSeek = seek;
                rsp.curLogfile = sTmpCurLogfile;
            }
            else if (errno == ENOENT)
            {
                return 0;
            }
            else
            {
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                TLOGERROR("BinLogImp::getLog access binlog :" << sFile << " error :" << errno << endl);
                return -1;
            }
        }
        else if (getLogFilePrefix(logfileOpt) != _binlogFile)
        {
            TLOGERROR("BinLogImp::getLog logfile error :" << logfileOpt << endl);
            return -1;
        }
        else
        {
            sTmpCurLogfile = logfileOpt;

            string sFile = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + sTmpCurLogfile;
            ifs.open(sFile.c_str(), ios::in | ios::binary);

            if (!ifs)
            {
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                TLOGERROR("BinLogImp::getLog open binlog :" << sFile << " failed, begin find next logfile" << endl);
                if (findNextLogFile(sTmpCurLogfile, sTmpCurLogfile))
                {
                    sFile = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + sTmpCurLogfile;
                    ifs.open(sFile.c_str(), ios::in | ios::binary);
                    if (!ifs)
                    {
                        TLOGERROR("BinLogImp::getLog open binlog :" << sFile << " failed" << endl);
                        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                        return -1;
                    }
                    seek = 0;
                    rsp.curSeek = seek;
                    rsp.curLogfile = sTmpCurLogfile;
                }
                else
                {
                    TLOGERROR("BinLogImp::getLog can not find binlog" << endl);
                    //g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                    return -1;
                }
            }
        }

        ifs.seekg((uint64_t)seek, ios::beg);
        if (!ifs)
        {
            ifs.close();
            TLOGERROR("BinLogImp::getLog seek binlog :" << sFile << " failed" << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            return -1;
        }

        string sNextLogfile;

        bool bNextFile = findNextLogFile(sTmpCurLogfile, sNextLogfile);
        //TLOGDEBUG("BinLogImp::getLog Next log file: " << sNextLogfile << endl);

        rsp.logContent.reserve(req.lineCount);
        for (int i = 0; i < req.lineCount; i++)
        {
            string line;

            int iRet = TBinLogEncode::GetOneLineBinLog(ifs, line);
            if (0 == iRet)
            {
                rsp.logContent.push_back(line);
                rsp.curSeek = ifs.tellg();
            }
            else
            {
                break;
            }
        }

        int logContentSize = int(rsp.logContent.size());

        if (ifs.eof())
        {
            if (bNextFile)
            {
                rsp.curSeek = 0;
                rsp.curLogfile = sNextLogfile;
            }
        }

        if (logContentSize > 0)
        {
            for (int i = logContentSize - 1; i >= 0; i--)
            {
                int32_t iTmpTime = TBinLogEncode::GetTime(rsp.logContent[i]);
                if (iTmpTime != 0)
                {
                    rsp.syncTime = iTmpTime;
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(rsp.syncTime, 0);

                    break;
                }
                else
                {
                    TLOGERROR("BinLogImp::getLog, getTime error! sFile:" << sTmpCurLogfile << "|content:" << rsp.logContent[i] << endl);
                    FDLOG("binlogerror") << "BinLogImp::getLog, getTime error! sFile:" << sTmpCurLogfile << "|content:" << rsp.logContent[i] << endl;
                }
            }
        }

        ifs.close();
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("BinLogImp::getLog: exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("BinLogImp::getLog: unkown exception: " << endl);
        return -1;
    }
    return 0;
}

tars::Int32 BinLogImp::getLogCompress(const DCache::BinLogReq &req, DCache::BinLogRsp &rsp, tars::TarsCurrentPtr current)
{
    string logfileOpt = req.logfile;
    int64_t seek = req.seek;

    rsp.lastTime = g_app.gstat()->getBinlogTimeLast();

    string oStr;
    oStr.reserve(20971520);//预分配20M
    /*int64_t beginTime,endTime;
    beginTime = TC_TimeProvider::getInstance()->getNowMs();*/
    try
    {
        ifstream ifs;
        rsp.compLog.clear();

        rsp.curSeek = seek;
        rsp.curLogfile = logfileOpt;

        string sTmpCurLogfile, sFile;
        if (logfileOpt == _binlogFile)
        {
            time_t t = TC_TimeProvider::getInstance()->getNow();

            //有两种binlog文件 如果_isKeySyncMode为真则读只记录key的binglog
            if (_isKeySyncMode)
            {
                sTmpCurLogfile = logfileOpt + "key_" + TC_Common::tm2str(t, "%Y%m%d%H") + ".log";
            }
            else
            {
                sTmpCurLogfile = logfileOpt + "_" + TC_Common::tm2str(t, "%Y%m%d%H") + ".log";
            }

            sFile = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + sTmpCurLogfile;
            if (access(sFile.c_str(), F_OK) == 0)
            {
                ifs.open(sFile.c_str(), ios::in | ios::binary);
                if (!ifs)
                {
                    g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                    TLOGERROR("[BinLogImp::getLogCompress] open binlog :" << sFile << " failed" << endl);
                    return -1;
                }
                seek = 0;
                rsp.curSeek = seek;
                rsp.curLogfile = sTmpCurLogfile;
            }
            else if (errno == ENOENT)
            {
                return 0;
            }
            else
            {
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                TLOGERROR("[BinLogImp::getLogCompress] access binlog :" << sFile << " error :" << errno << endl);
                return -1;
            }
        }
        else if (getLogFilePrefix(logfileOpt) != _binlogFile)
        {
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            TLOGERROR("[BinLogImp::getLogCompress] logfile error :" << logfileOpt << endl);
            return -1;
        }
        else
        {
            if (_isKeySyncMode)
            {
                if (logfileOpt.find("key_") == string::npos)
                {
                    g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                    TLOGERROR("[BinLogImp::getLogCompress] binFileName error keySyncMode must have _key_ fileName:" << logfileOpt << endl);
                    return -1;
                }
            }
            sTmpCurLogfile = logfileOpt;
            string sFile = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + sTmpCurLogfile;
            ifs.open(sFile.c_str(), ios::in | ios::binary);

            if (!ifs)
            {
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                TLOGERROR("[BinLogImp::getLogCompress] open binlog :" << sFile << " failed, begin find next logfile" << endl);
                return -1;
            }
        }
        ifs.seekg((uint64_t)seek, ios::beg);
        if (!ifs)
        {
            ifs.close();
            TLOGERROR("[BinLogImp::getLogCompress] seek binlog :" << sFile << " failed" << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            return -1;
        }

        string sNextLogfile;

        bool bNextFile = findNextLogFile(sTmpCurLogfile, sNextLogfile);

        string line;
        uint32_t iCurSize = 0, iCurLineCount = 0;
        set<string> keySet;
        string lastTimeString("");
        //beginTime = TC_TimeProvider::getInstance()->getNowMs();
        while (true)
        {
            line.clear();
            int iRet = TBinLogEncode::GetOneLineBinLog(ifs, line);
            if (0 == iRet)
            {
                if (ifs.eof())
                {
                    break;
                }
                else
                {
                    //获取最后的同步时间
                    TBinLogEncode::GetTimeString(line, lastTimeString);

                    rsp.curSeek = ifs.tellg();
                    ++iCurLineCount;

                    if (!_isKeySyncMode)
                    {
                        uint32_t iSize = line.length();
                        iCurSize += iSize;
                        iSize = htonl(iSize);
                        oStr.append(string((const char*)&iSize, sizeof(uint32_t)));
                        oStr.append(line);
                    }
                    else
                    {
                        TBinLogEncode sEncode;
                        sEncode.DecodeKey(line);
                        enum BinLogType logType = sEncode.GetBinLogType();

                        if (BINLOG_ACTIVE == logType)
                        {
                            uint32_t iSize = line.length();
                            iCurSize += iSize;
                            iSize = htonl(iSize);
                            oStr.append(string((const char*)&iSize, sizeof(uint32_t)));
                            oStr.append(line);
                        }
                        else
                        {
                            string sKey = sEncode.GetStringKey();
                            set<string>::iterator itrSet = keySet.find(sKey);
                            if (itrSet != keySet.end())
                            {
                                continue;
                            }
                            string value;
                            uint32_t iSynTime, iExpireTime;
                            uint8_t iVersion;
                            bool bDirty;
                            string binlogLast;
                            int iRet = g_sHashMap.get(sKey, value, iSynTime, iExpireTime, iVersion, bDirty, false, -1);
                            if (iRet == TC_HashMapMalloc::RT_OK)
                            {
                                binlogLast = sEncode.Encode(BINLOG_SET, bDirty, sKey, value, iExpireTime);
                            }
                            else if (iRet == TC_HashMapMalloc::RT_ONLY_KEY)
                            {
                                binlogLast = sEncode.Encode(BINLOG_SET_ONLYKEY, true, sKey, value);
                            }
                            else if (iRet == TC_HashMapMalloc::RT_NO_DATA)
                            {
                                binlogLast = sEncode.Encode(BINLOG_ERASE, true, sKey, value);
                            }
                            else if (iRet == TC_HashMapMalloc::RT_DATA_EXPIRED)
                            {
                                //过期数据不需要同步
                                continue;
                            }
                            else
                            {
                                TLOGERROR("[BinLogImp::getLogCompress] g_sHashMap.get error iRet:" << iRet << " key:" << sKey << endl);
                                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                                return -1;
                            }
                            uint32_t iSize = binlogLast.length();
                            iCurSize += iSize;
                            iSize = htonl(iSize);
                            oStr.append(string((const char*)&iSize, sizeof(uint32_t)));
                            oStr.append(binlogLast);
                            keySet.insert(sKey);
                        }
                    }

                    if (iCurSize >= (uint32_t)req.logSize || iCurLineCount >= (uint32_t)req.lineCount)
                    {
                        TLOGDEBUG("logSzie:" << iCurSize << " lineCount:" << iCurLineCount << endl);
                        break;
                    }
                }
            }
            else
            {
                break;
            }
        }

        if (!lastTimeString.empty())
        {
            int32_t iTmpTime = TBinLogEncode::GetTime(lastTimeString);
            if (iTmpTime != 0)
            {
                rsp.syncTime = iTmpTime;
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(rsp.syncTime, 0);
            }
        }

        if (ifs.eof())
        {
            if (bNextFile)
            {
                rsp.curSeek = 0;
                rsp.curLogfile = sNextLogfile;
            }
        }
        //endTime = TC_TimeProvider::getInstance()->getNowMs();
        ifs.close();
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("BinLogImp::getLog: exception: " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("BinLogImp::getLog: unkown exception: " << endl);
        return -1;
    }
    int64_t beginTime = TC_TimeProvider::getInstance()->getNowMs();
    bool bGzipOk = false;

    if (_isGzip)
    {
        //大于1M就开启压缩
        if (oStr.length() > 1048576)
        {
            bGzipOk = StringUtil::gzipCompress(oStr.c_str(), oStr.length(), rsp.compLog);
            if (!bGzipOk)
                TLOGERROR("[BinLogImp::getLogCompress] gzip compress error" << endl);
            TLOGDEBUG("gzip after size:" << rsp.compLog.size() << endl);
        }
    }

    if (!bGzipOk)
    {
        rsp.compLog = oStr;
        return 1;
    }
    int64_t endTime = TC_TimeProvider::getInstance()->getNowMs();
    TLOGDEBUG("[BinLogImp::getLogCompress] before size:" << oStr.length() << " after size:" << rsp.compLog.size() << " gzip time" << endTime - beginTime << endl);

    return 0;
}

tars::Int32 BinLogImp::getLastBinLogTime(tars::UInt32& lastTime, tars::TarsCurrentPtr current)
{
    lastTime = g_app.gstat()->getBinlogTimeLast();

    return 0;
}

string BinLogImp::getLogFileTime(const std::string & logfile)
{
    if (_isKeySyncMode)
    {
        return logfile.substr(_binlogFile.length() + 4, 10);
    }
    else
    {
        return logfile.substr(_binlogFile.length() + 1, 10);
    }
}

string BinLogImp::getLogFilePrefix(const std::string & logfile)
{
    return logfile.substr(0, _binlogFile.length());
}

bool BinLogImp::findNextLogFile(const std::string & logfile, std::string &nextfile)
{
    time_t tNow = TC_TimeProvider::getInstance()->getNow();
    string sNow = TC_Common::tm2str(tNow, "%Y%m%d%H");

    string sLogfileTime = getLogFileTime(logfile);
    if (sLogfileTime.length() <= 0)
        return false;
    sLogfileTime = sLogfileTime + "0000";

    struct tm stTm;
    TC_Common::str2tm(sLogfileTime, "%Y%m%d%H%M%S", stTm);
    time_t t = mktime(&stTm);
    t = t + 3600;
    string sNextTime = TC_Common::tm2str(t, "%Y%m%d%H");

    while (sNow >= sNextTime)
    {
        string sNextLogfile;
        if (_isKeySyncMode)
        {
            sNextLogfile = _binlogFile + "key_" + sNextTime + ".log";
        }
        else
        {
            sNextLogfile = _binlogFile + "_" + sNextTime + ".log";
        }

        string sTmp = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + sNextLogfile;
        if (access(sTmp.c_str(), F_OK) == 0)
        {
            nextfile = sNextLogfile;
            return true;
        }
        t = t + 3600;
        sNextTime = TC_Common::tm2str(t, "%Y%m%d%H");
    }
    return false;
}


