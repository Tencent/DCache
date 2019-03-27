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
#ifndef __PROPERTY_SERVER_H_
#define __PROPERTY_SERVER_H_

#include "servant/Application.h"
#include "PropertyHashMap.h"
#include "PropertyReapThread.h"
#include "CacheInfoUpdateThread.h"

using namespace tars;

class PropertyServer : public Application
{ 
public:
    /**
     * 构造函数
     **/
    PropertyServer()
    : _reapThread(NULL)
    , _updateThread(NULL) 
    {}

    /**
     * 析构函数
     **/
    virtual ~PropertyServer() {};

    void getTimeInfo(time_t &time, string &date, string &flag);
    
    const string & getClonePath(void) const;

    int getInsertInterval(void) const;

    const map<string, string> & getPropertyNameMap() const;

    PropertyHashMap & getHashMap();
        
protected:
    /**
     * 初始化, 只会进程调用一次
     */
    virtual void initialize();

    /**
     * 析够, 每个进程都会调用一次
     */
    virtual void destroyApp();
        
private:

    void initHashMap();
    
private:
    // 服务配置
    TC_Config _conf;

    // 数据文件目录
    string _clonePatch;
    
    // 数据库插入间隔
    int _insertInterval;

    // 特性名到db字段名的映射关系
    map<string, string> _propertyNameMap;

  PropertyReapThread* _reapThread;

    // cache服务信息更新时间间隔
    int _updateInterval;
    
    // cache服务信息更新线程
    CacheInfoUpdateThread* _updateThread;

    PropertyHashMap _hashMap;
};

extern PropertyServer g_app;

#endif

