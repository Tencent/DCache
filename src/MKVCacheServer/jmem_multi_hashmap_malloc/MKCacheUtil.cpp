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
#include "MKCacheUtil.h"
#include "util/tc_common.h"

#define TYPE_BYTE "byte"
#define TYPE_SHORT "short"
#define TYPE_INT "int"
#define TYPE_LONG "long"
#define TYPE_STRING "string"
#define TYPE_FLOAT "float"
#define TYPE_DOUBLE "double"
#define TYPE_UINT32 "unsigned int"
#define TYPE_UINT16 "unsigned short"

const double EPSILON = 1.00e-07;
#define FLOAT_EQ(x,v) (((v - EPSILON) < x) && (x <( v + EPSILON)))

namespace HashMap
{
    bool judgeValue(TarsDecode &decode, const string &value, Op op, const string &type, uint8_t tag, const string &sDefault, bool isRequire)
    {
        tars::TarsInputStream<tars::BufferReader> isk;
        isk.setBuffer(decode.getBuffer().c_str(), decode.getBuffer().length());

        if (type == TYPE_BYTE)
        {
            tars::Char n = tars::TC_Common::strto<tars::Char>(sDefault);
            isk.read(n, tag, isRequire);
            tars::Char m = tars::TC_Common::strto<tars::Char>(value);
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
                throw HashMap::MKDCacheException("judgeValue: op error");
        }
        else if (type == TYPE_SHORT)
        {
            tars::Short n = tars::TC_Common::strto<tars::Short>(sDefault);
            isk.read(n, tag, isRequire);
            tars::Short m = tars::TC_Common::strto<tars::Short>(value);
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
                throw HashMap::MKDCacheException("judgeValue: op error");
        }
        else if (type == TYPE_INT)
        {
            tars::Int32 n = tars::TC_Common::strto<tars::Int32>(sDefault);
            isk.read(n, tag, isRequire);
            tars::Int32 m = tars::TC_Common::strto<tars::Int32>(value);
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
                throw HashMap::MKDCacheException("judgeValue: op error");
        }
        else if (type == TYPE_LONG)
        {
            tars::Int64 n = tars::TC_Common::strto<tars::Int64>(sDefault);
            isk.read(n, tag, isRequire);
            tars::Int64 m = tars::TC_Common::strto<tars::Int64>(value);
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
                throw HashMap::MKDCacheException("judgeValue: op error");
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
                throw HashMap::MKDCacheException("judgeValue: op error");
        }
        else if (type == TYPE_FLOAT)
        {
            tars::Float n = tars::TC_Common::strto<tars::Float>(sDefault);
            isk.read(n, tag, isRequire);
            tars::Float m = tars::TC_Common::strto<tars::Float>(value);
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
                throw HashMap::MKDCacheException("judgeValue: op error");
        }
        else if (type == TYPE_DOUBLE)
        {
            tars::Double n = tars::TC_Common::strto<tars::Double>(sDefault);
            isk.read(n, tag, isRequire);
            tars::Double m = tars::TC_Common::strto<tars::Double>(value);
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
                throw HashMap::MKDCacheException("judgeValue: op error");
        }
        else if (type == TYPE_UINT32)
        {
            tars::UInt32 n = tars::TC_Common::strto<tars::UInt32>(sDefault);
            isk.read(n, tag, isRequire);
            tars::UInt32 m = tars::TC_Common::strto<tars::UInt32>(value);
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
                throw HashMap::MKDCacheException("judgeValue: op error");
        }
        else if (type == TYPE_UINT16)
        {
            tars::UInt16 n = tars::TC_Common::strto<tars::UInt16>(sDefault);
            isk.read(n, tag, isRequire);
            tars::UInt16 m = tars::TC_Common::strto<tars::UInt16>(value);
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
                throw HashMap::MKDCacheException("judgeValue: op error");
        }
        else
        {
            throw HashMap::MKDCacheException("judgeValue: type error");
        }
        return false;
    }
    string updateValue(const map<string, DCache::UpdateValue> &mpValue, const TC_Multi_HashMap_Malloc::FieldConf &fieldConf, const string &sStreamValue)
    {
	    tars::TarsInputStream<tars::BufferReader> isk;
        isk.setBuffer(sStreamValue.c_str(), sStreamValue.length());
	    tars::TarsOutputStream<tars::BufferWriter> osk;

        vector<string>::const_iterator it = fieldConf.vtValueName.begin();
        for (; it != fieldConf.vtValueName.end(); it++)
        {
            const string &sValueName = *it;
            const TC_Multi_HashMap_Malloc::FieldInfo &fieldInfo = fieldConf.mpFieldInfo.find(sValueName)->second;

            const string &type = fieldInfo.type;
            uint8_t tag = fieldInfo.tag;
            const string &sDefault = fieldInfo.defValue;
            bool isRequire = fieldInfo.bRequire;

            if (type == TYPE_BYTE)
            {
                tars::Char n = tars::TC_Common::strto<tars::Char>(sDefault);
                isk.read(n, tag, isRequire);

                map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
                tars::Char k;
                if (it != mpValue.end())
                {
                    tars::Char m = tars::TC_Common::strto<tars::Char>(it->second.value);
                    if (it->second.op == DCache::SET)
                        k = m;
                    else if (it->second.op == DCache::ADD)
                        k = n + m;
                    else if (it->second.op == DCache::SUB)
                        k = n - m;
                    else
                        throw HashMap::MKDCacheException("updateValue: op error");
                }
                else
                {
                    k = n;
                }
                osk.write(k, tag);
            }
            else if (type == TYPE_SHORT)
            {
                tars::Short n = tars::TC_Common::strto<tars::Short>(sDefault);
                isk.read(n, tag, isRequire);

                map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
                tars::Short k;
                if (it != mpValue.end())
                {
                    tars::Short m = tars::TC_Common::strto<tars::Short>(it->second.value);
                    if (it->second.op == DCache::SET)
                        k = m;
                    else if (it->second.op == DCache::ADD)
                        k = n + m;
                    else if (it->second.op == DCache::SUB)
                        k = n - m;
                    else
                        throw HashMap::MKDCacheException("updateValue: op error");
                }
                else
                {
                    k = n;
                }
                osk.write(k, tag);
            }
            else if (type == TYPE_INT)
            {
                tars::Int32 n = tars::TC_Common::strto<tars::Int32>(sDefault);
                isk.read(n, tag, isRequire);

                map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
                tars::Int32 k;
                if (it != mpValue.end())
                {
                    tars::Int32 m = tars::TC_Common::strto<tars::Int32>(it->second.value);
                    if (it->second.op == DCache::SET)
                        k = m;
                    else if (it->second.op == DCache::ADD)
                        k = n + m;
                    else if (it->second.op == DCache::SUB)
                        k = n - m;
                    else
                        throw HashMap::MKDCacheException("updateValue: op error");
                }
                else
                {
                    k = n;
                }
                osk.write(k, tag);
            }
            else if (type == TYPE_LONG)
            {
                tars::Int64 n = tars::TC_Common::strto<tars::Int64>(sDefault);
                isk.read(n, tag, isRequire);

                map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
                tars::Int64 k;
                if (it != mpValue.end())
                {
                    tars::Int64 m = tars::TC_Common::strto<tars::Int64>(it->second.value);
                    if (it->second.op == DCache::SET)
                        k = m;
                    else if (it->second.op == DCache::ADD)
                        k = n + m;
                    else if (it->second.op == DCache::SUB)
                        k = n - m;
                    else
                        throw HashMap::MKDCacheException("updateValue: op error");
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
                        throw HashMap::MKDCacheException("updateValue: op error");
                    else
                        throw HashMap::MKDCacheException("updateValue: op error");
                }
                else
                {
                    k = n;
                }
                osk.write(k, tag);
            }
            else if (type == TYPE_FLOAT)
            {
                tars::Float n = tars::TC_Common::strto<tars::Float>(sDefault);
                isk.read(n, tag, isRequire);

                map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
                tars::Float k;
                if (it != mpValue.end())
                {
                    tars::Float m = tars::TC_Common::strto<tars::Float>(it->second.value);
                    if (it->second.op == DCache::SET)
                        k = m;
                    else if (it->second.op == DCache::ADD)
                        k = n + m;
                    else if (it->second.op == DCache::SUB)
                        k = n - m;
                    else
                        throw HashMap::MKDCacheException("updateValue: op error");
                }
                else
                {
                    k = n;
                }
                osk.write(k, tag);
            }
            else if (type == TYPE_DOUBLE)
            {
                tars::Double n = tars::TC_Common::strto<tars::Double>(sDefault);
                isk.read(n, tag, isRequire);

                map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
                tars::Double k;
                if (it != mpValue.end())
                {
                    tars::Double m = tars::TC_Common::strto<tars::Double>(it->second.value);
                    if (it->second.op == DCache::SET)
                        k = m;
                    else if (it->second.op == DCache::ADD)
                        k = n + m;
                    else if (it->second.op == DCache::SUB)
                        k = n - m;
                    else
                        throw HashMap::MKDCacheException("updateValue: op error");
                }
                else
                {
                    k = n;
                }
                osk.write(k, tag);
            }
            else if (type == TYPE_UINT32)
            {
                tars::UInt32 n = tars::TC_Common::strto<tars::UInt32>(sDefault);
                isk.read(n, tag, isRequire);

                map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
                tars::UInt32 k;
                if (it != mpValue.end())
                {
                    tars::UInt32 m = tars::TC_Common::strto<tars::UInt32>(it->second.value);
                    if (it->second.op == DCache::SET)
                        k = m;
                    else if (it->second.op == DCache::ADD)
                        k = n + m;
                    else if (it->second.op == DCache::SUB)
                        k = n - m;
                    else
                        throw HashMap::MKDCacheException("updateValue: op error");
                }
                else
                {
                    k = n;
                }
                osk.write(k, tag);
            }
            else if (type == TYPE_UINT16)
            {
                tars::UInt16 n = tars::TC_Common::strto<tars::UInt16>(sDefault);
                isk.read(n, tag, isRequire);

                map<string, DCache::UpdateValue>::const_iterator it = mpValue.find(sValueName);
                tars::UInt16 k;
                if (it != mpValue.end())
                {
                    tars::UInt16 m = tars::TC_Common::strto<tars::UInt16>(it->second.value);
                    if (it->second.op == DCache::SET)
                        k = m;
                    else if (it->second.op == DCache::ADD)
                        k = n + m;
                    else if (it->second.op == DCache::SUB)
                        k = n - m;
                    else
                        throw HashMap::MKDCacheException("updateValue: op error");
                }
                else
                {
                    k = n;
                }
                osk.write(k, tag);
            }
            else
            {
                throw HashMap::MKDCacheException("updateValue: type error");
            }

        }
        string s(osk.getBuffer(), osk.getLength());
        //string s(osk.getByteBuffer().begin(), osk.getByteBuffer().end());

        return s;
    }

    bool judge(TarsDecode &decode, const vector<DCache::Condition> &vtCond, const TC_Multi_HashMap_Malloc::FieldConf &fieldInfo)
    {
        for (size_t i = 0; i < vtCond.size(); i++)
        {
            const DCache::Condition &cond = vtCond[i];
            map<string, TC_Multi_HashMap_Malloc::FieldInfo>::const_iterator it = fieldInfo.mpFieldInfo.find(cond.fieldName);
            if (it == fieldInfo.mpFieldInfo.end())
                throw HashMap::MKDCacheException("judge: field not found " + vtCond[i].fieldName);

            bool b = HashMap::judgeValue(decode, cond.value, cond.op, it->second.type, it->second.tag, it->second.defValue, it->second.bRequire);
            if (!b)
                return false;
        }
        return true;
    }

    string TarsDecode::read(uint8_t tag, const string& type, const string &sDefault, bool isRequire)
    {
	    tars::TarsInputStream<tars::BufferReader> isk;
        isk.setBuffer(m_sBuf.c_str(), m_sBuf.length());
        string s;

        if (type == TYPE_BYTE)
        {
            tars::Char n;
            n = tars::TC_Common::strto<tars::Char>(sDefault);
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
            n = tars::TC_Common::strto<tars::Short>(sDefault);
            isk.read(n, tag, isRequire);
            s = tars::TC_Common::tostr(n);
        }
        else if (type == TYPE_INT)
        {
            tars::Int32 n;
            n = tars::TC_Common::strto<tars::Int32>(sDefault);
            isk.read(n, tag, isRequire);
            s = tars::TC_Common::tostr(n);
        }
        else if (type == TYPE_LONG)
        {
            tars::Int64 n;
            n = tars::TC_Common::strto<tars::Int64>(sDefault);
            isk.read(n, tag, isRequire);
            s = tars::TC_Common::tostr(n);
        }
        else if (type == TYPE_STRING)
        {
            s = sDefault;
            isk.read(s, tag, isRequire);
        }
        else if (type == TYPE_FLOAT)
        {
            tars::Float n;
            n = tars::TC_Common::strto<tars::Float>(sDefault);
            isk.read(n, tag, isRequire);
            s = tars::TC_Common::tostr(n);
        }
        else if (type == TYPE_DOUBLE)
        {
            tars::Double n;
            n = tars::TC_Common::strto<tars::Double>(sDefault);
            isk.read(n, tag, isRequire);
            s = tars::TC_Common::tostr(n);
        }
        else if (type == TYPE_UINT32)
        {
            tars::UInt32 n;
            n = tars::TC_Common::strto<tars::UInt32>(sDefault);
            isk.read(n, tag, isRequire);
            s = tars::TC_Common::tostr(n);
        }
        else if (type == TYPE_UINT16)
        {
            tars::UInt16 n;
            n = tars::TC_Common::strto<tars::UInt16>(sDefault);
            isk.read(n, tag, isRequire);
            s = tars::TC_Common::tostr(n);
        }
        else
        {
            throw HashMap::MKDCacheException("TarsDecode::read type error");
        }
        return s;
    }
}

