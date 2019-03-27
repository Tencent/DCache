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
#ifndef _MKDBACCESSCALLBACK_H
#define _MKDBACCESSCALLBACK_H

#include "MKCacheServer.h"
#include "MKCacheComm.h"
#include "MKWCache.h"
struct SelectBatchParam : public TC_HandleBase
{
    SelectBatchParam() :bEnd(false) {}
    SelectBatchParam(int iCount, const vector<MainKeyValue>& vtKeyValue) :bEnd(false)
    {
        count.set(iCount);
        this->vtKeyValue.reserve(iCount);
        this->vtKeyValue = vtKeyValue;
    }

    SelectBatchParam(int iCount, const vector<DCache::Record>& vtData) :bEnd(false)
    {
        count.set(iCount);
        this->vtData = vtData;
    }

    void addValue(string sKey, const vector<map<string, string> > vtValue, int iRet)
    {
        mutex.lock();
        MainKeyValue sKeyValue;
        sKeyValue.mainKey = sKey;
        sKeyValue.value = vtValue;
        sKeyValue.ret = iRet;
        vtKeyValue.push_back(sKeyValue);
        mutex.unlock();
    }

    void addValue(const MainKeyValue& keyValue)
    {
        mutex.lock();
        vtKeyValue.push_back(keyValue);
        mutex.unlock();
    }

    void addValue(const DCache::Record &record)
    {
        mutex.lock();
        vtData.push_back(record);
        mutex.unlock();
    }

    bool setEnd()
    {
        bool bResult = true;
        mutex.lock();
        if (bEnd)
            bResult = false;
        else
            bEnd = true;
        mutex.unlock();
        return bResult;
    }

    TC_Atomic count;
    vector<MainKeyValue> vtKeyValue;
    TC_ThreadMutex mutex;

    vector<DCache::Record> vtData;  //针对selectBatchEx

    bool bEnd;
};

typedef TC_AutoPtr<SelectBatchParam> SelectBatchParamPtr;

struct InsertBatchParam : public TC_HandleBase
{
    InsertBatchParam() :bEnd(false) {}
    InsertBatchParam(int iCount, const map<tars::Int32, tars::Int32>& mpFailIndexReason) :bEnd(false)
    {
        count.set(iCount);
        this->rsp.rspData = mpFailIndexReason;
    }

    void addFailIndexReason(int iIndex, int iFailReason)
    {
        mutex.lock();
        rsp.rspData[iIndex] = iFailReason;
        mutex.unlock();
    }

    void addUpdateServant(int iIndex, string &sServant)
    {
        mutex.lock();
        updateServant.mpServant[sServant].push_back(iIndex);
        mutex.unlock();
    }

    bool setEnd()
    {
        bool bResult = true;
        mutex.lock();
        if (bEnd)
            bResult = false;
        else
            bEnd = true;
        mutex.unlock();
        return bResult;
    }

    TC_Atomic count;
    MKVBatchWriteRsp rsp;
    TC_ThreadMutex mutex;
    bool bEnd;
    RspUpdateServant updateServant;
};

typedef TC_AutoPtr<InsertBatchParam> InsertBatchParamPtr;

struct UpdateBatchParam : public TC_HandleBase
{
    UpdateBatchParam() :bEnd(false) {}
    UpdateBatchParam(int iCount, const map<tars::Int32, tars::Int32>& mpFailIndexReason) :bEnd(false)
    {
        count.set(iCount);
        this->rsp.rspData = mpFailIndexReason;
    }

    void addFailIndexReason(int iIndex, int iFailReason)
    {
        mutex.lock();
        rsp.rspData[iIndex] = iFailReason;
        mutex.unlock();
    }

    void addUpdateServant(int iIndex, string &sServant)
    {
        mutex.lock();
        updateServant.mpServant[sServant].push_back(iIndex);
        mutex.unlock();
    }

    bool setEnd()
    {
        bool bResult = true;
        mutex.lock();
        if (bEnd)
            bResult = false;
        else
            bEnd = true;
        mutex.unlock();
        return bResult;
    }

    TC_Atomic count;
    MKVBatchWriteRsp rsp;
    TC_ThreadMutex mutex;
    bool bEnd;
    RspUpdateServant updateServant;
};

typedef TC_AutoPtr<UpdateBatchParam> UpdateBatchParamPtr;

struct DelBatchParam : public TC_HandleBase
{
    DelBatchParam() :bEnd(false) {}
    DelBatchParam(TarsCurrentPtr current, int iCount, const map<tars::Int32, tars::Int32>& mRet, const map<string, int> &mIndex) :bEnd(false)
    {
        this->current = current;
        count.set(iCount);
        this->rsp.rspData = mRet;
        this->mIndex = mIndex;

        bFail = false;
    }

    void addFailIndexReason(int iIndex, int iFailReason)
    {
        mutex.lock();
        if (iFailReason < 0)
            bFail = true;
        rsp.rspData[iIndex] = iFailReason;

        if (count.dec() <= 0)
        {
            if (!bFail)
                MKWCache::async_response_delMKVBatch(current, ET_SUCC, rsp);
            else
            {
                if (updateServant.mpServant.size() > 0)
                {
                    map<string, string> rspContext;
                    rspContext[ROUTER_UPDATED] = "";
                    RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                    current->setResponseContext(rspContext);
                }
                MKWCache::async_response_delMKVBatch(current, ET_PARTIAL_FAIL, rsp);
            }
        }
        mutex.unlock();
    }

    void addUpdateServant(int iIndex, string &sServant)
    {
        mutex.lock();
        updateServant.mpServant[sServant].push_back(iIndex);
        mutex.unlock();
    }

    bool setEnd()
    {
        bool bResult = true;
        mutex.lock();
        if (bEnd)
            bResult = false;
        else
            bEnd = true;
        mutex.unlock();
        return bResult;
    }

    bool bFail;

    TarsCurrentPtr current;
    TC_Atomic count;
    MKVBatchWriteRsp rsp;
    map<string, int> mIndex;
    TC_ThreadMutex mutex;
    bool bEnd;
    RspUpdateServant updateServant;
};

typedef TC_AutoPtr<DelBatchParam> DelBatchParamPtr;

class MKDbAccessCBParam : public TC_HandleBase
{
public:
    enum
    {
        INIT = 0,
        FINISH = 1
    };
    struct SelectCBParam
    {
        TarsCurrentPtr current;
        string mk;
        vector<string> vtField;
        bool bUKey;
        vector<DCache::Condition> vtUKCond;
        vector<vector<Condition> > vvUKConds; //针对selectBatchEx时同一mainkey下多个全ukey字段查询
        vector<DCache::Condition> vtValueCond;
        Limit stLimit;
        bool bGetMKCout;
        bool bBatch;
        bool bMUKBatch;
        bool bBatchOr;
        SelectBatchParamPtr pParam;
    };

    struct GetCountCBParam
    {
        TarsCurrentPtr current;
        string mk;
    };

    struct InsertCBParam
    {
        TarsCurrentPtr current;
        string mk;
        string uk;
        string value;
        string logValue;
        char ver;
        bool dirty;
        bool replace;
        uint32_t expireTime;
        bool bBatch;
        tars::Int32 iIndex;
        InsertBatchParamPtr pParam;
    };

    struct UpdateCBParam
    {
        UpdateCBParam() :atom(false) {}

        TarsCurrentPtr current;
        string mk;
        map<string, DCache::UpdateValue> mpValue;
        bool bUKey;
        vector<DCache::Condition> vtUKCond;
        vector<DCache::Condition> vtValueCond;
        char ver;
        bool dirty;
        Limit stLimit;
        uint32_t expireTime;
        bool insert;
        bool atom;
    };

    struct UpdateCBParamForBatch
    {
        TarsCurrentPtr current;
        string mk;
        string uk;
        map<string, DCache::UpdateValue> mpInsertValue;
        map<string, DCache::UpdateValue> mpUpdateValue;
        string logValue;
        char ver;
        bool dirty;
        bool insert;
        uint32_t expireTime;
        tars::Int32 iIndex;
        UpdateBatchParamPtr pParam;
    };

    struct DelCBParam
    {
        TarsCurrentPtr current;
        string mk;
        bool bUKey;
        vector<DCache::Condition> vtUKCond;
        vector<DCache::Condition> vtValueCond;
        Limit stLimit;
        DbAccessPrx pMKDBaccess;
    };

    struct DelCBParamForBatch
    {
        string mk;
        bool bUKey;
        vector<DCache::Condition> vtUKCond;
        vector<DCache::Condition> vtValueCond;
        Limit stLimit;
        DbAccessPrx pMKDBaccess;
        char ver;

        DelBatchParamPtr pParam;
    };

    struct GetSetCBParam
    {
        TarsCurrentPtr current;
        string mk;
        vector<string> vtField;
    };

    struct DelSetCBParam
    {
        TarsCurrentPtr current;
        string mk;
        string value;
    };

    struct GetScoreZSetCBParam
    {
        TarsCurrentPtr current;
        string mk;
        string value;
    };

    struct GetRankZSetCBParam
    {
        TarsCurrentPtr current;
        string mk;
        string value;
        bool bOrder;
    };

    struct GetRangeZSetCBParam
    {
        TarsCurrentPtr current;
        string mk;
        vector<string> vtField;
        long iStart;
        long iEnd;
        bool bUp;
    };

    struct GetRangeZSetByScoreCBParam
    {
        TarsCurrentPtr current;
        string mk;
        vector<string> vtField;
        double iMin;
        double iMax;
    };

    struct IncScoreZSetCBParam
    {
        TarsCurrentPtr current;
        string mk;
        string value;
        double score;
        int iExpireTime;
        char iVersion;
        bool bDirty;
    };

    struct DelZSetCBParam
    {
        TarsCurrentPtr current;
        string mk;
        string value;
    };

    struct DelRangeZSetCBParam
    {
        TarsCurrentPtr current;
        string mk;
        double iMin;
        double iMax;
    };

    struct UpdateZSetCBParam
    {
        TarsCurrentPtr current;
        string mk;
        map<string, DCache::UpdateValue> mpValue;
        vector<DCache::Condition> vtValueCond;
        uint32_t iExpireTime;
        char iVersion;
        bool bDirty;
        bool bOnlyScore;
    };

public:
    MKDbAccessCBParam(const string &sMainKey) :m_sMainKey(sMainKey)
    {
        m_tCreat = TC_TimeProvider::getInstance()->getNow();
    }
    ~MKDbAccessCBParam() {}

    time_t getCreateTime()
    {
        return m_tCreat;
    }

    void setStatus(char status)
    {
        m_cStatus = status;
    }
    char getStatus()
    {
        return m_cStatus;
    }

    TC_ThreadMutex& getMutex()
    {
        return m_mutex;
    }
    string& getMainKey()
    {
        return m_sMainKey;
    }

    vector<SelectCBParam>& getSelect()
    {
        return m_vtSelect;
    }
    vector<InsertCBParam>& getInsert()
    {
        return m_vtInsert;
    }
    vector<UpdateCBParam>& getUpdate()
    {
        return m_vtUpdate;
    }
    vector<DelCBParam>& getDel()
    {
        return m_vtDel;
    }
    vector<DelCBParamForBatch>& getDelBatch()
    {
        return m_vtDelForBatch;
    }
    vector<UpdateCBParamForBatch>& getUpdateBatch()
    {
        return m_vtUpdateForBatch;
    }
    vector<GetCountCBParam>& getGetCount()
    {
        return m_vtGetCount;
    }
    vector<GetSetCBParam>& getGetSet()
    {
        return m_vtGetSet;
    }
    vector<DelSetCBParam>& getDelSet()
    {
        return m_vtDelSet;
    }
    vector<GetScoreZSetCBParam>& getGetScoreZSet()
    {
        return m_vtGetScoreZSet;
    }
    vector<GetRankZSetCBParam>& getGetRankZSet()
    {
        return m_vtGetRankZSet;
    }
    vector<GetRangeZSetCBParam>& getGetRangeZSet()
    {
        return m_vtGetRangeZSet;
    }
    vector<GetRangeZSetByScoreCBParam>& getGetRangeZSetByScore()
    {
        return m_vtGetRangeZSetByScore;
    }
    vector<IncScoreZSetCBParam>& getIncScoreZSet()
    {
        return m_vtIncScoreZSet;
    }
    vector<DelZSetCBParam>& getDelZSet()
    {
        return m_vtDelZSet;
    }
    vector<DelRangeZSetCBParam>& getDelRangeZSet()
    {
        return m_vtDelRangeZSet;
    }
    vector<UpdateZSetCBParam>& getUpdateZSet()
    {
        return m_vtUpdateZSet;
    }

    int AddSelect(SelectCBParam param);
    int AddInsert(InsertCBParam param);
    int AddUpdate(UpdateCBParam param);
    int AddDel(DelCBParam param);
    int AddDelForBatch(DelCBParamForBatch param);
    int AddUpdateForBatch(UpdateCBParamForBatch param);
    int AddGetCount(GetCountCBParam param);
    int AddGetSet(GetSetCBParam param);
    int AddDelSet(DelSetCBParam param);
    int AddGetScoreZSet(GetScoreZSetCBParam param);
    int AddGetRankZSet(GetRankZSetCBParam param);
    int AddGetRangeZSet(GetRangeZSetCBParam param);
    int AddGetRangeZSetByScore(GetRangeZSetByScoreCBParam param);
    int AddIncScoreZSet(IncScoreZSetCBParam param);
    int AddDelZSet(DelZSetCBParam param);
    int AddDelRangeZSet(DelRangeZSetCBParam param);
    int AddUpdateZSet(UpdateZSetCBParam param);
private:
    vector<SelectCBParam> m_vtSelect;
    vector<InsertCBParam> m_vtInsert;
    vector<UpdateCBParam> m_vtUpdate;
    vector<DelCBParam> m_vtDel;
    vector<DelCBParamForBatch> m_vtDelForBatch;
    vector<UpdateCBParamForBatch> m_vtUpdateForBatch;
    vector<GetCountCBParam> m_vtGetCount;
    vector<GetSetCBParam> m_vtGetSet;
    vector<DelSetCBParam> m_vtDelSet;
    vector<GetScoreZSetCBParam> m_vtGetScoreZSet;
    vector<GetRankZSetCBParam> m_vtGetRankZSet;
    vector<GetRangeZSetCBParam> m_vtGetRangeZSet;
    vector<GetRangeZSetByScoreCBParam> m_vtGetRangeZSetByScore;
    vector<IncScoreZSetCBParam> m_vtIncScoreZSet;
    vector<DelZSetCBParam> m_vtDelZSet;
    vector<DelRangeZSetCBParam> m_vtDelRangeZSet;
    vector<UpdateZSetCBParam> m_vtUpdateZSet;
    char m_cStatus;
    TC_ThreadMutex m_mutex;
    string m_sMainKey;
    time_t m_tCreat;
};
typedef tars::TC_AutoPtr<MKDbAccessCBParam> MKDbAccessCBParamPtr;

class CBQueue
{
public:
    CBQueue()
    {
        _timeout = 60;
    }
    ~CBQueue()
    {
        _callbackParams.clear();
    }
    MKDbAccessCBParamPtr getCBParamPtr(const string &mainKey, bool &isCreate);
    void erase(const string &mainKey);
private:
    map<string, MKDbAccessCBParamPtr> _callbackParams;
    TC_ThreadMutex _mutex;
    int _timeout;
};

class MKDbAccessCallback : public DbAccessPrxCallback
{
public:
    enum
    {
        SUCC = 0,
        ERROR = 1
    };
public:
    //定义构造函数，保存上下文
    MKDbAccessCallback(MKDbAccessCBParamPtr &cbParamPtr,
                       const string &sBinLogFile,
                       bool bRecordBinLog,
                       bool bKeyRecordBinLog,
                       bool bSaveOnlyKey,
                       bool bHead,
                       bool bUpdateOrder,
                       const string &sOrderItem,
                       bool bOrderDesc,
                       size_t iMkeyMaxSelectCount,
                       TC_Multi_HashMap_Malloc::MainKey::KEYTYPE keyType = TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE) :
        _cbParamPtr(cbParamPtr),
        _binlogFile(sBinLogFile),
        _keyBinlogFile(sBinLogFile + "key"),
        _recordBinLog(bRecordBinLog),
        _recordKeyBinLog(bKeyRecordBinLog),
        _saveOnlyKey(bSaveOnlyKey),
        _insertAtHead(bHead),
        _updateInOrder(bUpdateOrder),
        _orderItem(sOrderItem),
        _orderByDesc(bOrderDesc),
        _mkeyMaxSelectCount(iMkeyMaxSelectCount),
        _storeKeyType(keyType) {}

    virtual void callback_select(tars::Int32 ret, const vector<map<std::string, std::string> >& vtData);
    virtual void callback_select_exception(tars::Int32 ret);

private:
    int addCache(const vector<map<std::string, std::string> >& vtData, const int ret);
    void procCallBack(const char cFlag, const int iRetCode = TC_Multi_HashMap_Malloc::RT_OK);
    void procSelect(const vector<MKDbAccessCBParam::SelectCBParam> &vtSelect, const char cFlag, const int iRetCode);
    void procSelect(const string sMainKey, const MKDbAccessCBParam::SelectCBParam& selectCbParam, const vector<Condition> &vtCondition);
    void procInsert(const vector<MKDbAccessCBParam::InsertCBParam> &vtInsert, const char cFlag, const int iRetCode);
    void procUpdate(const vector<MKDbAccessCBParam::UpdateCBParam> &vtUpdate, const char cFlag, const int iRetCode);
    void procDel(const vector<MKDbAccessCBParam::DelCBParam> &vtDel, const char cFlag, const int iRetCode);
    void procDelForBatch(const vector<MKDbAccessCBParam::DelCBParamForBatch> &vtDel, const char cFlag, const int iRetCode);
    void procUpdateBatch(const vector<MKDbAccessCBParam::UpdateCBParamForBatch> &vtUpdateBatch, const char cFlag, const int iRetCode);
    void procGetCount(const vector<MKDbAccessCBParam::GetCountCBParam> &m_vtGetCount, const char cFlag, const int iRetCode);

    int stringOrderData(const vector<map<std::string, std::string> >& vtData, vector<int> &retData);
    int intOrderData(const vector<map<std::string, std::string> >& vtData, vector<int> &retData);
    void procGetSet(const vector<MKDbAccessCBParam::GetSetCBParam> &vtGetSet, const char cFlag, const int iRetCode);
    void procDelSet(const vector<MKDbAccessCBParam::DelSetCBParam> &vtDelSet, const char cFlag, const int iRetCode);

    void procGetScoreZSet(const vector<MKDbAccessCBParam::GetScoreZSetCBParam> &vtGetScoreZSet, const char cFlag, const int iRetCode);
    void procGetRankZSet(const vector<MKDbAccessCBParam::GetRankZSetCBParam> &vtGetRankZSet, const char cFlag, const int iRetCode);
    void procGetRangeZSet(const vector<MKDbAccessCBParam::GetRangeZSetCBParam> &vtGetRangeZSet, const char cFlag, const int iRetCode);
    void procGetRangeZSetByScore(const vector<MKDbAccessCBParam::GetRangeZSetByScoreCBParam> &vtGetRangeZSetByScore, const char cFlag, const int iRetCode);
    void procDelZSet(const vector<MKDbAccessCBParam::DelZSetCBParam> &vtDelZSet, const char cFlag, const int iRetCode);
    void procDelRangeZSet(const vector<MKDbAccessCBParam::DelRangeZSetCBParam> &vtDelRangeZSet, const char cFlag, const int iRetCode);
    void procUpdateZSet(const vector<MKDbAccessCBParam::UpdateZSetCBParam> &vtUpdateZSet, const char cFlag, const int iRetCode);

private:
    MKDbAccessCBParamPtr _cbParamPtr;
    string _binlogFile;
    string _keyBinlogFile;
    bool _recordBinLog;
    bool _recordKeyBinLog;
    bool _saveOnlyKey;
    bool _insertAtHead;
    bool _updateInOrder;

    string _orderItem;//排序的字段名
    bool _orderByDesc;//是否以递减的顺序排
    //查询主key数据，返回记录数限制
    size_t _mkeyMaxSelectCount;

    TC_Multi_HashMap_Malloc::MainKey::KEYTYPE _storeKeyType;
};

extern CBQueue g_cbQueue;

#endif
