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
#ifndef __ETCDREQUESTCALLBACK_H__
#define __ETCDREQUESTCALLBACK_H__

#include <queue>
#include "servant/Application.h"
#include "util/tc_autoptr.h"
#include "util/tc_common.h"
#include "util/tc_http_async.h"
#include "util/tc_monitor.h"
#include "util/tc_thread.h"

#define MSTIMEINSTR(x) (tars::TC_Common::tm2str((x) / 1000, "%Y-%m-%d %H:%M:%S"))

class RspHandler;

enum EtcdRet
{
    RET_SUCC = 0,    // http正常
    RET_EXCEPTION,   // http异常
    RET_TIMEOUT,     // http超时
    RET_CLOSE,       // http on close
    RET_ERROR_CODE,  // etcd 返回error
};

inline std::string EtcdRetToStr(EtcdRet e)
{
    switch (e)
    {
        case RET_SUCC:
            return "RET_SUCC";
        case RET_EXCEPTION:
            return "RET_EXCEPTION";
        case RET_TIMEOUT:
            return "RET_TIMEOUT";
        case RET_CLOSE:
            return "RET_CLOSE";
        case RET_ERROR_CODE:
            return "RET_ERROR_CODE";
        default:
            return "unknown";
    }
}

enum EtcdAction
{
    ETCD_CAS_REFRESH = 0,  // ETCD原子地对Key保活
};

struct EtcdRequestInfo
{
    EtcdRequestInfo() : startTimeMs(0), action(ETCD_CAS_REFRESH), etcdPort(0), ttl(0) {}

    int64_t startTimeMs;           // 请求开始的时间
    enum EtcdAction action;        // 对ETCD的操作
    std::string etcdHost;          // 请求IP
    uint32_t etcdPort;             // 请求端口
    std::string appName;           // app名
    std::string moduleName;        // 模块名
    std::string groupName;         // 分组名
    std::string serverName;        // 服务名
    tars::TarsCurrentPtr current;  // JceCurrent句柄
    std::string value;             // etcd key的value
    int ttl;                       // etcd key的ttl

    std::string actionInString() const
    {
        switch (action)
        {
            case ETCD_CAS_REFRESH:
                return "ETCD_CAS_REFRESH";
            default:
                return "unknown";
        }
    }

    // 将EtcdRequestInfo对象转换为字符串，用于打印日志
    std::string toString() const;
};

struct EtcdRspItem
{
    EtcdRspItem() : ret(RET_SUCC), errorCode(0) {}

    enum EtcdRet ret;
    int errorCode;
    EtcdRequestInfo reqInfo;
    std::string respContent;
};

class EtcdRequestCallback : public tars::TC_HttpAsync::RequestCallback
{
public:
    EtcdRequestCallback(const EtcdRequestInfo &info, std::shared_ptr<RspHandler> h)
        : _reqInfo(info), _handler(h)
    {
    }
    virtual void onResponse(bool isClose, tars::TC_HttpResponse &httpResponse);

    virtual void onException(const std::string &exMsg);

    virtual void onTimeout();

    virtual void onClose();

private:
    void handleErrorCode(int errCode);

    void handleRspSucc(const std::string &respContent);

private:
    const EtcdRequestInfo _reqInfo;
    std::shared_ptr<RspHandler> _handler;
};

typedef tars::TC_AutoPtr<EtcdRequestCallback> EtcdRequestCallbackPtr;

#endif  // __ETCDREQUESTCALLBACK_H__
