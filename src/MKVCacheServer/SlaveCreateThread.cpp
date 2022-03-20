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
#include <string>

#include "SlaveCreateThread.h"
#include "MKBinLogEncode.h"
#include "util/tc_common.h"
#include "NormalHash.h"
#include "MKCacheServer.h"
#include "MKCacheGlobe.h"
#include "zlib.h"

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

    _insertAtHead = (_tcConf["/Main/Cache<InsertOrder>"] == "Y" || _tcConf["/Main/Cache<InsertOrder>"] == "y") ? true : false;
    string sUpdateOrder = _tcConf.get("/Main/Cache<UpdateOrder>", "N");
    _updateInOrder = (sUpdateOrder == "Y" || sUpdateOrder == "y") ? true : false;

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
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] is stoped" << endl;
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
        g_HashMap.clear();

        if (!pthis->isStart())
        {
            // 直接处理返回还是跳出循环后继续处理？？？
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] is stoped" << endl;
            break;
        }

        // 首先加载镜像
        int iRet = pthis->restoreFromDumpMirrorFile();
        if (iRet != 0)
        {
            if (iRet == 1)
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] is stoped" << endl;
            }
            else
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] Import5DumpBinLog failed" << endl;
            }

            break;
        }

        // 加载镜像成功后开始加载按小时的binlog
        iRet = pthis->restoreFromHourBinLogFile();
        if (iRet != 0)
        {
            if (iRet == 1)
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] is stoped" << endl;
            }
            else
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Run] Import5HourBinLog failed" << endl;
            }

            break;
        }

        if (!pthis->isStart())
        {
            // 直接处理返回还是跳出循环后继续处理？？？
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
            FDLOG(_recoverDayLog) << "SlaveCreateThread::Run save sync point fileName:" << sBakLogFile << " force recover" << endl;

            bSucc = true;
        }
        else
        {
            string sBinLogPrx = bakServer.BinLogServant;
            MKBinLogPrx m_pBinLogPrx = Application::getCommunicator()->stringToProxy<MKBinLogPrx>(sBinLogPrx);

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

int SlaveCreateThread::gzipDecompressFile(const string & FileName, string & outFileName)
{
    string gzipCmd = "gzip -d " + FileName;
    pid_t ret = pox_system(gzipCmd.c_str());
    if (-1 == ret)
    {
        FDLOG(_recoverDayLog) << ret << gzipCmd << "fail fileName:" << FileName << " errno:" << string(strerror(errno)) << endl;
    }
    else
    {
        if (WIFEXITED(ret))
        {
            if (0 == WEXITSTATUS(ret))
            {
                FDLOG(_recoverDayLog) << gzipCmd << "sucss fileName" << FileName << endl;
                outFileName = FileName.substr(0, FileName.size() - 3);
                return 0;
            }
            else
            {
                FDLOG(_recoverDayLog) << ret << gzipCmd << "fail fileName:" << FileName << " errno:" << string(strerror(errno)) << endl;
            }
        }
        else
        {
            FDLOG(_recoverDayLog) << ret << gzipCmd << "fail fileName:" << FileName << " errno:" << string(strerror(errno)) << endl;
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
        FDLOG(_recoverDayLog) << "[SlaveCreateThread::saveSyncPoint] open file: " << sFile << " failed" << endl;
        g_app.ppReport(PPReport::SRP_EX, 1);
        return -1;
    }

    string line;
    line = bakLogFile + "|" + TC_Common::tostr(seek);

    ofs << line;
    if (!ofs)
    {
        FDLOG(_recoverDayLog) << "[SlaveCreateThread::saveSyncPoint] write sync point: " << line << " to file: " << sFile << " failed" << endl;
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
                os << "[SlaveCreateThread::checkLastLine] import dumpFile fail lastling binlogNum:" << vt[1] << " now import num:" << nowBinlogIndex;
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
    ostringstream os;
    ifstream ifs;

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
            os << "[SlaveCreateThread::restoreFromBinLog] open file: " << fullFileName << " failed, err = " << string(strerror(errno));
            FDLOG(_recoverDayLog) << os.str() << endl;
            TARS_NOTIFY_ERROR(os.str().c_str());
            return -1;
        }

        FDLOG(_recoverDayLog) << "open file: " + fullFileName + " ok, begin operation" << endl;
        if (getline(ifs, binLogLine))
        {
            vector<string> vt;
            vt = TC_Common::sepstr<string>(binLogLine, ":");
            if (vt.size() != 2)
            {
                os.str("");
                os << "[SlaveCreateThread::restoreFromBinLog] dumpFile firstLine error" << fullFileName << " failed, err = " << string(strerror(errno));
                FDLOG(_recoverDayLog) << os.str() << endl;
                TARS_NOTIFY_ERROR(os.str().c_str());
                return -1;
            }
            else
            {
                if (vt[1] == g_app.gstat()->groupName())
                {
                    os.str("");
                    os << "[SlaveCreateThread::restoreFromBinLog] dump mirror file firstLine ok, groupName：" << vt[1];
                    FDLOG(_recoverDayLog) << os.str() << endl;
                }
                else
                {
                    os.str("");
                    os << "[SlaveCreateThread::restoreFromBinLog] dump mirror file groupName in file is " << vt[1] << ". cache groupName" << g_app.gstat()->groupName();
                    FDLOG(_recoverDayLog) << os.str() << endl;
                }
            }
        }
        else
        {
            os.str("");
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
        int iRet = MKBinLogEncode::GetOneLineBinLog(ifs, binLogLine);
        if (0 == iRet)
        {
            if (isDumpBinLog)
            {
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
            _nowBinlogIndex++;

            time_t binlogTime = MKBinLogEncode::GetTime(binLogLine);
            if (binlogTime > _recoverTime)
            {
                bRecoverEnd = true;
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] end binlogTime:" << TC_Common::tm2str(binlogTime, "%Y%m%d%H%M%S")
                        << " _recoverTime:" << TC_Common::tm2str(_recoverTime, "%Y%m%d%H%M%S") << endl;
                break;
            }
            returnBinLog = binLogLine;

            MKBinLogEncode encode;
            try
            {
                encode.Decode(binLogLine);
            }
            catch (exception &ex)
            {
                bRecoverEnd = true;
                iRet = -1;
                FDLOG(_recoverDayLog) << "SlaveCreateThread::restoreFromBinLog encode.Decode exception: " << ex.what() << endl;
                break;
            }

            if (encode.GetBinLogType() == BINLOG_ACTIVE)
                continue;

            string mk;
            string uk;
            string value;
            vector<pair<uint32_t, string> > vtvalue;
            vector<MultiHashMap::Value> vs;

            switch (encode.GetOpt())
            {
            case BINLOG_SET:

                mk = encode.GetMK();
                uk = encode.GetUK();
                value = encode.GetValue();

                while (isStart())
                {
                    g_HashMap.eraseByForce(mk, uk);
                    int iSetRet = g_HashMap.set(mk, uk, value, encode.GetExpireTime(), 0, TC_Multi_HashMap_Malloc::DELETE_FALSE, encode.GetDirty(), TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder, 0, false);

                    if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] map set error, key = " << mk << ", ret = " << iSetRet << endl;
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

                while (isStart())
                {
                    int iEraseRet;
                    if (encode.GetOpt() == BINLOG_DEL)
                    {
                        iEraseRet = g_HashMap.delSetBit(mk, uk, time(NULL));

                        //如果没有数据，就插入一条onlykey
                        if (iEraseRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
                            iEraseRet = g_HashMap.setForDel(mk, uk, time(NULL));
                    }
                    else
                        iEraseRet = g_HashMap.erase(mk, uk);

                    if (iEraseRet != TC_Multi_HashMap_Malloc::RT_OK && iEraseRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iEraseRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iEraseRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] map erase error, key = " << mk << ", ret = " << iEraseRet << endl;
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
                    int iEraseRet;
                    if (encode.GetOpt() == BINLOG_DEL_MK)
                    {
                        uint64_t delCount = 0;
                        iEraseRet = g_HashMap.delSetBit(mk, time(NULL), delCount);
                    }
                    else
                    {
                        iEraseRet = g_HashMap.erase(mk);

                        //把主key下的数据设为部分数据
                        g_HashMap.setFullData(mk, false);
                    }

                    if (iEraseRet != TC_Multi_HashMap_Malloc::RT_OK && iEraseRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iEraseRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iEraseRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] map erase error, key = " << mk << ", ret = " << iEraseRet << endl;
                    }
                    else
                    {
                        break;
                    }
                    sleep(1);
                }
                break;
            case BINLOG_SET_MUTIL:
                mk = encode.GetMK();
                vs = encode.GetVs();
                while (true)
                {
                    //删除位设为DELETE_FALSE
                    for (unsigned int i = 0; i < vs.size(); i++)
                    {
                        vs[i]._isDelete = TC_Multi_HashMap_Malloc::DELETE_FALSE;
                    }
                    //为了保证恢复时 主key链下面的顺序 故填false
                    int iSetRet = g_HashMap.set(vs, TC_Multi_HashMap_Malloc::AUTO_DATA, false, false, true);

                    if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //严重错误
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] set error, ret = " << iSetRet << endl;
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
                    int iSetRet = g_HashMap.set(vs, TC_Multi_HashMap_Malloc::AUTO_DATA, false, false, false);

                    if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //严重错误
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] set error, ret = " << iSetRet << endl;
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
            case BINLOG_SET_ONLYKEY:
                mk = encode.GetMK();
                while (isStart())
                {
                    int iRet = g_HashMap.set(mk);
                    if ((iRet != TC_Multi_HashMap_Malloc::RT_OK) && (iRet != TC_Multi_HashMap_Malloc::RT_DATA_EXIST))
                    {
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::restoreFromBinLog] set onlyKey error! mainKey:" << mk << " ret:" << iRet << endl;
                        g_app.ppReport(PPReport::SRP_EX, 1);
                    }
                    else
                    {
                        FDLOG(_recoverDayLog) << "[SlaveCreateThread::Import5BinLog] set onlyKey succ! " << mk << endl;
                    }
                    break;
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
                        TLOG_ERROR("[SlaveCreateThread::restoreFromBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        time_t second = MKBinLogEncode::GetTime(binLogLine);
                        iRet = g_HashMap.trimList(mk, true, encode.GetHead(), false, 0, 0, second, value, delSize);
                    }
                    else
                        iRet = g_HashMap.trimList(mk, true, encode.GetHead(), false, 0, 0, 0, value, delSize);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("[SlaveCreateThread::restoreFromBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        time_t second = MKBinLogEncode::GetTime(binLogLine);
                        iRet = g_HashMap.pushList(mk, vtvalue, false, true, encode.GetPos(), second);
                    }
                    else
                        iRet = g_HashMap.pushList(mk, vtvalue, false, true, encode.GetPos(), 0);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("[SlaveCreateThread::restoreFromBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        time_t second = MKBinLogEncode::GetTime(binLogLine);
                        iRet = g_HashMap.trimList(mk, false, false, true, encode.GetStart(), encode.GetEnd(), second, value, delSize);
                    }
                    else
                        iRet = g_HashMap.trimList(mk, false, false, true, encode.GetStart(), encode.GetEnd(), 0, value, delSize);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("[SlaveCreateThread::restoreFromBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        time_t second = MKBinLogEncode::GetTime(binLogLine);
                        iRet = g_HashMap.trimList(mk, false, encode.GetHead(), false, 0, encode.GetCount(), second, value, delSize);
                    }
                    else
                        iRet = g_HashMap.trimList(mk, false, encode.GetHead(), false, 0, encode.GetCount(), 0, value, delSize);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("[SlaveCreateThread::restoreFromBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                    iRet = g_HashMap.addSet(mk, value, encode.GetExpireTime(), 0, encode.GetDirty(), TC_Multi_HashMap_Malloc::DELETE_FALSE);
                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("[SlaveCreateThread::restoreFromBinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        TLOG_ERROR("[SlaveCreateThread::Import5BinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        TLOG_ERROR("[SlaveCreateThread::Import5BinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        TLOG_ERROR("[SlaveCreateThread::Import5BinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        TLOG_ERROR("[SlaveCreateThread::Import5BinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        TLOG_ERROR("[SlaveCreateThread::Import5BinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        TLOG_ERROR("[SlaveCreateThread::Import5BinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        time_t second = MKBinLogEncode::GetTime(binLogLine);
                        iRet = g_HashMap.delRangeZSetSetBit(mk, encode.GetScoreMin(), encode.GetScoreMax(), second, time(NULL));
                    }
                    else
                        iRet = g_HashMap.delRangeZSetSetBit(mk, encode.GetScoreMin(), encode.GetScoreMax(), 0, time(NULL));
                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("[SlaveCreateThread::Import5BinLog] map set error, key = " << mk << " iRet = " << iRet << endl);
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
                        TLOG_ERROR("[MKDbAccessCallback::WirteBinLog] set error, ret = " << iRet << endl);
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
                    int iRet = g_HashMap.addSet(mk, vs, TC_Multi_HashMap_Malloc::AUTO_DATA);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //严重错误
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        TLOG_ERROR("[MKDbAccessCallback::WirteBinLog] set error, ret = " << iRet << endl);
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
                    int iRet = g_HashMap.addSet(mk, vs, TC_Multi_HashMap_Malloc::AUTO_DATA);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //严重错误
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        TLOG_ERROR("[MKDbAccessCallback::WirteBinLog] set error, ret = " << iRet << endl);
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
                    int iRet = g_HashMap.addZSet(mk, vs, TC_Multi_HashMap_Malloc::AUTO_DATA);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //严重错误
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        TLOG_ERROR("[MKDbAccessCallback::WirteBinLog] set error, ret = " << iRet << endl);
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
                    int iRet = g_HashMap.addZSet(mk, vs, TC_Multi_HashMap_Malloc::AUTO_DATA);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //严重错误
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        TLOG_ERROR("[MKDbAccessCallback::WirteBinLog] set error, ret = " << iRet << endl);
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
                        TLOG_ERROR("[MKDbAccessCallback::WirteBinLog] set error, ret = " << iRet << endl);
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
                        TLOG_ERROR("[MKDbAccessCallback::WirteBinLog] set error, ret = " << iRet << endl);
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
                        TLOG_ERROR("[MKBinLogThread::WirteBinLog] update zset error, key = " << mk << " iRet = " << iRet << endl);
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
                        iRet = g_HashMap.addZSet(mk, value, encode.GetScore(), _ExpireTime, 0, encode.GetDirty(), false, TC_Multi_HashMap_Malloc::DELETE_FALSE);
                        if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                        {
                            TLOG_ERROR("[MKBinLogThread::WirteBinLog] update zset error, key = " << mk << " iRet = " << iRet << endl);
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
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Import5BinLog] Binlog format error, opt error" << endl;
                g_app.ppReport(PPReport::SRP_BINLOG_ERR, 1);
                break;
            }
        }
        else
            break;
    }

    if (iRet != 1)
    {
        if (!bRecoverEnd)
        {
            if (!ifs.eof())
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Import5BinLog] getLine failed" << endl;
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
        FDLOG(_recoverDayLog) << "begin import from local dump file: " << localMirrorFile << endl;

        //dump的镜像文件是使用gzwrite写入的，这里需要先对镜像文件进行解压
        string lastFileName;
        int gzipRet = gzipDecompressFile(localMirrorFile, lastFileName);
        if (gzipRet != 0)
        {
            FDLOG(_recoverDayLog) << "[SlaveCreateThread::Import5DumpBinLog] gzipFile file: " << localMirrorFile << " failed" << endl;
            return -1;
        }

        // 如果有，则先恢复镜像中的数据
        string sLastBinLog;
        int iRet = restoreFromBinLog(lastFileName, sLastBinLog, true);
        if (iRet == 0)
        {
            if (sLastBinLog == "")
            {
                FDLOG(_recoverDayLog) << "[SlaveCreateThread::Import5DumpBinLog] dump file: " << localMirrorFile << " is empty, so can not locate hour binlog that start" << endl;
                return -1;
            }
            string sTime;
            MKBinLogEncode::GetTimeString(sLastBinLog, sTime);
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
        os << "[SlaveCreateThread::restoreFromMirrorFile] mirror file not exited, do not restore from mirror, need restore from hour binlog";
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
        MKBinLogEncode::GetTimeString(sLastBinLog, sTime);
        _hourBinLogTime = sTime.substr(0, 10);
    }

    return 0;
}

