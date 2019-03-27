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
#include "MKCacheServer.h"
#include "DeleteThread.h"
#include "MKCacheComm.h"
#include <ctype.h>

extern MKCacheServer g_app;

void DeleteThread::init(const string &sConf)
{
    _config = sConf;
    _tcConf.parseFile(_config);

    _deleteInterval = TC_Common::strto<int>(_tcConf.get("/Main/Cache<DeleteInterval>", "300"));
    _deleteSpeed = TC_Common::strto<size_t>(_tcConf.get("/Main/Cache<DeleteSpeed>", "0"));

    TLOGDEBUG("[DeleteThread::init] DeleteSpeed:" << _deleteSpeed << " DbInterval:" << _deleteInterval << endl);

    string dbFlag = _tcConf["/Main/DbAccess<DBFlag>"];
    if ((dbFlag == "Y") || (dbFlag == "y"))
        _existDB = true;
    else
        _existDB = false;

    string sRDBaccessObj = _tcConf["/Main/DbAccess<ObjName>"];
    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(sRDBaccessObj);

    _dbDayLog = _tcConf["/Main/Log<DbDayLog>"];

    _unblockPercent = TC_Common::strto<int>(_tcConf.get("/Main/Cache<SyncUNBlockPercent>", "60"));

    string sBlockTime = _tcConf.get("/Main/Cache<SyncBlockTime>", "0000-0000");

    vector<string> vTime = TC_Common::sepstr<string>(sBlockTime, ";", false);

    for (unsigned int i = 0; i < vTime.size(); i++)
    {
        vector<string> vTival = TC_Common::sepstr<string>(vTime[i], "-", false);
        if (vTival.size() != 2)
        {
            TLOGERROR("[DeleteThread::init] block time error! | " << vTime[i] << endl);
            assert(false);
        }
        //检查获取的时间是否正确
        if ((vTival[0].size() != 4) || (vTival[1].size() != 4))
        {
            TLOGERROR("[DeleteThread::init] block time error! | " << vTime[i] << endl);
            assert(false);
        }

        for (unsigned int j = 0; j < 4; j++)
        {
            if ((!isdigit(vTival[0][j])) || (!isdigit(vTival[1][j])))
            {
                TLOGERROR("[DeleteThread::init] block time error! | " << vTime[i] << endl);
                assert(false);
            }
        }

        int iHour, iMin;
        sscanf(vTival[0].c_str(), "%2d%2d", &iHour, &iMin);
        time_t m_iSyncBlockBegin = iHour * 60 * 60 + iMin * 60;

        sscanf(vTival[1].c_str(), "%2d%2d", &iHour, &iMin);
        time_t m_iSyncBlockEnd = iHour * 60 * 60 + iMin * 60;

        if (m_iSyncBlockBegin > m_iSyncBlockEnd)
        {
            TLOGERROR("[DeleteThread::init] block time error! | " << vTime[i] << endl);
            assert(false);
        }

        if (m_iSyncBlockBegin == m_iSyncBlockEnd)
            continue;

        _blockingTime.push_back(make_pair(m_iSyncBlockBegin, m_iSyncBlockEnd));

        TLOGDEBUG("[DeleteThread::init] " << m_iSyncBlockBegin << " " << m_iSyncBlockEnd << endl);
    }

    string sKeyType = _tcConf.get("/Main/Cache<MainKeyType>", "hash");
    if (sKeyType == "hash")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE;
    else if (sKeyType == "set")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::SET_TYPE;
    else if (sKeyType == "zset")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE;
    else if (sKeyType == "list")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::LIST_TYPE;

    TLOGDEBUG("DeleteThread::init succ" << endl);
}

void DeleteThread::reload()
{
    _tcConf.parseFile(_config);

    _deleteInterval = TC_Common::strto<int>(_tcConf.get("/Main/Cache<DeleteInterval>", "100"));
    _deleteSpeed = TC_Common::strto<size_t>(_tcConf.get("/Main/Cache<DeleteSpeed>", "0"));

    TLOGDEBUG("[DeleteThread::reload] DeleteSpeed:" << _deleteSpeed << " DbInterval:" << _deleteInterval << endl);

    string dbFlag = _tcConf["/Main/DbAccess<DBFlag>"];
    if ((dbFlag == "Y") || (dbFlag == "y"))
        _existDB = true;
    else
        _existDB = false;

    string sRDBaccessObj = _tcConf["/Main/DbAccess<ObjName>"];
    _dbaccessPrx = Application::getCommunicator()->stringToProxy<DbAccessPrx>(sRDBaccessObj);

    string sKeyType = _tcConf.get("/Main/Cache<MainKeyType>", "hash");
    if (sKeyType == "hash")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE;
    else if (sKeyType == "set")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::SET_TYPE;
    else if (sKeyType == "zset")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE;
    else if (sKeyType == "list")
        _storeKeyType = TC_Multi_HashMap_Malloc::MainKey::LIST_TYPE;

    TLOGDEBUG("DeleteThread::reload succ" << endl);
}

void DeleteThread::createThread()
{
    //创建线程
    pthread_t thread;

    if (!_isStart)
    {
        _isStart = true;
        if (pthread_create(&thread, NULL, Run, (void*)this) != 0)
        {
            throw runtime_error("Create DeleteThread fail");
        }
    }
}

void* DeleteThread::Run(void* arg)
{
    pthread_detach(pthread_self());
    DeleteThread* pthis = (DeleteThread*)arg;

    pthis->setRuning(true);

    MKCachePrx pMasterCachePrx;
    string sMasterCacheAddr = "";

    time_t tLastDb = 0;
    while (pthis->isStart())
    {
        try
        {
            time_t tNow = TC_TimeProvider::getInstance()->getNow();
            if (tNow - tLastDb >= pthis->_deleteInterval)
            {
                if (g_app.gstat()->serverType() == MASTER)
                {
                    pthis->deleteData(tNow);
                    g_app.gstat()->setDeleteTime(tNow);
                    TLOGDEBUG("master delete data, t= " << TC_Common::tm2str(tNow) << endl);
                }
                else if (g_app.gstat()->serverType() == SLAVE)
                {
                    string sTmpCacheAddr = pthis->getBakSourceAddr();
                    if (sTmpCacheAddr.length() > 0)
                    {
                        if (sTmpCacheAddr != sMasterCacheAddr)
                        {
                            TLOGDEBUG("MasterCacheAddr changed from " << sMasterCacheAddr << " to " << sTmpCacheAddr << endl);
                            sMasterCacheAddr = sTmpCacheAddr;
                            pMasterCachePrx = Application::getCommunicator()->stringToProxy<MKCachePrx>(sMasterCacheAddr);
                        }
                        time_t tDelete = pMasterCachePrx->getDeleteTime();
                        g_app.gstat()->setDeleteTime(tDelete);

                        if (tDelete > 60)
                        {
                            time_t tDeleteSlave = tDelete - 60;
                            pthis->deleteData(tDeleteSlave);
                        }

                        TLOGDEBUG("slave delete data, t= " << TC_Common::tm2str(tDelete) << " - 60" << endl);
                    }
                }
                tLastDb = tNow;
            }
            sleep(1);
        }
        catch (const TarsException & ex)
        {
            TLOGERROR("DeleteThread::Run: exception: " << ex.what() << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            usleep(100000);
        }
        catch (const std::exception &ex)
        {
            TLOGERROR("DeleteThread::Run: exception: " << ex.what() << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            usleep(100000);
        }
        catch (...)
        {
            TLOGERROR("DeleteThread::Run: unkown exception: " << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            usleep(100000);
        }
    }
    pthis->setRuning(false);
    pthis->setStart(false);
    return NULL;
}

void DeleteThread::deleteData(time_t t)
{
    //删除失败统计
    int failCount = 0;
    size_t iCount = 0;
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    time_t tBegin = TC_TimeProvider::getInstance()->getNow();
    do
    {
        failCount = 0;
        MultiHashMap::hash_iterator it = g_HashMap.hashBegin();
        while (isStart() && it != g_HashMap.hashEnd())
        {

            //检查是否屏蔽回写
            if (_blockingTime.size() > 0)
            {
                //今天凌晨开始的秒数
                time_t nows = (time(NULL) + 28800) % 86400;

                size_t elementCount = g_HashMap.getTotalElementCount();

                if (elementCount > 0 && ((g_app.gstat()->dirtyNum() + g_app.gstat()->delNum()) * 100 / elementCount) < _unblockPercent)
                {
                    vector<pair<time_t, time_t> >::iterator it = _blockingTime.begin();
                    while (it != _blockingTime.end())
                    {
                        if (it->first <= nows && nows <= it->second)
                        {
                            TLOGDEBUG("[DeleteThread::deleteData] block delete data! " << nows << endl);
                            sleep(30);
                            break;
                        }
                        it++;
                    }

                    if (it != _blockingTime.end())
                        continue;
                }
            }

            vector<MultiHashMap::DeleteData> vv;
            it->getDeleteData(vv);
            size_t i = 0;
            while (i < vv.size())
            {
                time_t tNow = TC_TimeProvider::getInstance()->getNow();

                if (_deleteSpeed > 0 && tBegin == tNow && iCount > _deleteSpeed)
                {
                    usleep(10000);
                }
                else
                {
                    if (tBegin < tNow)
                    {
                        iCount = 0;
                        tBegin = tNow;
                    }
                    try
                    {
                        g_app.insertDeleteKey(vv[i]._mkey);
                        //数据正在迁移，不需要动
                        if (g_route_table.isTransfering(vv[i]._mkey))
                        {
                            g_app.earseDeleteKey(vv[i]._mkey);
                            i++;
                            continue;
                        }

                        TLOGDEBUG("[DeleteThread::deleteData] " << vv[i]._mkey << " " << vv[i]._iDeleteTime << endl);

                        if (vv[i]._iDeleteTime != 0 && t >= vv[i]._iDeleteTime)
                        {
                            int iRet = 0;
                            if ((g_app.gstat()->serverType() == MASTER) && _existDB && g_route_table.isMySelf(vv[i]._mkey))
                            {
                                //先删除db的信息
                                int iRet;
                                vector<DCache::DbCondition> vtDbCond;
                                DbCondition cond;
                                cond.fieldName = fieldConfig.sMKeyName;
                                cond.op = DCache::EQ;
                                cond.value = vv[i]._mkey;
                                map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.sMKeyName);
                                if (itInfo == fieldConfig.mpFieldInfo.end())
                                    throw MKDCacheException("Field configuration error.");
                                cond.type = ConvertDbType(itInfo->second.type);
                                vtDbCond.push_back(cond);

                                if (_storeKeyType == TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE)
                                {
                                    UKBinToDbCondition(vv[i]._ukey, vtDbCond);
                                }
                                else
                                {
                                    ValueBinToDbCondition(vv[i]._ukey, vtDbCond);
                                }

                                string sLogCond = FormatLog::tostr(vtDbCond);
                                try
                                {
                                    iRet = _dbaccessPrx->delCond(vv[i]._mkey, vtDbCond);
                                    if (iRet < 0)
                                    {
                                        FDLOG(_dbDayLog) << "[DeleteThread::deleteData] del " << vv[i]._mkey << "|" << sLogCond << "|failed|" << iRet << endl;
                                        TLOGERROR("DeleteThread::deleteData delString error, key = " << vv[i]._mkey << ", iRet = " << iRet << endl);

                                        failCount++;

                                        g_app.earseDeleteKey(vv[i]._mkey);
                                        i++;
                                        continue;
                                    }
                                    else
                                    {
                                        FDLOG(_dbDayLog) << "[DeleteThread::deleteData] del db " << vv[i]._mkey << "|" << sLogCond << "|succ|" << endl;
                                    }
                                }
                                catch (const std::exception & ex)
                                {
                                    FDLOG(_dbDayLog) << "[DeleteThread::deleteData] del db " << vv[i]._mkey << "|" << sLogCond << "|failed|" << ex.what() << endl;
                                    TLOGERROR("DeleteThread::deleteData delString exception: " << ex.what() << ", key = " << vv[i]._mkey << endl);

                                    g_app.ppReport(PPReport::SRP_DB_EX, 1);
                                    failCount++;

                                    g_app.earseDeleteKey(vv[i]._mkey);
                                    i++;
                                    continue;
                                }

                                //增加删除计数
                                iCount++;
                            }

                            //删除cache里的数据
                            if (_storeKeyType == TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE)
                                iRet = g_HashMap.delReal(vv[i]._mkey, vv[i]._ukey);
                            else if (_storeKeyType == TC_Multi_HashMap_Malloc::MainKey::SET_TYPE)
                            {
                                iRet = g_HashMap.delSetReal(vv[i]._mkey, vv[i]._ukey);
                            }
                            else if (_storeKeyType == TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE)
                            {
                                iRet = g_HashMap.delZSetReal(vv[i]._mkey, vv[i]._ukey);
                            }

                            if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                TLOGERROR("[DeleteThread::deleteData] del |" << vv[i]._mkey << "|" << ((_storeKeyType == TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE) ? FormatLog::tostr(vv[i]._ukey) : FormatLog::tostr(vv[i]._ukey, false)) << "|failed|" << iRet << endl);
                                g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                                failCount++;
                            }
                        }

                        g_app.earseDeleteKey(vv[i]._mkey);

                        //自增，删除下一个数据
                        i++;
                    }
                    catch (const std::exception &ex)
                    {
                        TLOGERROR("[DeleteThread::deleteData] exception: " << ex.what() << ", mkey = " << vv[i]._mkey << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        failCount++;
                    }
                }
            }

            it++;
        }

    } while ((failCount != 0) && (isStart()));
    if (!isStart())
    {
        TLOGDEBUG("DeleteThread by stop" << endl);
    }
    else
    {
        TLOGDEBUG("DeleteThread::deleteData finish" << endl);
    }
}

string DeleteThread::getBakSourceAddr()
{
    string sServerName = ServerConfig::Application + "." + ServerConfig::ServerName;

    ServerInfo server;
    int iRet = g_route_table.getBakSource(sServerName, server);
    if (iRet != UnpackTable::RET_SUCC)
    {
        TLOGERROR("DeleteThread::getBakSourceAddr getBakSource error, iRet = " << iRet << endl);
        g_app.ppReport(PPReport::SRP_EX, 1);
        return "";
    }

    string sBakSourceAddr;
    sBakSourceAddr = server.CacheServant;
    return sBakSourceAddr;
}

