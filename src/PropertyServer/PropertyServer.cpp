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
#include "PropertyServer.h"
#include "PropertyImp.h"
#include "CacheInfo.h"
#include "CacheInfoManager.h"
#include "PropertyDbManager.h"

PropertyServer g_app;

void PropertyServer::initialize()
{
    try
    {
        // 加载配置文件
        addConfig("PropertyServer.conf");
        _conf.parseFile(ServerConfig::BasePath + "PropertyServer.conf");
        
        // 添加处理对象
        addServant<PropertyImp>(ServerConfig::Application + "." + ServerConfig::ServerName +".PropertyObj");       

        // 读取上报数据入库的时间间隔，单位分钟
        _insertInterval = TC_Common::strto<int>(_conf.get("/Main/HashMap<InsertInterval>", "5"));
        if (_insertInterval < 5)
        {
            _insertInterval = 5;
        }

        _propertyNameMap = _conf.getDomainMap("/Main/NameMap");
        auto it = _propertyNameMap.begin();
        while (it != _propertyNameMap.end())
        {
            size_t pos = it->first.find("reservedPro");
            if (pos != string::npos && pos == 0)
            {
                it = _propertyNameMap.erase(it);
                if (it == _propertyNameMap.end())
                    break;
            }
            else
                ++it;
        }

        CacheInfoManager::getInstance()->init(_conf);
        PropertyDbManager::getInstance()->init(_conf);

        initHashMap();

        // 读取cache服务信息更新时间间隔，单位秒
        _updateInterval = TC_Common::strto<int>(_conf.get("/Main/CacheInfo<UpdateInterval>", "600")); 
        
        _updateThread = new CacheInfoUpdateThread(_updateInterval);
        _updateThread->start();

        _reapThread = new PropertyReapThread();
        _reapThread->start();
    }
    catch (exception& ex)
    {
        TLOGERROR("PropertyServer::initialize catch exception:" << ex.what() << endl);
        exit(0);
    }
    catch (...)
    {
        TLOGERROR("PropertyServer::initialize unknow exception catched");
        exit(0);
    }
}

void PropertyServer::initHashMap()
{
    TLOGDEBUG("PropertyServer::initHashMap begin ..." << endl);

    int iMinBlock = TC_Common::strto<int>(_conf.get("/Main/HashMap<MinBlock>", "100"));
    int iMaxBlock = TC_Common::strto<int>(_conf.get("/Main/HashMap<MaxBlock>", "200"));
    float iFactor = TC_Common::strto<float>(_conf.get("/Main/HashMap<Factor>", "1.5"));
    int iSize     = TC_Common::toSize(_conf.get("/Main/HashMap<Size>"), 1024*1024*10);

    string sFile  = ServerConfig::DataPath + "/" + _conf.get("/Main/HashMap<File>", "hashmap.txt");
    string sPath  = TC_File::extractFilePath(sFile);
    if (!TC_File::makeDirRecursive(sPath))
    {
        TLOGERROR("PropertyServer::initHashMap can not create hashmap file path " << sPath << endl);
        exit(0);
    }

    _clonePatch  = ServerConfig::DataPath + "/" + _conf.get("/Main/HashMap<ClonePatch>", "clone");
    if (!TC_File::makeDirRecursive(_clonePatch))
    {
        TLOGERROR("PropertyServer::initHashMap can not create clone file path " << _clonePatch << endl);
        exit(0);
    }

    try
    {
        _hashMap.initDataBlockSize(iMinBlock, iMaxBlock, iFactor);

        if (TC_File::isFileExist(sFile))
        {
            iSize = TC_File::getFileSize(sFile);
        }
        _hashMap.initStore(sFile.c_str(), iSize);
    }
    catch (exception &ex)
    {
        TC_File::removeFile(sFile, false);
        throw runtime_error(ex.what());
    }
    
    TLOGDEBUG("PropertyServer::initHashMap succ" << endl);
}

const string & PropertyServer::getClonePath() const
{
    return _clonePatch;
}

int PropertyServer::getInsertInterval() const
{
    return _insertInterval;
}

const map<string, string> & PropertyServer::getPropertyNameMap() const
{
    return _propertyNameMap;
}

PropertyHashMap & PropertyServer::getHashMap()
{
    return _hashMap;
}

void PropertyServer::destroyApp()
{
    if (_reapThread)
    {
        delete _reapThread;
        _reapThread = NULL;
    }

    if (_updateThread)
    {
        delete _updateThread;
        _updateThread = NULL;
    }

    TLOGDEBUG("PropertyServer::destroyApp ok" << endl);
}

void PropertyServer::getTimeInfo(time_t &tTime, string &sDate, string &sFlag)
{
    // InsertInterval 单位为分钟 
    string sTime, sHour, sMinute;
    time_t t    = TNOW;
    t           = (t / (_insertInterval * 60)) * _insertInterval * 60; //要求必须为Inteval整数倍
    tTime       = t;
    t           = ((t % 3600) == 0) ? (t - 60) : t;                    //要求将9点写作0860  
    sTime       = TC_Common::tm2str(t, "%Y%m%d%H%M");
    sDate       = sTime.substr(0, 8);
    sHour       = sTime.substr(8, 2);
    sMinute     = sTime.substr(10, 2);
    sMinute		= (sMinute == "59") ? "60" : sMinute;				   //要求将9点写作0860
    sMinute		= (TC_Common::strto<int>(sMinute.substr(1)) >= 5)
                ? (sMinute.substr(0, 1) + "5")
                : (sMinute.substr(0, 1) + "0");						   //按五的倍数
    sFlag       = sHour +  sMinute;
}

int main( int argc, char* argv[] )
{
    try
    {
        g_app.main( argc, argv );
        
        g_app.waitForShutdown();
    }
    catch ( exception& ex )
    {
        cout << ex.what() << endl;
    }

    return 0;
}
