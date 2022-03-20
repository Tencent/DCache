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
#include <vector>
#include <fstream>
#include <strstream>
#include <zlib.h>

#include "DumpThread.h"
#include "MKBinLogEncode.h"
#include "MKCacheServer.h"

using namespace std;


int DumpThread::init(const string &dumpPath, const string &mirrorName, const string &sConf)
{
    if (_isStart)
    {
        TLOG_DEBUG("[DumpThread::init] DumpThread has started" << endl);
        return -1;
    }

    _config = sConf;
    _tcConf.parseFile(_config);

    _dumpDayLog = _tcConf.get("/Main/<BackupDayLog>", "dumpAndRecover");

    if (!TC_File::isAbsolute(dumpPath))
    {
        _dumpPath = "/data/dcache/dump_binlog/"; //使用默认dump路径
        FDLOG(_dumpDayLog) << "[DumpThread::init] dumpPath:" << dumpPath << " is not absolute path, use default path: /data/dcache/dump_binlog/" << endl;
    }
    else
    {
        _dumpPath = dumpPath;
    }

    if (mirrorName.empty())
    {
        // using default mirror name
        _mirrorName = g_app.gstat()->groupName() + string("_dump_binlog_") + TC_Common::now2str() + string(".log.gz");
        FDLOG(_dumpDayLog) << "[DumpThread::init] mirrorName is empty, use default mirror name: groupname_dump_binlog_nowtime.log.gz" << endl;
    }
    else
    {
        _mirrorName = mirrorName;
    }

    FDLOG(_dumpDayLog) << "[DumpThread::init] finish" << endl;

    return 0;
}

void DumpThread::doDump()
{
    stringstream errorOs;
    string errmsg;
    int iRet;

    // 线程运行标志设置为true
    _isRuning = true;

    try
    {
        do
        {
            //产生dump记录文件 如果dump过程中 服务被重启，会通过这个文件打印错误日志
            string sFile = ServerConfig::DataPath + "/Dumping.dat";
            ofstream ofs(sFile.c_str());
            if (!ofs)
            {
                errorOs << "[DumpThread::doDump] create file: " << sFile << " failed, errno = " << errno;
                errmsg = errorOs.str();
                FDLOG(_dumpDayLog) << errmsg << endl;
                break;
            }

            if (!_isStart)
            {
                errorOs << "[DumpThread::doDump] is stoped";
                errmsg = errorOs.str();
                FDLOG(_dumpDayLog) << errmsg << endl;
                break;
            }

            // dump镜像
            iRet = cacheToBinlog(_dumpPath, _mirrorName, errmsg);
            // -1 表示 cacheToBinlog在dump时候出现错误
            if (iRet < 0)
            {
                TLOG_ERROR("dump error: " << errmsg << endl);
                break;
            }

            if (!_isStart)
            {
                errorOs << "[DumpThread::doDump] is stoped";
                errmsg = errorOs.str();
                FDLOG(_dumpDayLog) << errmsg << endl;
                break;
            }

        } while (0);
    }
    catch (const std::exception & ex)
    {
        errorOs << "[DumpThread::doDump] catch exception: " << ex.what();
        errmsg = errorOs.str();
        FDLOG(_dumpDayLog) << errmsg << endl;
    }

    {
        // 正常结束，无论dump成功失败，删除Dumping文件
        string sFile = ServerConfig::DataPath + "/Dumping.dat";
        remove(sFile.c_str());
    }

    //dump执行完成
    _isRuning = false;
}

int DumpThread::cacheToBinlog(const string & dumpPath, const string & mirrorName, string & errmsg)
{
    if (!TC_File::makeDirRecursive(dumpPath))
    {
        errmsg = "[DumpThread::cacheToBinlog] cannot create dir: " + dumpPath;
        TLOG_ERROR(errmsg << endl);
        FDLOG(_dumpDayLog) << errmsg << endl;
        return -1;
    }

    //dump镜像的全路径
    string dumpFileName = dumpPath + mirrorName;

    gzFile gzf = gzopen(dumpFileName.c_str(), "wb9");
    if (!gzf)
    {
        errmsg = "DumpThread::doDump(), open dump filename:" + dumpFileName + " failed";
        FDLOG(_dumpDayLog) << errmsg << endl;
        return -1;
    }

    string sData;

    MKBinLogEncode logEncode;
    long elementCount = 0;
    int64_t dumpStartTime = 0;
    int64_t dumpEndTime = 0;
    string firstBinlogTime;
    string lastBinlogTime;

    dumpStartTime = TC_Common::now2ms();

    //dump mirror文件第一行内容是 组名
    sData = "groupName:" + g_app.gstat()->groupName() + "\n";
    gzwrite(gzf, sData.c_str(), sData.size());

    size_t iMKeyMaxBlockCount = 0;
    MultiHashMap::mk_hash_iterator it = g_HashMap.mHashBegin();
    while (it != g_HashMap.mHashEnd())
    {
        map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > > tmpMap;
        it->getAllData(tmpMap);
        map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator itr = tmpMap.begin();
        while (itr != tmpMap.end())
        {
            elementCount++;
            if (!_isStart)
            {
                errmsg = "[DumpThread::doDump] dump thread is stoped";
                FDLOG(_dumpDayLog) << errmsg << endl;
                return -1;
            }
            string sBinLog;
            string tmpBinlogTime;
            string sMainKey = itr->first;

            if (itr->second.second.size() > 0)
            {
                if (itr->second.second[0]._keyType == TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE)
                {
                    //不为空，表示不是onlyKey数据
                    if (!(itr->second.second[0]._ukey.empty()))
                        sBinLog = logEncode.EncodeSet(sMainKey, itr->second.second, itr->second.first, tmpBinlogTime) + "\n";
                    else
                    {
                        tmpBinlogTime = TC_Common::tm2str(TC_TimeProvider::getInstance()->getNow());
                        sBinLog = logEncode.EncodeMKOnlyKey(sMainKey) + "\n";
                    }
                }
                else if (itr->second.second[0]._keyType == TC_Multi_HashMap_Malloc::MainKey::LIST_TYPE)
                {
                    sBinLog = logEncode.EncodePushList(sMainKey, itr->second.second, itr->second.first, tmpBinlogTime) + "\n";
                }
                else if (itr->second.second[0]._keyType == TC_Multi_HashMap_Malloc::MainKey::SET_TYPE)
                {
                    sBinLog = logEncode.EncodeAddSet(sMainKey, itr->second.second, itr->second.first, tmpBinlogTime) + "\n";
                }
                else if (itr->second.second[0]._keyType == TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE)
                {
                    sBinLog = logEncode.EncodeAddZSet(sMainKey, itr->second.second, itr->second.first, tmpBinlogTime) + "\n";
                }

            }
            else
            {
                tmpBinlogTime = TC_Common::tm2str(TC_TimeProvider::getInstance()->getNow());
                sBinLog = logEncode.EncodeMKOnlyKey(sMainKey) + "\n";
            }

            gzwrite(gzf, sBinLog.c_str(), sBinLog.size());

            if (firstBinlogTime.empty())
            {
                firstBinlogTime = tmpBinlogTime;
            }
            lastBinlogTime = tmpBinlogTime;

            size_t vecMKeySize = itr->second.second.size();
            if (iMKeyMaxBlockCount < vecMKeySize)
            {
                iMKeyMaxBlockCount = vecMKeySize;
            }

            itr++;
        }

        ++it;
    }
    g_HashMap.setMaxBlockCount(iMKeyMaxBlockCount);

    //最后一行: binlog的总个数
    sData = string("totalBinlogNum:") + TC_Common::tostr(elementCount) + "\n";
    gzwrite(gzf, sData.c_str(), sData.size());

    gzclose(gzf);

    dumpEndTime = TC_Common::now2ms();

    long fileSize = TC_File::getFileSize(dumpFileName);
    FDLOG(_dumpDayLog) << "dump create finish begin rsync firstBinlogTime:" << firstBinlogTime << " lastBinlogTime:" << lastBinlogTime << " filesize:" << fileSize << " time cost(ms):" << (dumpEndTime - dumpStartTime) << endl;

    return 0;
}

