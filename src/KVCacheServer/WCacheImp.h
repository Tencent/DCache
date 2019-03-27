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
#ifndef _WCacheImp_H_
#define _WCacheImp_H_

#include "servant/Application.h"
#include "WCache.h"
#include "CacheServer.h"
#include "DbAccessCallback.h"

using namespace tars;
using namespace std;


/**
 *CacheObj的接口的实现
 *
 */
class WCacheImp : public DCache::WCache
{
public:
    /**
     *
     */
    virtual ~WCacheImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy();

    bool reloadConf(const string& command, const string& params, string& result);

    virtual tars::Int32 setKV(const DCache::SetKVReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 setKVBatch(const DCache::SetKVBatchReq &req, DCache::SetKVBatchRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 insertKV(const DCache::SetKVReq &req, tars::TarsCurrentPtr current);

    virtual tars::Int32 updateKV(const DCache::UpdateKVReq &req, DCache::UpdateKVRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 eraseKV(const DCache::RemoveKVReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 eraseKVBatch(const DCache::RemoveKVBatchReq &req, DCache::RemoveKVBatchRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 delKV(const DCache::RemoveKVReq &req, tars::TarsCurrentPtr current);
    virtual tars::Int32 delKVBatch(const DCache::RemoveKVBatchReq &req, DCache::RemoveKVBatchRsp &rsp, tars::TarsCurrentPtr current);

protected:
    //判断是否是迁移源地址
    bool isTransSrc(int pageNo);
    tars::Int32 addStringKey(const std::string & moduleName, const std::string & keyItem, const std::string & value, tars::Bool dirty, tars::Int32 expireTimeSecond, tars::TarsCurrentPtr current);
    tars::Int32 replaceStringKey(const std::string & moduleName, const std::string & keyItem, const std::string & value, tars::Bool dirty, tars::Int32 expireTimeSecond, tars::TarsCurrentPtr current);
    tars::Int32 setStringKey(const std::string & moduleName, const std::string & keyItem, const std::string & value, tars::Char ver, tars::Bool dirty, tars::Int32 expireTimeSecond, tars::TarsCurrentPtr current);


protected:
    string _moduleName;
    string _config;
    TC_Config _tcConf;
    string _binlogFile;
    string _keyBinlogFile;
    bool _isRecordBinLog;
    bool _isRecordKeyBinLog;
    DbAccessPrx _dbaccessPrx;
    
    bool _saveOnlyKey;
    bool _readDB;
    bool _existDB;
    int _hitIndex;
    //DB主索引长度有限制，为了落地不失败
    size_t _maxKeyLengthInDB;

};
/////////////////////////////////////////////////////
#endif

