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
#ifndef _WCacheImp_H_
#define _WCacheImp_H_

#include "servant/Application.h"
#include "MKWCache.h"
#include "MKCacheServer.h"
#include "MKCacheComm.h"
#include "MKDbAccessCallback.h"

using namespace tars;
using namespace std;

/**
 *CacheObj的接口的实现
 *
 */
class MKWCacheImp : public DCache::MKWCache
{
public:
    /**
     *
     */
    virtual ~MKWCacheImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy();

    bool reloadConf(const string& command, const string& params, string& result);

    virtual tars::Int32 insertMKV(const DCache::InsertMKVReq &req, tars::TarsCurrentPtr current);

    virtual tars::Int32 insertMKVBatch(const DCache::InsertMKVBatchReq &req, DCache::MKVBatchWriteRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 updateMKV(const DCache::UpdateMKVReq &req, tars::TarsCurrentPtr current);

    virtual tars::Int32 updateMKVAtom(const DCache::UpdateMKVAtomReq &req, tars::TarsCurrentPtr current);

    virtual tars::Int32 updateMKVBatch(const DCache::UpdateMKVBatchReq &req, DCache::MKVBatchWriteRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 eraseMKV(const DCache::MainKeyReq &req, tars::TarsCurrentPtr current);

    virtual tars::Int32 delMKV(const DCache::DelMKVReq &req, tars::TarsCurrentPtr current);

    virtual tars::Int32 delMKVBatch(const DCache::DelMKVBatchReq &req, DCache::MKVBatchWriteRsp &rsp, tars::TarsCurrentPtr current);

    //List/Set/ZSet
    virtual tars::Int32 pushList(const DCache::PushListReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 popList(const DCache::PopListReq &req, DCache::PopListRsp &rsp, tars::TarsCurrentPtr current);
    virtual tars::Int32 replaceList(const DCache::ReplaceListReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 trimList(const DCache::TrimListReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 remList(const DCache::RemListReq &req, tars::TarsCurrentPtr current);

    virtual tars::Int32 addSet(const DCache::AddSetReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 delSet(const DCache::DelSetReq &req, tars::TarsCurrentPtr current);

    virtual tars::Int32 addZSet(const DCache::AddZSetReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 incScoreZSet(const DCache::IncZSetScoreReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 delZSet(const DCache::DelZSetReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 delZSetByScore(const DCache::DelZSetByScoreReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 updateZSet(const DCache::UpdateZSetReq &req, tars::TarsCurrentPtr current);

protected:
    //判断是否是迁移源地址
    bool isTransSrc(int pageNo);

    void asyncDbSelect(const string &mainKey, DbAccessPrxCallbackPtr cb);

    int procUpdateUK(tars::TarsCurrentPtr current, const string &mk, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, char ver, bool dirty, uint32_t expireTimeSecond, int &iUpdateCount, bool insert);
    int procUpdateMK(tars::TarsCurrentPtr current, const string &mk, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, char ver, bool dirty, uint32_t expireTimeSecond, int &iUpdateCount);

    int procUpdateUKAtom(tars::TarsCurrentPtr current, const string &mk, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, bool dirty, uint32_t expireTimeSecond, int &iUpdateCount);
    int procUpdateMKAtom(tars::TarsCurrentPtr current, const string &mk, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, bool dirty, uint32_t expireTimeSecond, int &iUpdateCount);

    int procDelUK(tars::TarsCurrentPtr current, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit);
    int procDelUKVer(tars::TarsCurrentPtr current, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, uint8_t iVersion, int &ret);
    int procDelMK(tars::TarsCurrentPtr current, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit);
    int procDelMKVer(tars::TarsCurrentPtr current, const string &mk, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, int &ret);

protected:
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

    //DB主索引长度有限制，为了落地不失败
    size_t _maxKeyLengthInDB;
    //某个主key下最大限数据量,=0时无限制，只有在大于0且不读db时有效
    size_t _mkeyMaxDataCount;
    //限制主key下最大限数据量功能开启时，是否删除脏数据
    bool _deleteDirty;

    int _hitIndex;

    bool _mkIsInteger;

    //查询主key数据，返回记录数限制
    size_t _mkeyMaxSelectCount;

    FieldConf _fieldConf;
};
/////////////////////////////////////////////////////
#endif
