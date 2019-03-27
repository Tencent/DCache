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
#ifndef _CacheImp_H_
#define _CacheImp_H_

#include "servant/Application.h"
#include "Cache.h"
#include "CacheServer.h"
#include "DbAccessCallback.h"

using namespace tars;
using namespace std;

/**
 *CacheObj的接口的实现
 *
 */
class CacheImp : public DCache::Cache
{
public:
    /**
     *
     */
    virtual ~CacheImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy();

    bool reloadConf(const string& command, const string& params, string& result);

    virtual tars::Int32 checkKey(const DCache::CheckKeyReq &req, DCache::CheckKeyRsp &rsp, tars::TarsCurrentPtr current);
    virtual tars::Int32 getKV(const DCache::GetKVReq &req, DCache::GetKVRsp &rsp, tars::TarsCurrentPtr current);
    virtual tars::Int32 getKVBatch(const DCache::GetKVBatchReq &req, DCache::GetKVBatchRsp &rsp, tars::TarsCurrentPtr current);
    virtual tars::Int32 getAllKeys(const DCache::GetAllKeysReq &req, DCache::GetAllKeysRsp &rsp, tars::TarsCurrentPtr current);

    virtual tars::Int32 getSyncTime(tars::TarsCurrentPtr current);


    tars::Int32 checkString(const std::string& moduleName, const string& keyItem);

protected:
    //判断是否是迁移源地址
    bool isTransSrc(int pageNo);
    //获取迁移目标地址串
    string getTransDest(int pageNo);

    tars::Int32 getValueExp(const std::string & moduleName, const std::string & keyItem, std::string & value,
                           tars::Char & ver, tars::Int32 &expireTime, tars::TarsCurrentPtr current, bool accessDB = true);

protected:
    string _moduleName;
    string _config;
    TC_Config _tcConf;
    string _binlogFile;
    bool _isRecordBinLog;
    bool _isRecordKeyBinLog;
    DbAccessPrx _dbaccessPrx;

    bool _saveOnlyKey;
    bool _readDB;
    int _hitIndex;
};
/////////////////////////////////////////////////////
#endif
