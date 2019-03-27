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
#ifndef __CACHE_PROXY_FACTORY_H_
#define __CACHE_PROXY_FACTORY_H_

#include "util/tc_common.h"
#include "util/tc_config.h"
#include "util/tc_monitor.h"
#include "servant/Application.h"
#include "Cache.h"
#include "WCache.h"
#include "MKCache.h"
#include "MKWCache.h"

#include "RouterTableInfoFactory.h"

using namespace tars;
using namespace DCache;

class CacheProxyFactory
{
  public:
    CacheProxyFactory(const CommunicatorPtr &communicator, RouterTableInfoFactory *routeTableFactory);

    ~CacheProxyFactory();

    template <class T>
    int getWCacheProxy(const string &moduleName, const string &key, string &objectName, T &prxCache, const bool updateRouteTable = false);

    template <class T>
    int getCacheProxy(const string &moduleName, const string &key, string &objectName, T &prxCache, const string &idcArea, const bool updateRouteTable = false);

    template <class T>
    T getProxy(const string &objectName);

  private:
    CommunicatorPtr _communicator;

    RouterTableInfoFactory *_routeTableFactory;

    // 保存已创建的servantproxy
    map<string, ServantProxy *> _servantProxy;
};

template <class T>
T CacheProxyFactory::getProxy(const string &objectName)
{
    string err;

    map<string, ServantProxy *>::iterator it = _servantProxy.find(objectName);

    try
    {
        if (it == _servantProxy.end())
        {
            pair<map<string, ServantProxy *>::iterator, bool> p =
                _servantProxy.insert(pair<string, ServantProxy *>(objectName, (_communicator->stringToProxy<T>(objectName)).get()));
            it = p.first;
        }
    }
    catch (TarsException &ex)
    {
        err = string(ex.what()) + " at " + objectName;
        throw TarsException(err.c_str());
    }
    catch (...)
    {
        err = string("UnkownException") + " at " + objectName;
        throw TarsException(err.c_str());
    }

    return (typename T::element_type *)(it->second);
}

template <class T>
int CacheProxyFactory::getWCacheProxy(const string &moduleName,
                                      const string &key,
                                      string &objectName,
                                      T &prxCache,
                                      const bool updateRouteTable)
{
    // 获取moduleName模块的路由信息,如果为空则返回模块错误
    RouterTableInfo *pRouteTableInfo = _routeTableFactory->getRouterTableInfo(moduleName);
    if (pRouteTableInfo == NULL)
    {
        TLOGWARN("CacheProxyFactory::getWCacheProxy do not support module : " << moduleName << endl);

        return ET_MODULE_NAME_INVALID;
    }

    int iRet = ET_SUCC;

    if (updateRouteTable)
    {
        iRet = pRouteTableInfo->update();
        if (iRet != ET_SUCC)
        {
            return iRet;
        }
    }

    //获取路由表
    const RouterTable &routeTable = pRouteTableInfo->getRouterTable();

    // 从路由表中获得要访问的CacheServer的节点信息
    ServerInfo serverInfo;
    iRet = routeTable.getMaster(key, serverInfo);

    if (iRet != RouterTable::RET_SUCC)
    {
        // 如果通过key无法获取CacheServer的节点信息，则返回错误
        TLOGERROR("CacheProxyFactory::getWCacheProxy can not locate key:" << key << endl);
        if (iRet == UnpackTable::RET_GROUP_READONLY)
        {
            return ET_READ_ONLY;
        }
        else
        {
            return ET_KEY_INVALID;
        }
    }

    // 由节点信息获取CacheServer代理
    objectName = serverInfo.WCacheServant;

    try
    {
        prxCache = getProxy<T>(objectName);
    }
    catch (TarsException &ex)
    {
        TLOGERROR("CacheProxyFactory::getWCacheProxy " << ex.what() << endl);

        return ET_SYS_ERR;
    }

    return ET_SUCC;
}

template <class T>
int CacheProxyFactory::getCacheProxy(const string &moduleName,
                                     const string &key,
                                     string &objectName,
                                     T &prxCache,
                                     const string &idcArea,
                                     const bool updateRouteTable)
{
    // 获取moduleName模块的路由信息,如果为空则返回模块错误
    RouterTableInfo *pRouteTableInfo = _routeTableFactory->getRouterTableInfo(moduleName);
    if (pRouteTableInfo == NULL)
    {
        TLOGWARN("CacheProxyFactory::getCacheProxy do not support module:" << moduleName << endl);

        return ET_MODULE_NAME_INVALID;
    }

    int iRet = ET_SUCC;
    if (updateRouteTable)
    {
        iRet = pRouteTableInfo->update();
        if (iRet != ET_SUCC)
        {
            return iRet;
        }
    }

    // 获取路由表
    const RouterTable &routeTable = pRouteTableInfo->getRouterTable();

    // 从路由表中获得要访问的CacheServer的节点信息
    ServerInfo serverInfo;
    if (idcArea == "")
    {
        iRet = routeTable.getMaster(key, serverInfo);
    }
    else
    {
        iRet = routeTable.getIdcServer(key, idcArea, false, serverInfo);
        // iRet = routeTable.getIdcServer(key, idcArea, serverInfo);
    }

    if (iRet != RouterTable::RET_SUCC)
    {
        // 如果通过key无法获取CacheServer的节点信息，则返回错误
        TLOGERROR("CacheProxyFactory::getCacheProxy can not locate key:" << key << endl);

        return ET_KEY_INVALID;
    }

    // 由节点信息获取CacheServer代理
    objectName = serverInfo.CacheServant;

    try
    {
        prxCache = getProxy<T>(objectName);
    }
    catch (TarsException &ex)
    {
        TLOGERROR("CacheProxyFactory::getCacheProxy " << ex.what() << endl);

        return ET_SYS_ERR;
    }

    return ET_SUCC;
}

#endif

