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
#include <sys/time.h>

#include "Router.h"

#include "ProxyServer.h"
#include "ProxyImp.h"
#include "RouterClientImp.h"
#include "CacheProxyFactory.h"
#include "Global.h"

using namespace DCache;
using namespace tars;
using namespace std;

ProxyServer g_app;

void ProxyServer::initialize()
{
    string strErr;

    try
    {

        // 禁止写远程按天日志
        TarsTimeLogger::getInstance()->enableRemote("", false);
        TarsTimeLogger::getInstance()->initFormat("", "%Y%m%d%H");

        addServant<ProxyImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".ProxyObj");
        addServant<RouterClientImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".RouterClientObj");

        addManagementCmds();

        // 载入配置文件
        addConfig("ProxyServer.conf");
        TC_Config conf;
        conf.parseFile(ServerConfig::BasePath + "ProxyServer.conf");

        initStatReport(conf);

        initRouter(conf);

        initOthers(conf);

        TLOGDEBUG("DCache.ProxyServer init succ" << endl);
        TARS_NOTIFY_NORMAL("[ProxyServer::init] |succ|");
        return;
    }
    catch (exception &e)
    {
        strErr = string("[ProxyServer::init] exception:") + e.what();
    }
    catch (...)
    {
        strErr = "[ProxyServer::init] UnkownException";
    }

    TLOGERROR(strErr << endl);
    TARS_NOTIFY_ERROR(strErr);
    exit(-1);
}

void ProxyServer::initOthers(TC_Config &conf)
{
    // 是否记录批量接口中的key的个数
    string sLogBatchCount = conf.get("/Main<logBatchCount>", "N");
    _printBatchCount = (sLogBatchCount == "Y" || sLogBatchCount == "y") ? true : false;

    // 是否限制批量key个数
    _keyCountLimit = TC_Common::strto<unsigned int>(conf.get("/Main/<keyCountLimit>", "0"));

    // 重新加载配置文件的时间间隔
    _lastReloadConfTime = TNOWMS;
    _reloadConfInterval = TC_Common::strto<int>(conf.get("/Main<ReloadConfInterval>", "1"));
}

void ProxyServer::initStatReport(TC_Config &conf)
{
    // 是否按调用的模块进行统计上报
    _needStatReport = TC_Common::strto<bool>(conf.get("/Main<bReportStat>", "1"));

    int reportInterval = TC_Common::strto<int>(conf.get("/Main<interval>", "10"));
    string sStatObj = conf.get("/Main<statObj>", "tars.tafstat.StatObj");

    // 启动统计上报线程
    _statThread = new StatThread();
    _statThread->init(sStatObj, reportInterval);
    _statThread->start();

    TLOGDEBUG(__FUNCTION__ << "|" << __LINE__ << "|sStatObj:" << sStatObj
                           << "|_needStatReport:" << _needStatReport << "|reportInterval:" << reportInterval << "s" << endl);
}

// 初始化与路由表相关的对象
void ProxyServer::initRouter(TC_Config &conf)
{
    // 初始化RouterHandle
    RouterHandle::getInstance()->init(conf);

    // 初始化RouterTableInfoFactory
    _routerTableInfoFactory = new RouterTableInfoFactory(RouterHandle::getInstance());
    int iRet = _routerTableInfoFactory->init(conf);
    if (iRet)
    {
        TLOGERROR("init RouterTableInfoFactory failed, no RouterTable can be used" << endl);
    }

    // 启动定时线程，主动拉取并同步路由信息
    _timerThread = new TimerThread(TC_TimeProvider::getInstance()->getNow());
    _timerThread->init(conf);
    _timerThread->start();
}

// 添加管理命令
void ProxyServer::addManagementCmds()
{
    TARS_ADD_ADMIN_CMD_NORMAL("help", ProxyServer::help)
    TARS_ADD_ADMIN_CMD_NORMAL("reloadconf", ProxyServer::reloadConf);
    TARS_ADD_ADMIN_CMD_NORMAL("showstatus", ProxyServer::showStatus);
    TARS_ADD_ADMIN_CMD_NORMAL("module", ProxyServer::module);
}

void ProxyServer::destroyApp()
{
    if (_timerThread)
    {
        TC_ThreadControl timerThreadControl = _timerThread->getThreadControl();
        _timerThread->stop();
        timerThreadControl.join();
    }

    if (_routerTableInfoFactory)
    {
        delete _routerTableInfoFactory;
    }

    if (_timerThread && !_timerThread->isAlive())
    {
        delete _timerThread;
        TLOGDEBUG("destroy pointer of TimerThread succ" << endl);
    }
}

/*显示proxy提供的管理命令*/
bool ProxyServer::help(const string &command, const string &params, string &sResult)
{
    sResult = "命令：        功能\n";
    sResult += "reloadconf：  重新加载配置\n";
    sResult += "showstatus：  显示Proxy当前状态信息\n";
    sResult += "module：   查询Proxy是否支持某个模块";
    return true;
}

/*重新加载配置文件*/
bool ProxyServer::reloadConf(const string &command, const string &params, string &sResult)
{
    string strErr;
    try
    {
        int64_t nowus = TC_Common::now2ms();
        if ((nowus - _lastReloadConfTime) / 1000 < _reloadConfInterval)
        {
            TLOGDEBUG(__FUNCTION__ << " load frequently, Interval: "
                                   << _reloadConfInterval << "s" << endl);
            sResult = "load frequently, Interval: " + TC_Common::tostr<int>(_reloadConfInterval) + "s";
            return false;
        }
        addConfig("ProxyServer.conf");
        TC_Config conf;
        conf.parseFile(ServerConfig::BasePath + "ProxyServer.conf");

        // 是否记录批量接口中的key的个数
        string sLogBatchCount = conf.get("/Main<logBatchCount>", "N");
        _printBatchCount = (sLogBatchCount == "Y" || sLogBatchCount == "y") ? true : false;

        // 是否限制批量key个数
        _keyCountLimit = TC_Common::strto<unsigned int>(conf.get("/Main/<keyCountLimit>", "0"));

        sResult = "begin to reload config...\n";
        sResult += "----------------------------------------------------------------------\n";
        sResult += _routerTableInfoFactory->reloadConf(conf);
        _timerThread->reloadConf(conf);

        TARS_NOTIFY_NORMAL("[ProxyServer::reloadConf] |succ|");
        TLOGDEBUG("reload config ok!" << endl);
        sResult += "----------------------------------------------------------------------\n";
        sResult += "reload config ok!";
        _lastReloadConfTime = TC_Common::now2ms();
        return true;
    }
    catch (exception &e)
    {
        strErr = string("[ProxyServer::reloadConf] exception:") + e.what();
    }
    catch (...)
    {
        strErr = "[ProxyServer::reloadConf] UnkownException";
    }

    TARS_NOTIFY_ERROR(strErr);
    TLOGERROR(strErr << endl);
    sResult += strErr;
    return true;
}

/*显示ProxyServer的当前信息*/
bool ProxyServer::showStatus(const string &command, const string &params, string &sResult)
{
    sResult = "begin to show status of ProxyServer ...\n";
    sResult += _routerTableInfoFactory->showStatus();
    sResult += RouterTableInfo::showStatus();
    sResult += _timerThread->showStatus();
    sResult += "---------------------------------------------------------------\n";
    sResult += "show status of ProxyServer ok!";
    return true;
}

bool ProxyServer::module(const string &command, const string &params, string &sResult)
{
    if (params.empty())
    {
        sResult = "useage: MODULE modulename\n";
        sResult += "example: MODULE TestString";
        return true;
    }

    if (_routerTableInfoFactory->supportModule(params))
    {
        sResult = "support " + params;
    }
    else
    {
        sResult = "not support " + params;
    }

    return true;
}


