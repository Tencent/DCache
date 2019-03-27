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
#ifndef _PROXYIMP_H_
#define _PROXYIMP_H_

#include "Proxy.h"
#include "Cache.h"
#include "WCache.h"
#include "MKCache.h"
#include "MKWCache.h"
#include "Router.h"
#include "StringUtil.h"
#include "RouterShare.h"
#include "util/tc_common.h"
#include "util/tc_config.h"
#include "util/tc_monitor.h"
#include "servant/Application.h"
#include "util/tc_timeprovider.h"
#include "ProxyServer.h"
#include "StatThread.h"

#include "CacheProxyFactory.h"

using namespace std;
using namespace tars;
using namespace DCache;

class ProxyImp : public Proxy
{
  public:
    ProxyImp();

    virtual ~ProxyImp(){};

    virtual void initialize();

    virtual void destroy(){};

    //-----------------------------DCache一期接口-----------------------------//
    virtual int getKV(const GetKVReq &req, GetKVRsp &rsp, TarsCurrentPtr current);

    virtual int getKVBatch(const GetKVBatchReq &req, GetKVBatchRsp &rsp, TarsCurrentPtr current);

    virtual int checkKey(const CheckKeyReq &req, CheckKeyRsp &rsp, TarsCurrentPtr current);

    virtual int getAllKeys(const GetAllKeysReq &req, GetAllKeysRsp &rsp, TarsCurrentPtr current);

    virtual int setKV(const SetKVReq &req, TarsCurrentPtr current);

    virtual int updateKV(const UpdateKVReq &req, UpdateKVRsp &rsp, TarsCurrentPtr current);

    virtual int insertKV(const SetKVReq &req, TarsCurrentPtr current);

    virtual int eraseKV(const RemoveKVReq &req, TarsCurrentPtr current);

    virtual int delKV(const RemoveKVReq &req, TarsCurrentPtr current);

    virtual int setKVBatch(const SetKVBatchReq &req, SetKVBatchRsp &rsp, TarsCurrentPtr current);

    virtual int delKVBatch(const RemoveKVBatchReq &req, RemoveKVBatchRsp &rsp, TarsCurrentPtr current);

    virtual int eraseKVBatch(const RemoveKVBatchReq &req, RemoveKVBatchRsp &rsp, TarsCurrentPtr current);

    //-----------------------------DCache二期接口-----------------------------//
    virtual int getMKV(const GetMKVReq &req, GetMKVRsp &rsp, TarsCurrentPtr current);

    virtual int getMKVBatch(const MKVBatchReq &req, MKVBatchRsp &rsp, TarsCurrentPtr current);

    virtual int getMUKBatch(const MUKBatchReq &req, MUKBatchRsp &rsp, TarsCurrentPtr current);

    virtual int getMKVBatchEx(const MKVBatchExReq &req, MKVBatchExRsp &rsp, TarsCurrentPtr current);

    virtual int getMainKeyCount(const MainKeyReq &req, TarsCurrentPtr current);

    virtual int getAllMainKey(const GetAllKeysReq &req, GetAllKeysRsp &rsp, TarsCurrentPtr current);

    virtual int getList(const GetListReq &req, GetListRsp &rsp, TarsCurrentPtr current);

    virtual int getRangeList(const GetRangeListReq &req, BatchEntry &rsp, TarsCurrentPtr current);

    virtual int getSet(const GetSetReq &req, BatchEntry &rsp, TarsCurrentPtr current);

    virtual int getZSetScore(const GetZsetScoreReq &req, double &score, TarsCurrentPtr current);

    virtual int getZSetPos(const GetZsetPosReq &req, long &pos, TarsCurrentPtr current);

    virtual int getZSetByPos(const GetZsetByPosReq &req, BatchEntry &rsp, TarsCurrentPtr current);

    virtual int getZSetByScore(const GetZsetByScoreReq &req, BatchEntry &rsp, TarsCurrentPtr current);

    virtual int insertMKV(const InsertMKVReq &req, TarsCurrentPtr current);

    virtual int updateMKV(const UpdateMKVReq &req, TarsCurrentPtr current);

    virtual int updateMKVAtom(const UpdateMKVAtomReq &req, TarsCurrentPtr current);

    virtual int eraseMKV(const MainKeyReq &req, TarsCurrentPtr current);

    virtual int delMKV(const DelMKVReq &req, TarsCurrentPtr current);

    virtual int insertMKVBatch(const InsertMKVBatchReq &req, MKVBatchWriteRsp &rsp, TarsCurrentPtr current);

    virtual int updateMKVBatch(const UpdateMKVBatchReq &req, MKVBatchWriteRsp &rsp, TarsCurrentPtr current);

    virtual int delMKVBatch(const DelMKVBatchReq &req, MKVBatchWriteRsp &rsp, TarsCurrentPtr current);

    virtual int pushList(const PushListReq &req, TarsCurrentPtr current);

    virtual int popList(const PopListReq &req, PopListRsp &rsp, TarsCurrentPtr current);

    virtual int replaceList(const ReplaceListReq &req, TarsCurrentPtr current);

    virtual int trimList(const TrimListReq &req, TarsCurrentPtr current);

    virtual int remList(const RemListReq &req, TarsCurrentPtr current);

    virtual int addSet(const AddSetReq &req, TarsCurrentPtr current);

    virtual int delSet(const DelSetReq &req, TarsCurrentPtr current);

    virtual int addZSet(const AddZSetReq &req, TarsCurrentPtr current);

    virtual int incScoreZSet(const IncZSetScoreReq &req, TarsCurrentPtr current);

    virtual int delZSet(const DelZSetReq &req, TarsCurrentPtr current);

    virtual int delZSetByScore(const DelZSetByScoreReq &req, TarsCurrentPtr current);

    virtual int updateZSet(const UpdateZSetReq &req, TarsCurrentPtr current);

  protected:
    // 批量处理时，需限制Key的数量，不允许超过配置最大值
    bool checkKeyCount(size_t keyCount);

    
    // 日志记录批量操作
    void logBatchCount(const string &moduleName, const string &callerName, size_t uCount);

  private:
    CacheProxyFactory *_cacheProxyFactory;

    set<string> _printLogModules;

    bool _printReadLog;

    bool _printWriteLog;

    string _idcArea;
};

#endif

