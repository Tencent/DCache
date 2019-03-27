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
#ifndef _DumpThread_H_
#define _DumpThread_H_

#include <string>

#include "util/tc_thread.h"
#include "util/tc_common.h"
#include "util/tc_file.h"

#include "MKCacheGlobe.h"

using namespace tars;
using namespace std;
using namespace DCache;

class DumpThread : public TC_Thread, public TC_ThreadLock
{
public:
    DumpThread()
    {
        _isStart = false;
        _isRuning = false;
    }
    virtual ~DumpThread() {}

    /*
    * 初始化
    */
    int init(const string &dumpPath, const string &mirrorName, const string &sConf);

    /**
    * 结束线程
    */
    void terminate()
    {
    }

    void doDump();

    void stop()
    {
        _isStart = false;
    }

    bool isRuning()
    {
        return _isRuning;
    }

protected:
    virtual void run()
    {
        _isStart = true;
        doDump();
        _isStart = false;
    }

    int cacheToBinlog(const string & dumpPath, const string & mirrorName, string & errmsg);

protected:

    TC_Config _tcConf;
    string _config;

    //按天日志
    string _dumpDayLog;

    //线程启动停止标志
    bool _isStart;

    //线程当前状态
    bool _isRuning;

    //dump生成的镜像所在绝对路径
    string _dumpPath;

    //dump生成的镜像文件名
    string _mirrorName;
};

#endif
