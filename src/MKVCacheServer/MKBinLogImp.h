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
#ifndef _MKBINLOGIMP_H
#define _MKBINLOGIMP_H

#include <fstream>
#include "servant/Application.h"
#include "MKBinLog.h"
#include "MKCacheServer.h"

using namespace tars;
using namespace std;

/*
*BinLogObj接口的实现类
*/
class MKBinLogImp : public DCache::MKBinLog
{
public:
    /**
     *
     */
    virtual ~MKBinLogImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy() {}

    /*
    *重载配置
    */
    bool reloadConf(const string& command, const string& params, string& result);

    virtual tars::Int32 getLog(const DCache::BinLogReq &req, DCache::BinLogRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getLogCompress(const DCache::BinLogReq &req, DCache::BinLogRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getLogCompressWithPart(const DCache::BinLogReq &req, DCache::BinLogCompPartRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getLastBinLogTime(tars::UInt32& lastTime, tars::TarsCurrentPtr current);
protected:
    /*
    *从binlog日志文件名中，分离出时间部分,如 DCache.CacheServer_binlog_2009052010.log中提出时间2009052010
    */
    string getLogFileTime(const std::string & logfile);

    /*
    *提binlog日志文件名的前缀,如 DCache.CacheServer_binlog_2009052010.log中提出DCache.CacheServer_binlog
    */
    string getLogFilePrefix(const std::string & logfile);

    /*
    *查找下一个binlog日志
    */
    bool findNextLogFile(const std::string & logfile, std::string &nextfile);

private:

protected:
    TC_Config _tcConf;
    string _binlogFile;
    bool _isKeySyncMode;
    bool _isGzip;
};

#endif
