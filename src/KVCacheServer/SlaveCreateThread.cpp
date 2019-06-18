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
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <fstream>

#include "SlaveCreateThread.h"
#include "TBinLogEncode.h"
#include "util/tc_common.h"
#include "NormalHash.h"
#include "CacheServer.h"
#include "CacheGlobe.h"
#include "zlib.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>

using namespace std;
using namespace tars;
using namespace DCache;

// restore 按天日志
string SlaveCreateThread::_recoverDayLog;

SlaveCreateThread::SlaveCreateThread()
{
    _isStart = false;
    _isRuning = false;
}

int SlaveCreateThread::init(const string &mirrorPath, const vector<string> &binlogPath, time_t recoverTime, bool normal, const string &sConf)
{
    _recoverDayLog = _tcConf.get("/Main/<BackupDayLog>", "dumpAndRecover");

    if (_isStart)
    {
        FDLOG(_recoverDayLog) << "[SlaveCreateThread::init] SlaveCreateThread has started" << endl;
        return -1;
    }
    if (recoverTime > TNOW)
    {
        FDLOG(_recoverDayLog) << "[SlaveCreateThread::init] input recoverTime is invalid. input:" << recoverTime
                << "|" << TC_Common::tm2str(recoverTime, "%Y%m%d%H%M%S") << endl;
        return -1;
    }
    _config = sConf;
    _tcConf.parseFile(_config);

    _mirrorPath = mirrorPath;
    _binlogPath = binlogPath;

    _isNormal = normal;

    _recoverTime = recoverTime;

    return 0;
}

void SlaveCreateThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (!_isStart)
    {
        _isStart = true;
        if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
        {
            throw runtime_error("Create SlaveCreateThread fail");
        }
    }
}

void* SlaveCreateThread::Run(void* arg)
{
    pthread_detach(pthread_self());
    SlaveCreateThread* pthis = (SlaveCreateThread*)arg;
    pthis->setRuning(true);

    // 当前binlog行数置0
    pthis->_nowBinlogIndex = 0;

    bool bSucc = false;
    do
    {
        // 需要设置个标识，禁止一些操作，比如停止BinLogThread等
        g_app.gstat()->setSlaveCreating(true);

        // 判断备机自建过程中不允许的操作是不是都已经停止
        while (1)
        {
            if (!g_app.isAllInSlaveCreatingStatus())
            {
                usleep(100000);
            }
            else
            {
                if (g_app.gstat()->serverType() == MASTER)
                {
                    ostringstream os;
                    os << "server changed from SLAVE to MASTER, slave try to create data failed";
                    TARS_NOTIFY_ERROR(os.str());
                    FDLOG(_recoverDayLog) << os.str() << endl;
                    g_app.gstat()->setSlaveCreating(false);

                    pthis->setRuning(false);
                    pthis->setStart(false);

                    return NULL;
                }

                break;
            }
        }

        if (!pthis->isStart())
        {
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] slave create thread is stoped" << endl;
            break;
        }

        // 发起备机自建数据以后创建文件，成功后删除，否则一直保留
        string sFile = ServerConfig::DataPath + "/SlaveCreating.dat";
        ofstream ofs(sFile.c_str());
        if (!ofs)
        {
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] create file: " << sFile << " failed, errno = " << errno << endl;
            g_app.ppReport(PPReport::SRP_EX, 1);
            break;
        }
        ofs.close();

        // 清空hashmap
        g_sHashMap.clear();

        if (!pthis->isStart())
        {
            // 直接处理返回还是跳出循环后继续处理？？？
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] slave create thread is stoped" << endl;
            break;
        }

        // 首先加载镜像
        int iRet = pthis->restoreFromDumpMirrorFile();
        if (iRet != 0)
        {
            if (iRet == 1)
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] slave create thread is stoped" << endl;
            }
            else
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] restoreFromDumpMirrorFile failed" << endl;
            }

            break;
        }

        // 加载镜像成功后开始加载按小时的binlog
        iRet = pthis->restoreFromHourBinLogFile();
        if (iRet != 0)
        {
            if (iRet == 1)
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] slave create thread is stoped" << endl;
            }
            else
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] restoreFromHourBinLogFile failed" << endl;
            }

            break;
        }

        if (!pthis->isStart())
        {
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] is stoped" << endl;
            break;
        }

        ServerInfo bakServer;
        if (pthis->getBakSourceInfo(bakServer) != 0)
        {
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] server have no bak source, can not finish" << endl;
            break;
        }

        string sBakLogFile = bakServer.serverName + "_binlog_" + pthis->_hourBinLogTime + ".log";

        //检查是否为强制切换
        if (!pthis->_isNormal)
        {
            if (pthis->saveSyncPoint(sBakLogFile, 0) != 0)
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] save sync point failed, file: " << sBakLogFile << ", seek: 0" << endl;
                break;
            }
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] save sync point fileName:" << sBakLogFile << " force recover" << endl;
            bSucc = true;
        }
        else
        {
            string sBinLogPrx = bakServer.BinLogServant;
            BinLogPrx m_pBinLogPrx = Application::getCommunicator()->stringToProxy<BinLogPrx>(sBinLogPrx);

            BinLogReq req;
            BinLogRsp rsp;
            try
            {
                req.logfile = sBakLogFile;
                req.seek = 0;
                req.lineCount = 1;

                iRet = m_pBinLogPrx->getLog(req, rsp);
            }
            catch (exception& ex)
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] getLog from " << sBinLogPrx << " exception: " << ex.what() << endl;
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                break;
            }
            if (iRet != 0 || rsp.logContent.empty())
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] getLog from " << sBinLogPrx << " failed" << endl;
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                break;
            }

            if (pthis->saveSyncPoint(sBakLogFile, 0) != 0)
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] save sync point failed, file: " << sBakLogFile << ", seek: 0" << endl;
                break;
            }
            FDLOG(_recoverDayLog) << "SlaveCreateThread::Run save sync point fileName:" << sBakLogFile << endl;
            bSucc = true;
        }

    } while (0);

    // 恢复流程正常结束，无论成功失败，都要删除SlaveCreating.dat文件
    if (!bSucc)
    {
        FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] slave create data failed" << endl;
        TARS_NOTIFY_ERROR("slave create data failed");
        g_app.gstat()->setSlaveCreating(false);

        string sFile = ServerConfig::DataPath + "/SlaveCreating.dat";
        remove(sFile.c_str());
    }
    else
    {
        g_app.gstat()->setSlaveCreating(false);
        while (g_app.gstat()->getBinlogTimeLast() != g_app.gstat()->getBinlogTimeSync())
        {
            sleep(5);
        }

        FDLOG(_recoverDayLog) << "slave create data succ" << endl;
        TARS_NOTIFY_NORMAL("slave create data succ");

        string sFile = ServerConfig::DataPath + "/SlaveCreating.dat";
        remove(sFile.c_str());
    }

    pthis->setRuning(false);
    pthis->setStart(false);

    return NULL;
}

int SlaveCreateThread::gzipDecompressFile(const string & gzipFileName, string & outFileName)
{
    string gzipCmd = "gzip -d " + gzipFileName;
    pid_t ret = pox_system(gzipCmd.c_str());
    if (-1 == ret)
    {
        FDLOG(_recoverDayLog) << ret << "|" << gzipCmd << " fail gzip fileName:" << gzipFileName << " errno:" << string(strerror(errno)) << endl;
    }
    else
    {
        if (WIFEXITED(ret))
        {
            if (0 == WEXITSTATUS(ret))
            {
                FDLOG(_recoverDayLog) << gzipCmd << "sucss fileName" << gzipFileName << endl;
                outFileName = gzipFileName.substr(0, gzipFileName.size() - 3);
                return 0;
            }
            else
            {
                FDLOG(_recoverDayLog) << ret << "|" << gzipCmd << "fail fileName:" << gzipFileName << " errno:" << string(strerror(errno)) << endl;
            }
        }
        else
        {
            FDLOG(_recoverDayLog) << ret << "|" << gzipCmd << "fail fileName:" << gzipFileName << " errno:" << string(strerror(errno)) << endl;
        }
    }

    return -1;
}

int SlaveCreateThread::getBakSourceInfo(ServerInfo& serverInfo)
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    int iRet = g_route_table.getBakSource(sServerName, serverInfo);
    if (iRet != UnpackTable::RET_SUCC)
    {
        FDLOG(_recoverDayLog) << "[SlaveCreateThread::GetBakSourceInfo] getBakSourceInfo error, iRet = " << iRet << endl;
        return -1;
    }

    return 0;
}

int SlaveCreateThread::saveSyncPoint(const string& bakLogFile, size_t seek)
{
    string sFile = ServerConfig::DataPath + "/sync_point.data";

    ofstream ofs;
    ofs.open(sFile.c_str(), ios::out | ios::binary);

    if (!ofs)
    {
        FDLOG(_recoverDayLog) << "[SlaveCreateThread::SaveSyncPoint] open file: " << sFile << " failed" << endl;
        g_app.ppReport(PPReport::SRP_EX, 1);
        return -1;
    }

    string line;
    line = bakLogFile + "|" + TC_Common::tostr(seek);

    ofs << line;
    if (!ofs)
    {
        FDLOG(_recoverDayLog) << "[SlaveCreateThread::SaveSyncPoint] write sync point: " << line << " to file: " << sFile << " failed" << endl;
        g_app.ppReport(PPReport::SRP_EX, 1);
        return -1;
    }

    ofs.close();

    return 0;
}

int SlaveCreateThread::checkLastLine(const string &binLogLine, long nowBinlogIndex)
{
    vector<string> vt;
    ostringstream os;
    vt = TC_Common::sepstr<string>(binLogLine, ":");
    if (vt.size() != 2)
    {
        return 1;
    }
    else
    {
        if (vt[0] == "totalBinlogNum")
        {
            if (nowBinlogIndex == TC_Common::strto<long>(vt[1]))
            {
                return 0;
            }
            else
            {
                os << "[SlaveCreateThread::Import5BinLog] import dumpFile fail lastling binlogNum:" << vt[1] << " now import num:" << nowBinlogIndex;
                FDLOG(_recoverDayLog) << os.str() << endl;
                TARS_NOTIFY_ERROR(os.str());
                return -1;
            }
        }
        else
        {
            return 1;
        }
    }
}

int SlaveCreateThread::restoreFromBinLog(const string& fullFileName, string& returnBinLog, bool isDumpBinLog)
{
    ifstream ifs;
    ostringstream os;

    FDLOG(_recoverDayLog) << "open file: " + fullFileName + " ok, begin operation" << endl;

    string binLogLine("");
    int iRet = 0;
    bool bRecoverEnd = false;

    returnBinLog = "";

    if (isDumpBinLog)
    {
        ifs.open(fullFileName.c_str(), ios::in | ios::binary);
        if (!ifs)
        {
            os.str("");
            os << "[SlaveCreateThread::restoreFromBinLog] open mirror file: " << fullFileName << " failed, err = " << string(strerror(errno));
            FDLOG(_recoverDayLog) << os.str() << endl;
            TARS_NOTIFY_ERROR(os.str().c_str());
            return -1;
        }
        FDLOG(_recoverDayLog) << "open mirror file: " + fullFileName + " ok, begin restore from mirror file binlog" << endl;

        //dump的镜像文件首行是"groupName:" + g_groupName + "\n";
        if (getline(ifs, binLogLine))
        {
            vector<string> vt;
            vt = TC_Common::sepstr<string>(binLogLine, ":");
            if (vt.size() != 2)
            {
                os << "[SlaveCreateThread::restoreFromBinLog] dump mirror file firstLine error, mirror file name:" << fullFileName << ", err = " << string(strerror(errno));
                FDLOG(_recoverDayLog) << os.str() << endl;
                TARS_NOTIFY_ERROR(os.str().c_str());
                return -1;
            }
            else
            {
                if (vt[1] == g_app.gstat()->groupName())
                {
                    os << "[SlaveCreateThread::restoreFromBinLog] dump mirror file firstLine ok, groupName：" << vt[1];
                    FDLOG(_recoverDayLog) << os.str() << endl;
                }
                else
                {
                    os << "[SlaveCreateThread::restoreFromBinLog] dump mirror file groupName in file is " << vt[1] << ". cache groupName" << g_app.gstat()->groupName();
                    FDLOG(_recoverDayLog) << os.str() << endl;
                }
            }
        }
        else
        {
            ostringstream os;
            os << "[SlaveCreateThread::restoreFromBinLog] dumpFile getline error " << fullFileName << " failed, err = " << string(strerror(errno));
            FDLOG(_recoverDayLog) << os.str() << endl;
            TARS_NOTIFY_ERROR(os.str().c_str());
            return -1;
        }
    }
    else
    {
        ifs.open(fullFileName.c_str(), ios::in | ios::binary);

        if (!ifs)
        {
            os.str("");
            os << "[SlaveCreateThread::restoreFromBinLog] open hour binlog file: " << fullFileName << " failed, err = " << string(strerror(errno));
            FDLOG(_recoverDayLog) << os.str() << endl;
            TARS_NOTIFY_ERROR(os.str().c_str());
            return -1;
        }
        FDLOG(_recoverDayLog) << "open hour binlog file: " + fullFileName + " ok, begin operation" << endl;
    }

    while (true)
    {
        if (!isStart())
        {
            // restore线程被停止
            iRet = 1;
            break;
        }

        binLogLine.clear();
        int iRet = TBinLogEncode::GetOneLineBinLog(ifs, binLogLine);
        if (0 == iRet)
        {
            if (isDumpBinLog)
            {
                //dump文件的最后一行"totalBinlogNum:"+ 数据总条数
                iRet = checkLastLine(binLogLine, _nowBinlogIndex);
                if (iRet == 0)
                {
                    os << "[SlaveCreateThread::restoreFromBinLog] import dumpFile succ binlogNum:" << _nowBinlogIndex;
                    FDLOG(_recoverDayLog) << os.str() << endl;
                    ifs.close();
                    return 0;
                }
                else if (iRet == -1)
                {
                    ifs.close();
                    return -1;
                }

            }

            time_t binlogTime = TBinLogEncode::GetTime(binLogLine);
            if (binlogTime > _recoverTime)
            {
                bRecoverEnd = true;
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] end binlogTime:" << TC_Common::tm2str(binlogTime, "%Y%m%d%H%M%S")
                        << " _recoverTime:" << TC_Common::tm2str(_recoverTime, "%Y%m%d%H%M%S") << endl;
                break;
            }
            returnBinLog = binLogLine;
            TBinLogEncode sEncode;

            try
            {
                sEncode.Decode(binLogLine);
            }
            catch (exception &ex)
            {
                bRecoverEnd = true;
                iRet = -1;
                FDLOG(_recoverDayLog) << "SlaveCreateThread::restoreFromBinLog sEncode.Decode exception: " << ex.what() << endl;
                break;
            }

            if (sEncode.GetBinLogType() == BINLOG_ACTIVE)
                continue;

            string sKey;
            string sValue;
            switch (sEncode.GetOpt())
            {
            case BINLOG_SET:
                sKey = sEncode.GetStringKey();
                sValue = sEncode.GetValue();

                while (isStart())
                {
                    //先删除该key，防止主机还没回写，备机先回写了
                    g_sHashMap.eraseByForce(sKey);
                    int iRet = g_sHashMap.set(sKey, sValue, sEncode.GetDirty(), sEncode.GetExpireTime());
                    if (iRet != TC_HashMapMalloc::RT_OK)
                    {
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] map set error, key = " << sKey << " iRet = " << iRet << endl;
                        g_app.ppReport(PPReport::SRP_BAKCENTER_ERR, 1);
                    }
                    else
                    {
                        break;
                    }
                    sleep(1);
                }
                break;
            case BINLOG_DEL:
                sKey = sEncode.GetStringKey();
                while (true)
                {
                    iRet = g_sHashMap.del(sKey);
                    if (iRet != TC_HashMapMalloc::RT_OK && iRet != TC_HashMapMalloc::RT_NO_DATA && iRet != TC_HashMapMalloc::RT_ONLY_KEY)
                    {
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] map del error,  key = " << sKey << " iRet = " << iRet << endl;
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
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] map erase error, key = " << sKey << " iRet = " << iRet << endl;
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
                while (isStart())
                {
                    int iRet = g_sHashMap.set(sKey);
                    if (iRet != TC_HashMapMalloc::RT_OK)
                    {
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] map set error, key = " << sKey << " iRet = " << iRet << endl;
                        g_app.ppReport(PPReport::SRP_BAKCENTER_ERR, 1);
                    }
                    else
                    {
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] set only key succ! " << sKey << endl;
                    }
                    break;
                }
                break;
            default:
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] Binlog format error, opt error " << sEncode.GetOpt() << endl;
                break;
            }
        }
        else
        {
            break;
        }
    }

    if (iRet != 1)
    {
        if (!bRecoverEnd)
        {
            if (!ifs.eof())
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] getLine failed" << endl;
                iRet = -1;
            }
        }
    }

    ifs.close();
    return iRet;
}

int SlaveCreateThread::restoreFromDumpMirrorFile()
{
    //判断本地是否有镜像文件
    string localMirrorFile = _mirrorPath;
    if (access(localMirrorFile.c_str(), F_OK) == 0)
    {
        FDLOG(_recoverDayLog) << "begin restore from local dump mirror file: " << localMirrorFile << endl;

        //dump的镜像文件是使用gzwrite写入的，这里需要先对镜像文件进行解压
        string lastFileName;
        int iRet = gzipDecompressFile(localMirrorFile, lastFileName);
        if (iRet != 0)
        {
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromDumpMirrorFile] gzipDecompressFile file: " << localMirrorFile << " failed" << endl;
            return -1;
        }

        // 如果有，则先恢复镜像中的数据
        string sLastBinLog;
        iRet = restoreFromBinLog(lastFileName, sLastBinLog, true);
        if (iRet == 0)
        {
            if (sLastBinLog == "")
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromMirrorFile] dump file: " << localMirrorFile << " is empty, so can not locate hour binlog that start" << endl;
                return -1;
            }
            string sTime;
            TBinLogEncode::GetTimeString(sLastBinLog, sTime);
            _hourBinLogTime = sTime.substr(0, 10);
            return 0;
        }
        else
        {
            if (iRet == 1)
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromMirrorFile] slave create thread is stoped" << endl;
                return 1;
            }
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromMirrorFile] restoreFromBinLog failed, local dump file: " << localMirrorFile << endl;
            return -1;
        }
    }
    else if (errno != ENOENT)
    {
        ostringstream os;
        os << "[SlaveCreateThread::restoreFromMirrorFile] access file: " << localMirrorFile << " failed, err = " << string(strerror(errno));
        FDLOG(_recoverDayLog) << os.str() << endl;
        return -1;
    }
    else
    {
        ostringstream os;
        os << "[SlaveCreateThread::restoreFromMirrorFile] mirror file not exited, do not restore from mirror, need restore from binlog";
        FDLOG(_recoverDayLog) << os.str() << endl;
    }

    return 0;
}

int SlaveCreateThread::restoreFromHourBinLogFile()
{
    string sLastBinLog = "";
    vector<string>::iterator itr = _binlogPath.begin();

    while (itr != _binlogPath.end())
    {
        if ((*itr).empty())
        {
            continue;
        }

        int restoreRet = restoreFromBinLog(*itr, sLastBinLog, false);
        if (restoreRet == 0)
        {
            FDLOG(_recoverDayLog) << "restore data from hour binlog file: " << (*itr) << " succ" << endl;
            itr++;
            continue;
        }
        else if (restoreRet == 1)
        {
            FDLOG(_recoverDayLog) << "SlaveCreateThread is stoped when restoreFromBinLog in restoreFromHourBinLogFile" << endl;
            return 1;
        }
        else if (restoreRet == 2)
        {
            break;
        }
        else
        {
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromHourBinLogFile] restore data form hour binlog file: " << (*itr) << " failed" << endl;
            return -1;
        }
    }

    if (sLastBinLog != "")
    {
        string sTime;
        TBinLogEncode::GetTimeString(sLastBinLog, sTime);
        _hourBinLogTime = sTime.substr(0, 10);
    }

    return 0;
}

