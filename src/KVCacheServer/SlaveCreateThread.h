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
#ifndef _SLAVE_CREATE_THREAD_H_
#define _SLAVE_CREATE_THREAD_H_

#include <iostream>
#include "servant/Application.h"

#include "CacheGlobe.h"

using namespace std;
using namespace DCache;
using namespace tars;

class SlaveCreateThread
{
public:

    SlaveCreateThread();

    virtual ~SlaveCreateThread() {}

    /*
    *初始化
    */
    int init(const string &mirrorPath, const vector<string> &binlogPath, time_t recoverTime, bool normal, const string &sConf);

    /*
    *生成线程
    */
    void createThread();

    /*
    *线程run
    */
    static void* Run(void* arg);

    /*
    *恢复镜像文件 如果本地有则 直接用本地文件，没有则直接返回
    */
    int restoreFromDumpMirrorFile();

    /*
    *恢复binlog文件, 从本机目录下恢复
    */
    int restoreFromHourBinLogFile();

    /*
    *把binlog文件导入到cache
    */
    int restoreFromBinLog(const string & fullFileName, string & returnBinLog, bool isDumpBinLog);

    /*停止线程*/
    void stop() {
        _isStart = false;
    }

    bool isStart() {
        return _isStart;
    }

    void setStart(bool bStart) {
        _isStart = bStart;
    }

    bool isRuning() {
        return _isRuning;
    }

    void setRuning(bool bRuning) {
        _isRuning = bRuning;
    }

protected:

    int gzipDecompressFile(const string & gzipFileName, string & outFileName);

    int getBakSourceInfo(DCache::ServerInfo& serverInfo);

    int saveSyncPoint(const string& bakLogFile, size_t seek);

    int checkLastLine(const string &binLogLine, long nowBinlogIndex);

protected:

    //按天日志
    static string _recoverDayLog;

    //线程启动停止标志
    bool _isStart;

    //线程当前状态
    bool _isRuning;

    TC_Config _tcConf;

    string _config;

    //是否是正常恢复 正常的恢复流程再加载完镜像和binlog后，会再从主机同步一条binlog
    //而强制恢复流程，则是加载完镜像和binlog后，就认为恢复完成
    bool _isNormal;

    //镜像文件绝对路径
    string _mirrorPath;

    //binlog文件绝对路径
    vector<string> _binlogPath;

    // 恢复到的binlog时间
    time_t _recoverTime;

    // 恢复到的binlog时间点
    string _hourBinLogTime;

    // 标识binlog行数
    long _nowBinlogIndex;
};

#endif
