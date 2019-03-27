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
#ifndef _STAT_PROPERTY_H_
#define _STAT_PROPERTY_H_

#include <sys/time.h>
#include <map>
#include "global.h"

using namespace std;
using namespace tars;

// 开始统计上报
#define STAT_PROPERTY(m) StatProperty sp(__FUNCTION__, m)
// 设置成功调用
#define CALL_SUCCESSED() sp.setSucc(true)
// 设置失败调用
#define CALL_FAILED() sp.setSucc(false)
// 设置调用返回
#define CALL_RETURN(s) sp.setRet(s)
// 设置信息描述
#define CALL_MSG(s) sp.setMsg(s)
// 设置主调
#define SET_CALLER(s) sp.setCaller(s)
// 设置被调
#define SET_CALLEE(s) sp.setCallee(s)

class StatProperty
{
public:
    typedef map<string, PropertyReportPtr> PropertyCache;

    StatProperty(const string &funcName, const string &info = "");
    ~StatProperty();

    static void setCheck(bool b1 = true, bool b2 = true, bool b3 = true, bool b4 = true)
    {
        _report = b1;
        _reportTime = b2;
        _reportCallTimes = b3;
        _reportFailTimes = b4;
    }

    void SetCallTimes(int i) { _callTimes = i; }
    void setSucc(bool b) { _retSucc = b; }
    void setRet(const string &execResult) { _execResult = execResult; }
    void setMsg(const string &msg) { _msg = msg; }

    void setCaller(const string &callerIP) { _callerIP = callerIP; }

    void setCallee(const string &calleeIP) { _calleeIP = calleeIP; }

private:
    PropertyReportPtr getCostTimeReportPtr(const string &name);
    PropertyReportPtr getCallTimesReportPtr(const string &name);
    PropertyReportPtr getFailTimesReportPtr(const string &name);

private:
    static bool _report;                  // 是否开启统计
    static bool _reportTime;              // 是否统计调用所花费的时间
    static bool _reportCallTimes;         // 是否统计调用次数
    static bool _reportFailTimes;         // 是否统计调用失败次数
    static PropertyCache _propertyCache;  // 统计信息的缓存
    static TC_ThreadLock _lock;           // 线程锁

    timeval _beginTime;  // 统计开始的时间
    bool _retSucc;       // 函数返回是否成功
    int _callTimes;      // 调用次数
    string _funcName;    // 调用者所在函数名称
    string _info;        // 基本信息
    string _execResult;  // 函数执行结果
    string _msg;         // 信息描述
    string _callerIP;    // 主调IP
    string _calleeIP;    // 被调IP
};

#endif
