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

#include "CacheGlobe.h"
#include "Router.h"
#include "RouterClient.h"

using namespace std;
using namespace tars;
using namespace DCache;

class KeyMatch
{
public:
    void Set(unsigned int k) {
        hashKey = k;
    }
    bool operator()(const string &k)
    {
        if (hashCounter.HashRawString(k) == hashKey)
            return true;
        return false;
    }
protected:
    NormalHash hashCounter;
    unsigned int hashKey;
};

struct TransParam : public TC_HandleBase
{
    TransParam() : _end(false), _endPage(false)
    {
    }

    TransParam(int iCount) : _end(false), _endPage(false)
    {
        _count = iCount;
    }

    bool setEnd()
    {
        bool bResult = true;
        _mutex.lock();
        if (_end)
        {
            bResult = false;
        }
        else
        {
            _end = true;
        }
        _mutex.unlock();
        return bResult;
    }

    bool setFinish()
    {
        bool bResult = true;
        _mutex.lock();
        if (!_end)
        {
            if (_count == 0)
            {
                bResult = false;
                _end = true;
            }
            else
            {
                _endPage = true;
            }
        }
        _mutex.unlock();
        return bResult;
    }

    bool isFinish()
    {
        bool bResult = true;
        _mutex.lock();
        if ((_count--)>0 || _end)
        {
            bResult = false;
        }
        else
        {
            if (!_endPage)
                bResult = false;
            else
                _end = true;
        }
        _mutex.unlock();
        return bResult;
    }

    volatile bool  _end;
    volatile bool  _endPage;
    std::atomic<int> 	   _count;
    TC_ThreadMutex _mutex;
};

typedef tars::TC_AutoPtr<TransParam> TransParamPtr;

struct RouterClientCallback : public DCache::RouterClientPrxCallback
{
    RouterClientCallback(TarsCurrentPtr &current, TransParamPtr& pParam) :_current(current), _pParam(pParam)
    {
    }

    virtual void callback_setBatchFroTrans(tars::Int32 ret);

    virtual void callback_setBatchFroTrans_exception(tars::Int32 ret);

    virtual void callback_setBatchFroTransOnlyKey(tars::Int32 ret);

    virtual void callback_setBatchFroTransOnlyKey_exception(tars::Int32 ret);

    TarsCurrentPtr _current;
    TransParamPtr _pParam;
};

class RouterHandle : public TC_Singleton<RouterHandle>
{
public:

    RouterHandle();

    virtual ~RouterHandle();

    /*初始化配置*/
    void init(TC_Config& conf);

    tars::Int32 initRoute(bool forbidFromFile, bool &syncFromRouterSuccess);

    /* 主机服务重启上报 */
    void serviceRestartReport();

    void syncRoute();

    tars::Int32 setRouterInfo(const string &moduleName, const PackTable & packTable);

    tars::Int32 setTransRouterInfo(const string& moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo>& transferingInfoList, const PackTable& packTable);

    tars::Int32 fromTransferStart(const std::string& moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo>& transferingInfoList, const PackTable& packTable);

    tars::Int32 fromTransferDo(const TransferInfo& transferingInfo, tars::TarsCurrentPtr current);

    tars::Int32 toTransferStart(const string& moduleName, tars::Int32 transInfoListVer, const vector<TransferInfo>& transferingInfoList, const TransferInfo& transferingInfo, const PackTable& packTable);

    tars::Int32 setRouterInfoForSwicth(const std::string & moduleName, const DCache::PackTable & packTable);

    tars::Int32 getBinlogdif(tars::Int32 &difBinlogTime);

    void heartBeat();

    /*
     * 获取变更后的新路由地址
     */
    int getUpdateServant(const string & mainkey, int index, bool bWrite, const string &idc, RspUpdateServant &updateServant);
    int getUpdateServant(const string & mainkey, bool bWrite, const string &idc, RspUpdateServant &updateServant);
    int getUpdateServant(const vector<string> & vtMainkey, const vector<int> &vIndex, bool bWrite, const string &idc, RspUpdateServant &updateServant);
    int getUpdateServant(const vector<string> & vtMainkey, bool bWrite, const string &idc, RspUpdateServant &updateServant);
    int getUpdateServantKey(const vector<string> & vtMainkey, bool bWrite, const string &idc, RspUpdateServant &updateServant);
    int updateServant2Str(const RspUpdateServant &updateServant, string &sRet);

    //主备连接心跳计数
    void incSlaveHbCount() {
        ++_slaveHbCount;
    }
    int getSlaveHbCount() {
        return _slaveHbCount;
    }
    string getConnectHbAddr();
    int masterDowngrade();
private:

    /*
    *根据处于迁移状态的信息列表更新CanSync的KeyLimit范围
    */
    void updateCSyncKeyLimit();

    /*
    *Get迁移目的地址
    */
    string getTransDest(int pageNo);

    /*
    *Get Server状态，MASTER or SLAVE
    */
    static enum ServerType getType();

    /*
    *查看要执行的迁移任务是否在最近更新的迁移列表里
    */
    bool isInTransferingInfoList(const TransferInfo& transferingInfo);
private:

    string _moduleName; //业务名称

    string _routeFile;
    string _routeObj;
    RouterPrx _routePrx;
    //路由分页大小
    unsigned int _pageSize;

    TC_ThreadLock _lock; // 线程锁

    int _transInfoListVer; // 模块当前处于迁移状态信息列表的版本号

    vector<TransferInfo> _transferingInfoList; // 模快处于迁移状态的信息列表

    std::atomic<int> _slaveHbCount;

};

#endif
