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
#ifndef _ROUTERHANDLE_H_
#define _ROUTERHANDLE_H_

#include <math.h>
#include <stdio.h>
#include "servant/Application.h"
#include "jmem_multi_hashmap_malloc/policy_multi_hashmap_malloc.h"
#include "MKCacheGlobe.h"
#include "Router.h"
#include "RouterClient.h"


using namespace std;
using namespace tars;
using namespace DCache;

const size_t TRANS_ADDSIZE_STEP = 1024;
const size_t PACKET_SIZE_LIMIT = 10 * 1024 * 1024;

class KeyMatch
{
public:
    void Set(unsigned int k)
    {
        hashKey = k;
    }
    bool operator()(const string &mk)
    {
        uint32_t hash = g_route_table.hashKey(mk);
        if (hash == hashKey)
            return true;
        return false;
    }
protected:
    unsigned int hashKey;
};

struct TransDataSizeLimit
{
    size_t iMaxSizeLimit;
    size_t iSlowWindow;
    time_t tLastUpdate;
    int	   iStepFactor;
};

class RouterHandle : public TC_Singleton<RouterHandle>
{
public:

    RouterHandle();

    virtual ~RouterHandle();

    /*初始化配置*/
    void init(TC_Config& conf);

    virtual tars::Int32 setRouterInfo(const string &moduleName, const PackTable & packTable, tars::TarsCurrentPtr current);

    virtual tars::Int32 setTransRouterInfo(const string& moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo>& transferingInfoList, const PackTable& packTable, tars::TarsCurrentPtr current);

    virtual tars::Int32 fromTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo>& transferingInfoList, const PackTable& packTable, tars::TarsCurrentPtr current);

    virtual tars::Int32 fromTransferDo(const TransferInfo& transferingInfo, tars::TarsCurrentPtr current);

    virtual tars::Int32 toTransferStart(const string& moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo>& transferingInfoList, const TransferInfo& transferingInfo, const PackTable& packTable, tars::TarsCurrentPtr current);

    virtual tars::Int32 getBinlogdif(tars::Int32 &difBinlogTime, tars::TarsCurrentPtr current);

    virtual tars::Int32 setRouterInfoForSwicth(const std::string & moduleName, const DCache::PackTable & packTable, tars::TarsCurrentPtr current);

    tars::Int32 initRoute(bool forbidFromFile, bool &syncFromRouterSuccess);

    void syncRoute();

    void doAfterTypeChange();

    void cleanDestTransLimit();

    /*
     * 获取变更后的新路由地址
     */
    int getUpdateServant(const string & mainkey, int index, bool bWrite, const string &idc, RspUpdateServant &updateServant);
    int getUpdateServant(const string & mainkey, bool bWrite, const string &idc, RspUpdateServant &updateServant)
    {
        return getUpdateServant(mainkey, 0, bWrite, idc, updateServant);
    }
    int getUpdateServant(const vector<string> & vtMainkey, const vector<int> &vIndex, bool bWrite, const string &idc, RspUpdateServant &updateServant);
    int getUpdateServant(const vector<string> & vtMainkey, bool bWrite, const string &idc, RspUpdateServant &updateServant);
    int updateServant2Str(const RspUpdateServant &updateServant, string &sRet);

    /* 主机服务重启上报 */
    void serviceRestartReport();

    void heartBeat();

    //主备连接心跳计数
    void incSlaveHbCount()
    {
        ++_slaveHbCount;
    }
    int getSlaveHbCount()
    {
        return _slaveHbCount;
    }
    string getConnectHbAddr();
    int masterDowngrade();

protected:

    /*
    *Get迁移目的地址
    */
    string getTransDest(int pageNo);

    /*
    *Get Server状态，MASTER or SLAVE
    */
    enum ServerType getType();

    /*
    *查看要执行的迁移任务是否在最近更新的迁移列表里
    */
    bool isInTransferingInfoList(const TransferInfo& transferingInfo);

    /*
    *根据处于迁移状态的信息列表更新CanSync的KeyLimit范围
    */
    void updateCSyncKeyLimit();

    /*
    *Get 迁移目的服务器时的数据大小限制
    */
    TransDataSizeLimit getDestTransLimit(const string& dest);

    /*
    *Update 迁移目的服务器时的数据大小限制
    */
    void updateDestTransLimit(const string& dest, TransDataSizeLimit& transLimit, bool transOk = true);

protected:

    string _moduleName; //业务名称

    string _routeFile;

    TC_ThreadLock _lock; // 线程锁

    int _transInfoListVer; // 模块当前处于迁移状态信息列表的版本号

    vector<TransferInfo> _transferingInfoList; // 模快处于迁移状态的信息列表

    string _routeObj;
    RouterPrx _routePrx;

    //路由分页大小
    unsigned int _pageSize;

    //迁移目的服务器数据大小限制
    map<string, TransDataSizeLimit> _destTransLimit;

    //传输数据是否压缩
    bool _transferCompress;

	std::atomic<int>	_slaveHbCount;
};

#endif
