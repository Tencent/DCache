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
#include "ReleaseThread.h"

ReleaseRequestQueueManager::ReleaseRequestQueueManager()
{
}

ReleaseRequestQueueManager::~ReleaseRequestQueueManager()
{
}

void ReleaseRequestQueueManager::start(int iThreadNum)
{
    for (int i = 0; i < iThreadNum; i++)
    {
        ReleaseThread * t = new ReleaseThread(this);
        t->start();

        _releaseRunners.push_back(t);
    }
}

void ReleaseRequestQueueManager::terminate()
{
    for (size_t i = 0; i < _releaseRunners.size(); ++i)
    {
        if (_releaseRunners[i]->isAlive())
        {
            _releaseRunners[i]->terminate();
        }
    }

    for (size_t i = 0; i < _releaseRunners.size(); ++i)
    {
        if (_releaseRunners[i]->isAlive())
        {
            _releaseRunners[i]->getThreadControl().join();
        }
    }
}

void ReleaseRequestQueueManager::push_back(ReleaseRequest * request)
{
    TC_ThreadLock::Lock lock(_requestMutex);

    _requestQueue.push_back(request);

    TC_ThreadLock::Lock lock1(*this);
    notifyAll();
}

bool ReleaseRequestQueueManager::pop_front(ReleaseRequest ** request)
{
    TC_ThreadLock::Lock lock(_requestMutex);

    if (_requestQueue.empty())
    {
        return false;
    }

    *request = _requestQueue.front();
    _requestQueue.pop_front();

    return true;
}

void ReleaseRequestQueueManager::addProgressRecord(int requestId, ReleaseRequest * request)
{
    TC_ThreadLock::Lock lock(_progressMutex);
    ReleaseProgress tmpProgress;
    tmpProgress.releaseStatus.percent   = 0;
    tmpProgress.releaseStatus.status    = RELEASING;
    tmpProgress.releaseStatus.errmsg    = "";
    tmpProgress.releaseReq              = request;
    _releaseProgress[requestId] = tmpProgress;
}

void ReleaseRequestQueueManager::setProgressRecord(int requestId, int percent, ReStatus status, const string &errmsg)
{
    TC_ThreadLock::Lock lock(_progressMutex);

    map<int, ReleaseProgress>::iterator it = _releaseProgress.find(requestId);
    if (it == _releaseProgress.end())
    {
        TLOGDEBUG(FUN_LOG << "request id:" << requestId << "no found" << endl);
        return;
    }

    it->second.releaseStatus.percent    = percent;
    it->second.releaseStatus.status     = status;
    it->second.releaseStatus.errmsg     = errmsg;
}

ReleaseStatus ReleaseRequestQueueManager::getProgressRecord(int requestId, vector<ReleaseInfo> & releaseInfo)
{
    TC_ThreadLock::Lock lock(_progressMutex);

    map<int, ReleaseProgress>::iterator it = _releaseProgress.find(requestId);
    if (it == _releaseProgress.end())
    {
        throw runtime_error(TC_Common::tostr(requestId) + " no found");
    }
    releaseInfo = it->second.releaseReq->releaseInfo;

    return it->second.releaseStatus;
}

void ReleaseRequestQueueManager::deleteProgressRecord(int requestId)
{
    TC_ThreadLock::Lock lock(_progressMutex);

    map<int, ReleaseProgress>::iterator it = _releaseProgress.find(requestId);
    if (it == _releaseProgress.end())
    {
        TLOGDEBUG(FUN_LOG << "request id:" << requestId << " has been deleted" << endl);
        return;
    }
    delete it->second.releaseReq;
    _releaseProgress.erase(requestId);
}

size_t ReleaseRequestQueueManager::getReleaseQueueSize()
{
    TC_ThreadLock::Lock lock(_requestMutex);
    return _requestQueue.size();
}

//////////////////////////////////////////////////////////////////////
ReleaseThread::ReleaseThread(ReleaseRequestQueueManager* rrqm)
: _queueManager(rrqm), _shutDown(false)
{
    _communicator = Application::getCommunicator();

    TC_Config tcConf;
    tcConf.parseFile(ServerConfig::BasePath + "DCacheOptServer.conf");

    _adminproxy = _communicator->stringToProxy<AdminRegPrx>(tcConf.get("/Main/<AdminRegObj>", "tars.tarsAdminRegistry.AdminRegObj"));
}

ReleaseThread::~ReleaseThread()
{
}

void ReleaseThread::run()
{
    while (!_shutDown)
    {
        try
        {
            {
                TC_ThreadLock::Lock lock(*_queueManager);

                if (_queueManager->getReleaseQueueSize() == 0)
                {
                    _queueManager->timedWait(200);
                }
            }

            ReleaseRequest* request;
            if (_queueManager->pop_front(&request))
            {
                _queueManager->addProgressRecord(request->requestId, request);
                doReleaseRequest(request);
            }
        }
        catch (exception& e)
        {
            TLOGERROR(FUN_LOG << "catch exception:" << e.what() << endl);
        }
        catch (...)
        {
            TLOGERROR(FUN_LOG << "catch unkown exception" << endl);
        }
    }
}

void ReleaseThread::terminate()
{
    _shutDown = true;
}

void ReleaseThread::doReleaseRequest(ReleaseRequest* request)
{
    TLOGDEBUG(FUN_LOG<< "release request id:" << request->requestId << endl);
    string errmsg("");

    try
    {
        int iUnit = 0;
        if (request->releaseInfo.size() > 0)
        {
            iUnit = 100 / request->releaseInfo.size();
        }
        else
        {
            _queueManager->setProgressRecord(request->requestId, 100, RELEASE_FINISH);
        }

        int iPercent = 0;
        for (size_t i = 0; i < request->releaseInfo.size(); ++i)
        {
            int iRet = releaseServer(request->releaseInfo[i], errmsg);
            if (iRet != 0)
            {
                request->releaseInfo[i].status = -1;
                request->releaseInfo[i].errmsg = errmsg;
                _queueManager->setProgressRecord(request->requestId, iPercent, RELEASE_FAILED, errmsg);
                continue;
            }

            request->releaseInfo[i].status = 1;
            iPercent += iUnit;
            if (i == (request->releaseInfo.size() - 1))
            {
                _queueManager->setProgressRecord(request->requestId, 100, RELEASE_FINISH);
                break;
            }

            _queueManager->setProgressRecord(request->requestId, iPercent, RELEASING);
        }

        return;
    }
    catch (exception &ex)
    {
        errmsg = string("release server catch exception:") + ex.what() + "|request id:" + TC_Common::tostr(request->requestId);
        TLOGERROR(FUN_LOG << errmsg << endl);
    }
    catch(...)
    {
        errmsg = string("catch unknown exception|request id:") + TC_Common::tostr(request->requestId);
        TLOGERROR(FUN_LOG << errmsg << endl);
    }

    _queueManager->setProgressRecord(request->requestId, 0, RELEASE_FAILED, errmsg);
}

int ReleaseThread::releaseServer(ReleaseInfo &serverInfo, string & errmsg)
{
    string resultStr;

    TLOGDEBUG(FUN_LOG << "release app name:" << serverInfo.appName << "|server name:" << serverInfo.serverName << "|ip: " << serverInfo.nodeName << "|version:" << serverInfo.version << endl);

    tars::PatchRequest  req;
    req.appname     = serverInfo.appName;
    req.servername  = serverInfo.serverName;
    req.nodename    = serverInfo.nodeName;
    req.groupname   = serverInfo.groupName;
    req.binname     = "";
    req.version     = serverInfo.version;
    req.user        = serverInfo.user;
    req.servertype  = "";
    req.patchobj    = "";
    req.md5         = serverInfo.md5;
    req.ostype      = serverInfo.ostype;
    //req.filepath    = ""; //可指定带路径的发布包文件名
    TLOGDEBUG(FUN_LOG << "release server batch patch message: app name:" << req.appname << "|server name:" << req.servername << "|server ip:" << req.nodename << "|group name:" << req.groupname << "|version:" << req.version << "|md5:" << req.md5 << "|ostype:" << req.ostype << endl);

    // 批量 patch 服务
    int iRet = _adminproxy->batchPatch(req, resultStr);
    if (iRet != 0)
    {
        errmsg = string("patch server failed|servername:") + req.servername + "|server ip:" + req.nodename + "|errmsg:" + resultStr;
        TLOGERROR(FUN_LOG << errmsg << endl);
        return -1;
    }

    PatchInfo patchInfo;
    while(true)
    {
        if (0 != _adminproxy->getPatchPercent(req.appname, req.servername, req.nodename, patchInfo))
        {
            errmsg = string("get patch percent failed|servername:") + req.servername + "|server ip:" + req.nodename + "|patch server result:" + patchInfo.sResult;
            TLOGERROR(FUN_LOG << errmsg << endl);
            return -1;
        }

        serverInfo.percent = patchInfo.iPercent;

        if (!patchInfo.bPatching && patchInfo.iPercent == 100)
        {
            TLOGDEBUG(FUN_LOG << "release progress:" << patchInfo.iPercent << "%" << endl);
            break;
        }
        else
        {
            TLOGDEBUG(FUN_LOG << "release progress:" << patchInfo.iPercent << "%" << endl);
        }

        sleep(1);
    }

    TLOGDEBUG(FUN_LOG << "patch server successfully" << endl);

    // 启动服务
    iRet = _adminproxy->startServer("DCache", req.servername, req.nodename, resultStr);
    if (iRet != 0)
    {
        errmsg = string("start server failed|servername:") + req.servername + "|server ip:" + req.nodename + "|errmsg:" + resultStr;
        TLOGERROR(FUN_LOG << errmsg << endl);
        return -1;
    }

    int times = 0;//重试次数
    while(1)
    {
        //等待启动
        sleep(1);

        tars::ServerStateDesc state;
        iRet = _adminproxy->getServerState("DCache", req.servername, req.nodename, state, resultStr);
        if ((iRet != 0) || (state.presentStateInNode != "Active" && state.presentStateInNode != "active"))
        {
            if (times < 5)
            {
                times++;
            }
            else
            {
                errmsg = string("adminproxy: server state is unusual|servername:") + req.servername + "|server ip:" + req.nodename + "|cache server may be inactive! getServerState result:" + resultStr;
                TLOGERROR(FUN_LOG << errmsg << endl);
                // 服务未正常启动，不终止流程，继续发布其他服务
                break;
            }
        }
        else
        {
            break;
        }
    }

    TLOGDEBUG(FUN_LOG << "release server finish, result:" << (iRet == 0 ? "success" : "failed") << "|appname:" << req.appname << "|servername:" << req.servername << "|server ip:" << req.nodename << endl);

    return 0;
}


