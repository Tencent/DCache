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
#ifndef _CONTROL_ACK_H
#define _CONTROL_ACK_H

#include "servant/Application.h"
#include "ControlAck.h"


using namespace tars;
using namespace std;

/*
*BinLogAckObj接口的实现类
*/
class ControlAckImp : public DCache::ControlAck
{
public:
    /**
     *
     */
    virtual ~ControlAckImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy() {}


    virtual tars::Int32 connectHb(tars::TarsCurrentPtr current);

private:
    TC_Config 			_tcConf;
};

#endif
