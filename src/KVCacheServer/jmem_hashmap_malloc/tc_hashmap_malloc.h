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
#ifndef	__TC_HASHMAP_MALLOC_H__
#define __TC_HASHMAP_MALLOC_H__

#include <vector>
#include <memory>
#include <cassert>
#include <iostream>
#include "util/tc_ex.h"
#include "util/tc_mem_vector.h"
#include "util/tc_pack.h"
#include "tc_malloc_chunk.h"
#include "util/tc_hash_fun.h"
#include "util/tc_thread.h"
#include "CacheShare.h"


namespace DCache
{
/////////////////////////////////////////////////
// 说明: hashmap类(类似于TC_HashMapCompact，只是使用的内存分配器不一样，所以初始化时略有不同）
// Author : jarodruan@tencent.com
/////////////////////////////////////////////////
/**
* Hash map异常类
*/
struct TC_HashMapMalloc_Exception : public tars::TC_Exception
{
    TC_HashMapMalloc_Exception(const string &buffer) : TC_Exception(buffer) {};
    TC_HashMapMalloc_Exception(const string &buffer, int err) : TC_Exception(buffer, err) {};
    ~TC_HashMapMalloc_Exception() throw() {};
};

    ////////////////////////////////////////////////////////////////////////////////////
    /**
     * 基于内存的hashmap
     * 所有操作需要自己加锁
     */
    class TC_HashMapMalloc
    {
    public:
        struct HashMapIterator;
        struct HashMapLockIterator;

        //操作数据
        struct BlockData
        {
            string  _key;       //数据Key
            string  _value;     //数据value
            bool    _dirty;     //是否是脏数据
            uint32_t _synct;     //sync time, 不一定是真正的回写时间
            uint32_t _expiret;	// 数据过期的绝对时间，由设置或更新数据时提供，0表示不关心此时间
            uint8_t	_ver;		// 数据版本，1为初始版本，0为保留
            BlockData()
                : _dirty(false)
                , _synct(0)
                , _expiret(0)
                , _ver(1)
            {
            }
        };

        ///////////////////////////////////////////////////////////////////////////////////
        /**
        * 内存数据块
        *
        * 读取和存放数据
        */

        // 试图在Block内部访问TC_HashMapMalloc的成员变量 _tmpBuf
        //class Block;
        //friend class Block;

        class Block
        {
        public:

            /**
             * block数据头
             */
            struct tagBlockHead
            {
                uint32_t    _iSize;         /**block的容量大小*/
                uint32_t    _iIndex;        /**hash的索引*/
                uint32_t    _iBlockNext;    /**下一个Block,tagBlockHead, 没有则为0*/
                uint32_t    _iBlockPrev;    /**上一个Block,tagBlockHead, 没有则为0*/
                uint32_t    _iSetNext;      /**Set链上的上一个Block*/
                uint32_t    _iSetPrev;      /**Set链上的上一个Block*/
                uint32_t    _iGetNext;      /**Get链上的上一个Block*/
                uint32_t    _iGetPrev;      /**Get链上的上一个Block*/
                uint32_t    _iSyncTime;     /**上次缓写时间*/
                uint32_t	_iExpireTime;	/** 数据过期的绝对时间，由设置或更新数据时提供，0表示不关心此时间*/
                uint8_t		_iVersion;		/** 数据版本，1为初始版本，0为保留*/
                bool        _bDirty;        /**是否是脏数据*/
                bool        _bOnlyKey;      /**是否只有key, 没有内容*/
                bool        _bNextChunk;    /**是否有下一个chunk*/
                union
                {
                    uint32_t  _iNextChunk;    /**下一个Chunk块, _bNextChunk=true时有效, tagChunkHead*/
                    uint32_t  _iDataLen;      /**当前数据块中使用了的长度, _bNextChunk=false时有效*/
                };
                char        _cData[0];      /**数据开始部分*/
            }__attribute__((packed));

            /**
             * 非头部的block, 称为chunk
             */
            struct tagChunkHead
            {
                uint32_t    _iSize;         /**block的容量大小*/
                bool        _bNextChunk;    /**是否还有下一个chunk*/
                union
                {
                    uint32_t  _iNextChunk;    /**下一个数据块, _bNextChunk=true时有效, tagChunkHead*/
                    uint32_t  _iDataLen;      /**当前数据块中使用了的长度, _bNextChunk=false时有效*/
                };
                char        _cData[0];      /**数据开始部分*/
            }__attribute__((packed));

            /**
             * 构造函数
             * @param Map
             * @param 当前MemBlock的地址
             * @param pAdd
             */
            Block(TC_HashMapMalloc *pMap, uint32_t iAddr)
                : _pMap(pMap)
                , _iHead(iAddr)
            {
                _pHead = getBlockHead(_iHead);
            }

            /**
             * copy
             * @param mb
             */
            Block(const Block &mb)
                : _pMap(mb._pMap)
                , _iHead(mb._iHead)
            {
                _pHead = getBlockHead(_iHead);
            }

            /**
             * 获取block头绝对地址
             * @param iAddr
             *
             * @return tagChunkHead*
             */
            tagBlockHead *getBlockHead(uint32_t iAddr) { return ((tagBlockHead*)_pMap->getAbsolute(iAddr)); }

            /**
             * 获取MemBlock头地址
             *
             * @return void*
             */
            tagBlockHead *getBlockHead() { return _pHead; }

            /**
             * 头部
             *
             * @return uint32_t
             */
            uint32_t getHead() { return _iHead; }

            /**
             * 当前桶链表最后一个block的头部
             *
             * @return uint32_t
             */
            uint32_t getLastBlockHead();

            /**
            * 获取数据的过期时间
            *
            * @return uint32_t，单位为秒，返回0表示无过期时间
            */
            uint32_t getExpireTime() { return getBlockHead()->_iExpireTime; }

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
             * 最新Get时间
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
             *
             * @return int
             *          TC_HashMapMalloc::RT_OK, 正常, 其他异常
             *          TC_HashMapMalloc::RT_ONLY_KEY, 只有Key
             *          其他异常
             */
            int getBlockData(TC_HashMapMalloc::BlockData &data);

            /**
             * 获取数据
             * @param pData
             * @param iDatalen
             * @return int,
             *          TC_HashMapMalloc::RT_OK, 正常
             *          其他异常
             */
            int get(void *pData, uint32_t &iDataLen);

            /**
             * 获取数据
             * @param s
             * @return int
             *          TC_HashMapMalloc::RT_OK, 正常
             *          其他异常
             */
            int get(string &s);

            /**
             * 获取数据
             * @param iDataLen
             * @param iRet
             * @return char*
             *          TC_HashMapMalloc::RT_OK, 正常
             *          其他异常
             */
            char* get(uint32_t &iDataLen, int &iRet);

            /**
             * 设置数据
             * @param data
             * @param bOnlyKey，是否onlykey
             * @param iExpireTime，数据过期时间，单位为秒，0表示不设置过期时间
             * @param iVersion，数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
             * @param bNewBlock，是否新的block
             * @param vtData，淘汰的数据
             *
             * @return int
             */
            int set(const string& data, bool bOnlyKey, uint32_t iExpireTime, uint8_t iVersion, bool bNewBlock, vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             * 设置数据
             * @param data
             * @param bOnlyKey，是否onlykey
             * @param iExpireTime，数据过期时间，单位为秒，0表示不设置过期时间
             * @param iVersion，数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
             * @param bNewBlock，是否新的block
             * @param bCheckExpire，是否开启过期
             * @param iNowTime，当前时间
             * @param vtData，淘汰的数据
             *
             * @return int
             */
            int set(const string& data, bool bOnlyKey, uint32_t iExpireTime, uint8_t iVersion, bool bNewBlock, bool bCheckExpire, uint32_t iNowTime, vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             * 是否是脏数据
             *
             * @return bool
             */
            bool isDirty() { return getBlockHead()->_bDirty; }

            /**
             * 设置数据
             * @param b
             */
            void setDirty(bool b);

            /**
             * 是否只有key
             *
             * @return bool
             */
            bool isOnlyKey() { return getBlockHead()->_bOnlyKey; }

            /**
             * 当前元素移动到下一个block
             * @return true, 移到下一个block了, false, 没有下一个block
             *
             */
            bool nextBlock();

            /**
             * 当前元素移动到上一个block
             * @return true, 移到下一个block了, false, 没有下一个block
             *
             */
            bool prevBlock();

            /**
             * 释放block的所有空间
             */
            void deallocate();

            /**
             * 新block时调用该函数
             * 分配一个新的block
             * @param index, hash索引
             * @param iAllocSize, 内存大小
             */
            void makeNew(uint32_t index, uint32_t iAllocSize);

            void makeNew(uint32_t index, uint32_t iAllocSize, Block &block);

            /**
             * 从Block链表中删除当前Block
             * 只对Block有效, 对Chunk是无效的
             * @return
             */
            void erase();

            /**
             * 刷新set链表, 放在Set链表头部
             */
            void refreshSetList();

            /**
             * 刷新get链表, 放在Get链表头部
             */
            void refreshGetList();

        protected:

            Block& operator=(const Block &mb);
            bool operator==(const Block &mb) const;
            bool operator!=(const Block &mb) const;

            /**
             * 获取Chunk头绝对地址
             *
             * @return tagChunkHead*
             */
            tagChunkHead *getChunkHead() { return getChunkHead(_iHead); }

            /**
             * 获取chunk头绝对地址
             * @param iAddr
             *
             * @return tagChunkHead*
             */
            tagChunkHead *getChunkHead(uint32_t iAddr) { return ((tagChunkHead*)_pMap->getAbsolute(iAddr)); }

            /**
             * 从当前的chunk开始释放
             * @param iChunk 释放地址
             */
            void deallocate(uint32_t iChunk);

            /**
             * 插入到hashmap的链中
             */
            void insertHashMap();

            /**
             * 如果数据容量不够, 则新增加chunk, 不影响原有数据
             * 使新增加的总容量大于iDataLen
             * 释放多余的chunk
             * @param iDataLen
             *
             * @return int,
             */
            int allocate(uint32_t iDataLen, vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             * 挂接chunk, 如果core则挂接失败, 保证内存块还可以用
             * @param pChunk
             * @param chunks
             *
             * @return int
             */
            int joinChunk(tagChunkHead *pChunk, const vector<uint32_t> chunks);

            /**
             * 分配n个chunk地址, 注意释放内存的时候不能释放正在分配的对象
             * @param fn, 分配空间大小
             * @param chunks, 分配成功返回的chunks地址列表
             * @param vtData, 淘汰的数据
             * @return int
             */
            int allocateChunk(uint32_t fn, vector<uint32_t> &chunks, vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             * 获取数据长度
             *
             * @return uint32_t
             */
            uint32_t getDataLen();

        private:

            /**
             * Map
             */
            TC_HashMapMalloc   *_pMap;

            /**
             * block区块首地址, 相对地址
             */
            uint32_t            _iHead;

            /**
             * 地址
             */
            tagBlockHead *      _pHead;
        };

        ////////////////////////////////////////////////////////////////////////
        /*
        * 内存数据块分配器
        *
        */
        class BlockAllocator
        {
        public:

            /**
             * 构造函数
             */
            BlockAllocator(TC_HashMapMalloc *pMap)
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
             * @param fFactor, 因子
             */
            void create(void *pHeadAddr, size_t iSize)
            {
                _pChunkAllocator->create(pHeadAddr, iSize);
            }

            /**
             * 连接上
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
             * 重建
             */
            void rebuild()
            {
                _pChunkAllocator->rebuild();
            }

            /**
             * 获取每种数据块头部信息
             *
             * @return TC_MemChunk::tagChunkHead
             */
             //vector<TC_MemChunk::tagChunkHead> getBlockDetail() const  { return _pChunkAllocator->getBlockDetail(); }

             /**
              * 内存大小
              *
              * @return size_t
              */
            size_t getMemSize() const { return _pChunkAllocator->getMemSize(); }

            /**
             * 获取总的、真正的数据容量
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
             * 在内存中分配一个新的Block
             *
             * @param index, block hash索引
             * @param iAllocSize: in/需要分配的大小, out/分配的块大小
             * @param vtData, 返回释放的内存块数据
             * @return size_t, 相对地址,0表示没有空间可以分配
             */
            uint32_t allocateMemBlock(uint32_t index, uint32_t &iAllocSize, vector<TC_HashMapMalloc::BlockData> &vtData);

            uint32_t relocateMemBlock(uint32_t srcAddr, uint8_t iVersion, uint32_t index, uint32_t &iAllocSize, vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             * 为地址为iAddr的Block分配一个chunk
             *
             * @param iAddr,分配的Block的地址
             * @param iAllocSize, in/需要分配的大小, out/分配的块大小
             * @param vtData 返回释放的内存块数据
             * @return size_t, 相对地址,0表示没有空间可以分配
             */
            uint32_t allocateChunk(uint32_t iAddr, uint32_t &iAllocSize, vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             * 释放Block
             * @param v
             */
            void deallocateMemBlock(const vector<uint32_t> &v);

            /**
             * 释放Block
             * @param iAddr，Block的地址
             */
            void deallocateMemBlock(uint32_t iAddr);

            /**
             * 释放chunk
             * @param iAddr，chunk的地址
             */
            void deallocateChunk(uint32_t iAddr);

            /**
             * 释放chunk
             * @param v
             */
            void deallocateChunk(const vector<uint32_t>& v);

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
            TC_HashMapMalloc          *_pMap;

            /**
             * chunk分配器
             */
            TC_MallocChunkAllocator   *_pChunkAllocator;
        };

        ////////////////////////////////////////////////////////////////
        // map的数据项
        class HashMapLockItem
        {
        public:

            /**
             *
             * @param pMap
             * @param iAddr
             */
            HashMapLockItem(TC_HashMapMalloc *pMap, uint32_t iAddr);

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
             * 获取值, 如果只有Key(isOnlyKey)的情况下, v为空
             * @return int
             *          RT_OK:数据获取OK
             *          RT_ONLY_KEY: key有效, v无效为空
             *          其他值, 异常
             *
             */
            int get(string& k, string& v);

            /**
             * 获取值
             * @return int
             *          RT_OK:数据获取OK
             *          其他值, 异常
             */
            int get(string& k);

            /**
             * 数据块相对地址
             *
             * @return uint32_t
             */
            uint32_t getAddr() const { return _iAddr; }

        protected:

            /*
             * 设置数据
             * @param k
             * @param v
             * @param iExpiretime, 数据过期时间，单位为秒，0表示不设置过期时间
             * @param iVersion, 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
             * @param vtData, 淘汰的数据
             * @return int
             */
            int set(const string& k, const string& v, uint32_t iExpireTime, uint8_t iVersion, bool bNewBlock, vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             * 设置Key, 无数据
             * @param k
             * @param vtData
             *
             * @return int
             */
            int set(const string& k, uint8_t iVersion, bool bNewBlock, vector<TC_HashMapMalloc::BlockData> &vtData);

            /*
             * 设置数据
             * @param packData，组包后的数据
             * @param iExpiretime, 数据过期时间，单位为秒，0表示不设置过期时间
             * @param iVersion, 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
             * @param bOnlyKey，是否onlykey
             * @param bNewBlock，是否新的block
             * @param vtData, 淘汰的数据
             * @return int
             */
            int set(const string& packData, bool bOnlyKey, uint32_t iExpireTime, uint8_t iVersion, bool bNewBlock, vector<TC_HashMapMalloc::BlockData> &vtData);

            /*
             * 设置数据
             * @param packData，组包后的数据
             * @param iExpiretime, 数据过期时间，单位为秒，0表示不设置过期时间
             * @param iVersion, 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
             * @param bOnlyKey，是否onlykey
             * @param bNewBlock，是否新的block
             * @param bCheckExpire，是否开启过期
             * @param iNowTime，当前时间
             * @param vtData, 淘汰的数据
             * @return int
             */
            int set(const string& packData, bool bOnlyKey, uint32_t iExpireTime, uint8_t iVersion, bool bNewBlock, bool bCheckExpire, uint32_t iNowTime, vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             *
             * @param pKey
             * @param iKeyLen
             *
             * @return bool
             */
            bool equal(const string &k, string &v, int &ret);

            /**
             *
             * @param pKey
             * @param iKeyLen
             *
             * @return bool
             */
            bool equal(const string& k, int &ret);

            /**
             * 下一个item
             *
             * @return HashMapLockItem
             */
            void nextItem(int iType);

            /**
             * 上一个item
             * @param iType
             */
            void prevItem(int iType);

            friend class TC_HashMapMalloc;
            friend struct TC_HashMapMalloc::HashMapLockIterator;

        private:
            /**
             * map
             */
            TC_HashMapMalloc *_pMap;

            /**
             * block的地址
             */
            uint32_t          _iAddr;
        };

        /////////////////////////////////////////////////////////////////////////
        // 定义迭代器
        struct HashMapLockIterator
        {
        public:

            //定义遍历方式
            enum
            {
                IT_BLOCK = 0,        //普通的顺序
                IT_SET = 1,        //Set时间顺序
                IT_GET = 2,        //Get时间顺序
            };

            /**
             * 迭代器的顺序
             */
            enum
            {
                IT_NEXT = 0,        //顺序
                IT_PREV = 1,        //逆序
            };

            /**
             *
             */
            HashMapLockIterator();

            /**
             * 构造函数
             * @param iAddr, 地址
             * @param type
             */
            HashMapLockIterator(TC_HashMapMalloc *pMap, uint32_t iAddr, int iType, int iOrder);

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
            TC_HashMapMalloc  *_pMap;

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
             * @param iIndex
             */
            HashMapItem(TC_HashMapMalloc *pMap, uint32_t iIndex);

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
             * 获取当前hash桶的所有数量, 注意只获取有key/value的数据
             * 对于只有key的数据, 不获取
             *
             * @return
             */
            void get(vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             * 获取当前hash桶的所有数据，包括onlyKey
             *
             * @return
             */
            void getAllData(vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             * 获取当前hash桶的所有key
             *
             * @return
             */
            void getKey(vector<string> &vtData);

            /**
             * 获取当前hash桶的过期数据, 注意只获取有key/value的数据
             * 对于只有key的数据, 不获取
             *
             * @return
             */
            void getExpire(uint32_t t, vector<TC_HashMapMalloc::BlockData> &vtData);

            /**
             *
             *
             * @return uint32_t
             */
            uint32_t getIndex() const { return _iIndex; }

            /**
             * 设置索引值
             */
            bool setIndex(uint32_t index);

            /**
             * 获取hash数量
             */
            uint32_t getHashCount() { return _pMap->getHashCount(); }

            /**
             * 下一个item
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
             * 设置当前hash桶下所有数据为脏数据，注意只设置有key/value的数据
             * 对于只有key的数据, 不设置
             * @param
             * @return int
             */
            int setClean();
            /**
             * 删除当前hash桶下所有onlykey数据
             * @param
             * @return int
             */
            int delOnlyKey();
            friend class TC_HashMapMalloc;
            friend struct TC_HashMapMalloc::HashMapIterator;

        private:
            /**
             * map
             */
            TC_HashMapMalloc *_pMap;

            /**
             * 数据块地址
             */
            uint32_t     _iIndex;
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
             * @param iIndex, 地址
             * @param type
             */
            HashMapIterator(TC_HashMapMalloc *pMap, uint32_t iIndex);

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

            //设置索引值
            bool setIndex(uint32_t index);

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
            TC_HashMapMalloc  *_pMap;

            /**
             *
             */
            HashMapItem _iItem;
        };

        //////////////////////////////////////////////////////////////////////////////////////////////////
        // map定义
        //
        /**
         * map头
         */
        struct tagMapHead
        {
            char        _cMaxVersion;        //大版本
            char        _cMinVersion;        //小版本
            bool        _bReadOnly;          //是否只读
            bool        _bAutoErase;         //是否可以自动淘汰
            char        _cEraseMode;         //淘汰方式:0x00:按照Get链淘汰, 0x01:按照Set链淘汰
            size_t      _iMemSize;           //内存大小
            uint32_t	_iAvgDataSize;		 //平均数据块大小 
            uint32_t    _iMinChunkSize;		 //允许最小的数据块大小
            float       _fRadio;             //元素个数/hash个数
            uint32_t    _iElementCount;      //总元素个数
            uint32_t    _iEraseCount;        //每次删除个数
            uint32_t    _iDirtyCount;        //脏数据个数
            uint32_t    _iSetHead;           //Set时间链表头部
            uint32_t    _iSetTail;           //Set时间链表尾部
            uint32_t    _iGetHead;           //Get时间链表头部
            uint32_t    _iGetTail;           //Get时间链表尾部
            uint32_t    _iDirtyTail;         //脏数据链尾部
            uint32_t    _iSyncTime;          //回写时间
            uint32_t    _iUsedChunk;         //已经使用的内存块
            size_t		_iUsedDataMem;	 	 //已经使用的数据内存 
            uint32_t    _iGetCount;          //get次数
            uint32_t    _iHitCount;          //命中次数
            uint32_t    _iBackupTail;        //热备指针
            uint32_t    _iSyncTail;          //回写链表
            uint32_t    _iOnlyKeyCount;		 // OnlyKey个数
            bool 		_bInit;				 //是否已经完成初始化
            char		_cReserve[16]; 		 //保留
        }__attribute__((packed));

        /**
         * 需要修改的地址
         */
        struct tagModifyData
        {
            size_t  _iModifyAddr;       //修改的地址
            char    _cBytes;            //字节数
            size_t  _iModifyValue;      //值
        }__attribute__((packed));

        /**
         * 修改数据块头部
         */
        struct tagModifyHead
        {
            char            _cModifyStatus;         //修改状态: 0:目前没有人修改, 1: 开始准备修改, 2:修改完毕, 没有copy到内存中
            uint32_t        _iNowIndex;             //更新到目前的索引, 不能操作10个
            tagModifyData   _stModifyData[1000];     //一次最多50000次修改
        }__attribute__((packed));

        /**
         * HashItem
         */
        struct tagHashItem
        {
            uint32_t _iBlockAddr;     //指向数据项的偏移地址
            uint32_t _iListCount;     //链表个数
        }__attribute__((packed));

        //64位操作系统用基数版本号, 32位操作系统用偶数版本号
#if __WORDSIZE == 64

    //定义版本号
        enum
        {
            MAX_VERSION = 6,    //当前map的大版本号
            MIN_VERSION = 1,    //当前map的小版本号
        };

#else
    //定义版本号
        enum
        {
            MAX_VERSION = 6,    //当前map的大版本号
            MIN_VERSION = 0,    //当前map的小版本号
        };

#endif

        //定义淘汰方式
        enum
        {
            ERASEBYGET = 0x00, //按照Get链表淘汰
            ERASEBYSET = 0x01, //按照Set链表淘汰
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
            RT_DATA_VER_MISMATCH = 11,   //写入数据版本不匹配
            RT_DATA_EXPIRED = 12,	//数据已过期 
            RT_DECODE_ERR = -1,   //解析错误
            RT_EXCEPTION_ERR = -2,   //异常
            RT_LOAD_DATA_ERR = -3,   //加载数据异常
            RT_VERSION_MISMATCH_ERR = -4,   //版本不一致
            RT_DUMP_FILE_ERR = -5,   //dump到文件失败
            RT_LOAL_FILE_ERR = -6,   //load文件到内存失败
            RT_NOTALL_ERR = -7,   //没有复制完全
            RT_DATATYPE_ERR = -8,   //数据类型错误
        };

        //定义迭代器
        typedef HashMapIterator     hash_iterator;
        typedef HashMapLockIterator lock_iterator;

        //定义hash处理器
//        typedef TC_Functor<size_t, TL::TLMaker<const string &>::Result> hash_functor;
	    typedef std::function<size_t(const string &)> hash_functor;

	    //////////////////////////////////////////////////////////////////////////////////////////////
        //map的接口定义

        /**
         * 构造函数
         */
        TC_HashMapMalloc()
            : _iMinChunkSize(64)
            , _iAvgDataSize(0)
            , _fRadio(2)
            , _pDataAllocator(new BlockAllocator(this))
            , _lock_end(this, 0, 0, 0)
            , _end(this, (uint32_t)(-1))
            , _hashf(tars::hash_new<string>())
        {
        }

        /**
         * 初始化数据块平均大小
         */
        void initAvgDataSize(uint32_t iAvgDataSize);

        /**
         * 始化chunk数据块/hash项比值, 默认是2,
         * 有需要更改必须在create之前调用
         *
         * @param fRadio
         */
        void initHashRadio(float fRadio) { _fRadio = fRadio; }

        /**
         * 初始化, 之前需要调用:initDataAvgSize和initHashRadio
         * @param pAddr 绝对地址
         * @param iSize 大小
         * @return 失败则抛出异常
         */
        void create(void *pAddr, size_t iSize);

        /**
         * 链接到内存块
         * @param pAddr, 地址
         * @param iSize, 内存大小
         * @return 失败则抛出异常
         */
        void connect(void *pAddr, size_t iSize);

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
          * 获取数据内存分配区的大小
          */
        size_t getDataMemSize() { return _pDataAllocator->getAllCapacity(); }

        /**
         * 获取hash桶的个数
         *
         * @return size_t
         */
        uint32_t getHashCount() { return _hash.size(); }

        /**
         * 元素的个数
         *
         * @return size_t
         */
        uint32_t size() { return _pHead->_iElementCount; }

        uint32_t usedChunkCount() { return _pHead->_iUsedChunk; }
        /**
         * 脏数据元素个数
         *
         * @return uint32_t
         */
        uint32_t dirtyCount() { return _pHead->_iDirtyCount; }

        /**
         * OnlyKey数据元素个数
         *
         * @return uint32_t
         */
        uint32_t onlyKeyCount() { return _pHead->_iOnlyKeyCount; }

        /**
         * 设置每次淘汰数量
         * @param n
         */
        void setEraseCount(uint32_t n) { _pHead->_iEraseCount = n; }

        /**
         * 获取每次淘汰数量
         *
         * @return uint32_t
         */
        uint32_t getEraseCount() { return _pHead->_iEraseCount; }

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
         * TC_HashMapMalloc::ERASEBYGET
         * TC_HashMapMalloc::ERASEBYSET
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
         * 设置回写时间(秒)
         * @param iSyncTime
         */
        void setSyncTime(uint32_t iSyncTime) { _pHead->_iSyncTime = iSyncTime; }

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
         * 设置hash方式
         * @param hash_of
         */
        void setHashFunctor(hash_functor hashf) { _hashf = hashf; }

        /**
         * 返回hash处理器
         *
         * @return hash_functor&
         */
        hash_functor &getHashFunctor() { return _hashf; }

        /**
         * hash item
         * @param index
         *
         * @return tagHashItem&
         */
        tagHashItem *item(uint32_t iIndex) { return &_hash[iIndex]; }

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
         * 修复hash索引为i的hash链(i不能操作hashmap的索引值)
         * @param i
         * @param bRepair
         *
         * @return int
         */
        int recover(size_t i, bool bRepair);

        /**
         * 清空hashmap
         * 所有map的数据恢复到初始状态
         */
        void clear();

        /**
         * 检查数据干净状态
         * @param k
         *
         * @return int
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key
         *          RT_DIRTY_DATA: 是脏数据
         *          RT_OK: 是干净数据
         *          其他返回值: 错误
         */
        int checkDirty(const string &k, bool bCheckExpire = false, uint32_t iNowTime = -1);

        /**
         * 设置为脏数据, 修改SET时间链, 会导致数据回写
         * @param k
         *
         * @return int
         *          RT_READONLY: 只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK: 设置脏数据成功
         *          其他返回值: 错误
         */
        int setDirty(const string& k);

        /**
         * 数据回写失败后重新设置为脏数据
         * @param k
         *
         * @return int
         *          RT_READONLY: 只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK: 设置脏数据成功
         *          其他返回值: 错误
         */
        int setDirtyAfterSync(const string& k);

        /**
         * 设置为干净数据, 修改SET链, 导致数据不回写
         * @param k
         *
         * @return int
         *          RT_READONLY: 只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int setClean(const string& k);

        /**
         * 获取数据, 修改GET时间链
         * @param k
         * @param v
         * @param iSyncTime:数据上次回写的时间
         * @param iExpiretime: 数据过期时间，单位为秒，0表示不设置过期时间
         * @param iVersion: 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
         *
         * @return int:
         *          RT_NO_DATA: 没有数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK:获取数据成功
         *          其他返回值: 错误
         */
        int get(const string& k, string &v, uint32_t &iSyncTime, uint32_t& iExpireTime, uint8_t& iVersion, bool bCheckExpire = false, uint32_t iNowTime = -1);

        /**
        * 获取数据, 修改GET时间链
        * @param k
        * @param v
        * @param iSyncTime:数据上次回写的时间
        * @param iExpiretime: 数据过期时间，单位为秒，0表示不设置过期时间
        * @param iVersion: 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
        *
        * @return int:
        *          RT_NO_DATA: 没有数据
        *          RT_ONLY_KEY:只有Key
        *          RT_OK:获取数据成功
        *          其他返回值: 错误
        */
        int get(const string& k, string &v, uint32_t &iSyncTime, uint32_t& iExpireTime, uint8_t& iVersion, bool& bDirty, bool bCheckExpire = false, uint32_t iNowTime = -1);

        /**
         * 获取数据, 修改GET时间链
         * @param k
         * @param v
         * @param iSyncTime:数据上次回写的时间

         *
         * @return int:
         *          RT_NO_DATA: 没有数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK:获取数据成功
         *          其他返回值: 错误
         */
        int get(const string& k, string &v, uint32_t &iSyncTime, bool bCheckExpire = false, uint32_t iNowTime = -1);

        /**
         * 获取数据, 修改GET时间链
         * @param k
         * @param v
         *
         * @return int:
         *          RT_NO_DATA: 没有数据
         *          RT_ONLY_KEY:只有Key
         *          RT_OK:获取数据成功
         *          其他返回值: 错误
         */
        int get(const string& k, string &v, bool bCheckExpire = false, uint32_t iNowTime = -1);

        /**
         * 设置数据, 修改时间链, 内存不够时会自动淘汰老的数据
         * @param k: 关键字
         * @param v: 值
         * @param bDirty: 是否是脏数据
         * @param vtData: 被淘汰的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int set(const string& k, const string& v, bool bDirty, vector<BlockData> &vtData);

        /*
         * 设置数据, 修改时间链, 内存不够时会自动淘汰老的数据
         * @param k: 关键字
         * @param v: 值
         * @param iExpiretime: 数据过期时间，单位为秒，0表示不设置过期时间
         * @param iVersion: 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
         * @param bDirty: 是否是脏数据
         * @param vtData: 被淘汰的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int set(const string& k, const string& v, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, vector<BlockData> &vtData);

        /*
         * 设置数据, 修改时间链, 内存不够时会自动淘汰老的数据
         * @param k: 关键字
         * @param v: 值
         * @param iExpiretime: 数据过期时间，单位为秒，0表示不设置过期时间
         * @param iVersion: 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
         * @param bDirty: 是否是脏数据
         * @param bCheckExpire，是否开启过期
         * @param iNowTime，当前时间
         * @param vtData: 被淘汰的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int set(const string& k, const string& v, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, bool bCheckExpire, uint32_t iNowTime, vector<BlockData> &vtData);

        /**
         * 设置key, 但无数据
         * @param k
         * @param vtData
         *
         * @return int
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int set(const string& k, vector<BlockData> &vtData);

        /**
         * 设置数据, 修改时间链, 内存不够时会自动淘汰老的数据
         * @param k: 关键字
         * @param v: 值
         * @param bDirty: 是否是脏数据
         * @param iExpireTime:过期时间
         * @param vtData: 被淘汰的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int update(const string& k, const string& v, Op option, bool bDirty, uint32_t iExpireTime, bool bCheckExpire, uint32_t iNowTime, string &retValue, vector<BlockData> &vtData);

        /**
         * 设置key, 但无数据
         * @param k
         * @param iVersion: 数据版本, 应该根据get出的数据版本写回，为0表示不关心数据版本
         * @param vtData
         *
         * @return int
         *          RT_READONLY: map只读
         *          RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int set(const string& k, uint8_t iVersion, vector<BlockData>& vtData);

        /**
         * 删除数据
         * @param k, 关键字
         * @param data, 被删除的记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_NO_DATA: 没有当前数据
         *          RT_ONLY_KEY:只有Key, 删除成功
         *          RT_OK: 删除数据成功
         *         其他返回值: 错误
         */
        int del(const string& k, BlockData &data);

        int del(const string& k, const uint8_t iVersion, BlockData &data);

        /**
         * 淘汰数据, 每次删除一条, 根据Get时间淘汰
         * 外部循环调用该接口淘汰数据
         * 直到: 元素个数/chunks * 100 < radio, bCheckDirty 为true时，遇到脏数据则淘汰结束
         * @param radio: 共享内存chunks使用比例 0< radio < 100
         * @param data: 当前被删除的一条记录
         * @return int:
         *          RT_READONLY: map只读
         *          RT_OK: 不用再继续淘汰了
         *          RT_ONLY_KEY:只有Key, 删除成功
         *          RT_DIRTY_DATA:数据是脏数据，当bCheckDirty=true时会有可能产生这种返回值
         *          RT_ERASE_OK:淘汰当前数据成功, 继续淘汰
         *          其他返回值: 错误, 通常忽略, 继续调用erase淘汰
         */
        int erase(int radio, BlockData &data, bool bCheckDirty = false);

        /**
         * 回写, 每次返回需要回写的一条
         * 数据回写时间与当前时间超过_pHead->_iSyncTime则需要回写
         * _pHead->_iSyncTime由setSyncTime函数设定, 默认10分钟

         * 外部循环调用该函数进行回写
         * map只读时仍然可以回写
         * @param iNowTime: 当前时间
         *                  回写时间与当前时间相差_pHead->_iSyncTime都需要回写
         * @param data : 回写的数据
         * @return int:
         *          RT_OK: 到脏数据链表头部了, 可以sleep一下再尝试
         *          RT_ONLY_KEY:只有Key, 删除成功, 当前数据不要缓写,继续调用sync回写
         *          RT_NEED_SYNC:当前返回的data数据需要回写
         *          RT_NONEED_SYNC:当前返回的data数据不需要回写
         *          其他返回值: 错误, 通常忽略, 继续调用sync回写
         */
        int sync(uint32_t iNowTime, BlockData &data);

        /**
         * 开始回写, 调整回写指针
         */
        void sync();

        /**
         * 开始备份之前调用该函数
         *
         * @param bForceFromBegin: 是否强制重头开始备份
         * @return void
         */
        void backup(bool bForceFromBegin = false);

        /**
         * 开始备份数据, 每次返回需要备份的一条数据
         * @param data
         *
         * @return int
         *          RT_OK: 备份完毕
         *          RT_NEED_BACKUP:当前返回的data数据需要备份
         *          RT_ONLY_KEY:只有Key, 当前数据不要备份
         *          其他返回值: 错误, 通常忽略, 继续调用backup
         */
        int backup(BlockData &data);

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
         * 根据Key查找数据
         * @param string
         */
        lock_iterator find(const string& k);

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
         * 描述
         *
         * @return string
         */
        string desc();

    public:

        //错误保护
        struct FailureRecover
        {
            FailureRecover(TC_HashMapMalloc *pMap) : _pMap(pMap)
            {
                _pMap->doRecover();
            }

            ~FailureRecover()
            {
                _pMap->doUpdate();
            }

        protected:
            TC_HashMapMalloc   *_pMap;
        };

    protected:

        friend class Block;
        friend class BlockAllocator;
        friend struct HashMapIterator;
        friend class HashMapItem;
        friend struct HashMapLockIterator;
        friend class HashMapLockItem;

        //禁止copy构造
        TC_HashMapMalloc(const TC_HashMapMalloc &mcm);
        //禁止复制
        TC_HashMapMalloc &operator=(const TC_HashMapMalloc &mcm);

        /**
         * 初始化
         * @param pAddr
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
         * 减少数据个数
         */
        void delElementCount() { saveValue(&_pHead->_iElementCount, _pHead->_iElementCount - 1); }

        /**
         * 增加OnlyKey数据个数
         */
        void incOnlyKeyCount() { saveValue(&_pHead->_iOnlyKeyCount, _pHead->_iOnlyKeyCount + 1); }

        /**
         * 减少OnlyKey数据个数
         */
        void delOnlyKeyCount() { saveValue(&_pHead->_iOnlyKeyCount, _pHead->_iOnlyKeyCount - 1); }

        /** modified by smitchzhao @ 2011-5-25
         * 增加Chunk数
         * 以选择是否可直接更新, 因为有可能一次分配的chunk个数
         * 多余更新区块的内存空间, 导致越界错误
         */
        void incChunkCount(bool bUpdateDirect = true)
        {
            if (bUpdateDirect)
            {
                ++_pHead->_iUsedChunk;
            }
            else
                saveValue(&_pHead->_iUsedChunk, _pHead->_iUsedChunk + 1);
        }

        /** modified by smitchzhao @ 2011-9-18
         * 增加已经使用的数据内存
         * @param iMemSize，增加内存大小
         */
        void incUsedDataMemSize(size_t iMemSize)
        {
            assert(_pHead->_iUsedDataMem + iMemSize <= getDataMemSize());
            _pHead->_iUsedDataMem += iMemSize;
        }

        /**
         * 减少Chunk数
         * 可以选择是否直接更新, 因为有可能一次释放的chunk个数
         * 多余更新区块的内存空间, 导致越界错误
         */
        void delChunkCount(bool bUpdateDirect = true)
        {
            if (bUpdateDirect)
            {
                --_pHead->_iUsedChunk;
            }
            else
                saveValue(&_pHead->_iUsedChunk, _pHead->_iUsedChunk - 1);
        }

        /** modified by smitchzhao @ 2011-9-18
         * 减少已经使用的数据内存
         * @param iMemSize，减少内存大小
         */
        void delUsedDataMemSize(size_t iMemSize)
        {
            assert(_pHead->_iUsedDataMem >= max((size_t)_pHead->_iMinChunkSize, iMemSize));
            _pHead->_iUsedDataMem -= iMemSize;
        }

        /**
         * 增加hit次数
         */
        void incGetCount() { saveValue(&_pHead->_iGetCount, _pHead->_iGetCount + 1); }

        /**
         * 增加命中次数
         */
        void incHitCount() { saveValue(&_pHead->_iHitCount, _pHead->_iHitCount + 1); }

        /**
         * 某hash链表数据个数+1
         * @param index
         */
        void incListCount(uint32_t index) { saveValue(&item(index)->_iListCount, item(index)->_iListCount + 1); }

        /**
         * 某hash链表数据个数+1
         * @param index
         */
        void delListCount(uint32_t index) { saveValue(&item(index)->_iListCount, item(index)->_iListCount - 1); }

        /**
         * 相对地址换成绝对地址
         * @param iAddr
         *
         * @return void*
         */
        void *getAbsolute(uint32_t iAddr) { if (iAddr == 0) return NULL; return _pDataAllocator->_pChunkAllocator->getAbsolute((iAddr >> 9) - 1, iAddr & 0x1FF); }

        /**
         * 淘汰iNowAddr之外的数据(根据淘汰策略淘汰)
         * @param iNowAddr, 当前Block不能正在分配空间, 不能被淘汰
         *                  0表示做直接根据淘汰策略淘汰
         * @param vector<BlockData>, 被淘汰的数据
         * @return size_t,淘汰的数据个数
         */
        uint32_t eraseExcept(uint32_t iNowAddr, vector<BlockData> &vtData);

        /**
         * 根据Key计算hash值
         * @param pKey
         * @param iKeyLen
         *
         * @return size_t
         */
        uint32_t hashIndex(const string& k);

        /**
         * 根据Key查找数据
         *
         */
        lock_iterator find(const string& k, uint32_t index, string &v, int &ret);

        /**
         * 根据Key查找数据
         * @param mb
         */
        lock_iterator find(const string& k, uint32_t index, int &ret);

        /**
         * 分析hash的数据
         * @param iMaxHash
         * @param iMinHash
         * @param fAvgHash
         */
        void analyseHash(uint32_t &iMaxHash, uint32_t &iMinHash, float &fAvgHash, uint32_t & HashZeroNum);

        /**
         * 修改具体的值
         * @param iModifyAddr
         * @param iModifyValue
         */
        template<typename T>
        void saveValue(void* iModifyAddr, T iModifyValue, bool bModify = true)
        {
            //获取原始值
            T tmp = *(T*)iModifyAddr;

            //保存原始值
            _pstModifyHead->_stModifyData[_pstModifyHead->_iNowIndex]._iModifyAddr = (char*)iModifyAddr - (char*)_pHead;
            _pstModifyHead->_stModifyData[_pstModifyHead->_iNowIndex]._iModifyValue = tmp;
            _pstModifyHead->_stModifyData[_pstModifyHead->_iNowIndex]._cBytes = sizeof(iModifyValue);
            _pstModifyHead->_cModifyStatus = 1;

            _pstModifyHead->_iNowIndex++;

            if (bModify)
            {
                //修改具体值
                *(T*)iModifyAddr = iModifyValue;
            }

            assert(_pstModifyHead->_iNowIndex < sizeof(_pstModifyHead->_stModifyData) / sizeof(tagModifyData));
        }

        void saveAddr(uint32_t iAddr, char cByte = 0);

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
        uint32_t getMinPrimeNumber(uint32_t n);

        /**
         * 释放保护区中的chunk
         */
        void deallocate(uint32_t iChunk, uint32_t iIndex);

        /**
         * 释放保护区中的一个block
         */
        void deallocate2(uint32_t iHead);

    protected:

        /**
         * 区块指针
         */
        tagMapHead                  *_pHead;

        /**
         * 允许最小的数据块大小
         */
        const uint32_t              _iMinChunkSize;

        /**
         * 数据平均大小
         */
        uint32_t 					_iAvgDataSize;

        /**
         * 设置元素个数/hash项比值
         */
        float                       _fRadio;

        /**
         * hash对象
         */
        tars::TC_MemVector<tagHashItem>   _hash;

        /**
         * 修改数据块
         */
        tagModifyHead               *_pstModifyHead;

        /**
         * block分配器对象
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
         * hash值计算公式
         */
        hash_functor                _hashf;

        //用于Block中get 的临时缓存，避免内存的频繁分配和释放
        size_t _tmpBufSize;
        char* _tmpBuf;

        //用于统计最近未访问的数据
        uint32_t _iReadP;
        uint32_t _iReadPBak;
    };

}

#endif

