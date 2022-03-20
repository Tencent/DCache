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
#include "MKDbAccessCallback.h"

CBQueue g_cbQueue;

int MKDbAccessCBParam::AddSelect(SelectCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtSelect.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddInsert(InsertCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtInsert.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddUpdate(UpdateCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtUpdate.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddDel(DelCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtDel.push_back(param);

    return 0;
}
int MKDbAccessCBParam::AddDelForBatch(DelCBParamForBatch param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtDelForBatch.push_back(param);

    return 0;
}
int MKDbAccessCBParam::AddUpdateForBatch(UpdateCBParamForBatch param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtUpdateForBatch.push_back(param);

    return 0;
}
int MKDbAccessCBParam::AddGetCount(GetCountCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtGetCount.push_back(param);

    return 0;
}
int MKDbAccessCBParam::AddGetSet(GetSetCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtGetSet.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddDelSet(DelSetCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtDelSet.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddGetScoreZSet(GetScoreZSetCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtGetScoreZSet.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddGetRankZSet(GetRankZSetCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtGetRankZSet.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddGetRangeZSet(GetRangeZSetCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtGetRangeZSet.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddGetRangeZSetByScore(GetRangeZSetByScoreCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtGetRangeZSetByScore.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddIncScoreZSet(IncScoreZSetCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtIncScoreZSet.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddDelZSet(DelZSetCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtDelZSet.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddDelRangeZSet(DelRangeZSetCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtDelRangeZSet.push_back(param);

    return 0;
}

int MKDbAccessCBParam::AddUpdateZSet(UpdateZSetCBParam param)
{
    TC_LockT<TC_ThreadMutex> lock(m_mutex);
    if (m_cStatus != MKDbAccessCBParam::INIT)
        return 1;
    m_vtUpdateZSet.push_back(param);

    return 0;
}

MKDbAccessCBParamPtr CBQueue::getCBParamPtr(const string &mainKey, bool &isCreate)
{
    TC_LockT<TC_ThreadMutex> lock(_mutex);
    MKDbAccessCBParamPtr p;
    map<string, MKDbAccessCBParamPtr>::iterator it = _callbackParams.find(mainKey);
    if (it == _callbackParams.end())
    {
        p = new MKDbAccessCBParam(mainKey);
        p->setStatus(MKDbAccessCBParam::INIT);
        _callbackParams[mainKey] = p;
        isCreate = true;
    }
    else
    {
        time_t tNow = TC_TimeProvider::getInstance()->getNow();
        if (tNow - it->second->getCreateTime() > _timeout)
        {
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            TLOG_ERROR("CBQueue::getCBParamPtr: " << mainKey << " timeout, erase it" << endl);
            _callbackParams.erase(mainKey);
            p = new MKDbAccessCBParam(mainKey);
            p->setStatus(MKDbAccessCBParam::INIT);
            _callbackParams[mainKey] = p;
            isCreate = true;
        }
        else
        {
            p = it->second;
            isCreate = false;
        }
    }
    return p;
}

void CBQueue::erase(const string &mainKey)
{
    TC_LockT<TC_ThreadMutex> lock(_mutex);
    _callbackParams.erase(mainKey);
}


void MKDbAccessCallback::callback_select(tars::Int32 ret, const vector<map<std::string, std::string> >& vtData)
{
    TC_LockT<TC_ThreadMutex> lock(_cbParamPtr->getMutex());
    int iRet = addCache(vtData, ret);
    if (iRet != 0)
    {
        TLOG_ERROR("[MKDbAccessCallback::callback_select] addCache error, ret = " << iRet << ", mainKey = " << _cbParamPtr->getMainKey() << endl);
    }
    if (ret >= 0)
    {
        if (iRet == 0)
        {
            procCallBack(MKDbAccessCallback::SUCC);
        }
        else
        {
            procCallBack(MKDbAccessCallback::ERROR, ET_DB_TO_CACHE_ERR);
        }
    }
    else
    {
        TLOG_ERROR("[MKDbAccessCallback::callback_select] error, ret = " << ret << ", mainKey = " << _cbParamPtr->getMainKey() << endl);
        procCallBack(MKDbAccessCallback::ERROR);
        g_app.ppReport(PPReport::SRP_DB_ERR, 1);
    }
    _cbParamPtr->setStatus(MKDbAccessCBParam::FINISH);
    lock.release();
    g_cbQueue.erase(_cbParamPtr->getMainKey());
}

void MKDbAccessCallback::callback_select_exception(tars::Int32 ret)
{
    TC_LockT<TC_ThreadMutex> lock(_cbParamPtr->getMutex());
    TLOG_ERROR("[MKDbAccessCallback::callback_select_exception] ret = " << ret << ", mainKey = " << _cbParamPtr->getMainKey() << endl);
    g_app.ppReport(PPReport::SRP_DB_EX, 1);
    procCallBack(MKDbAccessCallback::ERROR);
    _cbParamPtr->setStatus(MKDbAccessCBParam::FINISH);
    lock.release();
    g_cbQueue.erase(_cbParamPtr->getMainKey());
}

int MKDbAccessCallback::stringOrderData(const vector<map<std::string, std::string> >& vtData, vector<int> &retData)
{
    //通过map来排序
    map<string, int> result;

    for (size_t i = 0; i < vtData.size(); i++)
    {
        map<std::string, std::string>::const_iterator it = vtData[i].find(_orderItem);
        if (it == vtData[i].end())
        {
            TLOG_ERROR("[MKDbAccessCallback::stringOrderData] not found item " << _orderItem << endl);
            return -1;
        }
        result[it->second] = i;
    }

    retData.reserve(vtData.size());

    if (_orderByDesc)
    {
        map<string, int>::reverse_iterator it = result.rbegin();
        for (; it != result.rend(); it++)
        {
            retData.push_back(it->second);
        }
    }
    else
    {
        map<string, int>::iterator it = result.begin();
        for (; it != result.end(); it++)
        {
            retData.push_back(it->second);
        }
    }

    return 0;
}

int MKDbAccessCallback::intOrderData(const vector<map<std::string, std::string> >& vtData, vector<int> &retData)
{
    //通过map来排序
    map<double, int> result;

    for (size_t i = 0; i < vtData.size(); i++)
    {
        map<std::string, std::string>::const_iterator it = vtData[i].find(_orderItem);
        if (it == vtData[i].end())
        {
            TLOG_ERROR("[MKDbAccessCallback::intOrderData] not found item " << _orderItem << endl);
            return -1;
        }
        result[TC_Common::strto<double>(it->second)] = i;
    }

    retData.reserve(vtData.size());

    if (_orderByDesc)
    {
        map<double, int>::reverse_iterator it = result.rbegin();
        for (; it != result.rend(); it++)
        {
            retData.push_back(it->second);
        }
    }
    else
    {
        map<double, int>::iterator it = result.begin();
        for (; it != result.end(); it++)
        {
            retData.push_back(it->second);
        }
    }

    return 0;
}

int MKDbAccessCallback::addCache(const vector<map<std::string, std::string> >& vtData, const int ret)
{
    string mk = _cbParamPtr->getMainKey();

    TLOG_DEBUG("async select db, ret:" << ret << " vtData.size() = " << vtData.size() << ", mainKey = " << mk << endl);
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (ret > 0)
    {
        if (vtData.size() > 0)
        {
            if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == _storeKeyType)
            {
                vector<MultiHashMap::Value> vtV;
                vtV.reserve(vtData.size());
                time_t NowTime = TC_TimeProvider::getInstance()->getNow();
                if (_orderItem.size() == 0)
                {
                    for (size_t i = 0; i < vtData.size(); i++)
                    {
                        string uk = createEncodeStream(vtData[i], UK_STREAM);
                        string value = createEncodeStream(vtData[i], VALUE_STREAM);

                        int ExpireTime = 0;
                        map<std::string, std::string>::const_iterator itExpireTime = vtData[i].find("sDCacheExpireTime");
                        if (itExpireTime != vtData[i].end())
                        {
                            ExpireTime = TC_Common::strto<int>(itExpireTime->second);
                        }
                        if (g_app.gstat()->isExpireEnabled())
                        {
                            TLOG_DEBUG("ExpireTime:" << ExpireTime << endl);
                            if (ExpireTime < NowTime && ExpireTime != 0)
                                continue;
                        }
                        MultiHashMap::Value v;
                        v._mkey = mk;
                        v._ukey = uk;
                        v._value = value;
                        v._iVersion = 0;
                        v._dirty = false;
                        v._isDelete = TC_Multi_HashMap_Malloc::DELETE_AUTO;//这里设为auto，避免查询回来之后把之前的delete覆盖了
                        v._iExpireTime = ExpireTime;
                        vtV.push_back(v);
                    }
                }
                else
                {
                    vector<int> retData;

                    map<string, FieldInfo>::const_iterator it = fieldConfig.mpFieldInfo.find(_orderItem);
                    if (it == fieldConfig.mpFieldInfo.end())
                    {
                        TLOG_ERROR("[MKDbAccessCallback::addCache] not found item " << _orderItem << endl);
                        return -1;
                    }
                    else
                    {
                        if (it->second.type == "short" || it->second.type == "int" || it->second.type == "long" || it->second.type == "float" ||
                                it->second.type == "double" || it->second.type == "unsigned int" || it->second.type == "unsigned short")
                        {
                            if (intOrderData(vtData, retData) < 0)
                            {
                                TLOG_ERROR("[MKDbAccessCallback::addCache] intOrderData ret < 0 " << endl);
                                return -1;
                            }
                        }
                        else if (it->second.type == "byte" || it->second.type == "string")
                        {
                            if (stringOrderData(vtData, retData) < 0)
                            {
                                TLOG_ERROR("[MKDbAccessCallback::addCache] stringOrderData ret < 0 " << endl);
                                return -1;
                            }
                        }
                        else
                        {
                            TLOG_ERROR("[MKDbAccessCallback::addCache] unknow item type " << it->second.type << endl);
                            return -1;
                        }
                    }
                    for (size_t n = 0; n < retData.size(); n++)
                    {
                        size_t i = retData[n];
                        string uk = createEncodeStream(vtData[i], UK_STREAM);
                        string value = createEncodeStream(vtData[i], VALUE_STREAM);

                        int ExpireTime = 0;
                        map<std::string, std::string>::const_iterator itExpireTime = vtData[i].find("sDCacheExpireTime");
                        if (itExpireTime != vtData[i].end())
                        {
                            ExpireTime = TC_Common::strto<int>(itExpireTime->second);
                        }

                        if (g_app.gstat()->isExpireEnabled())
                        {
                            TLOG_DEBUG("ExpireTime:" << ExpireTime << endl);
                            if (ExpireTime < NowTime && ExpireTime != 0)
                                continue;
                        }
                        MultiHashMap::Value v;
                        v._mkey = mk;
                        v._ukey = uk;
                        v._value = value;
                        v._iVersion = 0;
                        v._dirty = false;
                        v._isDelete = TC_Multi_HashMap_Malloc::DELETE_AUTO;//这里设为auto，避免查询回来之后把之前的delete覆盖了
                        v._iExpireTime = ExpireTime;
                        vtV.push_back(v);
                    }
                }
                if (vtV.size() != 0)
                {
                    int iRet = g_HashMap.set(vtV, TC_Multi_HashMap_Malloc::FULL_DATA, _insertAtHead, _updateInOrder, _orderItem.size() != 0, false);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //严重错误
                        TLOG_ERROR("[MKDbAccessCallback::addCache] set error, ret = " << iRet << ", mainKey = " << mk << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        g_HashMap.erase(mk);

                        return -1;
                    }
                    if (_recordBinLog)
                        WriteBinLog::set(mk, vtV, true, _binlogFile, true);
                    if (_recordKeyBinLog)
                        WriteBinLog::set(mk, vtV, true, _keyBinlogFile, true);
                }
                else
                {
                    int iCheckRet = g_HashMap.checkMainKey(mk);

                    if (iCheckRet == TC_Multi_HashMap_Malloc::RT_OK || iCheckRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        int iRet = g_HashMap.setFullData(mk, true);

                        if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            TLOG_ERROR("addCache|" << mk << "|setFullData|failed|" << iRet << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            return -2;
                        }
                    }
                    else if (iCheckRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
                    {
                        if (_saveOnlyKey)
                        {
                            int iRet = g_HashMap.set(mk);

                            if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_DATA_EXIST && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                            {
                                TLOG_ERROR("addCache|" << mk << "|saveOnlyKey|failed|" << iRet << endl);
                                g_app.ppReport(PPReport::SRP_EX, 1);
                                return -3;
                            }
                            if (_recordBinLog)
                                WriteBinLog::setMKOnlyKey(mk, _binlogFile);
                            if (_recordKeyBinLog)
                                WriteBinLog::setMKOnlyKey(mk, _keyBinlogFile);
                        }
                    }
                }

            }
            else if (TC_Multi_HashMap_Malloc::MainKey::SET_TYPE == _storeKeyType)
            {
                vector<MultiHashMap::Value> vtV;
                vtV.reserve(vtData.size());
                time_t NowTime = TC_TimeProvider::getInstance()->getNow();
                for (size_t i = 0; i < vtData.size(); i++)
                {
                    string value = createEncodeStream(vtData[i], VALUE_STREAM);

                    int ExpireTime = 0;
                    map<std::string, std::string>::const_iterator itExpireTime = vtData[i].find("sDCacheExpireTime");
                    if (itExpireTime != vtData[i].end())
                    {
                        ExpireTime = TC_Common::strto<int>(itExpireTime->second);
                    }

                    if (g_app.gstat()->isExpireEnabled())
                    {
                        TLOG_DEBUG("ExpireTime:" << ExpireTime << endl);
                        if (ExpireTime < NowTime && ExpireTime != 0)
                            continue;
                    }
                    MultiHashMap::Value v;
                    v._mkey = mk;
                    v._value = value;
                    v._iVersion = 0;
                    v._dirty = false;
                    v._isDelete = TC_Multi_HashMap_Malloc::DELETE_AUTO;//这里设为auto，避免查询回来之后把之前的delete覆盖了
                    v._iExpireTime = ExpireTime;
                    vtV.push_back(v);
                }
                if (vtV.size() != 0)
                {
                    int iRet = g_HashMap.addSet(mk, vtV, TC_Multi_HashMap_Malloc::FULL_DATA);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //严重错误
                        TLOG_ERROR("[MKDbAccessCallback::addCache] addSet error, ret = " << iRet << ", mainKey = " << mk << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        g_HashMap.erase(mk);

                        return -1;
                    }
                    if (_recordBinLog)
                        WriteBinLog::addSet(mk, vtV, true, _binlogFile, true);
                    if (_recordKeyBinLog)
                        WriteBinLog::addSet(mk, vtV, true, _keyBinlogFile, true);
                }
                else
                {
                    int iCheckRet = g_HashMap.checkMainKey(mk);

                    if (iCheckRet == TC_Multi_HashMap_Malloc::RT_OK || iCheckRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        int iRet = g_HashMap.setFullData(mk, true);

                        if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            TLOG_ERROR("addCache|" << mk << "|setFullData|failed|" << iRet << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            return -2;
                        }
                    }
                    else if (iCheckRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
                    {
                        if (_saveOnlyKey)
                        {
                            int iRet = g_HashMap.addSet(mk);

                            if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_DATA_EXIST && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                            {
                                TLOG_ERROR("addCache|" << mk << "|OnlyKey|failed|" << iRet << endl);
                                g_app.ppReport(PPReport::SRP_EX, 1);
                                return -3;
                            }
                            if (_recordBinLog)
                                WriteBinLog::addSet(mk, _binlogFile);
                            if (_recordKeyBinLog)
                                WriteBinLog::addSet(mk, _keyBinlogFile);
                        }
                    }
                }
            }
            else if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == _storeKeyType)
            {
                vector<MultiHashMap::Value> vtV;
                vtV.reserve(vtData.size());
                time_t NowTime = TC_TimeProvider::getInstance()->getNow();
                for (size_t i = 0; i < vtData.size(); i++)
                {
                    string value = createEncodeStream(vtData[i], VALUE_STREAM);

                    int ExpireTime = 0;
                    map<std::string, std::string>::const_iterator itExpireTime = vtData[i].find("sDCacheExpireTime");
                    if (itExpireTime != vtData[i].end())
                    {
                        ExpireTime = TC_Common::strto<int>(itExpireTime->second);
                    }
                    if (g_app.gstat()->isExpireEnabled())
                    {
                        TLOG_DEBUG("ExpireTime:" << ExpireTime << endl);
                        if (ExpireTime < NowTime && ExpireTime != 0)
                            continue;
                    }
                    MultiHashMap::Value v;
                    v._mkey = mk;
                    v._value = value;
                    v._score = TC_Common::strto<double>(vtData[i].find("sDCacheZSetScore")->second);
                    v._iVersion = 0;
                    v._dirty = false;
                    v._isDelete = TC_Multi_HashMap_Malloc::DELETE_AUTO;//这里设为auto，避免查询回来之后把之前的delete覆盖了
                    v._iExpireTime = ExpireTime;
                    vtV.push_back(v);
                }
                if (vtV.size() != 0)
                {
                    int iRet = g_HashMap.addZSet(mk, vtV, TC_Multi_HashMap_Malloc::FULL_DATA);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //严重错误
                        TLOG_ERROR("[MKDbAccessCallback::addCache] addSet error, ret = " << iRet << ", mainKey = " << mk << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        g_HashMap.erase(mk);

                        return -1;
                    }
                    if (_recordBinLog)
                        WriteBinLog::addZSet(mk, vtV, true, _binlogFile, true);
                    if (_recordKeyBinLog)
                        WriteBinLog::addZSet(mk, vtV, true, _keyBinlogFile, true);
                }
                else
                {
                    int iCheckRet = g_HashMap.checkMainKey(mk);

                    if (iCheckRet == TC_Multi_HashMap_Malloc::RT_OK || iCheckRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        int iRet = g_HashMap.setFullData(mk, true);

                        if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            TLOG_ERROR("addCache|" << mk << "|setFullData|failed|" << iRet << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            return -2;
                        }
                    }
                    else if (iCheckRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
                    {
                        if (_saveOnlyKey)
                        {
                            int iRet = g_HashMap.addZSet(mk);

                            if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_DATA_EXIST && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                            {
                                TLOG_ERROR("addCache|" << mk << "|OnlyKey|failed|" << iRet << endl);
                                g_app.ppReport(PPReport::SRP_EX, 1);
                                return -3;
                            }
                            if (_recordBinLog)
                                WriteBinLog::addZSet(mk, _binlogFile);
                            if (_recordKeyBinLog)
                                WriteBinLog::addZSet(mk, _keyBinlogFile);
                        }
                    }
                }
            }
        }
    }
    else if (ret == 0)
    {
        int iCheckRet = g_HashMap.checkMainKey(mk);

        if (iCheckRet == TC_Multi_HashMap_Malloc::RT_OK || iCheckRet == TC_Multi_HashMap_Malloc::RT_PART_DATA)
        {
            int iRet = g_HashMap.setFullData(mk, true);

            if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
            {
                TLOG_ERROR("addCache|" << mk << "|FullData|failed|" << iRet << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                return -2;
            }
        }
        else if (iCheckRet == TC_Multi_HashMap_Malloc::RT_NO_DATA)
        {
            if (_saveOnlyKey)
            {
                if (mk.size() > 1000)
                    return 0;

                if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == _storeKeyType)
                {
                    int iRet = g_HashMap.set(mk);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_DATA_EXIST && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("addCache|" << mk << "|OnlyKey|failed|" << iRet << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        return -3;
                    }
                    if (_recordBinLog)
                        WriteBinLog::setMKOnlyKey(mk, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::setMKOnlyKey(mk, _keyBinlogFile);
                }
                else if (TC_Multi_HashMap_Malloc::MainKey::SET_TYPE == _storeKeyType)
                {
                    int iRet = g_HashMap.addSet(mk);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_DATA_EXIST && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("addCache|" << mk << "|OnlyKey|failed|" << iRet << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        return -3;
                    }
                    if (_recordBinLog)
                        WriteBinLog::addSet(mk, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::addSet(mk, _keyBinlogFile);
                }
                else if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == _storeKeyType)
                {
                    int iRet = g_HashMap.addZSet(mk);

                    if (iRet != TC_Multi_HashMap_Malloc::RT_OK && iRet != TC_Multi_HashMap_Malloc::RT_DATA_EXIST && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("addCache|" << mk << "|OnlyKey|failed|" << iRet << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        return -3;
                    }
                    if (_recordBinLog)
                        WriteBinLog::addZSet(mk, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::addZSet(mk, _keyBinlogFile);
                }
            }
        }
    }
    return 0;
}

void MKDbAccessCallback::procCallBack(const char cFlag, const int iRetCode)
{
    procSelect(_cbParamPtr->getSelect(), cFlag, iRetCode);
    procInsert(_cbParamPtr->getInsert(), cFlag, iRetCode);
    procUpdate(_cbParamPtr->getUpdate(), cFlag, iRetCode);
    procDel(_cbParamPtr->getDel(), cFlag, iRetCode);
    procDelForBatch(_cbParamPtr->getDelBatch(), cFlag, iRetCode);
    procUpdateBatch(_cbParamPtr->getUpdateBatch(), cFlag, iRetCode);
    procGetCount(_cbParamPtr->getGetCount(), cFlag, iRetCode);

    procGetSet(_cbParamPtr->getGetSet(), cFlag, iRetCode);
    procDelSet(_cbParamPtr->getDelSet(), cFlag, iRetCode);

    procGetScoreZSet(_cbParamPtr->getGetScoreZSet(), cFlag, iRetCode);
    procGetRankZSet(_cbParamPtr->getGetRankZSet(), cFlag, iRetCode);
    procGetRangeZSet(_cbParamPtr->getGetRangeZSet(), cFlag, iRetCode);
    procGetRangeZSetByScore(_cbParamPtr->getGetRangeZSetByScore(), cFlag, iRetCode);
    procDelZSet(_cbParamPtr->getDelZSet(), cFlag, iRetCode);
    procDelRangeZSet(_cbParamPtr->getDelRangeZSet(), cFlag, iRetCode);
    procUpdateZSet(_cbParamPtr->getUpdateZSet(), cFlag, iRetCode);
}

void MKDbAccessCallback::procSelect(const vector<MKDbAccessCBParam::SelectCBParam> &vtSelect, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < vtSelect.size(); ++i)
        {
            if (vtSelect[i].bBatch && vtSelect[i].pParam->bEnd)
            {
                continue;
            }

            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procSelect: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtSelect.size(); ++j)
                    {
                        if (vtSelect[j].bBatch)
                        {
                            if (vtSelect[j].pParam->setEnd())
                            {
                                if (vtSelect[j].bMUKBatch)
                                {
                                    MUKBatchRsp rsp;
                                    MKCache::async_response_getMUKBatch(vtSelect[j].current, ET_KEY_AREA_ERR, rsp);
                                }
                                else if (vtSelect[j].bBatchOr)
                                {
                                    MKVBatchExRsp rsp;
                                    MKCache::async_response_getMKVBatchEx(vtSelect[j].current, ET_KEY_AREA_ERR, rsp);
                                }
                                else
                                {
                                    MKVBatchRsp rsp;
                                    MKCache::async_response_getMKVBatch(vtSelect[j].current, ET_KEY_AREA_ERR, rsp);

                                }
                            }
                        }
                        else
                        {
                            map<string, string>& context = vtSelect[j].current->getContext();
                            //API直连模式，返回增量更新路由
                            if (VALUE_YES == context[GET_ROUTE])
                            {
                                RspUpdateServant updateServant;
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                int ret = RouterHandle::getInstance()->getUpdateServant(sMainKey, false, context[API_IDC], updateServant);
                                if (ret != 0)
                                {
                                    TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                                }
                                else
                                {
                                    RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                                    vtSelect[j].current->setResponseContext(rspContext);
                                }
                            }

                            GetMKVRsp rsp;
                            MKCache::async_response_getMKV(vtSelect[j].current, ET_KEY_AREA_ERR, rsp);
                        }
                    }
                    return;
                }
                MainKeyValue keyValue;
                if (vtSelect[i].bUKey)
                {
                    if (!vtSelect[i].bMUKBatch)
                    {
                        procSelect(sMainKey, vtSelect[i], vtSelect[i].vtUKCond);
                    }
                    else
                    {
                        for (size_t k = 0; k < vtSelect[i].vvUKConds.size(); ++k)
                        {
                            TLOG_DEBUG("async selectbatchex db " << endl);
                            procSelect(sMainKey, vtSelect[i], vtSelect[i].vvUKConds[k]);
                        }
                    }

                }
                else
                {
                    vector<MultiHashMap::Value> vtValue;

                    size_t iNum = -1;
                    size_t iStart = 0;
                    size_t iDataCount = 0;
                    if (!vtSelect[i].stLimit.bLimit)
                    {
                        g_HashMap.get(sMainKey, iDataCount);
                        if (iDataCount > _mkeyMaxSelectCount)
                        {
                            FDLOG("MainKeyDataCount") << "[MKDbAccessCallback::procSelect] mainKey=" << sMainKey << " dataCount=" << iDataCount << endl;
                            TLOG_ERROR("MKDbAccessCallback::procSelect: g_HashMap.get(mk, iDataCount) error, mainKey = " << sMainKey << " iDataCount = " << iDataCount << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            if (vtSelect[i].bBatch)
                            {
                                if (vtSelect[i].pParam->setEnd())
                                {
                                    if (!vtSelect[i].bBatchOr)
                                    {
                                        MKVBatchRsp rsp;
                                        MKCache::async_response_getMKVBatch(vtSelect[i].current, ET_DATA_TOO_MUCH, rsp);
                                    }
                                    else
                                    {
                                        MKVBatchExRsp rsp;
                                        MKCache::async_response_getMKVBatchEx(vtSelect[i].current, ET_DATA_TOO_MUCH, rsp);
                                    }
                                }
                            }
                            else
                            {
                                GetMKVRsp rsp;
                                rsp.data = keyValue.value;
                                MKCache::async_response_getMKV(vtSelect[i].current, ET_DATA_TOO_MUCH, rsp);
                            }
                            continue;
                        }
                    }

                    if (vtSelect[i].stLimit.bLimit && vtSelect[i].vtUKCond.size() == 0 && vtSelect[i].vtValueCond.size() == 0 && vtSelect[i].vvUKConds.size() == 0)
                    {
                        iStart = vtSelect[i].stLimit.iIndex;
                        iNum = vtSelect[i].stLimit.iCount;
                    }

                    int iRet;
                    if (g_app.gstat()->isExpireEnabled())
                    {
                        iRet = g_HashMap.get(sMainKey, vtValue, iNum, iStart, true, true, TC_TimeProvider::getInstance()->getNow());
                    }
                    else
                    {
                        iRet = g_HashMap.get(sMainKey, vtValue, iNum, iStart);
                    }

                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        if (!vtSelect[i].bBatchOr)
                            getSelectResult(vtSelect[i].vtField, sMainKey, vtValue, vtSelect[i].vtUKCond, vtSelect[i].vtValueCond, vtSelect[i].stLimit, keyValue.value);
                        else
                            getSelectResult(vtSelect[i].vtField, sMainKey, vtValue, vtSelect[i].vvUKConds, vtSelect[i].stLimit, keyValue.value);

                        if (vtSelect[i].bGetMKCout)
                        {
                            keyValue.ret = g_HashMap.count(sMainKey);
                        }
                        else
                        {
                            keyValue.ret = keyValue.value.size();
                        }
                        if (vtSelect[i].bBatch)
                        {
                            keyValue.mainKey = sMainKey;
                            vtSelect[i].pParam->addValue(keyValue);
                            if ((--(vtSelect[i].pParam->count)) <= 0)
                            {
                                if (!vtSelect[i].bBatchOr)
                                {
                                    MKVBatchRsp rsp;
                                    rsp.data = vtSelect[i].pParam->vtKeyValue;
                                    MKCache::async_response_getMKVBatch(vtSelect[i].current, ET_SUCC, rsp);
                                }
                                else
                                {
                                    MKVBatchExRsp rsp;
                                    rsp.data = vtSelect[i].pParam->vtKeyValue;
                                    MKCache::async_response_getMKVBatchEx(vtSelect[i].current, ET_SUCC, rsp);
                                }

                                vtSelect[i].pParam->bEnd = true;
                            }
                        }
                        else
                        {
                            GetMKVRsp rsp;
                            rsp.data = keyValue.value;
                            MKCache::async_response_getMKV(vtSelect[i].current, keyValue.ret, rsp);
                        }
                    }
                    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        if (vtSelect[i].bBatch)
                        {
                            keyValue.mainKey = sMainKey;
                            keyValue.ret = 0;
                            vtSelect[i].pParam->addValue(keyValue);
                            if ((--(vtSelect[i].pParam->count)) <= 0)
                            {
                                if (!vtSelect[i].bBatchOr)
                                {
                                    MKVBatchRsp rsp;
                                    rsp.data = vtSelect[i].pParam->vtKeyValue;
                                    MKCache::async_response_getMKVBatch(vtSelect[i].current, ET_SUCC, rsp);
                                }
                                else
                                {
                                    MKVBatchExRsp rsp;
                                    rsp.data = vtSelect[i].pParam->vtKeyValue;
                                    MKCache::async_response_getMKVBatchEx(vtSelect[i].current, ET_SUCC, rsp);
                                }
                                vtSelect[i].pParam->bEnd = true;
                            }
                        }
                        else
                        {
                            GetMKVRsp rsp;
                            rsp.data = keyValue.value;
                            MKCache::async_response_getMKV(vtSelect[i].current, 0, rsp);
                        }
                    }
                    else
                    {
                        TLOG_ERROR("MKDbAccessCallback::procSelect g_HashMap.get return " << iRet << ", mainKey = " << sMainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        if (vtSelect[i].bBatch)
                        {
                            if (vtSelect[i].pParam->setEnd())
                            {
                                if (!vtSelect[i].bBatchOr)
                                {
                                    MKVBatchRsp rsp;
                                    MKCache::async_response_getMKVBatch(vtSelect[i].current, ET_SYS_ERR, rsp);
                                }
                                else
                                {
                                    MKVBatchExRsp rsp;
                                    MKCache::async_response_getMKVBatchEx(vtSelect[i].current, ET_SYS_ERR, rsp);
                                }
                            }
                        }
                        else
                        {
                            GetMKVRsp rsp;
                            rsp.data = keyValue.value;
                            MKCache::async_response_getMKV(vtSelect[i].current, ET_SYS_ERR, rsp);
                        }
                    }
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procSelect exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procSelect unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtSelect.size(); ++i)
        {
            try
            {
                if (vtSelect[i].bBatch)
                {
                    if (vtSelect[i].pParam->setEnd())
                    {

                        if (vtSelect[i].bMUKBatch)
                        {
                            MUKBatchRsp rsp;
                            if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                MKCache::async_response_getMUKBatch(vtSelect[i].current, ET_DB_ERR, rsp);
                            }
                            else
                            {
                                MKCache::async_response_getMUKBatch(vtSelect[i].current, iRetCode, rsp);
                            }
                        }
                        else if (vtSelect[i].bBatchOr)
                        {
                            MKVBatchExRsp rsp;;
                            if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                MKCache::async_response_getMKVBatchEx(vtSelect[i].current, ET_DB_ERR, rsp);
                            }
                            else
                            {
                                MKCache::async_response_getMKVBatchEx(vtSelect[i].current, iRetCode, rsp);
                            }
                        }
                        else
                        {
                            MKVBatchRsp rsp;
                            if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                MKCache::async_response_getMKVBatch(vtSelect[i].current, ET_DB_ERR, rsp);
                            }
                            else
                            {
                                MKCache::async_response_getMKVBatch(vtSelect[i].current, iRetCode, rsp);
                            }
                        }
                    }
                }
                else
                {
                    GetMKVRsp rsp;
                    if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        MKCache::async_response_getMKV(vtSelect[i].current, ET_DB_ERR, rsp);
                    }
                    else
                    {
                        MKCache::async_response_getMKV(vtSelect[i].current, iRetCode, rsp);
                    }
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procSelect exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procSelect unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procSelect(const string sMainKey, const  MKDbAccessCBParam::SelectCBParam &selectCbParam, const vector<Condition> &vtCondition)
{
    MainKeyValue keyValue;
    Record record;  //selectBatchEX时使用
    string uk;
    MultiHashMap::Value v;
    TarsEncode uKeyEncode;

    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    map<string, FieldInfo>::const_iterator itInfo;
    for (size_t j = 0; j < vtCondition.size(); ++j)
    {
        if (selectCbParam.bMUKBatch)
        {
            record.mpRecord[vtCondition[j].fieldName] = vtCondition[j].value;
        }
        itInfo = fieldConfig.mpFieldInfo.find(vtCondition[j].fieldName);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << " mpFieldInfo find error" << endl);
            return;
        }
        uKeyEncode.write(vtCondition[j].value, itInfo->second.tag, itInfo->second.type);
    }
    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

    int iRet;
    if (g_app.gstat()->isExpireEnabled())
    {
        iRet = g_HashMap.get(sMainKey, uk, v, true, TC_TimeProvider::getInstance()->getNow());
    }
    else
    {
        iRet = g_HashMap.get(sMainKey, uk, v);
    }


    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
    {
        TarsDecode vDecode;
        string value = v._value;
        vDecode.setBuffer(value);

        if (judge(vDecode, selectCbParam.vtValueCond))
        {
            if (!selectCbParam.stLimit.bLimit || (selectCbParam.stLimit.bLimit && selectCbParam.stLimit.iIndex == 0 && selectCbParam.stLimit.iCount >= 1))
                setResult(selectCbParam.vtField, sMainKey, selectCbParam.vtUKCond, vDecode, v._iVersion, v._iExpireTime, keyValue.value);
        }
        if (selectCbParam.bGetMKCout)
        {
            keyValue.ret = g_HashMap.count(sMainKey);
        }
        else
        {
            keyValue.ret = keyValue.value.size();
        }
        if (selectCbParam.bBatch)
        {
            if (selectCbParam.bMUKBatch)
            {

                record.mainKey = sMainKey;
                if (keyValue.value.size() != 1)
                {
                    TLOG_ERROR(__FUNCTION__ << ":" << __LINE__ << " value size error,should be 1 :" << keyValue.value.size() << endl);
                    MUKBatchRsp rsp;
                    MKCache::async_response_getMUKBatch(selectCbParam.current, ET_SYS_ERR, rsp);
                }
                else
                {
                    record.ret = 0;
                    record.mpRecord.insert(keyValue.value[0].begin(), keyValue.value[0].end());
                    selectCbParam.pParam->addValue(record);
                    if ((--(selectCbParam.pParam->count)) <= 0)
                    {
                        MUKBatchRsp rsp;
                        rsp.data = selectCbParam.pParam->vtData;
                        MKCache::async_response_getMUKBatch(selectCbParam.current, ET_SUCC, rsp);
                        selectCbParam.pParam->bEnd = true;
                    }
                }
            }
            else
            {
                keyValue.mainKey = sMainKey;
                selectCbParam.pParam->addValue(keyValue);
                if ((--(selectCbParam.pParam->count)) <= 0)
                {
                    MKVBatchRsp rsp;
                    rsp.data = selectCbParam.pParam->vtKeyValue;
                    MKCache::async_response_getMKVBatch(selectCbParam.current, ET_SUCC, rsp);
                    selectCbParam.pParam->bEnd = true;
                }
            }
        }
        else
        {
            GetMKVRsp rsp;
            rsp.data = keyValue.value;
            MKCache::async_response_getMKV(selectCbParam.current, keyValue.ret, rsp);
        }
    }
    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
    {
        if (selectCbParam.bGetMKCout)
        {
            keyValue.ret = g_HashMap.count(sMainKey);
        }
        else
        {
            keyValue.ret = 0;
        }
        if (selectCbParam.bBatch)
        {
            if (selectCbParam.bMUKBatch)
            {
                record.mainKey = sMainKey;
                iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY ? record.ret = ET_ONLY_KEY : record.ret = ET_NO_DATA;
                selectCbParam.pParam->addValue(record);
                if ((--(selectCbParam.pParam->count)) <= 0)
                {
                    MUKBatchRsp rsp;
                    rsp.data = selectCbParam.pParam->vtData;
                    MKCache::async_response_getMUKBatch(selectCbParam.current, ET_SUCC, rsp);
                    selectCbParam.pParam->bEnd = true;
                }
            }
            else
            {
                keyValue.mainKey = sMainKey;
                selectCbParam.pParam->addValue(keyValue);
                if ((--(selectCbParam.pParam->count)) <= 0)
                {
                    MKVBatchRsp rsp;
                    rsp.data = selectCbParam.pParam->vtKeyValue;
                    MKCache::async_response_getMKVBatch(selectCbParam.current, ET_SUCC, rsp);
                    selectCbParam.pParam->bEnd = true;
                }
            }
        }
        else
        {
            GetMKVRsp rsp;
            rsp.data = keyValue.value;
            MKCache::async_response_getMKV(selectCbParam.current, keyValue.ret, rsp);
        }
    }
    else
    {
        TLOG_ERROR("MKDbAccessCallback::procSelect g_HashMap.get return " << iRet << ", mainKey = " << sMainKey << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        if (selectCbParam.bBatch)
        {
            if (selectCbParam.pParam->setEnd())
            {

                if (selectCbParam.bMUKBatch)
                {
                    MUKBatchRsp rsp;
                    MKCache::async_response_getMUKBatch(selectCbParam.current, ET_SYS_ERR, rsp);

                }
                else
                {
                    MKVBatchRsp rsp;
                    MKCache::async_response_getMKVBatch(selectCbParam.current, ET_SYS_ERR, rsp);
                }
            }
        }
        else
        {
            GetMKVRsp rsp;
            rsp.data = keyValue.value;
            MKCache::async_response_getMKV(selectCbParam.current, ET_SYS_ERR, rsp);
        }
    }
}

void MKDbAccessCallback::procInsert(const vector<MKDbAccessCBParam::InsertCBParam> &vtInsert, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC /*|| cFlag == MKDbAccessCallback::NODATA*/)
    {
        int iPageNo = g_route_table.getPageNo(sMainKey);
        for (size_t i = 0; i < vtInsert.size(); ++i)
        {
            try
            {
                if (g_route_table.isTransfering(sMainKey) && g_route_table.isTransSrc(iPageNo, ServerConfig::ServerName))
                {
                    TLOG_ERROR("MKDbAccessCallback::procInsert: " << sMainKey << " forbid insert" << endl);
                    for (size_t j = i; j < vtInsert.size(); ++j)
                    {
                        if (vtInsert[j].bBatch)
                        {
                            vtInsert[j].pParam->addFailIndexReason(vtInsert[j].iIndex, ET_FORBID_OPT);
                            if ((--(vtInsert[j].pParam->count)) <= 0)
                            {
                                if (vtInsert[j].pParam->updateServant.mpServant.size() > 0)
                                {
                                    map<string, string> rspContext;
                                    rspContext[ROUTER_UPDATED] = "";
                                    RouterHandle::getInstance()->updateServant2Str(vtInsert[j].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                    vtInsert[j].current->setResponseContext(rspContext);
                                }
                                MKWCache::async_response_insertMKVBatch(vtInsert[j].current, ET_PARTIAL_FAIL, vtInsert[j].pParam->rsp);
                            }
                        }
                        else
                        {
                            MKWCache::async_response_insertMKV(vtInsert[j].current, ET_FORBID_OPT);
                        }

                    }
                    return;
                }

                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procInsert: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtInsert.size(); ++j)
                    {
                        if (vtInsert[j].bBatch)
                        {
                            //API直连模式，返回增量更新路由
                            map<string, string>& context = vtInsert[j].current->getContext();
                            if (VALUE_YES == context[GET_ROUTE])
                            {
                                ServerInfo serverInfo;
                                int ret = g_route_table.getMaster(sMainKey, serverInfo);
                                if (ret != 0)
                                {
                                    TLOG_ERROR(__FUNCTION__ << ":getMaster error:" << ret << endl);
                                }
                                else
                                {
                                    vtInsert[j].pParam->addUpdateServant(vtInsert[j].iIndex, serverInfo.WCacheServant);
                                }
                            }

                            vtInsert[j].pParam->addFailIndexReason(vtInsert[j].iIndex, ET_KEY_AREA_ERR);
                            if ((--(vtInsert[j].pParam->count)) <= 0)
                            {
                                if (vtInsert[j].pParam->updateServant.mpServant.size() > 0)
                                {
                                    map<string, string> rspContext;
                                    rspContext[ROUTER_UPDATED] = "";
                                    RouterHandle::getInstance()->updateServant2Str(vtInsert[j].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                    vtInsert[j].current->setResponseContext(rspContext);
                                }
                                MKWCache::async_response_insertMKVBatch(vtInsert[j].current, ET_PARTIAL_FAIL, vtInsert[j].pParam->rsp);
                            }
                        }
                        else
                        {
                            //API直连模式，返回增量更新路由
                            map<string, string>& context = vtInsert[j].current->getContext();
                            if (VALUE_YES == context[GET_ROUTE])
                            {
                                RspUpdateServant updateServant;
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                int ret = RouterHandle::getInstance()->getUpdateServant(sMainKey, true, "", updateServant);
                                if (ret != 0)
                                {
                                    TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                                }
                                else
                                {
                                    RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                                    vtInsert[j].current->setResponseContext(rspContext);
                                }
                            }
                            MKWCache::async_response_insertMKV(vtInsert[j].current, ET_KEY_AREA_ERR);
                        }

                    }
                    return;
                }

                MultiHashMap::Value v;
                int iRet;
                if (!vtInsert[i].replace /*&& cFlag == MKDbAccessCallback::SUCC*/)
                {
                    if (g_app.gstat()->isExpireEnabled())
                    {
                        iRet = g_HashMap.get(sMainKey, vtInsert[i].uk, v, true, TC_TimeProvider::getInstance()->getNow());
                    }
                    else
                    {
                        iRet = g_HashMap.get(sMainKey, vtInsert[i].uk, v);
                    }


                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        if (vtInsert[i].bBatch)
                        {
                            vtInsert[i].pParam->addFailIndexReason(vtInsert[i].iIndex, ET_DATA_EXIST);
                            if ((--(vtInsert[i].pParam->count)) <= 0)
                            {
                                if (vtInsert[i].pParam->updateServant.mpServant.size() > 0)
                                {
                                    map<string, string> rspContext;
                                    rspContext[ROUTER_UPDATED] = "";
                                    RouterHandle::getInstance()->updateServant2Str(vtInsert[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                    vtInsert[i].current->setResponseContext(rspContext);
                                }
                                MKWCache::async_response_insertMKVBatch(vtInsert[i].current, ET_PARTIAL_FAIL, vtInsert[i].pParam->rsp);
                            }
                        }
                        else
                        {
                            MKWCache::async_response_insertMKV(vtInsert[i].current, ET_DATA_EXIST);
                        }
                        continue;
                    }
                    else if (iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("MKDbAccessCallback::procInsert g_HashMap.get error, ret = " << iRet << ", mainKey = " << sMainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        if (vtInsert[i].bBatch)
                        {
                            vtInsert[i].pParam->addFailIndexReason(vtInsert[i].iIndex, ET_SYS_ERR);
                            if ((--(vtInsert[i].pParam->count)) <= 0)
                            {
                                if (vtInsert[i].pParam->updateServant.mpServant.size() > 0)
                                {
                                    map<string, string> rspContext;
                                    rspContext[ROUTER_UPDATED] = "";
                                    RouterHandle::getInstance()->updateServant2Str(vtInsert[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                    vtInsert[i].current->setResponseContext(rspContext);
                                }
                                MKWCache::async_response_insertMKVBatch(vtInsert[i].current, ET_PARTIAL_FAIL, vtInsert[i].pParam->rsp);
                            }
                        }
                        else
                        {
                            MKWCache::async_response_insertMKV(vtInsert[i].current, ET_SYS_ERR);
                        }

                        continue;
                    }
                }

                bool dirty = vtInsert[i].dirty;
                if (!dirty)
                {
                    iRet = g_HashMap.checkDirty(sMainKey, vtInsert[i].uk);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_DIRTY_DATA)
                    {
                        dirty = true;
                    }
                }

                iRet = g_HashMap.set(sMainKey, vtInsert[i].uk, vtInsert[i].value, vtInsert[i].expireTime, vtInsert[i].ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::FULL_DATA, _insertAtHead, _updateInOrder);

                if (iRet != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    if (iRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                    {
                        if (vtInsert[i].bBatch)
                        {
                            vtInsert[i].pParam->addFailIndexReason(vtInsert[i].iIndex, ET_DATA_VER_MISMATCH);
                            if ((--(vtInsert[i].pParam->count)) <= 0)
                            {
                                if (vtInsert[i].pParam->updateServant.mpServant.size() > 0)
                                {
                                    map<string, string> rspContext;
                                    rspContext[ROUTER_UPDATED] = "";
                                    RouterHandle::getInstance()->updateServant2Str(vtInsert[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                    vtInsert[i].current->setResponseContext(rspContext);
                                }
                                MKWCache::async_response_insertMKVBatch(vtInsert[i].current, ET_PARTIAL_FAIL, vtInsert[i].pParam->rsp);
                            }
                        }
                        else
                        {
                            MKWCache::async_response_insertMKV(vtInsert[i].current, ET_DATA_VER_MISMATCH);
                        }
                        continue;
                    }
                    else if (iRet == TC_Multi_HashMap_Malloc::RT_NO_MEMORY)
                    {
                        TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << sMainKey << "|" << vtInsert[i].logValue << "|" << (int)vtInsert[i].ver << "|" << vtInsert[i].dirty << "|" << vtInsert[i].replace << "|failed|no memory" << endl);
                        if (vtInsert[i].bBatch)
                        {
                            vtInsert[i].pParam->addFailIndexReason(vtInsert[i].iIndex, ET_MEM_FULL);
                            if ((--(vtInsert[i].pParam->count)) <= 0)
                            {
                                if (vtInsert[i].pParam->updateServant.mpServant.size() > 0)
                                {
                                    map<string, string> rspContext;
                                    rspContext[ROUTER_UPDATED] = "";
                                    RouterHandle::getInstance()->updateServant2Str(vtInsert[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                    vtInsert[i].current->setResponseContext(rspContext);
                                }
                                MKWCache::async_response_insertMKVBatch(vtInsert[i].current, ET_PARTIAL_FAIL, vtInsert[i].pParam->rsp);
                            }
                        }
                        else
                        {
                            MKWCache::async_response_insertMKV(vtInsert[i].current, ET_MEM_FULL);
                        }
                        continue;
                    }
                    else
                    {
                        TLOG_ERROR("MKDbAccessCallback::procInsert g_HashMap.set error, ret = " << iRet << ", mainKey = " << sMainKey << endl);
                        g_app.ppReport(PPReport::SRP_EX, 1);
                        if (vtInsert[i].bBatch)
                        {
                            vtInsert[i].pParam->addFailIndexReason(vtInsert[i].iIndex, ET_SYS_ERR);
                            if ((--(vtInsert[i].pParam->count)) <= 0)
                            {
                                if (vtInsert[i].pParam->updateServant.mpServant.size() > 0)
                                {
                                    map<string, string> rspContext;
                                    rspContext[ROUTER_UPDATED] = "";
                                    RouterHandle::getInstance()->updateServant2Str(vtInsert[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                    vtInsert[i].current->setResponseContext(rspContext);
                                }
                                MKWCache::async_response_insertMKVBatch(vtInsert[i].current, ET_PARTIAL_FAIL, vtInsert[i].pParam->rsp);
                            }
                        }
                        else
                        {
                            MKWCache::async_response_insertMKV(vtInsert[i].current, ET_SYS_ERR);
                        }
                        continue;
                    }
                }
                if (_recordBinLog)
                    WriteBinLog::set(sMainKey, vtInsert[i].uk, vtInsert[i].value, vtInsert[i].expireTime, dirty, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::set(sMainKey, vtInsert[i].uk, _keyBinlogFile);

                if (vtInsert[i].bBatch)
                {
                    if ((--(vtInsert[i].pParam->count)) <= 0)
                    {
                        if (vtInsert[i].pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(vtInsert[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            vtInsert[i].current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_insertMKVBatch(vtInsert[i].current, vtInsert[i].pParam->rsp.rspData.empty() ? ET_SUCC : ET_PARTIAL_FAIL, vtInsert[i].pParam->rsp);
                    }
                }
                else
                    MKWCache::async_response_insertMKV(vtInsert[i].current, ET_SUCC);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procInsert exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procInsert unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtInsert.size(); ++i)
        {
            try
            {
                if (vtInsert[i].bBatch)
                {
                    vtInsert[i].pParam->addFailIndexReason(vtInsert[i].iIndex, ET_SYS_ERR);
                    if ((--(vtInsert[i].pParam->count)) <= 0)
                    {
                        if (vtInsert[i].pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(vtInsert[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            vtInsert[i].current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_insertMKVBatch(vtInsert[i].current, ET_PARTIAL_FAIL, vtInsert[i].pParam->rsp);
                    }
                }
                else
                {
                    if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        MKWCache::async_response_insertMKV(vtInsert[i].current, ET_DB_ERR);
                    }
                    else
                    {
                        MKWCache::async_response_insertMKV(vtInsert[i].current, iRetCode);
                    }
                }
                TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << sMainKey << "|" << vtInsert[i].logValue << "|" << (int)vtInsert[i].ver << "|" << vtInsert[i].dirty << "|" << vtInsert[i].replace << "|failed|async select db error" << endl);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procInsert exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procInsert unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procUpdate(const vector<MKDbAccessCBParam::UpdateCBParam> &vtUpdate, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        int iPageNo = g_route_table.getPageNo(sMainKey);
        for (size_t i = 0; i < vtUpdate.size(); ++i)
        {
            string sLogCond = FormatLog::tostr(vtUpdate[i].vtUKCond, vtUpdate[i].vtValueCond);
            string sLogValue = FormatLog::tostr(vtUpdate[i].mpValue);
            int iUpdateCount = 0;
            try
            {
                if (g_route_table.isTransfering(sMainKey) && g_route_table.isTransSrc(iPageNo, ServerConfig::ServerName))
                {
                    TLOG_ERROR("MKDbAccessCallback::procUpdate: " << sMainKey << " forbid update" << endl);
                    for (size_t j = i; j < vtUpdate.size(); ++j)
                    {
                        MKWCache::async_response_updateMKV(vtUpdate[j].current, ET_FORBID_OPT);
                    }
                    return;
                }

                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procUpdate: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtUpdate.size(); ++j)
                    {
                        map<string, string>& context = vtUpdate[j].current->getContext();
                        //API直连模式，返回增量更新路由
                        if (VALUE_YES == context[GET_ROUTE])
                        {
                            RspUpdateServant updateServant;
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            int ret = RouterHandle::getInstance()->getUpdateServant(sMainKey, true, "", updateServant);
                            if (ret != 0)
                            {
                                TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                            }
                            else
                            {
                                RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                                vtUpdate[j].current->setResponseContext(rspContext);
                            }
                        }

                        MKWCache::async_response_updateMKV(vtUpdate[j].current, ET_KEY_AREA_ERR);
                    }
                    return;
                }

                if (vtUpdate[i].bUKey && !vtUpdate[i].atom)
                {
                    string uk;
                    MultiHashMap::Value v;

                    TarsEncode uKeyEncode;
                    map<string, FieldInfo>::const_iterator itInfo;
                    for (size_t j = 0; j < vtUpdate[i].vtUKCond.size(); j++)
                    {
                        itInfo = fieldConfig.mpFieldInfo.find(vtUpdate[i].vtUKCond[j].fieldName);
                        if (itInfo == fieldConfig.mpFieldInfo.end())
                        {
                            TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|mpFieldInfo find error" << endl);
                            return;
                        }
                        uKeyEncode.write(vtUpdate[i].vtUKCond[j].value, itInfo->second.tag, itInfo->second.type);
                    }
                    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

                    int iRet;
                    if (g_app.gstat()->isExpireEnabled())
                    {
                        iRet = g_HashMap.get(sMainKey, uk, v, true, TC_TimeProvider::getInstance()->getNow());
                    }
                    else
                    {
                        iRet = g_HashMap.get(sMainKey, uk, v);
                    }


                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        TarsDecode vDecode;

                        vDecode.setBuffer(v._value);

                        if (judge(vDecode, vtUpdate[i].vtValueCond))
                        {
                            if (!vtUpdate[i].stLimit.bLimit || (vtUpdate[i].stLimit.bLimit && vtUpdate[i].stLimit.iIndex == 0 && vtUpdate[i].stLimit.iCount >= 1))
                            {
                                string value = updateValue(vtUpdate[i].mpValue, v._value);

                                bool dirty = vtUpdate[i].dirty;
                                if (!dirty)
                                {
                                    iRet = g_HashMap.checkDirty(sMainKey, uk);

                                    if (iRet == TC_Multi_HashMap_Malloc::RT_DIRTY_DATA)
                                    {
                                        dirty = true;
                                    }
                                }

                                int iSetRet = g_HashMap.set(sMainKey, uk, value, vtUpdate[i].expireTime, vtUpdate[i].ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder);

                                if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                                {
                                    if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                                    {
                                        MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_DATA_VER_MISMATCH);
                                        continue;
                                    }
                                    else
                                    {
                                        TLOG_ERROR("MKDbAccessCallback::procUpdate g_HashMap.set error, ret = " << iSetRet << ",mainKey = " << sMainKey << endl);
                                        g_app.ppReport(PPReport::SRP_EX, 1);
                                        MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_SYS_ERR);
                                        continue;
                                    }
                                }
                                iUpdateCount = 1;
                                if (_recordBinLog)
                                    WriteBinLog::set(sMainKey, uk, value, vtUpdate[i].expireTime, dirty, _binlogFile);
                                if (_recordKeyBinLog)
                                    WriteBinLog::set(sMainKey, uk, _keyBinlogFile);
                            }
                        }
                        MKWCache::async_response_updateMKV(vtUpdate[i].current, iUpdateCount);
                        continue;
                    }
                    else if (iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("MKDbAccessCallback::procUpdate g_HashMap.get error, ret = " << iRet << ",mainKey = " << sMainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_SYS_ERR);
                        continue;
                    }
                    else
                    {
                        if (!vtUpdate[i].insert)
                        {
                            MKWCache::async_response_updateMKV(vtUpdate[i].current, 0);
                        }
                        else
                        {
                            if (vtUpdate[i].vtValueCond.size() != 0)
                            {
                                TLOG_ERROR("MKDbAccessCallback::procUpdate data not exist, need insert but cond param error, mainKey = " << sMainKey << endl);
                                MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_INPUT_PARAM_ERROR);
                                continue;
                            }

                            map<string, DCache::UpdateValue> mpJValue;
                            int iRetCode;
                            if (!checkSetValue(vtUpdate[i].mpValue, mpJValue, iRetCode))
                            {
                                TLOG_ERROR("MKDbAccessCallback::procUpdate data not exist, need insert but value param error, mainKey = " << sMainKey << endl);
                                MKWCache::async_response_updateMKV(vtUpdate[i].current, iRetCode);
                                continue;
                            }

                            TarsEncode vEncode;
                            map<string, FieldInfo>::const_iterator itInfo;
                            for (size_t k = 0; k < fieldConfig.vtValueName.size(); ++k)
                            {
                                const string &sValueName = fieldConfig.vtValueName[k];
                                itInfo = fieldConfig.mpFieldInfo.find(sValueName);
                                if (itInfo == fieldConfig.mpFieldInfo.end())
                                {
                                    TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|mpFieldInfo find error" << endl);
                                    return;
                                }
                                vEncode.write(mpJValue[sValueName].value, itInfo->second.tag, itInfo->second.type);
                            }

                            string value;
                            value.assign(vEncode.getBuffer(), vEncode.getLength());

                            int iSetRet = g_HashMap.set(sMainKey, uk, value, vtUpdate[i].expireTime, 1, TC_Multi_HashMap_Malloc::DELETE_FALSE, vtUpdate[i].dirty, TC_Multi_HashMap_Malloc::FULL_DATA, _insertAtHead, _updateInOrder);

                            if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                                {
                                    MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_DATA_VER_MISMATCH);
                                    TLOG_ERROR("MKDbAccessCallback::procUpdate data not exist, need insert but g_HashMap.set ver mismatch, mainKey = " << sMainKey << endl);
                                    continue;
                                }
                                else
                                {
                                    TLOG_ERROR("MKDbAccessCallback::procUpdate data not exist, need insert but g_HashMap.set error, ret = " << iSetRet << ", mainKey = " << sMainKey << endl);
                                    g_app.ppReport(PPReport::SRP_EX, 1);
                                    MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_SYS_ERR);
                                    continue;
                                }

                            }
                            TLOG_DEBUG("procUpdate data not exist, need insert, insert succ, mk = " << sMainKey << endl);
                            iUpdateCount = 1;
                            if (_recordBinLog)
                                WriteBinLog::set(sMainKey, uk, value, vtUpdate[i].expireTime, vtUpdate[i].dirty, _binlogFile);
                            if (_recordKeyBinLog)
                                WriteBinLog::set(sMainKey, uk, _keyBinlogFile);

                            MKWCache::async_response_updateMKV(vtUpdate[i].current, iUpdateCount);
                        }
                    }
                }
                else if (vtUpdate[i].bUKey && vtUpdate[i].atom)
                {
                    string uk;
                    MultiHashMap::Value v;

                    TarsEncode uKeyEncode;
                    map<string, FieldInfo>::const_iterator itInfo;
                    for (size_t j = 0; j < vtUpdate[i].vtUKCond.size(); j++)
                    {
                        itInfo = fieldConfig.mpFieldInfo.find(vtUpdate[i].vtUKCond[j].fieldName);
                        if (itInfo == fieldConfig.mpFieldInfo.end())
                        {
                            TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|mpFieldInfo find error" << endl);
                            return;
                        }
                        uKeyEncode.write(vtUpdate[i].vtUKCond[j].value, itInfo->second.tag, itInfo->second.type);
                    }
                    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

                    int iRet;

                    string value;
                    if (g_app.gstat()->isExpireEnabled())
                    {
                        iRet = g_HashMap.update(sMainKey, uk, vtUpdate[i].mpValue, vtUpdate[i].vtValueCond, reinterpret_cast<const TC_Multi_HashMap_Malloc::FieldConf*>(g_app.gstat()->fieldconfigPtr()), vtUpdate[i].stLimit.bLimit, vtUpdate[i].stLimit.iIndex, vtUpdate[i].stLimit.iCount, value, true, TC_TimeProvider::getInstance()->getNow(), vtUpdate[i].expireTime, vtUpdate[i].dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder);
                    }
                    else
                    {
                        iRet = g_HashMap.update(sMainKey, uk, vtUpdate[i].mpValue, vtUpdate[i].vtValueCond, reinterpret_cast<const TC_Multi_HashMap_Malloc::FieldConf*>(g_app.gstat()->fieldconfigPtr()), vtUpdate[i].stLimit.bLimit, vtUpdate[i].stLimit.iIndex, vtUpdate[i].stLimit.iCount, value, false, 0, vtUpdate[i].expireTime, vtUpdate[i].dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder);
                    }

                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        iUpdateCount = 1;
                        if (_recordBinLog)
                            WriteBinLog::set(sMainKey, uk, value, vtUpdate[i].expireTime, true, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::set(sMainKey, uk, _keyBinlogFile);

                        MKWCache::async_response_updateMKVAtom(vtUpdate[i].current, iUpdateCount);
                        continue;
                    }
                    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        MKWCache::async_response_updateMKVAtom(vtUpdate[i].current, 0);

                        continue;
                    }
                    else
                    {
                        TLOG_ERROR("MKDbAccessCallback::procUpdate g_HashMap.get error, ret = " << iRet << ",mainKey = " << sMainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        MKWCache::async_response_updateMKVAtom(vtUpdate[i].current, ET_SYS_ERR);
                        continue;
                    }
                }
                else if (vtUpdate[i].atom)
                {
                    size_t iDataCount = 0;
                    if (!vtUpdate[i].stLimit.bLimit)
                    {
                        g_HashMap.get(sMainKey, iDataCount);
                        if (iDataCount > _mkeyMaxSelectCount)
                        {
                            FDLOG("MainKeyDataCount") << "[MKDbAccessCallback::procUpdate] mainKey=" << sMainKey << " dataCount=" << iDataCount << endl;
                            TLOG_ERROR("MKDbAccessCallback::procUpdate: g_HashMap.get(mk, iDataCount) error, mainKey = " << sMainKey << " iDataCount = " << iDataCount << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            MKWCache::async_response_updateMKVAtom(vtUpdate[i].current, ET_SYS_ERR);
                            continue;
                        }
                    }

                    vector<MultiHashMap::Value> vtValue;
                    int iRet;
                    if (g_app.gstat()->isExpireEnabled())
                    {
                        iRet = g_HashMap.get(sMainKey, vtValue, true, TC_TimeProvider::getInstance()->getNow());
                    }
                    else
                    {
                        iRet = g_HashMap.get(sMainKey, vtValue);
                    }


                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        int iUpdateRet = updateResultAtom(sMainKey, vtValue, vtUpdate[i].mpValue, vtUpdate[i].vtUKCond, vtUpdate[i].vtValueCond, vtUpdate[i].stLimit, 0, vtUpdate[i].dirty, vtUpdate[i].expireTime, _insertAtHead, _updateInOrder, _binlogFile, _recordBinLog, _recordKeyBinLog, iUpdateCount);
                        if (iUpdateRet != 0)
                        {
                            TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << sMainKey << "|" << sLogValue << "|" << sLogCond << "|" << (int)vtUpdate[i].ver << "|" << vtUpdate[i].dirty << "|" << vtUpdate[i].insert << "|failed|updateResult error" << endl);
                            MKWCache::async_response_updateMKVAtom(vtUpdate[i].current, ET_SYS_ERR);
                            continue;
                        }
                        MKWCache::async_response_updateMKVAtom(vtUpdate[i].current, iUpdateCount);
                    }
                    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        MKWCache::async_response_updateMKVAtom(vtUpdate[i].current, 0);
                    }
                    else
                    {
                        TLOG_ERROR("MKDbAccessCallback::procUpdate g_HashMap.get error, ret = " << iRet << ",mainKey = " << sMainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        MKWCache::async_response_updateMKVAtom(vtUpdate[i].current, ET_SYS_ERR);
                        continue;
                    }
                }
                else
                {
                    size_t iDataCount = 0;
                    if (!vtUpdate[i].stLimit.bLimit)
                    {
                        g_HashMap.get(sMainKey, iDataCount);
                        if (iDataCount > _mkeyMaxSelectCount)
                        {
                            FDLOG("MainKeyDataCount") << "[MKDbAccessCallback::procUpdate] mainKey=" << sMainKey << " dataCount=" << iDataCount << endl;
                            TLOG_ERROR("MKDbAccessCallback::procUpdate: g_HashMap.get(mk, iDataCount) error, mainKey = " << sMainKey << " iDataCount = " << iDataCount << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_SYS_ERR);
                            continue;
                        }
                    }

                    vector<MultiHashMap::Value> vtValue;
                    int iRet;
                    if (g_app.gstat()->isExpireEnabled())
                    {
                        iRet = g_HashMap.get(sMainKey, vtValue, true, TC_TimeProvider::getInstance()->getNow());
                    }
                    else
                    {
                        iRet = g_HashMap.get(sMainKey, vtValue);
                    }


                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        int iUpdateRet = updateResult(sMainKey, vtValue, vtUpdate[i].mpValue, vtUpdate[i].vtUKCond, vtUpdate[i].vtValueCond, vtUpdate[i].stLimit, 0, vtUpdate[i].dirty, vtUpdate[i].expireTime, _insertAtHead, _updateInOrder, _binlogFile, _recordBinLog, _recordKeyBinLog, iUpdateCount);
                        if (iUpdateRet != 0)
                        {
                            TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << sMainKey << "|" << sLogValue << "|" << sLogCond << "|" << (int)vtUpdate[i].ver << "|" << vtUpdate[i].dirty << "|" << vtUpdate[i].insert << "|failed|updateResult error" << endl);
                            MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_SYS_ERR);
                            continue;
                        }
                        MKWCache::async_response_updateMKV(vtUpdate[i].current, iUpdateCount);
                    }
                    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        MKWCache::async_response_updateMKV(vtUpdate[i].current, 0);
                    }
                    else
                    {
                        TLOG_ERROR("MKDbAccessCallback::procUpdate g_HashMap.get error, ret = " << iRet << ",mainKey = " << sMainKey << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_SYS_ERR);
                        continue;
                    }
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdate exception: " << ex.what() << ", mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdate unkown exception, mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtUpdate.size(); i++)
        {
            string sLogCond = FormatLog::tostr(vtUpdate[i].vtUKCond, vtUpdate[i].vtValueCond);
            string sLogValue = FormatLog::tostr(vtUpdate[i].mpValue);
            try
            {
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKWCache::async_response_updateMKV(vtUpdate[i].current, ET_DB_ERR);
                }
                else
                {
                    MKWCache::async_response_updateMKV(vtUpdate[i].current, iRetCode);
                }
                TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << sMainKey << "|" << sLogValue << "|" << sLogCond << "|" << (int)vtUpdate[i].ver << "|" << vtUpdate[i].dirty << "|" << vtUpdate[i].insert << "|failed|async select db error" << endl);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdate exception: " << ex.what() << ", mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdate unkown exception, mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procDel(const vector<MKDbAccessCBParam::DelCBParam> &vtDel, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        int iPageNo = g_route_table.getPageNo(sMainKey);
        for (size_t i = 0; i < vtDel.size(); ++i)
        {
            string sLogCond = FormatLog::tostr(vtDel[i].vtUKCond, vtDel[i].vtValueCond);
            try
            {
                if (g_route_table.isTransfering(sMainKey) && g_route_table.isTransSrc(iPageNo, ServerConfig::ServerName))
                {
                    TLOG_ERROR("MKDbAccessCallback::procDel: " << sMainKey << " forbid del" << endl);
                    for (size_t j = i; j < vtDel.size(); ++j)
                    {
                        MKWCache::async_response_delMKV(vtDel[j].current, ET_FORBID_OPT);
                    }
                    return;
                }

                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procDel: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtDel.size(); ++j)
                    {
                        map<string, string>& context = vtDel[j].current->getContext();
                        //API直连模式，返回增量更新路由
                        if (VALUE_YES == context[GET_ROUTE])
                        {
                            RspUpdateServant updateServant;
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            int ret = RouterHandle::getInstance()->getUpdateServant(sMainKey, true, "", updateServant);
                            if (ret != 0)
                            {
                                TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                            }
                            else
                            {
                                RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                                vtDel[j].current->setResponseContext(rspContext);
                            }
                        }

                        MKWCache::async_response_delMKV(vtDel[j].current, ET_KEY_AREA_ERR);
                    }
                    return;
                }
                if (vtDel[i].bUKey)
                {
                    string uk;
                    MultiHashMap::Value v;

                    TarsEncode uKeyEncode;
                    map<string, FieldInfo>::const_iterator itInfo;
                    for (size_t j = 0; j < vtDel[i].vtUKCond.size(); j++)
                    {
                        itInfo = fieldConfig.mpFieldInfo.find(vtDel[i].vtUKCond[j].fieldName);
                        if (itInfo == fieldConfig.mpFieldInfo.end())
                        {
                            TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|mpFieldInfo find error" << endl);
                            return;
                        }
                        uKeyEncode.write(vtDel[i].vtUKCond[j].value, itInfo->second.tag, itInfo->second.type);
                    }
                    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

                    int iRet = g_HashMap.get(sMainKey, uk, v);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        TarsDecode vDecode;
                        vDecode.setBuffer(v._value);

                        if (judge(vDecode, vtDel[i].vtValueCond))
                        {
                            if (!vtDel[i].stLimit.bLimit || (vtDel[i].stLimit.bLimit && vtDel[i].stLimit.iIndex == 0 && vtDel[i].stLimit.iCount >= 1))
                            {
                                int iDelRet = g_HashMap.delSetBit(sMainKey, uk, time(NULL));

                                if ((iDelRet != TC_Multi_HashMap_Malloc::RT_OK) && (iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                                {
                                    TLOG_ERROR("MKDbAccessCallback::procDel g_HashMap.erase error, ret = " << iDelRet << endl);
                                    g_app.ppReport(PPReport::SRP_EX, 1);
                                    MKWCache::async_response_delMKV(vtDel[i].current, ET_SYS_ERR);
                                    continue;
                                }

                                if (_recordBinLog)
                                    WriteBinLog::del(sMainKey, uk, _binlogFile);
                                if (_recordKeyBinLog)
                                    WriteBinLog::del(sMainKey, uk, _keyBinlogFile);

                                MKWCache::async_response_delMKV(vtDel[i].current, ET_SUCC);
                            }
                        }
                    }
                    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        MKWCache::async_response_delMKV(vtDel[i].current, ET_SUCC);
                    }
                    else
                    {
                        TLOG_ERROR("MKDbAccessCallback::procDel g_HashMap.get error, ret = " << iRet << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        MKWCache::async_response_delMKV(vtDel[i].current, ET_SYS_ERR);
                    }
                }
                else
                {
                    if (TC_Multi_HashMap_Malloc::MainKey::HASH_TYPE == _storeKeyType)
                    {
                        vector<MultiHashMap::Value> vtValue;

                        int iRet = g_HashMap.get(sMainKey, vtValue);

                        if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            int iDelRet = DelResult(sMainKey, vtValue, vtDel[i].vtUKCond, vtDel[i].vtValueCond, vtDel[i].stLimit, _binlogFile, _recordBinLog, _recordKeyBinLog, vtDel[i].current, vtDel[i].pMKDBaccess, true);
                            if (iDelRet != 0)
                            {
                                TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << sMainKey << "|" << sLogCond << "|failed|del cache error, ret = " << iDelRet << endl);
                                MKWCache::async_response_delMKV(vtDel[i].current, ET_SYS_ERR);
                                return;
                            }
                            else
                                MKWCache::async_response_delMKV(vtDel[i].current, ET_SUCC);
                        }
                        else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                        {
                            MKWCache::async_response_delMKV(vtDel[i].current, ET_SUCC);
                        }
                        else
                        {
                            TLOG_ERROR("MKDbAccessCallback::procDel g_HashMap.get error, ret = " << iRet << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            MKWCache::async_response_delMKV(vtDel[i].current, ET_SYS_ERR);
                        }
                    }
                    else if (TC_Multi_HashMap_Malloc::MainKey::ZSET_TYPE == _storeKeyType)
                    {
                        int iRet = g_HashMap.delZSetSetBit(sMainKey, time(NULL));

                        if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                        {

                        }
                        else if ((iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                        {
                        }
                        else
                        {
                            TLOG_ERROR("MKWCacheImp::del: error, ret = " << iRet << ", mainKey = " << sMainKey << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            MKWCache::async_response_delMKV(vtDel[i].current, ET_SYS_ERR);
                        }

                        if (_recordBinLog)
                            WriteBinLog::delZSet(sMainKey, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::delZSet(sMainKey, _keyBinlogFile);

                        MKWCache::async_response_delMKV(vtDel[i].current, ET_SUCC);
                    }
                    else if (TC_Multi_HashMap_Malloc::MainKey::SET_TYPE == _storeKeyType)
                    {
                        int iRet = g_HashMap.delSetSetBit(sMainKey, time(NULL));

                        if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                        {

                        }
                        else if ((iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY) || (iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL))
                        {
                        }
                        else
                        {
                            TLOG_ERROR("MKWCacheImp::del: error, ret = " << iRet << ", mainKey = " << sMainKey << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            MKWCache::async_response_delMKV(vtDel[i].current, ET_SYS_ERR);
                        }

                        if (_recordBinLog)
                            WriteBinLog::delSet(sMainKey, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::delSet(sMainKey, _keyBinlogFile);

                        MKWCache::async_response_delMKV(vtDel[i].current, ET_SUCC);
                    }
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDel exception: " << ex.what() << ", mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDel unkown exception, mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtDel.size(); i++)
        {
            string sLogCond = FormatLog::tostr(vtDel[i].vtUKCond, vtDel[i].vtValueCond);
            try
            {
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKWCache::async_response_delMKV(vtDel[i].current, ET_DB_ERR);
                }
                else
                {
                    MKWCache::async_response_delMKV(vtDel[i].current, iRetCode);
                }
                TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << sMainKey << "|" << sLogCond << "|failed|async select db error" << endl);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDel exception: " << ex.what() << ", mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDel unkown exception, mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}
void MKDbAccessCallback::procDelForBatch(const vector<MKDbAccessCBParam::DelCBParamForBatch> &vtDel, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        int iPageNo = g_route_table.getPageNo(sMainKey);
        for (size_t i = 0; i < vtDel.size(); ++i)
        {
            string sLogCond = FormatLog::tostr(vtDel[i].vtUKCond, vtDel[i].vtValueCond);
            try
            {
                if (g_route_table.isTransfering(sMainKey) && g_route_table.isTransSrc(iPageNo, ServerConfig::ServerName))
                {
                    TLOG_ERROR("MKDbAccessCallback::procDelForBatch: " << vtDel[i].mk << " forbid del" << endl);
                    vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
                    continue;
                }

                if (!g_route_table.isMySelf(vtDel[i].mk))
                {
                    TLOG_ERROR("MKDbAccessCallback::procDelForBatch: " << vtDel[i].mk << " is not in self area" << endl);
                    //API直连模式，返回增量更新路由
                    map<string, string>& context = vtDel[i].pParam->current->getContext();
                    if (VALUE_YES == context[GET_ROUTE])
                    {
                        ServerInfo serverInfo;
                        int ret = g_route_table.getMaster(vtDel[i].mk, serverInfo);
                        if (ret != 0)
                        {
                            TLOG_ERROR(__FUNCTION__ << ":getMaster error:" << ret << endl);
                        }
                        else
                        {
                            vtDel[i].pParam->addUpdateServant(vtDel[i].pParam->mIndex[vtDel[i].mk], serverInfo.WCacheServant);
                        }
                    }

                    vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
                    continue;
                }
                if (vtDel[i].bUKey)
                {
                    string uk;
                    MultiHashMap::Value v;

                    TarsEncode uKeyEncode;
                    map<string, FieldInfo>::const_iterator itInfo;
                    for (size_t j = 0; j < vtDel[i].vtUKCond.size(); j++)
                    {
                        itInfo = fieldConfig.mpFieldInfo.find(vtDel[i].vtUKCond[j].fieldName);
                        if (itInfo == fieldConfig.mpFieldInfo.end())
                        {
                            TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|mpFieldInfo find error" << endl);
                            return;
                        }
                        uKeyEncode.write(vtDel[i].vtUKCond[j].value, itInfo->second.tag, itInfo->second.type);
                    }
                    uk.assign(uKeyEncode.getBuffer(), uKeyEncode.getLength());

                    int iRet = g_HashMap.get(vtDel[i].mk, uk, v);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        TarsDecode vDecode;
                        vDecode.setBuffer(v._value);

                        if (judge(vDecode, vtDel[i].vtValueCond))
                        {
                            if (!vtDel[i].stLimit.bLimit || (vtDel[i].stLimit.bLimit && vtDel[i].stLimit.iIndex == 0 && vtDel[i].stLimit.iCount >= 1))
                            {
                                int iDelRet = g_HashMap.delSetBit(vtDel[i].mk, uk, vtDel[i].ver, time(NULL));

                                if ((iDelRet != TC_Multi_HashMap_Malloc::RT_OK) && (iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                                        && (iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH))
                                {
                                    TLOG_ERROR("MKDbAccessCallback::procDelForBatch g_HashMap.erase error, ret = " << iDelRet << endl);
                                    g_app.ppReport(PPReport::SRP_EX, 1);
                                    vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
                                    continue;
                                }

                                if ((iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH))
                                {
                                    if (_recordBinLog)
                                        WriteBinLog::del(vtDel[i].mk, uk, _binlogFile);
                                    if (_recordKeyBinLog)
                                        WriteBinLog::del(vtDel[i].mk, uk, _keyBinlogFile);


                                    if (iDelRet == TC_Multi_HashMap_Malloc::RT_OK)
                                    {
                                        vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], 1);
                                    }
                                    else
                                        vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], 0);
                                }
                                else
                                {
                                    vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_DATA_VER_MISMATCH);
                                }
                            }
                            else
                                vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], 0);
                        }
                        else
                            vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], 0);
                    }
                    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], 0);
                    }
                    else
                    {
                        TLOG_ERROR("MKDbAccessCallback::procDelForBatch g_HashMap.get error, ret = " << iRet << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
                    }
                }
                else
                {
                    vector<MultiHashMap::Value> vtValue;

                    int iRet = g_HashMap.get(vtDel[i].mk, vtValue);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        int ret = 0;
                        int iDelRet = DelResult(vtDel[i].mk, vtValue, vtDel[i].vtUKCond, vtDel[i].vtValueCond, vtDel[i].stLimit, _binlogFile, _recordBinLog, _recordKeyBinLog, vtDel[i].pMKDBaccess, true, ret);
                        if (iDelRet != 0)
                        {
                            TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << vtDel[i].mk << "|" << sLogCond << "|failed|del cache error, ret = " << iDelRet << endl);
                            vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
                            return;
                        }
                        else
                            vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], ret);
                    }
                    else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], 0);
                    }
                    else
                    {
                        TLOG_ERROR("MKDbAccessCallback::procDelForBatch g_HashMap.get error, ret = " << iRet << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
                    }
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelForBatch exception: " << ex.what() << ", mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelForBatch unkown exception, mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtDel.size(); i++)
        {
            string sLogCond = FormatLog::tostr(vtDel[i].vtUKCond, vtDel[i].vtValueCond);
            try
            {
                vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
                TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << vtDel[i].mk << "|" << sLogCond << "|failed|async select db error" << endl);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelForBatch exception: " << ex.what() << ", mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelForBatch unkown exception, mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                vtDel[i].pParam->addFailIndexReason(vtDel[i].pParam->mIndex[vtDel[i].mk], DEL_ERROR);
            }
        }
    }
}
void MKDbAccessCallback::procUpdateBatch(const vector<MKDbAccessCBParam::UpdateCBParamForBatch> &vtUpdateBatch, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        int iPageNo = g_route_table.getPageNo(sMainKey);
        for (size_t i = 0; i < vtUpdateBatch.size(); ++i)
        {
            try
            {
                if (g_route_table.isTransfering(sMainKey) && g_route_table.isTransSrc(iPageNo, ServerConfig::ServerName))
                {
                    TLOG_ERROR("MKDbAccessCallback::procUpdateBatch: " << sMainKey << " forbid insert" << endl);
                    for (size_t j = i; j < vtUpdateBatch.size(); ++j)
                    {
                        vtUpdateBatch[j].pParam->addFailIndexReason(vtUpdateBatch[j].iIndex, ET_FORBID_OPT);
                        if ((--(vtUpdateBatch[j].pParam->count)) <= 0)
                        {
                            if (vtUpdateBatch[j].pParam->updateServant.mpServant.size() > 0)
                            {
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                RouterHandle::getInstance()->updateServant2Str(vtUpdateBatch[j].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                vtUpdateBatch[j].current->setResponseContext(rspContext);
                            }
                            MKWCache::async_response_updateMKVBatch(vtUpdateBatch[j].current, ET_PARTIAL_FAIL, vtUpdateBatch[j].pParam->rsp);
                        }
                    }
                    return;
                }

                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procUpdateBatch: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtUpdateBatch.size(); ++j)
                    {
                        //API直连模式，返回增量更新路由
                        map<string, string>& context = vtUpdateBatch[j].current->getContext();
                        if (VALUE_YES == context[GET_ROUTE])
                        {
                            ServerInfo serverInfo;
                            int ret = g_route_table.getMaster(sMainKey, serverInfo);
                            if (ret != 0)
                            {
                                TLOG_ERROR(__FUNCTION__ << ":getMaster error:" << ret << endl);
                            }
                            else
                            {
                                vtUpdateBatch[j].pParam->addUpdateServant(vtUpdateBatch[j].iIndex, serverInfo.WCacheServant);
                            }
                        }

                        vtUpdateBatch[j].pParam->addFailIndexReason(vtUpdateBatch[j].iIndex, ET_KEY_AREA_ERR);
                        if ((--(vtUpdateBatch[j].pParam->count)) <= 0)
                        {
                            if (vtUpdateBatch[j].pParam->updateServant.mpServant.size() > 0)
                            {
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                RouterHandle::getInstance()->updateServant2Str(vtUpdateBatch[j].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                vtUpdateBatch[j].current->setResponseContext(rspContext);
                            }
                            MKWCache::async_response_updateMKVBatch(vtUpdateBatch[j].current, ET_PARTIAL_FAIL, vtUpdateBatch[j].pParam->rsp);
                        }
                    }
                    return;
                }

                MultiHashMap::Value v;
                int iRet;
                if (g_app.gstat()->isExpireEnabled())
                {
                    iRet = g_HashMap.get(sMainKey, vtUpdateBatch[i].uk, v, true, TC_TimeProvider::getInstance()->getNow());
                }
                else
                    iRet = g_HashMap.get(sMainKey, vtUpdateBatch[i].uk, v);


                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    bool dirty = vtUpdateBatch[i].dirty;
                    string value;
                    value = updateValue(vtUpdateBatch[i].mpUpdateValue, v._value);

                    if (!vtUpdateBatch[i].dirty)
                    {
                        iRet = g_HashMap.checkDirty(sMainKey, vtUpdateBatch[i].uk);


                        if (iRet == TC_Multi_HashMap_Malloc::RT_DIRTY_DATA)
                        {
                            dirty = true;
                        }
                    }

                    int iSetRet = g_HashMap.set(sMainKey, vtUpdateBatch[i].uk, value, vtUpdateBatch[i].expireTime, vtUpdateBatch[i].ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder);


                    if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                        {
                            TLOG_ERROR("MKCacheImp::update g_HashMap.set ver mismatch" << endl);
                            vtUpdateBatch[i].pParam->addFailIndexReason(vtUpdateBatch[i].iIndex, ET_DATA_VER_MISMATCH);
                        }
                        else
                        {
                            TLOG_ERROR("MKCacheImp::update g_HashMap.set error, ret = " << iSetRet << endl);
                            g_app.ppReport(PPReport::SRP_EX, 1);
                            vtUpdateBatch[i].pParam->addFailIndexReason(vtUpdateBatch[i].iIndex, ET_SYS_ERR);
                        }
                        if ((--(vtUpdateBatch[i].pParam->count)) <= 0)
                        {
                            if (vtUpdateBatch[i].pParam->updateServant.mpServant.size() > 0)
                            {
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                RouterHandle::getInstance()->updateServant2Str(vtUpdateBatch[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                vtUpdateBatch[i].current->setResponseContext(rspContext);
                            }
                            MKWCache::async_response_updateMKVBatch(vtUpdateBatch[i].current, ET_PARTIAL_FAIL, vtUpdateBatch[i].pParam->rsp);
                        }
                        continue;
                    }
                    if (_recordBinLog)
                        WriteBinLog::set(sMainKey, vtUpdateBatch[i].uk, value, vtUpdateBatch[i].expireTime, dirty, _binlogFile);
                    if (_recordKeyBinLog)
                        WriteBinLog::set(sMainKey, vtUpdateBatch[i].uk, _keyBinlogFile);
                    if ((--(vtUpdateBatch[i].pParam->count)) <= 0)
                    {
                        if (vtUpdateBatch[i].pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(vtUpdateBatch[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            vtUpdateBatch[i].current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_updateMKVBatch(vtUpdateBatch[i].current, vtUpdateBatch[i].pParam->rsp.rspData.empty() ? ET_SUCC : ET_PARTIAL_FAIL, vtUpdateBatch[i].pParam->rsp);
                    }
                }
                else if (iRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iRet != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && iRet != TC_Multi_HashMap_Malloc::RT_DATA_EXPIRED && iRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("MKDbAccessCallback::procUpdateBatch g_HashMap.get error, ret = " << iRet << ", mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    vtUpdateBatch[i].pParam->addFailIndexReason(vtUpdateBatch[i].iIndex, ET_SYS_ERR);
                    if ((--(vtUpdateBatch[i].pParam->count)) <= 0)
                    {
                        if (vtUpdateBatch[i].pParam->updateServant.mpServant.size() > 0)
                        {
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            RouterHandle::getInstance()->updateServant2Str(vtUpdateBatch[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                            vtUpdateBatch[i].current->setResponseContext(rspContext);
                        }
                        MKWCache::async_response_updateMKVBatch(vtUpdateBatch[i].current, ET_PARTIAL_FAIL, vtUpdateBatch[i].pParam->rsp);
                    }
                }
                else
                {
                    if (vtUpdateBatch[i].insert)
                    {
                        TarsEncode vEncode;
                        map<string, FieldInfo>::const_iterator itInfo;
                        for (size_t j = 0; j < fieldConfig.vtValueName.size(); ++j)
                        {
                            const string& sValueName = fieldConfig.vtValueName[j];
                            itInfo = fieldConfig.mpFieldInfo.find(sValueName);
                            if (itInfo == fieldConfig.mpFieldInfo.end())
                            {
                                TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|mpFieldInfo find error" << endl);
                                return;
                            }
                            vEncode.write(vtUpdateBatch[i].mpInsertValue.find(sValueName)->second.value, itInfo->second.tag, itInfo->second.type);
                        }

                        string insertValue;
                        insertValue.assign(vEncode.getBuffer(), vEncode.getLength());

                        int iSetRet = g_HashMap.set(sMainKey, vtUpdateBatch[i].uk, insertValue, vtUpdateBatch[i].expireTime, vtUpdateBatch[i].ver, TC_Multi_HashMap_Malloc::DELETE_FALSE, vtUpdateBatch[i].dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, _insertAtHead, _updateInOrder);


                        if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
                            {
                                TLOG_ERROR("MKCacheImp::update g_HashMap.set ver mismatch" << endl);
                                vtUpdateBatch[i].pParam->addFailIndexReason(vtUpdateBatch[i].iIndex, ET_DATA_VER_MISMATCH);
                            }
                            else
                            {
                                TLOG_ERROR("MKCacheImp::update g_HashMap.set error, ret = " << iSetRet << endl);
                                g_app.ppReport(PPReport::SRP_EX, 1);
                                vtUpdateBatch[i].pParam->addFailIndexReason(vtUpdateBatch[i].iIndex, ET_SYS_ERR);
                            }
                            if ((--(vtUpdateBatch[i].pParam->count)) <= 0)
                            {
                                if (vtUpdateBatch[i].pParam->updateServant.mpServant.size() > 0)
                                {
                                    map<string, string> rspContext;
                                    rspContext[ROUTER_UPDATED] = "";
                                    RouterHandle::getInstance()->updateServant2Str(vtUpdateBatch[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                    vtUpdateBatch[i].current->setResponseContext(rspContext);
                                }
                                MKWCache::async_response_updateMKVBatch(vtUpdateBatch[i].current, ET_PARTIAL_FAIL, vtUpdateBatch[i].pParam->rsp);
                            }
                            continue;
                        }
                        if (_recordBinLog)
                            WriteBinLog::set(sMainKey, vtUpdateBatch[i].uk, insertValue, vtUpdateBatch[i].expireTime, vtUpdateBatch[i].dirty, _binlogFile);
                        if (_recordKeyBinLog)
                            WriteBinLog::set(sMainKey, vtUpdateBatch[i].uk, _keyBinlogFile);
                        if ((--(vtUpdateBatch[i].pParam->count)) <= 0)
                        {
                            if (vtUpdateBatch[i].pParam->updateServant.mpServant.size() > 0)
                            {
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                RouterHandle::getInstance()->updateServant2Str(vtUpdateBatch[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                vtUpdateBatch[i].current->setResponseContext(rspContext);
                            }
                            MKWCache::async_response_updateMKVBatch(vtUpdateBatch[i].current, vtUpdateBatch[i].pParam->rsp.rspData.empty() ? ET_SUCC : ET_PARTIAL_FAIL, vtUpdateBatch[i].pParam->rsp);
                        }
                    }
                    else
                    {
                        vtUpdateBatch[i].pParam->addFailIndexReason(vtUpdateBatch[i].iIndex, ET_NO_DATA);
                        if ((--(vtUpdateBatch[i].pParam->count)) <= 0)
                        {
                            if (vtUpdateBatch[i].pParam->updateServant.mpServant.size() > 0)
                            {
                                map<string, string> rspContext;
                                rspContext[ROUTER_UPDATED] = "";
                                RouterHandle::getInstance()->updateServant2Str(vtUpdateBatch[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                                vtUpdateBatch[i].current->setResponseContext(rspContext);
                            }
                            MKWCache::async_response_updateMKVBatch(vtUpdateBatch[i].current, ET_PARTIAL_FAIL, vtUpdateBatch[i].pParam->rsp);
                        }
                    }

                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdateBatch exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdateBatch unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtUpdateBatch.size(); ++i)
        {
            try
            {
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    vtUpdateBatch[i].pParam->addFailIndexReason(vtUpdateBatch[i].iIndex, ET_DB_ERR);
                }
                else
                {
                    vtUpdateBatch[i].pParam->addFailIndexReason(vtUpdateBatch[i].iIndex, iRetCode);
                }
                if ((--(vtUpdateBatch[i].pParam->count)) <= 0)
                {
                    if (vtUpdateBatch[i].pParam->updateServant.mpServant.size() > 0)
                    {
                        map<string, string> rspContext;
                        rspContext[ROUTER_UPDATED] = "";
                        RouterHandle::getInstance()->updateServant2Str(vtUpdateBatch[i].pParam->updateServant, rspContext[ROUTER_UPDATED]);
                        vtUpdateBatch[i].current->setResponseContext(rspContext);
                    }
                    MKWCache::async_response_updateMKVBatch(vtUpdateBatch[i].current, ET_PARTIAL_FAIL, vtUpdateBatch[i].pParam->rsp);
                }
                TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << sMainKey << "|" << vtUpdateBatch[i].logValue << "|" << (int)vtUpdateBatch[i].ver << "|" << vtUpdateBatch[i].dirty << "|" << vtUpdateBatch[i].insert << "|failed|async select db error" << endl);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdateBatch exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdateBatch unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procGetCount(const vector<MKDbAccessCBParam::GetCountCBParam> &m_vtGetCount, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < m_vtGetCount.size(); ++i)
        {
            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetCount: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < m_vtGetCount.size(); ++j)
                    {
                        map<string, string>& context = m_vtGetCount[j].current->getContext();
                        //API直连模式，返回增量更新路由
                        if (VALUE_YES == context[GET_ROUTE])
                        {
                            RspUpdateServant updateServant;
                            map<string, string> rspContext;
                            rspContext[ROUTER_UPDATED] = "";
                            int ret = RouterHandle::getInstance()->getUpdateServant(sMainKey, false, context[API_IDC], updateServant);
                            if (ret != 0)
                            {
                                TLOG_ERROR(__FUNCTION__ << ":getUpdatedRoute error:" << ret << endl);
                            }
                            else
                            {
                                RouterHandle::getInstance()->updateServant2Str(updateServant, rspContext[ROUTER_UPDATED]);
                                m_vtGetCount[j].current->setResponseContext(rspContext);
                            }
                        }
                        MKCache::async_response_getMainKeyCount(m_vtGetCount[j].current, ET_KEY_AREA_ERR);
                    }
                    return;
                }

                MKCache::async_response_getMainKeyCount(m_vtGetCount[i].current, g_HashMap.count(sMainKey));
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetCount exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetCount unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < m_vtGetCount.size(); ++i)
        {
            try
            {
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKCache::async_response_getMainKeyCount(m_vtGetCount[i].current, ET_DB_ERR);
                }
                else
                {
                    MKCache::async_response_getMainKeyCount(m_vtGetCount[i].current, iRetCode);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetCount exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetCount unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procGetSet(const vector<MKDbAccessCBParam::GetSetCBParam> &vtGetSet, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < vtGetSet.size(); ++i)
        {
            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procSelect: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtGetSet.size(); ++j)
                    {
                        DCache::BatchEntry rsp;
                        MKCache::async_response_getSet(vtGetSet[j].current, ET_KEY_AREA_ERR, rsp);
                    }
                    return;
                }

                vector<TC_Multi_HashMap_Malloc::Value> vt;

                int iRet;
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.getSet(sMainKey, TC_TimeProvider::getInstance()->getNow(), vt);
                else
                    iRet = g_HashMap.getSet(sMainKey, 0, vt);

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    DCache::BatchEntry rsp;
                    for (unsigned int j = 0; j < vt.size(); j++)
                    {
                        TarsDecode vDecode;
                        vDecode.setBuffer(vt[j]._value);
                        setResult(vtGetSet[i].vtField, sMainKey, vDecode, 0, 0, rsp.entries);
                    }

                    MKCache::async_response_getSet(vtGetSet[i].current, ET_SUCC, rsp);
                }
                else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    DCache::BatchEntry rsp;
                    MKCache::async_response_getSet(vtGetSet[i].current, ET_SUCC, rsp);
                }
                else
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetSet g_HashMap.getSet return " << iRet << ", mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);

                    DCache::BatchEntry rsp;
                    MKCache::async_response_getSet(vtGetSet[i].current, ET_SYS_ERR, rsp);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtGetSet.size(); ++i)
        {
            try
            {
                DCache::BatchEntry rsp;
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKCache::async_response_getSet(vtGetSet[i].current, ET_DB_ERR, rsp);
                }
                else
                {
                    MKCache::async_response_getSet(vtGetSet[i].current, iRetCode, rsp);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procDelSet(const vector<MKDbAccessCBParam::DelSetCBParam> &vtDelSet, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < vtDelSet.size(); ++i)
        {
            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procDelSet: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtDelSet.size(); ++j)
                    {
                        MKWCache::async_response_delSet(vtDelSet[j].current, ET_KEY_AREA_ERR);
                    }
                    return;
                }

                vector<TC_Multi_HashMap_Malloc::Value> vt;

                int iRet = g_HashMap.delSetSetBit(vtDelSet[i].mk, vtDelSet[i].value, time(NULL));

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK || iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    MKWCache::async_response_delSet(vtDelSet[i].current, ET_SUCC);
                }
                else
                {
                    TLOG_ERROR("MKDbAccessCallback::procDelSet g_HashMap.getSet return " << iRet << ", mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_EX, 1);

                    MKWCache::async_response_delSet(vtDelSet[i].current, ET_SYS_ERR);
                }

                if (_recordBinLog)
                    WriteBinLog::delSet(vtDelSet[i].mk, vtDelSet[i].value, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::delSet(vtDelSet[i].mk, vtDelSet[i].value, _keyBinlogFile);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtDelSet.size(); ++i)
        {
            try
            {
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKWCache::async_response_delSet(vtDelSet[i].current, ET_DB_ERR);
                }
                else
                {
                    MKWCache::async_response_delSet(vtDelSet[i].current, iRetCode);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procGetScoreZSet(const vector<MKDbAccessCBParam::MKDbAccessCBParam::GetScoreZSetCBParam> &vtGetScoreZSet, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < vtGetScoreZSet.size(); ++i)
        {
            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetScoreZSet: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtGetScoreZSet.size(); ++j)
                    {
                        double iScore = 0;
                        MKCache::async_response_getZSetScore(vtGetScoreZSet[j].current, ET_KEY_AREA_ERR, iScore);
                    }
                    return;
                }

                vector<TC_Multi_HashMap_Malloc::Value> vt;

                int iRet;
                double iScore = 0;
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.getScoreZSet(sMainKey, vtGetScoreZSet[i].value, TC_TimeProvider::getInstance()->getNow(), iScore);
                else
                    iRet = g_HashMap.getScoreZSet(sMainKey, vtGetScoreZSet[i].value, 0, iScore);

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKCache::async_response_getZSetScore(vtGetScoreZSet[i].current, ET_SUCC, iScore);
                }
                else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    double iScore = 0;
                    MKCache::async_response_getZSetScore(vtGetScoreZSet[i].current, ET_NO_DATA, iScore);
                }
                else
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetScoreZSet g_HashMap.getSet return " << iRet << ", mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);

                    double iScore = 0;
                    MKCache::async_response_getZSetScore(vtGetScoreZSet[i].current, ET_SYS_ERR, iScore);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetScoreZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetScoreZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtGetScoreZSet.size(); ++i)
        {
            try
            {
                double iScore = 0;
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKCache::async_response_getZSetScore(vtGetScoreZSet[i].current, ET_DB_ERR, iScore);
                }
                else
                {
                    MKCache::async_response_getZSetScore(vtGetScoreZSet[i].current, iRetCode, iScore);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetScoreZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetScoreZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}
void MKDbAccessCallback::procGetRankZSet(const vector<MKDbAccessCBParam::GetRankZSetCBParam> &vtGetRankZSet, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < vtGetRankZSet.size(); ++i)
        {
            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetRankZSet: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtGetRankZSet.size(); ++j)
                    {
                        long iPos = 0;
                        MKCache::async_response_getZSetPos(vtGetRankZSet[j].current, ET_KEY_AREA_ERR, iPos);
                    }
                    return;
                }

                vector<TC_Multi_HashMap_Malloc::Value> vt;

                int iRet;
                long iPos = 0;
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.getRankZSet(sMainKey, vtGetRankZSet[i].value, vtGetRankZSet[i].bOrder, TC_TimeProvider::getInstance()->getNow(), iPos);
                else
                    iRet = g_HashMap.getRankZSet(sMainKey, vtGetRankZSet[i].value, vtGetRankZSet[i].bOrder, 0, iPos);

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKCache::async_response_getZSetPos(vtGetRankZSet[i].current, ET_SUCC, iPos);
                }
                else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    long iPos = 0;
                    MKCache::async_response_getZSetPos(vtGetRankZSet[i].current, ET_NO_DATA, iPos);
                }
                else
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetRankZSet g_HashMap.getSet return " << iRet << ", mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);

                    long iPos = 0;
                    MKCache::async_response_getZSetPos(vtGetRankZSet[i].current, ET_SYS_ERR, iPos);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRankZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRankZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtGetRankZSet.size(); ++i)
        {
            try
            {
                long iPos = 0;
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKCache::async_response_getZSetPos(vtGetRankZSet[i].current, ET_DB_ERR, iPos);
                }
                else
                {
                    MKCache::async_response_getZSetPos(vtGetRankZSet[i].current, iRetCode, iPos);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRankZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRankZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}
void MKDbAccessCallback::procGetRangeZSet(const vector<MKDbAccessCBParam::GetRangeZSetCBParam> &vtGetRangeZSet, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < vtGetRangeZSet.size(); ++i)
        {
            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetRangeZSet: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtGetRangeZSet.size(); ++j)
                    {
                        DCache::BatchEntry rsp;
                        MKCache::async_response_getZSetByPos(vtGetRangeZSet[j].current, ET_KEY_AREA_ERR, rsp);
                    }
                    return;
                }

                int iRet;
                list<TC_Multi_HashMap_Malloc::Value> vt;
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.getZSet(sMainKey, vtGetRangeZSet[i].iStart, vtGetRangeZSet[i].iEnd, vtGetRangeZSet[i].bUp, TC_TimeProvider::getInstance()->getNow(), vt);
                else
                    iRet = g_HashMap.getZSet(sMainKey, vtGetRangeZSet[i].iStart, vtGetRangeZSet[i].iEnd, vtGetRangeZSet[i].bUp, 0, vt);

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK || iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    DCache::BatchEntry rsp;
                    list<TC_Multi_HashMap_Malloc::Value>::iterator it = vt.begin();
                    while (it != vt.end())
                    {
                        TarsDecode vDecode;
                        vDecode.setBuffer((*it)._value);
                        setResult(vtGetRangeZSet[i].vtField, sMainKey, vDecode, 0, 0, (*it)._score, rsp.entries);

                        it++;
                    }
                    MKCache::async_response_getZSetByPos(vtGetRangeZSet[i].current, ET_SUCC, rsp);
                }
                else
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetRangeZSet g_HashMap.getSet return " << iRet << ", mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);

                    DCache::BatchEntry rsp;
                    MKCache::async_response_getZSetByPos(vtGetRangeZSet[i].current, ET_SYS_ERR, rsp);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRangeZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRangeZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtGetRangeZSet.size(); ++i)
        {
            try
            {
                DCache::BatchEntry rsp;
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKCache::async_response_getZSetByPos(vtGetRangeZSet[i].current, ET_DB_ERR, rsp);
                }
                else
                {
                    MKCache::async_response_getZSetByPos(vtGetRangeZSet[i].current, iRetCode, rsp);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRangeZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRangeZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procGetRangeZSetByScore(const vector<MKDbAccessCBParam::GetRangeZSetByScoreCBParam> &vtGetRangeZSetByScore, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < vtGetRangeZSetByScore.size(); ++i)
        {
            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetRangeZSetByScore: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtGetRangeZSetByScore.size(); ++j)
                    {
                        DCache::BatchEntry rsp;
                        MKCache::async_response_getZSetByScore(vtGetRangeZSetByScore[j].current, ET_KEY_AREA_ERR, rsp);
                    }
                    return;
                }

                int iRet;
                list<TC_Multi_HashMap_Malloc::Value> vt;
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.getZSetByScore(sMainKey, vtGetRangeZSetByScore[i].iMin, vtGetRangeZSetByScore[i].iMax, TC_TimeProvider::getInstance()->getNow(), vt);
                else
                    iRet = g_HashMap.getZSetByScore(sMainKey, vtGetRangeZSetByScore[i].iMin, vtGetRangeZSetByScore[i].iMax, 0, vt);

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK || iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    DCache::BatchEntry rsp;
                    list<TC_Multi_HashMap_Malloc::Value>::iterator it = vt.begin();
                    while (it != vt.end())
                    {
                        TarsDecode vDecode;
                        vDecode.setBuffer((*it)._value);
                        setResult(vtGetRangeZSetByScore[i].vtField, sMainKey, vDecode, 0, 0, (*it)._score, rsp.entries);

                        it++;
                    }
                    MKCache::async_response_getZSetByScore(vtGetRangeZSetByScore[i].current, ET_SUCC, rsp);
                }
                else
                {
                    TLOG_ERROR("MKDbAccessCallback::procGetRangeZSetByScore g_HashMap.getSet return " << iRet << ", mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);

                    DCache::BatchEntry rsp;
                    MKCache::async_response_getZSetByScore(vtGetRangeZSetByScore[i].current, ET_SYS_ERR, rsp);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRangeZSetByScore exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRangeZSetByScore unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtGetRangeZSetByScore.size(); ++i)
        {
            try
            {
                DCache::BatchEntry rsp;
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKCache::async_response_getZSetByScore(vtGetRangeZSetByScore[i].current, ET_DB_ERR, rsp);
                }
                else
                {
                    MKCache::async_response_getZSetByScore(vtGetRangeZSetByScore[i].current, iRetCode, rsp);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRangeZSetByScore exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procGetRangeZSetByScore unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procDelZSet(const vector<MKDbAccessCBParam::DelZSetCBParam> &vtDelZSet, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < vtDelZSet.size(); ++i)
        {
            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procDelZSet: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtDelZSet.size(); ++j)
                    {
                        MKWCache::async_response_delZSet(vtDelZSet[j].current, ET_KEY_AREA_ERR);
                    }
                    return;
                }

                vector<TC_Multi_HashMap_Malloc::Value> vt;

                int iRet = g_HashMap.delZSetSetBit(vtDelZSet[i].mk, vtDelZSet[i].value, time(NULL));

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK || iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    MKWCache::async_response_delZSet(vtDelZSet[i].current, ET_SUCC);
                }
                else
                {
                    TLOG_ERROR("MKDbAccessCallback::procDelZSet g_HashMap.getSet return " << iRet << ", mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);

                    MKWCache::async_response_delZSet(vtDelZSet[i].current, ET_SYS_ERR);
                }

                if (_recordBinLog)
                    WriteBinLog::delZSet(vtDelZSet[i].mk, vtDelZSet[i].value, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::delZSet(vtDelZSet[i].mk, vtDelZSet[i].value, _keyBinlogFile);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtDelZSet.size(); ++i)
        {
            try
            {
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKWCache::async_response_delZSet(vtDelZSet[i].current, ET_DB_ERR);
                }
                else
                {
                    MKWCache::async_response_delZSet(vtDelZSet[i].current, iRetCode);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procDelRangeZSet(const vector<MKDbAccessCBParam::DelRangeZSetCBParam> &vtDelRangeZSet, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    if (cFlag == MKDbAccessCallback::SUCC)
    {
        for (size_t i = 0; i < vtDelRangeZSet.size(); ++i)
        {
            try
            {
                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procDelRangeZSet: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtDelRangeZSet.size(); ++j)
                    {
                        MKWCache::async_response_delZSetByScore(vtDelRangeZSet[j].current, ET_KEY_AREA_ERR);
                    }
                    return;
                }

                vector<TC_Multi_HashMap_Malloc::Value> vt;

                int iRet;
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.delRangeZSetSetBit(sMainKey, vtDelRangeZSet[i].iMin, vtDelRangeZSet[i].iMax, TC_TimeProvider::getInstance()->getNow(), time(NULL));
                else
                    iRet = g_HashMap.delRangeZSetSetBit(sMainKey, vtDelRangeZSet[i].iMin, vtDelRangeZSet[i].iMax, 0, time(NULL));

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK || iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    MKWCache::async_response_delZSetByScore(vtDelRangeZSet[i].current, ET_SUCC);
                }
                else
                {
                    TLOG_ERROR("MKDbAccessCallback::procDelRangeZSet g_HashMap.getSet return " << iRet << ", mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);

                    MKWCache::async_response_delZSetByScore(vtDelRangeZSet[i].current, ET_SYS_ERR);
                }

                if (_recordBinLog)
                    WriteBinLog::delRangeZSet(vtDelRangeZSet[i].mk, vtDelRangeZSet[i].iMin, vtDelRangeZSet[i].iMax, _binlogFile);
                if (_recordKeyBinLog)
                    WriteBinLog::delRangeZSet(vtDelRangeZSet[i].mk, vtDelRangeZSet[i].iMin, vtDelRangeZSet[i].iMax, _keyBinlogFile);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelRangeZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelRangeZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtDelRangeZSet.size(); ++i)
        {
            try
            {
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKWCache::async_response_delZSetByScore(vtDelRangeZSet[i].current, ET_DB_ERR);
                }
                else
                {
                    MKWCache::async_response_delZSetByScore(vtDelRangeZSet[i].current, iRetCode);
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelRangeZSet exception: " << ex.what() << ", mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procDelRangeZSet unkown exception, mainKey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}

void MKDbAccessCallback::procUpdateZSet(const vector<MKDbAccessCBParam::UpdateZSetCBParam> &vtUpdateZSet, const char cFlag, const int iRetCode)
{
    string sMainKey = _cbParamPtr->getMainKey();
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();

    if (cFlag == MKDbAccessCallback::SUCC)
    {
        int iPageNo = g_route_table.getPageNo(sMainKey);
        for (size_t i = 0; i < vtUpdateZSet.size(); ++i)
        {
            string sLogValue = FormatLog::tostr(vtUpdateZSet[i].mpValue);
            string sLogCond = FormatLog::tostr(vtUpdateZSet[i].vtValueCond);
            try
            {
                if (g_route_table.isTransfering(sMainKey) && g_route_table.isTransSrc(iPageNo, ServerConfig::ServerName))
                {
                    TLOG_ERROR("MKDbAccessCallback::procUpdateZSet: " << sMainKey << " forbid update" << endl);
                    for (size_t j = i; j < vtUpdateZSet.size(); ++j)
                    {
                        MKWCache::async_response_updateZSet(vtUpdateZSet[j].current, ET_FORBID_OPT);
                    }
                    return;
                }

                if (!g_route_table.isMySelf(sMainKey))
                {
                    TLOG_ERROR("MKDbAccessCallback::procUpdateZSet: " << sMainKey << " is not in self area" << endl);
                    for (size_t j = i; j < vtUpdateZSet.size(); ++j)
                    {
                        MKWCache::async_response_updateZSet(vtUpdateZSet[j].current, ET_KEY_AREA_ERR);
                    }
                    return;
                }

                TarsEncode vEncode;
                map<string, FieldInfo>::const_iterator itInfo;
                for (size_t k = 0; k < fieldConfig.vtValueName.size(); k++)
                {
                    const DCache::Condition &vCond = vtUpdateZSet[i].vtValueCond[k];
                    itInfo = fieldConfig.mpFieldInfo.find(vCond.fieldName);
                    if (itInfo == fieldConfig.mpFieldInfo.end())
                    {
                        TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|mpFieldInfo find error" << endl);
                        return;
                    }
                    vEncode.write(vCond.value, itInfo->second.tag, itInfo->second.type);
                }
                string value;
                value.assign(vEncode.getBuffer(), vEncode.getLength());

                int iRet;
                TC_Multi_HashMap_Malloc::Value vData;
                if (g_app.gstat()->isExpireEnabled())
                    iRet = g_HashMap.getZSet(sMainKey, value, TC_TimeProvider::getInstance()->getNow(), vData);
                else
                    iRet = g_HashMap.getZSet(sMainKey, value, 0, vData);

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    int iUpdateRet = updateResult(sMainKey, value, vtUpdateZSet[i].mpValue, vData._score, vData._iExpireTime, vtUpdateZSet[i].iExpireTime, vtUpdateZSet[i].iVersion, vtUpdateZSet[i].bDirty, vtUpdateZSet[i].bOnlyScore, _binlogFile, _keyBinlogFile, _recordBinLog, _recordKeyBinLog);
                    if (iUpdateRet != 0)
                    {
                        TLOG_ERROR("MKDbAccessCallback::procUpdateZSet g_HashMap.getZSet return " << iRet << ", mainKey = " << sMainKey << endl);
                        MKWCache::async_response_updateZSet(vtUpdateZSet[i].current, ET_SYS_ERR);
                        continue;
                    }

                    MKWCache::async_response_updateZSet(vtUpdateZSet[i].current, ET_SUCC);
                }
                else if (iRet == TC_Multi_HashMap_Malloc::RT_ONLY_KEY || iRet == TC_Multi_HashMap_Malloc::RT_NO_DATA || iRet == TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                {
                    TLOG_ERROR("MKDbAccessCallback::procUpdateZSet: " << "async_response_updateZSet " << sMainKey << " iRet = " << iRet << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    MKWCache::async_response_updateZSet(vtUpdateZSet[i].current, ET_NO_DATA);
                }
                else
                {
                    TLOG_ERROR("MKDbAccessCallback::procUpdate g_HashMap.getZSet error, ret = " << iRet << ",mainKey = " << sMainKey << endl);
                    g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                    MKWCache::async_response_updateZSet(vtUpdateZSet[i].current, ET_SYS_ERR);
                    continue;
                }
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdateZSet exception: " << ex.what() << ", mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                MKWCache::async_response_updateZSet(vtUpdateZSet[i].current, ET_SYS_ERR);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdateZSet unkown exception, mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
                MKWCache::async_response_updateZSet(vtUpdateZSet[i].current, ET_SYS_ERR);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtUpdateZSet.size(); i++)
        {
            string sLogValue = FormatLog::tostr(vtUpdateZSet[i].mpValue);
            string sLogCond = FormatLog::tostr(vtUpdateZSet[i].vtValueCond);
            try
            {
                if (iRetCode == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MKWCache::async_response_updateZSet(vtUpdateZSet[i].current, ET_DB_ERR);
                }
                else
                {
                    MKWCache::async_response_updateZSet(vtUpdateZSet[i].current, iRetCode);
                }
                TLOG_ERROR("MKDbAccessCallback::" << __FUNCTION__ << "|" << sMainKey << "|" << sLogValue << "|" << sLogCond << "|" << (int)vtUpdateZSet[i].iExpireTime << "|" << vtUpdateZSet[i].iVersion << "|" << vtUpdateZSet[i].bDirty << "|failed|async select db error" << endl);
            }
            catch (const std::exception &ex)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdateZSet exception: " << ex.what() << ", mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
            catch (...)
            {
                TLOG_ERROR("MKDbAccessCallback::procUpdateZSet unkown exception, mkey = " << sMainKey << endl);
                g_app.ppReport(PPReport::SRP_EX, 1);
            }
        }
    }
}
