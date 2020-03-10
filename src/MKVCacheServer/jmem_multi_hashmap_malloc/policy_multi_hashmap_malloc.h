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
#ifndef _POLICY_MULTI_HASHMAP_MALLOC_H
#define _POLICY_MULTI_HASHMAP_MALLOC_H

#include "tc_multi_hashmap_malloc.h"
#include "util/tc_autoptr.h"

namespace DCache
{

    /************************************************************************
     基本说明如下:
     基于Tars协议的支持多key的内存hashmap
     编解码出错则抛出Exception
     可以对锁策略和存储策略进行组合, 不同的组合, 初始化函数不完全一样
     初始化函数有:
     SemLockPolicy::initLock(key_t)
     ShmStorePolicy::initStore(key_t, size_t)
     FileStorePolicy::initStore(const char *file, size_t)

     ***********************************************************************

     基本特性说明:
     > 内存数据的map, 根据最后Get时间的顺序淘汰数据;
     > 支持缓写/dump到文件/在线备份;
     > 支持不同大小内存块的配置, 提供内存的使用率;
     > 支持回收到指定空闲比率的空间;
     > 支持仅设置Key的操作, 即数据无value, 只有Key, 类似与stl::set;
     > 支持自定义hash算法;
     > hash数可以根据内存块比率设置, 并优化有素数, 提高hash的散列性;
     > 支持几种方式的遍历, 通常遍历时需要对map加锁;
     > 对于hash方式的遍历, 遍历时可以不需要对map加锁, 推荐使用;
     > 支持自定义操作对象设置, 可以非常快速实现相关的接口;
     > 支持自动编解码, Key和Value的结构都通过tars2cpp生成;
     > tars协议支持自动扩展字段, 因此该hashmap支持自动扩展字段(Key和Value都必须是通过tars编码的);
     > map支持只读模式, 只读模式下set/erase/del等修改数据的操作不能使用, get/回写/在线备份正常使用
     > 支持自动淘汰, set时, 内存满则自动淘汰, 在非自动淘汰时, 内存满直接返回RT_READONLY
     > 对于mmap文件, 支持自动扩展文件, 即内存不够了, 可以自动扩展文件大小(注意hash的数量不变, 因此开始就需要考虑hash的数量), 而且不能跨JHashmap对象（即两个hashmap对象访问同一块文件，通知一个hashmap扩展以后，另外一个对象并不知道扩展了）

     ***********************************************************************

     hashmap链说明:
     hashmap链一共包括了如下几种链表:
     > Set时间链: 任何Set操作都会修改该链表, Set后数据被设置为脏数据, 且移动到Set链表头部;
     > Get时间链: 任何Get操作都会修改该链表, 除非链表只读, 注意Set链同时也会修改Get链
     > Dirty时间链: dirty链是Set链的一部分, 用于回写数据用
     > Backup链:备份链是Get链的一部分, 当备份数据时, 顺着Get链从尾部到头部开始备份;

     ***********************************************************************

     相关操作说明:
     > 可以设置map只读, 则所有写操作返回RT_READONLY, 此时Get操作不修改链表
     > 可以设置知否自动淘汰, 默认是自动淘汰的.如果不自动淘汰,则set时,无内存空间返回:RT_NO_MEMORY
     > 可以更改hash的算法, 调用setHashFunctor即可
     > 可以将某条数据设置为干净, 此时移出到Dirty链表指Dirty尾部的下一个元素;
     > 可以将某条数据设置为脏, 此时会移动到Set链表头部;
     > 每个数据都有一个上次回写时间(SyncTime), 如果启动回写操作, 则在一定时间内会回写;
     > 可以dump到文件或者从文件中load, 这个时候会对map加锁
     > 可以调用erase批量淘汰数据直到内存空闲率到一定比例
     > 可以调用sync进行数据回写, 保证一定时间内没有回写的数据会回写, map回写时间通过setSyncTime设置, 默认10分钟
     > 可以setToDoFunctor设置操作类, 以下是操作触发的情况:

     ***********************************************************************

     ToDoFunctor的函数说明:
     > 通常继承ToDoFunctor, 实现相关函数就可以了, 可以实现以下功能:Get数据, 淘汰数据, 删除数据, 回写数据, 备份数据
     > ToDoFunctor::erase, 当调用map.erase时, 该函数会被调用
     > ToDoFunctor::del, 当调用map.del时, 该函数会被调用, 注意删除时数据可能都不在cache中;
     > ToDoFunctor::sync, 当调用map.sync时, 会触发每条需要回写的数据该函数都被调用一次, 在该函数中处理回写请求;
     > ToDoFunctor::backup, 当调用map.backup时, 会触发需要备份的数据该函数会被调用一次, 在该函数中处理备份请求;
     > ToDoFunctor::get, 当调用map.get时, 如果map中无数据, 则该函数被调用, 该函数从db中获取数据, 并返回RT_OK, 如果db无数据则返回RT_NO_DATA;
     > ToDoFunctor所有接口被调用时, 都不会对map加锁, 因此可以操作map

     ***********************************************************************

     map的重要函数说明:
     > set, 设置数据到map中, 会更新set链表
            如果满了, 且可以自动淘汰, 则根据Get链淘汰数据, 此时ToDoFunctor的sync会被调用
            如果满了, 且可以不能自动淘汰, 则返回RT_NO_MEMORY

     > get, 从map获取数据, 如果有数据, 则直接从map获取数据并返回RT_OK;
            如果没有数据, 则调用ToDoFunctor::get函数, 此时get函数需要返回RT_OK, 同时会设置到map中, 并返回数据;
            如果没有数据, 则ToDoFunctor::get函数也无数据, 则需要返回RT_NO_DATA, 此时只会把Key设置到map中, 并返回RT_ONLY_KEY;
            在上面情况下, 如果再有get请求, 则不再调用ToDoFunctor::get, 直接返回RT_ONLY_KEY;

     > del, 删除数据, 无论cache是否有数据, ToDoFunctor::del都会被调用;
            如果只有Key, 则该数据也会被删除;

     > erase, 淘汰数据, 只有cache存在数据, ToDoFunctor::erase才会被调用
              如果只有Key, 则该数据也会被淘汰, 但是ToDoFunctor::erase不会被调用;

     > erase(int ratio), 批量淘汰数据, 直到空闲块比率到达ratio;
                         ToDoFunctor::erase会被调用;
                         只有Key的记录也会被淘汰, 但是ToDoFunctor::erase不会被调用;

     > sync: 缓写数据, 超时没有回写且是脏数据需要回写, 回写完毕后, 数据会自动设置为干净数据;
             可以多个线程或进程同时缓写;
             ToDoFunctor::sync会被调用;
             只有Key的记录, ToDoFunctor::sync不会被调用;

     > backup: 备份数据, 顺着顺着Get链从尾部到头部开始备份;
               ToDoFunctor::backup会被调用;
               只有Key的记录, ToDoFunctor::backup不会被调用;
               由于备份游标只有一个, 因此多个进程同时备份的时候数据可能会每个进程有一部分
               如果备份程序备份到一半down了, 则下次启动备份时会接着上次的备份进行, 除非将backup(true)调用备份

     ***********************************************************************

     返回值说明:
     > 注意函数所有int的返回值, 如无特别说明, 请参见TC_Multi_HashMap_Malloc::RT_

     ***********************************************************************

     遍历说明:
     > 可以用lock_iterator对map进行以下几种遍历, 在遍历过程中其实对map加锁处理了
     > end(): 迭代器尾部
     > begin(): 按照block区块遍历
     > rbegin():按照block区块逆序遍历
     > beginSetTime(): 按照Set时间顺序遍历
     > rbeginSetTime(): 按照Set时间顺序遍历
     > beginGetTime(): 按照Get时间顺序遍历
     > rbeginGetTime(): 按照Get时间逆序遍历
     > beginDirty(): 按时间逆序遍历脏数据链(如果setClean, 则也可能在脏链表上)
     > 其实回写数据链是脏数据量的子集
     > 注意:lock_iterator一旦获取, 就对map加锁了, 直到lock_iterator析够为止
     >
     > 可以用hash_iterator对map进行遍历, 遍历过程中对map没有加锁, 推荐使用
     > hashBegin(): 获取hash遍历迭代器
     > hashEnd(): hash遍历尾部迭代器
     > 注意:hash_iterator对应的其实是一个hash桶链, 每次获取数据其实会获取桶链上面的所有数据
    */

    template<typename LockPolicy,
        template<class, class> class StorePolicy>
    class PolicyMultiHashMapMalloc : public StorePolicy<TC_Multi_HashMap_Malloc, LockPolicy>
    {
    public:

        typedef TC_Multi_HashMap_Malloc::Value	Value;
        typedef TC_Multi_HashMap_Malloc::PackValue PackValue;
        typedef TC_Multi_HashMap_Malloc::ExpireTime Expiretime;
        typedef TC_Multi_HashMap_Malloc::DeleteData DeleteData;

        /**
        * 定义数据操作基类
        * 获取,遍历,删除,淘汰时都可以使用该操作类
        */
        class ToDoFunctor
        {
        public:
            /**
             * 数据记录
             */
            typedef Value DataRecord;	// 兼容老版本的名字

            /**
             * 析够
             */
            virtual ~ToDoFunctor() {};

            /**
             * 淘汰数据
             * @param stDataRecord: 被淘汰的数据
             */
            virtual void erase(const DataRecord &stDataRecord) {};

            /**
             * 淘汰数据
             * @param stDataRecord: 被淘汰的数据
             */
            virtual void eraseRadio(const DataRecord &stDataRecord) {};

            /**
             * 删除数据
             * @param bExists: 是否存在数据
             * @param stDataRecord: 数据, bExists==true时有效, 否则只有key有效
             */
            virtual void del(bool bExists, const DataRecord &stDataRecord) {};

            /**
             * 回写数据
             * @param stDataRecord: 数据
             */
            virtual void sync(const DataRecord &stDataRecord) {};

            /**
             * 备份数据，一次备份主key下的所有数据
             * @param vtRecords: 数据
             * @param bFull: 是否为完整数据
             */
            virtual void backup(const vector<DataRecord> &vtRecords, bool bFull) {};

            /**
             * 获取数据, 默认返回RT_NO_GET
             * stDataRecord中_key有效, 其他数据需要返回
             * @param stDataRecord: 需要获取的数据
             *
             * @return int, 获取到数据, 返回:TC_Multi_HashMap_Malloc::RT_OK
             *              没有数据,返回:TC_Multi_HashMap_Malloc::RT_NO_DATA,
             *              系统默认GET,返回:TC_Multi_HashMap_Malloc::RT_NO_GET
             *              其他,则返回:TC_Multi_HashMap_Malloc::RT_LOAD_DATA_ERR
             */
            virtual int get(DataRecord &stDataRecord)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_GET;
            }

            /**
            * 根据主key获取数据，默认返回RT_NO_GET
            * mk，需要获取数据的主key
            * vtRecords，返回的数据
            */
            virtual int get(string mk, vector<DataRecord>& vtRecords)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_GET;
            }
        };

        ///////////////////////////////////////////////////////////////////
        /**
         * 自动锁, 用于迭代器
         */
        class JhmAutoLock : public TC_HandleBase
        {
        public:
            /**
             * 构造
             * @param mutex
             */
            JhmAutoLock(typename LockPolicy::Mutex &mutex) : _lock(mutex)
            {
            }

        protected:
            //不实现
            JhmAutoLock(const JhmAutoLock &al);
            JhmAutoLock &operator=(const JhmAutoLock &al);

        protected:
            /**
             * 锁
             */
            TC_LockT<typename LockPolicy::Mutex>   _lock;
        };

        typedef TC_AutoPtr<JhmAutoLock> JhmAutoLockPtr;

        ///////////////////////////////////////////////////////////////////
        /**
         * 数据项
         */
        class JhmLockItem
        {
        public:

            /**
             * 构造函数
             * @param item
             */
            JhmLockItem(const TC_Multi_HashMap_Malloc::HashMapLockItem &item)
                : _item(item)
            {
            }

            /**
             * 拷贝构造
             * @param it
             */
            JhmLockItem(const JhmLockItem &item)
                : _item(item._item)
            {
            }

            /**
             * 复制
             * @param it
             *
             * @return JhmLockItem&
             */
            JhmLockItem& operator=(const JhmLockItem &item)
            {
                if (this != &item)
                {
                    _item = item._item;
                }

                return (*this);
            }

            /**
             *
             * @param item
             *
             * @return bool
             */
            bool operator==(const JhmLockItem& item)
            {
                return (_item == item._item);
            }

            /**
             *
             * @param item
             *
             * @return bool
             */
            bool operator!=(const JhmLockItem& item)
            {
                return !((*this) == item);
            }

            /**
             * 是否是脏数据
             *
             * @return bool
             */
            bool isDirty() { return _item.isDirty(); }

            /**
             * 是否只有Key
             *
             * @return bool
             */
            bool isOnlyKey() { return _item.isOnlyKey(); }

            /**
             * 最后回写时间
             *
             * @return uint32_t
             */
            uint32_t getSyncTime() { return _item.getSyncTime(); }

            /**
            * 获取数据过期时间
            *
            * @return uint32_t
            */
            uint32_t getExpireTime() { return _item.getExpireTime(); }

            /**
             * 获取当前item的key
             * @param mk, 主key
             * @param uk, 除主key外的联合主键
             * @return int
             *          TC_Multi_HashMap_Malloc::RT_OK:数据获取OK
             *          其他值, 异常
             */
            int get(string &mk, string &uk)
            {
                return _item.get(mk, uk);
            }

            /**
             * 获取值当前item的value(含key)
             * @param v
             * @return int
             *          TC_Multi_HashMap_Malloc::RT_OK:数据获取OK
             *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY: key有效, v无效为空
             *          其他值, 异常
             */
            int get(Value &v)
            {
                return _item.get(v);
            }

        protected:
            TC_Multi_HashMap_Malloc::HashMapLockItem _item;
        };

        ///////////////////////////////////////////////////////////////////
        /**
         * 迭代器
         */
        struct JhmLockIterator
        {
        public:

            /**
             * 构造
             * @param it
             * @param lock
             */
            JhmLockIterator(const TC_Multi_HashMap_Malloc::lock_iterator it, const JhmAutoLockPtr &lock)
                : _it(it), _item(it._iItem), _lock(lock)
            {
            }

            /**
             * 拷贝构造
             * @param it
             */
            JhmLockIterator(const JhmLockIterator &it)
                : _it(it._it), _item(it._item), _lock(it._lock)
            {
            }

            /**
             * 复制
             * @param it
             *
             * @return JhmLockIterator&
             */
            JhmLockIterator& operator=(const JhmLockIterator &it)
            {
                if (this != &it)
                {
                    _it = it._it;
                    _item = it._item;
                    _lock = it._lock;
                }

                return (*this);
            }

            /**
             *
             * @param it
             *
             * @return bool
             */
            bool operator==(const JhmLockIterator& it)
            {
                return (_it == it._it && _item == it._item);
            }

            /**
             *
             * @param mv
             *
             * @return bool
             */
            bool operator!=(const JhmLockIterator& it)
            {
                return !((*this) == it);
            }

            /**
             * 前置++
             *
             * @return JhmLockIterator&
             */
            JhmLockIterator& operator++()
            {
                ++_it;
                _item = JhmLockItem(_it._iItem);
                return (*this);
            }

            /**
             * 后置++
             *
             * @return JhmLockIterator&
             */
            JhmLockIterator operator++(int)
            {
                JhmLockIterator jit(_it, _lock);
                ++_it;
                _item = JhmLockItem(_it._iItem);
                return jit;
            }

            /**
             * 获取数据项
             *
             * @return JhmLockItem&
             */
            JhmLockItem& operator*() { return _item; }

            /**
             * 获取数据项
             *
             * @return JhmLockItem*
             */
            JhmLockItem* operator->() { return &_item; }

        protected:

            /**
             * 迭代器
             */
            TC_Multi_HashMap_Malloc::lock_iterator  _it;

            /**
             * 数据项
             */
            JhmLockItem                     _item;

            /**
             * 锁
             */
            JhmAutoLockPtr              _lock;
        };

        typedef JhmLockIterator lock_iterator;

        ///////////////////////////////////////////////////////////////////
        /**
         * 锁, 用于非锁迭代器
         *
         */
        class JhmLock : public TC_HandleBase
        {
        public:
            /**
             * 构造
             * @param mutex
             */
            JhmLock(typename LockPolicy::Mutex &mutex) : _mutex(mutex)
            {
            }

            /**
             * 获取锁
             *
             * @return typename LockPolicy::Mutex
             */
            typename LockPolicy::Mutex& mutex()
            {
                return _mutex;
            }
        protected:
            //不实现
            JhmLock(const JhmLock &al);
            JhmLock &operator=(const JhmLock &al);

        protected:
            /**
             * 锁
             */
            typename LockPolicy::Mutex &_mutex;
        };

        typedef TC_AutoPtr<JhmLock> JhmLockPtr;

        ///////////////////////////////////////////////////////////////////
        /**
         * 数据项
         */
        class JhmItem
        {
        public:

            /**
             * 构造函数
             * @param item
             */
            JhmItem(const TC_Multi_HashMap_Malloc::HashMapItem &item, const JhmLockPtr &lock)
                : _item(item), _lock(lock)
            {
            }

            /**
             * 拷贝构造
             * @param it
             */
            JhmItem(const JhmItem &item)
                : _item(item._item), _lock(item._lock)
            {
            }

            /**
             * 复制
             * @param it
             *
             * @return JhmItem&
             */
            JhmItem& operator=(const JhmItem &item)
            {
                if (this != &item)
                {
                    _item = item._item;
                    _lock = item._lock;
                }

                return (*this);
            }

            /**
             *
             * @param item
             *
             * @return bool
             */
            bool operator==(const JhmItem& item)
            {
                return (_item == item._item);
            }

            /**
             *
             * @param item
             *
             * @return bool
             */
            bool operator!=(const JhmItem& item)
            {
                return !((*this) == item);
            }

            /**
             * 获取当前hash桶的所有数据, 注意只获取有key/value的数据
             * 对于只有key的数据, 不获取
             * 如果协议解码有问题也不获取
             * @param vs,
             */
            void get(vector<PolicyMultiHashMapMalloc::Value> &vs)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                _item.get(vs);
            }

            /*
            * 获取当前hash桶所有数据过期时间
            * @param
            *
            * @return
            */
            void getExpireTime(vector<PolicyMultiHashMapMalloc::Expiretime> &vts)
            {
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                    _item.getExpireTime(vts);
                }
            }

            void eraseExpireData(time_t now)
            {
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                    _item.eraseExpireData(now);
                }
            }

            /**
            * 获取当前hash桶所有标记为删除的数据
            * @param vtData
            * @return
            */
            void getDeleteData(vector<PolicyMultiHashMapMalloc::DeleteData> &vtData)
            {
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                    _item.getDeleteData(vtData);
                }
            }

            /**
             * 设置当前桶下的数据为脏数据
             *
             * @param
             *
             * @return int
             */
            int setDirty()
            {
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                    return _item.setDirty();
                }
            }

            /**
             * 设置当前桶下的数据为干净数据
             *
             * @param
             *
             * @return int
             */
            int setClean()
            {
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                    return _item.setClean();
                }
            }

        protected:
            TC_Multi_HashMap_Malloc::HashMapItem _item;
            JhmLockPtr              _lock;
        };


        ///////////////////////////////////////////////////////////////////
        /**
         * 迭代器
         */
        struct JhmIterator
        {
        public:

            /**
             * 构造
             * @param it
             * @param lock
             */
            JhmIterator(const TC_Multi_HashMap_Malloc::hash_iterator &it, const JhmLockPtr &lock)
                : _it(it), _item(it._iItem, lock), _lock(lock)
            {
            }

            /**
             * 拷贝构造
             * @param it
             */
            JhmIterator(const JhmIterator &it)
                : _it(it._it), _item(it._item), _lock(it._lock)
            {
            }

            /**
             * 复制
             * @param it
             *
             * @return JhmIterator&
             */
            JhmIterator& operator=(const JhmIterator &it)
            {
                if (this != &it)
                {
                    _it = it._it;
                    _item = it._item;
                    _lock = it._lock;
                }

                return (*this);
            }

            /**
             *
             * @param it
             *
             * @return bool
             */
            bool operator==(const JhmIterator& it)
            {
                return (_it == it._it && _item == it._item);
            }

            /**
             *
             * @param mv
             *
             * @return bool
             */
            bool operator!=(const JhmIterator& it)
            {
                return !((*this) == it);
            }

            /**
             * 前置++
             *
             * @return JhmIterator&
             */
            JhmIterator& operator++()
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                ++_it;
                _item = JhmItem(_it._iItem, _lock);
                return (*this);
            }

            /**
             * 后置++
             *
             * @return JhmIterator&
             */
            JhmIterator operator++(int)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                JhmIterator jit(_it, _lock);
                ++_it;
                _item = JhmItem(_it._iItem, _lock);
                return jit;
            }

            /**
             * 获取数据项
             *
             * @return JhmItem&
             */
            JhmItem& operator*() { return _item; }

            /**
             * 获取数据项
             *
             * @return JhmItem*
             */
            JhmItem* operator->() { return &_item; }

        protected:

            /**
             * 迭代器
             */
            TC_Multi_HashMap_Malloc::hash_iterator  _it;

            /**
             * 数据项
             */
            JhmItem               _item;

            /**
             * 锁
             */
            JhmLockPtr            _lock;
        };

        typedef JhmIterator hash_iterator;
        ////////////////////////////////////////////////////////////////////////////
        /**
         * 数据项
         */
        class jMKhmItem
        {
        public:

            /**
             * 构造函数
             * @param item
             */
            jMKhmItem(const TC_Multi_HashMap_Malloc::MKHashMapItem &item, const JhmLockPtr &lock)
                : _item(item), _lock(lock)
            {
            }

            /**
             * 拷贝构造
             * @param it
             */
            jMKhmItem(const jMKhmItem &item)
                : _item(item._item), _lock(item._lock)
            {
            }

            /**
             * 复制
             * @param it
             *
             * @return JhmItem&
             */
            jMKhmItem& operator=(const jMKhmItem &item)
            {
                if (this != &item)
                {
                    _item = item._item;
                    _lock = item._lock;
                }

                return (*this);
            }

            /**
             *
             * @param item
             *
             * @return bool
             */
            bool operator==(const jMKhmItem& item)
            {
                return (_item == item._item);
            }

            /**
             *
             * @param item
             *
             * @return bool
             */
            bool operator!=(const jMKhmItem& item)
            {
                return !((*this) == item);
            }

            /**
             * 获取当前hash桶的所有数据, 注意只获取有key/value的数据
             * 对于只有key的数据, 不获取
             * 如果协议解码有问题也不获取
             * @param vs,
             */
            void get(map<string, pair<bool, vector<PolicyMultiHashMapMalloc::Value> > > &vs)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                _item.get(vs);
            }

            /**
             * 获取当前hash桶的所有数据, 同时获取onlyKey数据
             * @param vs,
             */
            void getAllData(map<string, pair<bool, vector<PolicyMultiHashMapMalloc::Value> > > &vs)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                _item.getAllData(vs);
            }

            void eraseExpireData(time_t now)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                _item.eraseExpireData(now);
            }

            /**
             * 获取当前hash桶的所有数据, 同时获取onlyKey数据
             * @param vs,
             */
            void getStaticData(size_t &iDirty, size_t &iDel)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                _item.getStaticData(iDirty, iDel);
            }

            /**
             * 获取当前hash桶的所key
             * @param vs,
             */
            void getKey(vector<string> &vs)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                _item.getKey(vs);
            }

            ///*
            //* 获取当前hash桶所有数据过期时间
            //* @param 
            //*
            //* @return
            //*/
            void getExpireTime(map<string, vector<PolicyMultiHashMapMalloc::Expiretime> > &vts)
            {
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                    _item.getExpireTime(vts);
                }
            }
            /**
             * 删除当前桶下的onlykey
             *
             * @param
             *
             * @return int
             */
            int delOnlyKey()
            {
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                    return _item.delOnlyKey();
                }
            }

            /**
             * 设置hash索引值
             *
             */
            bool setIndex(uint32_t index)
            {
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                    return _item.setIndex(index);
                }
            }

            /**
             * 获取hash桶个数
             *
             */
            uint32_t getHashCount()
            {
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                    return _item.getHashCount();
                }
            }

        protected:
            TC_Multi_HashMap_Malloc::MKHashMapItem _item;
            JhmLockPtr              _lock;
        };


        ///////////////////////////////////////////////////////////////////
        /**
         * 迭代器
         */
        struct JMKhmIterator
        {
        public:

            /**
             * 构造
             * @param it
             * @param lock
             */
            JMKhmIterator(const TC_Multi_HashMap_Malloc::mhash_iterator &it, const JhmLockPtr &lock)
                : _it(it), _item(it._iItem, lock), _lock(lock)
            {
            }

            /**
             * 拷贝构造
             * @param it
             */
            JMKhmIterator(const JMKhmIterator &it)
                : _it(it._it), _item(it._item), _lock(it._lock)
            {
            }

            /**
             * 复制
             * @param it
             *
             * @return JhmIterator&
             */
            JMKhmIterator& operator=(const JMKhmIterator &it)
            {
                if (this != &it)
                {
                    _it = it._it;
                    _item = it._item;
                    _lock = it._lock;
                }

                return (*this);
            }

            /**
             *
             * @param it
             *
             * @return bool
             */
            bool operator==(const JMKhmIterator& it)
            {
                return (_it == it._it && _item == it._item);
            }

            /**
             *
             * @param mv
             *
             * @return bool
             */
            bool operator!=(const JMKhmIterator& it)
            {
                return !((*this) == it);
            }

            /**
             * 前置++
             *
             * @return JhmIterator&
             */
            JMKhmIterator& operator++()
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                ++_it;
                _item = jMKhmItem(_it._iItem, _lock);
                return (*this);
            }

            /**
             * 后置++
             *
             * @return JhmIterator&
             */
            JMKhmIterator operator++(int)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                JMKhmIterator jit(_it, _lock);
                ++_it;
                _item = jMKhmItem(_it._iItem, _lock);
                return jit;
            }

            //设置索引值
            bool setIndex(uint32_t index)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(_lock->mutex());
                if (_it.setIndex(index) == true)
                {
                    _item = jMKhmItem(_it._iItem, _lock);
                    return true;
                }
                else
                    return false;
            }

            /**
             * 获取数据项
             *
             * @return JhmItem&
             */
            jMKhmItem& operator*() { return _item; }

            /**
             * 获取数据项
             *
             * @return JhmItem*
             */
            jMKhmItem* operator->() { return &_item; }

        protected:

            /**
             * 迭代器
             */
            TC_Multi_HashMap_Malloc::mhash_iterator  _it;

            /**
             * 数据项
             */
            jMKhmItem               _item;

            /**
             * 锁
             */
            JhmLockPtr            _lock;
        };

        typedef JMKhmIterator mk_hash_iterator;
        ////////////////////////////////////////////////////////////////////////////
        //
        /**
         * 构造函数
         */
        PolicyMultiHashMapMalloc()
        {
            _todo_of = NULL;
        }

        void initStore(size_t iStart, size_t iSize, void*shmaddr, uint8_t keyType, bool isCreate)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            StorePolicy<TC_Multi_HashMap_Malloc, LockPolicy>::initStore(iStart, iSize, shmaddr, keyType, isCreate);
        }

        /**
        * 初始化主key数据大小
        * 这个大小将会被用来分配主key专门的内存数据块，通常主key与数据区的数据大小相关会比较大
        * 区分不同的大小将有助于更好地利用内存空间，否则使用数据区的内存块来存储主key将是浪费
        * 这个值也可以不设，不设将默认数据区的内在块来保存主key
        */
        void initMainKeySize(size_t iMainKeySize)
        {
            this->_t.initMainKeySize(iMainKeySize);
        }

        /**
         * 初始化数据块平均大小
         * @param iDataSize
         */
        void initDataSize(size_t iDataSize)
        {
            this->_t.initDataSize(iDataSize);
        }

        /**
         * 设置hash比率(设置chunk数据块/hash项比值, 默认是2)
         * 有需要更改必须在create之前调用
         *
         * @param fratio
         */
        void initHashRatio(float fratio) { this->_t.initHashRatio(fratio); }

        /**
         * 初始化chunk个数/主key hash个数, 默认是1, 含义是一个主key下面大概有多个条数据
         * 有需要更改必须在create之前调用
         *
         * @param fratio
         */
        void initMainKeyHashRatio(float fratio) { this->_t.initMainKeyHashRatio(fratio); }

        /**
         * 设置hash方式，这个hash函数将作为联合主键的hash函数
         * @param hash_of
         */
        void setHashFunctor(TC_Multi_HashMap_Malloc::hash_functor hashf)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.setHashFunctor(hashf);
        }

        /**
         * 设置主key的hash方式，如果不设，主key将使用上面的联合主键的hash函数
         * @param hash_of
         */
        void setHashFunctorM(TC_Multi_HashMap_Malloc::hash_functor hashf)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.setHashFunctorM(hashf);
        }

        /**
         * 获取hash方式
         *
         * @return TC_Multi_HashMap_Malloc::hash_functor&
         */
        TC_Multi_HashMap_Malloc::hash_functor &getHashFunctor() { return this->_t.getHashFunctor(); }

        /* 获取主key hash方式
         *
         * @return TC_Multi_HashMap_Malloc::hash_functor&
         */
        TC_Multi_HashMap_Malloc::hash_functor &getHashFunctorM() { return this->_t.getHashFunctorM(); }

        /**
         * 设置淘汰操作类
         * @param erase_of
         */
        void setToDoFunctor(ToDoFunctor *todo_of) { this->_todo_of = todo_of; }

        /**
         * 获取总数据容量，即数据可用内存大小
         *
         * @return size_t
         */
        size_t getDataMemSize()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.getDataMemSize();
        }

        /**
         * 获取每种大小内存块的头部信息
         *
         * @return vector<TC_MemChunk::tagChunkHead>: 不同大小内存块头部信息
         */
         /*
         vector<TC_MemChunk::tagChunkHead> getBlockDetail()
         {
             TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
             return this->_t.getBlockDetail();
         }
         */

         /**
          * 所有block中chunk的个数
          *
          * @return size_t
          */
          /*
          size_t allBlockChunkCount()
          {
              TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
              return this->_t.allBlockChunkCount();
          }
          */

          /**
           * 每种block中chunk的个数(不同大小内存块的个数相同)
           *
           * @return size_t
           */
           /*
           vector<size_t> singleBlockChunkCount()
           {
               TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
               return this->_t.singleBlockChunkCount();
           }
           */

           /**
           * 获取主key chunk区的相关信息
           */
           /*
           vector<TC_MemChunk::tagChunkHead> getMainKeyDetail()
           {
               TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
               return this->_t.getBlockDetail();
           }
           */

           /**
           * 获取主key chunk区的chunk个数
           */
           /*
           size_t getMainKeyChunkCount()
           {
               TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
               return this->_t.getMainKeyChunkCount();
           }
           */

           /**
            * 获取主key内存区的容量
            *
            * @return size_t
            */
        size_t getMainKeyMemSize()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.getMainKeyMemSize();
        }

        /**
         * 获取hash桶的个数
         *
         * @return size_t
         */
        size_t getHashCount()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.getHashCount();
        }

        /**
        * 获取主key hash桶个数
        */
        size_t getMainKeyHashCount()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.getMainKeyHashCount();
        }

        /**
         * 元素的个数
         *
         * @return size_t
         */
        size_t size()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.size();
        }

        /**
         * 脏数据元素个数
         *
         * @return size_t
         */
        size_t dirtyCount()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.dirtyCount();
        }

        /**
         * 主键下Only key数据元素个数
         *
         * @return size_t
         */
        size_t onlyKeyCount()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.onlyKeyCount();
        }

        /**
         * 主key下Only key数据元素个数
         *
         * @return size_t
         */
        size_t onlyKeyCountM()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.onlyKeyCountM();
        }

        /**
         * 设置每次淘汰数量
         * @param n
         */
        void setEraseCount(size_t n)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.setEraseCount(n);
        }

        /**
         * 获取每次淘汰数量
         *
         * @return size_t
         */
        size_t getEraseCount()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.getEraseCount();
        }

        /**
        * 设置每次淘汰数量
        * @param n
        */
        void setMaxBlockCount(size_t n)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.setMaxBlockCount(n);
        }
        /**
         * 设置只读
         * @param bReadOnly
         */
        void setReadOnly(bool bReadOnly)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.setReadOnly(bReadOnly);
        }

        /**
         * 是否只读
         *
         * @return bool
         */
        bool isReadOnly()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.isReadOnly();
        }

        /**
         * 设置是否可以自动淘汰
         * @param bAutoErase
         */
        void setAutoErase(bool bAutoErase)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.setAutoErase(bAutoErase);
        }

        /**
         * 是否可以自动淘汰
         *
         * @return bool
         */
        bool isAutoErase()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.isAutoErase();
        }

        /**
         * 设置淘汰方式
         * TC_Multi_HashMap_Malloc::ERASEBYGET
         * TC_Multi_HashMap_Malloc::ERASEBYSET
         * @param cEraseMode
         */
        void setEraseMode(char cEraseMode)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.setEraseMode(cEraseMode);
        }

        /**
         * 获取淘汰方式
         *
         * @return bool
         */
        char getEraseMode()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.getEraseMode();
        }

        /**
         * 头部信息
         *
         * @return TC_Multi_HashMap_Malloc::tagMapHead
         */
        TC_Multi_HashMap_Malloc::tagMapHead& getMapHead() { return this->_t.getMapHead(); }

        /**
         * 设置回写时间间隔(秒)
         * @param iSyncTime
         */
        void setSyncTime(uint32_t iSyncTime)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.setSyncTime(iSyncTime);
        }

        void setMainKeyType(TC_Multi_HashMap_Malloc::MainKey::KEYTYPE keyType)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.setMainKeyType(keyType);
        }

        /**
         * 获取回写时间
         *
         * @return uint32_t
         */
        uint32_t getSyncTime()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.getSyncTime();
        }

        /**
         * dump到文件
         * @param sFile
         * @param bDoClear: 是否清空
         * @return int
         *          TC_Multi_HashMap_Malloc::RT_DUMP_FILE_ERR: dump到文件出错
         *          TC_Multi_HashMap_Malloc::RT_OK: dump到文件成功
         */
        int dump2file(const string &sFile, bool bDoClear = false)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            int ret = this->_t.dump2file(sFile);
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                return ret;
            }

            if (bDoClear)
                this->_t.clear();

            return ret;
        }

        /**
         * 从文件load
         * @param sFile
         *
         * @return int
         *          TC_Multi_HashMap_Malloc::RT_LOAL_FILE_ERR: load出错
         *          TC_Multi_HashMap_Malloc::RT_VERSION_MISMATCH_ERR: 版本不一致
         *          TC_Multi_HashMap_Malloc::RT_OK: load成功
         */
        int load5file(const string &sFile)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.load5file(sFile);
        }

        /**
         * 清空hash map
         * 所有map中的数据都被清空
         */
        void clear()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.clear();
        }

        /**
         * 检查数据脏状态
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key
         *          TC_Multi_HashMap_Malloc::RT_DIRTY_DATA: 是脏数据
         *          TC_Multi_HashMap_Malloc::RT_OK: 是干净数据
         *          其他返回值: 错误
         */
        int checkDirty(const string &mk, const string &uk)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.checkDirty(mk, uk);
        }

        /**
         * 检查主key下的数据脏状态，只要主key下任何一条记录是脏数据就返回脏
         * @param mk, 主key
         *
         * @return int
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key
         *          TC_Multi_HashMap_Malloc::RT_DIRTY_DATA: 是脏数据
         *          TC_Multi_HashMap_Malloc::RT_OK: 是干净数据
         *          其他返回值: 错误
         */
        int checkDirty(const string &mk)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.checkDirty(mk);
        }

        /**
         * 设置为干净数据i, 修改SET/GET时间链, 会导致数据不回写
         * @param k
         *
         * @return int
         *          TC_Multi_HashMap_Malloc::RT_READONLY: 只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key
         *          TC_Multi_HashMap_Malloc::RT_OK: 设置成功
         *          其他返回值: 错误
         */
        int setClean(const string &mk, const string &uk)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.setClean(mk, uk);
        }

        /**
         * 设置为脏数据, 修改SET/GET时间链, 会导致数据回写
         * @param mk
         * @param uk
         * @return int
         *          TC_Multi_HashMap_Malloc::RT_READONLY: 只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key
         *          TC_Multi_HashMap_Malloc::RT_OK: 设置脏数据成功
         *          其他返回值: 错误
         */
        int setDirty(const string &mk, const string &uk)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.setDirty(mk, uk);
        }

        /**
         * 设置数据回写时间
         * @param mk
         * @param uk
         * @param iSyncTime
         * @return int
         *          TC_Multi_HashMap_Malloc::RT_READONLY: 只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key
         *          TC_Multi_HashMap_Malloc::RT_OK: 设置脏数据成功
         *          其他返回值: 错误
         */
        int setSyncTime(const string &mk, const string &uk, uint32_t iSyncTime)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.setSyncTime(mk, uk, iSyncTime);
        }

        /**
         * 获取数据, 修改GET时间链
         * (如果没设置自定义Get函数,没有数据时返回:RT_NO_DATA)
         * @param mk
         * @param uk
         * @param v
         *
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有数据
         *          TC_Multi_HashMap_Malloc::RT_READONLY: 只读模式
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key
         *          TC_Multi_HashMap_Malloc::RT_OK:获取数据成功
         *          TC_Multi_HashMap_Malloc::RT_LOAD_DATA_ERR: load数据失败
         *          其他返回值: 错误
         */
        int get(const string &mk, const string &uk, Value &v, bool bCheckExpire = false, uint32_t iExpireTime = -1)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            //计算哈希值
            uint32_t unHash = this->_t.getHashFunctor()(mk + uk);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.get(mk, uk, unHash, v, bCheckExpire, iExpireTime);
            }

            // 读取到数据了, 解包
            if (ret != TC_Multi_HashMap_Malloc::RT_NO_DATA || _todo_of == NULL)
            {
                return ret;
            }

            //只读模式
            if (isReadOnly())
            {
                return TC_Multi_HashMap_Malloc::RT_READONLY;
            }

            // Hashmap中没有数据，从外部获取函数获取数据
            typename ToDoFunctor::DataRecord stDataRecord;
            stDataRecord._mkey = mk;
            stDataRecord._ukey = uk;
            ret = _todo_of->get(stDataRecord);
            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                v = stDataRecord;
                // 设置到hashmap中
                return this->set(stDataRecord._mkey, stDataRecord._ukey, stDataRecord._value, 0, 0, stDataRecord._dirty);
            }
            else if (ret == TC_Multi_HashMap_Malloc::RT_NO_GET)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_DATA;
            }
            else if (ret == TC_Multi_HashMap_Malloc::RT_NO_DATA)
            {
                // 没有数据，以only key的形式设置到hashmap中
                ret = this->set(stDataRecord._mkey, stDataRecord._ukey);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
                }
                return ret;
            }

            return TC_Multi_HashMap_Malloc::RT_LOAD_DATA_ERR;
        }
        int get(const string &mk, const string &uk, Value &v, uint32_t &iSyncTime, uint32_t& iDateExpireTime, uint8_t& iVersion, bool& bDirty, bool bCheckExpire = false, uint32_t iExpireTime = -1)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            //计算哈希值
            uint32_t unHash = this->_t.getHashFunctor()(mk + uk);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.get(mk, uk, unHash, v, iSyncTime, iDateExpireTime, iVersion, bDirty, bCheckExpire, iExpireTime);
            }

            // 读取到数据了, 解包
            if (ret != TC_Multi_HashMap_Malloc::RT_NO_DATA || _todo_of == NULL)
            {
                return ret;
            }

            //只读模式
            if (isReadOnly())
            {
                return TC_Multi_HashMap_Malloc::RT_READONLY;
            }

            // Hashmap中没有数据，从外部获取函数获取数据
            typename ToDoFunctor::DataRecord stDataRecord;
            stDataRecord._mkey = mk;
            stDataRecord._ukey = uk;
            ret = _todo_of->get(stDataRecord);
            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                v = stDataRecord;
                // 设置到hashmap中
                return this->set(stDataRecord._mkey, stDataRecord._ukey, stDataRecord._value, 0, 0, stDataRecord._dirty);
            }
            else if (ret == TC_Multi_HashMap_Malloc::RT_NO_GET)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_DATA;
            }
            else if (ret == TC_Multi_HashMap_Malloc::RT_NO_DATA)
            {
                // 没有数据，以only key的形式设置到hashmap中
                ret = this->set(stDataRecord._mkey, stDataRecord._ukey);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
                }
                return ret;
            }

            return TC_Multi_HashMap_Malloc::RT_LOAD_DATA_ERR;
        }
        /**
        * 仅根据主key获取所有的数据
        * @param mk
        * @param vs
        * @param bHead, 从头部开始取数据
        * @param iStart, 起始位置，类似于SQL limit
        * @param iCount, 结束位置
        * @return int:
        *          RT_NO_DATA: 没有数据
        *          RT_ONLY_KEY: 只有Key
        *          RT_PART_DATA: 数据不全，只有部分数据
        *          RT_OK: 获取数据成功
        *          其他返回值: 错误
        */
        int get(const string& mk, vector<Value> &vs, size_t iCount = -1, size_t iStart = 0, bool bHead = true, bool bCheckExpire = false, uint32_t iExpireTime = -1)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.get(mk, vs, iCount, iStart, bHead, bCheckExpire, iExpireTime);
            }

            if (ret != TC_Multi_HashMap_Malloc::RT_NO_DATA || _todo_of == NULL)
            {
                return ret;
            }

            //只读模式
            if (isReadOnly())
            {
                return TC_Multi_HashMap_Malloc::RT_READONLY;
            }

            // Hashmap中没有数据，从外部获取函数获取数据
            ret = _todo_of->get(mk, vs);
            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                // 设置到hashmap中
                for (size_t i = 0; i < vs.size(); i++)
                {
                    ret = this->set(vs[i]._mkey, vs[i]._ukey, vs[i]._value, 0, 0, vs[i]._dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, true, 0, TC_Multi_HashMap_Malloc::DELETE_AUTO);
                    if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        // 把设置进去的全部删除
                        vector<TC_Multi_HashMap_Malloc::Value> vtErased;
                        {
                            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                            this->_t.del(mk, vtErased);
                        }
                        return ret;
                    }
                }
            }
            else if (ret == TC_Multi_HashMap_Malloc::RT_NO_GET)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_DATA;
            }
            else if (ret == TC_Multi_HashMap_Malloc::RT_NO_DATA)
            {
                // 没有数据，以only key的形式设置到hashmap中
                ret = this->set(mk);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
                }
                return ret;
            }

            return TC_Multi_HashMap_Malloc::RT_LOAD_DATA_ERR;
        }


        int get(const string &mk, size_t &iDataCount)
        {

            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return	this->_t.get(mk, iDataCount);
        }
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
        int get(const string &mk, const string &uk, vector<Value> &vs, size_t iCount = 1, bool bForward = true, bool bCheckExpire = false, uint32_t iExpireTime = -1)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            //计算哈希值
            uint32_t unHash = this->_t.getHashFunctor()(mk + uk);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.get(mk, uk, unHash, vs, iCount, bForward, bCheckExpire, iExpireTime);
            }

            if (ret != TC_Multi_HashMap_Malloc::RT_NO_DATA || _todo_of == NULL)
            {
                return ret;
            }

            //只读模式
            if (isReadOnly())
            {
                return TC_Multi_HashMap_Malloc::RT_READONLY;
            }

            // Hashmap中没有数据，从外部获取函数获取数据
            ret = _todo_of->get(mk, vs);
            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                // 设置到hashmap中
                for (size_t i = 0; i < vs.size(); i++)
                {
                    ret = this->set(vs[i]._mkey, vs[i]._ukey, vs[i]._value, 0, 0, vs[i]._dirty, TC_Multi_HashMap_Malloc::AUTO_DATA, true, 0, TC_Multi_HashMap_Malloc::DELETE_AUTO);
                    if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        // 把设置进去的全部删除
                        vector<TC_Multi_HashMap_Malloc::Value> vtErased;
                        {
                            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                            this->_t.del(mk, vtErased);
                        }
                        return ret;
                    }
                }
            }
            else if (ret == TC_Multi_HashMap_Malloc::RT_NO_GET)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_DATA;
            }
            else if (ret == TC_Multi_HashMap_Malloc::RT_NO_DATA)
            {
                // 没有数据，以only key的形式设置到hashmap中
                ret = this->set(mk);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
                }
                return ret;
            }

            return TC_Multi_HashMap_Malloc::RT_LOAD_DATA_ERR;
        }

        int get(const string& mk, vector<Value> &vs, bool bHead)
        {
            return get(mk, vs, -1, 0, bHead);
        }

        int get(const string& mk, vector<Value>& vs, bool bCheckExpire, uint32_t iExpireTime)
        {
            return get(mk, vs, -1, 0, true, bCheckExpire, iExpireTime);
        }

        /**
         * 根据hash值获取相同hash值的所有数据
         * 注意:c匹配对象操作中, map是加锁的, 需要注意
         * @param h, h为联合主键(MK+UK)的hash
         * @param vv
         * @param c, 匹配仿函数: bool operator()(MK, UK);
         *
         * @return int, RT_OK
         */
        template<typename C>
        int getHash(uint32_t h, vector<Value> &vv, C c)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());

            size_t index = h % this->_t.getHashCount();
            size_t iAddr = this->_t.item(index)->_iBlockAddr;
            TC_Multi_HashMap_Malloc::lock_iterator it(&this->_t, iAddr, TC_Multi_HashMap_Malloc::lock_iterator::IT_UKEY, TC_Multi_HashMap_Malloc::lock_iterator::IT_NEXT);

            while (it != this->_t.end())
            {
                Value v;
                int ret = it->get(v);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    if (c(v._mkey, v._ukey))
                    {
                        vv.push_back(v);
                    }
                }
                it++;
            }

            return TC_Multi_HashMap_Malloc::RT_OK;
        }

        /**
        * 根据主key hash值获取相同主key hash值的所有数据
        * 注意:c匹配对象操作中, map是加锁的, 需要注意
        * @param h, h为主key(MK)的hash
        * @param mv, 返回结果集，以主key分组
        * @param c, 匹配仿函数: bool operator()(MK);
        *
        * @return int, RT_OK
        */
        template<typename C>
        int getHashM(uint32_t h, map<string, vector<Value> > &mv, C c)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            map<string, vector<TC_Multi_HashMap_Malloc::Value> > hmv;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.get(h, hmv);
            }

            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                map<string, vector<TC_Multi_HashMap_Malloc::Value> >::iterator it = hmv.begin();
                while (it != hmv.end())
                {
                    if (c(it->first))
                    {
                        mv[it->first] = it->second;
                    }
                    it++;
                }
            }
            return ret;
        }

        /**
         * 根据主key hash值获取相同主key hash值的所有数据
         * 注意:c匹配对象操作中, map是加锁的, 需要注意
         * @param h, h为主key(MK)的hash
         * @param mv, 返回结果集，以主key分组
         * @param c, 匹配仿函数: bool operator()(MK);
         *
         * @return int, RT_OK
         */
        template<typename C>
        int getHashM(uint32_t h, map<string, vector<PackValue> > &mv, C c)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            map<string, vector<TC_Multi_HashMap_Malloc::PackValue> > hmv;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.get(h, hmv);
            }

            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                map<string, vector<TC_Multi_HashMap_Malloc::PackValue> >::iterator it = hmv.begin();
                while (it != hmv.end())
                {
                    if (c(it->first))
                    {
                        mv[it->first] = it->second;
                    }
                    ++it;
                }
            }

            return ret;
        }

        /**
        * 仅设置Key, 用于删除数据库数据，内存不够时会自动淘汰老的数据
        * @param mk: 主key
        * @param uk: 除主key外的联合主键
        * @param bHead: 数据插入到主key链的头部还是尾部
        * @param eType: 插入的数据的类型
        *			PART_DATA: 插入的数据是不完整的数据
        *			FULL_DATA: 插入的数据是完整数据
        *			AUTO_DATA: 根据Cache已有数据类型决定最终数据类型，如果已有数据是不完整的，最终的数据也是不完整的，如果已有数据是完整的，最终数据也是完整的，如果Cache中没有数据，最终数据是不完整的
        * @return int:
        *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
        *          TC_Multi_HashMap_Malloc::RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
        *          TC_Multi_HashMap_Malloc::RT_OK: 设置成功
        *			TC_Multi_HashMap_Malloc::RT_DATA_EXIST: 主键下已经有数据，不能设置为only key
        *          其他返回值: 错误
        */
        int setForDel(const string &mk, const string &uk, const time_t t, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.setForDel(mk, uk, t, eType, bHead, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        /**
         * 设置数据, 修改时间链, 内存不够时会自动淘汰老的数据
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param v: 值
         * @param iExpiretime: 数据过期时间，单位为秒，0表示不设置过期时间
         * @param iVersion: 设置的数据版本，应该根据get的数据版本来设置，0表示不检查版本
         * @param bDirty: 是否是脏数据
         * @param bHead: 数据插入到主key链的头部还是尾部
         * @param bUpdateOrder: 数据更新同时更新主key链下的顺序
         * @param eType: 插入的数据的类型
         *			PART_DATA: 插入的数据是不完整的数据
         *			FULL_DATA: 插入的数据是完整数据
         *			AUTO_DATA: 根据Cache已有数据类型决定最终数据类型，如果已有数据是不完整的，最终的数据也是不完整的，如果已有数据是完整的，最终数据也是完整的，如果Cache中没有数据，最终数据是不完整的
         * @param iMaxDataCount: 该主key下所能容纳的最大数据量,等于0时表明无限制，默认情况下为0
         * @param deletetype: 数据是否标记为已删除
         * @param bDelDirty：iMaxDataCount功能开启时，是否删除脏数据
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *			TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH, 要设置的数据版本与当前版本不符，应该重新get后再set
         *          TC_Multi_HashMap_Malloc::RT_OK: 设置成功
         *          其他返回值: 错误
         */
         /*    int set(const string &mk, const string &uk, const string &v, uint32_t iExpireTime, uint8_t iVersion,
                 bool bDirty = true, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true,
                     bool bUpdateOrder = false, const uint32_t iMaxDataCount=0, TC_Multi_HashMap_Malloc::DELETETYPE deletetype = TC_Multi_HashMap_Malloc::DELETE_FALSE, bool bDelDirty = false)
             {
                 int ret = TC_Multi_HashMap_Malloc::RT_OK;
                 vector<TC_Multi_HashMap_Malloc::Value> vtErased;

                 uint32_t unHash = this->_t.getHashFunctor()(mk+uk);
                 {
                     TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                     ret = this->_t.set(mk, uk, unHash, v, iExpireTime, iVersion, bDirty, eType, bHead, bUpdateOrder, deletetype,vtErased);

                     if (TC_Multi_HashMap_Malloc::RT_OK == ret && iMaxDataCount>0)
                     {
                         size_t iDataCount = 0;
                         iDataCount = this->_t.count(mk);

                         int iDelCount = iDataCount -iMaxDataCount;
                         if (iDelCount > 0)
                         {
                                //删除的顺序与插入的顺序相反，以删除最老记录
                               vector<Value> value;
                               ret = this->_t.del(mk, iDelCount,value,0,!bHead, bDelDirty);
                         }
                     }
                 }

                 //操作淘汰数据
                 if(_todo_of)
                 {
                     for(size_t i = 0; i < vtErased.size(); i ++)
                     {
                         try
                         {
                             _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                         }
                         catch(exception &ex)
                         {
                         }
                     }
                 }
                 return ret;
             }
         */
         /**
          * 设置数据, 修改时间链, 内存不够时会自动淘汰老的数据
          * @param mk: 主key
          * @param uk: 除主key外的联合主键
          * @param v: 值
          * @param iExpiretime: 数据过期时间，单位为秒，0表示不设置过期时间
          * @param iVersion: 设置的数据版本，应该根据get的数据版本来设置，0表示不检查版本
          * @param bDirty: 是否是脏数据
          * @param bHead: 数据插入到主key链的头部还是尾部
          * @param bUpdateOrder: 数据更新同时更新主key链下的顺序
          * @param eType: 插入的数据的类型
          *			PART_DATA: 插入的数据是不完整的数据
          *			FULL_DATA: 插入的数据是完整数据
          *			AUTO_DATA: 根据Cache已有数据类型决定最终数据类型，如果已有数据是不完整的，最终的数据也是不完整的，如果已有数据是完整的，最终数据也是完整的，如果Cache中没有数据，最终数据是不完整的
          * @param iMaxDataCount: 该主key下所能容纳的最大数据量,等于0时表明无限制，默认情况下为0
          * @param deletetype: 数据是否标记为已删除
          * @param bDelDirty：iMaxDataCount功能开启时，是否删除脏数据
          * @param bCheckExpire：是否开启过期
          * @param iNowTime：当前时间
          * @return int:
          *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
          *          TC_Multi_HashMap_Malloc::RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
          *			TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH, 要设置的数据版本与当前版本不符，应该重新get后再set
          *          TC_Multi_HashMap_Malloc::RT_OK: 设置成功
          *          其他返回值: 错误
          */
        int set(const string &mk, const string &uk, const string &v, uint32_t iExpireTime, uint8_t iVersion,
            bool bDirty = true, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true,
            bool bUpdateOrder = false, const uint32_t iMaxDataCount = 0, TC_Multi_HashMap_Malloc::DELETETYPE deletetype = TC_Multi_HashMap_Malloc::DELETE_FALSE,
            bool bDelDirty = false, bool bCheckExpire = false, uint32_t iNowTime = -1)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;

            uint32_t unHash = this->_t.getHashFunctor()(mk + uk);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.set(mk, uk, unHash, v, iExpireTime, iVersion, bDirty, eType, bHead, bUpdateOrder, deletetype, bCheckExpire, iNowTime, vtErased);

                if (TC_Multi_HashMap_Malloc::RT_OK == ret && iMaxDataCount > 0)
                {
                    size_t iDataCount = 0;
                    iDataCount = this->_t.count(mk);

                    int iDelCount = iDataCount - iMaxDataCount;
                    if (iDelCount > 0)
                    {
                        //删除的顺序与插入的顺序相反，以删除最老记录
                        vector<Value> value;
                        ret = this->_t.del(mk, iDelCount, value, 0, !bHead, bDelDirty);
                    }
                }
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        /**
         * 仅设置Key, 内存不够时会自动淘汰老的数据
         * @param mk: 主key
         * @param uk: 除主key外的联合主键
         * @param bHead: 数据插入到主key链的头部还是尾部
         * @param bUpdateOrder: 数据更新同时更新主key链下的顺序
         * @param eType: 插入的数据的类型
         *			PART_DATA: 插入的数据是不完整的数据
         *			FULL_DATA: 插入的数据是完整数据
         *			AUTO_DATA: 根据Cache已有数据类型决定最终数据类型，如果已有数据是不完整的，最终的数据也是不完整的，如果已有数据是完整的，最终数据也是完整的，如果Cache中没有数据，最终数据是不完整的
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          TC_Multi_HashMap_Malloc::RT_OK: 设置成功
         *			TC_Multi_HashMap_Malloc::RT_DATA_EXIST: 主键下已经有数据，不能设置为only key
         *          其他返回值: 错误
         */
        int set(const string &mk, const string &uk, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true, bool bUpdateOrder = false, TC_Multi_HashMap_Malloc::DELETETYPE deletetype = TC_Multi_HashMap_Malloc::DELETE_FALSE)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.set(mk, uk, eType, bHead, bUpdateOrder, deletetype, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        /**
         * 仅设置主Key, 内存不够时会自动淘汰老的数据
         * @param mk: 主key
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
         *          TC_Multi_HashMap_Malloc::RT_OK: 设置成功
         *			TC_Multi_HashMap_Malloc::RT_DATA_EXIST: 主key下已经有数据，不能设置为only key
         *          其他返回值: 错误
         */
        int set(const string &mk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.set(mk, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        /**
        * 批量设置数据，内存不够时会自动淘汰
        * 注意，此接口通常用于从数据库中取出相同主key下的一批数据并同步到Cache
        * 不能滥用此接口来批量插入数据，要注意保证Cache和数据库数据的一致
        * @param vs, 批量数据集
        * @param eType: 插入的数据的类型
        *			PART_DATA: 插入的数据是不完整的数据
        *			FULL_DATA: 插入的数据是完整数据
        *			AUTO_DATA: 根据Cache已有数据类型决定最终数据类型，如果已有数据是不完整的，最终的数据也是不完整的，如果已有数据是完整的，最终数据也是完整的，如果Cache中没有数据，最终数据是不完整的
        * @param bHead, 数据插入到主key链的头部还是尾部
        * @param bUpdateOrder: 数据更新同时更新主key链下的顺序
        * @param bForce, 是否强制插入数据，为false则表示如果数据已经存在则不更新
        *
        * @return int:
        *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
        *          TC_Multi_HashMap_Malloc::RT_NO_MEMORY: 没有空间(不淘汰数据情况下会出现)
        *          TC_Multi_HashMap_Malloc::RT_OK: 设置成功
        *          其他返回值: 错误
        */
        int set(const vector<Value> &vs, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true, bool bUpdateOrder = false, bool bOrderByItem = false, bool bForce = true)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.set(vs, eType, bHead, bUpdateOrder, bOrderByItem, bForce, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }

            return ret;
        }

        int update(const string &mk, const string &uk, const map<std::string, DCache::UpdateValue> &mpValue,
            const vector<DCache::Condition> & vtUKCond, const TC_Multi_HashMap_Malloc::FieldConf *fieldInfo, bool bLimit, size_t iIndex, size_t iCount, string &retValue, bool bCheckExpire, uint32_t iNowTime, uint32_t iExpireTime,
            bool bDirty, TC_Multi_HashMap_Malloc::DATATYPE eType, bool bHead, bool bUpdateOrder, uint32_t iMaxDataCount, bool bDelDirty)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;

            uint32_t unHash = this->_t.getHashFunctor()(mk + uk);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.update(mk, uk, unHash, mpValue, vtUKCond, fieldInfo, bLimit, iIndex, iCount, retValue, bCheckExpire, iNowTime, iExpireTime, bDirty, eType, bHead, bUpdateOrder, vtErased);

                if (TC_Multi_HashMap_Malloc::RT_OK == ret && iMaxDataCount > 0)
                {
                    size_t iDataCount = 0;
                    iDataCount = this->_t.count(mk);

                    int iDelCount = iDataCount - iMaxDataCount;
                    if (iDelCount > 0)
                    {
                        //删除的顺序与插入的顺序相反，以删除最老记录
                        vector<Value> value;
                        ret = this->_t.del(mk, iDelCount, value, 0, !bHead, bDelDirty);
                    }
                }
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        /**
         * 不是真正删除数据，只是把标记位设置
         *
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int delSetBit(const string &mk, const string &uk, const time_t t)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delSetBit(mk, uk, t);
            }
            return ret;
        }
        int delSetBit(const string &mk, const string &uk, uint8_t iVersion, const time_t t)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delSetBit(mk, uk, iVersion, t);
            }
            return ret;
        }
        /**
         * 不是真正删除数据，只是把标记位设置
         *
         * @param mk, 主key
         * @param t, 删除时间
         *
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int delSetBit(const string &mk, const time_t t, uint64_t &delCount)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delSetBit(mk, t, delCount);
            }
            return ret;
        }


        /**
         * 真正删除数据，并且删除的时候检查删除标记是否设置
         *
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int delReal(const string &mk, const string &uk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delReal(mk, uk);
            }
            return ret;
        }


        /**
         * 删除数据
         * 无论cache是否有数据,todo的del都被调用
         *
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
         /*int del(const string &mk, const string &uk)
         {
             int ret = TC_Multi_HashMap_Malloc::RT_OK;
             TC_Multi_HashMap_Malloc::Value data;

             {
                 TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                 ret = this->_t.del(mk, uk, data);
             }
             return ret;
         }*/

         /**
        * 删除数据
        * 无论cache是否有数据,todo的del都被调用 给过期清理线程使用
        *
        * @param mk, 主key
        * @param uk, 除主key外的联合主键
        *
        * @return int:
        *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
        *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
        *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
        *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
        *          其他返回值: 错误
        */
        int delExpire(const string &mk, const string &uk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            TC_Multi_HashMap_Malloc::Value data;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.del(mk, uk, data);
            }

            if (ret != TC_Multi_HashMap_Malloc::RT_OK
                && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY
                && ret != TC_Multi_HashMap_Malloc::RT_NO_DATA
                && ret != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
            {
                return ret;
            }

            if (_todo_of)
            {
                if (ret == TC_Multi_HashMap_Malloc::RT_NO_DATA)
                {
                    data._mkey = mk;
                    data._ukey = uk;
                }
                _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), data);
            }
            return ret;
        }

        /**
         * 删除主key下的所有数据
         * 无论cache是否有数据,todo的del都被调用
         *
         * @param mk, 主key
         * @param iCount: 要删除多少条记录
         * @param iStart: 删除的起始位置
         * @param bHead: 从头部开始删除还是尾部开始
         *
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int del(const string &mk, size_t iCount = -1, size_t iStart = 0, bool bHead = true)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            vector<TC_Multi_HashMap_Malloc::Value> vtErased;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.del(mk, iCount, vtErased, iStart, bHead);
            }

            if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY && ret != TC_Multi_HashMap_Malloc::RT_NO_DATA && ret != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
            {
                return ret;
            }

            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        /**
        * 删除主key下的所有数据
        * 无论cache是否有数据,todo的del都被调用
        *
        * @param mk, 主key
        * @param iCount: 要删除多少条记录
        * @param iStart: 删除的起始位置
        * @param bHead: 从头部开始删除还是尾部开始
        *
        * @return int:
        *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
        *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
        *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
        *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
        *          其他返回值: 错误
        */
        int del(const string &mk, vector<TC_Multi_HashMap_Malloc::Value> &vtErased, size_t iCount = -1, size_t iStart = 0, bool bHead = true)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.del(mk, iCount, vtErased, iStart, bHead);
            }

            return ret;
        }

        /**
        * 强制删除数据,包括标记为删除的数据，不调用todo
        *
        * @param mk, 主key
        * @param uk, 除主key外的联合主键
        *
        * @return int:
        *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
        *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
        *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
        *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
        *          其他返回值: 错误
        */
        /*int delForce(const string &mk, const string &uk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            TC_Multi_HashMap_Malloc::Value data;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delForce(mk, uk, data);
            }

            if(ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                return ret;
            }

            return ret;
        }*/

        /**
         * 删除主key下的所有数据，除了被标记为已删除的数据
         * cache有数据,todo的erase被调用
         *
         * @param mk, 主key
         *
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int erase(const string &mk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.del(mk, vtErased);
            }

            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                return ret;
            }

            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->erase(vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        /**
         * 删除指定数据，被标记为已删除的返回TC_Multi_HashMap_Malloc::RT_DATA_DEL
         * cache有数据,todo的erase被调用
         *
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int erase(const string &mk, const string &uk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            TC_Multi_HashMap_Malloc::Value data;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.del(mk, uk, data);
            }

            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                return ret;
            }
            if (_todo_of)
            {
                try
                {
                    _todo_of->erase(data);
                }
                catch (exception &ex)
                {
                }
            }
            return ret;
        }

        /**
         * 强制删除数据,不调用todo的erase被调用
         *
         * @param mk, 主key
         * @param uk, 除主key外的联合主键
         *
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int eraseByForce(const string &mk, const string &uk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            TC_Multi_HashMap_Malloc::Value data;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delForce(mk, uk, data);
            }

            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                return ret;
            }

            return ret;
        }

        /**
         * 强制删除主key下的所有数据,不调用todo的erase被调用
         *
         * @param mk, 主key
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         */
        int eraseByForce(const string &mk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> data;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delForce(mk, data);
            }

            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                return ret;
            }

            return ret;
        }

        /**
         * 强制删除hash值等于h的主key下的所有数据,不调用todo的erase被调用
         *
         * @param mk, 主key
         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有当前数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key, 也删除了
         *          TC_Multi_HashMap_Malloc::RT_OK: 删除数据成功
         *          其他返回值: 错误
         * @param h, h为主key(MK)的hash
         * @param c, 匹配仿函数: bool operator()(MK);
         * @param vDelMK, 删除的MK集合
         */
        template<typename C>
        int eraseHashMByForce(uint32_t h, C c, vector<string>& vDelMK)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            map<string, vector<TC_Multi_HashMap_Malloc::Value> > hmv;

            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());

            ret = this->_t.get(h, hmv);

            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                map<string, vector<TC_Multi_HashMap_Malloc::Value> >::iterator it = hmv.begin();
                while (it != hmv.end())
                {
                    if (c(it->first))
                    {
                        vDelMK.push_back(it->first);
                    }
                    ++it;
                }
            }

            for (size_t i = 0; i < vDelMK.size(); ++i)
            {
                vector<TC_Multi_HashMap_Malloc::Value> data;
                ret = this->_t.delForce(vDelMK[i], data);
                if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    vDelMK.resize(i);
                    return ret;
                }
            }

            return ret;
        }

        /**
         * 淘汰数据, 根据Get时间淘汰
         * 直到: 元素个数/chunks * 100 < ratio，bCheckDirty 为true时，遇到脏数据则淘汰结束
         * @param ratio: 共享内存chunks使用比例 0< ratio < 100
         * @param bCheckDirty: 是否检查数据脏状态，如果检查则遇到脏数据不淘汰

         * @return int:
         *          TC_Multi_HashMap_Malloc::RT_READONLY: map只读
         *          TC_Multi_HashMap_Malloc::RT_OK:淘汰完毕
         */
        int erase(int ratio, bool bCheckDirty = false)
        {
            while (true)
            {
                int ret;
                vector<TC_Multi_HashMap_Malloc::Value> vtErased;

                {
                    TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                    ret = this->_t.erase(ratio, vtErased, bCheckDirty);
                    if (ret == TC_Multi_HashMap_Malloc::RT_OK || ret == TC_Multi_HashMap_Malloc::RT_READONLY)
                    {
                        return ret;
                    }

                    if (ret != TC_Multi_HashMap_Malloc::RT_ERASE_OK)
                    {
                        continue;
                    }
                }

                if (_todo_of)
                {
                    for (size_t i = 0; i < vtErased.size(); i++)
                    {
                        try
                        {
                            _todo_of->erase(vtErased[i]);
                        }
                        catch (exception &ex)
                        {
                        }
                    }
                }
            }
            return TC_Multi_HashMap_Malloc::RT_OK;
        }



        int erase(int ratio, unsigned int uMaxEraseOneTime, bool bCheckDirty = false)
        {
            unsigned int uEraseCount = 0;
            while (uEraseCount < uMaxEraseOneTime)
                //while(true)
            {
                int ret;
                vector<TC_Multi_HashMap_Malloc::Value> vtErased;

                {
                    TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                    ret = this->_t.erase(ratio, vtErased, bCheckDirty);
                    if (ret == TC_Multi_HashMap_Malloc::RT_OK || ret == TC_Multi_HashMap_Malloc::RT_READONLY)
                    {
                        return ret;
                    }

                    if (ret != TC_Multi_HashMap_Malloc::RT_ERASE_OK)
                    {
                        continue;
                    }
                    ++uEraseCount;
                }

                if (_todo_of)
                {
                    for (size_t i = 0; i < vtErased.size(); i++)
                    {
                        try
                        {
                            _todo_of->eraseRadio(vtErased[i]);
                        }
                        catch (exception &ex)
                        {
                        }
                    }
                }
            }
            return TC_Multi_HashMap_Malloc::RT_OK;
        }

        int pushList(const string &mk, const vector<pair<uint32_t, string> > &v, const bool bHead, const bool bReplace, const uint64_t iPos, const uint32_t iNowTime)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.pushList(mk, v, bHead, bReplace, iPos, iNowTime, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        int pushList(const string &mk, const vector<Value> &vt)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.pushList(mk, vt, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        int getList(const string &mk, const uint64_t iStart, const uint64_t iEnd, const uint32_t iNowTime, vector<string> &vs)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.getList(mk, iStart, iEnd, iNowTime, vs);
            }

            return ret;
        }

        int trimList(const string &mk, const bool bPop, const bool bHead, const bool bTrim, const uint64_t iStart, const uint64_t iCount, const uint32_t iNowTime, string &value, uint64_t &delSize)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.trimList(mk, bPop, bHead, bTrim, iStart, iCount, iNowTime, value, delSize);
            }

            return ret;
        }

        int addSet(const string &mk, const string &v, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, TC_Multi_HashMap_Malloc::DELETETYPE deleteType)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;

            uint32_t unHash = this->_t.getHashFunctor()(mk + v);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.addSet(mk, v, unHash, iExpireTime, iVersion, bDirty, deleteType, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        int addSet(const string &mk, const vector<Value> &vt, TC_Multi_HashMap_Malloc::DATATYPE eType)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.addSet(mk, vt, eType, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        int addSet(const string &mk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.addSet(mk, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        int getSet(const string &mk, const uint32_t iNowTime, vector<Value> &vtData)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.getSet(mk, iNowTime, vtData);
            }

            return ret;
        }

        int delSetSetBit(const string &mk, const string &v, const time_t t)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            uint32_t unHash = this->_t.getHashFunctor()(mk + v);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delSetSetBit(mk, v, unHash, t);
            }

            return ret;
        }

        int delSetSetBit(const string &mk, const time_t t)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delSetSetBit(mk, t);
            }

            return ret;
        }

        int delSetReal(const string &mk, const string &v)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            uint32_t unHash = this->_t.getHashFunctor()(mk + v);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delSetReal(mk, v, unHash);
            }

            return ret;
        }

        int addZSet(const string &mk, const string &v, double iScore, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, bool bInc, TC_Multi_HashMap_Malloc::DELETETYPE deleteType)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;

            uint32_t unHash = this->_t.getHashFunctor()(mk + v);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.addZSet(mk, v, unHash, iScore, iExpireTime, iVersion, bDirty, bInc, deleteType, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }

            return ret;
        }

        int addZSet(const string &mk, const vector<Value> &vt, TC_Multi_HashMap_Malloc::DATATYPE eType)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.addZSet(mk, vt, eType, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }

            return ret;
        }

        int addZSet(const string &mk)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.addZSet(mk, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }
            return ret;
        }

        int getZSet(const string &mk, const uint64_t iStart, const uint64_t iEnd, const bool bUp, const uint32_t iNowTime, list<Value> &vtData)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.getZSet(mk, iStart, iEnd, bUp, iNowTime, vtData);
            }

            return ret;
        }

        int getZSet(const string &mk, const string &v, const uint32_t iNowTime, Value& vData)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            uint32_t unHash = this->_t.getHashFunctor()(mk + v);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.getZSet(mk, v, unHash, iNowTime, vData);
            }

            return ret;
        }

        // Changed by tutuli 2017-7-24 21:33

        int getZSetLimit(const string &mk, const size_t iStart, const size_t iDataCount, const bool bUp, const uint32_t iNowTime, list<Value> &vtData)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.getZSetLimit(mk, iStart, iDataCount, bUp, iNowTime, vtData);
            }
            return ret;
        }

        int getScoreZSet(const string &mk, const string &v, const uint32_t iNowTime, double &iScore)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            uint32_t unHash = this->_t.getHashFunctor()(mk + v);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.getScoreZSet(mk, v, unHash, iNowTime, iScore);
            }

            return ret;
        }

        int getRankZSet(const string &mk, const string &v, const bool order, const uint32_t iNowTime, long &iPos)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            uint32_t unHash = this->_t.getHashFunctor()(mk + v);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.getRankZSet(mk, v, unHash, order, iNowTime, iPos);
            }

            return ret;
        }

        int getZSetByScore(const string &mk, const double iMin, const double iMax, const uint32_t iNowTime, list<Value> &vtData)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.getZSetByScore(mk, iMin, iMax, iNowTime, vtData);
            }

            return ret;
        }

        int delZSetSetBit(const string &mk, const string &v, const time_t t)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            uint32_t unHash = this->_t.getHashFunctor()(mk + v);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delZSetSetBit(mk, v, unHash, t);
            }

            return ret;
        }

        int delRangeZSetSetBit(const string &mk, const double iMin, const double iMax, const uint32_t iNowTime, const time_t t)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delRangeZSetSetBit(mk, iMin, iMax, iNowTime, t);
            }

            return ret;
        }

        int delZSetReal(const string &mk, const string &v)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            uint32_t unHash = this->_t.getHashFunctor()(mk + v);
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delZSetReal(mk, v, unHash);
            }

            return ret;
        }

        int delZSetSetBit(const string &mk, const time_t t)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.delZSetSetBit(mk, t);
            }

            return ret;
        }

        int updateZSet(const string &mk, const string &sOldValue, const string &sNewValue, double iScore, uint32_t iExpireTime, char iVersion, bool bDirty, bool bOnlyScore)
        {
            int ret = TC_Multi_HashMap_Malloc::RT_OK;
            vector<TC_Multi_HashMap_Malloc::Value> vtErased;

            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.updateZSet(mk, sOldValue, sNewValue, iScore, iExpireTime, iVersion, bDirty, bOnlyScore, vtErased);
            }

            //操作淘汰数据
            if (_todo_of)
            {
                for (size_t i = 0; i < vtErased.size(); i++)
                {
                    try
                    {
                        _todo_of->del((ret == TC_Multi_HashMap_Malloc::RT_OK), vtErased[i]);
                    }
                    catch (exception &ex)
                    {
                    }
                }
            }

            return ret;
        }

        /**
         * 回写单条记录, 如果记录不存在, 则不做任何处理
         * @param mk
         * @param uk
         *
         * @return int
         *          TC_Multi_HashMap_Malloc::RT_NO_DATA: 没有数据
         *          TC_Multi_HashMap_Malloc::RT_ONLY_KEY:只有Key
         *          TC_Multi_HashMap_Malloc::RT_OK:获取数据成功
         *          TC_Multi_HashMap_Malloc::RT_LOAD_DATA_ERR: load数据失败
         *          其他返回值: 错误
         */
        int sync(const string &mk, const string &uk)
        {
            Value v;
            int ret = get(mk, uk, v);

            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                if (_todo_of)
                {
                    _todo_of->sync(v);
                }
            }

            return ret;
        }

        /**
         * 将脏数据且一定时间没有回写的数据全部回写
         * 数据回写时间与当前时间超过_pHead->_iSyncTime(setSyncTime)则需要回写
         *
         * map只读时仍然可以回写
         *
         * @param iNowTime: 回写到什么时间, 通常是当前时间
         * @return int:
         *      TC_Multi_HashMap_Malloc::RT_OK: 回写完毕了
         */
        int sync(uint32_t iNowTime)
        {
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                this->_t.sync();
            }

            while (true)
            {
                TC_Multi_HashMap_Malloc::Value data;

                int ret;
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                    ret = this->_t.sync(iNowTime, data);
                    if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        return ret;
                    }

                    if (ret != TC_Multi_HashMap_Malloc::RT_NEED_SYNC)
                    {
                        continue;
                    }
                }

                if (_todo_of)
                {
                    _todo_of->sync(data);
                }
            }

            return TC_Multi_HashMap_Malloc::RT_OK;
        }

        /**
        *将脏数据尾指针赋给回写尾指针
        */
        void sync()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            this->_t.sync();
        }

        /**
         * 将脏数据且一定时间没有回写的数据回写,只回写一个脏数据，目的是替代int sync(uint32_t iNowTime)
         * 方法，把由业务控制每次回写数据量，使用时应该先调用void sync()
         *
         * 数据回写时间与当前时间超过_pHead->_iSyncTime(setSyncTime)则需要回写

         * map只读时仍然可以回写
         *
         * @param iNowTime: 回写到什么时间, 通常是当前时间
         * @return int:
         *      TC_Multi_HashMap_Malloc::RT_OK: 回写完毕了
         *
         * 示例：
         *      p->sync();
         *      while(true) {
         *          int iRet = pthis->SyncOnce(tNow);
         *          if( iRet == TC_Multi_HashMap_Malloc::RT_OK )
         *				break;
         *		}
         */
        int syncOnce(uint32_t iNowTime)
        {
            TC_Multi_HashMap_Malloc::Value data;

            int ret;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.sync(iNowTime, data);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    return ret;
                }

                if (ret != TC_Multi_HashMap_Malloc::RT_NEED_SYNC)
                {
                    return ret;
                }
            }

            if (_todo_of)
            {
                _todo_of->sync(data);
            }

            return ret;
        }

        /**
         * 将脏数据且一定时间没有回写的数据回写,只回写一个脏数据，目的是替代int sync(uint32_t iNowTime)
         * 方法，把由业务控制每次回写数据量，使用时应该先调用void sync()
         *
         * 数据回写时间与当前时间超过_pHead->_iSyncTime(setSyncTime)则需要回写

         * map只读时仍然可以回写
         *
         * @param iNowTime: 回写到什么时间, 通常是当前时间
         * @param c: 条件仿函数: bool operator()(K v);
         * @return int:
         *      TC_Multi_HashMap_Malloc::RT_OK: 回写完毕了
         *
         * 示例：
         *      p->sync();
         *      while(true) {
         *          int iRet = pthis->SyncOnce(tNow);
         *          if( iRet == TC_Multi_HashMap_Malloc::RT_OK )
         *				break;
         *		}
         */
        template <typename C>
        int syncOnce(uint32_t iNowTime, C &c)
        {
            TC_Multi_HashMap_Malloc::Value data;

            int ret;
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                ret = this->_t.sync(iNowTime, data);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    return ret;
                }

                if (ret != TC_Multi_HashMap_Malloc::RT_NEED_SYNC)
                {
                    return ret;
                }

                if (_todo_of)
                {
                    if (!c(data._mkey))
                    {
                        this->_t.setDirtyAfterSync(data._mkey, data._ukey);
                        return ret;
                    }
                }
                else
                    return ret;
            }

            _todo_of->sync(data);

            return ret;
        }

        /**
         * 备份数据
         * map只读时仍然可以备份
         * 可以多个线程/进程备份数据,同时备份时bForceFromBegin设置为false效率更高
         *
         * @param bForceFromBegin: 是否强制重头开始备份, 通常为false
         * @return int:
         *      TC_Multi_HashMap_Malloc::RT_OK: 备份OK了
         */
        int backup(bool bForceFromBegin = false)
        {
            {
                //开始准备备份
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                this->_t.backup(bForceFromBegin);
            }

            while (true)
            {
                vector<TC_Multi_HashMap_Malloc::Value> vtData;

                int ret;
                {
                    TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
                    ret = this->_t.backup(vtData);
                    if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        return ret;
                    }

                    if (ret != TC_Multi_HashMap_Malloc::RT_NEED_BACKUP && ret != TC_Multi_HashMap_Malloc::RT_PART_DATA)
                    {
                        continue;
                    }
                }

                if (_todo_of)
                {
                    _todo_of->backup(vtData, ret != TC_Multi_HashMap_Malloc::RT_PART_DATA);
                }
            }

            return TC_Multi_HashMap_Malloc::RT_OK;
        }

        /**
         * 统计最近未访问的数据大小
         *
         * @return int:
         *      TC_HashMapMalloc::RT_OK: 备份OK了
         */
        int calculateData(uint32_t &count, bool &isEnd)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.calculateData(count, isEnd);
        }

        /**
         * 重置统计指针
         *
         * @return int:
         *      TC_HashMapMalloc::RT_OK: 备份OK了
         */
        int resetCalculateData()
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.resetCalculatePoint();
        }

        /**
         * 描述
         *
         * @return string
         */
        string descWithHash() { return this->_t.descWithHash(); }

        /**
        * 描述
        *
        * @return string
        */
        string desc() { return this->_t.desc(); }

        ///////////////////////////////////////////////////////////////////////////////
        /**
         * 尾部
         *
         * @return lock_iterator
         */
        lock_iterator end()
        {
            JhmAutoLockPtr jlock;
            return JhmLockIterator(this->_t.end(), jlock);
        }

        /**
         * 根据联合主键(MK+UK)查找数据
         * @param mk
         * @param uk
         * @return lock_interator
         *		返回end()表示没有查到
         */
        lock_iterator find(const string &mk, const string &uk)
        {
            JhmAutoLockPtr jlock(new JhmAutoLock(this->mutex()));
            return JhmLockIterator(this->_t.find(mk, uk), jlock);
        }

        /**
        * 查找主key下所有数据数量
        * @param mk, 主key
        *
        * @return size_t, 主key下的记录数
        */
        size_t count(const string &mk)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.count(mk);
        }

        /**
        * 根据主key查找第一个数据的位置
        * 与上面的count函数组合可以遍历所有主key下的数据
        * 也可以直接使用迭代器，直到end
        * @param mk, 主key
        * @return lock_iterator, 返回end()表示没有数据
        */
        lock_iterator find(const string& mk)
        {
            JhmAutoLockPtr jlock(new JhmAutoLock(this->mutex()));
            return JhmLockIterator(this->_t.find(mk), jlock);
        }

        /**
        * 判断主key是否存在
        * @param mk, 主key
        *
        * @return int
        *		TC_Multi_HashMap_Malloc::RT_OK, 主key存在，且有数据
        *		TC_Multi_HashMap_Malloc::RT_ONLY_KEY, 主key存在，没有数据
        *		TC_Multi_HashMap_Malloc::RT_PART_DATA, 主key存在，里面的数据可能不完整
        *		TC_Multi_HashMap_Malloc::RT_NO_DATA, 主key不存在
        *       其他返回值: 错误
        */
        int checkMainKey(const string& mk)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.checkMainKey(mk);
        }

        int getMainKeyType(TC_Multi_HashMap_Malloc::MainKey::KEYTYPE &keyType)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.getMainKeyType(keyType);
        }

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
        int setFullData(const string &mk, bool bFull)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return this->_t.setFullData(mk, bFull);
        }

        /**
        * 检查坏block，并可进行修复
        * @param bRepaire, 是否进行修复
        *
        * @return size_t, 返回坏数据个数
        */
        size_t checkBadBlock(bool bRepair)
        {
            size_t c = this->_t.getHashCount();
            size_t e = 0;
            for (size_t i = 0; i < c; i++)
            {
                TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());

                e += this->_t.checkBadBlock(i, bRepair);
            }

            return e;
        }

        /**
         * block正序
         *
         * @return lock_iterator
         */
        lock_iterator begin()
        {
            JhmAutoLockPtr jlock(new JhmAutoLock(this->mutex()));
            return JhmLockIterator(this->_t.begin(), jlock);
        }

        /**
         * block逆序
         *
         * @return lock_iterator
         */
        lock_iterator rbegin()
        {
            JhmAutoLockPtr jlock(new JhmAutoLock(this->mutex()));
            return JhmLockIterator(this->_t.rbegin(), jlock);
        }

        /**
         * 以Set时间排序的迭代器
         * 返回的迭代器++表示按照时间顺序:最近Set-->最久Set
         *
         * @return lock_iterator
         */
        lock_iterator beginSetTime()
        {
            JhmAutoLockPtr jlock(new JhmAutoLock(this->mutex()));
            return JhmLockIterator(this->_t.beginSetTime(), jlock);
        }

        /**
         * Set时间链逆序的迭代器
         *
         * 返回的迭代器++表示按照时间顺序:最久Set-->最近Set
         *
         * @return lock_iterator
         */
        lock_iterator rbeginSetTime()
        {
            JhmAutoLockPtr jlock(new JhmAutoLock(this->mutex()));
            return JhmLockIterator(this->_t.rbeginSetTime(), jlock);
        }

        /**
         * 以Get时间排序的迭代器
         * 返回的迭代器++表示按照时间顺序:最近Get-->最久Get
         *
         * @return lock_iterator
         */
        lock_iterator beginGetTime()
        {
            JhmAutoLockPtr jlock(new JhmAutoLock(this->mutex()));
            return JhmLockIterator(this->_t.beginGetTime(), jlock);
        }

        /**
         * Get时间链逆序的迭代器
         *
         * 返回的迭代器++表示按照时间顺序:最久Get-->最近Get
         *
         * @return lock_iterator
         */
        lock_iterator rbeginGetTime()
        {
            JhmAutoLockPtr jlock(new JhmAutoLock(this->mutex()));
            return JhmLockIterator(this->_t.rbeginGetTime(), jlock);
        }

        /**
         * 获取脏链表尾部迭代器(最长时间没有Set的脏数据)
         *
         * 返回的迭代器++表示按照时间顺序:最近Set-->最久Set
         * 可能存在干净数据
         *
         * @return lock_iterator
         */
        lock_iterator beginDirty()
        {
            JhmAutoLockPtr jlock(new JhmAutoLock(this->mutex()));
            return JhmLockIterator(this->_t.beginDirty(), jlock);
        }

        /////////////////////////////////////////////////////////////////////////////////////////
        // 以下是遍历map函数, 不需要对map加锁

        /**
         * 根据hash桶遍历
         *
         * @return hash_iterator
         */
        hash_iterator hashBegin()
        {
            JhmLockPtr jlock(new JhmLock(this->mutex()));
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return JhmIterator(this->_t.hashBegin(), jlock);
        }

        /**
         * 结束
         *
         * @return
         */
        hash_iterator hashEnd()
        {
            JhmLockPtr jlock;
            return JhmIterator(this->_t.hashEnd(), jlock);
        }

        mk_hash_iterator mHashBegin()
        {
            JhmLockPtr jlock(new JhmLock(this->mutex()));
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());
            return JMKhmIterator(this->_t.mHashBegin(), jlock);
        }

        /**
         * 结束
         *
         * @return
         */
        mk_hash_iterator mHashEnd()
        {
            JhmLockPtr jlock;
            return JMKhmIterator(this->_t.mHashEnd(), jlock);
        }
    protected:

        /**
         * 删除数据的函数对象
         */
        ToDoFunctor                 *_todo_of;
    };

}

#endif
