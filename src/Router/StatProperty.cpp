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
#include "StatProperty.h"
#include "servant/Application.h"
#include "util/tc_monitor.h"

bool StatProperty::_report = true;
bool StatProperty::_reportTime = true;
bool StatProperty::_reportCallTimes = true;
bool StatProperty::_reportFailTimes = true;
StatProperty::PropertyCache StatProperty::_propertyCache;
TC_ThreadLock StatProperty::_lock;

StatProperty::StatProperty(const string &funcName, const string &info)
    : _retSucc(true), _callTimes(1), _funcName(funcName), _info(info)
{
    gettimeofday(&_beginTime, NULL);
}

StatProperty::~StatProperty()
{
    if (!_report) return;

    timeval finishTime;
    gettimeofday(&finishTime, NULL);
    uint64_t costTime = (finishTime.tv_sec - _beginTime.tv_sec) * 1000000 +
                        (finishTime.tv_usec - _beginTime.tv_usec);

    FDLOG("transfer") << FILE_FUN << _funcName << "|" << (_retSucc ? "Succ" : "Fail") << "|"
                      << _callerIP << "|" << _calleeIP << "|" << costTime << "|" << _callTimes
                      << "|" << _info << "|" << _execResult << "|" << _msg << endl;

    if (Application::getCommunicator()->getProperty("property").empty()) return;

    if (_reportTime)
    {
        PropertyReportPtr ptr = getCostTimeReportPtr(_funcName + "_CostTime");
        if (ptr) ptr->report(costTime);
    }

    if (_reportCallTimes)
    {
        PropertyReportPtr ptr = getCallTimesReportPtr(_funcName + "_CallTimes");
        if (ptr) ptr->report(_callTimes);
    }

    if (_reportFailTimes && !_retSucc)
    {
        PropertyReportPtr ptr = getFailTimesReportPtr(_funcName + "_FailCount");
        if (ptr) ptr->report(1);
    }
}

PropertyReportPtr StatProperty::getCostTimeReportPtr(const string &name)
{
    if (_propertyCache.find(name) != _propertyCache.end()) return _propertyCache[name];

    PropertyReportPtr ptr = Application::getCommunicator()->getStatReport()->createPropertyReport(
        name, PropertyReport::avg(), PropertyReport::max(), PropertyReport::min());

    TC_ThreadLock::Lock lock(_lock);
    _propertyCache[name] = ptr;
    return ptr;
}

PropertyReportPtr StatProperty::getCallTimesReportPtr(const string &name)
{
    if (_propertyCache.find(name) != _propertyCache.end()) return _propertyCache[name];

    PropertyReportPtr ptr = Application::getCommunicator()->getStatReport()->createPropertyReport(
        name, PropertyReport::sum(), PropertyReport::max(), PropertyReport::min());

    TC_ThreadLock::Lock lock(_lock);
    _propertyCache[name] = ptr;
    return ptr;
}

PropertyReportPtr StatProperty::getFailTimesReportPtr(const string &name)
{
    if (_propertyCache.find(name) != _propertyCache.end()) return _propertyCache[name];

    PropertyReportPtr ptr = Application::getCommunicator()->getStatReport()->createPropertyReport(
        name, PropertyReport::count());

    TC_ThreadLock::Lock lock(_lock);
    _propertyCache[name] = ptr;
    return ptr;
}
