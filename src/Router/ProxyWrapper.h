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
#ifndef __PROXY_WRAPPER_H__
#define __PROXY_WRAPPER_H__
#include "framework/AdminReg.h"
#include "servant/Application.h"

class AdminRegProxyWrapper
{
public:
    typedef std::map<std::string, std::string> TARS_CONTEXT;

    explicit AdminRegProxyWrapper(const std::string &obj)
        : _adminProxy(Application::getCommunicator()->stringToProxy<AdminRegPrx>(obj))
    {
    }

    virtual ~AdminRegProxyWrapper() = default;

    virtual void tars_timeout(int msecond) { _adminProxy->tars_timeout(msecond); }

    virtual tars::Int32 getServerState(const std::string &application,
                                       const std::string &serverName,
                                       const std::string &nodeName,
                                       tars::ServerStateDesc &state,
                                       std::string &result,
                                       const map<string, string> &context = TARS_CONTEXT(),
                                       map<string, string> *pResponseContext = NULL)
    {
        return _adminProxy->getServerState(
            application, serverName, nodeName, state, result, context, pResponseContext);
    }

protected:
    AdminRegProxyWrapper() = default;

private:
    AdminRegPrx _adminProxy;
};

class RouterClientWrapper
{
public:
    explicit RouterClientWrapper(const std::string &obj)
        : _routerClientPrx(Application::getCommunicator()->stringToProxy<RouterClientPrx>(obj))
    {
    }

    virtual ~RouterClientWrapper() = default;

    virtual void tars_timeout(int msecond) { _routerClientPrx->tars_timeout(msecond); }

protected:
    RouterClientWrapper() = default;

private:
    RouterClientPrx _routerClientPrx;
};

#endif  // __PROXY_WRAPPER_H__
