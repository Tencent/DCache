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
#ifndef _MKV_UTIL_H
#define _MKV_UTIL_H

#include "tc_multi_hashmap_malloc.h"
#include "CacheShare.h"

using namespace DCache;
using namespace tars;

namespace HashMap
{

    /*
     * 解码tars编码二进制流
     */
    class TarsDecode
    {
    public:
        TarsDecode() {}
        ~TarsDecode() {}
        void setBuffer(const string &sBuf)
        {
            m_sBuf = sBuf;
        }
        string getBuffer()
        {
            return m_sBuf;
        }

        /*
         * 将某字段从tars编码流中读出
         * @param tag: 某字段的tag编号
         * @param type: 某字段的类型
         * @param sDefault: 默认值
         * @param isRequire: 是否是必须字段
         */
        string read(uint8_t tag, const string& type, const string &sDefault, bool isRequire);

    private:
        string m_sBuf;
    };

    struct MKDCacheException : public std::runtime_error
    {
        MKDCacheException(const std::string& s) : std::runtime_error(s) {}
    };

    /*
     * 判断某字段是否满足用户输入的条件
     * @param decode: 数据记录的tars编码的decode对象实例
     * @param op: 判断的操作符
     * @param type: 需要判断字段的类型
     * @param tag: 需要判断字段的tag编号
     * @param sDefault: 需要判断字段的默认值
     * @param isRequire: 需要判断字段是否是必须存在的
     * @return bool:
     *          true：成功
     *          false：失败
     */
    bool judgeValue(TarsDecode &decode, const string &value, Op op, const string &type, uint8_t tag, const string &sDefault, bool isRequire);

    /*
     * 判断某数据记录是否满足用户输入的条件集
     * @param decode: 数据记录的tars编码的decode对象实例
     * @param vtCond: 用户输入的条件集
     * @return bool:
     *          true：成功
     *          false：失败
     */
    bool judge(TarsDecode &decode, const vector<DCache::Condition> &vtCond, const TC_Multi_HashMap_Malloc::FieldConf &fieldConf);
    bool judge(TarsDecode &ukDecode, TarsDecode &vDecode, const vector<vector<DCache::Condition> > &vtCond);

    void convertField(const string &sField, vector<string> &vtField);
    void setResult(const vector<string> &vtField, const string &mainKey, const vector<DCache::Condition> &vtUKCond, TarsDecode &vDecode, const char ver, uint32_t expireTime, vector<map<std::string, std::string> > &vtData);
    void setResult(const vector<string> &vtField, const string &mainKey, TarsDecode &ukDecode, TarsDecode &vDecode, const char ver, uint32_t expireTime, vector<map<std::string, std::string> > &vtData);

    string updateValue(const map<string, DCache::UpdateValue> &mpValue, const TC_Multi_HashMap_Malloc::FieldConf &fieldInfo, const string &sStreamValue);

};

#endif
