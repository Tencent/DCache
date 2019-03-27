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
#ifndef _MKBINLOGTHREAD_H
#define _MKBINLOGTHREAD_H

#include <iostream>
#include "servant/Application.h"
#include "MKBinLog.h"
#include "MKBinLogEncode.h"
#include "MKCacheGlobe.h"
#include "MKCacheComm.h"
#include "MKBinLogTimeThread.h"

using namespace std;
using namespace tars;
using namespace DCache;

const size_t SYNC_ADDSIZE_STEP = 64 * 1024;

//用于放入队列中的结构
struct stBinlogData :public TC_HandleBase
{
    int iRet;
    int iLastTime;
    int iSyncTime;
    int64_t curSeek;//返回数据后的更新文件偏移量
    string curLogfile;//返回数据后的更新文件名

    string m_logFile;//当前同步binlog文件名
    uint64_t m_seek;//当前同步binlog日志文件的偏移量

    string sLogContent;
    vector<string> vLogContent;
};

typedef tars::TC_AutoPtr<stBinlogData> stBinlogDataPtr;


/**
 * 同步BinLog的线程类
 */
class MKBinLogThread
{
public:
    MKBinLogThread() :_isBinlogReset(false), _isStart(false), _isRuning(false), _isInSlaveCreating(false), _syncCompress(false) {}
    ~MKBinLogThread() {}

    /*
    *初始化
    */
    void init(const string &sConf);

    /*
    *重载配置
    */
    void reload();

    /*
    *生成线程
    */
    void createThread();

    /*
    *获取binlog线程run
    */
    static void* Run(void* arg);

    /*
    *写数据线程
    */
    static void* WriteData(void* arg);

    /*
    *同步binlog
    */
    int syncBinLog();

    /*
    *同步经过压缩的binlog
    */
    int syncCompress();

    /*
    *停止线程
    */
    void stop()
    {
        _isStart = false;
    }

    bool isStart()
    {
        return _isStart;
    }

    void setStart(bool bStart)
    {
        _isStart = bStart;
    }

    bool isRuning()
    {
        return _isRuning;
    }

    void setRuning(bool bRuning)
    {
        _isRuning = bRuning;
    }

    bool isInSlaveCreatingStatus()
    {
        return _isInSlaveCreating;
    }
protected:
    /*
    *Get binlog 日志同步点
    */
    void getSyncPiont(bool aferSwitch = false);

    /*
    *save binlog 日志同步点
    */
    void saveSyncPiont(const string &logFile, const uint64_t seek);

    /*
    *获取主机地址
    */
    string getBakSourceAddr();

    /*
    *获取主机ServerName
    */
    string getServerName();

    /*
    * 写binlog
    */
    void wirteBinLog(const vector<string>& logContent);

    /*
    * 校验主机返回的同步点
    */
    bool checkSyncPoint(const string & strSyncPoint);

    /*
    * 重置binlog缓存
    */
    void resetBuff(const string &logFile, const uint64_t seek);

protected:
    //当前同步binlog文件名
    string _syncBinlogFile;

    //当前同步binlog日志文件的偏移量
    uint64_t _seek;

    //重置同步点标志
    bool _isBinlogReset;

    //线程启动停止标志
    bool _isStart;

    //线程当前状态
    bool _isRuning;

    //线程是否处于备件数据自建状态
    bool _isInSlaveCreating;

    MKBinLogPrx _binLogPrx;
    TC_Config _tcConf;
    string _config;

    //每次同步binlog的行数
    uint32_t _maxSyncLine;

    //binlog日志文件后缀名
    string _binlogFile;
    string _keyBinlogFile;

    bool _insertAtHead;
    bool _updateInOrder;

    bool _existDB;

    //选择是同步压缩binlog还是非压缩的
    volatile bool _syncCompress;

    bool _isAferSwicth;

    bool _isKeySyncMode;

    bool _recordBinLog;

    //某个主key下最大限数据量,=0时无限制，只有在大于0且无db时有效
    size_t _mkeyMaxDataCount;
    //限制主key下最大限数据量功能开启时，是否删除脏数据
    bool _deleteDirty;

    //binlog数据缓存大小
    unsigned int _binlogQueueSize;

    TC_ThreadLock _lock;
    //用于保存获取的binglog数据的fifo队列
    queue<stBinlogDataPtr> _binlogDataQueue;
};

#endif

