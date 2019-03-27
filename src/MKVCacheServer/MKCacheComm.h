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
#ifndef _MKCACHECOMM_H
#define _MKCACHECOMM_H

#include "MKCacheGlobe.h"
#include "MKBinLogEncode.h"
#include "DbAccess.h"
/*
 * 封装tars编码二进制流
 */
class TarsEncode
{
public:
    TarsEncode() {}
    ~TarsEncode() {}

    const char* getBuffer()
    {
        return _osk.getBuffer();
    }

    size_t getLength()
    {
        return _osk.getLength();
    }

    /*
     * 将某字段写入tars编码
     * @param s: 某字段的值
     * @param tag: 某字段的tag编号
     * @param type: 某字段的类型
     */
    void write(const string &s, uint8_t tag, string type);

private:
    TarsOutputStream<BufferWriter> _osk;
};

/*
 * 解码tars编码二进制流
 */
class TarsDecode
{
public:
    TarsDecode() {}
    ~TarsDecode() {}
    void setBuffer(const string &sBuf)
    {
        _buf = sBuf;
    }
    string getBuffer()
    {
        return _buf;
    }

    /*
     * 将某字段从tars编码流中读出
     * @param tag: 某字段的tag编号
     * @param type: 某字段的类型
     * @param sDefault: 默认值
     * @param isRequire: 是否是必须字段
     */
    string read(uint8_t tag, const string& type, const string &sDefault, bool isRequire);

private:
    string _buf;
};

struct MKDCacheException : public std::runtime_error
{
    MKDCacheException(const std::string& s) : std::runtime_error(s) {}
};

/*
* 检验用户输入的条件（vtCond）的合法性，并对其进行分析归类
* @param vtCond: 用户输入的条件
* @param vtUKCond: vtCond中是属于联合主键的条件
* @param vtValueCond: vtCond中是属于value的条件
* @param stLimit: vtCond中是属于limit的条件
* @param bUKey: 该条件集是否是一个主键条件
* @return bool:
*          true：检验分析成功
*          false：失败
*/
bool checkCondition(const vector<DCache::Condition> &vtCond, vector<DCache::Condition> &vtUKCond, vector<DCache::Condition> &vtValueCond, Limit &stLimit, bool &bUKey, int &iRetCode);

bool checkValueCondition(const vector<DCache::Condition> &vtCond, vector<DCache::Condition> &vtValueCond, bool &bUnique, int &iRetCode);

bool checkValueCondition(const vector<DCache::Condition>& vtCond, vector<DCache::Condition>& vtValueCond, Limit &stLimit, bool& isLimit, vector<DCache::Condition>& vtScoreCond, int &iRetCode);

bool checkRecord(const DCache::Record &record, string &sMainKey, vector<DCache::Condition> &vtUKCond, int &iRetCode);

void convertFromUKCondition(const vector<Condition> &vtUKeyCond, map<string, string> &mpUKey);
/*
* insert时，检验用户输入的需要设置字段集（mpValue）的合法性，并对其进行分析归类
* @param mpValue: 用户输入的需要设置字段集
* @param mpUK: mpValue中是属于联合主键的字段
* @param mpValue: mpValue中是属于value的字段
* @param iRetCode: 返回码
* @return bool:
*          true：检验分析成功
*          false：失败
*/
bool checkSetValue(const map<string, DCache::UpdateValue> &mpValue, map<string, DCache::UpdateValue> &mpUK, map<string, DCache::UpdateValue> &mpJValue, int &iRetCode);

/*
 * insert时，检验用户输入的需要设置字段集（mpValue，不包括联合键）的合法性，并对其进行分析归类
 * @param mpValue: 用户输入的需要设置字段集
 * @param mpValue: mpValue中是属于value的字段
 * @param iRetCode: 返回码
 * @return bool:
 *          true：检验分析成功
 *          false：失败
 */
bool checkSetValue(const map<string, DCache::UpdateValue> &mpValue, map<string, DCache::UpdateValue> &mpJValue, int &iRetCode);

bool checkSetValue(const map<string, UpdateFieldInfo> &mpValue, map<string, DCache::UpdateValue> &mpUK, map<string, DCache::UpdateValue> &mpInsertJValue, map<string, DCache::UpdateValue> &mpUpdateJValue, bool insert, int &iRetCode);
/*
 * update时，检验用户输入的需要更新字段集（mpValue）的合法性
 * @param mpValue: 用户输入的需要更新字段集

 * @return bool:
 *          true：检验成功
 *          false：失败
 */
bool checkUpdateValue(const map<string, DCache::UpdateValue> &mpValue, int &iRetCode);

bool checkUpdateValue(const map<string, DCache::UpdateValue> &mpValue, bool &bOnlyScore, int &iRetCode);

bool checkMK(const string & mkey, bool isInt, int &iRetCode);

/*
 * 将联合主键的tars编码转化为DbAccess的条件集
 * @param sUK: 联合主键的tars编码
 * @param vtDbCond: 访问DbAccess的条件集
 */
void UKBinToDbCondition(const string &sUK, vector<DCache::DbCondition> &vtDbCond);

void ValueBinToDbCondition(const string &sValue, vector<DCache::DbCondition> &vtDbCond);

/*
 * 将联合主键的tars编码转化为DbAccess的条件集
 * @param sUK: 联合主键的tars编码
 * @param vtDbCond: 访问DbAccess的条件集
 */
void MKUKBinToDbCondition(const string &sMK, const string &sUK, vector<DCache::DbCondition> &vtDbCond);

/*
 * 将主key，联合主键和value的tars编码转化为DbAccess的值集
 * @param sMK: 主key的tars编码
 * @param sUK: 联合主键的tars编码
 * @param sValue: value的tars编码
 * @param vtDbCond: DbAccess的值集
 */
void BinToDbValue(const string &sMK, const string &sUK, const string &sValue, map<string, DCache::DbUpdateValue> &mpDbValue);
void BinToDbValue(const string &sMK, const string &sUK, const string &sValue, int sExpireTime, map<string, DCache::DbUpdateValue> &mpDbValue);

/*
 * 判断某字段是否满足用户输入的条件
 * @param decode: 数据记录的tars编码的decode对象实例
 * @param op: 判断的操作符
 * @param type: 需要判断字段的类型
 * @param tag: 需要判断字段的tag编号
 * @param sDefault: 需要判断字段的默认值
 * @param isRequire: 需要判断字段是否是必须存在的
 * @return bool:
 *          true：成功
 *          false：失败
 */
bool judgeValue(TarsDecode &decode, const string &value, Op op, const string &type, uint8_t tag, const string &sDefault, bool isRequire);

/*
 * 判断某数据记录是否满足用户输入的条件集
 * @param decode: 数据记录的tars编码的decode对象实例
 * @param vtCond: 用户输入的条件集
 * @return bool:
 *          true：成功
 *          false：失败
 */
bool judge(TarsDecode &decode, const vector<DCache::Condition> &vtCond);
bool judge(TarsDecode &ukDecode, TarsDecode &vDecode, const vector<vector<DCache::Condition> > &vtCond);

void convertField(const string &sField, vector<string> &vtField);
void setResult(const vector<string> &vtField, const string &mainKey, const vector<DCache::Condition> &vtUKCond, TarsDecode &vDecode, const char ver, uint32_t expireTime, vector<map<std::string, std::string> > &vtData);
void setResult(const vector<string> &vtField, const string &mainKey, TarsDecode &ukDecode, TarsDecode &vDecode, const char ver, uint32_t expireTime, vector<map<std::string, std::string> > &vtData);
void setResult(const vector<string> &vtField, const string &mainKey, TarsDecode &vDecode, const char ver, uint32_t expireTime, vector<map<std::string, std::string> > &vtData);
void setResult(const vector<string> &vtField, const string &mainKey, TarsDecode &vDecode, const char ver, uint32_t expireTime, double score, vector<map<std::string, std::string> > &vtData);

string updateValue(const map<string, DCache::UpdateValue> &mpValue, const string &sStreamValue);
string updateValueForInsert(const pair<string, UpdateFieldInfo> &upValue);
enum DCache::DataType ConvertDbType(const string &type);

void getSelectResult(const vector<string> &vtField, const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, vector<map<std::string, std::string> > &vtData);
void getSelectResult(const vector<string> &vtField, const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<vector<DCache::Condition> > & vtCond, const Limit &stLimit, vector<map<std::string, std::string> > &vtData);
int getSelectResult(const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, vector<char> &vtData);
void getSelectResult(const vector<string> &vtField, const string &mk, const vector<MultiHashMap::Value> &vtValue, vector<map<std::string, std::string> > &vtData);
int updateResult(const string &mk, const vector<MultiHashMap::Value> &vtValue, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, char ver, bool dirty, uint32_t expireTimeSecond, bool bHead, bool bUpdateOrder, const string sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog, int &iUpdateCount);
int updateResultAtom(const string &mk, const vector<MultiHashMap::Value> &vtValue, const map<std::string, DCache::UpdateValue> &mpValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, char ver, bool dirty, uint32_t expireTimeSecond, bool bHead, bool bUpdateOrder, const string sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog, int &iUpdateCount);
int delResult(const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, const string & sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog);
int DelResult(const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, const string & sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog, tars::TarsCurrentPtr current, DbAccessPrx pMKDBaccess, bool bExistDB);
int DelResult(const string &mk, const vector<MultiHashMap::Value> &vtValue, const vector<DCache::Condition> & vtUKCond, const vector<DCache::Condition> &vtValueCond, const Limit &stLimit, const string & sBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog, DbAccessPrx pMKDBaccess, bool bExistDB, int &ret);

bool judgeScore(const tars::Double& dScore, const vector<DCache::Condition> &vtCond);
tars::Double updateScore(const map<string, DCache::UpdateValue> &mpValue, const tars::Double& dScore);
int updateResult(const string &mk, const string &sOldValue, const map<std::string, DCache::UpdateValue> &mpValue, double iScore, uint32_t iOldExpireTime, uint32_t iExpireTime, char iVersion, bool bDirty, bool bOnlyScore, const string sBinLogFile, const string sKeyBinLogFile, bool bRecordBinLog, bool bKeyRecordBinLog);


const char UK_STREAM = 1;
const char VALUE_STREAM = 2;
string createEncodeStream(const map<string, string> &mpValue, const char type);

/*
 * 记录binlog日志类
 */
class WriteBinLog
{
public:
    static void set(const string &mk, const string &uk, const string &value, uint32_t expireTime, bool dirty, const string &logfile, const enum BinLogType logType = BINLOG_NORMAL);
    static void set(const string &mk, const string &uk, const string &logfile, const enum BinLogType logType = BINLOG_NORMAL);
    static void set(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, const string &logfile, bool fromDB = false, const enum BinLogType logType = BINLOG_NORMAL);
    static void erase(const string &mk, const string &logfile, const enum BinLogType logType = BINLOG_NORMAL);
    static void del(const string &mk, const string &logfile, const enum BinLogType logType = BINLOG_NORMAL);
    static void setMKOnlyKey(const string &mk, const string &logfile, const enum BinLogType logType = BINLOG_NORMAL);
    static void erase(const string &mk, const string &uk, const string &logfile, const enum BinLogType logType = BINLOG_NORMAL);
    static void del(const string &mk, const string &uk, const string &logfile, const enum BinLogType logType = BINLOG_NORMAL);

    static void pushList(const string &mainKey, const vector<pair<uint32_t, string> > &vtvalue, bool bHead, const string &logfile);
    static void pushList(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, const string &logfile, bool fromDB = false);
    static void popList(const string &mainKey, bool bHead, const string &logfile);
    static void replaceList(const string &mainKey, const string &value, long iPos, uint32_t iExpireTime, const string &logfile);
    static void trimList(const string &mainKey, long iStart, long iEnd, const string &logfile);
    static void remList(const string &mainKey, bool bHead, long iCount, const string &logfile);

    static void addSet(const string &mk, const string &value, uint32_t expireTime, bool dirty, const string &logfile);
    static void addSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, const string &logfile, bool fromDB = false);
    static void addSet(const string &mk, const string &logfile);
    static void delSet(const string &mk, const string &value, const string &logfile);
    static void delSet(const string &mk, const string &logfile);

    static void addZSet(const string & mk, const string &value, double score, uint32_t iExpireTime, bool bDirty, const string &logfile);
    static void addZSet(const string &mk, const vector<MultiHashMap::Value> &vs, bool full, const string &logfile, bool fromDB = false);
    static void addZSet(const string &mk, const string &logfile);
    static void incScoreZSet(const string & mk, const string &value, double score, uint32_t iExpireTime, bool bDirty, const string &logfile);
    static void delZSet(const string &mk, const string &value, const string &logfile);
    static void delZSet(const string &mk, const string &logfile);
    static void delRangeZSet(const string &mk, double iMin, double iMax, const string &logfile);
    static void updateZSet(const string &mk, const string &sOldValue, const string &sNewValue, double score, uint32_t iOldExpireTime, uint32_t iExpireTime, bool bDirty, const string &logfile);
    static int createBinLogFile(const string &path, bool isKeyBinLog);
    static void writeToFile(const string &content, const string &logFile);
private:
    //写binlog文件的文件描述符和锁
    static int _binlogFD;
    static int _keyBinlogFD;
    static TC_ThreadLock _binlogLock;
};

/*
 * 格式化日志类
 */
class FormatLog
{
public:
    static string tostr(const vector<DCache::Condition> & vtCond);

    static string tostr(const map<std::string, DCache::UpdateValue> & mpValue);

    static string tostr(const vector<DCache::Condition> &vtUKCond, const vector<DCache::Condition> &vtValueCond);

    static string tostr(const vector<DCache::DbCondition> & vtDbCond);

    static string tostr(const map<std::string, DCache::DbUpdateValue> & mpDbValue);

    static string tostr(const map<string, DCache::UpdateFieldInfo>  & mpDbValue);

    static string tostr(enum Op op);

    static string tostr(const string &str, const bool bUkey = true);

    static string tostr(const vector<InsertKeyValue> & vtKeyValue);

    static string tostr(const vector<UpdateKeyValue> & vtKeyValue);

    static string tostr(const vector<MainKeyCondition> & vtKey);
};

#endif
