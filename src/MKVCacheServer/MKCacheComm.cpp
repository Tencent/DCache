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
#include "MKCacheComm.h"
#include "TimerThread.h"
#include "MKCacheServer.h"


#define TYPE_BYTE "byte"
#define TYPE_SHORT "short"
#define TYPE_INT "int"
#define TYPE_LONG "long"
#define TYPE_STRING "string"
#define TYPE_FLOAT "float"
#define TYPE_DOUBLE "double"
#define TYPE_UINT32 "unsigned int"
#define TYPE_UINT16 "unsigned short"


bool checkCondition(const vector<DCache::Condition> &vtCond, vector<DCache::Condition> &vtUKCond, vector<DCache::Condition> &vtValueCond, Limit &stLimit, bool &bUKey, int &iRetCode)
{
    size_t iUKeyCount = 0;
    stLimit.bLimit = false;

    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    for (size_t i = 0; i < vtCond.size(); i++)
    {
        const DCache::Condition &tmpCond = vtCond[i];
        if (tmpCond.op == DCache::LIMIT)
        {
            stLimit.bLimit = true;
            vector<string> vt = TC_Common::sepstr<string>(tmpCond.value, ":");
            if (vt.size() != 2)
            {
                TLOG_ERROR("checkCondition: limit value error" << endl);
                iRetCode = ET_PARAM_LIMIT_VALUE_ERR;
                return false;
            }
            stLimit.iIndex = TC_Common::strto<size_t>(TC_Common::trim(vt[0]));
            stLimit.iCount = TC_Common::strto<size_t>(TC_Common::trim(vt[1]));
            continue;
        }

        map<string, int>::const_iterator it = fieldConfig.mpFieldType.find(tmpCond.fieldName);
        if (it == fieldConfig.mpFieldType.end())
        {
            TLOG_ERROR("checkCondition: fileName not found! " << tmpCond.fieldName << endl);
            iRetCode = ET_PARAM_NOT_EXIST;
            return false;
        }

        switch (it->second)
        {
        case 0://主key
            break;
        case 1://联合key
        {
            vtUKCond.push_back(tmpCond);
            if (tmpCond.op == DCache::EQ)
                iUKeyCount++;
            break;
        }
        case 2://value字段
            vtValueCond.push_back(tmpCond);
            break;
        default:
            TLOG_ERROR("checkCondition: field type error! " << it->first << " " << it->second << endl);
            break;
        }
    }
    if (iUKeyCount == fieldConfig.vtUKeyName.size())
        bUKey = true;
    else if (iUKeyCount > fieldConfig.vtUKeyName.size())
    {
        iRetCode = ET_PARAM_REDUNDANT;
        return false;
    }
    else
        bUKey = false;

    if (bUKey)
    {
        if (vtUKCond.size() != fieldConfig.vtUKeyName.size())
        {
            iRetCode = ET_PARAM_REDUNDANT;
            return false;
        }
        else
        {
            for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
            {
                size_t j = i;
                for (; j < vtUKCond.size(); j++)
                {
                    if (fieldConfig.vtUKeyName[i] == vtUKCond[j].fieldName)
                    {
                        if (i != j)
                        {
                            DCache::Condition cond = vtUKCond[i];
                            vtUKCond[i] = vtUKCond[j];
                            vtUKCond[j] = cond;
                        }
                        break;
                    }
                }
                if (j >= vtUKCond.size())
                {
                    iRetCode = ET_PARAM_REDUNDANT;
                    return false;
                }
            }
        }
    }

    return true;
}

bool checkValueCondition(const vector<DCache::Condition>& vtCond, vector<DCache::Condition>& vtValueCond, Limit &stLimit, bool& isLimit, vector<DCache::Condition>& vtScoreCond, int &iRetCode)
{
    // Check valueCond
    stLimit.bLimit = false;
    bool bUnique = false;
    size_t iValueCount = 0;
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    // Check pass in argument
    for (size_t i = 0; i < vtCond.size(); i++)
    {
        const DCache::Condition &tmpCond = vtCond[i];
        if (tmpCond.op == DCache::LIMIT)
        {
            stLimit.bLimit = true;
            isLimit = true;
            vector<string> vt = TC_Common::sepstr<string>(tmpCond.value, ":");
            if (vt.size() != 2)
            {
                TLOG_ERROR("checkCondition: limit value error" << endl);
                iRetCode = ET_PARAM_LIMIT_VALUE_ERR;
                return false;
            }
            stLimit.iIndex = TC_Common::strto<size_t>(TC_Common::trim(vt[0]));
            stLimit.iCount = TC_Common::strto<size_t>(TC_Common::trim(vt[1]));
            TLOG_ERROR("------ Has limit" << endl);
            continue;
        }
        map<string, int>::const_iterator filedExist = fieldConfig.mpFieldType.find(tmpCond.fieldName);
        if ((filedExist == fieldConfig.mpFieldType.end()) && (tmpCond.fieldName != SCOREVALUE))
        {
            bUnique = false;
            TLOG_ERROR("Filed :" << tmpCond.fieldName << " not found!" << endl);
            iRetCode = ET_PARAM_NOT_EXIST;
            return false;
        }
        //处理 score 字段
        else if (tmpCond.fieldName == SCOREVALUE)
        {
            vtScoreCond.push_back(vtCond[i]);
        }
        // Juege op is EQ or not
        else
        {
            if (tmpCond.op != DCache::EQ)
            {
                bUnique = false;
                vtValueCond.push_back(vtCond[i]);
            }
            else
            {
                iValueCount++;
                vtValueCond.push_back(vtCond[i]);
            }
        }
    }

    if (iValueCount == fieldConfig.vtValueName.size())
    {
        bUnique = true;
    }
    else if (iValueCount > fieldConfig.vtValueName.size())
    {
        iRetCode = ET_PARAM_REDUNDANT;
        return false;
    }
    else
        bUnique = false;

    if (bUnique)
    {
        if (vtValueCond.size() != fieldConfig.vtValueName.size())
        {
            iRetCode = ET_PARAM_REDUNDANT;
            return false;
        }
        else
        {
            for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
            {
                size_t j = i;
                for (; j < vtValueCond.size(); j++)
                {
                    if (fieldConfig.vtValueName[i] == vtValueCond[j].fieldName)
                    {
                        if (i != j)
                        {
                            DCache::Condition cond = vtValueCond[i];
                            vtValueCond[i] = vtValueCond[j];
                            vtValueCond[j] = cond;
                        }
                        break;
                    }
                }
                if (j >= vtValueCond.size())
                {
                    iRetCode = ET_PARAM_REDUNDANT;
                    return false;
                }
            }
        }
    }

    return true;
}

bool checkValueCondition(const vector<DCache::Condition> &vtCond, vector<DCache::Condition> &vtValueCond, bool &bUnique, int &iRetCode)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();

    vtValueCond = vtCond;
    if (vtValueCond.size() == fieldConfig.vtValueName.size())
    {
        bUnique = true;
        for (unsigned int i = 0; i < vtValueCond.size(); i++)
        {
            if (vtValueCond[i].op != EQ)
            {
                bUnique = false;
                iRetCode = ET_PARAM_OP_ERR;
                return false;
            }
        }
    }
    else if (vtValueCond.size() > fieldConfig.vtValueName.size())
    {
        iRetCode = ET_PARAM_REDUNDANT;
        return false;
    }
    else
        bUnique = false;

    if (bUnique)
    {
        if (vtValueCond.size() != fieldConfig.vtValueName.size())
        {
            iRetCode = ET_PARAM_REDUNDANT;
            return false;
        }
        else
        {
            for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
            {
                size_t j = i;
                for (; j < vtValueCond.size(); j++)
                {
                    if (fieldConfig.vtValueName[i] == vtValueCond[j].fieldName)
                    {
                        if (i != j)
                        {
                            DCache::Condition cond = vtValueCond[i];
                            vtValueCond[i] = vtValueCond[j];
                            vtValueCond[j] = cond;
                        }
                        break;
                    }
                }
                if (j >= vtValueCond.size())
                {
                    iRetCode = ET_PARAM_REDUNDANT;
                    return false;
                }
            }
        }
    }

    return true;
}

bool checkRecord(const DCache::Record &record, string &sMainKey, vector<DCache::Condition> &vtUKCond, int &iRetCode)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (record.mpRecord.empty())
    {
        TLOG_ERROR(__FUNCTION__ << ":" << __LINE__ << " mpRecord is empty" << endl);
        iRetCode = ET_INPUT_PARAM_ERROR;
        return false;
    }

    sMainKey = record.mainKey;

    if (record.mainKey.empty())
    {
        TLOG_ERROR(__FUNCTION__ << ":" << __LINE__ << " no mainkey field[" << fieldConfig.sMKeyName << "] found" << endl);
        iRetCode = ET_PARAM_MISSING;
        return false;
    }

    vtUKCond.reserve(fieldConfig.vtUKeyName.size());
    for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); ++i)
    {
        const string &sUKey = fieldConfig.vtUKeyName[i];
        TLOG_DEBUG(sUKey << endl);
        map<string, string>::const_iterator it = record.mpRecord.find(sUKey);
        if (record.mpRecord.end() == it)
        {
            TLOG_ERROR(__FUNCTION__ << ":" << __LINE__ << " no ukey field[" << sUKey << "] found" << endl);
            iRetCode = ET_PARAM_MISSING;
            return false;
        }

        Condition con;
        con.fieldName = sUKey;
        con.op = EQ;
        con.value = it->second;
        vtUKCond.push_back(con);
    }

    return true;
}

void  convertFromUKCondition(const vector<Condition> &vtUKeyCond, map<string, string> &mpUKey)
{
    for (size_t i = 0; i < vtUKeyCond.size(); ++i)
    {
        mpUKey[vtUKeyCond[i].fieldName] = vtUKeyCond[i].value;
    }
}

bool checkSetValue(const map<string, DCache::UpdateValue> &mpValue, map<string, DCache::UpdateValue> &mpUK, map<string, DCache::UpdateValue> &mpJValue, int &iRetCode)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    size_t iCout = 0;
    if (mpValue.find(fieldConfig.sMKeyName) != mpValue.end())
        iCout++;
    for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
    {
        const string &sUkey = fieldConfig.vtUKeyName[i];
        map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sUkey);
        if (it == mpValue.end())
        {
            iRetCode = ET_PARAM_MISSING;
            TLOG_ERROR("checkSetValue not find uk: " << sUkey << endl);
            return false;
        }

        if (it->second.op != DCache::SET)
        {
            iRetCode = ET_PARAM_OP_ERR;
            TLOG_ERROR("checkSetValue op not set: " << sUkey << endl);
            return false;
        }

        map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sUkey);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            iRetCode = ET_SYS_ERR;
            TLOG_ERROR("checkSetValue mpFieldInfo find error: " << sUkey << endl);
            return false;
        }

        //防止ukey类型为整形，但业务输入非数字
        if (ConvertDbType(itInfo->second.type) == INT)
        {
            if (!IsDigit(it->second.value))
            {
                iRetCode = ET_PARAM_DIGITAL_ERR;
                TLOG_ERROR("checkSetValue int uk input error: " << it->second.value << endl);
                return false;
            }
        }
        int lengthInDB = itInfo->second.lengthInDB;
        //如果ukey是string类型，并且配置中有限制长度，则比较长度
        if (itInfo->second.type == "string"&&lengthInDB != 0)
        {
            size_t iUkeyLength = it->second.value.size();
            if (int(iUkeyLength) > lengthInDB)
            {
                iRetCode = ET_PARAM_TOO_LONG;
                TLOG_ERROR("checkSetValue string uk:" << sUkey << " length:" << iUkeyLength << " lengthLimit:" << lengthInDB << endl);
                return false;
            }
        }
        mpUK.insert(make_pair(it->first, it->second));
        iCout++;
    }

    for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
    {
        const string &sValue = fieldConfig.vtValueName[i];
        map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sValue);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            iRetCode = ET_SYS_ERR;
            TLOG_ERROR("checkSetValue mpFieldInfo find error: " << sValue << endl);
            return false;
        }

        map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValue);
        if (it == mpValue.end())
        {
            mpJValue[sValue].op = DCache::SET;
            mpJValue[sValue].value = itInfo->second.defValue;
        }
        else
        {
            if (it->second.op != DCache::SET)
            {
                iRetCode = ET_PARAM_OP_ERR;
                TLOG_ERROR("checkSetValue op not set: " << sValue << endl);
                return false;
            }

            //防止value类型为整形，但业务输入非数字
            if (ConvertDbType(itInfo->second.type) == INT)
            {
                if (!IsDigit(it->second.value))
                {
                    iRetCode = ET_PARAM_DIGITAL_ERR;
                    TLOG_ERROR("checkSetValue int value input error: " << it->second.value << endl);
                    return false;
                }
            }
            int lengthInDB = itInfo->second.lengthInDB;
            //如果ukey是string类型，并且配置中有限制长度，则比较长度
            if (itInfo->second.type == "string"&&lengthInDB != 0)
            {
                size_t iVkeyLength = it->second.value.size();
                if (int(iVkeyLength) > lengthInDB)
                {
                    iRetCode = ET_PARAM_TOO_LONG;
                    TLOG_ERROR("checkSetValue string vk:" << sValue << " length:" << iVkeyLength << " lengthLimit:" << lengthInDB << endl);
                    return false;
                }
            }
            mpJValue.insert(make_pair(it->first, it->second));
            iCout++;
        }
    }
    if (mpValue.size() != iCout)
    {
        iRetCode = ET_PARAM_NOT_EXIST;
        TLOG_ERROR("checkSetValue error, mpValue.size = " << mpValue.size() << ", iCout = " << iCout << endl);
        return false;
    }


    return true;
}
bool checkSetValue(const map<string, UpdateFieldInfo> &mpValue, map<string, DCache::UpdateValue> &mpUK, map<string, DCache::UpdateValue> &mpInsertJValue, map<string, DCache::UpdateValue> &mpUpdateJValue, bool insert, int &iRetCode)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    size_t iCout = 0;
    if (mpValue.find(fieldConfig.sMKeyName) != mpValue.end())
        iCout++;
    for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
    {
        const string &sUkeyName = fieldConfig.vtUKeyName[i];
        map<string, DCache::UpdateFieldInfo>::const_iterator it = mpValue.find(sUkeyName);
        if (it == mpValue.end())
        {
            iRetCode = ET_PARAM_MISSING;
            TLOG_ERROR("checkSetValue not find uk: " << sUkeyName << endl);
            return false;
        }

        if (it->second.upDateValue.op != DCache::EQ)
        {
            iRetCode = ET_PARAM_OP_ERR;
            TLOG_ERROR("checkSetValue uk op is not EQ: " << sUkeyName << endl);
            return false;
        }

        map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sUkeyName);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            iRetCode = ET_SYS_ERR;
            TLOG_ERROR(__FUNCTION__ << " mpFieldInfo find error" << endl);
            return false;
        }
        //防止ukey类型为整形，但业务输入非数字
        if (ConvertDbType(itInfo->second.type) == INT)
        {
            if (!IsDigit(it->second.upDateValue.value))
            {
                iRetCode = ET_PARAM_DIGITAL_ERR;
                TLOG_ERROR("checkSetValue int uk input error: " << it->second.upDateValue.value << endl);
                return false;
            }
        }

        int lengthInDB = itInfo->second.lengthInDB;
        //如果ukey是string类型，并且配置中有限制长度，则比较长度
        if (itInfo->second.type == "string"&&lengthInDB != 0)
        {
            size_t iUkeyLength = it->second.upDateValue.value.size();
            if (int(iUkeyLength) > lengthInDB)
            {
                iRetCode = ET_PARAM_TOO_LONG;
                TLOG_ERROR("checkSetValue string uk:" << sUkeyName << " length:" << iUkeyLength << " lengthLimit:" << lengthInDB << endl);
                return false;
            }
        }

        mpUK.insert(make_pair(it->first, it->second.upDateValue));
        iCout++;
    }
    for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
    {
        const string &sValueName = fieldConfig.vtValueName[i];
        map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sValueName);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            iRetCode = ET_SYS_ERR;
            TLOG_ERROR(__FUNCTION__ << " mpFieldInfo find error" << endl);
            return false;
        }

        map<string, DCache::UpdateFieldInfo>::const_iterator it = mpValue.find(sValueName);
        if (it == mpValue.end())
        {
            if (insert)
            {
                mpInsertJValue[sValueName].op = DCache::SET;
                mpInsertJValue[sValueName].value = itInfo->second.defValue;
            }
        }
        else
        {
            //防止value类型为整形，但业务输入非数字
            if (ConvertDbType(itInfo->second.type) == INT)
            {
                if (!IsDigit(it->second.upDateValue.value))
                {
                    iRetCode = ET_PARAM_DIGITAL_ERR;
                    TLOG_ERROR("checkSetValue int value input error: " << it->second.upDateValue.value << endl);
                    return false;
                }
            }

            int lengthInDB = itInfo->second.lengthInDB;
            //如果ukey是string类型，并且配置中有限制长度，则比较长度
            if (itInfo->second.type == "string"&&lengthInDB != 0)
            {
                size_t iVkeyLength = it->second.upDateValue.value.size();
                if (int(iVkeyLength) > lengthInDB)
                {
                    iRetCode = ET_PARAM_TOO_LONG;
                    TLOG_ERROR("checkSetValue string vk:" << sValueName << " length:" << iVkeyLength << " lengthLimit:" << lengthInDB << endl);
                    return false;
                }
            }

            if (insert)
            {
                UpdateValue updateValueTMP;
                updateValueTMP.op = DCache::SET;
                updateValueTMP.value = updateValueForInsert(*it);
                mpInsertJValue[it->first] = updateValueTMP;
            }
            if (it->second.replace)
            {
                mpUpdateJValue[it->first] = it->second.upDateValue;
            }
            iCout++;
        }
    }
    if (mpValue.size() != iCout)
    {
        iRetCode = ET_PARAM_NOT_EXIST;
        TLOG_ERROR("checkSetValue mpValue.size = " << mpValue.size() << ", iCout = " << iCout << endl);
        return false;
    }
    return true;
}
bool checkSetValue(const map<string, DCache::UpdateValue> &mpValue, map<string, DCache::UpdateValue> &mpJValue, int &iRetCode)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    size_t iCout = 0;
    if (mpValue.find(fieldConfig.sMKeyName) != mpValue.end())
        iCout++;

    for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
    {
        const string &sValueName = fieldConfig.vtValueName[i];
        map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sValueName);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            iRetCode = ET_SYS_ERR;
            TLOG_ERROR(__FUNCTION__ << " mpFieldInfo find error" << endl);
            return false;
        }

        map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
        if (it == mpValue.end())
        {
            mpJValue[sValueName].op = DCache::SET;
            mpJValue[sValueName].value = itInfo->second.defValue;
        }
        else
        {
            if (it->second.op != DCache::SET)
            {
                iRetCode = ET_PARAM_OP_ERR;
                TLOG_ERROR("checkSetValue op not SET, field:" << sValueName << endl);
                return false;
            }

            //防止value类型为整形，但业务输入非数字
            if (ConvertDbType(itInfo->second.type) == INT)
            {
                if (!IsDigit(it->second.value))
                {
                    iRetCode = ET_PARAM_DIGITAL_ERR;
                    TLOG_ERROR("checkSetValue int value input error, field:" << sValueName << ", value:" << it->second.value << endl);
                    return false;
                }
            }

            int lengthInDB = itInfo->second.lengthInDB;
            //如果ukey是string类型，并且配置中有限制长度，则比较长度
            if (itInfo->second.type == "string"&&lengthInDB != 0)
            {
                size_t iVkeyLength = it->second.value.size();
                if (int(iVkeyLength) > lengthInDB)
                {
                    iRetCode = ET_PARAM_TOO_LONG;
                    TLOG_ERROR("checkSetValue string vk overlength, field:" << sValueName << " length:" << iVkeyLength << " lengthLimit:" << lengthInDB << endl);
                    return false;
                }
            }

            mpJValue.insert(make_pair(it->first, it->second));
            iCout++;
        }
    }
    if (mpValue.size() != iCout)
    {
        iRetCode = ET_PARAM_NOT_EXIST;
        TLOG_ERROR("checkSetValue mpValue.size = " << mpValue.size() << ", iCout = " << iCout << endl);
        return false;
    }


    return true;
}

bool checkUpdateValue(const map<string, DCache::UpdateValue> &mpValue, int &iRetCode)
{
    if (mpValue.size() == 0)
        return false;
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
    {
        map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(fieldConfig.vtUKeyName[i]);
        if (it != mpValue.end())
        {
            iRetCode = ET_PARAM_UPDATE_UKEY_ERR;
            return false;
        }
    }

    size_t iCout = 0;
    for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
    {
        const string &sValueName = fieldConfig.vtValueName[i];
        map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
        if (it == mpValue.end())
            continue;
        else
        {
            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sValueName);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                iRetCode = ET_SYS_ERR;
                TLOG_ERROR(__FUNCTION__ << " mpFieldInfo find error" << endl);
                return false;
            }

            //防止value类型为整形，但业务输入非数字
            if (ConvertDbType(itInfo->second.type) == INT)
            {
                if (!IsDigit(it->second.value))
                {
                    iRetCode = ET_PARAM_DIGITAL_ERR;
                    TLOG_ERROR("checkSetValue int value input error: " << it->second.value << endl);
                    return false;
                }
            }
            int lengthInDB = itInfo->second.lengthInDB;
            //如果ukey是string类型，并且配置中有限制长度，则比较长度
            if (itInfo->second.type == "string"&&lengthInDB != 0)
            {
                size_t iVkeyLength = it->second.value.size();
                if (int(iVkeyLength) > lengthInDB)
                {
                    iRetCode = ET_PARAM_TOO_LONG;
                    TLOG_ERROR("checkSetValue string vk:" << sValueName << " length:" << iVkeyLength << " lengthLimit:" << lengthInDB << endl);
                    return false;
                }
            }
            iCout++;
        }
    }
    if (mpValue.size() != iCout)
    {
        iRetCode = ET_PARAM_NOT_EXIST;
        return false;
    }

    return true;
}

bool checkUpdateValue(const map<string, DCache::UpdateValue> &mpValue, bool &bOnlyScore, int &iRetCode)
{
    if (mpValue.size() == 0)
    {
        iRetCode = ET_INPUT_PARAM_ERROR;
        return false;
    }

    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    size_t iCout = 0;
    for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
    {
        const string &sValueName = fieldConfig.vtValueName[i];
        map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
        if (it == mpValue.end())
            continue;
        else
        {
            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sValueName);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                iRetCode = ET_SYS_ERR;
                TLOG_ERROR(__FUNCTION__ << " mpFieldInfo find error" << endl);
                return false;
            }

            //防止value类型为整形，但业务输入非数字
            if (ConvertDbType(itInfo->second.type) == INT)
            {
                if (!IsDigit(it->second.value))
                {
                    iRetCode = ET_PARAM_DIGITAL_ERR;
                    TLOG_ERROR("checkSetValue int value input error: " << it->second.value << endl);
                    return false;
                }
            }
            int lengthInDB = itInfo->second.lengthInDB;
            //如果value是string类型，并且配置中有限制长度，则比较长度
            if (itInfo->second.type == "string"&&lengthInDB != 0)
            {
                size_t iValueLength = it->second.value.size();
                if (int(iValueLength) > lengthInDB)
                {
                    iRetCode = ET_PARAM_TOO_LONG;
                    TLOG_ERROR("checkSetValue string vk:" << sValueName << " length:" << iValueLength << " lengthLimit:" << lengthInDB << endl);
                    return false;
                }
            }
            iCout++;
        }
    }
    //处理score字段
    map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(SCOREVALUE);
    if (it != mpValue.end())
    {
        //如果只更新score字段，则设置onlyScore
        if (iCout == 0)
        {
            bOnlyScore = true;
        }
        else
        {
            bOnlyScore = false;
        }
        iCout++;
    }
    if (mpValue.size() != iCout)
    {
        iRetCode = ET_PARAM_NOT_EXIST;
        return false;
    }

    return true;
}

bool checkMK(const string & mkey, bool isInt, int &iRetCode)
{
    //如果主key是数字类型，则检查业务传过来的是否是数字
    if (isInt)
    {
        if (!IsDigit(mkey))
        {
            iRetCode = ET_PARAM_DIGITAL_ERR;
            TLOG_ERROR("checkMK int mk input error: " << mkey << endl);
            return false;
        }
    }
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.sMKeyName);
    if (itInfo == fieldConfig.mpFieldInfo.end())
    {
        iRetCode = ET_SYS_ERR;
        TLOG_ERROR(__FUNCTION__ << " mpFieldInfo find error" << endl);
        return false;
    }

    int lengthInDB = itInfo->second.lengthInDB;
    //如果主key是string类型，并且配置中有限制长度，则比较长度
    if (itInfo->second.type == "string"&&lengthInDB != 0)
    {
        size_t iMkeyLength = mkey.size();
        if (int(iMkeyLength) > lengthInDB)
        {
            iRetCode = ET_PARAM_TOO_LONG;
            TLOG_ERROR("checkMK mk:" << mkey << " length:" << iMkeyLength << " lengthLimit:" << lengthInDB << endl);
            return false;
        }
    }
    return true;
}

void UKBinToDbCondition(const string &sUK, vector<DCache::DbCondition> &vtDbCond)
{
    TarsDecode decode;
    decode.setBuffer(sUK);

    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
    {
        DCache::DbCondition cond;
        cond.fieldName = fieldConfig.vtUKeyName[i];
        cond.op = DCache::EQ;
        map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(cond.fieldName);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            throw MKDCacheException("mpFieldInfo find error");
        }
        cond.value = decode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire);
        cond.type = ConvertDbType(itInfo->second.type);
        vtDbCond.push_back(cond);
    }
}
void ValueBinToDbCondition(const string &sValue, vector<DCache::DbCondition> &vtDbCond)
{
    TarsDecode decode;
    decode.setBuffer(sValue);

    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
    {
        DCache::DbCondition cond;
        cond.fieldName = fieldConfig.vtValueName[i];
        cond.op = DCache::EQ;
        map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(cond.fieldName);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            throw MKDCacheException("mpFieldInfo find error");
        }
        cond.value = decode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire);
        cond.type = ConvertDbType(itInfo->second.type);
        vtDbCond.push_back(cond);
    }
}
void MKUKBinToDbCondition(const string &sMK, const string &sUK, vector<DCache::DbCondition> &vtDbCond)
{
    TarsDecode decode;
    decode.setBuffer(sUK);

    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    DbCondition cond;
    cond.fieldName = fieldConfig.sMKeyName;
    cond.op = DCache::EQ;
    cond.value = sMK;
    map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(cond.fieldName);
    if (itInfo == fieldConfig.mpFieldInfo.end())
    {
        throw MKDCacheException("mpFieldInfo find error");
    }
    cond.type = ConvertDbType(itInfo->second.type);
    vtDbCond.push_back(cond);

    for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
    {
        DCache::DbCondition cond;
        cond.fieldName = fieldConfig.vtUKeyName[i];
        cond.op = DCache::EQ;
        itInfo = fieldConfig.mpFieldInfo.find(cond.fieldName);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            throw MKDCacheException("mpFieldInfo find error");
        }
        cond.value = decode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire);
        cond.type = ConvertDbType(itInfo->second.type);
        vtDbCond.push_back(cond);
    }
}
void BinToDbValue(const string &sMK, const string &sUK, const string &sValue, map<string, DCache::DbUpdateValue> &mpDbValue)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    DbUpdateValue value;

    value.op = DCache::SET;
    value.value = sMK;
    map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.sMKeyName);
    if (itInfo == fieldConfig.mpFieldInfo.end())
    {
        throw MKDCacheException("mpFieldInfo find error");
    }
    value.type = ConvertDbType(itInfo->second.type);
    mpDbValue[fieldConfig.sMKeyName] = value;

    if (sUK.size() > 0)
    {
        TarsDecode ukDecode;
        ukDecode.setBuffer(sUK);
        for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
        {
            value.op = DCache::SET;
            itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.vtUKeyName[i]);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }
            value.value = ukDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire);
            value.type = ConvertDbType(itInfo->second.type);
            mpDbValue[fieldConfig.vtUKeyName[i]] = value;
        }
    }
    TarsDecode vDecode;
    vDecode.setBuffer(sValue);
    for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
    {
        value.op = DCache::SET;
        itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.vtValueName[i]);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            throw MKDCacheException("mpFieldInfo find error");
        }
        value.value = vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire);
        value.type = ConvertDbType(itInfo->second.type);
        mpDbValue[fieldConfig.vtValueName[i]] = value;
    }
}
void BinToDbValue(const string &sMK, const string &sUK, const string &sValue, int sExpireTime, map<string, DCache::DbUpdateValue> &mpDbValue)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    DbUpdateValue value;

    value.op = DCache::SET;
    value.value = sMK;
    map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.sMKeyName);
    if (itInfo == fieldConfig.mpFieldInfo.end())
    {
        throw MKDCacheException("mpFieldInfo find error");
    }
    value.type = ConvertDbType(itInfo->second.type);
    mpDbValue[fieldConfig.sMKeyName] = value;

    value.op = DCache::SET;
    value.value = TC_Common::tostr(sExpireTime);
    value.type = INT;
    mpDbValue["sDCacheExpireTime"] = value;

    if (sUK.size() > 0)
    {
        TarsDecode ukDecode;
        ukDecode.setBuffer(sUK);
        for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
        {
            value.op = DCache::SET;
            itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.vtUKeyName[i]);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }
            value.value = ukDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire);
            value.type = ConvertDbType(itInfo->second.type);
            mpDbValue[fieldConfig.vtUKeyName[i]] = value;
        }
    }
    TarsDecode vDecode;
    vDecode.setBuffer(sValue);
    for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
    {
        value.op = DCache::SET;
        itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.vtValueName[i]);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            throw MKDCacheException("mpFieldInfo find error");
        }
        value.value = vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire);
        value.type = ConvertDbType(itInfo->second.type);
        mpDbValue[fieldConfig.vtValueName[i]] = value;
    }
}


const double EPSILON = 1.00e-07;
//#define FLOAT_EQ(x,v) (((v - EPSILON) <= x) && (x <=( v + EPSILON)))
#define FLOAT_EQ(x,v) (fabs((x) - (v)) <= EPSILON)

bool judgeValue(TarsDecode &decode, const string &value, Op op, const string &type, uint8_t tag, const string &sDefault, bool isRequire)
{
    TarsInputStream<BufferReader> isk;
    isk.setBuffer(decode.getBuffer().c_str(), decode.getBuffer().length());

    if (type == TYPE_BYTE)
    {
        tars::Char n = TC_Common::strto<tars::Char>(sDefault);
        isk.read(n, tag, isRequire);
        tars::Char m = TC_Common::strto<tars::Char>(value);
        if (op == DCache::EQ)
            return n == m;
        else if (op == DCache::NE)
            return n != m;
        else if (op == DCache::GT)
            return n > m;
        else if (op == DCache::LT)
            return n < m;
        else if (op == DCache::LE)
            return n <= m;
        else if (op == DCache::GE)
            return n >= m;
        else
            throw MKDCacheException("judgeValue: op error");
    }
    else if (type == TYPE_SHORT)
    {
        tars::Short n = TC_Common::strto<tars::Short>(sDefault);
        isk.read(n, tag, isRequire);
        tars::Short m = TC_Common::strto<tars::Short>(value);
        if (op == DCache::EQ)
            return n == m;
        else if (op == DCache::NE)
            return n != m;
        else if (op == DCache::GT)
            return n > m;
        else if (op == DCache::LT)
            return n < m;
        else if (op == DCache::LE)
            return n <= m;
        else if (op == DCache::GE)
            return n >= m;
        else
            throw MKDCacheException("judgeValue: op error");
    }
    else if (type == TYPE_INT)
    {
        tars::Int32 n = TC_Common::strto<tars::Int32>(sDefault);
        isk.read(n, tag, isRequire);
        tars::Int32 m = TC_Common::strto<tars::Int32>(value);
        if (op == DCache::EQ)
            return n == m;
        else if (op == DCache::NE)
            return n != m;
        else if (op == DCache::GT)
            return n > m;
        else if (op == DCache::LT)
            return n < m;
        else if (op == DCache::LE)
            return n <= m;
        else if (op == DCache::GE)
            return n >= m;
        else
            throw MKDCacheException("judgeValue: op error");
    }
    else if (type == TYPE_LONG)
    {
        tars::Int64 n = TC_Common::strto<tars::Int64>(sDefault);
        isk.read(n, tag, isRequire);
        tars::Int64 m = TC_Common::strto<tars::Int64>(value);
        if (op == DCache::EQ)
            return n == m;
        else if (op == DCache::NE)
            return n != m;
        else if (op == DCache::GT)
            return n > m;
        else if (op == DCache::LT)
            return n < m;
        else if (op == DCache::LE)
            return n <= m;
        else if (op == DCache::GE)
            return n >= m;
        else
            throw MKDCacheException("judgeValue: op error");
    }
    else if (type == TYPE_STRING)
    {
        string n = sDefault;
        isk.read(n, tag, isRequire);
        string m = value;
        if (op == DCache::EQ)
            return n == m;
        else if (op == DCache::NE)
            return n != m;
        else if (op == DCache::GT)
            return n > m;
        else if (op == DCache::LT)
            return n < m;
        else if (op == DCache::LE)
            return n <= m;
        else if (op == DCache::GE)
            return n >= m;
        else
            throw MKDCacheException("judgeValue: op error");
    }
    else if (type == TYPE_FLOAT)
    {
        tars::Float n = TC_Common::strto<tars::Float>(sDefault);
        isk.read(n, tag, isRequire);
        tars::Float m = TC_Common::strto<tars::Float>(value);
        if (op == DCache::EQ)
            return FLOAT_EQ(n, m);
        else if (op == DCache::NE)
            return !FLOAT_EQ(n, m);
        else if (op == DCache::GT)
            return n > m;
        else if (op == DCache::LT)
            return n < m;
        else if (op == DCache::LE)
            return n < m || FLOAT_EQ(n, m);
        else if (op == DCache::GE)
            return n > m || FLOAT_EQ(n, m);
        else
            throw MKDCacheException("judgeValue: op error");
    }
    else if (type == TYPE_DOUBLE)
    {
        tars::Double n = TC_Common::strto<tars::Double>(sDefault);
        isk.read(n, tag, isRequire);
        tars::Double m = TC_Common::strto<tars::Double>(value);
        if (op == DCache::EQ)
            return FLOAT_EQ(n, m);
        else if (op == DCache::NE)
            return !FLOAT_EQ(n, m);
        else if (op == DCache::GT)
            return n > m;
        else if (op == DCache::LT)
            return n < m;
        else if (op == DCache::LE)
            return n < m || FLOAT_EQ(n, m);
        else if (op == DCache::GE)
            return n > m || FLOAT_EQ(n, m);
        else
            throw MKDCacheException("judgeValue: op error");
    }
    else if (type == TYPE_UINT32)
    {
        tars::UInt32 n = TC_Common::strto<tars::UInt32>(sDefault);
        isk.read(n, tag, isRequire);
        tars::UInt32 m = TC_Common::strto<tars::UInt32>(value);
        if (op == DCache::EQ)
            return n == m;
        else if (op == DCache::NE)
            return n != m;
        else if (op == DCache::GT)
            return n > m;
        else if (op == DCache::LT)
            return n < m;
        else if (op == DCache::LE)
            return n <= m;
        else if (op == DCache::GE)
            return n >= m;
        else
            throw MKDCacheException("judgeValue: op error");
    }
    else if (type == TYPE_UINT16)
    {
        tars::UInt16 n = TC_Common::strto<tars::UInt16>(sDefault);
        isk.read(n, tag, isRequire);
        tars::UInt16 m = TC_Common::strto<tars::UInt16>(value);
        if (op == DCache::EQ)
            return n == m;
        else if (op == DCache::NE)
            return n != m;
        else if (op == DCache::GT)
            return n > m;
        else if (op == DCache::LT)
            return n < m;
        else if (op == DCache::LE)
            return n <= m;
        else if (op == DCache::GE)
            return n >= m;
        else
            throw MKDCacheException("judgeValue: op error");
    }
    else
    {
        throw MKDCacheException("judgeValue: type error");
    }
    return false;
}
string updateValueForInsert(const pair<string, UpdateFieldInfo> &upValue)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(upValue.first);
    if (itInfo == fieldConfig.mpFieldInfo.end())
    {
        throw MKDCacheException("mpFieldInfo find error");
    }
    string type = itInfo->second.type;
    string sDefault = itInfo->second.defValue;
    if (type == TYPE_BYTE)
    {
        tars::Char n = TC_Common::strto<tars::Char>(sDefault);
        tars::Char k;
        tars::Char m = TC_Common::strto<tars::Char>(upValue.second.upDateValue.value);
        if (upValue.second.upDateValue.op == DCache::SET)
            k = m;
        else if (upValue.second.upDateValue.op == DCache::ADD)
            k = n + m;
        else if (upValue.second.upDateValue.op == DCache::SUB)
            k = n - m;
        else
            throw MKDCacheException("updateValue: op error");
        return TC_Common::tostr(k);
    }
    else if (type == TYPE_SHORT)
    {
        tars::Short n = TC_Common::strto<tars::Short>(sDefault);
        tars::Short k;
        tars::Short m = TC_Common::strto<tars::Short>(upValue.second.upDateValue.value);
        if (upValue.second.upDateValue.op == DCache::SET)
            k = m;
        else if (upValue.second.upDateValue.op == DCache::ADD)
            k = n + m;
        else if (upValue.second.upDateValue.op == DCache::SUB)
            k = n - m;
        else
            throw MKDCacheException("updateValue: op error");
        return TC_Common::tostr(k);
    }
    else if (type == TYPE_INT)
    {
        tars::Int32 n = TC_Common::strto<tars::Int32>(sDefault);
        tars::Int32 k;
        tars::Int32 m = TC_Common::strto<tars::Int32>(upValue.second.upDateValue.value);
        if (upValue.second.upDateValue.op == DCache::SET)
            k = m;
        else if (upValue.second.upDateValue.op == DCache::ADD)
            k = n + m;
        else if (upValue.second.upDateValue.op == DCache::SUB)
            k = n - m;
        else
            throw MKDCacheException("updateValue: op error");
        return TC_Common::tostr(k);
    }
    else if (type == TYPE_LONG)
    {
        tars::Int64 n = TC_Common::strto<tars::Int64>(sDefault);
        tars::Int64 k;
        tars::Int64 m = TC_Common::strto<tars::Int64>(upValue.second.upDateValue.value);
        if (upValue.second.upDateValue.op == DCache::SET)
            k = m;
        else if (upValue.second.upDateValue.op == DCache::ADD)
            k = n + m;
        else if (upValue.second.upDateValue.op == DCache::SUB)
            k = n - m;
        else
            throw MKDCacheException("updateValue: op error");
        return TC_Common::tostr(k);
    }
    else if (type == TYPE_STRING)
    {
        string n = sDefault;
        string k;
        string m = upValue.second.upDateValue.value;
        if (upValue.second.upDateValue.op == DCache::SET)
            k = m;
        else if (upValue.second.upDateValue.op == DCache::ADD)
            k = n + m;
        else if (upValue.second.upDateValue.op == DCache::SUB)
            throw MKDCacheException("updateValue: op error");
        else
            throw MKDCacheException("updateValue: op error");
        return TC_Common::tostr(k);
    }
    else if (type == TYPE_FLOAT)
    {
        tars::Float n = TC_Common::strto<tars::Float>(sDefault);
        tars::Float k;
        tars::Float m = TC_Common::strto<tars::Float>(upValue.second.upDateValue.value);
        if (upValue.second.upDateValue.op == DCache::SET)
            k = m;
        else if (upValue.second.upDateValue.op == DCache::ADD)
            k = n + m;
        else if (upValue.second.upDateValue.op == DCache::SUB)
            k = n - m;
        else
            throw MKDCacheException("updateValue: op error");
        return TC_Common::tostr(k);
    }
    else if (type == TYPE_DOUBLE)
    {
        tars::Double n = TC_Common::strto<tars::Double>(sDefault);
        tars::Double k;
        tars::Double m = TC_Common::strto<tars::Double>(upValue.second.upDateValue.value);
        if (upValue.second.upDateValue.op == DCache::SET)
            k = m;
        else if (upValue.second.upDateValue.op == DCache::ADD)
            k = n + m;
        else if (upValue.second.upDateValue.op == DCache::SUB)
            k = n - m;
        else
            throw MKDCacheException("updateValue: op error");
        return TC_Common::tostr(k);
    }
    else if (type == TYPE_UINT32)
    {
        tars::UInt32 n = TC_Common::strto<tars::UInt32>(sDefault);
        tars::UInt32 k;
        tars::UInt32 m = TC_Common::strto<tars::UInt32>(upValue.second.upDateValue.value);
        if (upValue.second.upDateValue.op == DCache::SET)
            k = m;
        else if (upValue.second.upDateValue.op == DCache::ADD)
            k = n + m;
        else if (upValue.second.upDateValue.op == DCache::SUB)
            k = n - m;
        else
            throw MKDCacheException("updateValue: op error");
        return TC_Common::tostr(k);
    }
    else if (type == TYPE_UINT16)
    {
        tars::UInt16 n = TC_Common::strto<tars::UInt16>(sDefault);
        tars::UInt16 k;
        tars::UInt16 m = TC_Common::strto<tars::UInt16>(upValue.second.upDateValue.value);
        if (upValue.second.upDateValue.op == DCache::SET)
            k = m;
        else if (upValue.second.upDateValue.op == DCache::ADD)
            k = n + m;
        else if (upValue.second.upDateValue.op == DCache::SUB)
            k = n - m;
        else
            throw MKDCacheException("updateValue: op error");
        return TC_Common::tostr(k);
    }
    else
    {
        throw MKDCacheException("updateValue: type error");
    }
}
string updateValue(const map<string, DCache::UpdateValue> &mpValue, const string &sStreamValue)
{
    TarsInputStream<BufferReader> isk;
    isk.setBuffer(sStreamValue.c_str(), sStreamValue.length());
    TarsOutputStream<BufferWriter> osk;

    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
    {
        const string &sValueName = fieldConfig.vtValueName[i];
        map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sValueName);
        if (itInfo == fieldConfig.mpFieldInfo.end())
        {
            throw MKDCacheException("mpFieldInfo find error");
        }

        const string &type = itInfo->second.type;
        uint8_t tag = itInfo->second.tag;
        const string &sDefault = itInfo->second.defValue;
        bool isRequire = itInfo->second.bRequire;

        if (type == TYPE_BYTE)
        {
            tars::Char n = TC_Common::strto<tars::Char>(sDefault);
            isk.read(n, tag, isRequire);

            map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
            tars::Char k;
            if (it != mpValue.end())
            {
                tars::Char m = TC_Common::strto<tars::Char>(it->second.value);
                if (it->second.op == DCache::SET)
                    k = m;
                else if (it->second.op == DCache::ADD)
                    k = n + m;
                else if (it->second.op == DCache::SUB)
                    k = n - m;
                else
                    throw MKDCacheException("updateValue: op error");
            }
            else
            {
                k = n;
            }
            osk.write(k, tag);
        }
        else if (type == TYPE_SHORT)
        {
            tars::Short n = TC_Common::strto<tars::Short>(sDefault);
            isk.read(n, tag, isRequire);

            map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
            tars::Short k;
            if (it != mpValue.end())
            {
                tars::Short m = TC_Common::strto<tars::Short>(it->second.value);
                if (it->second.op == DCache::SET)
                    k = m;
                else if (it->second.op == DCache::ADD)
                    k = n + m;
                else if (it->second.op == DCache::SUB)
                    k = n - m;
                else
                    throw MKDCacheException("updateValue: op error");
            }
            else
            {
                k = n;
            }
            osk.write(k, tag);
        }
        else if (type == TYPE_INT)
        {
            tars::Int32 n = TC_Common::strto<tars::Int32>(sDefault);
            isk.read(n, tag, isRequire);

            map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
            tars::Int32 k;
            if (it != mpValue.end())
            {
                tars::Int32 m = TC_Common::strto<tars::Int32>(it->second.value);
                if (it->second.op == DCache::SET)
                    k = m;
                else if (it->second.op == DCache::ADD)
                    k = n + m;
                else if (it->second.op == DCache::SUB)
                    k = n - m;
                else
                    throw MKDCacheException("updateValue: op error");
            }
            else
            {
                k = n;
            }
            osk.write(k, tag);
        }
        else if (type == TYPE_LONG)
        {
            tars::Int64 n = TC_Common::strto<tars::Int64>(sDefault);
            isk.read(n, tag, isRequire);

            map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
            tars::Int64 k;
            if (it != mpValue.end())
            {
                tars::Int64 m = TC_Common::strto<tars::Int64>(it->second.value);
                if (it->second.op == DCache::SET)
                    k = m;
                else if (it->second.op == DCache::ADD)
                    k = n + m;
                else if (it->second.op == DCache::SUB)
                    k = n - m;
                else
                    throw MKDCacheException("updateValue: op error");
            }
            else
            {
                k = n;
            }
            osk.write(k, tag);
        }
        else if (type == TYPE_STRING)
        {
            string n = sDefault;
            isk.read(n, tag, isRequire);
            map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
            string k;
            if (it != mpValue.end())
            {
                string m = it->second.value;
                if (it->second.op == DCache::SET)
                    k = m;
                else if (it->second.op == DCache::ADD)
                    k = n + m;
                else if (it->second.op == DCache::SUB)
                    throw MKDCacheException("updateValue: op error");
                else
                    throw MKDCacheException("updateValue: op error");
            }
            else
            {
                k = n;
            }
            osk.write(k, tag);
        }
        else if (type == TYPE_FLOAT)
        {
            tars::Float n = TC_Common::strto<tars::Float>(sDefault);
            isk.read(n, tag, isRequire);

            map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
            tars::Float k;
            if (it != mpValue.end())
            {
                tars::Float m = TC_Common::strto<tars::Float>(it->second.value);
                if (it->second.op == DCache::SET)
                    k = m;
                else if (it->second.op == DCache::ADD)
                    k = n + m;
                else if (it->second.op == DCache::SUB)
                    k = n - m;
                else
                    throw MKDCacheException("updateValue: op error");
            }
            else
            {
                k = n;
            }
            osk.write(k, tag);
        }
        else if (type == TYPE_DOUBLE)
        {
            tars::Double n = TC_Common::strto<tars::Double>(sDefault);
            isk.read(n, tag, isRequire);

            map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
            tars::Double k;
            if (it != mpValue.end())
            {
                tars::Double m = TC_Common::strto<tars::Double>(it->second.value);
                if (it->second.op == DCache::SET)
                    k = m;
                else if (it->second.op == DCache::ADD)
                    k = n + m;
                else if (it->second.op == DCache::SUB)
                    k = n - m;
                else
                    throw MKDCacheException("updateValue: op error");
            }
            else
            {
                k = n;
            }
            osk.write(k, tag);
        }
        else if (type == TYPE_UINT32)
        {
            tars::UInt32 n = TC_Common::strto<tars::UInt32>(sDefault);
            isk.read(n, tag, isRequire);

            map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
            tars::UInt32 k;
            if (it != mpValue.end())
            {
                tars::UInt32 m = TC_Common::strto<tars::UInt32>(it->second.value);
                if (it->second.op == DCache::SET)
                    k = m;
                else if (it->second.op == DCache::ADD)
                    k = n + m;
                else if (it->second.op == DCache::SUB)
                    k = n - m;
                else
                    throw MKDCacheException("updateValue: op error");
            }
            else
            {
                k = n;
            }
            osk.write(k, tag);
        }
        else if (type == TYPE_UINT16)
        {
            tars::UInt16 n = TC_Common::strto<tars::UInt16>(sDefault);
            isk.read(n, tag, isRequire);

            map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
            tars::UInt16 k;
            if (it != mpValue.end())
            {
                tars::UInt16 m = TC_Common::strto<tars::UInt16>(it->second.value);
                if (it->second.op == DCache::SET)
                    k = m;
                else if (it->second.op == DCache::ADD)
                    k = n + m;
                else if (it->second.op == DCache::SUB)
                    k = n - m;
                else
                    throw MKDCacheException("updateValue: op error");
            }
            else
            {
                k = n;
            }
            osk.write(k, tag);
        }
        else
        {
            throw MKDCacheException("updateValue: type error");
        }

    }
    string s(osk.getBuffer(), osk.getLength());

    return s;
}

tars::Double updateScore(const map<string, DCache::UpdateValue> &mpValue, const tars::Double& dScore)
{
    map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(SCOREVALUE);
    tars::Double newScore;
    if (it != mpValue.end())
    {
        tars::Double m = TC_Common::strto<tars::Double>(it->second.value);
        if (it->second.op == DCache::SET)
            newScore = m;
        else if (it->second.op == DCache::ADD)
            newScore = dScore + m;
        else if (it->second.op == DCache::SUB)
            newScore = dScore - m;
        else
            throw MKDCacheException("updateValue: op error");
    }
    else
    {
        newScore = dScore;
    }

    return newScore;
}

bool judge(TarsDecode &decode, const vector<DCache::Condition> &vtCond)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    for (size_t i = 0; i < vtCond.size(); i++)
    {
        const DCache::Condition &cond = vtCond[i];
        map<string, FieldInfo>::const_iterator it = fieldConfig.mpFieldInfo.find(cond.fieldName);
        if (it == fieldConfig.mpFieldInfo.end())
            throw MKDCacheException("judge: field not found " + vtCond[i].fieldName);

        bool b = judgeValue(decode, cond.value, cond.op, it->second.type, it->second.tag, it->second.defValue, it->second.bRequire);
        if (!b)
            return false;
    }
    return true;
}

bool judge(TarsDecode &ukDecode, TarsDecode &vDecode, const vector<vector<DCache::Condition> > &vtCond)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    for (size_t i = 0; i < vtCond.size(); i++)
    {
        size_t j = 0;
        for (; j < vtCond[i].size(); j++)
        {
            const DCache::Condition &cond = vtCond[i][j];
            map<string, FieldInfo>::const_iterator it = fieldConfig.mpFieldInfo.find(cond.fieldName);
            if (it == fieldConfig.mpFieldInfo.end())
                throw MKDCacheException("judge: field not found " + cond.fieldName);

            map<string, int>::const_iterator itType = fieldConfig.mpFieldType.find(cond.fieldName);
            if (itType == fieldConfig.mpFieldType.end())
                throw MKDCacheException("judge: field type not found " + cond.fieldName);

            bool b = false;
            switch (itType->second)
            {
            case 0://主key
                break;
            case 1://联合key
                b = judgeValue(ukDecode, cond.value, cond.op, it->second.type, it->second.tag, it->second.defValue, it->second.bRequire);
                break;
            case 2://value字段
                b = judgeValue(vDecode, cond.value, cond.op, it->second.type, it->second.tag, it->second.defValue, it->second.bRequire);
                break;
            default:
                throw MKDCacheException("judge: field type error " + itType->first + " " + TC_Common::tostr(itType->second));
                break;
            }
            if (!b)
                break;
        }

        //表示这个条件满足了
        if (j == vtCond[i].size())
        {
            return true;
        }
    }
    return false;
}

bool judgeScore(const tars::Double& dScore, const vector<DCache::Condition> &vtCond)
{
    for (size_t i = 0; i < vtCond.size(); i++)
    {
        bool bRet = true;
        const DCache::Condition &cond = vtCond[i];

        tars::Double m = TC_Common::strto<tars::Double>(cond.value);

        if (cond.op == DCache::EQ)
            bRet = FLOAT_EQ(dScore, m) ? true : false;
        else if (cond.op == DCache::NE)
            bRet = !FLOAT_EQ(dScore, m) ? true : false;
        else if (cond.op == DCache::GT)
            bRet = dScore > m ? true : false;
        else if (cond.op == DCache::LT)
            bRet = dScore < m ? true : false;
        else if (cond.op == DCache::LE)
            bRet = (dScore < m || FLOAT_EQ(dScore, m)) ? true : false;
        else if (cond.op == DCache::GE)
            bRet = (dScore > m || FLOAT_EQ(dScore, m)) ? true : false;
        else
            throw MKDCacheException("judgeScore: op error");

        if (!bRet)
        {
            return false;
        }

    }
    return true;
}

void convertField(const string &sField, vector<string> &vtField)
{
    string sTmpField = TC_Common::trim(sField);
    if (sTmpField == "*")
    {
        return;
    }

    vtField = TC_Common::sepstr<string>(sTmpField, ",");

    //过滤空格
    for (vector<string>::iterator it = vtField.begin(); it != vtField.end(); it++)
        *it = TC_Common::trim(*it);
}

void setResult(const vector<string> &vtField, const string &mainKey, const vector<DCache::Condition> &vtUKCond, TarsDecode &vDecode, const char ver, uint32_t expireTime, vector<map<std::string, std::string> > &vtData)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (vtField.size() == 0)
    {
        //先插入一个空的
        vtData.push_back(map<string, string>());

        map<string, string> &mpValue = vtData[vtData.size() - 1];

        mpValue.insert(map<string, string>::value_type(fieldConfig.sMKeyName, mainKey));
        for (size_t i = 0; i < vtUKCond.size(); i++)
        {
            const DCache::Condition &con = vtUKCond[i];
            mpValue.insert(map<string, string>::value_type(con.fieldName, con.value));
        }
        for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
        {
            const string &valueName = fieldConfig.vtValueName[i];
            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(valueName);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }
            mpValue.insert(map<string, string>::value_type(valueName, vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
        }
        mpValue.insert(map<string, string>::value_type(DVER, TC_Common::tostr(ver)));
        mpValue.insert(map<string, string>::value_type(EXPIRETIME, TC_Common::tostr(expireTime)));
    }
    else
    {
        //先插入一个空的
        vtData.push_back(map<string, string>());

        map<string, string> &mpFieldValue = vtData[vtData.size() - 1];

        map<string, int>::const_iterator fieldEnd = fieldConfig.mpFieldType.end();

        for (size_t i = 0; i < vtField.size(); i++)
        {
            const string &sField = vtField[i];

            map<string, int>::const_iterator it = fieldConfig.mpFieldType.find(sField);

            if (it != fieldEnd)
            {
                map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sField);
                if (itInfo == fieldConfig.mpFieldInfo.end())
                {
                    throw MKDCacheException("mpFieldInfo find error");
                }

                switch (it->second)
                {
                case 0://主key
                    mpFieldValue.insert(map<string, string>::value_type(fieldConfig.sMKeyName, mainKey));
                    break;
                case 1://联合key
                {
                    unsigned int j;
                    unsigned int iukCondSize = vtUKCond.size();
                    for (j = 0; j < iukCondSize; j++)
                        if (vtUKCond[j].fieldName == sField)
                            break;
                    if (j < iukCondSize)
                        mpFieldValue.insert(map<string, string>::value_type(sField, vtUKCond[j].value));
                    else
                        throw MKDCacheException("setResult field not found in condition:" + sField);
                    break;
                }
                case 2://value字段
                    mpFieldValue.insert(map<string, string>::value_type(sField, vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
                    break;
                default:
                    TLOG_ERROR("setResult find field name type error!" << endl);
                }
            }
            else
                throw MKDCacheException("setResult field not found:" + sField);
        }
        mpFieldValue.insert(map<string, string>::value_type(DVER, TC_Common::tostr(ver)));
        mpFieldValue.insert(map<string, string>::value_type(EXPIRETIME, TC_Common::tostr(expireTime)));
    }
}


void setResult(const vector<string> &vtField, const string &mainKey, TarsDecode &ukDecode, TarsDecode &vDecode, const char ver, uint32_t expireTime, vector<map<std::string, std::string> > &vtData)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (vtField.size() == 0)
    {
        //先插入一个空的
        vtData.push_back(map<string, string>());

        map<string, string> &mpValue = vtData[vtData.size() - 1];

        mpValue.insert(map<string, string>::value_type(fieldConfig.sMKeyName, mainKey));
        for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
        {
            const string &uKeyName = fieldConfig.vtUKeyName[i];
            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(uKeyName);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }
            mpValue.insert(map<string, string>::value_type(uKeyName, ukDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
        }
        for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
        {
            const string &valueName = fieldConfig.vtValueName[i];
            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(valueName);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }
            mpValue.insert(map<string, string>::value_type(valueName, vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
        }

        mpValue.insert(map<string, string>::value_type(DVER, TC_Common::tostr(ver)));
        mpValue.insert(map<string, string>::value_type(EXPIRETIME, TC_Common::tostr(expireTime)));
    }
    else
    {
        //先插入一个空的
        vtData.push_back(map<string, string>());

        map<string, string> &mpFieldValue = vtData[vtData.size() - 1];

        map<string, int>::const_iterator fieldEnd = fieldConfig.mpFieldType.end();

        for (size_t i = 0; i < vtField.size(); i++)
        {
            const string &sField = vtField[i];

            map<string, int>::const_iterator it = fieldConfig.mpFieldType.find(sField);

            if (it != fieldEnd)
            {
                map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sField);
                if (itInfo == fieldConfig.mpFieldInfo.end())
                {
                    throw MKDCacheException("mpFieldInfo find error");
                }

                switch (it->second)
                {
                case 0://主key
                    mpFieldValue.insert(map<string, string>::value_type(fieldConfig.sMKeyName, mainKey));
                    break;
                case 1://联合key
                    mpFieldValue.insert(map<string, string>::value_type(sField, ukDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
                    break;
                case 2://value字段
                    mpFieldValue.insert(map<string, string>::value_type(sField, vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
                    break;
                default:
                    TLOG_ERROR("setResult find field name type error!" << endl);
                }
            }
            else
                throw MKDCacheException("setResult field not found:" + sField);
        }
        mpFieldValue.insert(map<string, string>::value_type(DVER, TC_Common::tostr(ver)));
        mpFieldValue.insert(map<string, string>::value_type(EXPIRETIME, TC_Common::tostr(expireTime)));
    }
}

void setResult(const vector<string> &vtField, const string &mainKey, TarsDecode &vDecode, const char ver, uint32_t expireTime, vector<map<std::string, std::string> > &vtData)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (vtField.size() == 0)
    {
        //先插入一个空的
        vtData.push_back(map<string, string>());

        map<string, string> &mpValue = vtData[vtData.size() - 1];

        mpValue.insert(map<string, string>::value_type(fieldConfig.sMKeyName, mainKey));
        for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
        {
            const string &valueName = fieldConfig.vtValueName[i];
            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(valueName);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }
            mpValue.insert(map<string, string>::value_type(valueName, vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
        }

        mpValue.insert(map<string, string>::value_type(DVER, TC_Common::tostr(ver)));
        mpValue.insert(map<string, string>::value_type(EXPIRETIME, TC_Common::tostr(expireTime)));

    }
    else
    {
        //先插入一个空的
        vtData.push_back(map<string, string>());

        map<string, string> &mpFieldValue = vtData[vtData.size() - 1];

        map<string, int>::const_iterator fieldEnd = fieldConfig.mpFieldType.end();

        for (size_t i = 0; i < vtField.size(); i++)
        {
            const string &sField = vtField[i];

            map<string, int>::const_iterator it = fieldConfig.mpFieldType.find(sField);

            if (it != fieldEnd)
            {
                map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sField);
                if (itInfo == fieldConfig.mpFieldInfo.end())
                {
                    throw MKDCacheException("mpFieldInfo find error");
                }

                switch (it->second)
                {
                case 0://主key
                    mpFieldValue.insert(map<string, string>::value_type(fieldConfig.sMKeyName, mainKey));
                    break;
                case 2://value字段
                    mpFieldValue.insert(map<string, string>::value_type(sField, vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
                    break;
                default:
                    TLOG_ERROR("setResult find field name type error!" << endl);
                }
            }
            else
                throw MKDCacheException("setResult field not found:" + sField);
        }
        mpFieldValue.insert(map<string, string>::value_type(DVER, TC_Common::tostr(ver)));
        mpFieldValue.insert(map<string, string>::value_type(EXPIRETIME, TC_Common::tostr(expireTime)));
    }
}

void setResult(const vector<string> &vtField, const string &mainKey, TarsDecode &vDecode, const char ver, uint32_t expireTime, double score, vector<map<std::string, std::string> > &vtData)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    if (vtField.size() == 0)
    {
        //先插入一个空的
        vtData.push_back(map<string, string>());

        map<string, string> &mpValue = vtData[vtData.size() - 1];

        mpValue.insert(map<string, string>::value_type(fieldConfig.sMKeyName, mainKey));
        for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
        {
            const string &valueName = fieldConfig.vtValueName[i];
            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(valueName);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }
            mpValue.insert(map<string, string>::value_type(valueName, vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
        }

        mpValue.insert(map<string, string>::value_type(DVER, TC_Common::tostr(ver)));
        mpValue.insert(map<string, string>::value_type(EXPIRETIME, TC_Common::tostr(expireTime)));

        mpValue.insert(map<string, string>::value_type(SCOREVALUE, TC_Common::tostr(score)));
    }
    else
    {
        //先插入一个空的
        vtData.push_back(map<string, string>());

        map<string, string> &mpFieldValue = vtData[vtData.size() - 1];

        map<string, int>::const_iterator fieldEnd = fieldConfig.mpFieldType.end();

        for (size_t i = 0; i < vtField.size(); i++)
        {
            const string &sField = vtField[i];

            map<string, int>::const_iterator it = fieldConfig.mpFieldType.find(sField);

            if (it != fieldEnd)
            {
                map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(sField);
                if (itInfo == fieldConfig.mpFieldInfo.end())
                {
                    throw MKDCacheException("mpFieldInfo find error");
                }

                switch (it->second)
                {
                case 0://主key
                    mpFieldValue.insert(map<string, string>::value_type(fieldConfig.sMKeyName, mainKey));
                    break;
                case 2://value字段
                    mpFieldValue.insert(map<string, string>::value_type(sField, vDecode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire)));
                    break;
                default:
                    TLOG_ERROR("setResult find field name type error!" << endl);
                }
            }
            else
                throw MKDCacheException("setResult field not found:" + sField);
        }
        mpFieldValue.insert(map<string, string>::value_type(DVER, TC_Common::tostr(ver)));
        mpFieldValue.insert(map<string, string>::value_type(EXPIRETIME, TC_Common::tostr(expireTime)));

        mpFieldValue.insert(map<string, string>::value_type(SCOREVALUE, TC_Common::tostr(score)));
    }
}

string createEncodeStream(const map<string, string> &mpValue, const char type)
{
    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    TarsEncode encode;
    if (type == UK_STREAM)
    {
        for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
        {
            map<string, string>::const_iterator it = mpValue.find(fieldConfig.vtUKeyName[i]);
            if (it == mpValue.end())
                throw MKDCacheException("createEncodeStream field not found:" + fieldConfig.vtUKeyName[i]);

            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.vtUKeyName[i]);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }
            encode.write(it->second, itInfo->second.tag, itInfo->second.type);
        }
    }
    else
    {
        for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
        {
            map<string, string>::const_iterator it = mpValue.find(fieldConfig.vtValueName[i]);
            if (it == mpValue.end())
                throw MKDCacheException("createEncodeStream field not found:" + fieldConfig.vtValueName[i]);

            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.vtValueName[i]);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }
            encode.write(it->second, itInfo->second.tag, itInfo->second.type);
        }
    }
    string s(encode.getBuffer(), encode.getLength());
    return s;
}

enum DCache::DataType ConvertDbType(const string &type)
{
    enum DataType t;
    if (type == TYPE_BYTE)
    {
        t = STR;
    }
    else if (type == TYPE_SHORT)
    {
        t = INT;
    }
    else if (type == TYPE_INT)
    {
        t = INT;
    }
    else if (type == TYPE_LONG)
    {
        t = INT;
    }
    else if (type == TYPE_STRING)
    {
        t = STR;
    }
    else if (type == TYPE_FLOAT)
    {
        t = INT;
    }
    else if (type == TYPE_DOUBLE)
    {
        t = INT;
    }
    else if (type == TYPE_UINT32)
    {
        t = INT;
    }
    else if (type == TYPE_UINT16)
    {
        t = INT;
    }
    else
    {
        throw MKDCacheException("ConvertDbType error");
    }
    return t;
}

void TarsEncode::write(const string &s, uint8_t tag, string type)
{
    if (type == TYPE_BYTE)
    {
        tars::Char n = TC_Common::strto<tars::Char>(s);
        _osk.write(n, tag);
    }
    else if (type == TYPE_SHORT)
    {
        tars::Short n = TC_Common::strto<tars::Short>(s);
        _osk.write(n, tag);
    }
    else if (type == TYPE_INT)
    {
        tars::Int32 n = TC_Common::strto<tars::Int32>(s);
        _osk.write(n, tag);
    }
    else if (type == TYPE_LONG)
    {
        tars::Int64 n = TC_Common::strto<tars::Int64>(s);
        _osk.write(n, tag);
    }
    else if (type == TYPE_STRING)
    {
        string n = TC_Common::strto<string>(s);
        _osk.write(n, tag);
    }
    else if (type == TYPE_FLOAT)
    {
        tars::Float n = TC_Common::strto<tars::Float>(s);
        _osk.write(n, tag);
    }
    else if (type == TYPE_DOUBLE)
    {
        tars::Double n = TC_Common::strto<tars::Double>(s);
        _osk.write(n, tag);
    }
    else if (type == TYPE_UINT32)
    {
        tars::UInt32 n = TC_Common::strto<tars::UInt32>(s);
        _osk.write(n, tag);
    }
    else if (type == TYPE_UINT16)
    {
        tars::UInt16 n = TC_Common::strto<tars::UInt16>(s);
        _osk.write(n, tag);
    }
    else
    {
        throw MKDCacheException("TarsEncode::write type error");
    }
}
void getSelectResult(const vector<string> &vtField, const string &mk, const vector<MultiHashMap::Value> &vtValue, vector<map<std::string, std::string> > &vtData)
{
    //先预先分配空间
    vtData.reserve(vtValue.size());

    size_t iJudgeCount = 0;
    for (size_t i = 0; i < vtValue.size(); i++)
    {
        iJudgeCount++;
        TarsDecode ukDecode;
        TarsDecode vDecode;
        ukDecode.setBuffer(vtValue[i]._ukey);
        vDecode.setBuffer(vtValue[i]._value);
        setResult(vtField, mk, ukDecode, vDecode, vtValue[i]._iVersion, vtValue[i]._iExpireTime, vtData);
    }
    g_app.ppReport(PPReport::SRP_GET_CNT, iJudgeCount);
}
void getSelectResult(const vector<string> &vtField, const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, vector<map<std::string, std::string> > &vtData)
{
    //先预先分配空间
    vtData.reserve(vtValue.size());

    size_t iJudgeCount = 0;
    if (stLimit.bLimit)
    {
        if (vtUKCond.size() == 0 && vtValueCond.size() == 0)
        {
            for (size_t i = 0; i < vtValue.size(); i++)
            {
                iJudgeCount++;
                TarsDecode ukDecode;
                TarsDecode vDecode;

                ukDecode.setBuffer(vtValue[i]._ukey);
                vDecode.setBuffer(vtValue[i]._value);
                setResult(vtField, mk, ukDecode, vDecode, vtValue[i]._iVersion, vtValue[i]._iExpireTime, vtData);
            }
        }
        else
        {
            size_t iIndex = 0;
            size_t iEnd = stLimit.iIndex + stLimit.iCount;
            for (size_t i = 0; i < vtValue.size(); i++)
            {
                iJudgeCount++;
                TarsDecode ukDecode;
                TarsDecode vDecode;

                ukDecode.setBuffer(vtValue[i]._ukey);
                vDecode.setBuffer(vtValue[i]._value);
                if (judge(ukDecode, vtUKCond))
                {
                    if (judge(vDecode, vtValueCond))
                    {
                        if (iIndex >= stLimit.iIndex && iIndex < iEnd)
                        {
                            setResult(vtField, mk, ukDecode, vDecode, vtValue[i]._iVersion, vtValue[i]._iExpireTime, vtData);
                        }
                        iIndex++;
                        if (iIndex >= iEnd)
                            break;
                    }
                }
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtValue.size(); i++)
        {
            iJudgeCount++;
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[i]._ukey);
            vDecode.setBuffer(vtValue[i]._value);
            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    setResult(vtField, mk, ukDecode, vDecode, vtValue[i]._iVersion, vtValue[i]._iExpireTime, vtData);
                }
            }
        }
    }
    g_app.ppReport(PPReport::SRP_GET_CNT, iJudgeCount);
}


void getSelectResult(const vector<string> &vtField, const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<vector<DCache::Condition> > & vtCond, const Limit &stLimit, vector<map<std::string, std::string> > &vtData)
{
    //先预先分配空间
    vtData.reserve(vtValue.size());

    size_t iJudgeCount = 0;
    if (stLimit.bLimit)
    {
        if (vtCond.size() == 0)
        {
            for (size_t i = 0; i < vtValue.size(); i++)
            {
                iJudgeCount++;
                TarsDecode ukDecode;
                TarsDecode vDecode;

                ukDecode.setBuffer(vtValue[i]._ukey);
                vDecode.setBuffer(vtValue[i]._value);
                setResult(vtField, mk, ukDecode, vDecode, vtValue[i]._iVersion, vtValue[i]._iExpireTime, vtData);
            }
        }
        else
        {
            size_t iIndex = 0;
            size_t iEnd = stLimit.iIndex + stLimit.iCount;
            for (size_t i = 0; i < vtValue.size(); i++)
            {
                iJudgeCount++;
                TarsDecode ukDecode;
                TarsDecode vDecode;

                ukDecode.setBuffer(vtValue[i]._ukey);
                vDecode.setBuffer(vtValue[i]._value);
                if (judge(ukDecode, vDecode, vtCond))
                {
                    if (iIndex >= stLimit.iIndex && iIndex < iEnd)
                    {
                        setResult(vtField, mk, ukDecode, vDecode, vtValue[i]._iVersion, vtValue[i]._iExpireTime, vtData);
                    }
                    iIndex++;
                    if (iIndex >= iEnd)
                        break;
                }
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtValue.size(); i++)
        {
            iJudgeCount++;
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[i]._ukey);
            vDecode.setBuffer(vtValue[i]._value);
            if (judge(ukDecode, vDecode, vtCond))
            {
                setResult(vtField, mk, ukDecode, vDecode, vtValue[i]._iVersion, vtValue[i]._iExpireTime, vtData);
            }
        }
    }
    g_app.ppReport(PPReport::SRP_GET_CNT, iJudgeCount);
}

int getSelectResult(const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, vector<char> &vtData)
{
    size_t iJudgeCount = 0;
    TarsOutputStream<BufferWriter> osk;
    TarsOutputStream<BufferWriter> osk2;
    Int32 iCount = 0;
    if (stLimit.bLimit)
    {
        if (vtUKCond.size() == 0 && vtValueCond.size() == 0)
        {
            for (size_t i = 0; i < vtValue.size(); i++)
            {
                iJudgeCount++;
                DataHead h2(DataHead::eStructBegin, 0);
                h2.writeTo(osk);

                osk.write(vtValue[i]._mkey, 0);
                osk.write(vtValue[i]._ukey, 1);
                osk.write(vtValue[i]._value, 2);
                osk.write((char)vtValue[i]._iVersion, 3);
                osk.write(vtValue[i]._iExpireTime, 4);

                h2.setType(DataHead::eStructEnd);
                h2.setTag(0);
                h2.writeTo(osk);
                iCount++;
            }
        }
        else
        {
            size_t iIndex = 0;
            size_t iEnd = stLimit.iIndex + stLimit.iCount;

            for (size_t i = 0; i < vtValue.size(); i++)
            {
                iJudgeCount++;
                TarsDecode ukDecode;
                TarsDecode vDecode;

                ukDecode.setBuffer(vtValue[i]._ukey);
                vDecode.setBuffer(vtValue[i]._value);
                if (judge(ukDecode, vtUKCond))
                {
                    if (judge(vDecode, vtValueCond))
                    {
                        if (iIndex >= stLimit.iIndex && iIndex < iEnd)
                        {
                            DataHead h2(DataHead::eStructBegin, 0);
                            h2.writeTo(osk);

                            osk.write(vtValue[i]._mkey, 0);
                            osk.write(vtValue[i]._ukey, 1);
                            osk.write(vtValue[i]._value, 2);
                            osk.write((char)vtValue[i]._iVersion, 3);
                            osk.write(vtValue[i]._iExpireTime, 4);

                            h2.setType(DataHead::eStructEnd);
                            h2.setTag(0);
                            h2.writeTo(osk);
                            iCount++;
                        }
                        iIndex++;
                        if (iIndex >= iEnd)
                            break;
                    }
                }
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtValue.size(); i++)
        {
            TarsDecode ukDecode;
            TarsDecode vDecode;
            iJudgeCount++;
            ukDecode.setBuffer(vtValue[i]._ukey);
            vDecode.setBuffer(vtValue[i]._value);
            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    DataHead h2(DataHead::eStructBegin, 0);
                    h2.writeTo(osk);

                    osk.write(vtValue[i]._mkey, 0);
                    osk.write(vtValue[i]._ukey, 1);
                    osk.write(vtValue[i]._value, 2);
                    osk.write((char)vtValue[i]._iVersion, 3);
                    osk.write(vtValue[i]._iExpireTime, 4);

                    h2.setType(DataHead::eStructEnd);
                    h2.setTag(0);
                    h2.writeTo(osk);
                    iCount++;
                }
            }
        }
    }

    DataHead h(DataHead::eList, 0);
    h.writeTo(osk2);
    osk2.write(iCount, 0);

    vtData.reserve(osk.getLength() + osk2.getLength());
    vtData.assign(osk2.getBuffer(), osk2.getBuffer() + osk2.getLength());
    vtData.insert(vtData.end(), osk.getBuffer(), osk.getBuffer() + osk.getLength());

    g_app.ppReport(PPReport::SRP_GET_CNT, iJudgeCount);

    return iCount;
}

int updateResult(const string &mk, const string &sOldValue, const map<std::string, DCache::UpdateValue> &mpValue, double iScore, uint32_t iOldExpireTime, uint32_t iExpireTime, char iVersion, bool bDirty, bool bOnlyScore, const string sBinLogFile, const string sKeyBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog)
{
    string sNewValue = updateValue(mpValue, sOldValue);
    double score = updateScore(mpValue, iScore);

    TLOG_DEBUG("MKWCacheImp::updateZSet: g_HashMap.updateZSet, mainkey = " << mk << endl);

    int iUpdateRet = g_HashMap.updateZSet(mk, sOldValue, sNewValue, score, iExpireTime, iVersion, bDirty, bOnlyScore);
    if (iUpdateRet != TC_Multi_HashMap_Malloc::RT_OK)
    {
        TLOG_ERROR("g_HashMap.updateZSet error, ret = " << iUpdateRet << ", mainKey = " << mk << endl);
        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
        return -1;
    }
    if (bRecordBinLog)
        WriteBinLog::updateZSet(mk, sOldValue, sNewValue, score, iOldExpireTime, iExpireTime, bDirty, sBinLogFile);
    if (bKeyRecordBinLog)
        WriteBinLog::updateZSet(mk, sOldValue, sNewValue, score, iOldExpireTime, iExpireTime, bDirty, sKeyBinLogFile);

    return 0;
}

int updateResult(const string &mk, const vector<MultiHashMap::Value> &vtValue, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, char ver, bool dirty, uint32_t expireTimeSecond, bool bHead, bool bUpdateOrder, const string sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog, int &iUpdateCount)
{
    size_t iJudgeCount = 0;
    vector<MultiHashMap::Value> vtV;
    vtV.reserve(vtValue.size());
    if (stLimit.bLimit)
    {
        size_t iIndex = 0;
        size_t iEnd = stLimit.iIndex + stLimit.iCount;
        for (size_t j = 0; j < vtValue.size(); j++)
        {
            iJudgeCount++;
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[j]._ukey);
            vDecode.setBuffer(vtValue[j]._value);

            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    if (iIndex >= stLimit.iIndex && iIndex < iEnd)
                    {
                        string value = updateValue(mpValue, vtValue[j]._value);

                        bool bDirty = dirty;
                        if (!bDirty)
                        {
                            int iRet = g_HashMap.checkDirty(mk, vtValue[j]._ukey);

                            if (iRet == TC_Multi_HashMap_Malloc::RT_DIRTY_DATA)
                            {
                                bDirty = true;
                            }
                        }

                        MultiHashMap::Value v;
                        v._mkey = mk;
                        v._ukey = vtValue[j]._ukey;
                        v._value = value;
                        v._iVersion = ver;
                        v._dirty = bDirty;
                        v._iExpireTime = expireTimeSecond;
                        v._isDelete = TC_Multi_HashMap_Malloc::DELETE_FALSE;

                        vtV.push_back(v);
                    }
                    iIndex++;
                    if (iIndex >= iEnd)
                        break;
                }
            }
        }
    }
    else
    {
        for (size_t j = 0; j < vtValue.size(); j++)
        {
            iJudgeCount++;
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[j]._ukey);
            vDecode.setBuffer(vtValue[j]._value);

            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    string value = updateValue(mpValue, vtValue[j]._value);

                    bool bDirty = dirty;
                    if (!bDirty)
                    {
                        int iRet = g_HashMap.checkDirty(mk, vtValue[j]._ukey);

                        if (iRet == TC_Multi_HashMap_Malloc::RT_DIRTY_DATA)
                        {
                            bDirty = true;
                        }
                    }

                    MultiHashMap::Value v;
                    v._mkey = mk;
                    v._ukey = vtValue[j]._ukey;
                    v._value = value;
                    v._iVersion = ver;
                    v._dirty = bDirty;
                    v._iExpireTime = expireTimeSecond;
                    v._isDelete = TC_Multi_HashMap_Malloc::DELETE_FALSE;

                    vtV.push_back(v);

                }
            }
        }
    }
    g_app.ppReport(PPReport::SRP_SET_CNT, iJudgeCount);
    if (vtV.size() > 0)
    {
        int iSetRet = g_HashMap.set(vtV, TC_Multi_HashMap_Malloc::AUTO_DATA, bHead, bUpdateOrder, true);
        if (iSetRet != TC_Multi_HashMap_Malloc::RT_OK)
        {
            if (iSetRet == TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH)
            {
                return -2;
            }
            TLOG_ERROR("MKDbAccessCallback::procUpdate g_HashMap.set error, ret = " << iSetRet << ",mainKey = " << mk << endl);
            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
            return -1;
        }
        if (bRecordBinLog)
            WriteBinLog::set(mk, vtV, false, sBinLogFile);
        if (bKeyRecordBinLog)
            WriteBinLog::set(mk, vtV, false, sBinLogFile + "key");
    }
    iUpdateCount = vtV.size();
    return 0;
}

int updateResultAtom(const string &mk, const vector<MultiHashMap::Value> &vtValue, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, char ver, bool dirty, uint32_t expireTimeSecond, bool bHead, bool bUpdateOrder, const string sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog, int &iUpdateCount)
{
    size_t iJudgeCount = 0;
    vector<MultiHashMap::Value> vtV;
    vtV.reserve(vtValue.size());
    if (stLimit.bLimit)
    {
        size_t iIndex = 0;
        size_t iEnd = stLimit.iIndex + stLimit.iCount;
        for (size_t j = 0; j < vtValue.size(); j++)
        {
            iJudgeCount++;
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[j]._ukey);
            vDecode.setBuffer(vtValue[j]._value);

            string retValue;

            if (judge(ukDecode, vtUKCond))
            {
                if (iIndex >= stLimit.iIndex && iIndex < iEnd)
                {
                    int iRet = g_HashMap.update(mk, vtValue[j]._ukey, mpValue, vtValueCond, reinterpret_cast<const TC_Multi_HashMap_Malloc::FieldConf*>(g_app.gstat()->fieldconfigPtr()), false, 0, 0, retValue, false, 0, expireTimeSecond, true, TC_Multi_HashMap_Malloc::AUTO_DATA, true, false, 0, false);

                    if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        MultiHashMap::Value v;
                        v._mkey = mk;
                        v._ukey = vtValue[j]._ukey;
                        v._value = retValue;
                        v._iVersion = 0;
                        v._dirty = true;
                        v._iExpireTime = expireTimeSecond;
                        v._isDelete = TC_Multi_HashMap_Malloc::DELETE_FALSE;

                        vtV.push_back(v);

                        iIndex++;
                        if (iIndex >= iEnd)
                            break;
                    }
                }
            }
        }
    }
    else
    {
        for (size_t j = 0; j < vtValue.size(); j++)
        {
            iJudgeCount++;
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[j]._ukey);
            vDecode.setBuffer(vtValue[j]._value);

            if (judge(ukDecode, vtUKCond))
            {
                string retValue;
                int iRet = g_HashMap.update(mk, vtValue[j]._ukey, mpValue, vtValueCond, reinterpret_cast<const TC_Multi_HashMap_Malloc::FieldConf*>(g_app.gstat()->fieldconfigPtr()), false, 0, 0, retValue, false, 0, expireTimeSecond, true, TC_Multi_HashMap_Malloc::AUTO_DATA, true, false, 0, false);

                if (iRet == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    MultiHashMap::Value v;
                    v._mkey = mk;
                    v._ukey = vtValue[j]._ukey;
                    v._value = retValue;
                    v._iVersion = 0;
                    v._dirty = true;
                    v._iExpireTime = expireTimeSecond;
                    v._isDelete = TC_Multi_HashMap_Malloc::DELETE_FALSE;

                    vtV.push_back(v);
                }
            }
        }
    }
    g_app.ppReport(PPReport::SRP_SET_CNT, iJudgeCount);
    if (vtV.size() > 0)
    {
        if (bRecordBinLog)
            WriteBinLog::set(mk, vtV, false, sBinLogFile);
        if (bKeyRecordBinLog)
            WriteBinLog::set(mk, vtV, false, sBinLogFile + "key");
    }
    iUpdateCount = vtV.size();
    return 0;
}

int delResult(const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, const string &sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog)
{
    if (stLimit.bLimit)
    {
        size_t iIndex = 0;
        size_t iEnd = stLimit.iIndex + stLimit.iCount;
        for (size_t i = 0; i < vtValue.size(); i++)
        {
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[i]._ukey);
            vDecode.setBuffer(vtValue[i]._value);

            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    if (iIndex >= stLimit.iIndex && iIndex < iEnd)
                    {
                        int iDelRet = g_HashMap.delSetBit(mk, vtValue[i]._ukey, time(NULL));

                        if (iDelRet != TC_Multi_HashMap_Malloc::RT_OK && iDelRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                        {
                            TLOG_ERROR("MKCacheImp::del g_HashMap.erase error, ret = " << iDelRet << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            return -1;
                        }
                        if (bRecordBinLog)
                            WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile);
                        if (bKeyRecordBinLog)
                            WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile + "key");
                    }
                    iIndex++;
                    if (iIndex >= iEnd)
                        break;
                }

            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtValue.size(); i++)
        {
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[i]._ukey);
            vDecode.setBuffer(vtValue[i]._value);

            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    int iDelRet = g_HashMap.delSetBit(mk, vtValue[i]._ukey, time(NULL));

                    if (iDelRet != TC_Multi_HashMap_Malloc::RT_OK && iDelRet != TC_Multi_HashMap_Malloc::RT_NO_DATA && iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("MKCacheImp::del g_HashMap.erase error, ret = " << iDelRet << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return -1;
                    }

                    if (bRecordBinLog)
                        WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile);
                    if (bKeyRecordBinLog)
                        WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile + "key");
                }
            }
        }
    }
    return 0;
}
int DelResult(const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, const string &sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog, tars::TarsCurrentPtr current, DbAccessPrx pMKDBaccess, bool bExistDB)
{
    vector<MultiHashMap::Value> vtDelValue;
    size_t iJudgeCount = 0;
    if (stLimit.bLimit)
    {
        size_t iIndex = 0;
        size_t iEnd = stLimit.iIndex + stLimit.iCount;
        for (size_t i = 0; i < vtValue.size(); i++)
        {
            iJudgeCount++;
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[i]._ukey);
            vDecode.setBuffer(vtValue[i]._value);

            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    if (iIndex >= stLimit.iIndex && iIndex < iEnd)
                    {
                        int iDelRet = g_HashMap.delSetBit(mk, vtValue[i]._ukey, time(NULL));

                        if (iDelRet != TC_Multi_HashMap_Malloc::RT_OK
                                && iDelRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                                && iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                        {
                            TLOG_ERROR("MKCacheImp::del g_HashMap.erase error, ret = " << iDelRet << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            return -1;
                        }
                        if (bRecordBinLog)
                            WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile);
                        if (bKeyRecordBinLog)
                            WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile + "key");

                    }
                    iIndex++;
                    if (iIndex >= iEnd)
                        break;
                }
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtValue.size(); i++)
        {
            TarsDecode ukDecode;
            TarsDecode vDecode;
            iJudgeCount++;
            ukDecode.setBuffer(vtValue[i]._ukey);
            vDecode.setBuffer(vtValue[i]._value);

            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    int iDelRet = g_HashMap.delSetBit(mk, vtValue[i]._ukey, time(NULL));

                    if (iDelRet != TC_Multi_HashMap_Malloc::RT_OK
                            && iDelRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                            && iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("MKCacheImp::del g_HashMap.erase error, ret = " << iDelRet << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return -1;
                    }

                    if (bRecordBinLog)
                        WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile);
                    if (bKeyRecordBinLog)
                        WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile + "key");

                }
            }
        }
    }
    g_app.ppReport(PPReport::SRP_SET_CNT, iJudgeCount);
    return 0;
}

int DelResult(const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, const string & sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog, DbAccessPrx pMKDBaccess, bool bExistDB, int &ret)
{
    ret = 0;
    vector<MultiHashMap::Value> vtDelValue;
    size_t iJudgeCount = 0;
    if (stLimit.bLimit)
    {
        size_t iIndex = 0;
        size_t iEnd = stLimit.iIndex + stLimit.iCount;
        for (size_t i = 0; i < vtValue.size(); i++)
        {
            iJudgeCount++;
            TarsDecode ukDecode;
            TarsDecode vDecode;

            ukDecode.setBuffer(vtValue[i]._ukey);
            vDecode.setBuffer(vtValue[i]._value);

            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    if (iIndex >= stLimit.iIndex && iIndex < iEnd)
                    {
                        int iDelRet = g_HashMap.delSetBit(mk, vtValue[i]._ukey, time(NULL));

                        if (iDelRet != TC_Multi_HashMap_Malloc::RT_OK
                                && iDelRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                                && iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                        {
                            TLOG_ERROR("MKCacheImp::del g_HashMap.erase error, ret = " << iDelRet << endl);
                            g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                            return -1;
                        }
                        if (bRecordBinLog)
                            WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile);
                        if (bKeyRecordBinLog)
                            WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile + "key");

                        ret++;

                    }
                    iIndex++;
                    if (iIndex >= iEnd)
                        break;
                }
            }
        }
    }
    else
    {
        for (size_t i = 0; i < vtValue.size(); i++)
        {
            TarsDecode ukDecode;
            TarsDecode vDecode;
            iJudgeCount++;
            ukDecode.setBuffer(vtValue[i]._ukey);
            vDecode.setBuffer(vtValue[i]._value);

            if (judge(ukDecode, vtUKCond))
            {
                if (judge(vDecode, vtValueCond))
                {
                    int iDelRet = g_HashMap.delSetBit(mk, vtValue[i]._ukey, time(NULL));

                    if (iDelRet != TC_Multi_HashMap_Malloc::RT_OK
                            && iDelRet != TC_Multi_HashMap_Malloc::RT_NO_DATA
                            && iDelRet != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    {
                        TLOG_ERROR("MKCacheImp::del g_HashMap.erase error, ret = " << iDelRet << endl);
                        g_app.ppReport(PPReport::SRP_CACHE_ERR, 1);
                        return -1;
                    }

                    if (bRecordBinLog)
                        WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile);
                    if (bKeyRecordBinLog)
                        WriteBinLog::del(mk, vtValue[i]._ukey, sBinLogFile + "key");

                    ret++;
                }
            }
        }
    }
    g_app.ppReport(PPReport::SRP_SET_CNT, iJudgeCount);
    return 0;
}

string TarsDecode::read(uint8_t tag, const string& type, const string &sDefault, bool isRequire)
{
    TarsInputStream<BufferReader> isk;
    isk.setBuffer(_buf.c_str(), _buf.length());
    string s;

    if (type == TYPE_BYTE)
    {
        tars::Char n;
        n = TC_Common::strto<tars::Char>(sDefault);
        isk.read(n, tag, isRequire);

        //用这种方法而不用TC_Common::tostr()
        //是由于tostr对于char类型的值为0时，会返回空字符串
        ostringstream sBuffer;
        sBuffer << n;
        s = sBuffer.str();
    }
    else if (type == TYPE_SHORT)
    {
        tars::Short n;
        n = TC_Common::strto<tars::Short>(sDefault);
        isk.read(n, tag, isRequire);
        s = TC_Common::tostr(n);
    }
    else if (type == TYPE_INT)
    {
        tars::Int32 n;
        n = TC_Common::strto<tars::Int32>(sDefault);
        isk.read(n, tag, isRequire);
        s = TC_Common::tostr(n);
    }
    else if (type == TYPE_LONG)
    {
        tars::Int64 n;
        n = TC_Common::strto<tars::Int64>(sDefault);
        isk.read(n, tag, isRequire);
        s = TC_Common::tostr(n);
    }
    else if (type == TYPE_STRING)
    {
        s = sDefault;
        isk.read(s, tag, isRequire);
    }
    else if (type == TYPE_FLOAT)
    {
        tars::Float n;
        n = TC_Common::strto<tars::Float>(sDefault);
        isk.read(n, tag, isRequire);
        s = TC_Common::tostr(n);
    }
    else if (type == TYPE_DOUBLE)
    {
        tars::Double n;
        n = TC_Common::strto<tars::Double>(sDefault);
        isk.read(n, tag, isRequire);
        s = TC_Common::tostr(n);
    }
    else if (type == TYPE_UINT32)
    {
        tars::UInt32 n;
        n = TC_Common::strto<tars::UInt32>(sDefault);
        isk.read(n, tag, isRequire);
        s = TC_Common::tostr(n);
    }
    else if (type == TYPE_UINT16)
    {
        tars::UInt16 n;
        n = TC_Common::strto<tars::UInt16>(sDefault);
        isk.read(n, tag, isRequire);
        s = TC_Common::tostr(n);
    }
    else
    {
        throw MKDCacheException("TarsEncode::read type error");
    }
    return s;
}

int WriteBinLog::_binlogFD = -1;
int WriteBinLog::_keyBinlogFD = -1;
TC_ThreadLock WriteBinLog::_binlogLock;

int WriteBinLog::createBinLogFile(const string &path, bool isKeyBinLog)
{
    TC_ThreadLock::Lock lock(_binlogLock);

    if (isKeyBinLog)
    {
        //先关闭上次打开的文件
        if (_keyBinlogFD > 0)
            close(_keyBinlogFD);

        _keyBinlogFD = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (_keyBinlogFD < 0)
        {
            TLOG_ERROR("[WriteBinLog::createBinLogFile] open file error! " << path + "|errno:" << errno << endl);
            return -1;
        }
    }
    else
    {
        //先关闭上次打开的文件
        if (_binlogFD > 0)
            close(_binlogFD);

        _binlogFD = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (_binlogFD < 0)
        {
            TLOG_ERROR("[WriteBinLog::createBinLogFile] open file error! " << path + "|errno:" << errno << endl);
            return -1;
        }
    }

    return 0;
}

void WriteBinLog::writeToFile(const string &content, const string &logFile)
{
    bool isKeyBinlog = (logFile.find("key") == string::npos) ? false : true;

    int exitCount = 5;//最大重试次数
    unsigned int n = 0;
    int fd;

    unsigned int iContentSize = content.size();

    //先加锁
    TC_ThreadLock::Lock lock(_binlogLock);

    //获取文件描述符
    if (isKeyBinlog)
        fd = _keyBinlogFD;
    else
        fd = _binlogFD;

    //循环写，最大重试5次
    do
    {
        long tmpn = 0;
        tmpn = write(fd, &(content[n]), iContentSize - n);
        if (tmpn < iContentSize - n)
        {
            string error_str = "[WriteBinLog::writeToFile] write binlog " + logFile + " file error! " + TC_Common::tostr(errno);
            TLOG_ERROR(error_str << endl);
            TARS_NOTIFY_ERROR(error_str);
        }

        if (tmpn >= 0)
            n += tmpn;

    } while ((n < iContentSize) && (exitCount-- > 0));
}

void WriteBinLog::set(const string &mk, const string &uk, const string &value, uint32_t expireTime, bool dirty, const string &logfile, const enum BinLogType logType)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeSet(mk, uk, value, expireTime, dirty, logType);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::set(const string &mk, const string &uk, const string &logfile, const enum BinLogType logType)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeSet(mk, uk, logType);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::set(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, const string &logfile, bool fromDB, const enum BinLogType logType)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeSet(mk, vs, full, fromDB, logType);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::erase(const string &mk, const string &logfile, const enum BinLogType logType)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeErase(mk, logType);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::del(const string &mk, const string &logfile, const enum BinLogType logType)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeDel(mk, logType);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}
void WriteBinLog::setMKOnlyKey(const string &mk, const string &logfile, const enum BinLogType logType)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeMKOnlyKey(mk, logType);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}
void WriteBinLog::erase(const string &mk, const string &uk, const string &logfile, const enum BinLogType logType)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeErase(mk, uk, logType);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::del(const string &mk, const string &uk, const string &logfile, const enum BinLogType logType)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeDel(mk, uk, logType);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::pushList(const string &mainKey, const vector<pair<uint32_t, string> > &vtvalue, bool bHead, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodePushList(mainKey, vtvalue, bHead);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::popList(const string &mainKey, bool bHead, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodePopList(mainKey, bHead);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}
void WriteBinLog::replaceList(const string &mainKey, const string &value, long iPos, uint32_t iExpireTime, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeRepalceList(mainKey, value, iPos, iExpireTime);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::trimList(const string &mainKey, long iStart, long iEnd, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeTrimList(mainKey, iStart, iEnd);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::remList(const string &mainKey, bool bHead, long iCount, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeRemList(mainKey, bHead, iCount);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::addSet(const string &mk, const string &value, uint32_t expireTime, bool dirty, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeAddSet(mk, value, expireTime, dirty);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::addSet(const string &mk, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeAddSet(mk);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::delSet(const string &mk, const string &value, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeDelSet(mk, value);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::delSet(const string &mk, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeDelSet(mk);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::addZSet(const string & mk, const string &value, double score, uint32_t iExpireTime, bool bDirty, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeAddZSet(mk, value, score, iExpireTime, bDirty);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::incScoreZSet(const string & mk, const string &value, double score, uint32_t iExpireTime, bool bDirty, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeIncScoreZSet(mk, value, score, iExpireTime, bDirty);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::delZSet(const string &mk, const string &value, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeDelZSet(mk, value);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::delZSet(const string &mk, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeDelZSet(mk);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::delRangeZSet(const string &mk, double iMin, double iMax, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeDelRangeZSet(mk, iMin, iMax);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::pushList(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, const string &logfile, bool fromDB)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodePushList(mk, vs, full, fromDB);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::addSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, const string &logfile, bool fromDB)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeAddSet(mk, vs, full, fromDB);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::addZSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, const string &logfile, bool fromDB)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeAddZSet(mk, vs, full, fromDB);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::addZSet(const string &mk, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeAddZSet(mk);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

void WriteBinLog::updateZSet(const string & mk, const string &sOldValue, const string &sNewValue, double score, uint32_t iOldExpireTime, uint32_t iExpireTime, bool bDirty, const string &logfile)
{
    string sBinLog;
    MKBinLogEncode logEncode;
    sBinLog = logEncode.EncodeUpdateZSet(mk, sOldValue, sNewValue, score, iOldExpireTime, iExpireTime, bDirty);
    writeToFile(sBinLog + "\n", logfile);
    if (g_app.gstat()->serverType() == MASTER)
        g_app.gstat()->setBinlogTime(0, TNOW);
}

string FormatLog::tostr(const vector<DCache::Condition> & vtCond)
{
    string s;
    for (size_t i = 0; i < vtCond.size(); i++)
    {
        s += vtCond[i].fieldName + FormatLog::tostr(vtCond[i].op) + vtCond[i].value + "&";
    }
    return s.substr(0, s.length() - 1);
}

string FormatLog::tostr(const map<std::string, DCache::UpdateValue> & mpValue)
{
    string s;
    for (map<std::string, DCache::UpdateValue>::const_iterator it = mpValue.begin(); it != mpValue.end(); ++it)
    {
        s += it->first + "[" + FormatLog::tostr(it->second.op) + "]&";
    }
    return s.substr(0, s.length() - 1);
}

string FormatLog::tostr(const vector<DCache::Condition> &vtUKCond, const vector<DCache::Condition> &vtValueCond)
{
    string s;
    for (size_t i = 0; i < vtUKCond.size(); i++)
    {
        s += vtUKCond[i].fieldName + FormatLog::tostr(vtUKCond[i].op) + vtUKCond[i].value + "&";
    }
    for (size_t i = 0; i < vtValueCond.size(); i++)
    {
        s += vtValueCond[i].fieldName + FormatLog::tostr(vtValueCond[i].op) + vtValueCond[i].value + "&";
    }
    return s.substr(0, s.length() - 1);
}

string FormatLog::tostr(const vector<DCache::DbCondition> & vtDbCond)
{
    string s;
    for (size_t i = 0; i < vtDbCond.size(); i++)
    {
        s += vtDbCond[i].fieldName + FormatLog::tostr(vtDbCond[i].op) + vtDbCond[i].value + "&";
    }
    return s.substr(0, s.length() - 1);
}

string FormatLog::tostr(const map<std::string, DCache::DbUpdateValue> & mpDbValue)
{
    string s;
    for (map<std::string, DCache::DbUpdateValue>::const_iterator it = mpDbValue.begin(); it != mpDbValue.end(); ++it)
    {
        s += it->first + "[" + FormatLog::tostr(it->second.op) + "]&";
    }
    return s.substr(0, s.length() - 1);
}
string FormatLog::tostr(const map<string, DCache::UpdateFieldInfo>  & mpDbValue)
{
    string s;
    for (map<std::string, DCache::UpdateFieldInfo>::const_iterator it = mpDbValue.begin(); it != mpDbValue.end(); ++it)
    {
        s += it->first + "[" + FormatLog::tostr(it->second.upDateValue.op) + "]&";
    }
    return s.substr(0, s.length() - 1);
}
string FormatLog::tostr(enum Op op)
{
    string s;
    if (op == DCache::SET)
    {
        s = "=";
    }
    else if (op == DCache::ADD)
    {
        s = "++";
    }
    else if (op == DCache::SUB)
    {
        s = "--";
    }
    else if (op == DCache::EQ)
    {
        s = "==";
    }
    else if (op == DCache::NE)
    {
        s = "!=";
    }
    else if (op == DCache::GT)
    {
        s = ">";
    }
    else if (op == DCache::LT)
    {
        s = "<";
    }
    else if (op == DCache::LE)
    {
        s = "<=";
    }
    else if (op == DCache::GE)
    {
        s = ">=";
    }
    else if (op == DCache::LIMIT)
    {
        s = " limit ";
    }
    else
    {
        throw MKDCacheException("op type error");
    }
    return s;
}

string FormatLog::tostr(const string &str, const bool bUkey)
{
    TarsDecode decode;
    decode.setBuffer(str);

    const FieldConf &fieldConfig = g_app.gstat()->fieldconfig();
    string s;
    if (bUkey)
    {
        for (size_t i = 0; i < fieldConfig.vtUKeyName.size(); i++)
        {
            if (i != 0)
            {
                s += "&";
            }
            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.vtUKeyName[i]);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }

            string value = decode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire);
            s += fieldConfig.vtUKeyName[i] + "=" + value;
        }
    }
    else
    {
        for (size_t i = 0; i < fieldConfig.vtValueName.size(); i++)
        {
            if (i != 0)
            {
                s += "&";
            }
            map<string, FieldInfo>::const_iterator itInfo = fieldConfig.mpFieldInfo.find(fieldConfig.vtValueName[i]);
            if (itInfo == fieldConfig.mpFieldInfo.end())
            {
                throw MKDCacheException("mpFieldInfo find error");
            }

            string value = decode.read(itInfo->second.tag, itInfo->second.type, itInfo->second.defValue, itInfo->second.bRequire);
            s += fieldConfig.vtValueName[i] + "=" + value;
        }
    }
    return s;
}

string FormatLog::tostr(const vector<InsertKeyValue> & vtKeyValue)
{
    ostringstream os;
    for (size_t i = 0; i < vtKeyValue.size(); ++i)
    {
        const InsertKeyValue& keyValue = vtKeyValue[i];
        os << keyValue.mainKey << "|" << tostr(keyValue.mpValue) << "|" << (int)keyValue.ver << "|" << keyValue.dirty << "|" << keyValue.replace << "|" << keyValue.expireTimeSecond << "\n";
    }

    return os.str().substr(0, os.str().length() - 1);
}
string FormatLog::tostr(const vector<UpdateKeyValue> & vtKeyValue)
{
    ostringstream os;
    for (size_t i = 0; i < vtKeyValue.size(); ++i)
    {
        const UpdateKeyValue& keyValue = vtKeyValue[i];
        os << keyValue.mainKey << "|" << tostr(keyValue.mpValue) << "|" << (int)keyValue.ver << "|" << keyValue.dirty << "|" << keyValue.insert << "|" << keyValue.expireTimeSecond << "\n";
    }

    return os.str().substr(0, os.str().length() - 1);
}
string FormatLog::tostr(const vector<MainKeyCondition> & vtKey)
{
    ostringstream os;
    for (size_t i = 0; i < vtKey.size(); ++i)
    {
        const MainKeyCondition& keyValue = vtKey[i];
        os << keyValue.mainKey << "|" << keyValue.field << "|";

        for (size_t j = 0; j < keyValue.cond.size(); j++)
        {
            os << tostr(keyValue.cond[j]) << " ";
        }

        os << "|";

        if (keyValue.limit.op == DCache::LIMIT)
        {
            os << FormatLog::tostr(keyValue.limit.op) + keyValue.limit.value;
        }

        os << "|" << keyValue.bGetMKCout << "\n";
    }

    return os.str().substr(0, os.str().length() - 1);
}

