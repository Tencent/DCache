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
#include "RouterHandle.h"

RouterHandle::RouterHandle() {}

RouterHandle::~RouterHandle() {}

void RouterHandle::init(TC_Config &conf)
{
    _routeObj = conf["/Main<RouterObj>"];
    _routerEndpoints = Application::getCommunicator()->getEndpoint4All(_routeObj);

    if (_routerEndpoints.empty())
    {
        TLOGERROR("[RouterHandle::init] Endpoint of " << _routeObj << " is 0" << endl);
        TARS_NOTIFY_WARN(string("[RouterHandle::init] have no useful Endpoint of ") + _routeObj);
        _lastGetEndpointTime = 0;
    }
    else
    {
        _lastGetEndpointTime = TC_TimeProvider::getInstance()->getNow();
        srand(TC_TimeProvider::getInstance()->getNow());
        _endpointIndex = rand() % _routerEndpoints.size();
        _curEndpoint = _routerEndpoints[_endpointIndex];
        _routerProxy = _routeObj + "@tcp -h " + _curEndpoint.getHost() + " -p " + TC_Common::tostr(_curEndpoint.getPort());
        _routerPrx = Application::getCommunicator()->stringToProxy<RouterPrx>(_routerProxy);
    }

    TLOGDEBUG("RouterHandle::initialize Succ" << endl);
}

int RouterHandle::getRouterInfo(const string &moduleName, PackTable &packTable, bool needRetry)
{
    do
    {
        RouterPrx routerPrx;
        if (getRouterPrx(routerPrx) != 0)
        {
            TLOGERROR("[RouterHandle::getRouterInfo] have no useful end_point for " << _routeObj << endl);
            return -1;
        }
        bool bShouldRetry = false;
        try
        {
            int iRet = routerPrx->getRouterInfo(moduleName, packTable);

            if (iRet != ROUTER_SUCC)
            {
                TLOGERROR("[RouterHandle::getRouterInfo] from " << _routeObj << "@" << routerPrx->tars_invoke_endpoint().getHost() << " for module: " << moduleName << " failed, ret = " << iRet << endl);
                if (iRet == ROUTER_SYS_ERR)
                {
                    bShouldRetry = true;
                }
            }
            else
            {
                return 0;
            }
        }
        catch (const exception &ex)
        {
            TLOGERROR("[RouterHandle::getRouterInfo] exception:" << ex.what() << endl);
            bShouldRetry = true;
        }

        int iRet = 0;
        if (bShouldRetry)
        {
            iRet = updateRouterPrx(routerPrx);
        }
        else
        {
            return -1;
        }

        if (needRetry)
        {
            if (iRet != 0)
            {
                return -1;
            }
            needRetry = false;
        }
        else
        {
            return -1;
        }
    } while (1);

    return 0;
}

int RouterHandle::getRouterVersion(const string &moduleName, int &version, bool needRetry)
{

    do
    {
        RouterPrx routerPrx;
        if (getRouterPrx(routerPrx) != 0)
        {
            TLOGERROR("[RouterHandle::getRouterVersion] have no useful end_point for " << _routeObj << endl);
            return -1;
        }
        bool bShouldRetry = false;
        try
        {
            int iRet = routerPrx->getVersion(moduleName);

            if (iRet < 0)
            {
                TLOGERROR("[RouterHandle::getRouterVersion] from " << _routeObj << "@" << routerPrx->tars_invoke_endpoint().getHost() << " for module: " << moduleName << " failed, ret = " << iRet << endl);
                if (iRet == ROUTER_SYS_ERR)
                {
                    bShouldRetry = true;
                }
            }
            else
            {
                version = iRet;
                return 0;
            }
        }
        catch (const exception &ex)
        {
            TLOGERROR("[RouterHandle::getRouterVersion] exception:" << ex.what() << endl);
            bShouldRetry = true;
        }

        int iRet = 0;
        if (bShouldRetry)
        {
            iRet = updateRouterPrx(routerPrx);
        }
        else
        {
            return -1;
        }

        if (needRetry)
        {
            if (iRet != 0)
            {
                return -1;
            }
            needRetry = false;
        }
        else
        {
            return -1;
        }
    } while (1);

    return 0;
}

int RouterHandle::getRouterVersionBatch(const vector<string> &moduleList, map<string, int> &mapModuleVersion, bool needRetry)
{
    do
    {
        RouterPrx routerPrx;
        if (getRouterPrx(routerPrx) != 0)
        {
            TLOGERROR("[RouterHandle::getRouterVersion] have no useful end_point for " << _routeObj << endl);
            return -1;
        }
        bool bShouldRetry = false;
        try
        {
            int iRet = routerPrx->getRouterVersionBatch(moduleList, mapModuleVersion);

            if (iRet < 0)
            {
                TLOGERROR("[RouterHandle::getRouterVersionBatch] from " << _routeObj << "@" << routerPrx->tars_invoke_endpoint().getHost() << " failed, ret = " << iRet << endl);
                if (iRet == ROUTER_SYS_ERR)
                {
                    bShouldRetry = true;
                }
            }
            else
            {
                return 0;
            }
        }
        catch (const exception &ex)
        {
            TLOGERROR("[RouterHandle::getRouterVersion] exception:" << ex.what() << endl);
            bShouldRetry = true;
        }

        int iRet = 0;
        if (bShouldRetry)
        {
            iRet = updateRouterPrx(routerPrx);
        }
        else
        {
            return -1;
        }

        if (needRetry)
        {
            if (iRet != 0)
            {
                return -1;
            }
            needRetry = false;
        }
        else
        {
            return -1;
        }
    } while (1);

    return 0;
}

int RouterHandle::reportSwitchGroup(const string &moduleName, const string &groupName, const string &serverName)
{
    int iTryTime = 0;
    do
    {
        if (iTryTime > 3)
            break;
        RouterPrx routerPrx;
        if (getRouterPrx(routerPrx) != 0)
        {
            TLOGERROR("[RouterHandle::reportSwitchGroup] have no useful end_point for " << _routeObj << endl);
            return -1;
        }
        bool bShouldRetry = false;
        try
        {
            // routerPrx->reportSwitchGroup(moduleName, groupName,serverName);  // 该方法在Router端被弃用了 dengyouwang
            return 0;
        }
        catch (const exception &ex)
        {
            TLOGERROR("[RouterHandle::reportSwitchGroup] exception:" << ex.what() << endl);
            bShouldRetry = true;
        }

        if (bShouldRetry)
        {
            updateRouterPrx(routerPrx);
        }
        iTryTime++;
    } while (1);

    return -1;
}

int RouterHandle::getModuleList(vector<string> &moduleList)
{
    int iTryTime = 0;
    do
    {
        if (iTryTime > 3)
            break;

        RouterPrx routerPrx;
        if (getRouterPrx(routerPrx) != 0)
        {
            TLOGERROR("[RouterHandle::getModuleList] have no useful end_point for " << _routeObj << endl);
            return -1;
        }
        bool bShouldRetry = false;
        try
        {
            routerPrx->getModuleList(moduleList);
            return 0;
        }
        catch (const exception &ex)
        {
            TLOGERROR("[RouterHandle::getModuleList] exception:" << ex.what() << endl);
            bShouldRetry = true;
        }

        if (bShouldRetry)
        {
            updateRouterPrx(routerPrx);
        }
        iTryTime++;
    } while (1);

    return -1;
}

int RouterHandle::getRouterPrx(RouterPrx &routerPrx)
{
    TC_ThreadLock::Lock lock(_lock);

    if (_routerEndpoints.empty())
    {
        _routerEndpoints = Application::getCommunicator()->getEndpoint4All(_routeObj);
        _lastGetEndpointTime = TC_TimeProvider::getInstance()->getNow();
        if (_routerEndpoints.empty())
        {
            return -1;
        }

        srand(TC_TimeProvider::getInstance()->getNow());
        _endpointIndex = rand() % _routerEndpoints.size();
        _curEndpoint = _routerEndpoints[_endpointIndex];
        _routerProxy = _routeObj + "@tcp -h " + _curEndpoint.getHost() + " -p " + TC_Common::tostr(_curEndpoint.getPort());
    }

    routerPrx = Application::getCommunicator()->stringToProxy<RouterPrx>(_routerProxy);
    return 0;
}

int RouterHandle::updateRouterPrx(RouterPrx &routerPrx)
{
    TC_ThreadLock::Lock lock(_lock);

    time_t tNow = TC_TimeProvider::getInstance()->getNow();
    if (tNow - _lastGetEndpointTime >= 60)
    {
        _routerEndpoints = Application::getCommunicator()->getEndpoint4All(_routeObj);
        _lastGetEndpointTime = tNow;
    }

    if (_routerEndpoints.empty())
    {
        return -1;
    }
    else if (_routerEndpoints.size() == 1)
    {
        if (_routerEndpoints[0] == _curEndpoint)
        {
            return -2;
        }
        else
        {
            _curEndpoint = _routerEndpoints[0];
        }
    }
    else
    {
        srand(TC_TimeProvider::getInstance()->getNow());
        _endpointIndex = rand() % _routerEndpoints.size();
        if (_curEndpoint == _routerEndpoints[_endpointIndex])
        {
            _endpointIndex = (_endpointIndex >= _routerEndpoints.size() - 1) ? 0 : _endpointIndex + 1;
        }
        _curEndpoint = _routerEndpoints[_endpointIndex];
    }

    _routerProxy = _routeObj + "@tcp -h " + _curEndpoint.getHost() + " -p " + TC_Common::tostr(_curEndpoint.getPort());
    _routerPrx = Application::getCommunicator()->stringToProxy<RouterPrx>(_routerProxy);

    TLOGDEBUG(_curEndpoint.getHost() << ":" << _curEndpoint.getPort() << endl);

    return 0;
}

