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
#include "CacheServer.h"

extern SHashMap g_sHashMap;

class HashmapTest : public ::testing::Test
{
  protected:
    HashmapTest() = default;
    ~HashmapTest() = default;

    void SetUp() override
    {
        _cacheConf =
        R"(<Main>
            <Cache>
                #指定共享内存使用的key
                ShmKey=12345
                #内存的大小
                ShmSize=1G
                #平均数据size，用于初始化共享内存空间
                AvgDataSize=128
                #设置hash比率(设置chunk数据块/hash项比值)
                HashRadio=2

                #内存块个数
                JmemNum=10

            </Cache>
        </Main>)";

        _tcConf.parseString(_cacheConf);
        _key = "mm123";
        _value = string(32, 's');
        _dirty = true;
    }

    void TearDown() override
    {
    }

    bool initHashmap(bool create)
    {
        key_t key;
        key = TC_Common::strto<key_t>(_tcConf.get("/Main/Cache<ShmKey>", "0"));
        if (key == 0)
        {
            cout << __FUNCTION__ << "|shmkey is 0" << endl;
            return false;
        }

        if (create)
        {
            if (shmget(key, 0, 0) != -1 || errno != ENOENT)
            {
                cout << __FUNCTION__ << "|" << key << " has been used, may be a conflict key" << endl;
                return false;
            }
        }
        else
        {
            int shmid = shmget(key, 0, 0);
            if (shmid == -1)
            {
                cout << __FUNCTION__ << "|shmkey:" << key << " not exist, errno = " << errno << endl;
                return false;
            }
            else
            {
                struct shmid_ds buf;
                if (shmctl(shmid, IPC_STAT, &buf) != 0)
                {
                    cout << __FUNCTION__ << "|shmkey:" << key << "|shmid:" << shmid << " shmctl error, errno = " << errno << endl;
                    return false;
                }
                else if (buf.shm_nattch != 0)
                {
                    cout << __FUNCTION__ << "|shmkey:" << key << "|shmid:" << shmid << " attached process is " << buf.shm_nattch << ", not 0." << endl;
                    return false;
                }
            }
        }

        size_t shmSize = TC_Common::toSize(_tcConf["/Main/Cache<ShmSize>"], 0);
        unsigned int shmNum = TC_Common::strto<unsigned int>(_tcConf.get("/Main/Cache<JmemNum>", "10"));
        g_sHashMap.init(shmNum);
        g_sHashMap.initHashRadio(atof(_tcConf["/Main/Cache<HashRadio>"].c_str()));
        g_sHashMap.initAvgDataSize(TC_Common::strto<unsigned int>(_tcConf["/Main/Cache<AvgDataSize>"]));
        g_sHashMap.initLock(key, shmNum, -1);
        cout << __FUNCTION__ << "|initLock succ" << endl;

        g_sHashMap.initStore(key, shmSize);
        cout << __FUNCTION__ << "| initStore succ" << endl;

        NormalHash *pHash = new NormalHash();
        typedef size_t (NormalHash::*TpMem)(const string &);
        TC_HashMapMalloc::hash_functor cmd(pHash, static_cast<TpMem>(&NormalHash::HashRawString));
        g_sHashMap.setHashFunctor(cmd);
        cout << __FUNCTION__ << "| setHashFunctor succ" << endl;

        g_sHashMap.setAutoErase(false);

        //g_sHashMap.setSyncTime(TC_Common::strto<unsigned int>(_tcConf["/Main/Cache<SyncTime>"]));
        //g_sHashMap.setToDoFunctor(&_todoFunctor);

        return true;
    }

    bool destroyShm()
    {
        key_t key;
        key = TC_Common::strto<key_t>(_tcConf.get("/Main/Cache<ShmKey>", "0"));
        if (key == 0)
        {
            cout << __FUNCTION__ << "|shmkey is 0" << endl;
            return false;
        }
        int shmid = shmget(key, 0, 0);
        if (shmid == -1)
        {
            cout << __FUNCTION__ << "|shmkey:" << key << " not exist, errno = " << errno << endl;
            return false;
        }
        int ret = shmctl(shmid, IPC_RMID, 0);
        if (ret != 0)
        {
            cout << __FUNCTION__ << "|shmid:" << shmid << " shmctl error, errno = " << errno << endl;
            return false;
        }

        return true;
    }

    string _cacheConf;
    TC_Config _tcConf;

    string _key;
    string _value;
    bool _dirty;
};
//Firstly, create shm
TEST_F(HashmapTest, hashmapCreate)
{
    bool succ = initHashmap(true);
    ASSERT_TRUE(succ);
}

//set and get
TEST_F(HashmapTest, set)
{
    int ret = g_sHashMap.set(_key, _value, _dirty, 0, 0);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);

    string value;
    uint32_t iSynTime, iExpireTime;
    uint8_t iVersion;
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, _value);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;
}

TEST_F(HashmapTest, setExpire)
{
    uint32_t expireTime = time(NULL) + 10;
    int ret = g_sHashMap.set(_key, _value, _dirty, expireTime, 0);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);

    string value;
    uint32_t iSynTime, iExpireTime;
    uint8_t iVersion;
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, _value);
    EXPECT_EQ(iExpireTime, expireTime);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;
    
    sleep(15);
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_DATA_EXPIRED);
}

TEST_F(HashmapTest, setWithVersion)
{
    _key += "ver";
    int ret = g_sHashMap.set(_key, _value, _dirty, 0, 0);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);

    string value;
    uint32_t iSynTime, iExpireTime;
    uint8_t iVersion;
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, _value);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;
    
    string valueNew = _value + "version";
    ret = g_sHashMap.set(_key, valueNew, _dirty, 0, iVersion + 1);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_DATA_VER_MISMATCH);
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, _value);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;

    ret = g_sHashMap.set(_key, valueNew, _dirty, 0, iVersion);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, valueNew);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;
}

TEST_F(HashmapTest, update)
{
    _key += "update";
    _value = "1";
    int ret = g_sHashMap.set(_key, _value, _dirty, 0, 0);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);

    string value;
    uint32_t iSynTime, iExpireTime;
    uint8_t iVersion;
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, _value);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;

    //ADD 1, result should be 2;
    DCache::Op option = ADD;
    string retValue;
    string upValue = "1";
    ret = g_sHashMap.update(_key, upValue, option, _dirty, 0, false, -1, retValue);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(retValue, "2");
    
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, "2");

    upValue = "xyz";
    ret = g_sHashMap.update(_key, upValue, option, _dirty, 0, false, -1, retValue);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_DATATYPE_ERR);
    
    //SUB 1, result should be 1;
    upValue = "1";
    option = SUB;
    ret = g_sHashMap.update(_key, upValue, option, _dirty, 0, false, -1, retValue);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(retValue, "1");
    
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, "1");

    upValue = "xyz";
    ret = g_sHashMap.update(_key, upValue, option, _dirty, 0, false, -1, retValue);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_DATATYPE_ERR);
    
    //APPEND 3, result should be 13;
    option = APPEND;
    upValue = "3";
    ret = g_sHashMap.update(_key, upValue, option, _dirty, 0, false, -1, retValue);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(retValue, "13");
    
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, "13");

    //PREPEND 3, result should be 313;
    option = PREPEND;
    upValue = "3";
    ret = g_sHashMap.update(_key, upValue, option, _dirty, 0, false, -1, retValue);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(retValue, "313");
    
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, "313");

    //ADD_INSERT 3, result should be 3;
    _key = "addinsert";
    option = ADD_INSERT;
    upValue = "3";
    ret = g_sHashMap.update(_key, upValue, option, _dirty, 0, false, -1, retValue);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(retValue, "3");
    
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, "3");

    //SUB_INSERT 3, result should be -3;
    _key = "subinsert";
    option = SUB_INSERT;
    upValue = "3";
    ret = g_sHashMap.update(_key, upValue, option, _dirty, 0, false, -1, retValue);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(retValue, "-3");
    
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, "-3");
}

TEST_F(HashmapTest, erase)
{
    _key += "erase";
    _value += "erase";
    int ret = g_sHashMap.set(_key, _value, _dirty, 0, 0);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);

    string value;
    uint32_t iSynTime, iExpireTime;
    uint8_t iVersion;
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, _value);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;

    ret = g_sHashMap.erase(_key, 0);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_NO_DATA);
}

TEST_F(HashmapTest, del)
{
    _key += "del";
    _value += "del";
    int ret = g_sHashMap.set(_key, _value, _dirty, 0, 0);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);

    string value;
    uint32_t iSynTime, iExpireTime;
    uint8_t iVersion;
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, _value);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;

    ret = g_sHashMap.del(_key, 0);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_NO_DATA);
}

TEST_F(HashmapTest, delWithVer)
{
    _key += "delVer";
    _value += "delVer";
    int ret = g_sHashMap.set(_key, _value, _dirty, 0, 0);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);

    string value;
    uint32_t iSynTime, iExpireTime;
    uint8_t iVersion;
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, _value);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;

    ret = g_sHashMap.del(_key, iVersion+1);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_DATA_VER_MISMATCH);
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    EXPECT_EQ(value, _value);
    cout << "key:" << _key << endl;
    cout << "value:" << value << endl;

    ret = g_sHashMap.del(_key, iVersion);
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_OK);
    ret = g_sHashMap.get(_key, value, iSynTime, iExpireTime, iVersion, true, time(NULL));
    ASSERT_EQ(ret, TC_HashMapMalloc::RT_NO_DATA);
}

//Test hashmapDestory must be the last one.
TEST_F(HashmapTest, hashmapDestory)
{
    bool succ = destroyShm();
    ASSERT_TRUE(succ);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
