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
#ifndef _ASSISTANCE_HEAD_H_
#define _ASSISTANCE_HEAD_H_
#include <string>
#include <stdlib.h>

#include <util/tc_mysql.h>

#include "Node.h"
#include "AdminReg.h"

using namespace std;
using namespace tars;

#define FUN_LOG __FUNCTION__ << ":" << __LINE__ << "|"

class Assistance
{
public:
    int hasOutOfBounds(string page);
    void getIPAndPort(const string& endPoint, string& ip, string& port);
public:
    static const long C_PAGE_BEGIN;     //页起始编号
    static const long C_PAGE_END;       //页终止编号

    static const int C_SUCCESS;         //成功
};

class Tool
{
public:

    static int getNodeObj(TC_Mysql &mysqlTarsDb, const string & sIp, string & sNodeObj);

    static void parseServerName(const string& sFullServerName, string &application, string &servername);

    static bool isInactive(TC_Mysql &mysqlTarsDb, const string &application, const string &servername, const string &host, string &sError);

    static void delRouteInfo4CacheServer(TC_Mysql &mysqlRouterDb, const string &CacheServer);

    static void delConfigInfo4CacheServer(TC_Mysql &mysqlRelationDb, const string & sFullCacheServer);

    static void delReferConfig4CacheServer(TC_Mysql &mysqlRelationDb, const string & sFullCacheServer);

    static void delRelation4CacheServer(TC_Mysql &mysqlRelationDb, const string & sFullCacheServer);

    //删除proxy的配置信息
    static int cleanProxyConf(TC_Mysql &mysqlRelationDb, const string &proxyName);

    //删除router的配置信息
    static int cleanRouterConf(TC_Mysql &mysqlRelationDb, const string &routerName);

    //删除dbaccess配置信息
    static int cleanDBaccessConf(TC_Mysql &mysqlRelationDb, const string &dbaccessName);

    static void UninstallCacheServer(TC_Mysql &mysqlRouterDb, TC_Mysql &mysqlTarsDb, TC_Mysql &mysqlRelationDb, const string &sFullCacheServer, const string &sCacheBakPath, bool checkNode=true);

    static int UninstallTarsServer(TC_Mysql &mysqlTarsDb, const string &sTarsServerName, const  string &sIp, std::string &sError);

};

#endif



