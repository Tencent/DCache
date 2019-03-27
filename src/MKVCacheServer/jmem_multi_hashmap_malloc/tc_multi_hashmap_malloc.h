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
#ifndef	__TC_MULTI_HASHMAP_MALLOC_H__
#define __TC_MULTI_HASHMAP_MALLOC_H__

#include <vector>
#include <list>
#include <map>
#include <memory>
#include <cassert>
#include <iostream>
#include "util/tc_ex.h"
#include "util/tc_mem_vector.h"
#include "util/tc_pack.h"
#include "CacheShare.h"
#include "tc_malloc_chunk.h"
#include "tc_functor.h"
#include "tc_hash_fun.h"


namespace tars
{
    /////////////////////////////////////////////////
    // 说明: 支持多key的hashmap类
    // Author : smitchzhao@tencent.com              
    // based on TC_Multi_HashMap by jarodruan@tencent.com & joeyzhong@tencent.com & hendysu@tencent.com
    /////////////////////////////////////////////////

    /**
    * Multi Hash map异常类
    */
    struct TC_Multi_HashMap_Malloc_Exception : public TC_Exception
    {
        TC_Multi_HashMap_Malloc_Exception(const string &buffer) : TC_Exception(buffer) {};
        TC_Multi_HashMap_Malloc_Exception(const string &buffer, int err) : TC_Exception(buffer, err) {};
        ~TC_Multi_HashMap_Malloc_Exception() throw() {};
    };

    ////////////////////////////////////////////////////////////////////////////////////
    /**
     * 基于内存的支持多key的hashmap
     * 所有操作需要自己加锁
     * 所有存储的地址均采用32位保存，为内存块的索引，要求总内存块数不能超过32位范围
     */
    class TC_Multi_HashMap_Malloc
    {
    public:

        struct FieldInfo
        {
            uint8_t tag;
            string type;
            bool bRequire;
            string defValue;
            int lengthInDB;
        };

        struct FieldConf
        {
            string sMKeyName;
            vector<string> vtUKeyName;
            vector<string> vtValueName;
            map<string, int> mpFieldType;//用于快速查找字段类型，0表示主key，1表示联合key，2表示value字段
            map<string, FieldInfo> mpFieldInfo;
        };

        struct HashMapIterator;
        struct HashMapLockIterator;
        struct MKHashMapIterator;
        // 内存块信息
        struct MemChunk
        {
            uint32_t _iAddr;  			// 内存块地址，相对地址，内存块索引
            size_t 	 _iSize;			// 内存块大小
        };

        // 指定删除标记位值
        enum DELETETYPE
        {
            DELETE_AUTO = 0,		// 保持原有的删除标记
            DELETE_FALSE,			// 设删除标记为false
            DELETE_TRUE,             //设删除标记为true
        };

        // 真正存储的数据结构
        struct BlockData
        {
            string			_key;       // 数据Key，联合key(联合主键去掉主key后)
            string			_value;     // 数据value
            double          _score;     // 分值，用于ZSet
            uint8_t         _keyType;   //数据类型
            DELETETYPE   _isDelete;  // 是否已经删除
            bool			_dirty;     // 是否是脏数据
            uint8_t			_iVersion;	// 数据版本，1为初始版本，0为保留
            uint32_t		_synct;     // sync time, 不一定是真正的回写时间，如果已删除，则为删除时间
            uint32_t		_expire;	// 过期时间
            BlockData()
                : _score(0)
                , _keyType(MainKey::HASH_TYPE)
                , _isDelete(DELETE_FALSE)
                , _dirty(false)
                , _iVersion(1)
                , _synct(0)
                , _expire(0)
            {
            }
        };

        // 真正存储的数据结构
        struct BlockPackData
        {
            string			_keyValue;  // 数据Key，联合key(联合主键去掉主key后)和value的打包值
            bool            _isDelete;  // 是否已经被删除
            bool			_dirty;     // 是否是脏数据
            uint8_t			_iVersion;	// 数据版本，1为初始版本，0为保留
            uint32_t		_synct;     // sync time, 不一定是真正的回写时间，如果已被删除，则为删除时间
            uint32_t		_expire;	// 过期时间
            BlockPackData()
                : _isDelete(false)
                , _dirty(false)
                , _iVersion(1)
                , _synct(0)
                , _expire(0)
            {
            }
        };

        // 完整的数据结构，作为get的返回值
        struct Value
        {
            string			_mkey;			// 主key
            // 以下字段为BlockData结构，为了能给外层直接使用，重新定义
            string			_ukey;			// 联合key
            string			_value;			// 数据值
            double          _score;        // 分值，用于ZSet
            uint8_t         _keyType;       // 数据类型
            DELETETYPE      _isDelete;      // 是否设为删除
            bool			_dirty;			// 是否为脏数
            uint8_t			_iVersion;		// 取值范围为1-255，0为特殊值，表示不检查版本。循环使用
            uint32_t		_iSyncTime;		// 上次回写时间，如果已经被删除，则为删除时间
            uint32_t		_iExpireTime;	// 数据过期时间，0表示不关注

            Value()
                : _score(0)
                , _keyType(MainKey::HASH_TYPE)
                , _isDelete(DELETE_FALSE)
                , _dirty(false)
                , _iVersion(1)
                , _iSyncTime(0)
                , _iExpireTime(0)
            {
            }

            Value(const string &mkey, const BlockData &data)
                : _mkey(mkey)
                , _ukey(data._key)
                , _value(data._value)
                , _score(data._score)
                , _keyType(data._keyType)
                , _isDelete(data._isDelete)
                , _dirty(data._dirty)
                , _iVersion(data._iVersion)
                , _iSyncTime(data._synct)
                , _iExpireTime(data._expire)
            {
            }

            // 赋值
            void assign(const string &mkey, const BlockData &data)
            {
                _mkey = mkey;
                _ukey = data._key;
                _value = data._value;
                _score = data._score;
                _keyType = data._keyType;
                _isDelete = data._isDelete;
                _dirty = data._dirty;
                _iVersion = data._iVersion;
                _iSyncTime = data._synct;
                _iExpireTime = data._expire;
            }

            void assign(const BlockData &data)
            {
                _ukey = data._key;
                _value = data._value;
                _score = data._score;
                _keyType = data._keyType;
                _isDelete = data._isDelete;
                _dirty = data._dirty;
                _iVersion = data._iVersion;
                _iSyncTime = data._synct;
                _iExpireTime = data._expire;
            }
        };

        // 完整的数据结构(不同于Value的是，ukey和value是经过TC_Pack打包的)，作为get的返回值
        struct PackValue
        {
            string			_mkey;			// 主key
            // 以下字段为BlockPackData结构，为了能给外层直接使用，重新定义
            string			_ukValue;		// 联合key和value的打包值
            bool            _isDelete;      // 是否已经被删除
            bool			_dirty;			// 是否为脏数
            uint8_t			_iVersion;		// 取值范围为1-255，0为特殊值，表示不检查版本。循环使用
            uint32_t		_iSyncTime;		// 上次回写时间，如果已被删除，则为删除时间
            uint32_t		_iExpireTime;	// 数据过期时间，0表示不关注

            PackValue()
                : _isDelete(false)
                , _dirty(false)
                , _iVersion(1)
                , _iSyncTime(0)
                , _iExpireTime(0)
            {
            }

            PackValue(const string &mkey, const BlockPackData &data)
                : _mkey(mkey)
                , _ukValue(data._keyValue)
                , _isDelete(data._isDelete)
                , _dirty(data._dirty)
                , _iVersion(data._iVersion)
                , _iSyncTime(data._synct)
                , _iExpireTime(data._expire)
            {
            }

            // 赋值
            void assign(const string &mkey, const BlockPackData &data)
            {
                _mkey = mkey;
                _ukValue = data._keyValue;
                _isDelete = data._isDelete;
                _dirty = data._dirty;
                _iVersion = data._iVersion;
                _iSyncTime = data._synct;
                _iExpireTime = data._expire;
            }

            void assign(const BlockPackData &data)
            {
                _ukValue = data._keyValue;
                _isDelete = data._isDelete;
                _dirty = data._dirty;
                _iVersion = data._iVersion;
                _iSyncTime = data._synct;
                _iExpireTime = data._expire;
            }
        };

        // 数据过期时间结构
        struct ExpireTime
        {
            string		_mkey;
            string		_ukey;
            uint32_t	_iExpireTime;
        };

        // 数据标记为已删除的数据
        struct DeleteData
        {
            string		_mkey;
            string		_ukey;
            uint32_t	_iDeleteTime;
        };

        /**
        * 判断某个标志位是否已经设置
        * @param bitset, 要判断的字节
        * @param bw, 要判断的位
        */
        static bool ISSET(uint8_t bitset, uint8_t bw) { return bool((bitset & (0x01 << bw)) >> bw); }

        /*
        * 设置某个标志位
        */
        static void SET(uint8_t &iBitset, uint8_t bw)
        {
            iBitset |= 0x01 << bw;
        }

        /*
        * 清空某个标志位
        */
        static void UNSET(uint8_t &iBitset, uint8_t bw)
        {
            iBitset &= 0xFF ^ (0x01 << bw);
        }

        struct Node
        {
            virtual ~Node() {};

            virtual uint8_t getLevel() = 0;


            virtual uint32_t getNext(int n) = 0;


            virtual int setNext(int n, uint32_t addr) = 0;


            virtual uint32_t getSpan(int n) = 0;


            virtual int setSpan(int n, uint32_t iSpan) = 0;


            virtual double getScore() = 0;


            virtual int setScore(double score) = 0;
        };

        //////////////////////////////////////////////////////////////////////////
        /**
        * 主key数据块
        */
        class MainKey : public Node
        {
        public:
            /**
            * 头部中bitwise位的意义
            */
            enum BITWISE
            {
                NEXTCHUNK_BIT = 0,		// 是否有下一个chunk
                INTEGRITY_BIT,			// 主key下的数据是否完整
            };

            enum KEYTYPE
            {
                HASH_TYPE = 0,          //hash类型
                SET_TYPE,               //set类型
                ZSET_TYPE,              //排序set类型
                LIST_TYPE,              //list类型
            };

            // 主key头
            struct tagMainKeyHead
            {
                uint32_t	_iSize;         // 容量大小
                uint32_t	_iIndex;		// 主key hash索引
                uint32_t	_iBlockHead;	// 主key下数据链首地址
                uint32_t	_iBlockTail;	// 主key下数据链的尾地址
                uint32_t	_iNext;			// 主key链的下一个主key
                uint32_t	_iPrev;			// 主key链的上一个主key
                uint32_t	_iGetNext;		// 主key Get链的下一个主key
                uint32_t	_iGetPrev;		// 主key Get链的上一个主key
                uint32_t	_iBlockCount;	// 主key下数据个数
                uint8_t		_iBitset;		// 8个bit，用于标识不同的bool值，各bit的含义见BITWISE枚举定义
                union
                {
                    uint32_t	_iNextChunk;    // 下一个Chunk块地址, _bNextChunk=true时有效
                    uint32_t	_iDataLen;      // 当前数据块中使用了的长度, _bNextChunk=false时有效
                };
                char			_cData[0];      // 数据开始地址
                tagMainKeyHead()
                    : _iSize(0)
                    , _iIndex(0)
                    , _iBlockHead(0)
                    , _iBlockTail(0)
                    , _iNext(0)
                    , _iPrev(0)
                    , _iGetNext(0)
                    , _iGetPrev(0)
                    , _iBlockCount(0)
                    , _iBitset(0)
                    , _iDataLen(0)
                {
                    _cData[0] = 0;
                }
            }__attribute__((packed));

            /**
            * 一个chunk放不下数据，后面挂接其它chunk
            * 非第一个chunk的chunk头部
            */
            struct tagChunkHead
            {
                uint32_t    _iSize;         // 当前chunk的容量大小
                bool        _bNextChunk;    // 是否还有下一个chunk
                union
                {
                    uint32_t  _iNextChunk;    // 下一个数据块地址, _bNextChunk=true时有效
                    uint32_t  _iDataLen;      // 当前数据块中使用了的长度, _bNextChunk=false时有效
                };
                char        _cData[0];      // 数据开始地址

                tagChunkHead()
                    :_iSize(0)
                    , _bNextChunk(false)
                    , _iDataLen(0)
                {
                    _cData[0] = 0;
                }

            }__attribute__((packed));

            /**
             * 构造函数
             * @param pMap
             * @param iAddr, 主key的地址
             */
            MainKey(TC_Multi_HashMap_Malloc *pMap, uint32_t iAddr)
                : _pMap(pMap)
                , _iHead(iAddr)
            {
                // 构造时保存绝对地址，避免每次都计算
                _pHead = _pMap->getMainKeyAbsolute(iAddr);
            }

            /**
             * 拷贝构造函数
             * @param mk
             */
            MainKey(const MainKey &mk)
                : _pMap(mk._pMap)
                , _iHead(mk._iHead)
                , _pHead(mk._pHead)
            {
            }

            /**
             * 赋值操作符
             * @param mk
             *
             * @return Block&
             */
            MainKey& operator=(const MainKey &mk)
            {
                _iHead = mk._iHead;
                _pMap = mk._pMap;
                _pHead = mk._pHead;
                return (*this);
            }

            /**
             *
             * @param mb
             *
             * @return bool
             */
            bool operator==(const MainKey &mk) const { return _iHead == mk._iHead && _pMap == mk._pMap; }

            /**
             *
             * @param mb
             *
             * @return bool
             */
            bool operator!=(const MainKey &mk) const { return _iHead != mk._iHead || _pMap != mk._pMap; }

            int getDataByPos(char *ptr, int len, uint32_t pos);

            //int setDataByPos(char *ptr, int len, uint32_t pos);

            int setDataByPos(uint32_t data, uint32_t pos);

            uint8_t getLevel()
            {
                uint8_t level;
                assert(getDataByPos((char *)&level, 1, 0) == TC_Multi_HashMap_Malloc::RT_OK);

                return level;
            }

            uint32_t getNext(int n)
            {
                Block::NodeLevel node;
                assert(getDataByPos((char *)&node, sizeof(Block::NodeLevel), 1 + sizeof(Block::NodeLevel) * n) == TC_Multi_HashMap_Malloc::RT_OK);

                return node.next;
            }

            int setNext(int n, uint32_t addr)
            {
                Block::NodeLevel node;
                return setDataByPos(addr, 1 + sizeof(Block::NodeLevel) * n);
            }

            uint32_t getSpan(int n)
            {
                Block::NodeLevel node;
                assert(getDataByPos((char *)&node, sizeof(Block::NodeLevel), 1 + sizeof(Block::NodeLevel) * n) == TC_Multi_HashMap_Malloc::RT_OK);

                return node.iSpan;
            }

            int setSpan(int n, uint32_t iSpan)
            {
                Block::NodeLevel node;
                return setDataByPos(iSpan, 1 + sizeof(Block::NodeLevel) * n + 4);
            }

            double getScore()
            {
                return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;
            }

            int setScore(double score)
            {
                return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;
            }

            int getData(string &data)
            {
                string tmp;
                int ret = get(tmp);
                if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                    return ret;

                int dataPos = 1 + sizeof(Block::NodeLevel) * getLevel();
                data.assign(tmp.c_str() + dataPos, tmp.size() - dataPos);

                return RT_OK;
            }

            /**
             * 获取主key头指针
             *
             * @return tagMainKeyHead*
             */
            tagMainKeyHead* getHeadPtr() { return (tagMainKeyHead *)_pHead; }


            /**
             * 根据主key头地址获取主key头指针
             * @param iAddr, 主key头地址
             * @return tagMainKeyHead*
             */
            tagMainKeyHead* getHeadPtr(uint32_t iAddr) { return ((tagMainKeyHead*)_pMap->getMainKeyAbsolute(iAddr)); }

            /**
             * 获取主key头的地址
             *
             * @return uint32_t
             */
            uint32_t getHead() { return _iHead; }

            /**
             * 获取主key
             * @param mk, 主key
             * @return int
             *          TC_Multi_HashMap_Malloc::RT_OK, 正常
             *          其他异常
             */
            int get(string &mk);

            /**
             * 设置主key
             * @param pData
             * @param iDatalen
             * @param vtData, 淘汰的数据
             */
            int set(const void *pData, uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 将当前主key移动到主key链上的下一个主key
             * @return true, 移到下一个主key了, false, 没有下一个主key
             *
             */
            bool next();

            /**
             * 将当前主key移动到主key链上的上一个主key
             * @return true, 移到上一个主key了, false, 没有上一个主key
             *
             */
            bool prev();

            /**
             * 释放所有空间
             */
            void deallocate();

            /**
             * 新主key时调用该函数，初始化主key相关信息
             * @param iIndex, 主key hash索引
             */
            void makeNew(uint32_t iIndex);

            /**
             * 从主key链表中删除当前主key下的所有未被标记为删除的数据
             * 返回被删除的主key下的数据
             * @return int, TC_Multi_HashMap_Malloc::RT_OK成功，其它失败
             */
            int erase(vector<Value> &vtData);

            /**
             * 刷新get链表, 将当前主key放在get链表头部
             */
            void refreshGetList();

        protected:

            /**
             * 获取chunk头指针
             * @param iAddr, chunk头地址索引
             *
             * @return tagChunkHead*
             */
            tagChunkHead *getChunkHead(uint32_t iAddr) { return ((tagChunkHead*)_pMap->getMainKeyAbsolute(iAddr)); }

            /**
             * 如果数据容量不够, 则新增加chunk, 不影响原有数据
             * 使新增加的总容量大于iDataLen
             * 释放多余的chunk
             * @param iDataLen
             * @param vtData, 返回被淘汰的数据
             *
             * @return int
             */
            int allocate(uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 挂接chunk, 如果core则挂接失败, 保证内存块还可以用
             * @param pChunk, 第一个chunk指针
             * @param chunks, 所有的chunk地址
             *
             * @return int
             */
            int joinChunk(tagChunkHead *pChunk, const vector<uint32_t>& chunks);

            /**
             * 分配指定大小的内存空间，可能会有多个chunk
             * @param fn, 分配的空间大小
             * @param chunks, 分配成功返回的chunks地址列表
             * @param vtData, 淘汰的数据
             * @return int
             */
            int allocateChunk(uint32_t fn, vector<uint32_t> &chunks, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 释放指定chunk之后的所有chunk
             * @param iChunk 释放地址
             */
            void deallocate(uint32_t iChunk);

            /**
             * 获取主key存储空间大小
             *
             * @return uint32_t
             */
            uint32_t getDataLen();

            /**
             * 获取主key
             * @param pData
             * @param iDatalen
             * @return int,
             *          TC_Multi_HashMap_Malloc::RT_OK, 正常
             *          其他异常
             */
            int get(void *pData, uint32_t &iDataLen);

        public:
            bool ISFULLDATA() { return TC_Multi_HashMap_Malloc::ISSET(getHeadPtr()->_iBitset, INTEGRITY_BIT); }
        protected:
            bool HASNEXTCHUNK() { return TC_Multi_HashMap_Malloc::ISSET(getHeadPtr()->_iBitset, NEXTCHUNK_BIT); }

        public:
            void SETFULLDATA(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getHeadPtr()->_iBitset, INTEGRITY_BIT); else TC_Multi_HashMap_Malloc::UNSET(getHeadPtr()->_iBitset, INTEGRITY_BIT); }
        protected:
            void SETNEXTCHUNK(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getHeadPtr()->_iBitset, NEXTCHUNK_BIT); else TC_Multi_HashMap_Malloc::UNSET(getHeadPtr()->_iBitset, NEXTCHUNK_BIT); }

        private:

            /**
             * Map
             */
            TC_Multi_HashMap_Malloc *_pMap;

            /**
             * 主key头首地址, 相对地址，内存块索引
             */
            uint32_t				_iHead;

            /**
            * 主key头首地址，绝对地址
            */
            void					*_pHead;

        };

        ///////////////////////////////////////////////////////////////////////////////////
        /**
        * 联合主key及数据块
        */
        class Block : public Node
        {
        public:

            /**
            * block头部中bitwise位的意义
            */
            enum BITWISE
            {
                NEXTCHUNK_BIT = 0,		// 是否有下一个chunk
                DIRTY_BIT,				// 是否为脏数据位
                ONLYKEY_BIT,			// 是否为OnlyKey
                DELETE_BIT,             //是否为已删除标志位
            };

            /**
             * block数据头
             */
            struct tagBlockHead
            {
                uint32_t		_iSize;         // block的容量大小
                uint32_t		_iIndex;        // hash的索引
                uint32_t		_iUKBlockNext;  // 联合主键block链的下一个Block, 没有则为0
                uint32_t		_iUKBlockPrev;  // 联合主键block链的上一个Block, 没有则为0
                uint32_t		_iMKBlockNext;  // 主key block链的下一个Block, 没有则为0
                uint32_t		_iMKBlockPrev;  // 主key block链的上一个Block, 没有则为0
                uint32_t		_iSetNext;      // Set链上的上一个Block, 没有则为0
                uint32_t		_iSetPrev;      // Set链上的上一个Block, 没有则为0
                uint32_t		_iMainKey;		// 指向所属主key头
                uint32_t		_iSyncTime;     // 上次缓写时间
                uint32_t		_iExpireTime;	// 数据过期的绝对时间，由设置或更新数据时提供，0表示不关心此时间
                uint8_t			_iVersion;		// 数据版本，1为初始版本，0为保留
                uint8_t			_iBitset;		// 8个bit，用于标识不同的bool值，各bit的含义见BITWISE枚举定义
                union
                {
                    uint32_t	_iNextChunk;    // 下一个Chunk块, _iBitwise中的NEXTCHUNK_BIT为1时有效
                    uint32_t	_iDataLen;      // 当前数据块中使用了的长度, _iBitwise中的NEXTCHUNK_BIT为0时有效
                };
                char			_cData[0];      // 数据开始部分

                tagBlockHead()
                    :_iSize(0)
                    , _iIndex(0)
                    , _iUKBlockNext(0)
                    , _iUKBlockPrev(0)
                    , _iMKBlockNext(0)
                    , _iMKBlockPrev(0)
                    , _iSetNext(0)
                    , _iSetPrev(0)
                    , _iMainKey(0)
                    , _iSyncTime(0)
                    , _iExpireTime(0)
                    , _iVersion(1)
                    , _iBitset(0)
                    , _iDataLen(0)
                {
                    _cData[0] = 0;
                }

            }__attribute__((packed));

            struct tagListBlockHead
            {
                uint32_t		_iSize;         // block的容量大小
                uint32_t		_iMKBlockNext;  // 主key block链的下一个Block, 没有则为0
                uint32_t		_iMKBlockPrev;  // 主key block链的上一个Block, 没有则为0
                uint32_t		_iMainKey;		// 指向所属主key头
                uint32_t		_iExpireTime;   //
                uint8_t			_iBitset;		// 8个bit，用于标识不同的bool值，各bit的含义见BITWISE枚举定义
                union
                {
                    uint32_t	_iNextChunk;    // 下一个Chunk块, _iBitwise中的NEXTCHUNK_BIT为1时有效
                    uint32_t	_iDataLen;      // 当前数据块中使用了的长度, _iBitwise中的NEXTCHUNK_BIT为0时有效
                };
                char			_cData[0];      // 数据开始部分

                tagListBlockHead()
                    :_iSize(0)
                    , _iMKBlockNext(0)
                    , _iMKBlockPrev(0)
                    , _iMainKey(0)
                    , _iExpireTime(0)
                    , _iBitset(0)
                    , _iDataLen(0)
                {
                    _cData[0] = 0;
                }

            }__attribute__((packed));

            struct NodeLevel
            {
                uint32_t next;
                uint32_t iSpan;

                NodeLevel()
                    :next(0)
                    , iSpan(0)
                {
                }

            }__attribute__((packed));

            /**
             * 一个chunk放不下所有数据，后面挂接其它chunk
             * 其它chunk头部
             */
            struct tagChunkHead
            {
                uint32_t    _iSize;         // 当前chunk的容量大小
                bool        _bNextChunk;    // 是否还有下一个chunk
                union
                {
                    uint32_t  _iNextChunk;    // 下一个chunk, _bNextChunk=true时有效
                    uint32_t  _iDataLen;      // 当前chunk中使用了的长度, _bNextChunk=false时有效
                };
                char        _cData[0];      // 数据开始部分

                tagChunkHead()
                    :_iSize(0)
                    , _bNextChunk(false)
                    , _iDataLen(0)
                {
                    _cData[0] = 0;
                }

            }__attribute__((packed));

            /**
             * 构造函数
             * @param Map
             * @param Block的地址
             * @param pAdd
             */
            Block(TC_Multi_HashMap_Malloc *pMap, uint32_t iAddr)
                : _pMap(pMap)
                , _iHead(iAddr)
            {
                // 构造时保存绝对地址，避免每次都计算
                _pHead = _pMap->getAbsolute(iAddr);
            }

            /**
             * 拷贝构造函数
             * @param mb
             */
            Block(const Block &mb)
                : _pMap(mb._pMap)
                , _iHead(mb._iHead)
                , _pHead(mb._pHead)
            {
            }

            /**
             * 赋值运算符
             * @param mb
             *
             * @return Block&
             */
            Block& operator=(const Block &mb)
            {
                _iHead = mb._iHead;
                _pMap = mb._pMap;
                _pHead = mb._pHead;
                return (*this);
            }

            /**
             *
             * @param mb
             *
             * @return bool
             */
            bool operator==(const Block &mb) const { return _iHead == mb._iHead && _pMap == mb._pMap; }

            /**
             *
             * @param mb
             *
             * @return bool
             */
            bool operator!=(const Block &mb) const { return _iHead != mb._iHead || _pMap != mb._pMap; }

            int getDataByPos(char *ptr, int len, uint32_t pos);

            //int setDataByPos(char *ptr, int len, uint32_t pos);

            int setDataByPos(uint32_t data, uint32_t pos);

            uint8_t getLevel()
            {
                uint8_t level;
                assert(getDataByPos((char *)&level, 1, 0) == TC_Multi_HashMap_Malloc::RT_OK);

                return level;
            }

            uint32_t getNext(int n)
            {
                Block::NodeLevel node;
                assert(getDataByPos((char *)&node, sizeof(Block::NodeLevel), 1 + sizeof(Block::NodeLevel) * n) == TC_Multi_HashMap_Malloc::RT_OK);

                return node.next;
            }

            int setNext(int n, uint32_t addr)
            {
                Block::NodeLevel node;
                return setDataByPos(addr, 1 + sizeof(Block::NodeLevel) * n);
            }

            uint32_t getSpan(int n)
            {
                Block::NodeLevel node;
                assert(getDataByPos((char *)&node, sizeof(Block::NodeLevel), 1 + sizeof(Block::NodeLevel) * n) == TC_Multi_HashMap_Malloc::RT_OK);

                return node.iSpan;
            }

            int setSpan(int n, uint32_t iSpan)
            {
                Block::NodeLevel node;
                return setDataByPos(iSpan, 1 + sizeof(Block::NodeLevel) * n + 4);
            }

            double getScore()
            {
                double score;
                assert(getDataByPos((char *)&score, sizeof(double), 1 + sizeof(Block::NodeLevel) * getLevel()) == TC_Multi_HashMap_Malloc::RT_OK);

                return score;
            }

            int setScore(double score)
            {
                uint32_t *pData = (uint32_t*)&score;
                uint32_t iData = *pData;

                int ret = setDataByPos(iData, 1 + sizeof(Block::NodeLevel) * getLevel());
                if (TC_Multi_HashMap_Malloc::RT_OK != ret)
                    return ret;

                iData = *(uint32_t*)(((char *)&score) + 4);

                return setDataByPos(iData, 1 + sizeof(Block::NodeLevel) * getLevel() + 4);
            }

            int getData(string &data)
            {
                string tmp;
                int ret = get(tmp);
                if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                    return ret;

                int dataPos = 1 + sizeof(Block::NodeLevel) * getLevel() + sizeof(double);
                data.assign(tmp.c_str() + dataPos, tmp.size() - dataPos);

                return RT_OK;
            }

            /**
             * 获取Block头指针
             *
             * @return tagBlockHead*
             */
            tagBlockHead *getBlockHead() { return (tagBlockHead*)_pHead; }

            tagListBlockHead *getListBlockHead() { return (tagListBlockHead*)_pHead; }

            /**
             * 根据头地址获取Block头指针
             * @param iAddr, block头地址
             * @return tagBlockHead*
             */
            tagBlockHead *getBlockHead(uint32_t iAddr) { return ((tagBlockHead*)_pMap->getAbsolute(iAddr)); }

            tagListBlockHead *getListBlockHead(uint32_t iAddr) { return ((tagListBlockHead*)_pMap->getAbsolute(iAddr)); }

            /**
             * 获取头部地址
             *
             * @return size_t
             */
            uint32_t getHead() { return _iHead; }

            /**
             * 获取当前桶链表最后一个block的头部地址
             * @return uint32_t
             */
            uint32_t getLastBlockHead();

            /**
            * 获取数据的过期时间
            *
            * @return uint32_t，单位为秒，返回0表示无过期时间
            */
            uint32_t getExpireTime() { return getBlockHead()->_iExpireTime; }

            uint32_t getListExpireTime() { return getListBlockHead()->_iExpireTime; }

            /**
            * 设置数据的过期时间
            * @param iExpireTime，过期绝对时间，单位为秒
            */
            void setExpireTime(uint32_t iExpireTime)
            {
                if (iExpireTime != 0)
                {
                    getBlockHead()->_iExpireTime = iExpireTime;
                }
            }

            /**
             * 获取回写时间
             *
             * @return uint32_t
             */
            uint32_t getSyncTime() { return getBlockHead()->_iSyncTime; }

            /**
             * 设置回写时间
             * @param iSyncTime
             */
            void setSyncTime(uint32_t iSyncTime) { getBlockHead()->_iSyncTime = iSyncTime; }

            /**
            * 获取数据版本
            */
            uint8_t getVersion() { return getBlockHead()->_iVersion; }

            /**
            * 设置数据版本
            */
            void setVersion(uint8_t iVersion) { getBlockHead()->_iVersion = iVersion; }

            /**
             * 获取Block中的数据
             * @param data
             * @return int
             *          TC_Multi_HashMap_Malloc::RT_OK, 正常, 其他异常
             *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY, 只有Key
             *          其他异常
             */
            int getBlockData(TC_Multi_HashMap_Malloc::BlockData &data);

            int getListBlockData(TC_Multi_HashMap_Malloc::BlockData &data);

            int getSetBlockData(TC_Multi_HashMap_Malloc::BlockData &data);

            int getZSetBlockData(TC_Multi_HashMap_Malloc::BlockData &data);

            /**
            * 获取Block中的数据
            * @param data
            * @return int
            *          TC_Multi_HashMap_Malloc::RT_OK, 正常, 其他异常
            *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY, 只有Key
            *          其他异常
            */
            int getBlockData(TC_Multi_HashMap_Malloc::BlockPackData &data);

            /**
             * 获取原始数据
             * @param pData
             * @param iDatalen
             * @return int,
             *          TC_Multi_HashMap_Malloc::RT_OK, 正常
             *          其他异常
             */
            int get(void *pData, uint32_t &iDataLen);

            int getList(void *pData, uint32_t &iDataLen);

            /**
             * 获取原始数据
             * @param s
             * @return int
             *          TC_Multi_HashMap_Malloc::RT_OK, 正常
             *          其他异常
             */
            int get(string &s);

            int getList(string &s);

            /**
             * 设置数据
             * @param pData
             * @param iDatalen
             * @param bOnlyKey
             * @param iExpiretime, 数据过期时间，0表示不关心
             * @param iVersion, 数据版本，应该根据get出的数据版本写回，为0表示不关心数据版本
             * @param vtData, 淘汰的数据
             * @return int
             *				RT_OK, 设置成功
             *				RT_DATA_VER_MISMATCH, 要设置的数据版本与当前版本不符，应该重新get后再set
             *				其它为失败
             */
            int set(const void *pData, uint32_t iDataLen, bool bOnlyKey, uint32_t iExpireTime,
                uint8_t iVersion, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 设置数据
             * @param pData
             * @param iDatalen
             * @param bOnlyKey
             * @param iExpiretime, 数据过期时间，0表示不关心
             * @param iVersion, 数据版本，应该根据get出的数据版本写回，为0表示不关心数据版本
             * @param bCheckExpire, 是否开启过期
             * @param iNowTime, 当前时间
             * @param vtData, 淘汰的数据
             * @return int
             *				RT_OK, 设置成功
             *				RT_DATA_VER_MISMATCH, 要设置的数据版本与当前版本不符，应该重新get后再set
             *				其它为失败
             */
            int set(const void *pData, uint32_t iDataLen, bool bOnlyKey, uint32_t iExpireTime,
                uint8_t iVersion, bool bCheckExpire, uint32_t iNowTime, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            int set(const void *pData, uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            int setList(const void *pData, uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 是否是脏数据
             *
             * @return bool
             */
            bool isDirty() { return ISDIRTY(); }

            /**
             * 设置数据
             * @param b
             */
            void setDirty(bool b);

            /**
             * 是否已删除
             *
             * @return bool
             */
            bool isDelete() { return ISDELETE(); }

            /**
             * 设置数据
             * @param b
             */
            void setDelete(bool b);

            /**
             * 是否只有key
             *
             * @return bool
             */
            bool isOnlyKey() { return ISONLYKEY(); }

            /**
             * 当前元素移动到联合主键block链的下一个block
             * @return true, 移到下一个block了, false, 没有下一个block
             *
             */
            bool nextBlock();

            /**
             * 当前元素移动到联合主键block链的上一个block
             * @return true, 移到上一个block了, false, 没有上一个block
             *
             */
            bool prevBlock();

            /**
             * 释放block的所有空间
             */
            void deallocate();

            /**
             * 新block时调用该函数
             * 初始化新block的一些信息
             * @param iMainKeyAddr, 所属主key地址
             * @param uIndex, 联合主键hash索引
             * @param bHead, 插入到主key链上的顺序，前序或后序
             */
            void makeNew(uint32_t iMainKeyAddr, uint32_t uIndex, bool bHead);

            void makeNew(uint32_t iMainKeyAddr, uint32_t uIndex, Block &block);

            void makeListNew(uint32_t iMainKeyAddr, bool bHead);

            void makeZSetNew(uint32_t iMainKeyAddr, uint32_t uIndex, bool bHead);

            /**
             * 从Block链表中删除当前Block
             * @param bCheckMainKey, 是否在删除自身后检查主key状态，即主key下没有数据后是否删除主key
             * @return
             */
            void erase(bool bCheckMainKey = true);

            void eraseList(bool bCheckMainKey = true);

            void eraseZSet(bool bCheckMainKey = true);

            /**
             * 刷新set链表, 将当前block放在Set链表头部
             */
            void refreshSetList();

            /**
             * 刷新主key链下的顺序
             */
            void refreshKeyList(bool bHead);

        protected:

            /**
             * 根据chunk头地址获取chunk头指针
             * @param iAddr
             *
             * @return tagChunkHead*
             */
            tagChunkHead *getChunkHead(uint32_t iAddr) { return ((tagChunkHead*)_pMap->getAbsolute(iAddr)); }

            /**
             * 如果数据容量不够, 则新增加chunk, 不影响原有数据
             * 使新增加的总容量大于iDataLen
             * 释放多余的chunk
             * @param iDataLen
             * @param vtData, 淘汰的数据
             *
             * @return int,
             */
            int allocate(uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            int allocateList(uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 挂接chunk, 如果core则挂接失败, 保证内存块还可以用
             * @param pChunk, 第一个chunk指针
             * @param chunks, 所有的chunk地址
             *
             * @return int
             */
            int joinChunk(tagChunkHead *pChunk, const vector<uint32_t>& chunks);

            /**
             * 分配指定大小的内存空间, 可能会有多个chunk
             * @param fn, 分配的空间大小
             * @param chunks, 分配成功返回的chunks地址列表
             * @param vtData, 淘汰的数据
             * @return int
             */
            int allocateChunk(uint32_t fn, vector<uint32_t> &chunks, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            int allocateChunkList(uint32_t fn, vector<uint32_t> &chunks, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 释放指定chunk之后的所有chunk
             * @param iChunk 释放地址
             */
            void deallocate(uint32_t iChunk);

            /**
             * 获取数据长度
             *
             * @return size_t
             */
            uint32_t getDataLen();

            uint32_t getListDataLen();

            bool HASNEXTCHUNK() { return TC_Multi_HashMap_Malloc::ISSET(getBlockHead()->_iBitset, NEXTCHUNK_BIT); }
            bool ISDIRTY() { return TC_Multi_HashMap_Malloc::ISSET(getBlockHead()->_iBitset, DIRTY_BIT); }
            bool ISONLYKEY() { return TC_Multi_HashMap_Malloc::ISSET(getBlockHead()->_iBitset, ONLYKEY_BIT); }
            bool ISDELETE() { return TC_Multi_HashMap_Malloc::ISSET(getBlockHead()->_iBitset, DELETE_BIT); }

            void SETNEXTCHUNK(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getBlockHead()->_iBitset, NEXTCHUNK_BIT); else TC_Multi_HashMap_Malloc::UNSET(getBlockHead()->_iBitset, NEXTCHUNK_BIT); }
            void SETDIRTY(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getBlockHead()->_iBitset, DIRTY_BIT); else TC_Multi_HashMap_Malloc::UNSET(getBlockHead()->_iBitset, DIRTY_BIT); }
            void SETONLYKEY(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getBlockHead()->_iBitset, ONLYKEY_BIT); else TC_Multi_HashMap_Malloc::UNSET(getBlockHead()->_iBitset, ONLYKEY_BIT); }
            void SETDELETE(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getBlockHead()->_iBitset, DELETE_BIT); else TC_Multi_HashMap_Malloc::UNSET(getBlockHead()->_iBitset, DELETE_BIT); }

            bool HASNEXTCHUNKLIST() { return TC_Multi_HashMap_Malloc::ISSET(getListBlockHead()->_iBitset, NEXTCHUNK_BIT); }
            bool ISDIRTYLIST() { return TC_Multi_HashMap_Malloc::ISSET(getListBlockHead()->_iBitset, DIRTY_BIT); }
            bool ISONLYKEYLIST() { return TC_Multi_HashMap_Malloc::ISSET(getListBlockHead()->_iBitset, ONLYKEY_BIT); }
            bool ISDELETELIST() { return TC_Multi_HashMap_Malloc::ISSET(getListBlockHead()->_iBitset, DELETE_BIT); }

            void SETNEXTCHUNKLIST(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getListBlockHead()->_iBitset, NEXTCHUNK_BIT); else TC_Multi_HashMap_Malloc::UNSET(getBlockHead()->_iBitset, NEXTCHUNK_BIT); }
            void SETDIRTYLIST(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getListBlockHead()->_iBitset, DIRTY_BIT); else TC_Multi_HashMap_Malloc::UNSET(getBlockHead()->_iBitset, DIRTY_BIT); }
            void SETONLYKEYLIST(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getListBlockHead()->_iBitset, ONLYKEY_BIT); else TC_Multi_HashMap_Malloc::UNSET(getBlockHead()->_iBitset, ONLYKEY_BIT); }
            void SETDELETELIST(bool b) { if (b) TC_Multi_HashMap_Malloc::SET(getListBlockHead()->_iBitset, DELETE_BIT); else TC_Multi_HashMap_Malloc::UNSET(getBlockHead()->_iBitset, DELETE_BIT); }

        private:

            /**
             * Map
             */
            TC_Multi_HashMap_Malloc  *_pMap;

            /**
             * block区块首地址, 相对地址，内存块索引
             */
            uint32_t				 _iHead;

            /**
            * Block首地址，绝对地址
            */
            void					 *_pHead;

        };

        ////////////////////////////////////////////////////////////////////////
        /*
        * 内存数据块分配器，可同时为数据区和主key区分配内存
        *
        */
        class BlockAllocator
        {
        public:

            /**
             * 构造函数
             */
            BlockAllocator(TC_Multi_HashMap_Malloc *pMap)
                : _pMap(pMap)
                , _pChunkAllocator(new TC_MallocChunkAllocator())
            {
            }

            /**
             * 析够函数
             */
            ~BlockAllocator()
            {
                if (_pChunkAllocator != NULL)
                {
                    delete _pChunkAllocator;
                }
                _pChunkAllocator = NULL;
            }


            /**
             * 初始化
             * @param pHeadAddr, 地址, 换到应用程序的绝对地址
             * @param iSize, 内存大小
             * @param iMinBlockSize, 最小数据块大小
             * @param iMaxBlockSize, 最大数据块大小
             * @param fFactor, 数据块增长因子
             */
            void create(void *pHeadAddr, size_t iSize)
            {
                _pChunkAllocator->create(pHeadAddr, iSize);
            }

            /**
             * 连接到已经结构化的内存(如共享内存)
             * @param pAddr, 地址, 换到应用程序的绝对地址
             */
            void connect(void *pHeadAddr)
            {
                _pChunkAllocator->connect(pHeadAddr);
            }

            /**
             * 扩展空间
             * @param pAddr
             * @param iSize
             */
            void append(void *pAddr, size_t iSize)
            {
                _pChunkAllocator->append(pAddr, iSize);
            }

            /**
             * 重建内存结构
             */
            void rebuild()
            {
                _pChunkAllocator->rebuild();
            }

            /**
             * 获取每种数据块头部信息
             *
             * @return vector<TC_MemChunk::tagChunkHead>
             */
             //vector<TC_MemChunk::tagChunkHead> getBlockDetail() const  { return _pChunkAllocator->getBlockDetail(); }

             /**
              * 总内存大小
              *
              * @return size_t
              */
            size_t getMemSize() const { return _pChunkAllocator->getMemSize(); }

            /**
             * 获取总的、实际的数据容量
             *
             * @return size_t
             */
            size_t getAllCapacity() const { return _pChunkAllocator->getAllCapacity(); }

            /**
             * 获取每块内存分区的真正数据容量
             *
             * @return vector<size_t>
             */
            vector<size_t> getSingleBlockCapacity() const { return _pChunkAllocator->getSingleBlockCapacity(); }

            /**
             * 在内存中分配一个新的Block，实际上只分配一个chunk，并初始化Block头
             * @param iMainKeyAddr, 新block所属主key地址
             * @param index, block hash索引
             * @param bHead, 新块插入到主key链上的顺序，前序或后序
             * @param iAllocSize: in/需要分配的大小, out/分配的块大小
             * @param vtData, 返回淘汰的数据
             * @return size_t, 内存块地址索引, 0表示没有空间可以分配
             */
            uint32_t allocateMemBlock(uint8_t type, uint32_t iMainKeyAddr, uint32_t index, bool bHead, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            uint32_t relocateMemBlock(uint32_t srcAddr, uint8_t iVersion, uint32_t iMainKeyAddr, uint32_t index, bool bHead, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 在内存中分配一个新的Block，实际上只分配一个chunk，并初始化Block头
             * @param iMainKeyAddr, 新block所属主key地址
             * @param index, block hash索引
             * @param bHead, 新块插入到主key链上的顺序，前序或后序
             * @param iAllocSize: in/需要分配的大小, out/分配的块大小
             * @param vtData, 返回淘汰的数据
             * @return size_t, 内存块地址索引, 0表示没有空间可以分配
             */
            uint32_t allocateListBlock(uint32_t iMainKeyAddr, bool bHead, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
            * 在内存中分配一个主key头，只需要一个chunk即可
            * @param index, 主key hash索引
            * @param iAllocSize: in/需要分配的大小, out/分配的块大小
            * @param vtData, 返回释放的内存块数据
            * @return size_t, 主key头首地址,0表示没有空间可以分配
            */
            uint32_t allocateMainKeyHead(uint32_t index, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 为地址为iAddr的Block分配一个chunk         *
             * @param iAddr,分配的Block的地址
             * @param iAllocSize, in/需要分配的大小, out/分配的块大小
             * @param vtData 返回释放的内存块数据
             * @return size_t, 相对地址,0表示没有空间可以分配
             */
            uint32_t allocateChunk(uint32_t iAddr, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 在主key区为地址为iAddr的主key分配一个chunk         *
             * @param iAddr,分配的Block的地址
             * @param iAllocSize, in/需要分配的大小, out/分配的块大小
             * @param vtData 返回释放的内存块数据
             * @return size_t, 相对地址,0表示没有空间可以分配
             */
            uint32_t allocateMainKeyChunk(uint32_t iAddr, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 释放内存块
             * @param chunk，被释放的内存块
             * @param bMainKey，是否主key
             */
            void deallocateMemChunk(const MemChunk& chunk, bool bMainKey = false);

            /**
             * 释放内存快
             * @param vtChunk, 需要释放的chunk
             * @param bMainKey，是否主key
             */
            void deallocateMemChunk(const vector<MemChunk> &vtChunk, bool bMainKey = false);

            /**
             * 释放主key下的chunk
             * @param v
             */
            void deallocateMainKeyChunk(const vector<uint32_t> &v);

            /**
             * 释放Block下的chunk
             * @param v
             */
            void deallocateChunk(const vector<uint32_t> &v);

        protected:
            //不允许copy构造
            BlockAllocator(const BlockAllocator &);
            //不允许赋值
            BlockAllocator& operator=(const BlockAllocator &);
            bool operator==(const BlockAllocator &mba) const;
            bool operator!=(const BlockAllocator &mba) const;

        public:
            /**
             * map
             */
            TC_Multi_HashMap_Malloc         *_pMap;

            /**
             * chunk分配器
             */
            TC_MallocChunkAllocator			*_pChunkAllocator;
        };

        ////////////////////////////////////////////////////////////////
        // map的数据项
        class HashMapLockItem
        {
        public:

            /**
             *
             * @param pMap,
             * @param iAddr, 与LockItem对应的Block首地址
             */
            HashMapLockItem(TC_Multi_HashMap_Malloc *pMap, uint32_t iAddr);

            /**
             *
             * @param mcmdi
             */
            HashMapLockItem(const HashMapLockItem &mcmdi);

            /**
             *
             * @param mcmdi
             *
             * @return HashMapLockItem&
             */
            HashMapLockItem &operator=(const HashMapLockItem &mcmdi);

            /**
             *
             * @param mcmdi
             *
             * @return bool
             */
            bool operator==(const HashMapLockItem &mcmdi);

            /**
             *
             * @param mcmdi
             *
             * @return bool
             */
            bool operator!=(const HashMapLockItem &mcmdi);

            /**
             * 是否是脏数据
             *
             * @return bool
             */
            bool isDirty();

            /**
             * 是否已标记为删除
             *
             * @return bool
             */
            bool isDelete();

            /**
             * 是否只有Key
             *
             * @return bool
             */
            bool isOnlyKey();

            /**
             * 最后Sync时间
             *
             * @return uint32_t
             */
            uint32_t getSyncTime();

            /**
            * 数据过期时间
            *
            * @return uint32_t
            */
            uint32_t getExpireTime();

            /**
             * 获取键与值
             * @param v
             * @return int
             *          RT_OK:数据获取OK
             *          RT_ONLY_KEY: key有效, v无效为空
             *          其他值, 异常
             *
             */
            int get(TC_Multi_HashMap_Malloc::Value &v);

            int getList(TC_Multi_HashMap_Malloc::Value &v);

            int getSet(TC_Multi_HashMap_Malloc::Value &v);

            ///**
      //       * 获取键与值
            // * @param v
      //       * @return int
      //       *          RT_OK:数据获取OK
      //       *          RT_ONLY_KEY: key有效, v无效为空
      //       *          其他值, 异常
      //       *
      //  	 */
            //int get(string &v);

            /**
             * 获取键与值
             * @param v
             * @return int
             *          RT_OK:数据获取OK
             *          RT_ONLY_KEY: key有效, v无效为空
             *          其他值, 异常
             *
             */
            int get(TC_Multi_HashMap_Malloc::PackValue &v);

            /**
             * 仅获取key
             * @param mk, 主key
             * @param uk, 联合主key(除主key外的联合主键)
             * @return int
             *          RT_OK:数据获取OK
             *          其他值, 异常
             */
            int get(string &mk, string &uk);

            int getSet(string &mk, string &v);

            int getZSet(string &mk, string &v);

            /**
             * 获取对应block的相对地址
             *
             * @return size_t
             */
            uint32_t getAddr() const { return _iAddr; }

        protected:

            /**
             * 设置数据
             * @param mk, 主key
             * @param uk, 除主key外的联合主键
             * @param v, 数据值
             * @param iExpireTime, 数据过期时间，0表示不关注
             * @param iVersion, 数据版本(1-255), 0表示不关注版本
             * @param vtData, 淘汰的数据
             * @return int
             */
            int set(const string &mk, const string &uk, const string& v, uint32_t iExpireTime, uint8_t iVersion, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 设置数据
             * @param mk, 主key
             * @param uk, 除主key外的联合主键
             * @param v, 数据值
             * @param iExpireTime, 数据过期时间，0表示不关注
             * @param iVersion, 数据版本(1-255), 0表示不关注版本
             * @param bCheckExpire, 是否开启过期
             * @param iNowTime, 当前时间
             * @param vtData, 淘汰的数据
             * @return int
             */
            int set(const string &mk, const string &uk, const string& v, uint32_t iExpireTime, uint8_t iVersion, bool bCheckExpire, uint32_t iNowTime, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 设置Key, 无数据(Only Key)
             * @param mk, 主key
             * @param uk, 除主key外的联合主键
             * @param vtData, 淘汰的数据
             *
             * @return int
             */
            int set(const string &mk, const string &uk, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            int setListBlock(const string &mk, const string &v, uint32_t iExpireTime, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            int set(const string &mk, const string& v, uint64_t uniqueId, uint32_t iExpireTime, uint8_t iVersion, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            int set(const string &mk, const string& v, uint32_t iExpireTime, uint8_t iVersion, vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
             * 判断当前item是否是指定key的item, 如果是还返回value
             * @param pKey
             * @param iKeyLen
             *
             * @return bool
             */
            bool equal(const string &mk, const string &uk, TC_Multi_HashMap_Malloc::Value &v, int &ret);

            /**
             * 判断当前item是否是指定key的item
             * @param pKey
             * @param iKeyLen
             *
             * @return bool
             */
            bool equal(const string &mk, const string &uk, int &ret);

            bool equalSet(const string &mk, const string &v, int &ret);

            bool equalZSet(const string &mk, const string &v, int &ret);

            /**
             * 将当前item移动到下一个item
             *
             * @return HashMapLockItem
             */
            void nextItem(int iType);

            /**
             * 将当前item移动到上一个item
             * @param iType
             */
            void prevItem(int iType);

            friend class TC_Multi_HashMap_Malloc;
            friend struct TC_Multi_HashMap_Malloc::HashMapLockIterator;

        private:
            /**
             * map
             */
            TC_Multi_HashMap_Malloc *_pMap;

            /**
             * 对应的block的地址
             */
            uint32_t      _iAddr;
        };

        /////////////////////////////////////////////////////////////////////////
        // 定义迭代器
        struct HashMapLockIterator
        {
        public:

            // 定义遍历方式
            enum
            {
                IT_BLOCK = 0,        // 普通的顺序
                IT_SET = 1,        // Set时间顺序
                IT_GET = 2,		// Get时间顺序
                IT_MKEY = 3,		// 同一主key下的block遍历
                IT_UKEY = 4,		// 同一联合主键下的block遍历
            };

            /**
             * 迭代器的顺序
             */
            enum
            {
                IT_NEXT = 0,        // 顺序
                IT_PREV = 1,        // 逆序
            };

            /**
             *
             */
            HashMapLockIterator();

            /**
             * 构造函数
             * @param pMap,
             * @param iAddr, 对应的block地址
             * @param iType, 遍历类型
             * @param iOrder, 遍历的顺序
             */
            HashMapLockIterator(TC_Multi_HashMap_Malloc *pMap, uint32_t iAddr, int iType, int iOrder);

            /**
             * copy
             * @param it
             */
            HashMapLockIterator(const HashMapLockIterator &it);

            /**
             * 复制
             * @param it
             *
             * @return HashMapLockIterator&
             */
            HashMapLockIterator& operator=(const HashMapLockIterator &it);

            /**
             *
             * @param mcmi
             *
             * @return bool
             */
            bool operator==(const HashMapLockIterator& mcmi);

            /**
             *
             * @param mv
             *
             * @return bool
             */
            bool operator!=(const HashMapLockIterator& mcmi);

            /**
             * 前置++
             *
             * @return HashMapLockIterator&
             */
            HashMapLockIterator& operator++();

            /**
             * 后置++
             *
             * @return HashMapLockIterator&
             */
            HashMapLockIterator operator++(int);

            /**
             *
             *
             * @return HashMapLockItem&i
             */
            HashMapLockItem& operator*() { return _iItem; }

            /**
             *
             *
             * @return HashMapLockItem*
             */
            HashMapLockItem* operator->() { return &_iItem; }

        public:
            /**
             *
             */
            TC_Multi_HashMap_Malloc  *_pMap;

            /**
             *
             */
            HashMapLockItem _iItem;

            /**
             * 迭代器的方式
             */
            int        _iType;

            /**
             * 迭代器的顺序
             */
            int        _iOrder;

        };

        ////////////////////////////////////////////////////////////////
        // map的HashItem项, 一个HashItem对应多个数据项
        class HashMapItem
        {
        public:

            /**
             *
             * @param pMap
             * @param iIndex, Hash索引
             */
            HashMapItem(TC_Multi_HashMap_Malloc *pMap, uint32_t iIndex);

            /**
             *
             * @param mcmdi
             */
            HashMapItem(const HashMapItem &mcmdi);

            /**
             *
             * @param mcmdi
             *
             * @return HashMapItem&
             */
            HashMapItem &operator=(const HashMapItem &mcmdi);

            /**
             *
             * @param mcmdi
             *
             * @return bool
             */
            bool operator==(const HashMapItem &mcmdi);

            /**
             *
             * @param mcmdi
             *
             * @return bool
             */
            bool operator!=(const HashMapItem &mcmdi);

            /**
             * 获取当前hash桶的所有数据, 注意只获取有key/value的数据
             * 对于只有key的数据, 不获取
             * @param vtData
             * @return
             */
            void get(vector<TC_Multi_HashMap_Malloc::Value> &vtData);

            /**
            * 获取当前hash桶所有数据的过期时间
            * @param vtTimes
            * @return
            */
            void getExpireTime(vector<TC_Multi_HashMap_Malloc::ExpireTime> &vtTimes);

            void eraseExpireData(time_t now);

            /**
            * 获取当前hash桶所有标记为删除的数据
            * @param vtData
            * @return
            */
            void getDeleteData(vector<TC_Multi_HashMap_Malloc::DeleteData> &vtData);

            /**
             * 获取当前item的hash索引
             *
             * @return int
             */
            uint32_t getIndex() const { return _iIndex; }

            /**
             * 将当前item移动为下一个item
             *
             */
            void nextItem();

            /**
             * 设置当前hash桶下所有数据为脏数据，注意只设置有key/value的数据
             * 对于只有key的数据, 不设置
             * @param
             * @return int
             */
            int setDirty();

            /**
             * 设置当前hash桶下所有数据为干净数据，注意只设置有key/value的数据
             * 对于只有key的数据, 不设置
             * @param
             * @return int
             */
            int setClean();

            friend class TC_Multi_HashMap_Malloc;
            friend struct TC_Multi_HashMap_Malloc::HashMapIterator;

        private:
            /**
             * map
             */
            TC_Multi_HashMap_Malloc *_pMap;

            /**
             * 对应的数据块索引
             */
            uint32_t      _iIndex;
        };

        /////////////////////////////////////////////////////////////////////////
        // 定义迭代器
        struct HashMapIterator
        {
        public:

            /**
             * 构造函数
             */
            HashMapIterator();

            /**
             * 构造函数
             * @param iIndex, hash索引
             * @param type
             */
            HashMapIterator(TC_Multi_HashMap_Malloc *pMap, uint32_t iIndex);

            /**
             * copy
             * @param it
             */
            HashMapIterator(const HashMapIterator &it);

            /**
             * 复制
             * @param it
             *
             * @return HashMapLockIterator&
             */
            HashMapIterator& operator=(const HashMapIterator &it);

            /**
             *
             * @param mcmi
             *
             * @return bool
             */
            bool operator==(const HashMapIterator& mcmi);

            /**
             *
             * @param mv
             *
             * @return bool
             */
            bool operator!=(const HashMapIterator& mcmi);

            /**
             * 前置++
             *
             * @return HashMapIterator&
             */
            HashMapIterator& operator++();

            /**
             * 后置++
             *
             * @return HashMapIterator&
             */
            HashMapIterator operator++(int);

            /**
             *
             *
             * @return HashMapItem&i
             */
            HashMapItem& operator*() { return _iItem; }

            /**
             *
             *
             * @return HashMapItem*
             */
            HashMapItem* operator->() { return &_iItem; }

        public:
            /**
             *
             */
            TC_Multi_HashMap_Malloc  *_pMap;

            /**
             *
             */
            HashMapItem _iItem;
        };
        ///////////////////////////
        //MKHASH
          // map的HashItem项, 一个HashItem对应多个数据项
        class MKHashMapItem
        {
        public:

            /**
             *
             * @param pMap
             * @param iIndex, Hash索引
             */
            MKHashMapItem(TC_Multi_HashMap_Malloc *pMap, uint32_t iIndex);

            /**
             *
             * @param mcmdi
             */
            MKHashMapItem(const MKHashMapItem &mcmdi);

            /**
             *
             * @param mcmdi
             *
             * @return HashMapItem&
             */
            MKHashMapItem &operator=(const MKHashMapItem &mcmdi);

            /**
             *
             * @param mcmdi
             *
             * @return bool
             */
            bool operator==(const MKHashMapItem &mcmdi);

            /**
             *
             * @param mcmdi
             *
             * @return bool
             */
            bool operator!=(const MKHashMapItem &mcmdi);

            /**
             * 获取当前hash桶的所有数据, 注意只获取有key/value的数据
             * 对于只有key的数据, 不获取
             * @param vtData
             * @return
             */
            void get(map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > > &mData);

            /**
             * 获取当前hash桶的所有数据,也会获取onlyKey数据
             * @param vtData
             * @return
             */
            void getAllData(map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > > &mData);

            void eraseExpireData(time_t now);

            /**
             * 获取当前hash桶不可淘汰数据和del数据的数量
             * @param iDirty
             * @param iDel
             */
            void getStaticData(size_t &iDirty, size_t &iDel);

            /**
             * 获取当前hash桶的所有主key
             * @param vtData
             * @return
             */
            void getKey(vector<string> &mData);

            /**
            * 获取当前hash桶所有数据的过期时间
            * @param vtTimes
            * @return
            */
            void getExpireTime(map<string, vector<TC_Multi_HashMap_Malloc::ExpireTime> > &mTimes);

            //      /**
            //       * 获取当前item的hash索引
            //       * 
            //       * @return int
            //       */
            uint32_t getIndex() const { return _iIndex; }

            /**
             * 设置hash索引值
             *
             */
            bool setIndex(uint32_t index);

            /**
             * 获取hash桶个数
             *
             */
            uint32_t getHashCount() const { return _pMap->getMainKeyHashCount(); }

            /**
             * 将当前item移动为下一个item
             *
             */
            void nextItem();

            /**
             * 删除当前hash桶下所有onlykey数据
             * @param
             * @return int
             */
            int delOnlyKey();

            friend class TC_Multi_HashMap_Malloc;
            friend struct TC_Multi_HashMap_Malloc::MKHashMapIterator;

        private:
            /**
             * map
             */
            TC_Multi_HashMap_Malloc *_pMap;

            /**
             * 对应的数据块索引
             */
            uint32_t      _iIndex;
        };

        /////////////////////////////////////////////////////////////////////////
        // 定义迭代器
        struct MKHashMapIterator
        {
        public:

            /**
             * 构造函数
             */
            MKHashMapIterator();

            /**
             * 构造函数
             * @param iIndex, hash索引
             * @param type
             */
            MKHashMapIterator(TC_Multi_HashMap_Malloc *pMap, uint32_t iIndex);

            /**
             * copy
             * @param it
             */
            MKHashMapIterator(const MKHashMapIterator &it);

            /**
             * 复制
             * @param it
             *
             * @return HashMapLockIterator&
             */
            MKHashMapIterator& operator=(const MKHashMapIterator &it);

            /**
             *
             * @param mcmi
             *
             * @return bool
             */
            bool operator==(const MKHashMapIterator& mcmi);

            /**
             *
             * @param mv
             *
             * @return bool
             */
            bool operator!=(const MKHashMapIterator& mcmi);

            /**
             * 前置++
             *
             * @return HashMapIterator&
             */
            MKHashMapIterator& operator++();

            /**
             * 后置++
             *
             * @return HashMapIterator&
             */
            MKHashMapIterator operator++(int);

            //设置索引值
            bool setIndex(uint32_t index) { return _iItem.setIndex(index); };

            /**
             *
             *
             * @return HashMapItem&i
             */
            MKHashMapItem& operator*() { return _iItem; }

            /**
             *
             *
             * @return HashMapItem*
             */
            MKHashMapItem* operator->() { return &_iItem; }

        public:
            /**
             *
             */
            TC_Multi_HashMap_Malloc  *_pMap;

            /**
             *
             */
            MKHashMapItem _iItem;
        };
        //////////////////////////////////////////////////////////////////////////////////////////////////
        // map定义
        //
        /**
         * map头
         */
        struct tagMapHead
        {
            char		_cMaxVersion;        //大版本
            char		_cMinVersion;        //小版本
            bool		_bReadOnly;          //是否只读
            bool		_bAutoErase;         //是否可以自动淘汰
            char		_cEraseMode;         //淘汰方式:0x00:按照Get链淘汰, 0x01:按照Set链淘汰
            size_t		_iMemSize;           //内存大小
            size_t		_iMainKeySize;	 	 //主key大小，粗略平均值
            size_t		_iDataSize;          //数据平均大小
            size_t		_iMinChunkSize;		 //允许最小chunk大小
            float		_fHashRatio;         //chunks个数/hash个数
            float		_fMainKeyRatio;		 //chunks个数/主key hash个数
            size_t		_iMainKeyCount;		 //主key个数
            size_t		_iElementCount;      //总元素个数
            size_t		_iEraseCount;        //每次淘汰个数
            size_t		_iDirtyCount;        //脏数据个数
            uint32_t	_iSetHead;           //Set时间链表头部
            uint32_t	_iSetTail;           //Set时间链表尾部
            uint32_t	_iGetHead;           //Get时间链表头部
            uint32_t	_iGetTail;           //Get时间链表尾部
            uint32_t	_iDirtyTail;         //脏数据链尾部
            uint32_t	_iBackupTail;        //热备指针
            uint32_t	_iSyncTail;          //回写链表
            uint32_t	_iSyncTime;          //回写时间
            size_t		_iMKUsedChunk;		 //主key已经使用的chunk数
            size_t		_iDataUsedChunk;     //数据已经使用的内存块
            size_t 		_iMKUsedMem;		 //主key已经使用的内存
            size_t 		_iUsedDataMem;		 //已经使用的数据内存 
            size_t		_iGetCount;          //get次数
            size_t		_iHitCount;          //命中次数
            size_t		_iMKOnlyKeyCount;	 //主key的onlykey个数
            size_t		_iOnlyKeyCount;		 //主键的OnlyKey个数, 这个数通常为0
            size_t		_iMaxBlockCount;	 //主key链下最大的记录数，这个数值要监控，不能太大，否则会导致查询变慢
            bool		_bInit;				 //是否已经完成初始化
            uint8_t     _iKeyType;           //主key类型
            uint8_t     _iMaxLevel;          //用于zset结构的最大层数
            char		_cReserve[30];       //保留
        }__attribute__((packed));

        /**
         * 需要修改的地址
         */
        struct tagModifyData
        {
            size_t		_iModifyAddr;       //修改的地址
            char		_cBytes;            //字节数
            size_t		_iModifyValue;      //值
        }__attribute__((packed));

        /**
         * 修改数据块头部
         */
        struct tagModifyHead
        {
            char            _cModifyStatus;         //修改状态: 0:目前没有人修改, 1: 开始准备修改, 2:修改完毕, 没有copy到内存中
            size_t          _iNowIndex;             //更新到目前的索引, 不能操作1000个
            tagModifyData   _stModifyData[1000];     //一次最多1000次修改
        }__attribute__((packed));

        /**
         * HashItem
         */
        struct tagHashItem
        {
            uint32_t	_iBlockAddr;		//指向数据项的内存地址索引
            uint32_t	_iListCount;		//链表个数
        }__attribute__((packed));

        /**
        * 主key HashItem
        */
        struct tagMainKeyHashItem
        {
            uint32_t	_iMainKeyAddr;		// 主key数据项的偏移地址
            uint32_t	_iListCount;		// 相同主key hash索引下主key个数
        }__attribute__((packed));

        //64位操作系统用基数版本号, 32位操作系统用偶数版本号
#if __WORDSIZE == 64

    //定义版本号
        enum
        {
            MAX_VERSION = 8,		//当前map的大版本号
            MIN_VERSION = 2,		//当前map的小版本号
        };

#else
    //定义版本号
        enum
        {
            MAX_VERSION = 8,		//当前map的大版本号
            MIN_VERSION = 2,		//当前map的小版本号
        };

#endif

        //定义淘汰方式
        enum
        {
            ERASEBYGET = 0x00,		//按照Get链表淘汰
            ERASEBYSET = 0x01,		//按照Set链表淘汰
        };

        // 定义设置数据时的选项
        enum DATATYPE
        {
            PART_DATA = 0,		// 不完整数据
            FULL_DATA = 1,		// 完整数据
            AUTO_DATA = 2,		// 根据内部的数据完整性状态来决定
        };

        /**
         * get, set等int返回值
         */
        enum
        {
            RT_OK = 0,    //成功
            RT_DIRTY_DATA = 1,    //脏数据
            RT_NO_DATA = 2,    //没有数据
            RT_NEED_SYNC = 3,    //需要回写
            RT_NONEED_SYNC = 4,    //不需要回写
            RT_ERASE_OK = 5,    //淘汰数据成功
            RT_READONLY = 6,    //map只读
            RT_NO_MEMORY = 7,    //内存不够
            RT_ONLY_KEY = 8,    //只有Key, 没有Value
            RT_NEED_BACKUP = 9,    //需要备份
            RT_NO_GET = 10,   //没有GET过
            RT_DATA_VER_MISMATCH = 11,	//写入数据版本不匹配
            RT_PART_DATA = 12,	//主key数据不完整
            RT_DATA_EXIST = 13,	//设置OnlyKey时的返回，表示实际上有数据
            RT_DATA_EXPIRED = 14,	//数据已过期 
            RT_DATA_DEL = 15,	//数据已删除
            RT_DECODE_ERR = -1,   //解析错误
            RT_EXCEPTION_ERR = -2,   //异常
            RT_LOAD_DATA_ERR = -3,   //加载数据异常
            RT_VERSION_MISMATCH_ERR = -4,   //版本不一致
            RT_DUMP_FILE_ERR = -5,   //dump到文件失败
            RT_LOAD_FILE_ERR = -6,   //load文件到内存失败
            RT_NOTALL_ERR = -7,   //没有复制完全
        };

        //定义迭代器
        typedef HashMapIterator     hash_iterator;
        typedef HashMapLockIterator lock_iterator;
        typedef MKHashMapIterator   mhash_iterator;
        //定义hash处理器
        typedef TC_Functor<uint32_t, TL::TLMaker<const string &>::Result> hash_functor;

        //////////////////////////////////////////////////////////////////////////////////////////////
        //map的接口定义

        /**
         * 构造函数
         */
        TC_Multi_HashMap_Malloc()
            : _iMinChunkSize(64)
            , _iMainKeySize(0)
            , _iDataSize(0)
            , _fHashRatio(2.0)
            , _fMainKeyRatio(1.0)
            , _pMainKeyAllocator(NULL)
            , _pDataAllocator(new BlockAllocator(this))
            , _lock_end(this, 0, 0, 0)
            , _end(this, (uint32_t)(-1))
            , _mk_end(this, (uint32_t)(-1))
            , _hashf(magic_string_hash())
        {
        }

        /**
        * 初始化主key数据大小
        * 这个大小将会被用来分配主key专门的内存数据块，通常主key与数据区的数据大小相关会比较大
        * 区分不同的大小将有助于更好地利用内存空间，否则使用数据区的内存块来存储主key将是浪费
        * 这个值也可以不设，不设将默认数据区的内在块来保存主key
        */
        void initMainKeySize(size_t iMainKeySize);

        /**
         * 初始化数据块平均大小
         * @param iDataSize
         */
        void initDataSize(size_t iDataSize);

        /**
         * 始化chunk数据块/hash项比值, 默认是2,
         * 有需要更改必须在create之前调用
         *
         * @param fRatio
         */
        void initHashRatio(float fRatio) { _fHashRatio = fRatio; }

        /**
         * 初始化chunk个数/主key hash个数, 默认是1, 含义是一个主key下面大概有多个条数据
         * 有需要更改必须在create之前调用
         *
         * @param fRatio
         */
        void initMainKeyHashRatio(float fRatio) { _fMainKeyRatio = fRatio; }

        /**
         * 初始化, 之前需要调用:initDataAvgSize和initHashRatio
         * @param pAddr 外部分配好的存储的绝对地址
         * @param iSize 存储空间大小
         * @return 失败则抛出异常
         */
        void create(void *pAddr, size_t iSize, uint8_t _iKeyType);

        /**
         * 链接到已经格式化的内存块
         * @param pAddr, 内存地址
         * @param iSize, 内存大小
         * @return 失败则抛出异常
         */
        void connect(void *pAddr, size_t iSize, uint8_t _iKeyType);

        /**
         * 原来的数据块基础上扩展内存, 注意通常只能对mmap文件生效
         * (如果iSize比本来的内存就小,则返回-1)
         * @param pAddr, 扩展后的空间
         * @param iSize
         * @return 0:成功, -1:失败
         */
        int append(void *pAddr, size_t iSize);

        /**
         * 获取每种大小内存块的头部信息
         *
         * @return vector<TC_MemChunk::tagChunkHead>: 不同大小内存块头部信息
         */
         //vector<TC_MemChunk::tagChunkHead> getBlockDetail() { return _pDataAllocator->getBlockDetail(); }

         /**
          * 一共可用的数据内存大小
          *
          * @return size_t
          */
        size_t getDataMemSize() { return _pDataAllocator->getAllCapacity(); }

        /**
         * 每种block中可用的数据内存大小
         *
         * @return vector<size_t>
         */
        vector<size_t> getSingleBlockDataMemSize() { return _pDataAllocator->getSingleBlockCapacity(); }

        /**
        * 获取主key chunk区的相关信息
        */
        //vector<TC_MemChunk::tagChunkHead> getMainKeyDetail() { return _pMainKeyAllocator->getBlockDetail(); }

        /**
        * 获取主key可用的内存区大小
        */
        size_t getMainKeyMemSize() { return _pMainKeyAllocator->getAllCapacity(); }

        /**
         * 获取数据区hash桶的个数
         *
         * @return size_t
         */
        size_t getHashCount() { return _hash.size(); }

        /**
        * 获取主key hash桶个数
        */
        size_t getMainKeyHashCount() { return _hashMainKey.size(); }

        /**
         * 元素的个数
         *
         * @return size_t
         */
        size_t size() { return _pHead->_iElementCount; }

        /**
         * 脏数据元素个数
         *
         * @return size_t
         */
        size_t dirtyCount() { return _pHead->_iDirtyCount; }

        /**
         * 主键OnlyKey数据元素个数
         *
         * @return size_t
         */
        size_t onlyKeyCount() { return _pHead->_iOnlyKeyCount; }

        /**
         * 主key OnlyKey数据元素个数
         *
         * @return size_t
         */
        size_t onlyKeyCountM() { return _pHead->_iMKOnlyKeyCount; }

        /**
         * 设置每次淘汰数量
         * @param n
         */
        void setEraseCount(size_t n) { _pHead->_iEraseCount = n; }

        /**
         * 设置主key下面最大数据个数
         * @param n
         */
        void setMaxBlockCount(size_t n) { _pHead->_iMaxBlockCount = n; }

        /**
         * 获取每次淘汰数量
         *
         * @return size_t
         */
        size_t getEraseCount() { return _pHead->_iEraseCount; }

        /**
         * 设置只读
         * @param bReadOnly
         */
        void setReadOnly(bool bReadOnly) { _pHead->_bReadOnly = bReadOnly; }

        /**
         * 是否只读
         *
         * @return bool
         */
        bool isReadOnly() { return _pHead->_bReadOnly; }

        /**
         * 设置是否可以自动淘汰
         * @param bAutoErase
         */
        void setAutoErase(bool bAutoErase) { _pHead->_bAutoErase = bAutoErase; }

        /**
         * 是否可以自动淘汰
         *
         * @return bool
         */
        bool isAutoErase() { return _pHead->_bAutoErase; }

        /**
         * 设置淘汰方式
         * TC_Multi_HashMap_Malloc::ERASEBYGET
         * TC_Multi_HashMap_Malloc::ERASEBYSET
         * @param cEraseMode
         */
        void setEraseMode(char cEraseMode) { _pHead->_cEraseMode = cEraseMode; }

        /**
         * 获取淘汰方式
         *
         * @return bool
         */
        char getEraseMode() { return _pHead->_cEraseMode; }

        /**
         * 设置回写时间间隔(秒)
         * @param iSyncTime
         */
        void setSyncTime(uint32_t iSyncTime) { _pHead->_iSyncTime = iSyncTime; }

        void setMainKeyType(MainKey::KEYTYPE keyType) { _pHead->_iKeyType = keyType; }

        /**
         * 获取回写时间
         *
         * @return uint32_t
         */
        uint32_t getSyncTime() { return _pHead->_iSyncTime; }

        /**
         * 获取头部数据信息
         *
         * @return tagMapHead&
         */
        tagMapHead& getMapHead() { return *_pHead; }

        /**
         * 设置联合主键hash方式
         * @param hashf
         */
        void setHashFunctor(hash_functor hashf) { _hashf = hashf; }

        /**
        * 设置主key的hash方式
        * @param hashf
        */
        void setHashFunctorM(hash_functor hashf) { _mhashf = hashf; }

        /**
         * 返回hash处理器
         *
         * @return hash_functor&
         */
        hash_functor &getHashFunctor() { return _hashf; }
        hash_functor &getHashFunctorM() { return _mhashf; }

        /**
         * 获取指定索引的hash item
         * @param index, hash索引
         *
         * @return tagHashItem&
         */
        tagHashItem *item(size_t iIndex) { return &_hash[iIndex]; }

        /**
        * 根据主key hash索引取主key item
        * @param iIndex, 主key的hash索引
        */
        tagMainKeyHashItem* itemMainKey(size_t iIndex) { return &_hashMainKey[iIndex]; }

        /**
         * dump到文件
         * @param sFile
         *
         * @return int
         *          RT_DUMP_FILE_ERR: dump到文件出错
         *          RT_OK: dump到文件成功
         */
        int dump2file(const string &sFile);

        /**
         * 从文件load
         * @param sFile
         *
         * @return int
         *          RT_LOAL_FILE_ERR: load出错
         *          RT_VERSION_MISMATCH_ERR: 版本不一致
         *          RT_OK: load成功
         */
        int load5file(const string &sFile);

        /**
         * 清空hashmap
         * 所有map的数据恢复到初始状态
         */
        void clear();

        /**
        * 检查主key是否存在
        * @param mk, 主key
        *
        * @return int
        *		TC_Multi_HashMap_Malloc::RT_OK, 主key存在，且有数据
        *		TC_Multi_HashMap_Malloc::RT_ONLY_KEY, 主key存在，没有数据
        *		TC_Multi_HashMap_Malloc::RT_PART_DATA, 主key存在，里面的数据可能不完整
        *		TC_Multi_HashMap_Malloc::RT_NO_DATA, 主key不存在
        *       其他返回值: 错误
        */
        int checkMainKey(const string &mk);

        int getMainKeyType(TC_Multi_HashMap_Malloc::MainKey::KEYTYPE &keyType);

        /**
        * 设置主key下数据的完整性
        * @param mk, 主key
        * @param bFull, true为完整数据，false为不完整数据
        *
        * @return
        *          RT_READONLY: 只读
        *          RT_NO_DATA: 没有当前数据
        *          RT_OK: 设置成功
        *          其他返回值: 错误
        */
        int setFullData(const string &mk, bool bFull);

        /**
         * 检查数据干净状态
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key
         *          RT_DIRTY_DATA: 是脏数据
         *          RT_OK: 是干净数据
         *          其他返回值: 错误
         */
        int checkDirty(const string &mk, const string &uk);

        /**
        * 检查主key下数据的干净状态，只要主key下面有一条脏数据，则返回脏
        * @param mk, 主key
        * @return int
        *          RT_NO_DATA: 没有当前数据
        *          RT_ONLY_KEY:只有Key
        *          RT_DIRTY_DATA: 是脏数据
        *          RT_OK: 是干净数据
        *          其他返回值: 错误
        */
        int checkDirty(const string &mk);

        /**
         * 设置为脏数据, 修改SET时间链, 会导致数据回写
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int
         *          RT_READONLY: 只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK: 设置脏数据成功
         *          其他返回值: 错误
         */
        int setDirty(const string &mk, const string &uk);

        /**
         * 数据回写失败后重新设置为脏数据
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int
         *          RT_READONLY: 只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK: 设置脏数据成功
         *          其他返回值: 错误
         */
        int setDirtyAfterSync(const string &mk, const string &uk);

        /**
         * 设置为干净数据, 修改SET链, 导致数据不回写
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int
         *          RT_READONLY: 只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int setClean(const string &mk, const string &uk);

        /**
        * 更新数据的回写时间
        * @param mk,
        * @param uk,
        * @param iSynctime
        *
        * @return int
        *          RT_READONLY: 只读
        *          RT_NO_DATA: 没有当前数据
        *          RT_ONLY_KEY:只有Key
        *          RT_OK: 设置脏数据成功
        *          其他返回值: 错误
        */
        int setSyncTime(const string &mk, const string &uk, uint32_t iSyncTime);

        /**
         * 获取数据, 修改GET时间链
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         * @param unHash, 主key加联合key的哈希值
         @ @param v, 返回的数据
         *
         * @return int:
         *          RT_NO_DATA: 没有数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK:获取数据成功
         *          其他返回值: 错误
         */
        int get(const string &mk, const string &uk, const uint32_t unHash, Value &v, bool bCheckExpire = false, uint32_t iExpireTime = -1);

        /**
       * 获取数据, 修改GET时间链
       * @param mk, 主key
       * @param uk, 除主key外的联合主键*
       * @param unHash, 主key加联合key的哈希值
       @ @param v, 返回的数据
       *
       * @return int:
       *          RT_NO_DATA: 没有数据
       *          RT_ONLY_KEY:只有Key
       *          RT_OK:获取数据成功
       *          其他返回值: 错误
       */
        int get(const string &mk, const string &uk, const uint32_t unHash, Value &v, uint32_t &iSyncTime, uint32_t& iDateExpireTime, uint8_t& iVersion, bool& bDirty, bool bCheckExpire = false, uint32_t iExpireTime = -1);

        /**
         * 获取主key下的所有数据, 修改GET时间链
         * @param mk, 主key
         * @param bHead, 从头部开始取数据
         * @param iStart, 起始位置，类似于SQL limit
         * @param iCount, 结束位置
         * @param vs, 返回的数据集
         *
         * @return int:
         *          RT_NO_DATA: 没有数据
         *          RT_ONLY_KEY: 只有Key
         *          RT_PART_DATA: 数据不全，只有部分数据
         *          RT_OK: 获取数据成功
         *          其他返回值: 错误
         */
        int get(const string &mk, vector<Value> &vs, size_t iCount = -1, size_t iStart = 0, bool bHead = true, bool bCheckExpire = false, uint32_t iExpireTime = -1);


        /**
        * 获取主key下的所有数据, 修改GET时间链
        * @param mk, 主key
        * @param iDataCount, 结束位置
        *
        * @return int:
        *          RT_NO_DATA: 没有数据
        *          RT_ONLY_KEY: 只有Key
        *          RT_PART_DATA: 数据不全，只有部分数据
        *          RT_OK: 获取数据成功
        *          其他返回值: 错误
        */
        int get(const string &mk, size_t &iDataCount);

        /**
        * 根据联合主健来获取该数据在主key链上的前面或者后面的数据
        * @param mk
        * @param uk
        * @param iCount, 要取的记录数
        * @param bForward, 指定主key链方向
        * @return int:
        *          RT_NO_DATA: 没有数据
        *          RT_ONLY_KEY: 只有Key
        *          RT_PART_DATA: 数据不全，只有部分数据
        *          RT_OK: 获取数据成功
        *          其他返回值: 错误
        */
        int get(const string &mk, const string &uk, const uint32_t unHash, vector<Value> &vs, size_t iCount = 1, bool bForward = true, bool bCheckExpire = false, uint32_t iExpireTime = -1);
        /**
         * 获取主key hash下的所有数据, 不修改GET时间链，主要用于迁移
         * @param mh, 主key hash值
         * @param vs, 返回的数据集，以主key进行分组的map
         *
         * @return int:
         *          RT_OK: 获取数据成功
         *          其他返回值: 错误
         */
        int get(uint32_t &mh, map<string, vector<Value> > &vs);

        /**
         * 获取主key hash下的所有数据, 不修改GET时间链，主要用于迁移
         * @param mh, 主key hash值
         * @param vs, 返回的数据集，以主key进行分组的map
         *
         * @return int:
         *          RT_OK: 获取数据成功
         *          其他返回值: 错误
         */
        int get(uint32_t &mh, map<string, vector<PackValue> > &vs);


        /**
         * 设置onlykey, 用于删除数据库，内存不够时会自动淘汰老的数据
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param eType: set的数据类型，PART_DATA-不完事的数据，FULL_DATA-完整的数据，AUTO_DATA-根据已有数据类型决定
         * @param bHead: 插入到主key链的顺序，前序或后序
         * @param vtData: 被淘汰的数据
         *
         * @return int
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          RT_OK: 设置成功
         *			RT_DATA_EXIST: 主键下已经有数据，不能设置为only key
         *          其他返回值: 错误
         */
        int setForDel(const string &mk, const string &uk, const time_t t, DATATYPE eType, bool bHead, vector<Value> &vtData);

        /**
         * 设置数据, 修改时间链, 内存不够时会自动淘汰老的数据
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param unHash: 主key加联合key的哈希值
         * @param v: 数据值
         * @param iExpiretime: 数据过期时间，单位为秒，0表示不设置过期时间
         * @param iVersion: 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
         * @param bDirty: 是否是脏数据
         * @param deleteType: 是否设置为已删除
         * @param eType: set的数据类型，PART_DATA-不完整的数据，FULL_DATA-完整的数据，AUTO_DATA-根据已有数据类型决定
         * @param bHead: 插入到主key链的顺序，前序或后序
         * @param bUpdateOrder: 数据更新同时更新主key链下的顺序
         * @param vtData: 被淘汰的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *			RT_DATA_VER_MISMATCH, 要设置的数据版本与当前版本不符，应该重新get后再set
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int set(const string &mk, const string &uk, const uint32_t unHash, const string &v, uint32_t iExpireTime,
            uint8_t iVersion, bool bDirty, DATATYPE eType, bool bHead, bool bUpdateOrder, DELETETYPE deleteType, vector<Value> &vtData);

        /**
         * 设置数据, 修改时间链, 内存不够时会自动淘汰老的数据
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param unHash: 主key加联合key的哈希值
         * @param v: 数据值
         * @param iExpiretime: 数据过期时间，单位为秒，0表示不设置过期时间
         * @param iVersion: 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
         * @param bDirty: 是否是脏数据
         * @param deleteType: 是否设置为已删除
         * @param eType: set的数据类型，PART_DATA-不完整的数据，FULL_DATA-完整的数据，AUTO_DATA-根据已有数据类型决定
         * @param bHead: 插入到主key链的顺序，前序或后序
         * @param bUpdateOrder: 数据更新同时更新主key链下的顺序
         * @param bCheckExpire: 是否开启过期
         * @param iNowTime: 当前时间
         * @param vtData: 被淘汰的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *			RT_DATA_VER_MISMATCH, 要设置的数据版本与当前版本不符，应该重新get后再set
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int set(const string &mk, const string &uk, const uint32_t unHash, const string &v, uint32_t iExpireTime,
            uint8_t iVersion, bool bDirty, DATATYPE eType, bool bHead, bool bUpdateOrder, DELETETYPE deleteType, bool bCheckExpire, uint32_t iNowTime, vector<Value> &vtData);

        /**
         * 设置key, 但无数据(only key), 内存不够时会自动淘汰老的数据
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param eType: set的数据类型，PART_DATA-不完事的数据，FULL_DATA-完整的数据，AUTO_DATA-根据已有数据类型决定
         * @param bHead: 插入到主key链的顺序，前序或后序
         * @param bUpdateOrder: 数据更新同时更新主key链下的顺序
         * @param deleteType: 是否设置为已删除
         * @param vtData: 被淘汰的数据
         *
         * @return int
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          RT_OK: 设置成功
         *			RT_DATA_EXIST: 主键下已经有数据，不能设置为only key
         *          其他返回值: 错误
         */
        int set(const string &mk, const string &uk, DATATYPE eType, bool bHead, bool bUpdateOrder, DELETETYPE deleteType, vector<Value> &vtData);

        /**
         * 仅设置主key, 无联合key及数据, 内存不够时会自动淘汰老的数据
         * @param mk: 主key
         * @param vtData: 被淘汰的数据
         *
         * @return int
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          RT_OK: 设置成功
         *			RT_DATA_EXIST: 主key下已经有数据，不能设置为only key
         *          其他返回值: 错误
         */
        int set(const string &mk, vector<Value> &vtData);

        /**
        * 批量设置数据, 内存不够时会自动淘汰老的数据
        * @param vtSet: 需要设置的数据
        * @param eType: set的数据类型，PART_DATA-不完事的数据，FULL_DATA-完整的数据，AUTO_DATA-根据已有数据类型决定
        * @param bHead: 插入到主key链的顺序，前序或后序
        * @param bUpdateOrder: 数据更新同时更新主key链下的顺序
        * @param bOrderByItem: 是否以字段排序，这个参数会影响数据的顺序
        * @param bForce, 是否强制插入数据，为false则表示如果数据已经存在则不更新
        * @param vtErased: 内存不足时被淘汰的数据
        *
        * @return
        *          RT_READONLY: map只读
        *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
        *          RT_OK: 设置成功
        *          其他返回值: 错误
        */
        int set(const vector<Value> &vtSet, DATATYPE eType, bool bHead, bool bUpdateOrder, bool bOrderByItem, bool bForce, vector<Value> &vtErased);

        /**
         * 设置数据, 修改时间链, 内存不够时会自动淘汰老的数据
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param unHash: 主key加联合key的哈希值
         * @param v: 数据值
         * @param iExpiretime: 数据过期时间，单位为秒，0表示不设置过期时间
         * @param iVersion: 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
         * @param bDirty: 是否是脏数据
         * @param deleteType: 是否设置为已删除
         * @param eType: set的数据类型，PART_DATA-不完整的数据，FULL_DATA-完整的数据，AUTO_DATA-根据已有数据类型决定
         * @param bHead: 插入到主key链的顺序，前序或后序
         * @param bUpdateOrder: 数据更新同时更新主key链下的顺序
         * @param vtData: 被淘汰的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *			RT_DATA_VER_MISMATCH, 要设置的数据版本与当前版本不符，应该重新get后再set
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int update(const string &mk, const string &uk, const uint32_t unHash, const map<std::string, DCache::UpdateValue> &mpValue,
            const vector<DCache::Condition> & vtValueCond, const FieldConf *fieldInfo, bool bLimit, size_t iIndex, size_t iCount, std::string &retValue, bool bCheckExpire, uint32_t iNowTime, uint32_t iExpireTime,
            bool bDirty, DATATYPE eType, bool bHead, bool bUpdateOrder, vector<Value> &vtData);

        /**
        * 不是真正删除数据，只是把标记位设置一下
        * @param mk: 主key
        * @param uk: 除主key外的联合主键
        * @return int:
        *          RT_READONLY: map只读
        *          RT_NO_DATA: 没有当前数据
        *          RT_ONLY_KEY:只有Key, 删除成功
        *          RT_OK: 删除数据成功
        *         其他返回值: 错误
        */
        int delSetBit(const string &mk, const string &uk, const time_t t);

        int delSetBit(const string &mk, const string &uk, uint8_t iVersion, const time_t t);

        /**
         * 不是真正删除数据，只是把标记位设置一下
         * @param mk: 主key
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key, 删除成功
         *          RT_OK: 删除数据成功
         *         其他返回值: 错误
         */
        int delSetBit(const string &mk, const time_t t, uint64_t &delCount);

        /**
         * 把已设置删除标记的数据删除
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key, 删除成功
         *          RT_OK: 删除数据成功
         *         其他返回值: 错误
         */
        int delReal(const string &mk, const string &uk);

        /**
         * 把未设置删除标记的数据删除
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param data: 被删除的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key, 删除成功
         *          RT_OK: 删除数据成功
         *         其他返回值: 错误
         */
        int del(const string &mk, const string &uk, Value &data);

        /**
         * 把主key下所有未设置删除标记的数据删除
         * @param mk: 主key
         * @param data: 被删除的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key, 删除成功
         *          RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int del(const string &mk, vector<Value> &data);

        /**
         * 删除数据，除非强制删除某条数据，否则应该调用下面的删除主key下所有数据的函数
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param data: 被删除的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key, 删除成功
         *          RT_OK: 删除数据成功
         *         其他返回值: 错误
         */
        int delForce(const string &mk, const string &uk, Value &data);

        /**
         * 删除主key下所有数据
         * @param mk: 主key
         * @param data: 被删除的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key, 删除成功
         *          RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int delForce(const string &mk, vector<Value> &data);

        /**
        * 删除主key下指定范围的数据
        * @param mk: 主key
        * @param iCount: 要删除多少条记录
        * @param data: 被删除的数据
        * @param iStart: 删除的起始位置
        * @param bHead: 从头部开始删除还是尾部开始
        * @param bDelDirty: 是否删除脏数据
        * @return int:
        *          RT_READONLY: map只读
        *          RT_NO_DATA: 没有当前数据
        *          RT_ONLY_KEY:只有Key, 删除成功
        *          RT_OK: 删除数据成功
        *          其他返回值: 错误
        */
        int del(const string &mk, size_t iCount, vector<Value> &data, size_t iStart = 0, bool bHead = true, bool bDelDirty = false);

        /**
         * 淘汰数据, 每次删除一条, 根据Get时间淘汰
         * 外部循环调用该接口淘汰数据
         * 直到: 元素个数/chunks * 100 < Ratio, bCheckDirty 为true时，遇到脏数据则淘汰结束
         * @param ratio: 共享内存chunks使用比例 0< Ratio < 100
         * @param data: 被删除的数据集
         * @param bCheckDirty: 是否检查脏数据
         * @return int:
         *          RT_READONLY: map只读
         *          RT_OK: 不用再继续淘汰了
         *          RT_ONLY_KEY:只有Key, 删除成功
         *          RT_DIRTY_DATA:数据是脏数据，当bCheckDirty=true时会有可能产生这种返回值
         *          RT_ERASE_OK:淘汰当前数据成功, 继续淘汰
         *          其他返回值: 错误, 通常忽略, 继续调用erase淘汰
         */
        int erase(int ratio, vector<Value> &vtData, bool bCheckDirty = false);

        int pushList(const string &mk, const vector<pair<uint32_t, string> > &vt, const bool bHead, const bool bReplace, const uint64_t iPos, const uint32_t iNowTime, vector<Value> &vtData);

        int pushList(const string &mk, const vector<Value> &vt, vector<Value> &vtData);

        int getList(const string &mk, const uint64_t iStart, const uint64_t iEnd, const uint32_t iNowTime, vector<string> &vs);

        int trimList(const string &mk, const bool bPop, const bool bHead, const bool bTrim, const uint64_t iStart, const uint64_t iCount, const uint32_t iNowTime, string &value, uint64_t &delSize);

        int addSet(const string &mk, const string &v, const uint32_t unHash, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, DELETETYPE deleteType, vector<Value> &vtData);

        int addSet(const string &mk, const vector<Value> &vt, DATATYPE eType, vector<Value> &vtData);

        int addSet(const string &mk, vector<Value> &vtData);

        int getSet(const string &mk, const uint32_t iNowTime, vector<Value> &vtData);

        int delSetSetBit(const string &mk, const string &v, const uint32_t unHash, const time_t t);

        int delSetReal(const string &mk, const string &v, const uint32_t unHash);

        int delSetSetBit(const string &mk, const time_t t);

        int addZSet(const string &mk, const string &v, const uint32_t unHash, double iScore, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, bool bInc, DELETETYPE deleteType, vector<Value> &vtData);

        int addZSet(const string &mk, const vector<Value> &vt, DATATYPE eType, vector<Value> &vtData);

        int addZSet(const string &mk, vector<Value> &vtData);

        int getZSet(const string &mk, const uint64_t iStart, const uint64_t iEnd, const bool bUp, const uint32_t iNowTime, list<Value> &vtData);

        int getZSet(const string &mk, const string &v, const uint32_t unHash, const uint32_t iNowTime, Value& vData);

        // Changed by tutuli 2017-7-24 16:03
        // 支持getZSet的limit操作
        int getZSetLimit(const string &mk, const size_t iStart, const size_t iDataCount, const bool bUp, const uint32_t iNowTime, list<Value> &vtData);

        int getScoreZSet(const string &mk, const string &v, const uint32_t unHash, const uint32_t iNowTime, double &iScore);

        int getRankZSet(const string &mk, const string &v, const uint32_t unHash, const bool order, const uint32_t iNowTime, long &iPos);

        int getZSetByScore(const string &mk, const double iMin, const double iMax, const uint32_t iNowTime, list<Value> &vtData);

        int delZSetSetBit(const string &mk, const string &v, const uint32_t unHash, const time_t t);

        int delRangeZSetSetBit(const string &mk, const double iMin, const double iMax, const uint32_t iNowTime, const time_t t);

        int delZSetReal(const string &mk, const string &v, const uint32_t unHash);

        int delZSetSetBit(const string &mk, const time_t t);

        int updateZSet(const string &mk, const string &oldValue, const string &newValue, double iScore, uint32_t iExpireTime, char iVersion, bool bDirty, bool bOnlyScore, vector<Value> &vtData);

        /**
         * 回写, 每次返回需要回写的一条
         * 数据回写时间与当前时间超过_pHead->_iSyncTime则需要回写
         * _pHead->_iSyncTime由setSyncTime函数设定, 默认10分钟

         * 外部循环调用该函数进行回写
         * map只读时仍然可以回写
         * @param iNowTime: 当前时间
         *                  回写时间与当前时间相差_pHead->_iSyncTime都需要回写
         * @param data : 返回需要回写的数据
         * @return int:
         *          RT_OK: 到脏数据链表头部了, 可以sleep一下再尝试
         *          RT_ONLY_KEY:只有Key, 删除成功, 当前数据不要缓写,继续调用sync回写
         *          RT_NEED_SYNC:当前返回的data数据需要回写
         *          RT_NONEED_SYNC:当前返回的data数据不需要回写
         *          其他返回值: 错误, 通常忽略, 继续调用sync回写_
         */
        int sync(uint32_t iNowTime, Value &data);

        /**
         * 开始回写, 调整回写指针
         */
        void sync();

        /**
         * 开始备份之前调用该函数
         *
         * @param bForceFromBegin: 是否强制从头开始备份
         * @return void
         */
        void backup(bool bForceFromBegin = false);

        /**
         * 开始备份数据, 每次返回需要备份的数据(一个主key下的所有数据)
         * @param data
         *
         * @return int
         *          RT_OK: 备份完毕
         *          RT_NEED_BACKUP:当前返回的data数据需要备份
         *			RT_PART_DATA:当前返回的数据需要备份，并且数据不全
         *          RT_ONLY_KEY:只有Key, 当前数据不要备份
         *          其他返回值: 错误, 通常忽略, 继续调用backup
         */
        int backup(vector<Value> &vtData);

        /**
         * 统计_iReadP后面数据占用空间大小，每次返回10个数据
         *
         * @return int
         */
        int calculateData(uint32_t &count, bool &isEnd);

        /**
        * 重置_iReadP
        *
        * @return int
        */
        int resetCalculatePoint();

        /////////////////////////////////////////////////////////////////////////////////////////
        // 以下是遍历map函数, 需要对map加锁

        /**
         * 结束
         *
         * @return
         */
        lock_iterator end() { return _lock_end; }

        /**
         * block正序
         *
         * @return lock_iterator
         */
        lock_iterator begin();

        /**
         * block逆序
         *
         * @return lock_iterator
         */
        lock_iterator rbegin();

        /**
         * 以Set时间排序的迭代器
         *
         * @return lock_iterator
         */
        lock_iterator beginSetTime();

        /**
         * Set链逆序的迭代器
         *
         * @return lock_iterator
         */
        lock_iterator rbeginSetTime();

        /**
         * 以Get时间排序的迭代器
         *
         * @return lock_iterator
         */
        lock_iterator beginGetTime();

        /**
         * Get链逆序的迭代器
         *
         * @return lock_iterator
         */
        lock_iterator rbeginGetTime();

        /**
         * 获取脏链表尾部迭代器(最长时间没有操作的脏数据)
         *
         * 返回的迭代器++表示按照时间顺序==>(最短时间没有操作的脏数据)
         *
         * @return lock_iterator
         */
        lock_iterator beginDirty();

        /////////////////////////////////////////////////////////////////////////////////////////
        // 以下是遍历map函数, 不需要对map加锁

        /**
         * 根据hash桶遍历
         *
         * @return hash_iterator
         */
        hash_iterator hashBegin();

        /**
         * 结束
         *
         * @return
         */
        hash_iterator hashEnd() { return _end; }

        /**
        * 根据主key的hash桶遍历
        *
        * @return mhash_iterator
        */
        mhash_iterator mHashBegin();

        /**
         * 结束
         *
         * @return
         */
        mhash_iterator mHashEnd() { return _mk_end; }
        /**
         * 根据Key查找数据
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @return lock_iterator
         */
        lock_iterator find(const string &mk, const string &uk);

        /**
        * 获取主key链上block的数量
        * @param mk: 主key
        * @return size_t
        */
        size_t count(const string &mk);

        /**
        * 根据主key查找第一个block位置，与count配合可以遍历某个主key下的所有数据
        * 也可以直接使用迭代器，直到end
        * @param mk: 主key
        * @return lock_iterator
        */
        lock_iterator find(const string &mk);

        /**
         * 描述
         *
         * @return string
         */
        string desc();

        /**
         * 描述
         *
         * @return string
         */
        string descWithHash();

        /**
        * 对可能的坏block进行检查，并可进行修复
        * @param iHash, hash索引
        * @param bRepaire, 是否进行修复
        *
        * @return size_t, 返回坏数据个数
        */
        size_t checkBadBlock(uint32_t iHash, bool bRepair);

    protected:

        friend class MainKey;
        friend class Block;
        friend class BlockAllocator;
        friend class HashMapIterator;
        friend class HashMapItem;
        friend class HashMapLockIterator;
        friend class HashMapLockItem;

        //禁止copy构造
        TC_Multi_HashMap_Malloc(const TC_Multi_HashMap_Malloc &mcm);
        //禁止复制
        TC_Multi_HashMap_Malloc &operator=(const TC_Multi_HashMap_Malloc &mcm);

        // 用于数据更新过程失败的自动恢复
        // 在所有可能进行关键数据更新的函数的最开始构造
        /*
        struct FailureRecover
        {
            FailureRecover(TC_Multi_HashMap_Malloc *pMap) : _pMap(pMap)
            {
                // 构造时恢复可能损坏的数据
                _pMap->doRecover();
                assert(_iRefCount ++ == 0);
            }

            ~FailureRecover()
            {
                // 析构时清理已经成功更新的数据
                _pMap->doUpdate();
                assert(_iRefCount-- == 1);
            }

        protected:
            TC_Multi_HashMap_Malloc   *_pMap;
            // 避免嵌套调用
            static int				  _iRefCount;
        };
        */

        /*
        int incRefCount()
        {
            return _iRefCount++;
        }

        int decRefCount()
        {
            return _iRefCount--;
        }
        */

        friend class FailureRecover;

        class FailureRecover
        {
        public:
            FailureRecover(TC_Multi_HashMap_Malloc *pMap) : _pMap(pMap)
            {
                // 构造时恢复可能损坏的数据
                _pMap->doRecover();
                assert(_pMap->_iRefCount++ == 0);
                //assert(_pMap->incRefCount() == 0);
            }

            ~FailureRecover()
            {
                // 析构时清理已经成功更新的数据
                _pMap->doUpdate();
                assert(_pMap->_iRefCount-- == 1);
                //assert(_pMap->decRefCount() == 1);
            }

        protected:
            TC_Multi_HashMap_Malloc   *_pMap;
            // 避免嵌套调用
            //static int				  _iRefCount;
        };

        class Random
        {
        public:
            uint32_t seed_;

            //Random(uint32_t s) : seed_(s & 0x7fffffffu) { }

            uint32_t Next()
            {
                static const uint32_t M = 2147483647L;   // 2^31-1
                static const uint64_t A = 16807;  // bits 14, 8, 7, 5, 2, 1, 0
                // We are computing
                //       seed_ = (seed_ * A) % M,    where M = 2^31-1
                //
                // seed_ must not be zero or M, or else all subsequent computed values
                // will be zero or M respectively.  For all other values, seed_ will end
                // up cycling through every number in [1,M-1]
                uint64_t product = seed_ * A;

                // Compute (product % M) using the fact that ((x << 31) % M) == x.
                seed_ = static_cast<uint32_t>((product >> 31) + (product & M));
                // The first reduction may overflow by 1 bit, so we may need to
                // repeat.  mod == M is not possible; using > allows the faster
                // sign-bit-based test.
                if (seed_ > M)
                {
                    seed_ -= M;
                }
                return seed_;
            }

            // Returns a uniformly distributed value in the range [0..n-1]
            // REQUIRES: n > 0
            uint32_t Uniform(int n) { return Next() % n; }

            // Randomly returns true ~"1/n" of the time, and false otherwise.
            // REQUIRES: n > 0
            bool OneIn(int n) { return (Next() % n) == 0; }

            // Skewed: pick "base" uniformly from range [0,max_log] and then
            // return "base" random bits.  The effect is to pick a number in the
            // range [0,2^max_log-1] with exponential bias towards smaller numbers.
            uint32_t Skewed(int max_log)
            {
                return Uniform(1 << Uniform(max_log + 1));
            }
        };

        /**
         * 初始化
         * @param pAddr, 外部分配好的存储地址
         */
        void init(void *pAddr);

        /**
         * 增加脏数据个数
         */
        void incDirtyCount() { saveValue(&_pHead->_iDirtyCount, _pHead->_iDirtyCount + 1); }

        /**
         * 减少脏数据个数
         */
        void delDirtyCount() { saveValue(&_pHead->_iDirtyCount, _pHead->_iDirtyCount - 1); }

        /**
         * 增加数据个数
         */
        void incElementCount() { saveValue(&_pHead->_iElementCount, _pHead->_iElementCount + 1); }

        /**
         * 增加主key个数
         */
        void incMainKeyCount() { saveValue(&_pHead->_iMainKeyCount, _pHead->_iMainKeyCount + 1); }

        /**
         * 减少数据个数
         */
        void delElementCount() { saveValue(&_pHead->_iElementCount, _pHead->_iElementCount - 1); }

        /**
         * 减少主key个数
         */
        void delMainKeyCount() { saveValue(&_pHead->_iMainKeyCount, _pHead->_iMainKeyCount - 1); }

        /**
         * 增加主键下OnlyKey数据个数
         */
        void incOnlyKeyCount() { saveValue(&_pHead->_iOnlyKeyCount, _pHead->_iOnlyKeyCount + 1); }

        /**
         * 减少主键下OnlyKey数据个数
         */
        void delOnlyKeyCount() { saveValue(&_pHead->_iOnlyKeyCount, _pHead->_iOnlyKeyCount - 1); }

        /**
         * 增加主key下OnlyKey数据个数
         */
        void incOnlyKeyCountM() { saveValue(&_pHead->_iMKOnlyKeyCount, _pHead->_iMKOnlyKeyCount + 1); }

        /**
         * 减少主key下OnlyKey数据个数
         */
        void delOnlyKeyCountM() { saveValue(&_pHead->_iMKOnlyKeyCount, _pHead->_iMKOnlyKeyCount - 1); }

        /**
         * 增加Chunk数
         */
        void incChunkCount(bool bUpdateDirect = true)
        {
            if (bUpdateDirect)
            {
                ++_pHead->_iDataUsedChunk;
            }
            else
                saveValue(&_pHead->_iDataUsedChunk, _pHead->_iDataUsedChunk + 1);
        }

        /**
         * 增加已经使用的数据内存
         * @param iMemSize
         * @param bUpdateDirect
         */
        void incUsedDataMemSize(size_t iMemSize, bool bUpdateDirect = true)
        {
            assert(_pHead->_iUsedDataMem + iMemSize <= getDataMemSize());
            if (bUpdateDirect)
            {
                _pHead->_iUsedDataMem += iMemSize;
            }
            else
                saveValue(&_pHead->_iUsedDataMem, _pHead->_iUsedDataMem + iMemSize);
        }

        /**
        * 增加主key已使用的chunk数
        */
        void incMainKeyChunkCount(bool bUpdateDirect = true)
        {
            if (bUpdateDirect)
            {
                ++_pHead->_iMKUsedChunk;
            }
            else
                saveValue(&_pHead->_iMKUsedChunk, _pHead->_iMKUsedChunk + 1);
        }

        /**
         * 增加主key使用的内存
         * @param iMemSize
         * @param bUpdateDirect
         */
        void incMainKeyUsedMemSize(size_t iMemSize, bool bUpdateDirect = true)
        {
            if (_iMainKeySize > 0)
            {
                assert(_pHead->_iMKUsedMem + iMemSize <= getMainKeyMemSize());
                if (bUpdateDirect)
                {
                    _pHead->_iMKUsedMem += iMemSize;
                }
                else
                    saveValue(&_pHead->_iMKUsedMem, _pHead->_iMKUsedMem + iMemSize);
            }
            else
            {
                assert(_pHead->_iUsedDataMem + iMemSize <= getDataMemSize());
                if (bUpdateDirect)
                {
                    _pHead->_iUsedDataMem += iMemSize;
                    _pHead->_iMKUsedMem += iMemSize;
                }
                else
                {
                    saveValue(&_pHead->_iUsedDataMem, _pHead->_iUsedDataMem + iMemSize);
                    saveValue(&_pHead->_iMKUsedMem, _pHead->_iMKUsedMem + iMemSize);
                }
            }
        }

        /**
         * 减少Chunk数
         */
        void delChunkCount(bool bUpdateDirect = true)
        {
            if (bUpdateDirect)
            {
                --_pHead->_iDataUsedChunk;
            }
            else
                saveValue(&_pHead->_iDataUsedChunk, _pHead->_iDataUsedChunk - 1);
        }

        /**
         * 减少已经使用的数据内存
         * @param iMemSize
         * @param bUpdateDirect
         */
        void delUsedDataMemSize(size_t iMemSize, bool bUpdateDirect = true)
        {
            assert(_pHead->_iUsedDataMem >= max((size_t)_pHead->_iMinChunkSize, iMemSize));
            if (bUpdateDirect)
            {
                _pHead->_iUsedDataMem -= iMemSize;
            }
            else
                saveValue(&_pHead->_iUsedDataMem, _pHead->_iUsedDataMem - iMemSize);
        }

        /**
        * 减少主key已使用的chunk数
        */
        void delMainKeyChunkCount(bool bUpdateDirect = true)
        {
            if (bUpdateDirect)
            {
                --_pHead->_iMKUsedChunk;
            }
            else
                saveValue(&_pHead->_iMKUsedChunk, _pHead->_iMKUsedChunk - 1);
        }

        void delMainKeyUsedMemSize(size_t iMemSize, bool bUpdateDirect = true)
        {
            if (_iMainKeySize > 0)
            {
                assert(_pHead->_iMKUsedMem >= max((size_t)_pHead->_iMinChunkSize, iMemSize));
                if (bUpdateDirect)
                {
                    _pHead->_iMKUsedMem -= iMemSize;
                }
                else
                    saveValue(&_pHead->_iMKUsedMem, _pHead->_iMKUsedMem - iMemSize);
            }
            else
            {
                assert(_pHead->_iUsedDataMem >= max((size_t)_pHead->_iMinChunkSize, iMemSize));
                if (bUpdateDirect)
                {
                    _pHead->_iUsedDataMem -= iMemSize;
                    _pHead->_iMKUsedMem -= iMemSize;
                }
                else
                {
                    saveValue(&_pHead->_iUsedDataMem, _pHead->_iUsedDataMem - iMemSize);
                    saveValue(&_pHead->_iMKUsedMem, _pHead->_iMKUsedMem - iMemSize);
                }
            }
        }

        int RandomHeight();

        int insertSkipList(MainKey &mainKey, uint32_t blockAddr, double score);

        int delSkipList(MainKey &mainKey, uint32_t blockAddr);

        int insertSkipListSpan(MainKey &mainKey, uint32_t blockAddr);

        int delSkipListSpan(MainKey &mainKey, uint32_t blockAddr);

        /**
         * 增加hit次数
         */
        void incGetCount() { ++_pHead->_iGetCount; /*saveValue(&_pHead->_iGetCount, _pHead->_iGetCount+1);*/ }

        /**
         * 增加命中次数
         */
        void incHitCount() { ++_pHead->_iHitCount; /*saveValue(&_pHead->_iHitCount, _pHead->_iHitCount+1);*/ }

        /**
         * 某hash链表数据个数+1
         * @param index
         */
        void incListCount(uint32_t index) { saveValue(&item(index)->_iListCount, item(index)->_iListCount + 1); }

        /**
        * 某hash值主key链上主key个数+1
        */
        void incMainKeyListCount(uint32_t index) { saveValue(&itemMainKey(index)->_iListCount, itemMainKey(index)->_iListCount + 1); }

        /**
         * 某hash链表数据个数-1
         * @param index
         */
        void delListCount(uint32_t index) { saveValue(&item(index)->_iListCount, item(index)->_iListCount - 1); }

        /**
        * 某hash值主key链上主key个数-1
        */
        void delMainKeyListCount(uint32_t index) { saveValue(&itemMainKey(index)->_iListCount, itemMainKey(index)->_iListCount - 1); }

        /**
        * 某hash值主key链上blockdata个数+/-1
        * @param mk, 主key
        * @param bInc, 是增加还是减少
        */
        void incMainKeyBlockCount(const string &mk, bool bInc = true);

        /**
        * 更新主key下面最大记录数信息
        */
        void updateMaxMainKeyBlockCount(size_t iCount);

        /**
         * 相对地址换成绝对地址
         * @param iAddr
         *
         * @return void*
         */
        void *getAbsolute(uint32_t iAddr) { if (iAddr == 0) return NULL; return _pDataAllocator->_pChunkAllocator->getAbsolute((iAddr >> 9) - 1, iAddr & 0x1FF); }

        /**
         * 绝对地址换成相对地址
         *
         * @return size_t
         */
         //uint32_t getRelative(void *pAddr) { return (uint32_t)_pDataAllocator->_pChunkAllocator->getRelative(pAddr); }

         /**
          * 主key chunk区相对地址换成绝对地址
          * @param iAddr
          *
          * @return void*
          */
        void *getMainKeyAbsolute(uint32_t iAddr) { if (iAddr == 0) return NULL; return _pMainKeyAllocator->_pChunkAllocator->getAbsolute((iAddr >> 9) - 1, iAddr & 0x1FF); }

        /**
         * 主key chunk区绝对地址换成相对地址
         *
         * @return size_t
         */
         //uint32_t getMainKeyRelative(void *pAddr) { return (uint32_t)_pMainKeyAllocator->_pChunkAllocator->getRelative(pAddr); }

         /**
          * 淘汰iNowAddr之外的数据(根据淘汰策略淘汰)
          * @param iNowAddr, 当前主key正在分配内存，是不能被淘汰的
          *                  0表示做直接根据淘汰策略淘汰
          * @param vector<Value>, 被淘汰的数据
          * @return size_t, 淘汰的数据个数
          */
        size_t eraseExcept(uint32_t iNowAddr, vector<Value> &vtData);

        /**
         * 根据Key计算hash值
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         *
         * @return uint32_t
         */
        uint32_t hashIndex(const string &mk, const string &uk);

        /**
         * 根据Key计算hash值
         * @param k: key
         *
         * @return uint32_t
         */
        uint32_t hashIndex(const string &k);

        /**
        * 根据主key计算主key的hash
        * @param mk: 主key
        * @return uint32_t
        */
        uint32_t mhashIndex(const string &mk);

        /**
        * 根据主key计算主key的hash值
        * @param mk: 主key
        * @return uint32_t
        */
        uint32_t mhash(const string &mk);

        /**
         * 根据hash索引查找指定key(mk+uk)的数据的位置, 并返回数据
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param index: 联合主键的hash索引
         * @param v: 如果存在数据，则返回数据值
         * @param ret: 具体的返回值
         * @return lock_iterator: 返回找到的数据的位置，不存在则返回end()
         *
         */
        lock_iterator find(const string &mk, const string &uk, uint32_t index, Value &v, int &ret);

        /**
         * 根据hash索引查找指定key(mk+uk)的数据的位置
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param index: 联合主键的hash索引
         * @param ret: 具体的返回值
         * @return lock_iterator: 返回找到的数据的位置，不存在则返回end()
         *
         */
        lock_iterator find(const string &mk, const string &uk, uint32_t index, int &ret);

        lock_iterator findSet(const string &mk, const string &v, uint32_t index, int &ret);

        lock_iterator findZSet(const string &mk, const string &v, uint32_t index, int &ret);

        /**
        * 根据主key hash索引查找主key的地址，找不到返回0
        * @param mk: 主key
        * @param index: 主key hash索引
        * @param ret: 具体返回值
        * @return uint32_t: 返回找到的主key的首地址，找不到返回0
        */
        uint32_t find(const string &mk, uint32_t index, int &ret);

        /**
        * 刷新联合key的位置
        * @param mk: 主key
        * @param uk: 除主key外的联合主键
        * @param bHead: 是否调整到链表头部
        */
        int refreshKeyList(const string &mk, const string &uk, uint32_t unHash, bool bHead);

        /**
         * 分析主键hash的数据
         * @param iMaxHash: 最大的block hash桶上元素个数
         * @param iMinHash: 最小的block hash桶上元素个数
         * @param fAvgHash: 平均元素个数
         */
        void analyseHash(uint32_t &iMaxHash, uint32_t &iMinHash, float &fAvgHash);

        /**
         * 分析主key hash的数据
         * @param iMaxHash: 最大的主key hash桶上元素个数
         * @param iMinHash: 最小的主key hash桶上元素个数
         * @param fAvgHash: 平均元素个数
         */
        void analyseHashM(uint32_t &iMaxHash, uint32_t &iMinHash, float &fAvgHash);

        /**
        * 修改具体的值
        * @param pModifyAddr
        * @param iModifyValue
        */
        template<typename T>
        void saveValue(void* pModifyAddr, T iModifyValue)
        {
            //获取原始值
            T tmp = *(T*)pModifyAddr;

            //保存原始值
            _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex]._iModifyAddr = (char*)pModifyAddr - (char*)_pHead;
            _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex]._iModifyValue = tmp;
            _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex]._cBytes = sizeof(iModifyValue);
            _pstCurrModify->_cModifyStatus = 1;

            _pstCurrModify->_iNowIndex++;

            //修改具体值
            *(T*)pModifyAddr = iModifyValue;

            assert(_pstCurrModify->_iNowIndex < sizeof(_pstCurrModify->_stModifyData) / sizeof(tagModifyData));
        }

        /**
        * 对某个值的某位进行更新
        * @param pModifyAddr, 待修改的(整型数)内存地址
        * @param bit, 需要修改的整型数的位
        * @param b, 需要修改的整型数位的值
        */
        template<typename T>
        void saveValue(T *pModifyAddr, uint8_t bit, bool b)
        {
            T tmp = *pModifyAddr;	// 取出原值
            if (b)
            {
                tmp |= 0x01 << bit;
            }
            else
            {
                tmp &= T(-1) ^ (0x01 << bit);
            }
            saveValue(pModifyAddr, tmp);
        }

        void saveAddr(uint32_t iAddr, char cByte, bool bMainKey = false);

        /**
        * 恢复数据
        */
        void doRecover();

        /**
        * 确认处理完毕
        */
        void doUpdate();

        /**
         * 确认处理完毕
         */
        void doUpdate2();

        /**
         * 确认处理完毕
         */
        void doUpdate3();

        /**
         * 确认处理完毕
         */
        void doUpdate4();

        /**
         * 获取大于n且离n最近的素数
         * @param n
         *
         * @return size_t
         */
        size_t getMinPrimeNumber(size_t n);

        /**
         * 释放保护区中的chunk链
         */
        void deallocate(uint32_t iChunk, size_t iIndex, bool bMainKey);

        /**
         * 释放保护区中的整个block
         */
        void deallocate2();

        /**
         * 释放保护区中的一个主key或者Block头部
         */
        void deallocate3(uint32_t iHead, size_t iIndex, bool bMainKey);

    protected:

        /**
         * 头部指针
         */
        tagMapHead                  *_pHead;

        /**
         * 允许最小的chunk块大小
         */
        const size_t              	_iMinChunkSize;

        /**
        * 主key数据大小，粗略平均值
        */
        size_t						_iMainKeySize;

        /**
         * 平均的数据块大小
         */
        size_t                      _iDataSize;

        /**
         * 设置chunk数据块/hash项比值
         */
        float                       _fHashRatio;

        /**
        * 主key hash个数/联合hash个数
        */
        float						_fMainKeyRatio;

        /**
         * 联合主键hash索引区
         */
        TC_MemVector<tagHashItem>   _hash;

        /**
        * 主key hash索引区
        */
        TC_MemVector<tagMainKeyHashItem>	_hashMainKey;

        /**
         * 修改数据块
         */
        tagModifyHead               *_pstInnerModify;

        /**
        * 用于备份数据修改区修改记录
        * 如果嵌套的函数调用里有doUpdate操作，必须在上层函数里先调用backupModifyData备份
        * 调用完下层函数后使用restoreModifyData进行还原
        */
        tagModifyHead				*_pstOuterModify;

        /**
         * 当前使用的数据保护区
         */
        tagModifyHead				*_pstCurrModify;

        /**
        * 主key的chunk分配器
        */
        BlockAllocator				*_pMainKeyAllocator;

        /**
         * 数据区的chunk分配器对象
         */
        BlockAllocator              *_pDataAllocator;

        /**
         * 尾部
         */
        lock_iterator               _lock_end;

        /**
         * 尾部
         */
        hash_iterator               _end;

        /**
        * 尾部
        */
        mhash_iterator               _mk_end;
        /**
         * 联合主键hash值计算公式
         */
        hash_functor                _hashf;

        /**
        * 主key的hash计算函数, 如果不提供，将使用上面的_hashf
        */
        hash_functor				_mhashf;


        // 避免嵌套调用,应用于FailureRecover 内
        int				  _iRefCount;

        //用于Block中get 的临时缓存，避免内存的频繁分配和释放
        size_t _tmpBufSize;
        char* _tmpBuf;

        //用于统计最近未访问的数据
        uint32_t _iReadP;
        uint32_t _iReadPBak;

        Random _rand;

        uint32_t *_prev;
    };

}

#endif

