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
#ifndef __PROPERTY_IMP_H_
#define __PROPERTY_IMP_H_

#include "util/tc_common.h"
#include "util/tc_thread.h"
#include "util/tc_option.h"
#include "servant/Application.h"
#include "Property.h"
#include "PropertyHashMap.h"

using namespace tars;
using namespace DCache;

class PropertyImp : public Property
{
public:

    /**
     *
     */
    PropertyImp();

    /**
     * 析够函数
     */
    ~PropertyImp()  { }

    /**
     * 初始化
     *
     * @return void
     */
    virtual void initialize();

    /**
     * 退出
     */
    virtual void destroy() { }

    /**
    * 上报性属信息
    * @param statmsg, 上报信息
    * @return int, 返回0表示成功
    */
    virtual int reportPropMsg(const map<DCache::StatPropMsgHead, DCache::StatPropMsgBody> &propMsg, tars::TarsCurrentPtr current);

    virtual int queryPropData(const DCache::QueryPropCond &req, vector<DCache::QueriedResult> &rsp, tars::TarsCurrentPtr current);

  private:

    void dump2file();
};

#endif


