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
#ifndef __PROPERTY_HASHMAP_H_
#define __PROPERTY_HASHMAP_H_

#include "util/tc_common.h"
#include "jmem/jmem_hashmap.h"
#include "Property.h"

using namespace tars;
using namespace DCache;

typedef DCache::StatPropMsgBody  PropBody;
typedef DCache::StatPropMsgHead  PropHead;
typedef DCache::StatPropMsgKey   PropKey;
typedef DCache::StatPropMsgValue PropValue;
typedef TarsHashMap<PropKey, PropValue, ThreadLockPolicy, FileStorePolicy> PropHashMap;
//typedef map<PropKey, PropValue, std::less<PropKey>, __gnu_cxx::__pool_alloc<std::pair<PropKey const, PropValue> > > PropertyMsg;
#if TARGET_PLATFORM_LINUX
typedef map<PropKey, PropValue, std::less<PropKey>, __gnu_cxx::__pool_alloc<std::pair<PropKey const, PropValue> > > PropertyMsg;
#else
typedef map<PropKey, PropValue, std::less<PropKey>> PropertyMsg;
#endif

class PropertyHashMap : public PropHashMap
{
public:
    /**
    * 添加数据
    * @param PropHead(Key)
    * @param PropBody(Value)
    *
    * @return int, 0表示成功，其他失败
    */
    int add(const PropHead &head, const PropBody &body)
    {
        PropKey key;
        PropValue value;

        key.moduleName    = head.moduleName;
        key.setName       = head.setName;
        key.setArea       = head.setArea;
        key.setID         = head.setID;
        key.ip            = head.ip;
        TarsOutputStream<BufferWriter> osk;
        key.writeTo(osk);

        string sk(osk.getBuffer(), osk.getLength());
        string sv;
        time_t t = 0;
        TC_LockT<ThreadLockPolicy::Mutex> lock(ThreadLockPolicy::mutex());
        int ret = this->_t.get(sk, sv, t);
        if (ret < 0 || ret == TC_HashMap::RT_NO_DATA)
        {
            value.statInfo.insert(pair<string, DCache::StatPropInfo>(head.propertyName, body.vInfo[0]));
        }
        // 读取到数据了, 解包
        else if (ret == TC_HashMap::RT_OK)
        {
            TarsInputStream<BufferReader> is;
            is.setBuffer(sv.c_str(), sv.length());
            value.readFrom(is);
            if (LOG->isNeedLog(TarsRollLogger::INFO_LOG))
            {
                ostringstream os;
                key.displaySimple(os);
                os << " " <<endl;
                value.displaySimple(os);
                TLOGINFO("read hash body|" << os.str() << endl);
            }

            map<string, DCache::StatPropInfo>::iterator it = value.statInfo.find(head.propertyName);
            if (it == value.statInfo.end())
            {
                value.statInfo.insert(pair<string, DCache::StatPropInfo>(head.propertyName, body.vInfo[0]));
            }
            else
            {
                const string &policy = it->second.policy;
                const string &inValue = body.vInfo[0].value;

                if (policy == "Count" )
                {
                    long long count = TC_Common::strto<long long>(it->second.value) + TC_Common::strto<long long>(inValue);
                    it->second.value = TC_Common::tostr(count);
                }
                else if (policy == "Sum")
                {
                    long long sum =  TC_Common::strto<long long>(it->second.value) + TC_Common::strto<long long>(inValue);
                    it->second.value = TC_Common::tostr(sum);

                }
                else if (policy == "Min")
                {
                    long long cur = TC_Common::strto<long long>(it->second.value);
                    long long in  = TC_Common::strto<long long>(inValue);
                    long long min = (cur < in) ? cur: in;
                    it->second.value = TC_Common::tostr(min);
                }			
                else if (policy == "Max")
                {
                    long long cur = TC_Common::strto<long long>(it->second.value);
                    long long in  = TC_Common::strto<long long>(inValue);
                    long long max = (cur > in) ? cur : in;
                    it->second.value = TC_Common::tostr(max);
                }
                else if (policy == "Distr")
                {
                    vector<string> fields = TC_Common::sepstr<string>(it->second.value, ",");
                    vector<string> fieldsIn = TC_Common::sepstr<string>(inValue, ",");
                    string tmpValue = "";
                    for (size_t k = 0; k < fields.size(); ++k)
                    {
                        vector<string> sTmp	 = TC_Common::sepstr<string>(fields[k], "|");
                        vector<string> inTmp = TC_Common::sepstr<string>(fieldsIn[k], "|");
                        long long tmp = TC_Common::strto<long long>(sTmp[1]) + TC_Common::strto<long long>(inTmp[1]);
                        sTmp[1] = TC_Common::tostr(tmp);
                        fields[k] = sTmp[0] + "|" + sTmp[1];

                        if (k == 0)
                        {
                            tmpValue = fields[k];
                        }
                        else
                        {
                            tmpValue = tmpValue + "," + fields[k];
                        }
                    }
                    it->second.value = tmpValue;
                }
                else if (policy == "Avg")
                {
                    vector<string> sTmp  = TC_Common::sepstr<string>(it->second.value, "=");
                    vector<string> inTmp = TC_Common::sepstr<string>(inValue, "=");

                    // 总值求和
                    double tmpValueSum = TC_Common::strto<double>(sTmp[0]) + TC_Common::strto<double>(inTmp[0]);
                    // 新版本平均值带有记录数,记录求和
                    long tmpCntSum = ((2 == inTmp.size()) ? (TC_Common::strto<long>(inTmp[1])) : 1) +
                        ((2 == sTmp.size()) ? (TC_Common::strto<long>(sTmp[1])) : 1);

                    it->second.value = TC_Common::tostr(tmpValueSum) + "=" + TC_Common::tostr(tmpCntSum);
                }
            }

            if(LOG->isNeedLog(TarsRollLogger::INFO_LOG))
            {
                ostringstream os;
                key.displaySimple(os);
                os << " " <<endl;
                value.displaySimple(os);
                TLOGINFO("reset hash body|" << os.str() << endl);
            }
        }
        else
        {
            return ret;
        }

        TarsOutputStream<BufferWriter> osv;
        value.writeTo(osv);
        sv.assign(osv.getBuffer(), osv.getLength());
        vector<TC_HashMap::BlockData> data;
        return this->_t.set(sk, sv, true, data);
    }
};

#endif
