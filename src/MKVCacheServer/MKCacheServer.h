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
#ifndef _CacheServer_H_
#define _CacheServer_H_

#include <iostream>
#include "servant/Application.h"
#include "UnpackTable.h"
#include "DbAccess.h"
#include "MKBinLogThread.h"
#include "TimerThread.h"
#include "EraseThread.h"
#include "SyncThread.h"
#include "SyncAllThread.h"
#include "ExpireThread.h"
#include "DeleteThread.h"
#include "MKBinLogEncode.h"
#include "MKHash.h"
#include "MKCacheGlobe.h"
#include "MKBinLogTimeThread.h"
#include "RouterHandle.h"
#include "SlaveCreateThread.h"
#include "DumpThread.h"
#include "Config.h"

using namespace std;
using namespace tars;
using namespace DCache;


/*
*MKCacheToDoFunctor类从MultiHashMap的ToDoFunctor的类继承，
*用于HashMap做不同操作时完成相应的自定义操作,实现了del、sync和erase三个成员函数
*del：调用DbAccess对数据库中的数据进行删除
*sync：回写脏数据，如果回写失败将该数据重新设为脏数据等待下次回写
*erase：当hashmap erase数据时，如果erase的数据是脏数据，则讲该数据做为脏数据重新写会MultiHashMap
*/

class MKCacheToDoFunctor : public MultiHashMap::CacheToDoFunctor
{
public:
    void init(const string& sConf);
    void reload();

    virtual void del(bool bExists, const MKCacheToDoFunctor::DataRecord &data);

    virtual void sync(const MKCacheToDoFunctor::DataRecord &data);

    virtual void erase(const MKCacheToDoFunctor::DataRecord &stDataRecord);

    virtual void eraseRadio(const MKCacheToDoFunctor::DataRecord &stDataRecord);

    bool isAllSync();

    bool haveSyncKeyIn(unsigned int begin, unsigned int end);

protected:
    DbAccessPrx _dbaccessPrx;
    string _config;
    string _binlogFile;
    string _keyBinlogFile;
    bool _recordBinLog;
    bool _recordKeyBinLog;
    string _dbDayLog;
    bool _existDB;
    bool _insertAtHead;
    bool _updateInOrder;

    TC_Multi_HashMap_Malloc::MainKey::KEYTYPE _storeKeyType;

    TC_ThreadLock _lock;
    set<string> _syncMKey;
    size_t _syncCount;

    FieldConf _fieldConf;
};

class EraseDataInPageFunctor
{
public:
    EraseDataInPageFunctor() {}

    ~EraseDataInPageFunctor() {}

    void init(TC_Config& conf);

    void reload(TC_Config& conf);

    void operator()(int pageNoStart, int pageNoEnd);

private:

    unsigned int _pageSize;
    string _binlogFile;
    string _keyBinlogFile;
    bool _recordBinLog;
    bool _recordKeyBinLog;
};

//统计未访问数据线程
class CalculateThread
{
public:
    CalculateThread() :_runNow(false), _isRunning(false), _startTime(time(NULL)), _enable(true), _resetCycleDays(7), _reportCnt(0) {}
    /*
    *生成线程
    */
    void createThread();

    /*
    *线程run
    */
    static void* Run(void* arg);

    void runImmediately(bool b)
    {
        _runNow = b;
    }

    bool _runNow;

    bool _isRunning;

    time_t _startTime;


    bool _enable;

    int _resetCycleDays;

    int _reportCnt;
};

/**
 *
 **/
class MKCacheServer : public Application
{
public:
    /**
     *构析函数
     **/
    virtual ~MKCacheServer() {};

    /**
     *初始化函数
     **/
    virtual void initialize();

    /**
     *销毁函数
     **/
    virtual void destroyApp();

    /*
    *通过admin端口显示帮助信息
    *	command: 命令字为 "help"
    *	params:	空
    *	result:	帮助信息
    */
    bool help(const string& command, const string& params, string& result);

    /**
    *通过admin端口显示CacheServer的状态信息
    *	command: 命令字为 "status"
    *	params:	空
    *	result:	格式化厚的CacheServer的状态信息
    */
    bool showStatus(const string& command, const string& params, string& result);

    /**
    *通过admin端口显示CacheServer的状态信息,输出hash比例情况
    *	command: 命令字为 "statushash"
    *	params:	空
    *	result:	格式化厚的CacheServer的状态信息
    */
    bool showStatusWithHash(const string& command, const string& params, string& result);

    /**
    *通过admin端口回写Cache的所有脏数据
    *	command: 命令字为 "syncdata"
    *	params:	空
    *	result:	操作结果
    */
    bool syncData(const string& command, const string& params, string& result);

    /**
    * 生成脏数据数量
    */
    bool dirtyStatic(const string& command, const string& params, string& result);

    /**
    *通过admin端口返回Cache内存使用状况，用于内存监控
    *	command: 命令字为 "cachestatus"
    *	params:	空
    *	result:	总内存大小|总chunk数量|已使用chunk数量|数据量|脏数据量
    */
    bool cacheStatus(const string& command, const string& params, string& result);

    /**
    *通过admin端口返回binlog同步状况，用于内存监控
    *	command: 命令字为 "binlogstatus"
    *	params:	空
    *	result:	binlog同步时间点|最近binlog时间点
    */
    bool binlogStatus(const string& command, const string& params, string& result);

    /**
    *通过admin端口重载配置
    *	command: 命令字为 "reload"
    *	params:	空
    *	result:	操作结果
    */
    bool reloadConf(const string& command, const string& params, string& result);

    /**
    *通过admin端口查看版本
    *   command: 命令字为 "ver"
    *	params:	空
    *	result:	操作结果
    */
    bool showVer(const string& command, const string& params, string& result);

    /**
    *通过admin端口删除指定页范围内的数据
    *   command: 命令字为 "erasedatainpage"
    *   params:	起始页 结束页
    *	result:	操作结果
    */
    bool eraseDataInPage(const string& command, const string& params, string& result);

    /*
    	删除不属于此cacheserver的页
    */
    bool eraseWrongRouterData(const string& command, const string& params, string& result);

    /**
    *统计一段时间未被访问数据的总大小
    */
    bool calculateData(const string& command, const string& params, string& result);

    /**
    *复位统计指针，从get链头部重新开始统计
    */
    bool resetCalculatePoint(const string& command, const string& params, string& result);

    /**
    *通过admin端口获取信号量或者共享内存的key
    *   command: 命令字为 "key"
    *   params:	空
    *	result:	操作结果
    */
    bool showKey(const string& command, const string& params, string& result);

    /**
    *通过admin端口查看服务状态
    *   command: 命令字为 "servertype"
    *	params:	空
    *	result:	操作结果
    */
    bool showServerType(const string& command, const string& params, string& result);

    /**
    *通过admin端口将全部数据设置成脏数据
    *   command: 命令字
    *   params:	身份鉴权信息
    *	result:	操作结果
    */
    bool setAllDirty(const string& command, const string& params, string& result);

    /**
    *通过admin端口将内存清空
    *   command: 命令字
    *   params:	身份鉴权信息
    *	result:	操作结果
    */
    bool clearCache(const string& command, const string& params, string& result);

    /**
     * 判断所有线程是否处于备机自建数据状态
     *
     * @return bool
     */
    bool isAllInSlaveCreatingStatus();

    /**
     * 是否有主key在做删除操作
     *
     * @return bool
     */
    bool haveDeleteKeyIn(unsigned int begin, unsigned int end);

    /**
     * 插入需要做删除操作的主key
     *
     * @return int
     */
    int insertDeleteKey(string &sKey);

    /**
     * 删除需要做删除操作的主key
     *
     * @return int
     */
    int earseDeleteKey(string &sKey);

    void ppReport(enum PPReport::PropertyType type, int value);

    bool isAllSync();

    bool haveSyncKeyIn(unsigned int begin, unsigned int end);

    GlobalStat* gstat();

    //主机回写脏数据时间点
    int getSyncTime();

    DumpThread* dumpThread();

    SlaveCreateThread* slaveCreateThread();

    void enableConnectHb(bool enable)
    {
        _heartBeatThread.enableConHb(enable);
    }
protected:
    /*加载配置文件，覆盖Applicatin的addconfig方法
    *
    * @param filename
    *
    * @return bool
    */
    bool addConfig(const string &filename);

    /*
    *读取ConfigServer上配置文件到本地，并备份原文件
    * @param  fileName 		  文件名称
    * @param  result          结果
    * @param  bAppConfigOnly  是否只获取应用级别的配置
    *
    * @return bool
    */
    bool pullConfig(const string & fileName, string &result, bool bAppConfigOnly = false, int _iMaxBakNum = 5);

    /**
    * 备份文件名称 Config.conf.1.bak,Config.conf.2.bak ...
    * 该方法提供下标到文件名的转化
    *
    * @param index         第几个备份文件
    *
    * @return string       配置文件全路径
    */
    inline string index2file(const string & sFullFileName, int index);

    /**
    *  rename系统操作的封装，当oldFile不存在时抛出异常
    *
    * @param oldFile   原文件路径和名称
    * @param newFile   新文件逻辑和名称
    */
    inline void localRename(const string& oldFile, const string& newFile);

    /*
    *实现请求ConfigServer并将结果以文件形式保存到本地目录
    *  	@param  fileName 	   文件名称
    *	@param  bAppConfigOnly 是否只获取应用级别的配置
    *
    *	@return string         生成的文件名称
    */
    string getRemoteFile(const string &fileName, bool bAppConfigOnly);

    /**
    *Get服务状态，MASTER or SLAVE
    */
    enum ServerType getServerType();

    /**
    *格式化ServerInfo信息
    */
    string formatServerInfo(ServerInfo &serverInfo);

    /**
    *格式化TC_HashMap的head信息
    */
    string formatCacheHead(vector<TC_Multi_HashMap_Malloc::tagMapHead>& vHead);
protected:
    TC_Config _tcConf;

    //DCache ConfigServer代理
    DCache::ConfigPrx _configPrx;

    //路由信息处理类
    RouterHandle* _routerHandle;

    //同步binlog的线程类
    MKBinLogThread _syncBinlogThread;

    //获取备份源最后记录binlog时间的定时类
    MKBinLogTimeThread _binlogTimeThread;

    //定时作业线程类
    TimerThread _timerThread;

    //定时生成binlog文件线程
    CreateBinlogFileThread _createBinlogFileThread;

    //统计线程
    DirtyStatisticThread _dirtyStatisticThread;
    DirtyStatisticNowThread _dirtyStatisticNowThread;

    EraseThread _eraseThread;

    //回写脏数据线程类
    SyncThread _syncThread;

    //回写所有脏数据线程类
    SyncAllThread _syncAllThread;

    //清除过期数据线程类
    ExpireThread _expireThread;

    //删除数据线程
    DeleteThread _deleteThread;

    //统计未访问数据线程
    CalculateThread _clodDataCntingThread;

    //定时上报心跳线程
    HeartBeatThread _heartBeatThread;

    //dump线程
    DumpThread _dumpThread;

    //恢复线程
    SlaveCreateThread _slaveCreateThread;

    //线程池，用来执行后台操作
    TC_ThreadPool _tpool;

    EraseDataInPageFunctor _eraseDataInPageFunc;

    MKCacheToDoFunctor _todoFunctor;

    PPReport _ppReport;

    GlobalStat _gStat;

    // 信号量或者共享内存key
    string _shmKey;

    //做删除操作的主key集合
    TC_ThreadLock _deleteLock;
    set<string> _deleteMKey;

};

extern MKCacheServer g_app;

////////////////////////////////////////////
#endif
