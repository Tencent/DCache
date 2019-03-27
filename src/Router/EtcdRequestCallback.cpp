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
#include "EtcdRequestCallback.h"
#include "EtcdHandle.h"
#include "EtcdHttp.h"
#include "global.h"
#include "util/tc_timeprovider.h"

using namespace tars;

std::string EtcdRequestInfo::toString() const
{
    std::string client;
    if (current)
    {
        client = current->getIp();
    }
    else
    {
        client = "NULL";
    }
    return "client=" + client + "|" + actionInString() + "|" + appName + "." + moduleName + "." +
           groupName + "." + serverName + "|etcdhost=" + etcdHost +
           "|etcdport=" + TC_Common::tostr(etcdPort) + "|" + MSTIMEINSTR(startTimeMs);
}

void EtcdRequestCallback::onResponse(bool isClose, TC_HttpResponse &httpResponse)
{
    int64_t finishTime = TNOWMS;
    int64_t timeUsed = finishTime - _reqInfo.startTimeMs;

    try
    {
        std::string respContent = httpResponse.getContent();
        TLOGDEBUG(FILE_FUN << "resp action:" << _reqInfo.actionInString()
                           << "|rsp content:" << respContent << endl);

        EtcdHttpParser p;
        if (p.parseContent(respContent) != 0)
        {
            TLOGERROR(FILE_FUN << " |parse json error|" << respContent << "|"
                               << MSTIMEINSTR(finishTime) << "|use=" << timeUsed << "ms" << endl);
            return;
        }

        int errCode;
        std::string errMsg;
        if (p.getEtcdError(&errCode, &errMsg) != 0)
        {
            TLOGERROR(FILE_FUN << " etcd return error: " << errMsg << ",errorCode:" << errCode
                               << endl);
            handleErrorCode(errCode);
            return;
        }

        handleRspSucc(respContent);
        return;
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("EtcdRequestCallback::onResponse exception" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("EtcdRequestCallback::onResponse unknow exception" << endl);
    }
}

void EtcdRequestCallback::onException(const std::string &exMsg)
{
    int64_t finishTime = TNOWMS;
    int64_t timeUsed = finishTime - _reqInfo.startTimeMs;

    try
    {
        std::shared_ptr<EtcdRspItem> i = std::make_shared<EtcdRspItem>();
        i->ret = RET_EXCEPTION;
        i->errorCode = 0;
        i->reqInfo = _reqInfo;

        _handler->addRspItem(i);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("EtcdRequestCallback::onException exception" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("EtcdRequestCallback::onException unknow exception" << endl);
    }

    TLOGERROR("EtcdRequestCallback::onException action:"
              << _reqInfo.actionInString() << "|" << MSTIMEINSTR(finishTime) << "|use" << timeUsed
              << "ms|exception=" << exMsg << endl);
}

void EtcdRequestCallback::onTimeout()
{
    int64_t finishTime = TNOWMS;
    int64_t timeUsed = finishTime - _reqInfo.startTimeMs;

    try
    {
        std::shared_ptr<EtcdRspItem> i = std::make_shared<EtcdRspItem>();
        i->ret = RET_TIMEOUT;
        i->errorCode = 0;
        i->reqInfo = _reqInfo;

        _handler->addRspItem(i);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("EtcdRequestCallback::onTimeout exception" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("EtcdRequestCallback::onTimeout unknow exception" << endl);
    }

    TLOGERROR("EtcdRequestCallback::onTimeout action:" << _reqInfo.actionInString() << "|"
                                                       << MSTIMEINSTR(finishTime)
                                                       << "|use=" << timeUsed << "ms" << endl);
}

void EtcdRequestCallback::onClose()
{
    try
    {
        std::shared_ptr<EtcdRspItem> i = std::make_shared<EtcdRspItem>();
        i->ret = RET_CLOSE;
        i->errorCode = 0;
        i->reqInfo = _reqInfo;

        _handler->addRspItem(i);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("EtcdRequestCallback::onClose exception" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("EtcdRequestCallback::onClose unknow exception" << endl);
    }

    TLOGDEBUG("EtcdRequestCallback::onClose action:" << _reqInfo.actionInString() << endl);
}

void EtcdRequestCallback::handleErrorCode(int errCode)
{
    try
    {
        std::shared_ptr<EtcdRspItem> i = std::make_shared<EtcdRspItem>();
        i->ret = RET_ERROR_CODE;
        i->errorCode = errCode;
        i->reqInfo = _reqInfo;

        _handler->addRspItem(i);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("EtcdRequestCallback::handleErrorCode exception" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("EtcdRequestCallback::handleErrorCode unknow exception" << endl);
    }
}

void EtcdRequestCallback::handleRspSucc(const std::string &content)
{
    try
    {
        std::shared_ptr<EtcdRspItem> i = std::make_shared<EtcdRspItem>();
        i->ret = RET_SUCC;
        i->errorCode = 0;
        i->reqInfo = _reqInfo;
        i->respContent = content;

        _handler->addRspItem(i);
    }
    catch (const std::exception &ex)
    {
        TLOGERROR("EtcdRequestCallback::handleRspSucc exception" << ex.what() << endl);
    }
    catch (...)
    {
        TLOGERROR("EtcdRequestCallback::handleRspSucc unknow exception" << endl);
    }
}

// void HandleRspThread::addRspItem(const std::shared_ptr<EtcdRspItem> &item)
// {
//     TC_ThreadLock::Lock lock(_queue_lock);
//     _queue.push(item);
// }
