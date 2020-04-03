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
#ifndef _TBINLOGENCODE_H
#define _TBINLOGENCODE_H

#include <netinet/in.h>
#include "servant/Application.h"

#include "util/tc_timeprovider.h"
#include "BinLogEncodeComm.h"

using namespace std;
using namespace tars;
using namespace DCache;

/*
* BinLog编解码类，用于TCache
* BinLog格式：时间 操作标志 脏数据标志 key类型 key长度 key值 value
*/
class TBinLogEncode : public BinLogEncodeComm
{
public:

    TBinLogEncode() {}
    ~TBinLogEncode() {}

    /*
    * 编码binlog：只编码key
    */
    template <typename T>
    const string& EncodeSetKey(const T& key, const enum BinLogType logType = BINLOG_NORMAL)
    {
        if (logType != BINLOG_NORMAL && logType != BINLOG_BAK && logType != BINLOG_TRANS && logType != BINLOG_SYNC && logType != BINLOG_ADMIN)
        {
            throw BinLogException("encode error, logType = " + TC_Common::tostr(logType));
        }
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        _binLogBefTrans = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType) + " " + TC_Common::tostr(BINLOG_SET);

        string sKey = TC_Common::tostr(key);
        int iLen = sKey.length();

        string sLen = TC_Common::tostr(iLen);
        if (iLen >= 1000 || iLen <= 0)
        {
            throw BinLogException("encode error, key len =" + sLen);
        }
        int iSLenLen = sLen.length();
        sLen.append(3 - iSLenLen, ' ');

        _binLogBefTrans = _binLogBefTrans + " " + sLen + " " + sKey;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(_binLogBefTrans, _binLogAftTrans);

        return _binLogAftTrans;
    }

    /*
    *编码Binlog
    *参数:
    *	opt:	操作类型 set/del/erase/onlykey
    *	dirt:	是否是脏数据
    *	key:	key值
    *	value:	value值
    *返回:
    *	编码后的字符串
    */
    template <typename T>
    const string& Encode(const enum BinLogOpt opt, const bool dirty, const T &key, const string &value, uint32_t expireTime = 0, const enum BinLogType logType = BINLOG_NORMAL)
    {
        if (logType != BINLOG_NORMAL && logType != BINLOG_BAK && logType != BINLOG_TRANS && logType != BINLOG_SYNC && logType != BINLOG_ADMIN)
        {
            throw BinLogException("encode error, logType = " + TC_Common::tostr(logType));
        }
        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        _binLogBefTrans = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType);

        if (opt != BINLOG_SET && opt != BINLOG_DEL && opt != BINLOG_ERASE && opt != BINLOG_SET_ONLYKEY)
        {
            throw BinLogException("encode error, opt =" + TC_Common::tostr(opt));
        }

        if ((int)opt > 9)
        {
            _binLogBefTrans = _binLogBefTrans + " " + (char)('A' + ((int)opt - 10));
        }
        else
        {
            _binLogBefTrans = _binLogBefTrans + " " + TC_Common::tostr(opt);
        }

        string sDirty = dirty ? "1" : "0";
        string sExpireTime = string((char *)&expireTime, sizeof(uint32_t));

        string sKey = TC_Common::tostr(key);
        int iLen = sKey.length();

        string sLen = TC_Common::tostr(iLen);

        if (iLen >= 1000 || iLen <= 0)
        {
            throw BinLogException("encode error, key len =" + sLen);
        }

        int iSLenLen = sLen.length();
        sLen.append(3 - iSLenLen, ' ');

        _binLogBefTrans = _binLogBefTrans + " " + sDirty + " " + sExpireTime + " " + sLen + " " + sKey  + " " + value;

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(_binLogBefTrans, _binLogAftTrans);

        return _binLogAftTrans;
    }

    const string& EncodeActive()
    {
        enum BinLogType logType = BINLOG_ACTIVE;

        int64_t timestamp = tars_htonll(TC_TimeProvider::getInstance()->getNow());
        _binLogBefTrans = std::string((char*)(&timestamp), sizeof(int64_t)) + " " + TC_Common::tostr(logType);
        _binLogBefTrans += " ";
        _binLogBefTrans += "Active";

        //不进行特殊字符转义，增加头部SERA+BinlogLength
        EncodeAddHead(_binLogBefTrans, _binLogAftTrans);

        return _binLogAftTrans;
    }

    /*
    *binlog解码， sBinLog格式：时间 类型 操作标志 key长度 key值
    */
    virtual bool DecodeKey(const string &binLog)
    {
        DecodeRemoveHead(binLog, _binLogBefTrans);

        // 跳过时间戳和一个空格(8 + 1)
        int iIndex = 9;
        int iLogType = TC_Common::strto<int>(_binLogBefTrans.substr(iIndex, 2));
        if (iLogType >= (int)BINLOG_NORMAL && iLogType <= (int)BINLOG_ACTIVE)
        {
            _logType = (enum BinLogType)iLogType;
            _haveLogType = true;
            iIndex += 3;
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

        string sOpt = _binLogBefTrans.substr(iIndex, 1);
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

        iIndex += 2;

        if (_opt == BINLOG_SET)
        {
            string sKeyLen = TC_Common::trim(_binLogBefTrans.substr(iIndex, 3), " ");
            int iKeyLen = TC_Common::strto<int>(sKeyLen);
            iIndex += 4;

            _keyString = _binLogBefTrans.substr(iIndex, iKeyLen);
            return true;
        }
        else
        {
            return false;
        }
    }

    /*
    *binlog解码， sBinLog格式：时间 操作标志 脏数据标志 过期时间 key长度 key值 value
    */
    virtual void Decode(const string &binLog)
    {
        DecodeRemoveHead(binLog, _binLogBefTrans);

        // 跳过时间戳和一个空格(8 + 1)
        int iIndex = 9;
        int iLogType = TC_Common::strto<int>(_binLogBefTrans.substr(iIndex, 2));
        if (iLogType >= (int)BINLOG_NORMAL && iLogType <= (int)BINLOG_ACTIVE)
        {
            _logType = (enum BinLogType)iLogType;
            _haveLogType = true;
            iIndex += 3;
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

        string sOpt = _binLogBefTrans.substr(iIndex, 1);
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

        if (_opt != BINLOG_SET && _opt != BINLOG_DEL && _opt != BINLOG_ERASE && _opt != BINLOG_SET_ONLYKEY)
        {
            throw BinLogException("decode error, invalid opt =" + sOpt);
        }

        iIndex += 2;

        string sDirty = _binLogBefTrans.substr(iIndex, 1);
        if (sDirty != "0" && sDirty != "1")
        {
            throw BinLogException("decode error, sDirty =" + sDirty);
        }

        _isDirty = (sDirty == "1") ? true : false;
        iIndex += 2;

        _expireTime = *(uint32_t *)(_binLogBefTrans.substr(iIndex, sizeof(uint32_t)).c_str());
        iIndex += sizeof(uint32_t) + 1;

        string sKeyLen = TC_Common::trim(_binLogBefTrans.substr(iIndex, 3), " ");
        int iKeyLen = TC_Common::strto<int>(sKeyLen);
        iIndex += 4;

        _keyString = _binLogBefTrans.substr(iIndex, iKeyLen);

        if (_opt == BINLOG_SET)
        {
            iIndex += iKeyLen + 1;
            _value = _binLogBefTrans.substr(iIndex);
        }
    }

    enum BinLogType GetBinLogType() {
        return _logType;
    }

    enum BinLogOpt GetOpt() {
        return _opt;
    }

    bool GetDirty() {
        return _isDirty;
    }

    string GetStringKey() {
        return _keyString;
    }

    string GetValue() {
        return _value;
    }

    uint32_t GetExpireTime() {
        return _expireTime;
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
    //binlog
    string _binLogBefTrans;

    string _binLogAftTrans;

    //binlog类型，normal/bak/trans/sync
    enum BinLogType _logType;

    //操作类型，set/del/erase
    enum BinLogOpt _opt;

    //数据是否是脏数据
    bool _isDirty;

    //解析的binlog中是否含有logtype字段
    bool _haveLogType;

    string _keyString;
    uint32_t _expireTime;
    string _value;

};

#endif
