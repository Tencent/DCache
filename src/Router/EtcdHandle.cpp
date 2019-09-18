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
#include "EtcdHandle.h"
#include "EtcdHost.h"
#include "EtcdHttp.h"
#include "RouterServer.h"
#include "string.h"
#include "util/tc_common.h"
#include "util/tc_thread.h"

using namespace tars;

extern RouterServer g_app;

RspHandler::~RspHandler()
{
    if (isAlive())
    {
        terminate();
        getThreadControl().join();
    }
}

void RspHandler::terminate()
{
    _terminate = true;
    TC_ThreadLock::Lock lock(*this);
    notifyAll();
}

void RspHandler::run()
{
    while (!_terminate)
    {
        try
        {
            std::shared_ptr<EtcdRspItem> item = nullptr;
            bool found = true;

            {
                TC_ThreadLock::Lock lock(_queue_lock);

                if (_queue.size() > 0)
                {
                    item = _queue.front();
                    _queue.pop();
                }
                else
                {
                    found = false;
                }
            }

            if (!found)
            {
                TC_ThreadLock::Lock lock(*this);
                timedWait(50);
                continue;
            }

            handleEtcdRsp(item);
        }
        catch (const std::exception &ex)
        {
            TLOGERROR("RspHandler::run exception: " << ex.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("RspHandler::run unkown exception" << endl);
        }
    }
}

void RspHandler::handleEtcdRsp(const std::shared_ptr<EtcdRspItem> &item) const
{
    try
    {
        EtcdRet rc = item->ret;
        EtcdAction action = item->reqInfo.action;

        TLOGDEBUG(FILE_FUN << " Action:" << item->reqInfo.actionInString()
                           << "| ret:" << EtcdRetToStr(rc) << endl);

        if (RET_SUCC == rc)
        {
            switch (action)
            {
                case ETCD_CAS_REFRESH:
                    handleCommon(item->respContent);
                    g_app.updateLastHeartbeat(item->reqInfo.startTimeMs / 1000);
                    break;
                default:
                    break;
            }
        }
        else if (RET_EXCEPTION == rc || RET_TIMEOUT == rc)
        {
            // tc_http在onException/onTimeout时都会先执行doClose触发onClose.
            // 所以这里不需要做处理
        }
        else if (RET_CLOSE == rc)
        {
        }
        else if (RET_ERROR_CODE == rc)
        {
            switch (action)
            {
                case ETCD_CAS_REFRESH:
                    //维持心跳失败，本机成为router slave
                    // err 100, key not found
                    // err 101, prevValue not right
                    g_app.setRouterType(ROUTER_SLAVE);
                    break;
                default:
                    break;
            }
        }
        else
        {
            TLOGERROR("RspHandler::handleEtcdRsp unknow eRet: " << rc << endl);
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("RspHandler::handleEtcdRsp exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("RspHandler::handleEtcdRsp unkown exception" << endl);
    }
}

void RspHandler::handleCommon(const std::string &respContent) const
{
    try
    {
        EtcdHttpParser parser;
        if (parser.parseContent(respContent) != 0)
        {
            TLOGERROR(FILE_FUN << " parse json error|" << respContent << endl);
            return;
        }

        std::string action;
        (void)parser.getAction(&action);

        std::string key;
        if (parser.getCurrentNodeKey(&key) != 0)
        {
            TLOGERROR(FILE_FUN << " can not find key." << endl);
        }
        else
        {
            TLOGDEBUG(FILE_FUN << " key : " << key << " action : " << action << endl);
        }
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("RspHandler::handleCommon exception: " << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("RspHandler::handleCommon unkown exception" << endl);
    }
}

void RspHandler::addRspItem(std::shared_ptr<EtcdRspItem> item)
{
    TC_ThreadLock::Lock lock(_queue_lock);
    _queue.push(item);
}

int EtcdHandle::init(const RouterServerConfig &config, std::shared_ptr<EtcdHost> etcdHost)
{
    try
    {
        _appName = config.getAppName("");
        if (_appName == "")
        {
            TLOGERROR(FILE_FUN << "read AppName config error!" << endl);
            return -1;
        }

        // 开启一个EtcdHost线程，定时去更新Etcd主机的信息
        _etcdHost = etcdHost;
        if (_etcdHost->init(config) != 0)
        {
            TLOGERROR(FILE_FUN << "EtcdHost initialize error" << endl);
            return -1;
        }
        _etcdHost->start();

        // 开启一个异步HTTP线程，去执行HTTP请求。
        int httpTimeout = config.getEtcdReqTimeout(3);
        _asyncHttp.setTimeout(httpTimeout * 1000);
        _asyncHttp.start();

        // 开启一个线程，去处理异步HTTP请求的响应
        _rspHandler = std::make_shared<RspHandler>();
        _rspHandler->start();
    }
    catch (const std::exception &ex)
    {
        TLOGERROR(FILE_FUN << " exception:" << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR(FILE_FUN << " unknow exception:" << endl);
        return -1;
    }
    return 0;
}

int EtcdHandle::getRouterObj(std::string *obj) const
{
    std::string etcdUrl = _etcdHost->getRouterHost();
    return doGetEtcd(etcdUrl, obj);
}

int EtcdHandle::doGetEtcd(const std::string &url, std::string *keyvalue) const
{
    try
    {
        std::unique_ptr<EtcdHttpRequest> httpReq = makeEtcdHttpRequest(url, HTTP_GET);
        httpReq->setDefaultHeader();
        std::string resp;
        int rc = httpReq->doRequest(DEFAULT_HTTP_REQUEST_TIMEOUT, &resp);
        if (rc != 0)
        {
            TLOGERROR("doGetEtcd error,url:" << httpReq->dumpURL() << "|" << rc << endl);
            return -1;
        }

        TLOGDEBUG("doGetEtcd url:" << httpReq->dumpURL() << "| resp content" << resp << endl);

        EtcdHttpParser p;
        if (p.parseContent(resp) != 0)
        {
            TLOGERROR("parseContent error, content:" << resp << "parse error" << endl);
            return -1;
        }

        int errCode;
        std::string errMsg;
        if (p.getEtcdError(&errCode, &errMsg) != 0)
        {
            TLOGERROR("etcd error, url:" << httpReq->dumpURL() << "| errorMsg:" << errMsg << endl);
            return -1;
        }

        if (p.getCurrentNodeValue(keyvalue) != 0)
        {
            TLOGERROR("doGetEtcd error,url:" << httpReq->dumpURL() << "|no value!" << endl);
            return -1;
        }

        TLOGDEBUG("doGetEtcd " << httpReq->dumpURL() << " | parse got value :" << *keyvalue
                               << endl);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("doGetEtcd exception " << ex.what() << endl);
        return -1;
    }
    catch (...)
    {
        TLOGERROR("doGetEtcd: unknow exception " << endl);
        return -1;
    }

    return 0;
}

int EtcdHandle::createCAS(int ttl, const std::string &keyvalue) const
{
    std::string etcdUrl = _etcdHost->getRouterHost();

    std::unique_ptr<EtcdHttpRequest> httpReq = makeEtcdHttpRequest(etcdUrl, HTTP_PUT);
    httpReq->setDefaultHeader();
    httpReq->setPrevExist(false);
    httpReq->setKeyTTL(ttl);
    httpReq->setValue(keyvalue);

    std::string resp;
    int rc = httpReq->doRequest(DEFAULT_HTTP_REQUEST_TIMEOUT, &resp);
    if (rc != 0)
    {
        TLOGERROR(FILE_FUN << httpReq->dumpURL() << "|" << keyvalue << "| error:" << rc << endl);
        return -1;
    }

    EtcdHttpParser p;
    if (p.parseContent(resp) != 0)
    {
        TLOGERROR(FILE_FUN << resp << "parse error" << endl);
        return -1;
    }

    // 在ETCD的HTTP API中，对Key的请求根据body内容的不同，返回的action字段也不同。
    // PUT方法中，请求body中存在 prevExist=true时，action为update
    // prevExist=false时，action为create； 其他为set。
    std::string action;
    rc = p.getAction(&action);
    if (rc == 0 && action == "create")
    {
        TLOGDEBUG(FILE_FUN << " createCAS [" << etcdUrl << " : " << keyvalue << "] succ."
                           << "resp : " << resp << endl);
        return 0;
    }

    TLOGERROR(FILE_FUN << " createCAS [" << etcdUrl << " : " << keyvalue << "] failed."
                       << "ETCD resp content : " << resp << endl);
    return -1;
}

int EtcdHandle::refreshCAS(int ttl, const std::string &prevValue, const bool sync)
{
    std::string etcdUrl = _etcdHost->getRouterHost();

    std::unique_ptr<EtcdHttpRequest> httpReq = makeEtcdHttpRequest(etcdUrl, HTTP_PUT);
    httpReq->setDefaultHeader();
    httpReq->setKeyTTL(ttl);
    httpReq->setRefresh(true);
    httpReq->setPrevExist(true);
    httpReq->setPrevValue(prevValue);

    if (sync)
    {
        std::string resp;
        int rc = httpReq->doRequest(DEFAULT_HTTP_REQUEST_TIMEOUT, &resp);
        if (rc != 0)
        {
            TLOGERROR(FILE_FUN << httpReq->dumpURL() << "|" << prevValue << "| error:" << rc << endl);
            return -1;
        }

        EtcdHttpParser p;
        if (p.parseContent(resp) != 0)
        {
            TLOGERROR(FILE_FUN << resp << "parse error" << endl);
            return -1;
        }

        // 在ETCD的HTTP API中，对Key的请求根据body内容的不同，返回的action字段也不同。
        // PUT方法中，请求body中存在 prevExist=true时，action为update
        // prevExist=false时，action为create； 其他为set。
        std::string action;
        rc = p.getAction(&action);
        if (rc == 0 && action == "compareAndSwap")
        {
            TLOGDEBUG(FILE_FUN << " refreshCAS [" << etcdUrl << " : " << prevValue << "] succ."
                << "resp : " << resp << endl);
            return 0;
        }

        TLOGERROR(FILE_FUN << " refreshCAS [" << etcdUrl << " : " << prevValue << "] failed."
            << "ETCD resp content : " << resp << endl);
    }
    else
    {
        EtcdRequestInfo etcdReqInfo;
        etcdReqInfo.startTimeMs = TNOWMS;
        etcdReqInfo.action = ETCD_CAS_REFRESH;
        etcdReqInfo.current = NULL;

        httpReq->getHostPort(etcdReqInfo.etcdHost, etcdReqInfo.etcdPort);
        getModuleInfo(
            etcdReqInfo.appName, etcdReqInfo.moduleName, etcdReqInfo.groupName, etcdReqInfo.serverName);
        etcdReqInfo.ttl = ttl;
        etcdReqInfo.value = "prevValue=" + prevValue;

        TC_HttpAsync::RequestCallbackPtr callback = new EtcdRequestCallback(etcdReqInfo, _rspHandler);

        TLOGDEBUG(FILE_FUN << httpReq->dumpURL() << "|" << prevValue << endl);

        int rc = _asyncHttp.doAsyncRequest(httpReq->getTCHttpRequest(), callback);
        if (rc == 0)
        {
            return rc;
        }

        std::string s = "send req to " + etcdReqInfo.etcdHost + ":" +
            TC_Common::tostr(etcdReqInfo.etcdPort) +
            ",failure,ret=" + TC_Common::tostr(rc);
        TLOGERROR(FILE_FUN << etcdReqInfo.toString() << "|" << s << "strerror = " << strerror(errno)
            << endl);
    }

    return -1;
}

int EtcdHandle::watchRouterMaster() const
{
    std::string etcdUrl = _etcdHost->getRouterHost();
    std::unique_ptr<EtcdHttpRequest> httpReq = makeEtcdHttpRequest(etcdUrl, HTTP_GET);
    httpReq->setDefaultHeader();
    httpReq->setWatchKey();

    std::string resp;
    int rc = httpReq->doRequest(ETCD_WATCH_REQUEST_TIMEOUT, &resp);
    if (rc != 0)
    {
        TLOGERROR(FILE_FUN << httpReq->dumpURL() << "| error:" << rc << endl);
        return -1;
    }

    EtcdHttpParser p;
    if (p.parseContent(resp) != 0)
    {
        TLOGERROR(FILE_FUN << resp << "parse error" << endl);
        return -1;
    }

    std::string action;
    if (p.getAction(&action) == 0 && action == "expire")
    {
        TLOGDEBUG(FILE_FUN << "router server key expire : " << etcdUrl << endl);
        return 0;
    }
    TLOGERROR(FILE_FUN << "watchRouterMaster error, response : " << resp << endl);
    return -1;
}

void EtcdHandle::destroy()
{
    _etcdHost->terminate();
    _etcdHost->getThreadControl().join();
    _asyncHttp.waitForAllDone(1000);
    // 这里不能调用_asyncHttp.getThreadControl().join()
    // 因为TC_HttpAsync虽然继承自TC_Thread，但是直接覆盖掉了TC_Thread::start方法，不再起线程了。
    _rspHandler->terminate();
    _rspHandler->getThreadControl().join();
}
