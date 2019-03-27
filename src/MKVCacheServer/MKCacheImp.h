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
#ifndef _CacheImp_H_
#define _CacheImp_H_

#include "servant/Application.h"
#include "MKCache.h"
#include "MKCacheServer.h"
#include "MKCacheComm.h"
#include "MKDbAccessCallback.h"

using namespace tars;
using namespace std;

/**
 *CacheObj的接口的实现
 *
 */
class MKCacheImp : public DCache::MKCache
{
public:
    /**
     *
     */
    virtual ~MKCacheImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy();

    bool reloadConf(const string& command, const string& params, string& result);

    virtual tars::Int32 getMKV(const DCache::GetMKVReq &req, DCache::GetMKVRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getMKVBatch(const DCache::MKVBatchReq &req, DCache::MKVBatchRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getMUKBatch(const DCache::MUKBatchReq &req, DCache::MUKBatchRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getMKVBatchEx(const DCache::MKVBatchExReq &req, DCache::MKVBatchExRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getSyncTime(tars::TarsCurrentPtr current);

    virtual tars::Int32 getDeleteTime(tars::TarsCurrentPtr current);

    virtual tars::Int32 getMainKeyCount(const DCache::MainKeyReq &req, tars::TarsCurrentPtr current);

    virtual tars::Int32 getAllMainKey(const DCache::GetAllKeysReq &req, DCache::GetAllKeysRsp &rsp, tars::TarsCurrentPtr current);

    //List/Set/ZSet
    virtual tars::Int32 getList(const DCache::GetListReq &req, DCache::GetListRsp &rsp, tars::TarsCurrentPtr current);
    virtual tars::Int32 getRangeList(const DCache::GetRangeListReq &req, DCache::BatchEntry &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getSet(const DCache::GetSetReq &req, DCache::BatchEntry &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getZSetScore(const DCache::GetZsetScoreReq &req, tars::Double &score, tars::TarsCurrentPtr current);
    virtual tars::Int32 getZSetPos(const DCache::GetZsetPosReq &req, tars::Int64 &pos, tars::TarsCurrentPtr current);
    virtual tars::Int32 getZSetByPos(const DCache::GetZsetByPosReq &req, DCache::BatchEntry &rsp, tars::TarsCurrentPtr current);
    virtual tars::Int32 getZSetByScore(const DCache::GetZsetByScoreReq &req, DCache::BatchEntry &rsp, tars::TarsCurrentPtr current);

    //FIXME, bug in MKDbAccessCallback
    virtual tars::Int32 getZSetBatch(const DCache::GetZSetBatchReq &req, DCache::GetZSetBatchRsp &rsp, tars::TarsCurrentPtr current);

protected:
    //判断是否是迁移源地址
    bool isTransSrc(int pageNo);
    //获取迁移目标地址串
    string getTransDest(int pageNo);

    void asyncDbSelect(const string &mainKey, DbAccessPrxCallbackPtr cb);

    int procSelectUK(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord);
    int procSelectMK(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord);

    int selectUKCache(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const string &uk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord);
    int selectMKCache(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord);
    int selectMKCache(const vector<string> &vtField, const string &mk, const vector<vector<DCache::Condition> > & vtCond, const Limit &stLimit, tars::Bool bGetMKCout, vector<map<std::string, std::string> > &vtData, int &iMKRecord);

    int asyncSelect(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, bool bUKey, tars::Bool bGetMKCout);

    int asyncGetSet(tars::TarsCurrentPtr current, const vector<string> &vtField, const string &mk);

    int asyncGetZSet(tars::TarsCurrentPtr current, const string &mk, const string &value);
    int asyncGetZSet(tars::TarsCurrentPtr current, const string &mk, const string &value, const bool bOrder);
    int asyncGetZSet(tars::TarsCurrentPtr current, const string &mk, const vector<string> &field, long iStart, long iEnd, bool bUp);
    int asyncGetZSetByScore(tars::TarsCurrentPtr current, const string &mk, const vector<string> &field, double iMin, double iMax);

protected:
    //业务名称
    string _moduleName;
    string _config;
    TC_Config _tcConf;
    string _binlogFile;
    string _keyBinlogFile;
    bool _recordBinLog;
    bool _recordKeyBinLog;
    DbAccessPrx _dbaccessPrx;

    bool _saveOnlyKey;
    bool _readDB;
    bool _existDB;
    bool _insertAtHead;
    bool _updateInOrder;

    string _orderItem;//排序的字段名
    bool _orderByDesc;//是否以递减的顺序排

    int _hitIndex;

    TC_Multi_HashMap_Malloc::MainKey::KEYTYPE _storeKeyType;

    //查询主key数据，返回记录数限制
    size_t _mkeyMaxSelectCount;

    FieldConf _fieldConf;
};
/////////////////////////////////////////////////////
#endif
