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
#ifndef _MKBINLOGENCODE_H
#define _MKBINLOGENCODE_H

#include "MKCacheGlobe.h"
#include "util/tc_encoder.h"
#include "BinLogEncodeComm.h"
#include <sstream>
using namespace std;
using namespace tars;

/*
* BinLog编解码类
* BinLog格式：时间 操作标志 脏数据标志 MK长度MK值UK长度UK值Value长度Value值
*/
class MKBinLogEncode
{
public:
    MKBinLogEncode() {}
    ~MKBinLogEncode() {}

    string EncodeErase(const string &mk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        return EncodeOpt(mk, BINLOG_ERASE_MK, logType);
    }
    string EncodeDel(const string &mk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        return EncodeOpt(mk, BINLOG_DEL_MK, logType);
    }

    string EncodeMKOnlyKey(const string &mk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        return EncodeOpt(mk, BINLOG_SET_ONLYKEY, logType);
    }

    string EncodeOpt(const string &mk, const enum BinLogOpt opt, const enum BinLogType logType = BINLOG_NORMAL)
    {
        if (logType != BINLOG_NORMAL && logType != BINLOG_BAK && logType != BINLOG_TRANS && logType != BINLOG_SYNC && logType != BINLOG_ADMIN)
        {
            throw BinLogException("encode error, logType = " + TC_Common::tostr(logType));
        }

        if (opt != BINLOG_DEL_MK && opt != BINLOG_ERASE_MK  && opt != BINLOG_SET_ONLYKEY)
        {
            throw BinLogException("encode error, opt =" + TC_Common::tostr(opt));
        }
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        string sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " ";
        if (opt != BINLOG_SET_ONLYKEY)
        {
            sBinLog += TC_Common::tostr(opt);
        }
        else
        {
            sBinLog += "A";
        }

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        string sRet("");
        EncodeAddHead(sBinLog, sRet);
        return sRet;
    }

    string EncodeErase(const string &mk, const string &uk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        return EncodeEraseDel(mk, uk, BINLOG_ERASE, logType);
    }
    string EncodeDel(const string &mk, const string &uk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        return EncodeEraseDel(mk, uk, BINLOG_DEL, logType);
    }

    string EncodeEraseDel(const string &mk, const string &uk, const enum BinLogOpt opt, const enum BinLogType logType = BINLOG_NORMAL)
    {
        if (logType != BINLOG_NORMAL && logType != BINLOG_BAK && logType != BINLOG_TRANS && logType != BINLOG_SYNC && logType != BINLOG_ADMIN)
        {
            throw BinLogException("encode error, logType = " + TC_Common::tostr(logType));
        }

        if (opt != BINLOG_DEL && opt != BINLOG_ERASE)
        {
            throw BinLogException("encode error, opt =" + TC_Common::tostr(opt));
        }
        string sBinLog;
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + TC_Common::tostr(opt);

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iUKLen = uk.length();
        string sUKLen((char *)&iUKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk + sUKLen + uk;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        string sRet("");
        EncodeAddHead(sBinLog, sRet);
        return sRet;
    }

    string EncodeSet(const string &mk, const string &uk, const string &value, uint32_t expireTime, bool dirty, const enum BinLogType logType = BINLOG_NORMAL)
    {
        if (logType != BINLOG_NORMAL && logType != BINLOG_BAK && logType != BINLOG_TRANS && logType != BINLOG_SYNC && logType != BINLOG_ADMIN)
        {
            throw BinLogException("encode error, logType = " + TC_Common::tostr(logType));
        }

        string sBinLog;
        string ResultBinlog;
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + TC_Common::tostr(BINLOG_SET);

        string sDirty = dirty ? "1" : "0";

        sBinLog = sBinLog + " " + sDirty;

        string sExpireTime((char *)&expireTime, sizeof(uint32_t));

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iUKLen = uk.length();
        string sUKLen((char *)&iUKLen, sizeof(uint32_t));

        uint32_t iValueLen = value.length();
        string sValueLen((char *)&iValueLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sExpireTime + sMKLen + mk + sUKLen + uk + sValueLen + value;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }
    //只记录key的binlog 适用于value比较大且写量较的server
    string EncodeSet(const string &mk, const string &uk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        if (logType != BINLOG_NORMAL && logType != BINLOG_BAK && logType != BINLOG_TRANS && logType != BINLOG_SYNC && logType != BINLOG_ADMIN)
        {
            throw BinLogException("encode error, logType = " + TC_Common::tostr(logType));
        }

        string sBinLog;
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + TC_Common::tostr(BINLOG_SET);

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iUKLen = uk.length();
        string sUKLen((char *)&iUKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk + sUKLen + uk;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        string sRet("");
        EncodeAddHead(sBinLog, sRet);
        return sRet;
    }

    string EncodeSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, string & binlogTime, const enum BinLogType logType = BINLOG_NORMAL)
    {
        if (logType != BINLOG_NORMAL && logType != BINLOG_BAK && logType != BINLOG_TRANS && logType != BINLOG_SYNC && logType != BINLOG_ADMIN)
        {
            throw BinLogException("encode error, logType = " + TC_Common::tostr(logType));
        }

        string sBinLog;
        int64_t timestamp = TC_TimeProvider::getInstance()->getNow();
        binlogTime = TC_Common::tm2str(timestamp);
        timestamp = tars_htonll(timestamp);
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + TC_Common::tostr(BINLOG_SET_MUTIL);

        string sFull = full ? "1" : "0";

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iVsSize = vs.size();
        string sVsSize((char *)&iVsSize, sizeof(uint32_t));
        ostringstream tmpStream;
        sBinLog = sBinLog + " " + sFull + " " + sMKLen + mk + sVsSize;
        tmpStream << sBinLog;
        for (uint32_t i = 0; i < iVsSize; i++)
        {
            if (mk != vs[i]._mkey)
                throw BinLogException("encode error, mk not match");
            string sDirty = vs[i]._dirty ? "1" : "0";

            if (vs[i]._isDelete == TC_Multi_HashMap_Malloc::DELETE_TRUE)
            {
                sDirty = "3";
            }

            string sExpireTime((char *)&(vs[i]._iExpireTime), sizeof(uint32_t));

            string sUK = vs[i]._ukey;
            uint32_t iUKLen = sUK.length();
            string sUKLen((char *)&iUKLen, sizeof(uint32_t));

            string sValue = vs[i]._value;
            uint32_t iValueLen = sValue.length();
            string sValueLen((char *)&iValueLen, sizeof(uint32_t));
            sBinLog = sDirty + sExpireTime + sUKLen + sUK + sValueLen + sValue;
            tmpStream << sBinLog;
        }
        sBinLog.clear();
        string tmpBinlog = tmpStream.str();

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(tmpBinlog, sBinLog);
        return sBinLog;
    }

    string EncodeSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, bool fromDB = false, const enum BinLogType logType = BINLOG_NORMAL)
    {
        if (logType != BINLOG_NORMAL && logType != BINLOG_BAK && logType != BINLOG_TRANS && logType != BINLOG_SYNC && logType != BINLOG_ADMIN)
        {
            throw BinLogException("encode error, logType = " + TC_Common::tostr(logType));
        }

        string sBinLog;
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        if (fromDB)
        {
            sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + TC_Common::tostr(BINLOG_SET_MUTIL_FROMDB);
        }
        else
        {
            sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + TC_Common::tostr(BINLOG_SET_MUTIL);
        }

        string sFull = full ? "1" : "0";

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iVsSize = vs.size();
        string sVsSize((char *)&iVsSize, sizeof(uint32_t));
        ostringstream tmpStream;
        sBinLog = sBinLog + " " + sFull + " " + sMKLen + mk + sVsSize;
        tmpStream << sBinLog;
        for (uint32_t i = 0; i < iVsSize; i++)
        {
            if (mk != vs[i]._mkey)
                throw BinLogException("encode error, mk not match");
            string sDirty = vs[i]._dirty ? "1" : "0";

            if (!fromDB && vs[i]._isDelete == TC_Multi_HashMap_Malloc::DELETE_TRUE)
            {
                sDirty = "3";
            }

            string sExpireTime((char *)&(vs[i]._iExpireTime), sizeof(uint32_t));

            string sUK = vs[i]._ukey;
            uint32_t iUKLen = sUK.length();
            string sUKLen((char *)&iUKLen, sizeof(uint32_t));

            string sValue = vs[i]._value;
            uint32_t iValueLen = sValue.length();
            string sValueLen((char *)&iValueLen, sizeof(uint32_t));
            sBinLog = sDirty + sExpireTime + sUKLen + sUK + sValueLen + sValue;
            tmpStream << sBinLog;
        }
        sBinLog.clear();
        string tmpBinlog = tmpStream.str();

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(tmpBinlog, sBinLog);
        return sBinLog;
    }

    string EncodePushList(const string &mk, const vector<pair<uint32_t, string> > &vtvalue, bool bHead, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_PUSH_LIST - 10);

        string sHead = bHead ? "1" : "0";

        sBinLog = sBinLog + " " + sHead;

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        unsigned int valueSize = vtvalue.size();
        string sValueSize((char *)&valueSize, sizeof(unsigned int));

        sBinLog = sBinLog + " " + sMKLen + mk + sValueSize;

        for (unsigned int i = 0; i < vtvalue.size(); i++)
        {
            string sExpireTime((char *)&(vtvalue[i].first), sizeof(uint32_t));

            uint32_t iValueLen = vtvalue[i].second.length();
            string sValueLen((char *)&iValueLen, sizeof(uint32_t));

            sBinLog += sExpireTime + sValueLen + vtvalue[i].second;
        }

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodePopList(const string &mk, bool bHead, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_POP_LIST - 10);

        string sHead = bHead ? "1" : "0";

        sBinLog = sBinLog + " " + sHead;

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeRepalceList(const string &mk, const string &value, long iPos, uint32_t iExpireTime, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_REPLACE_LIST - 10);

        string sPos((char*)&iPos, sizeof(long));

        string sExpireTime((char *)&(iExpireTime), sizeof(uint32_t));

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iValueLen = value.length();
        string sValueLen((char *)&iValueLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sPos + " " + sExpireTime + sMKLen + mk + sValueLen + value;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeTrimList(const string &mk, long iStart, long iEnd, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_TRIM_LIST - 10);

        string sStart((char*)&iStart, sizeof(long));
        string sEnd((char*)&iEnd, sizeof(long));

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sStart + " " + sEnd + " " + sMKLen + mk;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeRemList(const string &mk, bool bHead, long iCount, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_REM_LIST - 10);

        string sHead = bHead ? "1" : "0";

        sBinLog = sBinLog + " " + sHead;

        string sCount((char*)&iCount, sizeof(long));

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sCount + " " + sMKLen + mk;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeAddSet(const string &mk, const string &value, uint32_t expireTime, bool dirty, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_SET - 10);

        string sDirty = dirty ? "1" : "0";

        sBinLog = sBinLog + " " + sDirty;

        string sTime((char*)&expireTime, sizeof(expireTime));

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iValueLen = value.length();
        string sValueLen((char*)&iValueLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sTime + sMKLen + mk + sValueLen + value;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeAddSet(const string &mk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_SET_ONLYKEY - 10);

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeDelSet(const string &mk, const string &value, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_DEL_SET - 10);

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iValueLen = value.length();
        string sValueLen((char*)&iValueLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk + sValueLen + value;
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeDelSet(const string &mk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_DEL_SET_MK - 10);

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk;
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeAddZSet(const string &mk, const string &value, double score, uint32_t iExpireTime, bool bDirty, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_ZSET - 10);

        string sDirty = bDirty ? "1" : "0";

        sBinLog = sBinLog + " " + sDirty;

        string sTime((char*)&iExpireTime, sizeof(iExpireTime));

        string sScore((char*)&score, sizeof(double));

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iValueLen = value.length();
        string sValueLen((char*)&iValueLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sTime + sScore + sMKLen + mk + sValueLen + value;
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeIncScoreZSet(const string &mk, const string &value, double score, uint32_t iExpireTime, bool bDirty, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_INC_SCORE_ZSET - 10);

        string sDirty = bDirty ? "1" : "0";

        sBinLog = sBinLog + " " + sDirty;

        string sTime((char*)&iExpireTime, sizeof(iExpireTime));

        string sScore((char*)&score, sizeof(double));

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iValueLen = value.length();
        string sValueLen((char*)&iValueLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sTime + sScore + sMKLen + mk + sValueLen + value;
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeDelZSet(const string &mk, const string &value, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_DEL_ZSET - 10);

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iValueLen = value.length();
        string sValueLen((char*)&iValueLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk + sValueLen + value;
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeDelZSet(const string &mk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_DEL_ZSET_MK - 10);

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk;
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeDelRangeZSet(const string &mk, double iMin, double iMax, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_DEL_RANGE_ZSET - 10);

        string sMin((char*)&iMin, sizeof(iMin));
        string sMax((char*)&iMax, sizeof(iMax));

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMin + sMax + sMKLen + mk;
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodePushList(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, bool fromDB = false, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_PUSH_LIST_MUTIL - 10);

        string sFull = full ? "1" : "0";

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iVsSize = vs.size();
        string sVsSize((char *)&iVsSize, sizeof(uint32_t));
        ostringstream tmpStream;
        sBinLog = sBinLog + " " + sFull + " " + sMKLen + mk + sVsSize;
        tmpStream << sBinLog;
        for (uint32_t i = 0; i < iVsSize; i++)
        {
            if (mk != vs[i]._mkey)
                throw BinLogException("encode error, mk not match");
            string sDirty = vs[i]._dirty ? "1" : "0";

            string sExpireTime((char *)&(vs[i]._iExpireTime), sizeof(uint32_t));

            string sValue = vs[i]._value;
            uint32_t iValueLen = sValue.length();
            string sValueLen((char *)&iValueLen, sizeof(uint32_t));
            sBinLog = sDirty + sExpireTime + sValueLen + sValue;
            tmpStream << sBinLog;
        }
        sBinLog.clear();
        string tmpBinlog = tmpStream.str();
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(tmpBinlog, sBinLog);
        return sBinLog;
    }

    string EncodePushList(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, string &binlogTime, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        char type = 'A';
        int64_t timestamp = TC_TimeProvider::getInstance()->getNow();
        binlogTime = TC_Common::tm2str(timestamp);
        timestamp = tars_htonll(timestamp);
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_PUSH_LIST_MUTIL - 10);

        string sFull = full ? "1" : "0";

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iVsSize = vs.size();
        string sVsSize((char *)&iVsSize, sizeof(uint32_t));
        ostringstream tmpStream;
        sBinLog = sBinLog + " " + sFull + " " + sMKLen + mk + sVsSize;
        tmpStream << sBinLog;
        for (uint32_t i = 0; i < iVsSize; i++)
        {
            if (mk != vs[i]._mkey)
                throw BinLogException("encode error, mk not match");
            string sDirty = vs[i]._dirty ? "1" : "0";

            string sExpireTime((char *)&(vs[i]._iExpireTime), sizeof(uint32_t));

            string sValue = vs[i]._value;
            uint32_t iValueLen = sValue.length();
            string sValueLen((char *)&iValueLen, sizeof(uint32_t));
            sBinLog = sDirty + sExpireTime + sValueLen + sValue;
            tmpStream << sBinLog;
        }
        sBinLog.clear();
        string tmpBinlog = tmpStream.str();
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(tmpBinlog, sBinLog);
        return sBinLog;
    }

    string EncodeAddSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, bool fromDB = false, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        if (!fromDB)
        {
            sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_SET_MUTIL - 10);
        }
        else
        {
            sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_SET_MUTIL_FROMDB - 10);
        }

        string sFull = full ? "1" : "0";

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iVsSize = vs.size();
        string sVsSize((char *)&iVsSize, sizeof(uint32_t));
        ostringstream tmpStream;
        sBinLog = sBinLog + " " + sFull + " " + sMKLen + mk + sVsSize;
        tmpStream << sBinLog;
        for (uint32_t i = 0; i < iVsSize; i++)
        {
            if (mk != vs[i]._mkey)
                throw BinLogException("encode error, mk not match");
            string sDirty = vs[i]._dirty ? "1" : "0";

            if (!fromDB && vs[i]._isDelete == TC_Multi_HashMap_Malloc::DELETE_TRUE)
            {
                sDirty = "3";
            }

            string sExpireTime((char *)&(vs[i]._iExpireTime), sizeof(uint32_t));

            string sValue = vs[i]._value;
            uint32_t iValueLen = sValue.length();
            string sValueLen((char *)&iValueLen, sizeof(uint32_t));
            sBinLog = sDirty + sExpireTime + sValueLen + sValue;
            tmpStream << sBinLog;
        }
        sBinLog.clear();
        string tmpBinlog = tmpStream.str();
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(tmpBinlog, sBinLog);
        return sBinLog;
    }

    string EncodeAddSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, string &binlogTime, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        char type = 'A';

        int64_t timestamp = TC_TimeProvider::getInstance()->getNow();
        binlogTime = TC_Common::tm2str(timestamp);
        timestamp = tars_htonll(timestamp);
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_SET_MUTIL - 10);

        string sFull = full ? "1" : "0";

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iVsSize = vs.size();
        string sVsSize((char *)&iVsSize, sizeof(uint32_t));
        ostringstream tmpStream;
        sBinLog = sBinLog + " " + sFull + " " + sMKLen + mk + sVsSize;
        tmpStream << sBinLog;
        for (uint32_t i = 0; i < iVsSize; i++)
        {
            if (mk != vs[i]._mkey)
                throw BinLogException("encode error, mk not match");
            string sDirty = vs[i]._dirty ? "1" : "0";

            if (vs[i]._isDelete == TC_Multi_HashMap_Malloc::DELETE_TRUE)
            {
                sDirty = "3";
            }

            string sExpireTime((char *)&(vs[i]._iExpireTime), sizeof(uint32_t));

            string sValue = vs[i]._value;
            uint32_t iValueLen = sValue.length();
            string sValueLen((char *)&iValueLen, sizeof(uint32_t));
            sBinLog = sDirty + sExpireTime + sValueLen + sValue;
            tmpStream << sBinLog;
        }
        sBinLog.clear();
        string tmpBinlog = tmpStream.str();
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(tmpBinlog, sBinLog);
        return sBinLog;
    }

    string EncodeAddZSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, bool fromDB = false, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        if (!fromDB)
        {
            sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_ZSET_MUTIL - 10);
        }
        else
        {
            sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_ZSET_MUTIL_FROMDB - 10);
        }

        string sFull = full ? "1" : "0";

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iVsSize = vs.size();
        string sVsSize((char *)&iVsSize, sizeof(uint32_t));
        ostringstream tmpStream;
        sBinLog = sBinLog + " " + sFull + " " + sMKLen + mk + sVsSize;
        tmpStream << sBinLog;
        for (uint32_t i = 0; i < iVsSize; i++)
        {
            if (mk != vs[i]._mkey)
                throw BinLogException("encode error, mk not match");
            string sDirty = vs[i]._dirty ? "1" : "0";

            if (!fromDB && vs[i]._isDelete == TC_Multi_HashMap_Malloc::DELETE_TRUE)
            {
                sDirty = "3";
            }

            string sExpireTime((char *)&(vs[i]._iExpireTime), sizeof(uint32_t));

            string sScore((char *)&(vs[i]._score), sizeof(double));

            string sValue = vs[i]._value;
            uint32_t iValueLen = sValue.length();
            string sValueLen((char *)&iValueLen, sizeof(uint32_t));
            sBinLog = sDirty + sExpireTime + sScore + sValueLen + sValue;
            tmpStream << sBinLog;
        }
        sBinLog.clear();
        string tmpBinlog = tmpStream.str();
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(tmpBinlog, sBinLog);
        return sBinLog;
    }

    string EncodeAddZSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, string &binlogTime, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        char type = 'A';

        int64_t timestamp = TC_TimeProvider::getInstance()->getNow();
        binlogTime = TC_Common::tm2str(timestamp);
        timestamp = tars_htonll(timestamp);
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_ZSET_MUTIL - 10);

        string sFull = full ? "1" : "0";

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iVsSize = vs.size();
        string sVsSize((char *)&iVsSize, sizeof(uint32_t));
        ostringstream tmpStream;
        sBinLog = sBinLog + " " + sFull + " " + sMKLen + mk + sVsSize;
        tmpStream << sBinLog;
        for (uint32_t i = 0; i < iVsSize; i++)
        {
            if (mk != vs[i]._mkey)
                throw BinLogException("encode error, mk not match");
            string sDirty = vs[i]._dirty ? "1" : "0";

            if (vs[i]._isDelete == TC_Multi_HashMap_Malloc::DELETE_TRUE)
            {
                sDirty = "3";
            }

            string sExpireTime((char *)&(vs[i]._iExpireTime), sizeof(uint32_t));

            string sScore((char *)&(vs[i]._score), sizeof(double));

            string sValue = vs[i]._value;
            uint32_t iValueLen = sValue.length();
            string sValueLen((char *)&iValueLen, sizeof(uint32_t));
            sBinLog = sDirty + sExpireTime + sScore + sValueLen + sValue;
            tmpStream << sBinLog;
        }
        sBinLog.clear();
        string tmpBinlog = tmpStream.str();
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(tmpBinlog, sBinLog);
        return sBinLog;
    }

    string EncodeAddZSet(const string &mk, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_ADD_ZSET_ONLYKEY - 10);

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        sBinLog = sBinLog + " " + sMKLen + mk;
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);
        return ResultBinlog;
    }

    string EncodeUpdateZSet(const string &mk, const string &sOldValue, const string &sNewValue, double score, uint32_t iOldExpireTime, uint32_t iExpireTime, bool bDirty, const enum BinLogType logType = BINLOG_NORMAL)
    {
        string sBinLog;
        string ResultBinlog;
        char type = 'A';

        if (logType != BINLOG_NORMAL && logType != BINLOG_BAK && logType != BINLOG_TRANS && logType != BINLOG_SYNC && logType != BINLOG_ADMIN)
        {
            throw BinLogException("encode error, logType = " + TC_Common::tostr(logType));
        }

        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        sBinLog = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + (char)(type + BINLOG_UPDATE_ZSET - 10);

        string sDirty = bDirty ? "1" : "0";

        string sOldExpireTime((char *)&iOldExpireTime, sizeof(uint32_t));
        string sExpireTime((char *)&iExpireTime, sizeof(uint32_t));

        uint32_t iMKLen = mk.length();
        string sMKLen((char *)&iMKLen, sizeof(uint32_t));

        uint32_t iOldValueLen = sOldValue.length();
        string sOldValueLen((char *)&iOldValueLen, sizeof(uint32_t));

        uint32_t iNewValueLen = sNewValue.length();
        string sNewValueLen((char *)&iNewValueLen, sizeof(uint32_t));

        string sScore((char *)&(score), sizeof(double));

        sBinLog = sBinLog + " " + sDirty + " " + sOldExpireTime + sExpireTime + sMKLen + mk + sOldValueLen + sOldValue + sNewValueLen + sNewValue + sScore;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLog, ResultBinlog);

        return ResultBinlog;
    }

    const string EncodeActive()
    {
        enum BinLogType logType = BINLOG_ACTIVE;

        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        string sBinLogBefTrans = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType);
        sBinLogBefTrans += " ";
        sBinLogBefTrans += "Active";

        string sBinLogAftTrans;
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(sBinLogBefTrans, sBinLogAftTrans);

        return sBinLogAftTrans;
    }

    bool DecodeKey(const string &sBinLog)
    {
        string s("");
        //判断binlog是否转义
        DecodeRemoveHead(sBinLog, s);

        int curIndex = 15;
        int iLogType = TC_Common::strto<int>(s.substr(curIndex, 2));
        if (iLogType >= (int)BINLOG_NORMAL && iLogType <= (int)BINLOG_ACTIVE)
        {
            _logType = (enum BinLogType)iLogType;
            _haveLogType = true;
            curIndex += 3;
        }
        else
        {
            _logType = BINLOG_NORMAL;
            _haveLogType = false;
        }

        //Active binlog
        if (BINLOG_ACTIVE == _logType)
        {
            return false;
        }

        string sOpt = s.substr(curIndex, 1);

        if (sOpt != "A")
        {
            _opt = (enum BinLogOpt) TC_Common::strto<int>(sOpt);
        }
        else
        {
            _opt = (enum BinLogOpt)10;
        }

        curIndex += 2;

        uint32_t iMKLen;
        uint32_t iUKLen;

        switch (_opt)
        {
        case BINLOG_SET:

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iUKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _uKey = s.substr(curIndex, iUKLen);
            return true;
        case BINLOG_DEL:
        case BINLOG_ERASE:
        case BINLOG_DEL_MK:
        case BINLOG_ERASE_MK:
        case BINLOG_SET_MUTIL:
        case BINLOG_SET_MUTIL_FROMDB:
        case BINLOG_SET_ONLYKEY:

        case BINLOG_PUSH_LIST:
        case BINLOG_POP_LIST:
        case BINLOG_REPLACE_LIST:
        case BINLOG_TRIM_LIST:
        case BINLOG_REM_LIST:

        case BINLOG_ADD_SET:
        case BINLOG_DEL_SET:
        case BINLOG_DEL_SET_MK:

        case BINLOG_ADD_ZSET:
        case BINLOG_INC_SCORE_ZSET:
        case BINLOG_DEL_ZSET:
        case BINLOG_DEL_ZSET_MK:
        case BINLOG_DEL_RANGE_ZSET:

        case BINLOG_PUSH_LIST_MUTIL:
        case BINLOG_ADD_SET_MUTIL:
        case BINLOG_ADD_ZSET_MUTIL:
        case BINLOG_ADD_SET_MUTIL_FROMDB:
        case BINLOG_ADD_ZSET_MUTIL_FROMDB:

        case BINLOG_ADD_SET_ONLYKEY:
        case BINLOG_ADD_ZSET_ONLYKEY:

        case BINLOG_UPDATE_ZSET:
            return false;
        default:
            throw BinLogException("decode error, opt =" + sOpt);
        }
    }

    //解码只记录key的binlog 返回是否为set 其他类型不需要处理 之间丢给备机
    void Decode(const string &sBinLog)
    {
        string s("");
        //判断binlog是否转义
        DecodeRemoveHead(sBinLog, s);

        // 跳过时间戳和一个空格(8 + 1)
        int curIndex = 9;
        int iLogType = TC_Common::strto<int>(s.substr(curIndex, 2));
        if (iLogType >= (int)BINLOG_NORMAL && iLogType <= (int)BINLOG_ACTIVE)
        {
            _logType = (enum BinLogType)iLogType;
            _haveLogType = true;
            curIndex += 3;
        }
        else
        {
            _logType = BINLOG_NORMAL;
            _haveLogType = false;
        }

        //Active binlog
        if (BINLOG_ACTIVE == _logType)
        {
            return;
        }

        string sOpt = s.substr(curIndex, 1);

        if (sOpt[0] < 'A')
        {
            _opt = (enum BinLogOpt) TC_Common::strto<int>(sOpt);
        }
        else if (sOpt[0] == 'A')
        {
            _opt = (enum BinLogOpt)10;
        }
        else
        {
            _opt = (enum BinLogOpt)(sOpt[0] - 'A' + 10);
        }

        curIndex += 2;

        uint32_t iMKLen;
        uint32_t iUKLen;
        uint32_t iValueLen;
        uint32_t iVsSize;
        string sDirty;
        string sFull;
        string sHead;
        unsigned int valueSize;
        string sOnlyScore;

        switch (_opt)
        {
        case BINLOG_SET:
            sDirty = s.substr(curIndex, 1);
            if (sDirty != "0" && sDirty != "1")
            {
                throw BinLogException("decode error, sDirty =" + sDirty);
            }
            _dirty = (sDirty == "1") ? true : false;
            curIndex += 2;

            _expireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iUKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _uKey = s.substr(curIndex, iUKLen);
            curIndex += iUKLen;


            iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _value = s.substr(curIndex, iValueLen);
            break;
        case BINLOG_DEL:
        case BINLOG_ERASE:
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iUKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _uKey = s.substr(curIndex, iUKLen);

            break;
        case BINLOG_DEL_MK:
        case BINLOG_ERASE_MK:
        case BINLOG_SET_ONLYKEY:
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            break;
        case BINLOG_SET_MUTIL:
        case BINLOG_SET_MUTIL_FROMDB:
            sFull = s.substr(curIndex, 1);
            if (sFull != "0" && sFull != "1")
            {
                throw BinLogException("decode error, sFull =" + sFull);
            }
            _isFull = (sFull == "1") ? true : false;
            curIndex += 2;
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iVsSize = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);
            for (uint32_t i = 0; i < iVsSize; i++)
            {
                MultiHashMap::Value v;
                v._mkey = _mainKey;

                sDirty = s.substr(curIndex, 1);
                if (sDirty == "3")
                {
                    v._isDelete = DCache::TC_Multi_HashMap_Malloc::DELETE_TRUE;
                }
                else if (sDirty != "0" && sDirty != "1")
                {
                    throw BinLogException("decode error, sDirty =" + sDirty);
                }
                v._dirty = (sDirty == "1") ? true : false;
                curIndex += 1;

                v._iExpireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                iUKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                v._ukey = s.substr(curIndex, iUKLen);
                curIndex += iUKLen;

                iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                v._value = s.substr(curIndex, iValueLen);
                curIndex += iValueLen;

                v._iVersion = 0;
                _hashmapValue.push_back(v);
            }
            break;
        case BINLOG_PUSH_LIST:
            sHead = s.substr(curIndex, 1);
            if (sHead != "0" && sHead != "1")
            {
                throw BinLogException("decode error, sHead =" + sHead);
            }
            _atHead = (sHead == "1") ? true : false;
            curIndex += 2;

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            valueSize = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            for (unsigned int i = 0; i < valueSize; i++)
            {
                _expireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                _ListValue.push_back(make_pair(_expireTime, s.substr(curIndex, iValueLen)));
                curIndex += iValueLen;
            }

            break;
        case BINLOG_POP_LIST:
            sHead = s.substr(curIndex, 1);
            if (sHead != "0" && sHead != "1")
            {
                throw BinLogException("decode error, sHead =" + sHead);
            }
            _atHead = (sHead == "1") ? true : false;
            curIndex += 2;

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            break;
        case BINLOG_REPLACE_LIST:
            _pos = *(long*)s.substr(curIndex, sizeof(long)).c_str();
            curIndex += sizeof(long) + 1;

            _expireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _value = s.substr(curIndex, iValueLen);

            break;
        case BINLOG_TRIM_LIST:
            _startIndex = *(long*)s.substr(curIndex, sizeof(long)).c_str();
            curIndex += sizeof(long) + 1;

            _endIndex = *(long*)s.substr(curIndex, sizeof(long)).c_str();
            curIndex += sizeof(long) + 1;

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;
            break;
        case BINLOG_REM_LIST:
            sHead = s.substr(curIndex, 1);
            if (sHead != "0" && sHead != "1")
            {
                throw BinLogException("decode error, sHead =" + sHead);
            }
            _atHead = (sHead == "1") ? true : false;
            curIndex += 2;

            _count = *(long*)s.substr(curIndex, sizeof(long)).c_str();
            curIndex += sizeof(long) + 1;

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;
            break;
        case BINLOG_ADD_SET:
            sDirty = s.substr(curIndex, 1);
            if (sDirty != "0" && sDirty != "1")
            {
                throw BinLogException("decode error, sDirty =" + sDirty);
            }
            _dirty = (sDirty == "1") ? true : false;
            curIndex += 2;

            _expireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _value = s.substr(curIndex, iValueLen);
            break;
        case BINLOG_DEL_SET:
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _value = s.substr(curIndex, iValueLen);
            break;
        case BINLOG_DEL_SET_MK:
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;
            break;
        case BINLOG_ADD_ZSET:
        case BINLOG_INC_SCORE_ZSET:
            sDirty = s.substr(curIndex, 1);
            if (sDirty != "0" && sDirty != "1")
            {
                throw BinLogException("decode error, sDirty =" + sDirty);
            }
            _dirty = (sDirty == "1") ? true : false;
            curIndex += 2;

            _expireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _score = *(double*)s.substr(curIndex, sizeof(double)).c_str();
            curIndex += sizeof(double);

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _value = s.substr(curIndex, iValueLen);
            break;
        case BINLOG_DEL_ZSET:
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _value = s.substr(curIndex, iValueLen);
            break;
        case BINLOG_DEL_ZSET_MK:
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;
            break;
        case BINLOG_DEL_RANGE_ZSET:
            _minScore = *(double*)s.substr(curIndex, sizeof(double)).c_str();
            curIndex += sizeof(double);

            _maxScore = *(double*)s.substr(curIndex, sizeof(double)).c_str();
            curIndex += sizeof(double);

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;
            break;
        case BINLOG_PUSH_LIST_MUTIL:
            sFull = s.substr(curIndex, 1);
            if (sFull != "0" && sFull != "1")
            {
                throw BinLogException("decode error, sFull =" + sFull);
            }
            _isFull = (sFull == "1") ? true : false;
            curIndex += 2;
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iVsSize = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);
            for (uint32_t i = 0; i < iVsSize; i++)
            {
                MultiHashMap::Value v;
                v._mkey = _mainKey;

                sDirty = s.substr(curIndex, 1);
                if (sDirty != "0" && sDirty != "1")
                {
                    throw BinLogException("decode error, sDirty =" + sDirty);
                }
                v._dirty = (sDirty == "1") ? true : false;
                curIndex += 1;

                v._iExpireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                v._value = s.substr(curIndex, iValueLen);
                curIndex += iValueLen;

                v._iVersion = 0;
                _hashmapValue.push_back(v);
            }
            break;
        case BINLOG_ADD_SET_MUTIL:
        case BINLOG_ADD_SET_MUTIL_FROMDB:
            sFull = s.substr(curIndex, 1);
            if (sFull != "0" && sFull != "1")
            {
                throw BinLogException("decode error, sFull =" + sFull);
            }
            _isFull = (sFull == "1") ? true : false;
            curIndex += 2;
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iVsSize = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);
            for (uint32_t i = 0; i < iVsSize; i++)
            {
                MultiHashMap::Value v;
                v._mkey = _mainKey;

                sDirty = s.substr(curIndex, 1);
                TLOGDEBUG("decode mk:" << _mainKey << " sDirty:" << sDirty << endl);
                if (sDirty == "3")
                {
                    v._isDelete = DCache::TC_Multi_HashMap_Malloc::DELETE_TRUE;
                }
                else if (sDirty != "0" && sDirty != "1")
                {
                    throw BinLogException("decode error, sDirty =" + sDirty);
                }
                v._dirty = (sDirty == "1") ? true : false;
                curIndex += 1;

                v._iExpireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                v._value = s.substr(curIndex, iValueLen);
                curIndex += iValueLen;

                v._iVersion = 0;
                _hashmapValue.push_back(v);
            }
            break;
        case BINLOG_ADD_ZSET_MUTIL:
        case BINLOG_ADD_ZSET_MUTIL_FROMDB:
            sFull = s.substr(curIndex, 1);
            if (sFull != "0" && sFull != "1")
            {
                throw BinLogException("decode error, sFull =" + sFull);
            }
            _isFull = (sFull == "1") ? true : false;
            curIndex += 2;
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iVsSize = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);
            for (uint32_t i = 0; i < iVsSize; i++)
            {
                MultiHashMap::Value v;
                v._mkey = _mainKey;

                sDirty = s.substr(curIndex, 1);
                if (sDirty == "3")
                {
                    v._isDelete = DCache::TC_Multi_HashMap_Malloc::DELETE_TRUE;
                }
                else if (sDirty != "0" && sDirty != "1")
                {
                    throw BinLogException("decode error, sDirty =" + sDirty);
                }
                v._dirty = (sDirty == "1") ? true : false;
                curIndex += 1;

                v._iExpireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                v._score = *(double *)s.substr(curIndex, sizeof(double)).c_str();
                curIndex += sizeof(double);

                iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
                curIndex += sizeof(uint32_t);

                v._value = s.substr(curIndex, iValueLen);
                curIndex += iValueLen;

                v._iVersion = 0;
                _hashmapValue.push_back(v);
            }
            break;
        case BINLOG_ADD_SET_ONLYKEY:
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;
            break;
        case BINLOG_ADD_ZSET_ONLYKEY:
            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;
            break;
        case BINLOG_UPDATE_ZSET:
            sDirty = s.substr(curIndex, 1);
            if (sDirty != "0" && sDirty != "1")
            {
                throw BinLogException("decode error, sDirty =" + sDirty);
            }
            _dirty = (sDirty == "1") ? true : false;
            curIndex += 2;

            _oldExpireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _expireTime = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            iMKLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _mainKey = s.substr(curIndex, iMKLen);
            curIndex += iMKLen;

            iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _oldValue = s.substr(curIndex, iValueLen);
            curIndex += iValueLen;

            iValueLen = *(uint32_t *)s.substr(curIndex, sizeof(uint32_t)).c_str();
            curIndex += sizeof(uint32_t);

            _value = s.substr(curIndex, iValueLen);
            curIndex += iValueLen;

            _score = *(double *)s.substr(curIndex, sizeof(double)).c_str();

            break;
        default:
            throw BinLogException("decode error, opt =" + sOpt);
            break;
        }
    }

    enum BinLogType GetBinLogType()
    {
        return _logType;
    }
    enum BinLogOpt GetOpt()
    {
        return _opt;
    }

    bool GetDirty()
    {
        return _dirty;
    }
    bool GetFull()
    {
        return _isFull;
    }
    bool GetHead()
    {
        return _atHead;
    }
    long GetPos()
    {
        return _pos;
    }
    long GetStart()
    {
        return _startIndex;
    }
    long GetEnd()
    {
        return _endIndex;
    }
    long GetCount()
    {
        return _count;
    }
    double GetScore()
    {
        return _score;
    }
    double GetScoreMin()
    {
        return _minScore;
    }
    double GetScoreMax()
    {
        return _maxScore;
    }
    vector<pair<uint32_t, string> > GetListValue()
    {
        return _ListValue;
    }
    string& GetMK()
    {
        return _mainKey;
    }
    string& GetUK()
    {
        return _uKey;
    }
    string& GetValue()
    {
        return _value;
    }
    vector<MultiHashMap::Value> & GetVs()
    {
        return _hashmapValue;
    }
    uint32_t GetExpireTime()
    {
        return _expireTime;
    }
    uint32_t GetOldExpireTime()
    {
        return _oldExpireTime;
    }
    string & GetOldValue()
    {
        return _oldValue;
    }


    /*读取一行binlog
     * return: 0 : succ;  -1 : error; -2: eof()
     */
    static int GetOneLineBinLog(ifstream &ifs, string &line)
    {
        uint32_t iBinLogLen = 0;

        //读头部，判断binlog类型
        {
            char headChar[8];
            char *p = headChar;
            ifs.read(p, 8);
            if (ifs)
            {
                if (string(p, 4) == "SERA")
                {
                    //读取头部信息
                    uint32_t iLen = *(uint32_t *)(p + 4);
                    iBinLogLen = ntohl(iLen);

                    line.append(string(p, 8));
                }
                else
                {
                    ifs.clear();
                    ifs.seekg(-8, ios::cur);
                    TLOGERROR("GetOneLineBinLog parse binlog head error," << string(p, 8) << endl);
                    return -1;
                }
            }
            else
            {
                //可读取字符数不足8, break, eof
                return -2;
            }
        }

        //SERA binlog处理
        {
            bool bEndFile = false;

            //binlog编码在行尾多加了'\n'
            char* pChar = new char[iBinLogLen + 1];
            ifs.read(pChar, iBinLogLen + 1);
            if (ifs)
            {
                //行尾'\n'不用append
                line.append(string(pChar, iBinLogLen));
            }
            else
            {
                //可读取字符数不足
                bEndFile = true;
            }

            delete[] pChar;

            if (bEndFile)
            {
                return -2;
            }
        }

        return 0;
    }

    //get binlog timestamp
    static int64_t GetTime(const std::string &longContent)
    {
        if ("SERA" == longContent.substr(0, 4))
        {
            //SERA模式的binlog，4字节Head，4字节length
            if (longContent.size() < 16)
                return 0;
            const char *p = longContent.data();
            p += 8;
            int64_t time = tars_ntohll(*(int64_t *)p);
            return time;
        }

        return 0;
    }

    static void GetTimeString(const string &longContent, string &timeString)
    {
        //SERA模式的binlog，4字节Head，4字节length, timestamp
        if (longContent.size() < 16)
            return;

        const char *p = longContent.data();
        p += 8;
        int64_t time = tars_ntohll(*(int64_t *)p);
        timeString = TC_Common::tm2str(time);
    }
private:

    int EncodeAddHead(const string &sBinLog, string &sRet)
    {
        //不进行特殊字符转义，增加头部SERA+BinlogLength
        sRet = "SERA";
        uint32_t binlogLen = sBinLog.size();
        uint32_t iSize = htonl(binlogLen);
        sRet.append(string((const char*)&iSize, sizeof(uint32_t)));
        sRet.append(sBinLog);

        return 0;
    }

    int DecodeRemoveHead(const string &sBinLog, string &sRet)
    {
        const char *p = sBinLog.data();
        size_t curIndex = 0;
        //4字节的头部
        string sHead(p, 4);
        if (sHead == "SERA")
        {
            curIndex += 4;

            //4字节的length
            uint32_t iLen = *(uint32_t*)(p + curIndex);
            uint32_t iBinlogLen = ntohl(iLen);
            curIndex += 4;

            sRet.clear();
            const char *pVal = p + curIndex;
            sRet.append(pVal, iBinlogLen);
        }

        return 0;
    }

private:
    //binlog类型，normal/bak/trans/sync
    enum BinLogType _logType;

    enum BinLogOpt _opt;

    //数据是否是脏数据
    bool _dirty;

    //解析的binlog中是否含有logtype字段
    bool _haveLogType;

    string _mainKey;
    string _uKey;
    string _value;
    uint32_t _expireTime;
    bool _isFull;
    vector<MultiHashMap::Value> _hashmapValue;

    bool _atHead;
    long _pos;
    long _startIndex;
    long _endIndex;
    long _count;
    double _score;
    double _minScore;
    double _maxScore;

    uint32_t _oldExpireTime;
    string _oldValue;

    vector<pair<uint32_t, string> > _ListValue;

};


#endif

