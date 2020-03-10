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
#ifndef _DbAccessCallback_H_
#define _DbAccessCallback_H_

#include "CacheServer.h"
#include "WCache.h"

using namespace tars;
using namespace std;

class DbAccessCBParam : public TC_HandleBase
{
public:
    enum
    {
        INIT = 0,
        FINISH = 1
    };

    struct UpdateCBParam
    {
        TarsCurrentPtr current;
        string key;
        string value;
        Op option;
        bool dirty;
        uint32_t expireTime;
    };

public:
    DbAccessCBParam(const string &sMainKey) :_mainKey(sMainKey)
    {
        _createTime = TC_TimeProvider::getInstance()->getNow();
    }
    ~DbAccessCBParam() {}

    time_t getCreateTime() {
        return _createTime;
    }

    void setStatus(char status) {
        _status = status;
    }
    char getStatus() {
        return _status;
    }

    TC_ThreadMutex& getMutex() {
        return _mutex;
    }
    string& getMainKey() {
        return _mainKey;
    }

    vector<UpdateCBParam>& getUpdate() {
        return _updateParams;
    }

    int AddUpdate(UpdateCBParam param);

private:

    vector<UpdateCBParam> _updateParams;

    char _status;
    TC_ThreadMutex _mutex;
    string _mainKey;
    time_t _createTime;
};
typedef tars::TC_AutoPtr<DbAccessCBParam> DbAccessCBParamPtr;

class CBQueue
{
public:
    CBQueue()
    {
        _timeout = 60;
    }
    ~CBQueue()
    {
        _keyParamQueue.clear();
    }
    DbAccessCBParamPtr getCBParamPtr(const string &mainKey, bool &isCreate);
    void erase(const string &mainKey);
private:
    map<string, DbAccessCBParamPtr> _keyParamQueue;
    TC_ThreadMutex _mutex;
    int _timeout;
};

struct BatchParam : public TC_HandleBase
{
    BatchParam() :bEnd(false) {}
    BatchParam(int iCount, const vector<SKeyValue> vtValue) :bEnd(false)
    {
        count = iCount;
        this->vtValue = vtValue;
    }

    void addValue(string sKey, string sValue, int iRet, uint8_t iVersion, uint32_t iExpireTime)
    {
        mutex.lock();
        SKeyValue sKeyValue;
        sKeyValue.keyItem = sKey;
        sKeyValue.value = sValue;
        sKeyValue.ret = iRet;
        sKeyValue.ver = iVersion;
        sKeyValue.expireTime = iExpireTime;
        vtValue.push_back(sKeyValue);
        mutex.unlock();
    }

    std::atomic<int> count;
    vector<SKeyValue> vtValue;
    TC_ThreadMutex mutex;
    bool bEnd;
};

typedef tars::TC_AutoPtr<BatchParam> BatchParamPtr;

struct DelBatchParam : public TC_HandleBase
{
    DelBatchParam() :bEnd(false) {}
    DelBatchParam(int iCount, const vector<string> &vtKey, DCache::RemoveKVBatchRsp &rsp) :bEnd(false)
    {
        count = iCount;
        this->vtKey = vtKey;
        result = rsp;
    }

    void addValue(string sKey, int ret)
    {
        mutex.lock();
        result.keyResult[sKey] = ret;
        mutex.unlock();
    }

    DCache::RemoveKVBatchRsp result;

    std::atomic<int> count;
    vector<string> vtKey;
    TC_ThreadMutex mutex;
    bool bEnd;
};

typedef tars::TC_AutoPtr<DelBatchParam> DelBatchParamPtr;

/*
*DbAccess异步查询Callback类
*/
struct DbAccessCallback : public DbAccessPrxCallback
{
    //定义构造函数，保存上下文
    DbAccessCallback(TarsCurrentPtr &current, const string &sKey, const string &sBinLogFile, bool bSaveOnlyKey, bool bBatch, bool bRecordBinLog, bool bRecordKeyBinlog, string type, DbAccessCBParamPtr pParam) :
        _current(current), _key(sKey), _binlogFile(sBinLogFile), _saveOnlyKey(bSaveOnlyKey), _batchReq(bBatch), _isRecordBinLog(bRecordBinLog), _isRecordKeyBinLog(bRecordKeyBinlog), _cbParam(pParam), _type(type) {}

    DbAccessCallback(TarsCurrentPtr &current, const string &sKey, const string &sBinLogFile, bool bSaveOnlyKey, bool bBatch, bool bRecordBinLog, bool bRecordKeyBinlog, BatchParamPtr pParam) :
        _current(current), _key(sKey), _binlogFile(sBinLogFile), _saveOnlyKey(bSaveOnlyKey), _batchReq(bBatch), _isRecordBinLog(bRecordBinLog), _isRecordKeyBinLog(bRecordKeyBinlog), _pParam(pParam), _type("") {}

    DbAccessCallback(TarsCurrentPtr &current, const string &sKey, const string &sBinLogFile, bool bSaveOnlyKey, bool bBatch, bool bRecordBinLog, bool bRecordKeyBinlog, BatchParamPtr pParam, string type, const string &sValue, bool dirty, tars::Int32 expireTimeSecond) :
        _current(current), _key(sKey), _value(sValue), _binlogFile(sBinLogFile), _saveOnlyKey(bSaveOnlyKey), _batchReq(bBatch), _isRecordBinLog(bRecordBinLog), _isRecordKeyBinLog(bRecordKeyBinlog), _pParam(pParam), _type(type), _dirty(dirty), _expireTimeSecond(expireTimeSecond) {}

    DbAccessCallback(TarsCurrentPtr &current, const string & sKey, const string &sBinLogFile, bool bSaveOnlyKey, bool bBatch, bool bRecordBinLog, bool bRecordKeyBinlog, BatchParamPtr pParam, string type, const string &sValue, bool dirty, tars::Int32 expireTimeSecond, enum Op option) :
        _current(current), _key(sKey), _value(sValue), _binlogFile(sBinLogFile), _saveOnlyKey(bSaveOnlyKey), _batchReq(bBatch), _isRecordBinLog(bRecordBinLog), _isRecordKeyBinLog(bRecordKeyBinlog), _pParam(pParam), _type(type), _dirty(dirty), _expireTimeSecond(expireTimeSecond), _option(option) {}

    DbAccessCallback(TarsCurrentPtr &current, const string &sKey, const string &sBinLogFile, bool bRecordBinLog, bool bRecordKeyBinlog) :
        _current(current), _key(sKey), _binlogFile(sBinLogFile), _batchReq(false), _isRecordBinLog(bRecordBinLog), _isRecordKeyBinLog(bRecordKeyBinlog) {}

    DbAccessCallback(TarsCurrentPtr &current, const string &sKey, const string &sBinLogFile, bool bBatch, bool bRecordBinLog, bool bRecordKeyBinlog, DelBatchParamPtr pParam) :
        _current(current), _key(sKey), _binlogFile(sBinLogFile), _batchReq(bBatch), _isRecordBinLog(bRecordBinLog), _isRecordKeyBinLog(bRecordKeyBinlog), _pDelParam(pParam) {}


    //回调函数,Key类型为string，返回带过期时间@2016.3.18
    virtual void callback_get(tars::Int32 ret, const std::string &value, tars::Int32 expireTime);

    //异常回调函数,Key类型为string, @2016.3.18
    virtual void callback_get_exception(tars::Int32 ret);

    virtual void callback_del(tars::Int32 ret);

    virtual void callback_del_exception(tars::Int32 ret);

    void response_del();

    TarsCurrentPtr _current;
    string _key;
    string _value;
    string _binlogFile;
    bool _saveOnlyKey;
    bool _batchReq;
    bool _isRecordBinLog;
    bool _isRecordKeyBinLog;
    BatchParamPtr _pParam;
    DelBatchParamPtr _pDelParam;
    DbAccessCBParamPtr _cbParam;
    string _type;
    bool _dirty;
    tars::Int32 _expireTimeSecond;
    enum Op _option;
};

/////////////////////////////////////////////////////
#endif

