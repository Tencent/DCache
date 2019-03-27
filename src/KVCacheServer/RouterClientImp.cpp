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
#include "RouterClientImp.h"

void RouterClientImp::initialize()
{
    //initialize servant here:
    //...

    string sConf = ServerConfig::BasePath + "CacheServer.conf";
    _tcConf.parseFile(sConf);

    _moduleName = _tcConf["/Main<ModuleName>"];

    _binlogFile = _tcConf["/Main/BinLog<LogFile>"];

    _keyBinlogFile = _tcConf["/Main/BinLog<LogFile>"] + "key";

    string sRecordBinLog = _tcConf.get("/Main/BinLog<Record>", "Y");
    _recordBinLog = (sRecordBinLog == "Y" || sRecordBinLog == "y") ? true : false;

    string sRecordKeyBinLog = _tcConf.get("/Main/BinLog<KeyRecord>", "N");
    _recordKeyBinLog = (sRecordKeyBinLog == "Y" || sRecordKeyBinLog == "y") ? true : false;

    _cleanDataFunc.Init(_tcConf);

    _tpool.init(1);
    _tpool.start();

    TLOGDEBUG("RouterClientImp::initialize Succ" << endl);
}

void RouterClientImp::destroy()
{
}

tars::Int32 RouterClientImp::setRouterInfo(const string &moduleName, const PackTable & packTable, tars::TarsCurrentPtr current)
{
    return _routerHandle->setRouterInfo(moduleName, packTable);
}

tars::Int32 RouterClientImp::setTransRouterInfo(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const PackTable& packTable,
        tars::TarsCurrentPtr current)
{
    return _routerHandle->setTransRouterInfo(moduleName, transInfoListVer, transferingInfoList, packTable);
}

tars::Int32 RouterClientImp::fromTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const PackTable& packTable, tars::TarsCurrentPtr current)
{
    return _routerHandle->fromTransferStart(moduleName, transInfoListVer, transferingInfoList, packTable);
}

tars::Int32 RouterClientImp::fromTransferDo(const TransferInfo& transferingInfo, tars::TarsCurrentPtr current)
{
    return _routerHandle->fromTransferDo(transferingInfo, current);
}

tars::Int32 RouterClientImp::toTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer,
        const vector<TransferInfo>& transferingInfoList, const TransferInfo& transferingInfo, const PackTable& packTable, tars::TarsCurrentPtr current)
{
    return _routerHandle->toTransferStart(moduleName, transInfoListVer, transferingInfoList, transferingInfo, packTable);
}

tars::Int32 RouterClientImp::getBinlogdif(tars::Int32 &difBinlogTime, tars::TarsCurrentPtr current)
{
    return _routerHandle->getBinlogdif(difBinlogTime);
}

tars::Int32 RouterClientImp::setRouterInfoForSwitch(const std::string & moduleName, const DCache::PackTable & packTable, tars::TarsCurrentPtr current)
{
    return _routerHandle->setRouterInfoForSwicth(moduleName, packTable);
}



bool RouterClientImp::isTransSrc(int pageNo)
{
    ServerInfo srcServer;
    ServerInfo destServer;
    g_route_table.getTrans(pageNo, srcServer, destServer);

    if (srcServer.serverName == (ServerConfig::Application + "." + ServerConfig::ServerName))
        return true;
    return false;
}

tars::Int32 RouterClientImp::helloBaby(tars::TarsCurrentPtr current)
{
    TLOGDEBUG("i am ok" << endl);
    return 0;
}

tars::Int32 RouterClientImp::helleBabyByName(const std::string & serverName, tars::TarsCurrentPtr current)
{
    return 0;
}

tars::Int32 RouterClientImp::setBatchFroTrans(const std::string & moduleName, const vector<DCache::Data> & data, tars::TarsCurrentPtr current)
{
    if (g_app.gstat()->serverType() != MASTER)
    {
        //SLAVE状态下不提供接口服务
        TLOGERROR("RouterClientImp::setBatchFroTrans: ServerType is not Master" << endl);
        return ET_SERVER_TYPE_ERR;
    }
    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOGERROR("RouterClientImp::setBatchFroTrans: moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    size_t dataCount = data.size();
    for (size_t i = 0; i < dataCount; ++i)
    {
        bool dirty = data[i].dirty;

        try
        {
            //迁移时禁止set，由于迁移时允许set的逻辑有漏洞，在解决漏洞之前禁止Set
            if (g_route_table.isTransfering(data[i].k.keyItem))
            {
                int iPageNo = g_route_table.getPageNo(data[i].k.keyItem);
                if (isTransSrc(iPageNo))
                {
                    TLOGERROR("RouterClientImp::setBatchFroTrans: " << data[i].k.keyItem << " forbid set" << endl);
                    return ET_FORBID_OPT;
                }
            }

            //检查key是否是在自己服务范围内
            if (!g_route_table.isMySelf(data[i].k.keyItem))
            {
                //返回模块错误
                TLOGERROR("RouterClientImp::setBatchFroTrans: " << data[i].k.keyItem << " is not in self area" << endl);
                TLOGERROR(g_route_table.toString() << endl);
                return ET_KEY_AREA_ERR;
            }

            if (!data[i].bIsOnlyKey)
            {
                int iRet = g_sHashMap.set(data[i].k.keyItem, data[i].v.value, dirty, data[i].expireTimeSecond);

                if (iRet != TC_HashMapMalloc::RT_OK)
                {
                    TLOGERROR("RouterClientImp::setBatchFroTrans hashmap.set(" << data[i].k.keyItem << ") error:" << iRet << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);
                    return ET_SYS_ERR;
                }
                //写Binlog
                if (_recordBinLog)
                {
                    TBinLogEncode logEncode;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET, dirty, data[i].k.keyItem, data[i].v.value, data[i].expireTimeSecond, BINLOG_TRANS) + "\n", _binlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
                if (_recordKeyBinLog)
                {
                    TBinLogEncode logEncode;
                    CacheServer::WriteToFile(logEncode.EncodeSetKey(data[i].k.keyItem, BINLOG_TRANS) + "\n", _keyBinlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
            }
            else
            {
                int iRet = g_sHashMap.set(data[i].k.keyItem, 1);

                if (iRet == TC_HashMapMalloc::RT_OK)
                {
                    if (_recordBinLog)
                    {
                        TBinLogEncode logEncode;
                        string value;
                        CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET_ONLYKEY, true, data[i].k.keyItem, value) + "\n", _binlogFile);
                        if (g_app.gstat()->serverType() == MASTER)
                            g_app.gstat()->setBinlogTime(0, TNOW);
                    }
                    if (_recordKeyBinLog)
                    {
                        TBinLogEncode logEncode;
                        string value;
                        CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET_ONLYKEY, true, data[i].k.keyItem, value) + "\n", _keyBinlogFile);
                        if (g_app.gstat()->serverType() == MASTER)
                            g_app.gstat()->setBinlogTime(0, TNOW);
                    }
                }
            }
        }
        catch (const std::exception & ex)
        {
            TLOGERROR("RouterClientImp::setBatchFroTrans exception: " << ex.what() << " , key = " << data[i].k.keyItem << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
        catch (...)
        {
            TLOGERROR("RouterClientImp::setBatchFroTrans unkown_exception, key = " << data[i].k.keyItem << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
    }

    return ET_SUCC;
}

tars::Int32 RouterClientImp::setBatchFroTransOnlyKey(const std::string & moduleName, const vector<std::string> & mainKey, tars::TarsCurrentPtr current)
{
    TLOGDEBUG("RouterClientImp::setBatchFroTransOnlyKey recv moduleName:" << moduleName << ", mainKey.size:" << mainKey.size() << endl);

    if (g_app.gstat()->serverType() != MASTER)
    {
        TLOGERROR("[RouterClientImp::setBatchFroTransOnlyKey] ServerType is not Master" << endl);
        return ET_SERVER_TYPE_ERR;
    }
    if (moduleName != _moduleName)
    {
        TLOGERROR("[RouterClientImp::setBatchFroTransOnlyKey] moduleName error" << endl);
        return ET_MODULE_NAME_INVALID;
    }

    vector<std::string>::const_iterator it = mainKey.begin();
    for (; it != mainKey.end(); it++)
    {
        try
        {
            if (g_route_table.isTransfering(*it))
            {
                int iPageNo = g_route_table.getPageNo(*it);
                if (isTransSrc(iPageNo))
                {
                    TLOGERROR("[RouterClientImp::setBatchFroTransOnlyKey] " << (*it) << " forbid trans" << endl);
                    return ET_FORBID_OPT;
                }
            }

            if (!g_route_table.isMySelf(*it))
            {
                TLOGERROR("[RouterClientImp::setBatchFroTransOnlyKey] " << (*it) << " is not in self area" << endl);
                return ET_KEY_AREA_ERR;
            }

            int iRet = g_sHashMap.set(*it, 1);

            if (iRet == TC_HashMapMalloc::RT_OK)
            {
                if (_recordBinLog)
                {
                    TBinLogEncode logEncode;
                    string value;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET_ONLYKEY, true, *it, value) + "\n", _binlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
                if (_recordKeyBinLog)
                {
                    TBinLogEncode logEncode;
                    string value;
                    CacheServer::WriteToFile(logEncode.Encode(BINLOG_SET_ONLYKEY, true, *it, value) + "\n", _keyBinlogFile);
                    if (g_app.gstat()->serverType() == MASTER)
                        g_app.gstat()->setBinlogTime(0, TNOW);
                }
            }
        }
        catch (const std::exception &ex)
        {
            TLOGERROR("[RouterClientImp::setBatchFroTransOnlyKey] exception: " << ex.what() << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
        catch (...)
        {
            TLOGERROR("[RouterClientImp::setBatchFroTransOnlyKey] unkown exception, " << endl);
            g_app.ppReport(PPReport::SRP_EX, 1);
            return ET_SYS_ERR;
        }
    }

    return ET_SUCC;
}

tars::Int32 RouterClientImp::cleanFromTransferData(const std::string & moduleName, tars::Int32 fromPageNo, tars::Int32 toPageNo, tars::TarsCurrentPtr current)
{
    TLOGDEBUG("RouterClientImp::cleanFromTransferData, moduleName:" << moduleName << ", fromPageNo:" << fromPageNo << ", toPageNo:" << toPageNo << endl);
    if (g_app.gstat()->serverType() != MASTER)
    {
        //SLAVE状态下不做迁移
        TLOGERROR("[RouterClientImp::cleanFromTransferData] is not Master" << endl);
        return 1;
    }
    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOGERROR("[RouterClientImp::cleanFromTransferData] moduleName error, " << _moduleName << " != " << moduleName << "(config != param)" << endl);
        return 2;
    }

    if (fromPageNo > toPageNo)
    {
        TLOGERROR("[RouterClientImp::cleanFromTransferData]range error! fromPageNo:" << fromPageNo << " toPageNo:" << toPageNo << endl);
        return -1;
    }

    TC_Functor<void, TL::TLMaker<int, int>::Result> cmd(_cleanDataFunc);
    TC_Functor<void, TL::TLMaker<int, int>::Result>::wrapper_type fwrapper(cmd, fromPageNo, toPageNo);
    _tpool.exec(fwrapper);

    return 0;
}

int RouterClientImp::isReadyForSwitch(const std::string & moduleName, bool &bReady, tars::TarsCurrentPtr current)
{
    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOGERROR("RouterClientImp::isReadyForSwitch: moduleName error" << endl);
        return -1;
    }

    bReady = true;

    if (g_app.gstat()->isSlaveCreating() == true)
    {
        bReady = false;
        TLOGERROR("RouterClientImp::isReadyForSwitch slaveCreating.. can't switch. " << endl);
    }

    return 0;
}


tars::Int32 RouterClientImp::notifyMasterDowngrade(const std::string & moduleName, tars::TarsCurrentPtr current)
{
    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOGERROR("RouterClientImp::notifyMasterDowngrade: moduleName error" << endl);
        return -1;
    }

    return _routerHandle->masterDowngrade();
}

int RouterClientImp::disconnectFromMaster(const std::string & moduleName, tars::TarsCurrentPtr current)
{
    if (moduleName != _moduleName)
    {
        //返回模块错误
        TLOGERROR("RouterClientImp::disconnectFromMaster: moduleName error" << endl);
        return -1;
    }

    if (g_app.gstat()->serverType() == MASTER)
    {
        string err("RouterClientImp::disconnectFromMaster, server is master. error.");
        TLOGERROR(err << endl);
        FDLOG("switch") << err << endl;
        return -1;
    }

    int ret = 0;

    g_app.enableConnectHb(false);
    FDLOG("switch") << "disconnectFromMaster succ." << endl;

    return ret;
}


void RouterClientImp::CleanDataFunctor::Init(TC_Config& conf)
{
    _pageSize = TC_Common::strto<unsigned int>(conf["/Main/Router<PageSize>"]);

    _eraseDataFunctor.init(conf);
}

void RouterClientImp::CleanDataFunctor::operator()(int pageNoStart, int pageNoEnd)
{
    unsigned int iHash = pageNoStart * _pageSize;
    int count = 6;
    while (count-- > 0)
    {
        //如果还是属于本机的路由范围的，就等待
        if (g_route_table.isMySelfByHash(iHash))
            sleep(10);
        else
            break;
    }

    if (count <= 0)
    {
        TLOGERROR("[CleanDataFunctor::operator()] pageno error! " << pageNoStart << " " << pageNoEnd << endl);
        return;
    }

    _eraseDataFunctor(pageNoStart, pageNoEnd);
}

