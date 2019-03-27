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
#include "MKBinLogImp.h"

//一次最大返回的大小，如果超过就分块
const size_t MAX_BLOCK_SIZE = 10000000 - 1000;

//分块的大小
const size_t TRANSFER_BLOCK_SIZE = 5 * 1024 * 1024;

void MKBinLogImp::initialize()
{
    _tcConf.parseFile(ServerConfig::BasePath + "MKCacheServer.conf");
    _binlogFile = ServerConfig::Application + "." + ServerConfig::ServerName + "_" + _tcConf["/Main/BinLog<LogFile>"];

    string sIsKeySyncMode = _tcConf.get("/Main/BinLog<KeySyncMode>", "N");
    _isKeySyncMode = (sIsKeySyncMode == "Y" || sIsKeySyncMode == "y") ? true : false;

    string sIsGzip = _tcConf.get("/Main/BinLog<IsGzip>", "Y");
    _isGzip = (sIsGzip == "Y" || sIsGzip == "y") ? true : false;

}

tars::Int32 MKBinLogImp::getLog(const DCache::BinLogReq &req, DCache::BinLogRsp &rsp, tars::TarsCurrentPtr current)
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
                    TLOGERROR("MKBinLogImp::getLog open binlog :" << sFile << " failed" << endl);
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
                TLOGERROR("MKBinLogImp::getLog access binlog :" << sFile << " error :" << errno << endl);
                return -1;
            }
        }
        else if (getLogFilePrefix(logfileOpt) != _binlogFile)
        {
            TLOGERROR("MKBinLogImp::getLog logfile error :" << logfileOpt << endl);
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
                TLOGERROR("MKBinLogImp::getLog open binlog :" << sFile << " failed, begin find next logfile" << endl);
                if (findNextLogFile(sTmpCurLogfile, sTmpCurLogfile))
                {
                    sFile = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + sTmpCurLogfile;
                    ifs.open(sFile.c_str(), ios::in | ios::binary);
                    if (!ifs)
                    {
                        TLOGERROR("MKBinLogImp::getLog open binlog :" << sFile << " failed" << endl);
                        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                        return -1;
                    }
                    seek = 0;
                    rsp.curSeek = seek;
                    rsp.curLogfile = sTmpCurLogfile;
                }
                else
                {
                    TLOGERROR("MKBinLogImp::getLog can not find binlog" << endl);
                    return -1;
                }
            }
        }

        ifs.seekg((uint64_t)seek, ios::beg);
        if (!ifs)
        {
            ifs.close();
            TLOGERROR("MKBinLogImp::getLog seek binlog :" << sFile << " failed" << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            return -1;
        }

        string sNextLogfile;

        bool bNextFile = findNextLogFile(sTmpCurLogfile, sNextLogfile);

        rsp.logContent.reserve(req.lineCount);
        for (int i = 0; i < req.lineCount; i++)
        {
            string line;
            int iRet = MKBinLogEncode::GetOneLineBinLog(ifs, line);
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


        if (ifs.eof())
        {
            if (bNextFile)
            {
                rsp.curSeek = 0;
                rsp.curLogfile = sNextLogfile;
            }
        }

        int logContentSize = int(rsp.logContent.size());
        if (logContentSize > 0)
        {
            for (int i = logContentSize - 1; i >= 0; i--)
            {
                int iTmpTime = MKBinLogEncode::GetTime(rsp.logContent[i]);
                if (iTmpTime != 0)
                {
                    rsp.syncTime = iTmpTime;
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(rsp.syncTime, 0);

                    break;
                }
            }
        }

        ifs.close();
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("MKBinLogImp::getLog: exception: " << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("MKBinLogImp::getLog: unkown exception: " << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }
    return 0;
}

tars::Int32 MKBinLogImp::getLogCompress(const DCache::BinLogReq &req, DCache::BinLogRsp &rsp, tars::TarsCurrentPtr current)
{
    string logfileOpt = req.logfile;
    int64_t seek = req.seek;
    rsp.lastTime = g_app.gstat()->getBinlogTimeLast();

    ostringstream os;
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
                    TLOGERROR("[MKBinLogImp::getLogCompress] open binlog :" << sFile << " failed" << endl);
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
                TLOGERROR("[MKBinLogImp::getLogCompress] access binlog :" << sFile << " error :" << errno << endl);
                return -1;
            }
        }
        else if (getLogFilePrefix(logfileOpt) != _binlogFile)
        {
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            TLOGERROR("[MKBinLogImp::getLogCompress] logfile error :" << logfileOpt << endl);
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
                TLOGERROR("[MKBinLogImp::getLogCompress] open binlog :" << sFile << " failed, begin find next logfile" << endl);
                return -1;
            }
        }
        ifs.seekg((uint64_t)seek, ios::beg);
        if (!ifs)
        {
            ifs.close();
            TLOGERROR("[MKBinLogImp::getLogCompress] seek binlog :" << sFile << " failed" << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            return -1;
        }

        string sNextLogfile;

        bool bNextFile = findNextLogFile(sTmpCurLogfile, sNextLogfile);
        string line;
        string lastTimeString("");
        uint32_t iCurSize = 0, iCurLineCount = 0;
        while (true)
        {
            line.clear();
            int iRet = MKBinLogEncode::GetOneLineBinLog(ifs, line);
            if (0 == iRet)
            {
                if (ifs.eof())
                {
                    break;
                }
                else
                {
                    //获取最后的同步时间
                    MKBinLogEncode::GetTimeString(line, lastTimeString);

                    rsp.curSeek = ifs.tellg();
                    ++iCurLineCount;
                    if (!_isKeySyncMode)
                    {
                        uint32_t iSize = line.length();
                        iCurSize += iSize;
                        iSize = htonl(iSize);
                        os.write((const char*)&iSize, sizeof(uint32_t));
                        os << line;
                    }
                    else
                    {
                        MKBinLogEncode sEncode;
                        bool bIsSetType = sEncode.DecodeKey(line);

                        if (BINLOG_ACTIVE == sEncode.GetBinLogType())
                        {
                            uint32_t iSize = line.length();
                            iCurSize += iSize;
                            iSize = htonl(iSize);
                            os.write((const char*)&iSize, sizeof(uint32_t));
                            os << line;
                        }
                        else
                        {
                            if (bIsSetType)
                            {
                                string mKey = sEncode.GetMK();
                                string uKey = sEncode.GetUK();
                                MultiHashMap::Value value;
                                uint32_t iSynTime, iExpireTime = 0;
                                uint8_t iVersion;
                                bool bDirty;
                                int iRet = g_HashMap.get(mKey, uKey, value, iSynTime, iExpireTime, iVersion, bDirty, false, -1);
                                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                                {
                                    line = sEncode.EncodeSet(mKey, uKey, value._value, iExpireTime, bDirty);
                                }
                                else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                                {
                                    line = sEncode.EncodeErase(mKey, uKey);
                                }
                                else
                                {
                                    TLOGERROR("[MKBinLogImp::getLogCompress] g_HashMap.get error iRet:" << iRet << " mkey:" << mKey << endl);
                                    g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                                    return -1;
                                }
                            }
                            uint32_t iSize = line.length();
                            iCurSize += iSize;
                            iSize = htonl(iSize);
                            os.write((const char*)&iSize, sizeof(uint32_t));
                            os << line;
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
            rsp.syncTime = MKBinLogEncode::GetTime(lastTimeString);
            if (g_app.gstat()->serverType() == MASTER)
                g_app.gstat()->setBinlogTime(rsp.syncTime, 0);
        }
        if (ifs.eof())
        {
            if (bNextFile)
            {
                rsp.curSeek = 0;
                rsp.curLogfile = sNextLogfile;
            }
        }

        ifs.close();
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("[MKBinLogImp::getLogCompress] exception:" << ex.what() << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("[MKBinLogImp::getLogCompress] unkown exception" << endl);
        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
        return -1;
    }

    bool bGzipOk = false;
    if (_isGzip)
    {
        //大于1M就开启压缩
        if (os.str().length() > 1048576)
        {
            bGzipOk = StringUtil::gzipCompress(os.str().c_str(), os.str().length(), rsp.compLog);
            if (!bGzipOk)
                TLOGERROR("[MKBinLogImp::getLogCompress] gzip compress error" << endl);
        }
    }

    if (getLogFilePrefix(rsp.curLogfile) != _binlogFile)
    {
        TLOGERROR("[BinLogImp::getLogCompress] return error:" << rsp.curLogfile << " should be:" << _binlogFile << endl);
    }

    if (!bGzipOk)
    {
        rsp.compLog = os.str();
        return 1;
    }

    return 0;
}

//用于临时保存同步的binlog数据
TC_ThreadLock g_BinLogLock;
map<string, string> g_mBinlogData;

tars::Int32 MKBinLogImp::getLogCompressWithPart(const DCache::BinLogReq &req, DCache::BinLogCompPartRsp &rsp, tars::TarsCurrentPtr current)
{
    int64_t seek = req.seek;
    BinLogRsp &rspData = rsp.data;
    // 标识是否压缩成功
    bool bGzipOk = false;

    if (!rsp.isPart || !_isKeySyncMode)
    {
        string logfileOpt = req.logfile;
        rspData.lastTime = g_app.gstat()->getBinlogTimeLast();
        string oStr;
        oStr.reserve(20971520);//预分配20M
        try
        {
            ifstream ifs;
            rspData.compLog.clear();

            rspData.curSeek = seek;
            rspData.curLogfile = logfileOpt;

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
                        TLOGERROR("[MKBinLogImp::getLogCompressWithPart] open binlog :" << sFile << " failed" << endl);
                        return -1;
                    }
                    seek = 0;
                    rspData.curSeek = seek;
                    rspData.curLogfile = sTmpCurLogfile;
                }
                else if (errno == ENOENT)
                {
                    return 0;
                }
                else
                {
                    g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                    TLOGERROR("[MKBinLogImp::getLogCompressWithPart] access binlog :" << sFile << " error :" << errno << endl);
                    return -1;
                }
            }
            else if (getLogFilePrefix(logfileOpt) != _binlogFile)
            {
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                TLOGERROR("[MKBinLogImp::getLogCompressWithPart] logfile error :" << logfileOpt << endl);
                return -1;
            }
            else
            {
                if (_isKeySyncMode)
                {
                    if (logfileOpt.find("key_") == string::npos)
                    {
                        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                        TLOGERROR("[MKBinLogImp::getLogCompressWithPart] binFileName error keySyncMode must have _key_ fileName:" << logfileOpt << endl);
                        return -1;
                    }
                }

                sTmpCurLogfile = logfileOpt;

                string sFile = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + sTmpCurLogfile;
                ifs.open(sFile.c_str(), ios::in | ios::binary);

                if (!ifs)
                {
                    g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                    TLOGERROR("[MKBinLogImp::getLogCompressWithPart] open binlog :" << sFile << " failed, begin find next logfile" << endl);
                    return -1;
                }
            }

            ifs.seekg((uint64_t)seek, ios::beg);
            if (!ifs)
            {
                ifs.close();
                TLOGERROR("[MKBinLogImp::getLogCompressWithPart] seek binlog :" << sFile << " failed" << endl);
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                return -1;
            }

            string sNextLogfile;

            bool bNextFile = findNextLogFile(sTmpCurLogfile, sNextLogfile);
            string line;
            string lastTimeString("");
            uint32_t iCurSize = 0, iCurLineCount = 0;
            while (true)
            {
                line.clear();
                int iRet = MKBinLogEncode::GetOneLineBinLog(ifs, line);
                if (0 == iRet)
                {
                    if (ifs.eof())
                    {
                        break;
                    }
                    else
                    {
                        //获取最后的同步时间
                        MKBinLogEncode::GetTimeString(line, lastTimeString);

                        rspData.curSeek = ifs.tellg();
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
                            //这里Active也同步 是为了保证备份中心每个小时都有binlog
                            MKBinLogEncode sEncode;
                            bool bIsSetType = sEncode.DecodeKey(line);
                            if (BINLOG_ACTIVE == sEncode.GetBinLogType())
                            {
                                uint32_t iSize = line.length();
                                iCurSize += iSize;
                                iSize = htonl(iSize);
                                oStr.append(string((const char*)&iSize, sizeof(uint32_t)));
                                oStr.append(line);
                            }
                            else
                            {
                                if (bIsSetType)
                                {
                                    string mKey = sEncode.GetMK();
                                    string uKey = sEncode.GetUK();
                                    MultiHashMap::Value value;
                                    uint32_t iSynTime, iExpireTime = 0;
                                    uint8_t iVersion;
                                    bool bDirty;
                                    int iRet = g_HashMap.get(mKey, uKey, value, iSynTime, iExpireTime, iVersion, bDirty, false, -1);
                                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                                    {
                                        line = sEncode.EncodeSet(mKey, uKey, value._value, iExpireTime, bDirty);
                                    }
                                    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                                    {
                                        line = sEncode.EncodeErase(mKey, uKey);
                                    }
                                    else
                                    {
                                        TLOGERROR("[MKBinLogImp::getLogCompressWithPart] g_HashMap.get error iRet:" << iRet << " mkey:" << mKey << endl);
                                        g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                                        return -1;
                                    }
                                }
                                uint32_t iSize = line.length();
                                iCurSize += iSize;
                                iSize = htonl(iSize);
                                oStr.append(string((const char*)&iSize, sizeof(uint32_t)));
                                oStr.append(line);
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
                rspData.syncTime = MKBinLogEncode::GetTime(lastTimeString);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(rspData.syncTime, 0);
            }
            if (ifs.eof())
            {
                if (bNextFile)
                {
                    rspData.curSeek = 0;
                    rspData.curLogfile = sNextLogfile;
                }
            }

            ifs.close();
        }
        catch (const std::exception &ex)
        {
            TLOGERROR("[MKBinLogImp::getLogCompressWithPart] exception:" << ex.what() << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            return -1;
        }
        catch (...)
        {
            TLOGERROR("[MKBinLogImp::getLogCompressWithPart] unkown exception" << endl);
            g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
            return -1;
        }

        if (_isGzip)
        {
            //大于1M就开启压缩
            if (oStr.length() > 1048576)
            {
                bGzipOk = StringUtil::gzipCompress(oStr.c_str(), oStr.size(), rspData.compLog);
                if (!bGzipOk)
                    TLOGERROR("[MKBinLogImp::getLogCompressWithPart] gzip compress error" << endl);
            }
        }

        if (getLogFilePrefix(rspData.curLogfile) != _binlogFile)
        {
            TLOGERROR("[MKBinLogImp::getLogCompressWithPart] return error:" << rspData.curLogfile << " should be:" << _binlogFile << endl);
        }

        if (!bGzipOk)
        {
            rspData.compLog = oStr;
        }
    }

    unsigned int retLogSize = rspData.compLog.size();
    if (rsp.isPart)
    {
        //指定分块返回
        if (_isKeySyncMode)
        {
            string index = req.logfile + "_" + TC_Common::tostr(seek) + "_" + current->getIp();
            TC_ThreadLock::Lock lock(g_BinLogLock);
            map<string, string>::iterator it = g_mBinlogData.find(index);

            if (it == g_mBinlogData.end())
            {
                TLOGERROR("[MKBinLogImp::getLogCompressWithPart] can not find binlog data at g_mBinlogData! " << index << endl);
                return -2;
            }

            //TLOGDEBUG("[MKBinLogImp::getLogCompressWithPart] found data! " <<index<< endl);

            rspData.compLog = it->second;
            retLogSize = rspData.compLog.size();
        }

        unsigned int begin = rsp.partNum * TRANSFER_BLOCK_SIZE;
        unsigned int end = (rsp.partNum + 1) * TRANSFER_BLOCK_SIZE;

        if (begin > retLogSize)
            return -1;

        if (end >= retLogSize)
        {
            end = retLogSize;
            rsp.isEnd = true;

            //清理数据
            if (_isKeySyncMode)
            {
                string index = req.logfile + "_" + TC_Common::tostr(seek) + "_" + current->getIp();
                TC_ThreadLock::Lock lock(g_BinLogLock);
                map<string, string>::iterator it = g_mBinlogData.find(index);

                if (it != g_mBinlogData.end())
                {
                    TLOGDEBUG("[MKBinLogImp::getLogCompressWithPart] erase data! " << index << endl);
                    g_mBinlogData.erase(it);
                }
            }
        }

        rspData.compLog = rspData.compLog.substr(begin, end - begin);

        // 返回0表示gz压缩成功的，返回1表示没有压缩
        if (!bGzipOk)
            return 1;
        else
            return 0;
    }
    else if ((retLogSize > MAX_BLOCK_SIZE))
    {
        //返回的数据超过大小
        //缓存keybinlog的数据
        if (_isKeySyncMode)
        {
            string index = req.logfile + "_" + TC_Common::tostr(seek) + "_" + current->getIp();
            TC_ThreadLock::Lock lock(g_BinLogLock);
            g_mBinlogData[index] = rspData.compLog;

            TLOGDEBUG("[MKBinLogImp::getLogCompressWithPart] insert g_mBinlogData " << index << endl);
        }
        rspData.compLog = rspData.compLog.substr(0, TRANSFER_BLOCK_SIZE);

        rsp.isPart = true;
        rsp.partNum = 0;
        rsp.isEnd = false;

        if (!bGzipOk)
            return 1;
        else
            return 0;
    }

    //TLOGDEBUG("[MKBinLogImp::getLogCompressWithPart] logsize:"<< retLogSize<< " transfer limit size:" <<MAX_BLOCK_SIZE << endl) ;

    rsp.isPart = false;
    rsp.partNum = 0;
    rsp.isEnd = true;

    if (!bGzipOk)
        return 1;
    else
        return 0;
}

tars::Int32 MKBinLogImp::getLastBinLogTime(tars::UInt32& lastTime, tars::TarsCurrentPtr current)
{
    lastTime = g_app.gstat()->getBinlogTimeLast();

    return 0;
}

string MKBinLogImp::getLogFileTime(const std::string & logfile)
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

string MKBinLogImp::getLogFilePrefix(const std::string & logfile)
{
    return logfile.substr(0, _binlogFile.length());
}

bool MKBinLogImp::findNextLogFile(const std::string & logfile, std::string &nextfile)
{
    time_t tNow = TC_TimeProvider::getInstance()->getNow();
    string sNow = TC_Common::tm2str(tNow, "%Y%m%d%H");

    string sLogfileTime = getLogFileTime(logfile);
    if (sLogfileTime.length() <= 0)
        return false;

    //检查获取的时间是否正确
    for (unsigned int i = 0; i < sLogfileTime.size(); i++)
    {
        if (!isdigit(sLogfileTime[i]))
        {
            TLOGERROR("[MKBinLogImp::findNextLogFile] binlog file name error! " << logfile << endl);
            return false;
        }
    }

    sLogfileTime = sLogfileTime + "0000";
    struct tm stTm;
    TC_Common::str2tm(sLogfileTime, "%Y%m%d%H%M%S", stTm);
    time_t t = mktime(&stTm);

    //binlog最少是从半年前开始，应该不会有binlog保存超过半年的吧，-_-!
    if ((tNow - t) > 15552000)
    {
        TLOGERROR("[MKBinLogImp::findNextLogFile] binlog file name may be too old! " << logfile << endl);
        t = tNow - 15552000;
    }

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
