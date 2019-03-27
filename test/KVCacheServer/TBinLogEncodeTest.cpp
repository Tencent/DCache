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
#include <gtest/gtest.h>
#include <unistd.h>
#include "TBinLogEncode.h"

class TBinLogEncodeTest : public ::testing::Test
{
  protected:
    TBinLogEncodeTest() = default;
    ~TBinLogEncodeTest() = default;

    void SetUp() override
    {
        _keyString = "mm123";
        _keyInterger = 12345;
        _value = string(128, 's');
        _expireTime = time(NULL) + 30;
        _dirty = true;
        _optSet = BINLOG_SET;
        _optDel = BINLOG_DEL;
        _optErase = BINLOG_ERASE;
        _optOnlykey = BINLOG_SET_ONLYKEY;
    }

    void TearDown() override
    {
    }

    string _keyString;
    int64_t _keyInterger;
    string _value;
    uint32_t _expireTime;
    bool _dirty;
    BinLogOpt _optSet;
    BinLogOpt _optDel;
    BinLogOpt _optErase;
    BinLogOpt _optOnlykey;
};

TEST_F(TBinLogEncodeTest, EncodeSetString)
{
    TBinLogEncode encode;
    string s1 = encode.Encode(_optSet, _dirty, _keyString, _value, _expireTime);
    //cout << "s1:" << s1 << endl;

    TBinLogEncode decode;
    decode.Decode(s1);
    EXPECT_EQ(BINLOG_NORMAL, decode.GetBinLogType());
    EXPECT_EQ(_optSet, decode.GetOpt());
    EXPECT_EQ(_dirty, decode.GetDirty());
    EXPECT_EQ(_keyString, decode.GetStringKey());
    EXPECT_EQ(_value, decode.GetValue());
    EXPECT_EQ(_expireTime, decode.GetExpireTime());
}

TEST_F(TBinLogEncodeTest, EncodeSetInterger)
{
    TBinLogEncode encode;
    string s1 = encode.Encode(_optSet, _dirty, _keyInterger, _value, _expireTime);
    //cout << "s1:" << s1 << endl;

    TBinLogEncode decode;
    decode.Decode(s1);
    EXPECT_EQ(BINLOG_NORMAL, decode.GetBinLogType());
    EXPECT_EQ(_optSet, decode.GetOpt());
    EXPECT_EQ(_dirty, decode.GetDirty());

    string key = std::to_string(_keyInterger);
    EXPECT_EQ(key, decode.GetStringKey());
    EXPECT_EQ(_value, decode.GetValue());
    EXPECT_EQ(_expireTime, decode.GetExpireTime());
}

TEST_F(TBinLogEncodeTest, EncodeSetKey)
{
    TBinLogEncode encode;
    string s1 = encode.EncodeSetKey(_keyString);
    //cout << "s1:" << s1 << endl;

    TBinLogEncode decode;
    bool succ = decode.DecodeKey(s1);
    EXPECT_EQ(succ, true);
    EXPECT_EQ(BINLOG_NORMAL, decode.GetBinLogType());
    EXPECT_EQ(_optSet, decode.GetOpt());
    EXPECT_EQ(_keyString, decode.GetStringKey());
}

TEST_F(TBinLogEncodeTest, EncodeErase)
{
    TBinLogEncode encode;
    string valueEmpty;
    string s1 = encode.Encode(_optErase, _dirty, _keyString, valueEmpty);
    //cout << "s1:" << s1 << endl;

    TBinLogEncode decode;
    decode.Decode(s1);
    EXPECT_EQ(BINLOG_NORMAL, decode.GetBinLogType());
    EXPECT_EQ(_optErase, decode.GetOpt());
    EXPECT_EQ(_dirty, decode.GetDirty());
    EXPECT_EQ(_keyString, decode.GetStringKey());
    EXPECT_EQ(valueEmpty, decode.GetValue());
}

TEST_F(TBinLogEncodeTest, EncodeDel)
{
    TBinLogEncode encode;
    string valueEmpty;
    string s1 = encode.Encode(_optDel, _dirty, _keyString, valueEmpty);
    //cout << "s1:" << s1 << endl;

    TBinLogEncode decode;
    decode.Decode(s1);
    EXPECT_EQ(BINLOG_NORMAL, decode.GetBinLogType());
    EXPECT_EQ(_optDel, decode.GetOpt());
    EXPECT_EQ(_dirty, decode.GetDirty());
    EXPECT_EQ(_keyString, decode.GetStringKey());
    EXPECT_EQ(valueEmpty, decode.GetValue());
}

TEST_F(TBinLogEncodeTest, EncodeOnlykey)
{
    TBinLogEncode encode;
    string valueEmpty;
    string s1 = encode.Encode(_optOnlykey, _dirty, _keyString, valueEmpty);
    //cout << "s1:" << s1 << endl;

    TBinLogEncode decode;
    decode.Decode(s1);
    EXPECT_EQ(BINLOG_NORMAL, decode.GetBinLogType());
    EXPECT_EQ(_optOnlykey, decode.GetOpt());
    EXPECT_EQ(_dirty, decode.GetDirty());
    EXPECT_EQ(_keyString, decode.GetStringKey());
    EXPECT_EQ(valueEmpty, decode.GetValue());
}

TEST(TBinLogEncode, GetTime)
{
    string s("SERA0000");
    int64_t nowTime = time(NULL); 
    int64_t timestamp = tars_htonll(nowTime);
    s += string((char*)(&timestamp), sizeof(timestamp));

    int64_t time = TBinLogEncode::GetTime(s);
    EXPECT_EQ(time, nowTime);
}

TEST(TBinLogEncode, GetTimeString)
{
    string s("SERA0000");
    int64_t nowTime = time(NULL); 
    char szBuf[64] = {0};
    strftime(szBuf, sizeof(szBuf), "%Y%m%d%H%M%S", localtime(&nowTime));
    string nowTimeString(szBuf); 

    int64_t timestamp = tars_htonll(nowTime);
    s += string((char*)(&timestamp), sizeof(timestamp));

    string decodeString("");
    TBinLogEncode::GetTimeString(s, decodeString);
    EXPECT_EQ(nowTimeString, decodeString);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
