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

#ifndef _BINLOGENCODE_COMM_H_
#define _BINLOGENCODE_COMM_H_

#include "Common.h"
#include "util/tc_autoptr.h"

namespace DCache
{

    struct BinLogException : public std::runtime_error
    {
        BinLogException(const std::string& s) :runtime_error(s) {}
    };

    enum BinLogOpt
    {
        BINLOG_SET = 1,		//set一条
        BINLOG_DEL,			//del一条记录
        BINLOG_ERASE,		//erase一条记录
        BINLOG_DEL_MK,		//del主key下所有记录，只适用于MKCache 
        BINLOG_ERASE_MK,	//erase主key下所有记录 ，只适用于MKCache
        BINLOG_SET_MUTIL,	//set主key下一批记录，只适用于MKCache
        BINLOG_CLEAR,		//clear
        BINLOG_ERASE_HASH_BYFORCE, //erase一个hash值下的所有记录
        BINLOG_SET_MUTIL_FROMDB, //set 主key下一批记录，只适用于MKCache从DB获取数据后写binlog
        BINLOG_SET_ONLYKEY, // 设置onlykey

        BINLOG_PUSH_LIST,
        BINLOG_POP_LIST,
        BINLOG_REPLACE_LIST,
        BINLOG_TRIM_LIST,
        BINLOG_REM_LIST,

        BINLOG_ADD_SET,
        BINLOG_DEL_SET,
        BINLOG_DEL_SET_MK,

        BINLOG_ADD_ZSET,
        BINLOG_INC_SCORE_ZSET,
        BINLOG_DEL_ZSET,
        BINLOG_DEL_ZSET_MK,
        BINLOG_DEL_RANGE_ZSET,

        BINLOG_PUSH_LIST_MUTIL,
        BINLOG_ADD_SET_MUTIL,
        BINLOG_ADD_ZSET_MUTIL,
        BINLOG_ADD_SET_MUTIL_FROMDB,
        BINLOG_ADD_ZSET_MUTIL_FROMDB,

        BINLOG_ADD_SET_ONLYKEY,
        BINLOG_ADD_ZSET_ONLYKEY,

        BINLOG_UPDATE_ZSET,

        BINLOG_UNDO = 0   //无操作 
    };

    enum BinLogType
    {
        BINLOG_NORMAL = 16, //普通业务操作产生的binlog
        BINLOG_BAK,			//备机备份时产生的binlog
        BINLOG_TRANS,		//迁移时产生的binlog
        BINLOG_SYNC,		//节点间同步时产生的binlog
        BINLOG_ADMIN,        //外部管理命令操作产生的binlog 
        BINLOG_ACTIVE		//保活日志，确保每个小时都有binlog生成
    };

    class BinLogEncodeComm : public TC_HandleBase
    {
    public:

        BinLogEncodeComm() {}

        ~BinLogEncodeComm() {}

        virtual void Decode(const string& binLog) = 0;


        virtual enum BinLogOpt GetOpt() = 0;

        virtual string GetStringKey() = 0;
    };

    typedef TC_AutoPtr<BinLogEncodeComm> BinLogEncodePtr;
}
#endif


