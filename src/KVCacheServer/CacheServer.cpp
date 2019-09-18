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
#include <sys/resource.h>
#include "CacheServer.h"
#include "CacheImp.h"
#include "WCacheImp.h"
#include "BinLogImp.h"
#include "RouterClientImp.h"
#include "BackUpImp.h"
#include "ControlAckImp.h"
#include "RouterHandle.h"

CacheServer g_app;

//本地路由表
UnpackTable g_route_table;

//保存key为string的hashmap
SHashMap g_sHashMap;

void CacheStringToDoFunctor::init(const string& sConf)
{
    _configFile = sConf;
    TC_Config conf;
    conf.parseFile(sConf);

    string sRDBaccessObj = conf["/Main/DbAccess<ObjName>"];
    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(sRDBaccessObj);

    _binlogFile = conf["/Main/BinLog<LogFile>"];

    _keyBinlogFile = conf["/Main/BinLog<LogFile>"] + "key";
    _dbDayLog = conf["/Main/Log<DbDayLog>"];

    _hasDb = conf["/Main/DbAccess<DBFlag>"] == "Y" ? true : false;

    string sRecordBinLog = conf.get("/Main/BinLog<Record>", "Y");
    _isRecordBinlog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = conf.get("/Main/BinLog<KeyRecord>", "N");
    _isRecordKeyBinlog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;
}

void CacheStringToDoFunctor::reload()
{
    TC_Config conf;
    conf.parseFile(_configFile);
    string sRDBaccessObj = conf["/Main/DbAccess<ObjName>"];
    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(sRDBaccessObj);

    _binlogFile = conf["/Main/BinLog<LogFile>"];
    _keyBinlogFile = conf["/Main/BinLog<LogFile>"] + "key";
    _dbDayLog = conf["/Main/Log<DbDayLog>"];
    _hasDb = conf["/Main/DbAccess<DBFlag>"] == "Y" ? true : false;

    string sRecordBinLog = conf.get("/Main/BinLog<Record>", "Y");
    _isRecordBinlog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = conf.get("/Main/BinLog<KeyRecord>", "N");
    _isRecordKeyBinlog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;
}

void CacheStringToDoFunctor::del(bool bExists, const CacheStringToDoFunctor::DataRecord &data)
{
    if ((g_app.gstat()->serverType() == MASTER) && _hasDb)
    {
        int iRet = 0;
        try
        {
            iRet = _dbaccessPrx->del(data._key);
            if (iRet != eDbSucc && iRet != eDbRecordNotExist)
            {
                FDLOG(_dbDayLog) << "del|" << data._key << "|Err|" << iRet << endl;
                TLOGERROR("CacheStringToDoFunctor::del del error, key = " << data._key << ", iRet = " << iRet << endl);
                g_app.ppReport(PPReport::SRP_DB_ERR, 1);
                throw runtime_error("CacheStringToDoFunctor::del del error");
            }
            FDLOG(_dbDayLog) << "del|" << data._key << "|Succ|" << iRet << endl;
        }
        catch (const TarsException & ex)
        {
            FDLOG(_dbDayLog) << "del|" << data._key << "|Err|" << ex.what() << endl;
            TLOGERROR("CacheStringToDoFunctor::del del exception: " << ex.what() << ", key = " << data._key << endl);
            g_app.ppReport(PPReport::SRP_DB_EX, 1);
            throw runtime_error("CacheStringToDoFunctor::del del error");
        }
    }
}

void CacheStringToDoFunctor::sync(const CacheStringToDoFunctor::DataRecord &data)
{
    {
        TC_ThreadLock::Lock lock(_lock);
        _syncKeys.insert(data._key);
        ++_syncCnt;
    }

    try {
        if (g_route_table.isTransfering(data._key))
        {
            if (g_app.gstat()->serverType() != MASTER || !g_route_table.isMySelf(data._key))
            {
                TC_ThreadLock::Lock lock(_lock);
                _syncKeys.erase(data._key);
                return;
            }

            //检查该key数据是否是脏数据，如果是脏数据表示数据已经变更过了，不能再把回写失败的数据set回去
            int iRetCheck = g_sHashMap.checkDirty(data._key);

            if (iRetCheck != TC_HashMapMalloc::RT_DIRTY_DATA)
            {
                int iSetRet = g_sHashMap.set(data._key, data._value, true, data._expiret, data._ver);

                if (iSetRet == TC_HashMapMalloc::RT_OK)
                {
                    if (_isRecordBinlog)
                    {
                        TBinLogEncode logEncode;
                        CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, true, data._key, data._value, data._expiret) + "\n", _binlogFile);
                        if (g_app.gstat()->serverType() == MASTER)
                            g_app.gstat()->setBinlogTime(0, TNOW);
                    }
                    if (_isRecordKeyBinlog)
                    {
                        TBinLogEncode logEncode;
                        CacheServer::WriteToFile(logEncode.EncodeSetKey(data._key) + "\n", _keyBinlogFile);
                        if (g_app.gstat()->serverType() == MASTER)
                            g_app.gstat()->setBinlogTime(0, TNOW);
                    }
                }
                else
                {
                    TLOGERROR("CacheStringToDoFunctor::sync reset dirty data, key = " << data._key << ", error:" << iSetRet << endl);
                    if (iSetRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                    {
                        g_app.ppReport(PPReport::SRP_EX, 1);
                    }
                }
            }
        }
        else if (g_app.gstat()->serverType() == MASTER  && _hasDb && g_route_table.isMySelf(data._key))
        {
            int iRet = 0;
            bool bEx = false;
            try
            {
                iRet = setDb(data);

                if (iRet != eDbSucc)
                {
                    TLOGERROR("CacheStringToDoFunctor::sync error, ret = " << iRet << ", key = " << data._key << ", setDb again" << endl);
                    iRet = setDb(data);
                    if (iRet != eDbSucc)
                    {
                        TLOGERROR("CacheStringToDoFunctor::sync error, ret = " << iRet << ", key = " << data._key << endl);
                        FDLOG(_dbDayLog) << "set|" << data._key << "|Err|" << iRet << endl;
                        g_app.ppReport(PPReport::SRP_DB_ERR, 1);
                    }
                    else
                    {
                        FDLOG(_dbDayLog) << "set|" << data._key << "|Succ|" << iRet << endl;
                    }
                }
                else
                {
                    FDLOG(_dbDayLog) << "set|" << data._key << "|Succ|" << iRet << endl;
                }
            }
            catch (const TarsException & ex)
            {
                TLOGDEBUG("CacheStringToDoFunctor::sync exception, setDb again, key = " << data._key << endl);
                try
                {
                    iRet = setDb(data);

                    if (iRet != eDbSucc)
                    {
                        TLOGERROR("CacheStringToDoFunctor::sync error, ret = " << iRet << ", key = " << data._key << endl);
                        FDLOG(_dbDayLog) << "set|" << data._key << "|Err|" << iRet << endl;
                        g_app.ppReport(PPReport::SRP_DB_ERR, 1);
                    }
                    else
                    {
                        FDLOG(_dbDayLog) << "set|" << data._key << "|Succ|" << iRet << endl;
                    }
                }
                catch (const TarsException & ex)
                {
                    bEx = true;
                    FDLOG(_dbDayLog) << "set|" << data._key << "|Err|" << ex.what() << endl;
                    TLOGERROR("CacheStringToDoFunctor::sync setString exception: " << ex.what() << ", key = " << data._key << endl);
                    try
                    {
                        g_app.ppReport(PPReport::SRP_DB_EX, 1);
                    }
                    catch (const std::exception & ex)
                    {
                        TLOGDEBUG("g_srp_dbex ex:" << ex.what() << endl);
                    }
                    catch (...)
                    {
                        TLOGDEBUG("g_srp_dbex unkown ex" << endl);
                    }

                }
            }
            if (iRet != eDbSucc || bEx)
            {
                //检查该key数据是否是脏数据，如果是脏数据表示数据已经变更过了，不能再把回写失败的数据set回去
                int iRetCheck = g_sHashMap.checkDirty(data._key);

                if (iRetCheck != TC_HashMapMalloc::RT_DIRTY_DATA)
                {
                    int iSetRet = g_sHashMap.set(data._key, data._value, true, data._expiret, data._ver);

                    if (iSetRet == TC_HashMapMalloc::RT_OK)
                    {
                        if (_isRecordBinlog)
                        {
                            TBinLogEncode logEncode;
                            CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, true, data._key, data._value, data._expiret) + "\n", _binlogFile);
                            if (g_app.gstat()->serverType() == MASTER)
                                g_app.gstat()->setBinlogTime(0, TNOW);
                        }
                        if (_isRecordKeyBinlog)
                        {
                            TBinLogEncode logEncode;
                            CacheServer::WriteToFile(logEncode.EncodeSetKey(data._key) + "\n", _keyBinlogFile);
                            if (g_app.gstat()->serverType() == MASTER)
                                g_app.gstat()->setBinlogTime(0, TNOW);
                        }
                    }
                    else
                    {
                        TLOGERROR("CacheStringToDoFunctor::sync reset dirty data, key = " << data._key << ", error:" << iSetRet << endl);
                        if (iSetRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
                        {
                            g_app.ppReport(PPReport::SRP_EX, 1);
                        }
                    }
                }
            }
        }
    }
    catch (exception& e)
    {
        TLOGERROR("CacheStringToDoFunctor::sync exception: " << e.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("CacheStringToDoFunctor::sync unknown exception" << endl);
    }

    TC_ThreadLock::Lock lock(_lock);
    _syncKeys.erase(data._key);
}

void CacheStringToDoFunctor::erase(const CacheStringToDoFunctor::DataRecord &data)
{
    if (data._dirty)
    {
        int iRet = g_sHashMap.set(data._key, data._value, true, data._expiret, 1);

        if (iRet == TC_HashMapMalloc::RT_OK)
        {
            if (_isRecordBinlog)
            {
                TBinLogEncode logEncode;
                CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, true, data._key, data._value, data._expiret) + "\n", _binlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
            if (_isRecordKeyBinlog)
            {
                TBinLogEncode logEncode;
                CacheServer::WriteToFile(logEncode.EncodeSetKey(data._key) + "\n", _keyBinlogFile);
                if (g_app.gstat()->serverType() == MASTER)
                    g_app.gstat()->setBinlogTime(0, TNOW);
            }
            TLOGDEBUG("CacheStringToDoFunctor::erase reset dirty data, key = " << data._key << endl);
        }
        else
        {
            TLOGERROR("CacheStringToDoFunctor::erase reset dirty data, key = " << data._key << ", error:" << iRet << endl);
            if (iRet != TC_HashMapMalloc::RT_DATA_VER_MISMATCH)
            {
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}
void CacheStringToDoFunctor::eraseRadio(const CacheStringToDoFunctor::DataRecord &data)
{
    g_app.ppReport(PPReport::SRP_ERASE_CNT, 1);
    if (data._expiret > TC_TimeProvider::getInstance()->getNow())
    {
        g_app.ppReport(PPReport::SRP_ERASE_UNEXPIRE_CNT, 1);
    }
}
bool CacheStringToDoFunctor::isAllSync()
{
    TC_ThreadLock::Lock lock(_lock);

    CanSync& canSync = g_app.gstat()->getCanSync();
    size_t iCanSyncCount = canSync.getCanSyncCount();

    return (_syncCnt == iCanSyncCount);
}

bool CacheStringToDoFunctor::haveSyncKeyIn(unsigned int hashBegin, unsigned int hashEnd)
{
    TC_ThreadLock::Lock lock(_lock);

    set<string>::const_iterator it = _syncKeys.begin(), itEnd = _syncKeys.end();
    while (it != itEnd)
    {
        unsigned int hash = g_route_table.hashKey(*it);
        if (hash >= hashBegin && hash <= hashEnd)
            return true;
        else
            ++it;
    }

    return false;
}

int CacheStringToDoFunctor::setDb(const CacheStringToDoFunctor::DataRecord &data)
{
    int iRet;
    time_t iExpireTime = data._expiret; //@2016.3.21
    iRet = _dbaccessPrx->set(data._key, data._value, iExpireTime);

    return iRet;
}

/////////////////////////////////////////////////////////////////

void EraseDataInPageFunctor::init(TC_Config& conf)
{
    _pageSize = TC_Common::strto<unsigned int>(conf["/Main/Router<PageSize>"]);

    _binlogFile = conf["/Main/BinLog<LogFile>"];

    _keyBinlogFile = conf["/Main/BinLog<LogFile>"] + "key";

    string sRecordBinLog = conf.get("/Main/BinLog<Record>", "Y");
    _isRecordBinlog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = conf.get("/Main/BinLog<KeyRecord>", "N");
    _isRecordKeyBinlog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;
}

void EraseDataInPageFunctor::reload(TC_Config& conf)
{
    string sRecordBinLog = conf.get("/Main/BinLog<Record>", "Y");
    _isRecordBinlog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = conf.get("/Main/BinLog<KeyRecord>", "N");
    _isRecordKeyBinlog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;
}

void EraseDataInPageFunctor::operator()(int pageNoStart, int pageNoEnd)
{
    uint32_t iMaxPageNo = UnpackTable::__allPageCount - 1;
    if (pageNoStart < 0 || (uint32_t)pageNoStart > iMaxPageNo)
    {
        TLOGERROR("[EraseDataInPageFunctor::operator] start page error, " << pageNoStart << " not in [0," << iMaxPageNo << "]" << endl);
        return;
    }

    if (pageNoEnd <0 || (uint32_t)pageNoEnd > iMaxPageNo)
    {
        TLOGERROR("[EraseDataInPageFunctor::operator] end page error, " << pageNoEnd << " not in [0," << iMaxPageNo << "]" << endl);
        return;
    }

    if (pageNoEnd < pageNoStart)
    {
        TLOGERROR("[EraseDataInPageFunctor::operator] pageNoStart is greater than pageNoEnd." << endl);
        return;
    }

    int iPageNo;
    for (iPageNo = pageNoStart; iPageNo <= pageNoEnd; ++iPageNo)
    {
        unsigned int uBegin = (unsigned int)iPageNo*_pageSize;
        unsigned int uEnd = uBegin + _pageSize - 1;
        if (uEnd < uBegin)
        {
            uEnd = UnpackTable::__max;
        }
        int iRet;
        bool bFinish = false;

        size_t i = uBegin;
        while (true)
        {
            KeyMatch k;
            vector<string> vDelKey;
            vDelKey.clear();

            if (g_app.gstat()->serverType() != MASTER)
            {
                string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
                TLOGERROR("[EraseDataInPageFunctor::operator] " << sServerName << " is not MASTER, so cannot continue" << endl);
                break;
            }

            if (g_route_table.isMySelfByHash(i))
            {
                TLOGERROR("[EraseDataInPageFunctor::operator] " << i << " is still in self area" << endl);
                break;
            }

            k.Set(i);
            iRet = g_sHashMap.eraseHashByForce(i, k, vDelKey);

            for (size_t j = 0; j < vDelKey.size(); ++j)
            {
                if (_isRecordBinlog)
                {
                    TBinLogEncode logEncode;
                    string value;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_ERASE, true, vDelKey[j], value, BINLOG_ADMIN) + "\n", _binlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
                if (_isRecordKeyBinlog)
                {
                    TBinLogEncode logEncode;
                    string value;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_ERASE, true, vDelKey[j], value, BINLOG_ADMIN) + "\n", _keyBinlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
            }

            if (iRet != TC_HashMapMalloc::RT_OK)
            {
                TLOGERROR("[EraseDataInPageFunctor::operator] hashmap eraseHashByForce error, hash = " << i << ", iRet = " << iRet << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                break;
            }

            if (i == uEnd)
            {
                bFinish = true;
                break;
            }

            ++i;
        }

        if (!bFinish)
        {
            TLOGERROR("[EraseDataInPageFunctor::operator] erase data in page: " << iPageNo << " failed" << endl);
            break;
        }
        TLOGDEBUG("[EraseDataInPageFunctor::operator] erase data in page: " << iPageNo << " succ" << endl);

        usleep(1000 * 10);
    }

    if (iPageNo > pageNoEnd)
    {
        TLOGDEBUG("[EraseDataInPageFunctor::operator] erase data in [" << pageNoStart << "," << pageNoEnd << "] succ" << endl);
    }
}
////////////////////////////////////////////////////////////////
void CalculateThread::createThread()
{
    if (_isRunning)
        return;

    //创建线程
    pthread_t thread;

    if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
    {
        TLOGERROR("[CalculateThread::createThread] Create SyncAllThread fail" << endl);
    }
}

void* CalculateThread::Run(void* arg)
{
    while (true)
    {
        CalculateThread *t = (CalculateThread*)arg;

        if (!t->_enable)
        {
            sleep(300);
            continue;
        }

        if (!t->_runNow)
        {
            //当前时间
            time_t tNow = time(NULL);
            struct tm tm_now;
            localtime_r(&tNow, &tm_now);

            char now_str[31] = "\0";
            strftime(now_str, 31, "%d,%H,%M", &tm_now);
            string sNow(now_str);
            //TLOGDEBUG("CalculateThread::Run, sNow:" << sNow << endl);
            vector<string> vNow = TC_Common::sepstr<string>(sNow, ",");
            if (vNow.size() != 3)
            {
                TLOGERROR("CalculateThread::Run, vNow size error, sNow" << sNow << endl);
                FDLOG("unread") << "CalculateThread::Run, vNow size error, sNow" << sNow << endl;
                sleep(300);
                continue;
            }

            //上一次统计的时间
            struct tm tm_start;
            localtime_r(&(t->_startTime), &tm_start);

            char start_str[31] = "\0";
            strftime(start_str, 31, "%d,%H,%M", &tm_start);
            string sStart(start_str);
            //TLOGDEBUG("CalculateThread::Run, sStart:" << sStart << endl);
            vector<string> vStart = TC_Common::sepstr<string>(sStart, ",");
            if (vStart.size() != 3)
            {
                TLOGERROR("CalculateThread::Run, vStart size error, sStart" << sStart << endl);
                FDLOG("unread") << "CalculateThread::Run, vStart size error, sStart" << sStart << endl;
                sleep(300);
                continue;
            }

            // same day
            if (vNow[0] == vStart[0])
            {
                sleep(300);
                continue;
            }
            else
            {
                //different day, do statistic at 2~3 clock
                int randMinute = rand() % 60;
                int minute = 60 * (TC_Common::strto<Int32>(vNow[1])) + TC_Common::strto<Int32>(vNow[2]);
                if (minute < (2 * 60 + randMinute))
                {
                    sleep(300);
                    continue;
                }
            }

        }

        t->_isRunning = true;

        string result;
        uint64_t count = 0;
        //默认是10个内存块
        for (int i = 0; i < 10; i++)
        {
            bool isEnd = false;
            while (!isEnd)
            {
                uint32_t tmp;
                g_sHashMap.calculateData(i, tmp, isEnd);
                count += tmp;
            }
        }

        size_t totalMemSize = g_sHashMap.getMemSize();
        size_t totalDataMemSize = g_sHashMap.getDataMemSize();
        size_t usedDataMemSize = g_sHashMap.getUsedDataMem();
        size_t totalElementCnt = g_sHashMap.getTotalElementCount();

        size_t unreadRatio = 100;

        //根据内存大小统计
        //if (totalDataMemSize > 0)
        //{
        //size_t unreadRatio = (totalDataMemSize - usedDataMemSize + count) * 100 / totalDataMemSize;
        //}

        //根据记录条数进行统计，1 - ( usedDataMemSize * (1 - count / totalElementCnt) ) / totalDataMemSize
        if ((totalDataMemSize > 0) && (totalElementCnt > 0))
        {
            unreadRatio = (totalDataMemSize - usedDataMemSize + (usedDataMemSize*count / totalElementCnt)) * 100 / totalDataMemSize;
        }
        g_app.ppReport(PPReport::SRP_COLD_RATIO, unreadRatio);
        //result = "unread:" + TC_Common::tostr(count) + " usedDataMem:" + TC_Common::tostr(usedDataMemSize) + " totalDataMem:" + TC_Common::tostr(totalDataMemSize) +
        //	" totalMem:" + TC_Common::tostr(totalMemSize) + " unuseMemPercent:" + TC_Common::tostr(unreadRatio) + "% startTime:" + TC_Common::tostr(t->iStartTime_);
        result = "unread:" + TC_Common::tostr(count) + " totalElementCnt:" + TC_Common::tostr(totalElementCnt) +
                 " usedDataMem:" + TC_Common::tostr(usedDataMemSize) + " totalDataMem:" + TC_Common::tostr(totalDataMemSize) +
                 " totalMem:" + TC_Common::tostr(totalMemSize) + " unuseMemPercent:" + TC_Common::tostr(unreadRatio) +
                 "% startTime:" + TC_Common::tostr(t->_startTime);
        FDLOG("unread") << result << endl;
        TLOGERROR(result << endl);

        t->_startTime = time(NULL);

        t->_isRunning = false;

        t->_runNow = false;

        t->_reportCnt++;
        if (t->_reportCnt >= t->_resetCycleDays)
        {
            g_sHashMap.resetCalculateData();
            t->_reportCnt = 0;
            TLOGDEBUG("CalculateThread::Run, cycle reset!" << endl);
            FDLOG("unread") << "CalculateThread::Run, cycle reset!" << endl;
        }
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////
void CacheServer::initialize()
{
    //initialize application here:
    //...

    //禁止写远程按天日志
    TarsTimeLogger::getInstance()->enableRemote("", false);
    TarsTimeLogger::getInstance()->initFormat("", "%Y%m%d%H");

    _configPrx = Application::getCommunicator()->stringToProxy<DCache::ConfigPrx>("DCache.ConfigServer.ConfigObj");
    addConfig("CacheServer.conf");
    _tcConf.parseFile(ServerConfig::BasePath + "CacheServer.conf");

    addServant<RouterClientImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".RouterClientObj");
    addServant<BackUpImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".BackUpObj");
    addServant<CacheImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".CacheObj");
    addServant<WCacheImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".WCacheObj");
    addServant<BinLogImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".BinLogObj");
    addServant<ControlAckImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".ControlAckObj");

    TARS_ADD_ADMIN_CMD_NORMAL("help", CacheServer::help);
    TARS_ADD_ADMIN_CMD_NORMAL("status", CacheServer::showStatus);
    TARS_ADD_ADMIN_CMD_NORMAL("detailstatus", CacheServer::showStatusDetail);
    TARS_ADD_ADMIN_CMD_NORMAL("syncdata", CacheServer::syncData);
    TARS_ADD_ADMIN_CMD_NORMAL("reload", CacheServer::reloadConf);
    TARS_ADD_ADMIN_CMD_NORMAL("cachestatus", CacheServer::cacheStatus);
    TARS_ADD_ADMIN_CMD_NORMAL("binlogstatus", CacheServer::binlogStatus);
    TARS_ADD_ADMIN_CMD_NORMAL("ver", CacheServer::showVer);
    TARS_ADD_ADMIN_CMD_NORMAL("erasedatainpage", CacheServer::eraseDataInPage);
    TARS_ADD_ADMIN_CMD_NORMAL("erasewrongrouterdata", CacheServer::eraseWrongRouterData);
    TARS_ADD_ADMIN_CMD_NORMAL("calculateData", CacheServer::calculateData);
    TARS_ADD_ADMIN_CMD_NORMAL("resetCalculatePoint", CacheServer::resetCalculatePoint);
    TARS_ADD_ADMIN_CMD_NORMAL("setalldirty", CacheServer::setAllDirty);
    TARS_ADD_ADMIN_CMD_NORMAL("clearcache", CacheServer::clearCache);
    TARS_ADD_ADMIN_CMD_NORMAL("key", CacheServer::showKey);


    int iRet = _ppReport.init();
    assert(iRet == 0);

    iRet = _gStat.init();
    assert(iRet == 0);

    // 设置允许产生coredump的大小
    struct rlimit rl;
    getrlimit(RLIMIT_CORE, &rl);
    size_t iOldLimit = rl.rlim_cur;
    string sCoreLimit = _tcConf.get("/Main<CoreSizeLimit>", "");
    if (sCoreLimit == "-1")
    {
        rl.rlim_cur = rl.rlim_max;
    }
    else
    {
        rl.rlim_cur = TC_Common::toSize(sCoreLimit, 0);
    }
    iRet = setrlimit(RLIMIT_CORE, &rl);
    if (iRet != 0)
    {
        TLOGERROR("CacheServer::initialize set coredump limit fail, errno=" << errno << ", old rlim_cur:"
                     << iOldLimit << ", rlim_max:" << rl.rlim_max << ", try set rlim_cur:" << rl.rlim_cur << endl);
    }

    //初始化RouterHandle
    RouterHandle::getInstance()->init(_tcConf);

    //初始化路由
    bool bSyncFromRouter = true;
    if (RouterHandle::getInstance()->initRoute(false, bSyncFromRouter) != 0)
    {
        TLOGERROR("CacheServer::initialize init route failed" << endl);
        assert(false);
    }

    //路由初始化完成后，获取groupName
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
    GroupInfo groupinfo;
    g_route_table.getGroup(sServerName, groupinfo);
    _gStat.setGroupName(groupinfo.groupName);
    TLOGDEBUG("CacheServer::initialize groupName:" << groupinfo.groupName << endl);

    //hashmap初始化
    NormalHash *pHash = new NormalHash();
    typedef size_t(NormalHash::*TpMem)(const string &);

    bool bCreate = false;
    string sSemKeyFile = ServerConfig::DataPath + "/SemKey.dat", sSemKey = "";
    if (access(sSemKeyFile.c_str(), F_OK) != 0)
    {
        if (errno != ENOENT)
        {
            TLOGERROR("[CacheServer::initialize] access file: " << sSemKeyFile << " error, errno = " << errno << endl);
            assert(false);
        }

        bCreate = true;
    }
    else
    {
        ifstream ifs(sSemKeyFile.c_str(), ios::in | ios::binary);
        if (!ifs)
        {
            TLOGERROR("[CacheServer::initialize] open file: " << sSemKeyFile << " failed" << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
        }
        else
        {
            if (!getline(ifs, sSemKey))
            {
                if (ifs.eof())
                {
                    TLOGERROR("[CacheServer::initialize] file: " << sSemKeyFile << " is empty" << endl);
                }
                else
                {
                    TLOGERROR("[CacheServer::initialize] read file: " << sSemKeyFile << " failed" << endl);
                }
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            else
            {
                TLOGDEBUG("CacheServer::initialize get semkey from file: " << sSemKeyFile << " succ, semkey = " << sSemKey << endl);
            }

            ifs.close();
        }
    }

    key_t key;
    key = TC_Common::strto<key_t>(_tcConf.get("/Main/Cache<ShmKey>", "0"));
    if (key == 0)
        key = ftok(ServerConfig::BasePath.c_str(), 'D');

    _shmKey = TC_Common::tostr(key);

    if (!sSemKey.empty() && sSemKey != _shmKey)
    {
        TLOGERROR("[CacheServer::initialize] semkey from file != ftok/config, (" << sSemKey << " != " << key << ")" << endl);
        assert(false);
    }
    TLOGDEBUG("CacheServer::initialize semkey = " << key << endl);

    size_t n = TC_Common::toSize(_tcConf["/Main/Cache<ShmSize>"], 0);

    if (bCreate)
    {
        if (shmget(key, 0, 0) != -1 || errno != ENOENT)
        {
            TLOGERROR("[CacheServer::initialize] shmkey: " << key << " has been used, may be a conflict key" << endl);
            assert(false);
        }
    }
    else
    {
        if (sSemKey.empty()) //文件中读取的内容为空
        {
            int iShmid = shmget(key, 0, 0);
            if (iShmid == -1)
            {
                TLOGERROR("[CacheServer::initialize] file: " << sSemKeyFile << " exist and has no content, try shmget "
                        << key << " failed, errno = " << errno << endl);
                assert(false);
            }
            else
            {
                struct shmid_ds buf;
                if (shmctl(iShmid, IPC_STAT, &buf) != 0)
                {
                    TLOGERROR("[CacheServer::initialize] shmctl " << key << " failed, errno = " << errno << endl);
                    assert(false);
                }
                else if (buf.shm_nattch != 0)
                {
                    TLOGERROR("[CacheServer::initialize] number of current attach to shm: " << key << " is not zero" << endl);
                    assert(false);
                }
            }
        }
        else
        {
            int iShmid = shmget(key, 0, 0);
            if (iShmid == -1)
            {
                if (MASTER == getServerType()) //主机不允许启动，应该触发主备切换
                {
                    TLOGERROR("[CacheServer::initialize] file: " << sSemKeyFile << " exist, but shmget " << key << " failed, errno = " << errno
                             << ",master server assert." << endl);
                    assert(false);
                }
                else
                {
                    bool hasDb = _tcConf["/Main/DbAccess<DBFlag>"] == "Y" ? true : false;
                    if (hasDb)
                    {
                        TLOGERROR("[CacheServer::initialize] file: " << sSemKeyFile << " exist, but shmget " << key << " failed, errno = " << errno
                                                                     << ",allowed boot up because there is backend DB." << endl);
                    }
                    else
                    {
                        TLOGERROR("[CacheServer::initialize] file: " << sSemKeyFile << " exist, but shmget " << key << " failed, errno = " << errno
                                                                     << ",has no backend DB, assert." << endl);
                        assert(false);
                    }
                }
            }
        }
    }

    if (bCreate || sSemKey == "")
    {
        ofstream ofs(sSemKeyFile.c_str(), ios::out | ios::binary);
        if (!ofs)
        {
            TLOGERROR("[CacheServer::initialize] open file: " << sSemKeyFile << " failed, semkey can not be noted" << endl);
            if (bCreate)
            {
                assert(false);
            }
        }
        else
        {
            string sKey = TC_Common::tostr(key);
            ofs.write(sKey.c_str(), sKey.length());
            ofs.close();
        }
    }

    _todoFunctor.init(ServerConfig::BasePath + "CacheServer.conf");

    unsigned int shmNum = TC_Common::strto<unsigned int>(_tcConf.get("/Main/Cache<JmemNum>", "10"));
    g_sHashMap.init(shmNum);
    g_sHashMap.initHashRadio(atof(_tcConf["/Main/Cache<HashRadio>"].c_str()));
    g_sHashMap.initAvgDataSize(TC_Common::strto<unsigned int>(_tcConf["/Main/Cache<AvgDataSize>"]));
    g_sHashMap.initLock(key, shmNum, -1);
    TLOGDEBUG("CacheServer::initialize, initLock finish" << endl);

    g_sHashMap.initStore(key, n);

    TLOGDEBUG("CacheServer::initialize, initStore finish" << endl);
    g_sHashMap.setSyncTime(TC_Common::strto<unsigned int>(_tcConf["/Main/Cache<SyncTime>"]));
    TLOGDEBUG("g_sHashMap.setToDoFunctor" << endl);
    g_sHashMap.setToDoFunctor(&_todoFunctor);

    TC_HashMapMalloc::hash_functor cmd(pHash, static_cast<TpMem>(&NormalHash::HashRawString));
    g_sHashMap.setHashFunctor(cmd);
    g_sHashMap.setAutoErase(false);


    //生成binlog文件
    _binlogFD = -1;
    _keyBinlogFD = -1;

    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    bool m_bRecordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    bool m_bKeyRecordBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    string path = ServerConfig::LogPath + "/" + ServerConfig::Application + "/" + ServerConfig::ServerName + "/" + ServerConfig::Application + "." + ServerConfig::ServerName + "_";

    char str[128];
    struct tm ptr;
    time_t nowSec;
    string normalBinlogPath, keyBinlogPath;

    memset(str, '\0', 128);
    nowSec = time(NULL);
    ptr = *(localtime(&nowSec));
    strftime(str, 80, "%Y%m%d%H", &ptr);

    if (m_bRecordBinLog)
    {
        normalBinlogPath = path + _tcConf["/Main/BinLog<LogFile>"] + "_" + str + ".log";

        iRet = createBinLogFile(normalBinlogPath, false);
        assert(iRet == 0);
        TLOGDEBUG("[CacheServer::initialize] open file succ! " << normalBinlogPath << endl);
    }

    if (m_bKeyRecordBinLog)
    {
        keyBinlogPath = path + _tcConf["/Main/BinLog<LogFile>"] + "key_" + str + ".log";

        iRet = createBinLogFile(keyBinlogPath, true);
        assert(iRet == 0);
        TLOGDEBUG("[CacheServer::initialize] open file succ! " << keyBinlogPath << endl);
    }

    //启动定时生成binlog文件线程
    _createBinlogFileThread.createThread();


    //确定server启动模式，是master还是slave
    enum ServerType serverRole = getServerType();
    _gStat.setServerType(serverRole);

    //主机上报重启信息
    if (serverRole == MASTER)
    {
        RouterHandle::getInstance()->serviceRestartReport();
    }

    for (size_t i = 0; i < 50; i++)
    {
        _gStat.resetHitCnt(i);
    }

    _gStat.setSlaveCreating(false);
    string sSlaveCreatingDataFile = ServerConfig::DataPath + "/SlaveCreating.dat";
    if (access(sSlaveCreatingDataFile.c_str(), F_OK) != 0)
    {
        if (errno != ENOENT)
        {
            TLOGERROR("[CacheServer::initialize] access file: " << sSlaveCreatingDataFile << " error, errno = " << errno << endl);
            assert(false);
        }
    }
    else
    {
        ostringstream os;
        if (_gStat.serverType() == MASTER)
        {
            os << "server can not changed from SLAVE to MASTER when in creating data status" << endl;
            TARS_NOTIFY_ERROR(os.str());
            TLOGERROR("[CacheServer::initialize] " << os.str() << endl);
            assert(false);
        }
        _gStat.setSlaveCreating(true);

        os << "slave still in creating data status";
        TARS_NOTIFY_ERROR(os.str());
        TLOGDEBUG("[CacheServer::initialize] " << os.str() << endl);
    }
    string sDumpingDataFile = ServerConfig::DataPath + "/Dumping.dat";
    if (access(sDumpingDataFile.c_str(), F_OK) != 0)
    {
        if (errno != ENOENT)
        {
            TLOGERROR("[CacheServer::initialize] access file: " << sDumpingDataFile << " error, errno = " << errno << endl);
            assert(false);
        }
    }
    else
    {
        string sFile = ServerConfig::DataPath + "/Dumping.dat";
        remove(sFile.c_str());

        ostringstream os;
        os << "Dumping not finished";
        TARS_NOTIFY_ERROR(os.str());
        TLOGERROR("[CacheServer::initialize] " << os.str() << endl);
    }

    //创建获取Binlog日志线程
    //创建获取备份源最后记录binlog时间的线程
    _syncBinlogThread.init(ServerConfig::BasePath + "CacheServer.conf");
    _syncBinlogThread.createThread();
    _binlogTimeThread.init(ServerConfig::BasePath + "CacheServer.conf");
    _binlogTimeThread.createThread();

    //创建定时作业线程，负责读取路由表和回写脏数据等等
    _timerThread.init(ServerConfig::BasePath + "CacheServer.conf");
    _timerThread.createThread();

    _eraseThread.init(ServerConfig::BasePath + "CacheServer.conf");
    _eraseThread.createThread();

    _syncThread.init(ServerConfig::BasePath + "CacheServer.conf");
    _syncThread.createThread();

    _heartBeatThread.createThread();
    string sMasterConAddr = RouterHandle::getInstance()->getConnectHbAddr();
    if (!sMasterConAddr.empty())
        _heartBeatThread.enableConHb(true);

    string sStartExpireThread = _tcConf.get("/Main/Cache<StartExpireThread>", "N");
    if (sStartExpireThread == "Y" || sStartExpireThread == "y")
    {
        _gStat.enableExpire(true);
        _expireThread = ExpireThread::getInstance();
        _expireThread->init(ServerConfig::BasePath + "CacheServer.conf");
        int iRet = _expireThread->createThread();
        if (iRet != 0)
        {
            TLOGERROR("CacheServer::initialize create ExpireThread fail" << endl);
            exit(-1);
        }
    }

    _syncAllThread.init(ServerConfig::BasePath + "CacheServer.conf");

    _eraseDataInPageFunc.init(_tcConf);

    // 创建线程池，暂时设定线程数为1
    _tpool.init(1);
    _tpool.start();

    string sCaculateEnable = _tcConf.get("/Main/Cache<coldDataCalEnable>", "Y");
    _clodDataCntingThread._enable = (sCaculateEnable == "Y" || sCaculateEnable == "y") ? true : false;
    _clodDataCntingThread._resetCycleDays = TC_Common::strto<int>(_tcConf.get("/Main/Cache<coldDataCalCycle>", "7"));

    _clodDataCntingThread.createThread();

    TLOGDEBUG("CacheServer::initialize Succ" << endl);
}

bool CacheServer::help(const string& command, const string& params, string& result)
{
    result = "status: 显示Server的状态信息\n";
    result += "detailstatus: 显示每个jmem的状态信息\n";
    result += "reload: 重新加载配置\n";
    result += "syncdata: 回写所有脏数据\n";
    result += "cachestatus: 内存使用情况\n";
    result += "binlogstatus: binlog同步情况\n";
    result += "ver: 显示Cache版本\n";
    result += "erasedatainpage: 删除指定页范围内的数据\n";
    result += "key: 显示信号量或者共享内存key\n";
    result += "recreatedata: 备机重建数据\n";
    result += "erasewrongrouterdata: 删除不属于本CacheServer的分页\n";
    result += "calculateData: 统计未被访问数据大小\n";
    result += "setalldirty：将cache内全部数据设置成脏数据\n";
    result += "clearcache：清空cache，危险操作，请三思\n";
    return true;
}

bool CacheServer::showStatus(const string& command, const string& params, string& result)
{
    string sServerType = _gStat.serverType() == MASTER ? "Master" : "Slave";
    result = string("Server Type: ") + sServerType + "\n";
    result += "--------------------------------------------\n";

    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    if (_gStat.serverType() != MASTER)
    {
        ServerInfo server;
        g_route_table.getBakSource(sServerName, server);
        result += "Master Addr: \n";
        result += formatServerInfo(server);
    }

    result += "Binlog SyncTime: " + TC_Common::tm2str(_gStat.getBinlogTimeSync()) + "\n";
    result += "Binlog LastTime: " + TC_Common::tm2str(_gStat.getBinlogTimeLast()) + "\n";


    result += "--------------------------------------------\n";
    result += "Cache Info: \n";



    vector<TC_HashMapMalloc::tagMapHead> tmpHead;
    g_sHashMap.getMapHead(tmpHead);

    result += "数据可使用内存：" + TC_Common::tostr(g_sHashMap.getDataMemSize()) + "\n";

    result += formatCacheHead(tmpHead);

    result += "hashCount：" + TC_Common::tostr(g_sHashMap.getHashCount()) + "\n";

    result += "--------------------------------------------\n";
    result += "Router Info: \n";

    result += g_route_table.toString();

    return true;
}
bool CacheServer::showStatusDetail(const string& command, const string& params, string& result)
{
    TLOGDEBUG("CacheServer::showStatus begin" << endl);

    string sServerType = _gStat.serverType() == MASTER ? "Master" : "Slave";
    result = string("Server Type: ") + sServerType + "\n";
    result += "--------------------------------------------\n";

    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;



    if (_gStat.serverType() != MASTER)
    {
        ServerInfo server;
        g_route_table.getBakSource(sServerName, server);
        result += "Master Addr: \n";
        result += formatServerInfo(server);
    }

    result += "Binlog SyncTime: " + TC_Common::tm2str(_gStat.getBinlogTimeSync()) + "\n";
    result += "Binlog LastTime: " + TC_Common::tm2str(_gStat.getBinlogTimeLast()) + "\n";


    result += "--------------------------------------------\n";
    result += "Cache Info: \n";

    string desc;

    vector<TC_HashMapMalloc::tagMapHead> tmpHead;
    g_sHashMap.getMapHead(tmpHead);

    desc = g_sHashMap.descDetail();
    result += "数据可使用内存：" + TC_Common::tostr(g_sHashMap.getDataMemSize()) + "\n";
    result += formatCacheHead(tmpHead);
    result += "detail<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";
    for (size_t i = 0; i < tmpHead.size(); i++)
    {
        result += "JMEMNUM: " + TC_Common::tostr(i) + "\n";
        result += formatCacheHead(tmpHead[i]);
        result += "-------------------------------------------\n";
    }
    result += desc;

    result += "--------------------------------------------\n";
    result += "Router Info: \n";

    result += g_route_table.toString();

    TLOGDEBUG(result << endl);

    TLOGDEBUG("CacheServer::showStatus Succ" << endl);

    return true;
}
bool CacheServer::syncData(const string& command, const string& params, string& result)
{
    TLOGDEBUG("CacheServer::syncData begin" << endl);

    _syncAllThread.createThread();
    result = "syncdata had send";

    TLOGDEBUG("CacheServer::syncData succ" << endl);
    return true;
}

bool CacheServer::reloadConf(const string& command, const string& params, string& result)
{
    result = "reload config finish, warning:";
    addConfig("CacheServer.conf");
    _tcConf.parseFile(ServerConfig::BasePath + "CacheServer.conf");

    _todoFunctor.reload();

    g_sHashMap.setSyncTime(TC_Common::strto<unsigned int>(_tcConf["/Main/Cache<SyncTime>"]));

    _syncBinlogThread.reload();
    _timerThread.reload();
    _syncThread.reload();
    _syncAllThread.reload();
    _eraseDataInPageFunc.reload(_tcConf);
    _binlogTimeThread.reload();

    string sStartExpireThread = _tcConf.get("/Main/Cache<StartExpireThread>", "N");
    if (sStartExpireThread == "Y" || sStartExpireThread == "y")
    {
        _gStat.enableExpire(true);
        if (_expireThread == NULL)
        {
            _expireThread = ExpireThread::getInstance();
            _expireThread->init(ServerConfig::BasePath + "CacheServer.conf");
            tars::Int32 iRet = _expireThread->createThread();
            if (iRet != 0)
            {
                result += "\nrestart ExpireThread fail, please check";
                _expireThread = NULL;
            }
        }
        else
        {
            _expireThread->reload();
        }
    }
    else
    {
        _gStat.enableExpire(false);
        if (_expireThread)
        {
            _expireThread->stop();
            _expireThread = NULL;
        }
    }

    string sCaculateEnable = _tcConf.get("/Main/Cache<coldDataCalEnable>", "Y");
    _clodDataCntingThread._enable = (sCaculateEnable == "Y" || sCaculateEnable == "y") ? true : false;
    _clodDataCntingThread._resetCycleDays = TC_Common::strto<int>(_tcConf.get("/Main/Cache<coldDataCalCycle>", "7"));

    TLOGDEBUG(result << endl);
    return true;
}

bool CacheServer::cacheStatus(const string& command, const string& params, string& result)
{
    vector<TC_HashMapMalloc::tagMapHead> headVec;
    g_sHashMap.getMapHead(headVec);


    for (size_t i = 0; i < headVec.size(); i++)
    {
        result += TC_Common::tostr(headVec[i]._iMemSize) + "|" + TC_Common::tostr(g_sHashMap.getDataMemSize()) + "|" + TC_Common::tostr(headVec[i]._iUsedDataMem) + "|" + TC_Common::tostr(headVec[i]._iUsedChunk)
                  + "|" + TC_Common::tostr(headVec[i]._iElementCount) + "|" + TC_Common::tostr(headVec[i]._iDirtyCount) + "/r/n";
        result += string("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<") + "/r/n";
    }

    return true;
}

bool CacheServer::binlogStatus(const string& command, const string& params, string& result)
{
    result = TC_Common::tostr(_gStat.getBinlogTimeSync()) + "|" + TC_Common::tostr(_gStat.getBinlogTimeLast());
    return true;
}

bool CacheServer::showVer(const string& command, const string& params, string& result)
{
    result = CACHE_VER;
    return true;
}

bool CacheServer::showKey(const string& command, const string& params, string& result)
{
    result = _shmKey;
    return true;
}

bool CacheServer::setAllDirty(const string& command, const string& params, string& result)
{
    ostringstream os;
    int ret = TC_HashMapMalloc::RT_OK;
    SHashMap::dcache_hash_iterator it = g_sHashMap.hashBegin();
    size_t index = 0;
    while (it != g_sHashMap.hashEnd())
    {
        int ret = it->setDirty();
        if (ret != TC_HashMapMalloc::RT_OK)
        {
            os << "failed to set hashmap[" << index << "] data dirty, ret=" << ret;
            break;
        }
        ++it;
        ++index;
    }

    if (ret != TC_HashMapMalloc::RT_OK)
    {
        result = os.str();
        TLOGERROR("[CacheServer::setAllDirty] " << os.str() << endl);
    }
    else
    {
        result = "set all data dirty successfully";
    }

    return true;
}

bool CacheServer::clearCache(const string& command, const string& params, string& result)
{
    // 鉴权

    bool certified = true;

    if (certified)
    {
        g_sHashMap.clear();
    }

    result = "clear cache successfully";

    return true;
}

bool CacheServer::isAllInSlaveCreatingStatus()
{
    return (_syncBinlogThread.isInSlaveCreatingStatus() && _binlogTimeThread.isInSlaveCreatingStatus() && _timerThread.isInSlaveCreatingStatus());
}

void CacheServer::ppReport(enum PPReport::PropertyType type, int value)
{
    _ppReport.report(type, value);
}

bool CacheServer::isAllSync()
{
    return _todoFunctor.isAllSync();
}

bool CacheServer::haveSyncKeyIn(unsigned int begin, unsigned int end)
{
    return _todoFunctor.haveSyncKeyIn(begin, end);
}

DCache::GlobalStat* CacheServer::gstat()
{
    return &_gStat;
}

int CacheServer::getSyncTime()
{
    return _syncThread.getSyncTime();
}

DumpThread* CacheServer::dumpThread()
{
    return &_dumpThread;
}

SlaveCreateThread* CacheServer::slaveCreateThread()
{
    return &_slaveCreateThread;
}

int CacheServer::createBinLogFile(const string &path, bool isKeyBinLog)
{
    TC_ThreadLock::Lock lock(_binlogLock);

    if (isKeyBinLog)
    {
        //先关闭上次打开的文件
        if (_keyBinlogFD > 0)
            close(_keyBinlogFD);

        _keyBinlogFD = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (_keyBinlogFD < 0)
        {
            TLOGERROR("[CacheServer::createBinLogFile] open file error! " << path + "|errno:" << errno << endl);
            return -1;
        }
    }
    else
    {
        //先关闭上次打开的文件
        if (_binlogFD > 0)
            close(_binlogFD);

        _binlogFD = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (_binlogFD < 0)
        {
            TLOGERROR("[CacheServer::createBinLogFile] open file error! " << path + "|errno:" << errno << endl);
            return -1;
        }
    }

    return 0;
}

void CacheServer::WriteToFile(const string &content, const string &logFile)
{
    bool isKeyBinlog = (logFile.find("key") == string::npos) ? false : true;

    int exitCount = 5;//最大重试次数
    unsigned int n = 0;
    int fd;

    unsigned int iContentSize = content.size();

    //先加锁
    TC_ThreadLock::Lock lock(g_app._binlogLock);

    //获取文件描述符
    if (isKeyBinlog)
        fd = g_app._keyBinlogFD;
    else
        fd = g_app._binlogFD;

    //循环写，最大重试5次
    do
    {
        long tmpn = 0;
        tmpn = write(fd, &(content[n]), iContentSize - n);
        if (tmpn < iContentSize - n)
        {
            string error_str = "[CacheServer::WriteToFile] write binlog " + logFile + " file error! " + TC_Common::tostr(errno);;
            TLOGERROR(error_str << endl);
            TARS_NOTIFY_ERROR(error_str);
        }

        //一部分写成功，就接着写
        if (tmpn >= 0)
            n += tmpn;

    } while ((n < iContentSize) && (exitCount-- > 0));
}

string CacheServer::formatServerInfo(ServerInfo &serverInfo)
{
    string s;

    s = "serverName :" + serverInfo.serverName + "\n";
    s += "ip :" + serverInfo.ip + "\n";
    s += "BinLogServant :" + serverInfo.BinLogServant + "\n";
    s += "CacheServant :" + serverInfo.CacheServant + "\n";
    s += "RouteClientServant :" + serverInfo.RouteClientServant + "\n";

    return s;
}

string CacheServer::formatCacheHead(struct TC_HashMapMalloc::tagMapHead &head)
{
    string s;
    s = "内存大小：" + TC_Common::tostr(head._iMemSize) + "\n";
    s += "总元素个数：" + TC_Common::tostr(head._iElementCount) + "\n";
    s += "脏数据个数：" + TC_Common::tostr(head._iDirtyCount) + "\n";
    s += "每次删除个数：" + TC_Common::tostr(head._iEraseCount) + "\n";
    s += "回写时间：" + TC_Common::tostr(head._iSyncTime) + "\n";
    s += "已经使用的数据内存大小：" + TC_Common::tostr(head._iUsedDataMem) + "\n";
    s += "已经使用的内存块：" + TC_Common::tostr(head._iUsedChunk) + "\n";
    s += "get次数：" + TC_Common::tostr(head._iGetCount) + "\n";
    s += "命中次数：" + TC_Common::tostr(head._iHitCount) + "\n";

    return s;
}

string CacheServer::formatCacheHead(vector<TC_HashMapMalloc::tagMapHead> &tmpHead)
{
    size_t totalMemSize = 0;
    size_t totalElement = 0;
    size_t totalDirtyEle = 0;
    size_t totalOnlyKey = 0;
    size_t totalUsedDataMem = 0;
    size_t totalUsedChunk = 0;
    size_t totalGetCount = 0;
    size_t totalHitCount = 0;
    for (size_t i = 0; i < tmpHead.size(); i++)
    {
        totalMemSize += tmpHead[i]._iMemSize;
        totalElement += tmpHead[i]._iElementCount;
        totalDirtyEle += tmpHead[i]._iDirtyCount;
        totalOnlyKey += tmpHead[i]._iOnlyKeyCount;
        totalUsedDataMem += tmpHead[i]._iUsedDataMem;
        totalUsedChunk += tmpHead[i]._iUsedChunk;
        totalGetCount += tmpHead[i]._iGetCount;
        totalHitCount += tmpHead[i]._iHitCount;
    }
    string s;
    s = "内存大小：" + TC_Common::tostr(totalMemSize) + "\n";
    s += "总元素个数：" + TC_Common::tostr(totalElement) + "\n";
    s += "脏数据个数：" + TC_Common::tostr(totalDirtyEle) + "\n";
    s += "onlyKey个数：" + TC_Common::tostr(totalOnlyKey) + "\n";
    s += "每次删除个数：" + TC_Common::tostr(tmpHead[0]._iEraseCount) + "\n";
    s += "回写时间：" + TC_Common::tostr(tmpHead[0]._iSyncTime) + "\n";
    s += "已经使用的数据内存大小：" + TC_Common::tostr(totalUsedDataMem) + "\n";
    s += "已经使用的内存块：" + TC_Common::tostr(totalUsedChunk) + "\n";
    s += "get次数：" + TC_Common::tostr(totalGetCount) + "\n";
    s += "命中次数：" + TC_Common::tostr(totalHitCount) + "\n";
    s += "AvgDataSize: " + TC_Common::tostr(tmpHead[0]._iAvgDataSize) + "\n";
    s += "HashRadio: " + TC_Common::tostr(tmpHead[0]._fRadio) + "\n";
    return s;
}




bool CacheServer::eraseDataInPage(const string& command, const string& params, string& result)
{
    TLOGDEBUG("CacheServer::eraseDataInPage, " << params << endl);

    if (_gStat.serverType() != MASTER)
    {
        string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;
        result = sServerName + " is not MASTER, so cannot execute this command";
        return true;
    }

    if (params.empty() || TC_Common::sepstr<string>(params, " ").size() != 2)
    {
        result = "usage: erasedatainpage 起始页 结束页\n";
        result += "for example: 要删除属于1-100页范围内的数据\n";
        result += "erasedatainpage 1 100";
        return true;
    }

    vector<string> vtParams = TC_Common::sepstr<string>(params, " ");
    if (!TC_Common::isdigit(vtParams[0]) || !TC_Common::isdigit(vtParams[1]))
    {
        result = "params error, usage: erasedatainpage 起始页 结束页\n";
        result += "for example: 要删除属于1-100页范围内的数据\n";
        result += "erasedatainpage 1 100";
        return true;
    }
    int iPageNoStart, iPageNoEnd;
    iPageNoStart = TC_Common::strto<int>(vtParams[0]);
    iPageNoEnd = TC_Common::strto<int>(vtParams[1]);

    TC_Functor<void, TL::TLMaker<int, int>::Result> cmd(_eraseDataInPageFunc);
    TC_Functor<void, TL::TLMaker<int, int>::Result>::wrapper_type fwrapper(cmd, iPageNoStart, iPageNoEnd);
    _tpool.exec(fwrapper);

    result = "begin erase data in page [" + vtParams[0] + "," + vtParams[1] + "]\n";
    result += "please wait and check result later, bye";
    return true;

}


//在确保此命令运行完成后，不得再进行新的迁移
bool CacheServer::eraseWrongRouterData(const string& command, const string& params, string& result)
{
    TLOGDEBUG("CacheServer::eraseWrongRouterData start." << endl);
    ostringstream os;
    os << "begin erase data in page ";
    const PackTable& packTable = g_route_table.getPackTable();
    size_t nRangeCount = packTable.recordList.size();

    TLOGDEBUG("CacheServer::eraseWrongRouterData nRangeCount=" << nRangeCount << endl);
    for (size_t i = 0; i < nRangeCount; ++i)
    {
        TLOGDEBUG("CacheServer::eraseWrongRouterData groupName=" << packTable.recordList[i].groupName << "|groupName=" << _gStat.groupName() << endl);
        if (packTable.recordList[i].groupName != _gStat.groupName())
        {
            int iPageNoStart = packTable.recordList[i].fromPageNo;
            int iPageNoEnd = packTable.recordList[i].toPageNo;
            TLOGDEBUG("CacheServer::eraseWrongRouterData iPageNoStart=" << iPageNoStart << "|iPageNoEnd=" << iPageNoEnd << endl);
            os << "[" << iPageNoStart << "," << iPageNoEnd << "],";

            TC_Functor<void, TL::TLMaker<int, int>::Result> cmd(_eraseDataInPageFunc);
            TC_Functor<void, TL::TLMaker<int, int>::Result>::wrapper_type fwrapper(cmd, iPageNoStart, iPageNoEnd);
            _tpool.exec(fwrapper);
        }
    }
    os << " please wait and check result later, bye.";
    result = os.str();
    TLOGDEBUG("CacheServer::eraseWrongRouterData finish." << endl);
    return true;
}

bool CacheServer::calculateData(const string& command, const string& params, string& result)
{
    _clodDataCntingThread.runImmediately(true);

    result = "calculate is running!";

    return true;
}

bool CacheServer::resetCalculatePoint(const string& command, const string& params, string& result)
{
    g_sHashMap.resetCalculateData();
    _clodDataCntingThread._startTime = time(NULL);
    _clodDataCntingThread._reportCnt = 0;

    result = "reset calculate point success!";
    FDLOG("unread") << result << endl;

    return true;
}

/////////////////////////////////////////////////////////////////
enum ServerType CacheServer::getServerType()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    GroupInfo group;
    g_route_table.getGroup(sServerName, group);

    if (group.masterServer == sServerName)
    {
        return MASTER;
    }
    return SLAVE;
}

bool CacheServer::addConfig(const string &filename)
{
    string result;

    if (pullConfig(filename, result))
    {
        TLOGDEBUG("CacheServer::addConfig pullConfig result = " << result << endl);
        TarsRemoteNotify::getInstance()->report(result);

        return true;
    }

    TLOGERROR("[CacheServer::addConfig] pullConfig result = " << result << endl);
    TarsRemoteNotify::getInstance()->report(result);

    return true;
}


bool CacheServer::pullConfig(const string & fileName, string &result, bool bAppConfigOnly, int _iMaxBakNum)
{
    int retryTimes = 1;
    do {
        try
        {
            string sFullFileName = ServerConfig::BasePath + "/" + fileName;

            string newFile = getRemoteFile(fileName, bAppConfigOnly);

            if (newFile.empty() || access(newFile.c_str(), R_OK) != 0)//拉取不到配置中心的配置文件
            {
                if (access(sFullFileName.c_str(), R_OK) == 0) //获取本地配置成功，返回成功，但需要告警一下。
                {
                    result = "[fail] get remote config:" + fileName + "fail,use the local config.";

                    return true;
                }
                throw runtime_error("access file error:" + newFile);
            }

            if (TC_File::load2str(newFile) != TC_File::load2str(sFullFileName))
            {
                for (int i = _iMaxBakNum - 1; i >= 1; --i)
                {
                    if (access(index2file(sFullFileName, i).c_str(), F_OK) == 0)
                    {
                        localRename(index2file(sFullFileName, i), index2file(sFullFileName, i + 1));
                    }
                }

                if (access(sFullFileName.c_str(), F_OK) == 0)
                {
                    localRename(sFullFileName, index2file(sFullFileName, 1));
                }
            }

            localRename(newFile, sFullFileName);

            assert(!access(sFullFileName.c_str(), R_OK));

            result = "[succ] get remote config:" + fileName;

            return true;
        }
        catch (std::exception& e)
        {
            TLOGERROR("[CacheServer::pullConfig] exception: " << e.what() << endl);
            result = "[fail] get remote config '" + fileName + "' error:" + string(e.what());
        }
        catch (...)
        {
            TLOGERROR("[CacheServer::pullConfig] unknown exception" << endl);
            result = "[fail] get remote config '" + fileName + "' unknown error";
        }
    } while (retryTimes--);

    return false;
}

string CacheServer::getRemoteFile(const string &fileName, bool bAppConfigOnly)
{
    if (_configPrx)
    {
        string stream;
        int ret = _configPrx->readConfig(ServerConfig::Application, (bAppConfigOnly ? "" : ServerConfig::ServerName), stream);
        if (ret != 0)
        {
            throw runtime_error("remote config file is empty:" + fileName);
        }
        if (stream.empty())
        {
            throw runtime_error("my remote config file is empty:" + fileName);
        }

        string newFile = ServerConfig::BasePath + "/" + fileName + "." + TC_Common::tostr(time(NULL));

        std::ofstream out(newFile.c_str());

        string result;
        if (out)
        {
            out << stream;//如果硬盘满了，是否能写入成功需要进行判断。
            if (out.bad())
            {
                result = "[fail] copy stream to disk error.";
                TarsRemoteNotify::getInstance()->report(result);
                return "";
            }
            else
            {
                return newFile;
            }
        }
    }
    return "";
}

string CacheServer::index2file(const string & sFullFileName, int index)
{
    return   sFullFileName + "." + TC_Common::tostr(index) + ".bak";
}

void CacheServer::localRename(const string& oldFile, const string& newFile)
{
    if (::rename(oldFile.c_str(), newFile.c_str()) != 0)
    {
        throw runtime_error("rename file error:" + oldFile + "->" + newFile);
    }
}

/////////////////////////////////////////////////////////////////
void
CacheServer::destroyApp()
{
    //destroy application here:
    //...
    _syncBinlogThread.stop();
    _binlogTimeThread.stop();
    _timerThread.stop();
    _eraseThread.stop();
    _syncThread.stop();
    _syncAllThread.stop();
    _createBinlogFileThread.stop();
    _heartBeatThread.stop();

    _dumpThread.stop();
    _slaveCreateThread.stop();

    if ((_tcConf["/Main/Cache<StartExpireThread>"] == "Y" || _tcConf["/Main/Cache<StartExpireThread>"] == "y"))
    {
        _expireThread->stop();
    }

    while (_syncBinlogThread.isRuning() || _binlogTimeThread.isRuning() || _timerThread.isRuning() || _syncAllThread.isRuning() || _syncThread.isRuning() || _eraseThread.isRuning() || _slaveCreateThread.isRuning() || _dumpThread.isRuning())
    {
        usleep(10000);
    }

    if ((_tcConf["/Main/Cache<StartExpireThread>"] == "Y" || _tcConf["/Main/Cache<StartExpireThread>"] == "y"))
    {
        while (_expireThread->isRuning())
        {
            usleep(10000);
        }
    }
    TLOGERROR("CacheServer::destroyApp Succ" << endl);
}

