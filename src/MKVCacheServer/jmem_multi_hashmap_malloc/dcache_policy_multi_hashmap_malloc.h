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
#ifndef DCACHE_JMEM_MULTI_HASHMAP_MALLOC_H__

#define DCACHE_JMEM_MULTI_HASHMAP_MALLOC_H__

#include "policy_multi_hashmap_malloc.h"
#include "tc_multi_hashmap_malloc.h"
#include "dcache_jmem_policy.h"
#include "NormalHash.h"
#include "util/tc_shm.h"
#include <math.h>
namespace DCache
{
    template<typename LockPolicy,
        template<class, class> class StorePolicy>
    class MultiHashMapMallocDCache
    {
    public:
        typedef PolicyMultiHashMapMalloc<LockPolicy, StorePolicy> JmemMultiHashMap;
        typedef typename JmemMultiHashMap::ToDoFunctor CacheToDoFunctor;
        typedef typename JmemMultiHashMap::ToDoFunctor::DataRecord CacheDataRecord;

        typedef typename JmemMultiHashMap::jMKhmItem	Jmem_multi_hash_mkItem;
        typedef typename JmemMultiHashMap::mk_hash_iterator Jmem_multi_hash_mkiterator;

        typedef typename JmemMultiHashMap::JhmItem	Jmem_multi_hash_Item;
        typedef typename JmemMultiHashMap::hash_iterator Jmem_multi_hash_iterator;



        typedef typename JmemMultiHashMap::Value	Value;
        typedef typename JmemMultiHashMap::PackValue PackValue;
        typedef typename JmemMultiHashMap::Expiretime Expiretime;
        typedef typename JmemMultiHashMap::DeleteData DeleteData;

        ///////////////////////////////////////////////////////////////
        /**
         * 迭代器,按照main key 迭代
         */
        struct MultiHashMKIterator
        {
        public:

            /**
             * 构造
             * @param it
             * @param lock
             */
            MultiHashMKIterator(MultiHashMapMallocDCache<LockPolicy, StorePolicy> * pMap, const Jmem_multi_hash_mkiterator & itr, unsigned short jmemIndex, unsigned short jmemNum)
                : _pMap(pMap), _itr(itr), _jmemIndex(jmemIndex), _jmemNum(jmemNum)
            {
            }

            /**
             * 拷贝构造
             * @param it
             */
            MultiHashMKIterator(const MultiHashMKIterator &it)
                : _pMap(it._pMap), _itr(it._itr), _jmemIndex(it._jmemIndex), _jmemNum(it._jmemNum)
            {
            }

            /**
             * 复制
             * @param it
             *
             * @return JhmIterator&
             */
            MultiHashMKIterator& operator=(const MultiHashMKIterator &it)
            {
                if (this != &it)
                {
                    _pMap = it._pMap;
                    _itr = it._itr;
                    _jmemIndex = it._jmemIndex;
                    _jmemNum = it._jmemNum;
                }

                return (*this);
            }

            /**
             *
             * @param it
             *
             * @return bool
             */
            bool operator==(const MultiHashMKIterator& it)
            {
                return (_pMap == it._pMap && _itr == it._itr && _jmemIndex == it._jmemIndex && _jmemNum == it._jmemNum);
            }

            /**
             *
             * @param mv
             *
             * @return bool
             */
            bool operator!=(const MultiHashMKIterator& it)
            {
                return !((*this) == it);
            }

            /**
             * 前置++
             *
             * @return JhmIterator&
             */
            MultiHashMKIterator& operator++()
            {
                ++_itr;
                if (_itr == _pMap->_multiHashMapVec[_jmemIndex]->mHashEnd())
                {
                    if (_jmemIndex != (_jmemNum - 1))
                    {
                        _jmemIndex++;
                        _itr = _pMap->_multiHashMapVec[_jmemIndex]->mHashBegin();
                    }
                }

                return (*this);
            }

            /**
             * 后置++
             *
             * @return JhmIterator&
             */
            MultiHashMKIterator operator++(int)
            {
                MultiHashMKIterator itTmp(*this);
                ++_itr;
                if (_itr == _pMap->_multiHashMapVec[_jmemIndex]->mHashEnd())
                {
                    if (_jmemIndex != (_jmemNum - 1))
                    {
                        _jmemIndex++;
                        _itr = _pMap->_multiHashMapVec[_jmemIndex]->mHashBegin();
                    }
                }
                return itTmp;
            }

            //设置索引值
            bool setIndex(uint32_t index)
            {
                return _itr.setIndex(index);
            }

            /**
             * 获取数据项
             *
             * @return JhmItem&
             */
            Jmem_multi_hash_mkItem& operator*() { return *_itr; }

            /**
             * 获取数据项
             *
             * @return JhmItem*
             */
            Jmem_multi_hash_mkItem* operator->() { return &(*_itr); }

        protected:
            MultiHashMapMallocDCache<LockPolicy, StorePolicy> * _pMap;

            Jmem_multi_hash_mkiterator  _itr;

            unsigned short _jmemIndex;

            unsigned short _jmemNum;
        };



        typedef MultiHashMKIterator mk_hash_iterator;




        /**
         * 迭代器,按照记录迭代
         */
        struct MultiHashIterator
        {
        public:

            /**
             * 构造
             * @param it
             * @param lock
             */
            MultiHashIterator(MultiHashMapMallocDCache<LockPolicy, StorePolicy> * pMap, const Jmem_multi_hash_iterator & itr, unsigned short jmemIndex, unsigned short jmemNum)
                : _pMap(pMap), _itr(itr), _jmemIndex(jmemIndex), _jmemNum(jmemNum)
            {
            }

            /**
             * 拷贝构造
             * @param it
             */
            MultiHashIterator(const MultiHashIterator &it)
                : _pMap(it._pMap), _itr(it._itr), _jmemIndex(it._jmemIndex), _jmemNum(it._jmemNum)
            {
            }

            /**
             * 复制
             * @param it
             *
             * @return JhmIterator&
             */
            MultiHashIterator& operator=(const MultiHashIterator &it)
            {
                if (this != &it)
                {
                    _pMap = it._pMap;
                    _itr = it._itr;
                    _jmemIndex = it._jmemIndex;
                    _jmemNum = it._jmemNum;
                }

                return (*this);
            }

            /**
             *
             * @param it
             *
             * @return bool
             */
            bool operator==(const MultiHashIterator& it)
            {
                return (_pMap == it._pMap && _itr == it._itr && _jmemIndex == it._jmemIndex && _jmemNum == it._jmemNum);
            }

            /**
             *
             * @param mv
             *
             * @return bool
             */
            bool operator!=(const MultiHashIterator& it)
            {
                return !((*this) == it);
            }

            /**
             * 前置++
             *
             * @return JhmIterator&
             */
            MultiHashIterator& operator++()
            {
                ++_itr;
                if (_itr == _pMap->_multiHashMapVec[_jmemIndex]->hashEnd())
                {
                    if (_jmemIndex != (_jmemNum - 1))
                    {
                        _jmemIndex++;
                        _itr = _pMap->_multiHashMapVec[_jmemIndex]->hashBegin();
                    }
                }

                return (*this);
            }

            /**
             * 后置++
             *
             * @return JhmIterator&
             */
            MultiHashIterator operator++(int)
            {
                MultiHashIterator itTmp(*this);
                ++_itr;
                if (_itr == _pMap->_multiHashMapVec[_jmemIndex]->hashEnd())
                {
                    if (_jmemIndex != (_jmemNum - 1))
                    {
                        _jmemIndex++;
                        _itr = _pMap->_multiHashMapVec[_jmemIndex]->hashBegin();
                    }
                }
                return itTmp;
            }

            /**
             * 获取数据项
             *
             * @return JhmItem&
             */
            Jmem_multi_hash_Item& operator*() { return *_itr; }

            /**
             * 获取数据项
             *
             * @return JhmItem*
             */
            Jmem_multi_hash_Item* operator->() { return &(*_itr); }

        protected:
            MultiHashMapMallocDCache<LockPolicy, StorePolicy> * _pMap;

            Jmem_multi_hash_iterator  _itr;

            unsigned short _jmemIndex;

            unsigned short _jmemNum;
        };




        typedef MultiHashIterator hash_iterator;









    public:
        MultiHashMapMallocDCache()
        {
            _pHash = new NormalHash();
        }
        ~MultiHashMapMallocDCache()
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                if (_multiHashMapVec[i] != NULL)
                {
                    delete _multiHashMapVec[i];
                }
            }
            if (_pHash != NULL)
            {
                delete _pHash;
            }
        }
        void init(unsigned int jmemNum)
        {
            _multiHashMapVec.clear();
            _jmemNum = jmemNum;
            for (unsigned int i = 0; i < jmemNum; i++)
            {
                JmemMultiHashMap * hashMapPtr = new JmemMultiHashMap();
                _multiHashMapVec.push_back(hashMapPtr);
            }
        }


        void initStore(key_t keyShm, size_t length, uint8_t keyType)
        {
            TLOGDEBUG("initStore start" << endl);
            _shm.init(length, keyShm);
            size_t oneJmemLength = length / _jmemNum;
            TLOGDEBUG("_jmemNum=" << _jmemNum << "|oneJmemLength=" << oneJmemLength << endl);

            if (_shm.iscreate())
            {
                for (size_t i = 0; i < _jmemNum; i++)
                {
                    _multiHashMapVec[i]->initStore(i*oneJmemLength, oneJmemLength, _shm.getPointer(), keyType, true);
                }
            }
            else
            {
                for (size_t i = 0; i < _jmemNum; i++)
                {
                    _multiHashMapVec[i]->initStore(i*oneJmemLength, oneJmemLength, _shm.getPointer(), keyType, false);
                }
            }
            TLOGDEBUG("initStore finish" << endl);
        }

        void initLock(key_t iKey, unsigned short SemNum, short index)
        {
            _Mutex.init(iKey, SemNum, index);
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _multiHashMapVec[i]->initLock(iKey, SemNum, i);
            }
            TLOGDEBUG("initLock finish" << endl);
        }
        void setSyncTime(uint32_t iSyncTime)
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _multiHashMapVec[i]->setSyncTime(iSyncTime);
            }
            TLOGDEBUG("setSyncTime finish" << endl);
        }

        void setMainKeyType(TC_Multi_HashMap_Malloc::MainKey::KEYTYPE keyType)
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _multiHashMapVec[i]->setMainKeyType(keyType);
            }
            TLOGDEBUG("setMainKeyType finish" << endl);
        }





        /**
        * 初始化主key数据大小
        * 这个大小将会被用来分配主key专门的内存数据块，通常主key与数据区的数据大小相关会比较大
        * 区分不同的大小将有助于更好地利用内存空间，否则使用数据区的内存块来存储主key将是浪费
        * 这个值也可以不设，不设将默认数据区的内在块来保存主key
        */
        void initMainKeySize(size_t iMainKeySize)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->initMainKeySize(iMainKeySize);
            }
        }

        /**
         * 初始化数据块平均大小
         * @param iDataSize
         */
        void initDataSize(size_t iDataSize)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->initDataSize(iDataSize);
            }
        }

        /**
         * 设置hash比率(设置chunk数据块/hash项比值, 默认是2)
         * 有需要更改必须在create之前调用
         *
         * @param fratio
         */
        void initHashRatio(float fratio)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->initHashRatio(fratio);
            }
        }

        /**
         * 初始化chunk个数/主key hash个数, 默认是1, 含义是一个主key下面大概有多个条数据
         * 有需要更改必须在create之前调用
         *
         * @param fratio
         */
        void initMainKeyHashRatio(float fratio)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->initMainKeyHashRatio(fratio);
            }
        }

        /**
         * 设置hash方式，这个hash函数将作为联合主键的hash函数
         * @param hash_of
         */
        void setHashFunctor(TC_Multi_HashMap_Malloc::hash_functor hashf)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->setHashFunctor(hashf);
            }
        }

        /**
         * 设置主key的hash方式，如果不设，主key将使用上面的联合主键的hash函数
         * @param hash_of
         */
        void setHashFunctorM(TC_Multi_HashMap_Malloc::hash_functor hashf)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->setHashFunctorM(hashf);
            }
        }

        /**
         * 获取hash方式
         *
         * @return TC_Multi_HashMap_Malloc::hash_functor&
         */
        TC_Multi_HashMap_Malloc::hash_functor &getHashFunctor()
        {
            //多个jmem使用的肯定是同一个hash函数
            return _multiHashMapVec[0]->getHashFunctor();
        }

        /* 获取主key hash方式
         *
         * @return TC_Multi_HashMap_Malloc::hash_functor&
         */
        TC_Multi_HashMap_Malloc::hash_functor &getHashFunctorM()
        {
            //多个jmem使用的肯定是同一个hash函数
            return  _multiHashMapVec[0]->getHashFunctorM();
        }

        /**
         * 设置淘汰操作类
         * @param erase_of
         */
        void setToDoFunctor(CacheToDoFunctor *todo_of)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->setToDoFunctor(todo_of);
            }
        }

        /**
         * 获取总数据容量，即数据可用内存大小
         *
         * @return size_t
         */
        size_t getDataMemSize()
        {
            size_t totalSize = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalSize += _multiHashMapVec[i]->getDataMemSize();
            }
            return totalSize;
        }

        int getUseRatio(unsigned int uJmemIndex, int& fRatio)
        {
            fRatio = _multiHashMapVec[uJmemIndex]->getMapHead()._iUsedDataMem * 100 / _multiHashMapVec[uJmemIndex]->getDataMemSize();
            if (_multiHashMapVec[uJmemIndex]->getMapHead()._iMainKeySize > 0)
            {
                //如果mk是单独的分配器，那么需要返回最高的比例,外部会根据这个比例来决定是否淘汰数据
                int fMKRatio = _multiHashMapVec[uJmemIndex]->getMapHead()._iMKUsedMem * 100 / _multiHashMapVec[uJmemIndex]->getMainKeyMemSize();
                if (fRatio < fMKRatio)
                {
                    fRatio = fMKRatio;
                }
            }
            return 0;
        }




        /**
         * 获取主key内存区的容量
         *
         * @return size_t
         */
        size_t getMainKeyMemSize()
        {
            size_t totalSize = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalSize += _multiHashMapVec[i]->getMainKeyMemSize();
            }
            return totalSize;
        }

        size_t getTotalElementCount()
        {
            size_t totalCount = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalCount += _multiHashMapVec[i]->getMapHead()._iElementCount;
            }
            return totalCount;
        }

        size_t getTotalDirtyCount()
        {
            size_t totalCount = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalCount += _multiHashMapVec[i]->getMapHead()._iDirtyCount;
            }
            return totalCount;
        }

        size_t getTotalDataUsedChunk()
        {
            size_t totalCount = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalCount += _multiHashMapVec[i]->getMapHead()._iDataUsedChunk;
            }
            return totalCount;
        }

        /**
         * 获取hash桶的个数
         *
         * @return size_t
         */
        size_t getHashCount()
        {
            size_t totalCount = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalCount += _multiHashMapVec[i]->getHashCount();
            }
            return totalCount;
        }

        size_t getHashCount(unsigned int i)
        {
            if (i >= _jmemNum)
            {
                return 0;
            }

            return _multiHashMapVec[i]->getHashCount();
        }

        /**
        * 获取主key hash桶个数
        */
        size_t getMainKeyHashCount()
        {
            size_t totalCount = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalCount += _multiHashMapVec[i]->getMainKeyHashCount();
            }
            return totalCount;
        }

        size_t getMainKeyHashCount(unsigned int i)
        {
            if (i >= _jmemNum)
            {
                return 0;
            }

            return _multiHashMapVec[i]->getMainKeyHashCount();
        }

        /**
         * 元素的个数
         *
         * @return size_t
         */
        size_t size()
        {
            size_t totalSize = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalSize += _multiHashMapVec[i]->size();
            }
            return totalSize;
        }

        size_t size(unsigned int i)
        {
            return _multiHashMapVec[i]->size();
        }

        /**
         * 脏数据元素个数
         *
         * @return size_t
         */
        size_t dirtyCount()
        {
            size_t totalDirtyCount = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalDirtyCount += _multiHashMapVec[i]->dirtyCount();
            }
            return totalDirtyCount;
        }

        size_t dirtyCount(unsigned int i)
        {
            return _multiHashMapVec[i]->dirtyCount();
        }

        /**
         * 主键下Only key数据元素个数
         *
         * @return size_t
         */
        size_t onlyKeyCount()
        {
            size_t totalOnlyKeyCount = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalOnlyKeyCount += _multiHashMapVec[i]->onlyKeyCount();
            }
            return totalOnlyKeyCount;
        }

        size_t onlyKeyCount(unsigned int i)
        {
            return _multiHashMapVec[i]->onlyKeyCount();
        }

        /**
         * 主key下Only key数据元素个数
         *
         * @return size_t
         */
        size_t onlyKeyCountM()
        {
            size_t totalOnlyKeyCount = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalOnlyKeyCount += _multiHashMapVec[i]->onlyKeyCountM();
            }
            return totalOnlyKeyCount;
        }

        /**
         * 设置每次淘汰数量
         * @param n
         */
        void setEraseCount(size_t n)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->setEraseCount(n);
            }
        }

        /**
         * 获取每次淘汰数量
         *
         * @return size_t
         */
        size_t getEraseCount()
        {
            return _multiHashMapVec[0]->getEraseCount();
        }

        /**
        * 设置主key下面最大数据个数，用于备机每天做镜像时的统计
        * @param n
        */
        void setMaxBlockCount(size_t n)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->setMaxBlockCount(n);
            }
        }

        /**
         * 设置只读
         * @param bReadOnly
         */
        void setReadOnly(bool bReadOnly)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->setReadOnly(bReadOnly);
            }
        }

        /**
         * 是否只读
         *
         * @return bool
         */
        bool isReadOnly()
        {
            return _multiHashMapVec[0]->isReadOnly();
        }

        /**
         * 设置是否可以自动淘汰
         * @param bAutoErase
         */
        void setAutoErase(bool bAutoErase)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->setAutoErase(bAutoErase);
            }
        }

        /**
         * 是否可以自动淘汰
         *
         * @return bool
         */
        bool isAutoErase()
        {
            return _multiHashMapVec[0]->isAutoErase();
        }

        /**
         * 设置淘汰方式
         * TC_Multi_HashMap_Malloc::ERASEBYGET
         * TC_Multi_HashMap_Malloc::ERASEBYSET
         * @param cEraseMode
         */
        void setEraseMode(char cEraseMode)
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->setEraseMode(cEraseMode);
            }
        }

        /**
         * 获取淘汰方式
         *
         * @return bool
         */
        char getEraseMode()
        {
            return _multiHashMapVec[0]->getEraseMode();
        }

        /**
         * 头部信息
         *
         * @return TC_Multi_HashMap_Malloc::tagMapHead
         */

        void getMapHead(vector<TC_Multi_HashMap_Malloc::tagMapHead> & headVtr)
        {
            headVtr.clear();
            for (unsigned int i = 0; i < _jmemNum; i++)
            {
                headVtr.push_back(_multiHashMapVec[i]->getMapHead());
            }
        }

        size_t getTotalMemSize()
        {
            size_t totalMemSize = 0;
            for (unsigned int i = 0; i < _jmemNum; i++)
            {
                totalMemSize += _multiHashMapVec[i]->getMapHead()._iMemSize;
            }
            return totalMemSize;
        }

        size_t getTotalUsedMemSize()
        {
            size_t totalUsedMemSize = 0;
            for (unsigned int i = 0; i < _jmemNum; i++)
            {
                totalUsedMemSize += _multiHashMapVec[i]->getMapHead()._iUsedDataMem;
            }
            return totalUsedMemSize;
        }

        size_t getTotalMKUsedMemSize()
        {
            size_t totalMKUsedMemSize = 0;
            for (unsigned int i = 0; i < _jmemNum; i++)
            {
                totalMKUsedMemSize += _multiHashMapVec[i]->getMapHead()._iMKUsedMem;
            }
            return totalMKUsedMemSize;
        }
        /**
         * dump到文件
         * @param sFile
         * @param bDoClear: 是否清空
         * @return int
         *          TC_Multi_HashMap_Malloc::RT_DUMP_FILE_ERR: dump到文件出错
         *          TC_Multi_HashMap_Malloc::RT_OK: dump到文件成功
         */
        int dump2file(const string &sFile)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(_Mutex);
            FILE *fp = fopen(sFile.c_str(), "wb");
            if (fp == NULL)
            {
                return TC_Multi_HashMap_Malloc::RT_DUMP_FILE_ERR;
            }

            size_t ret = fwrite((void*)_shm.getPointer(), 1, _shm.size(), fp);
            fclose(fp);

            if (ret == _shm.size())
            {
                return TC_Multi_HashMap_Malloc::RT_OK;
            }
            return TC_Multi_HashMap_Malloc::RT_DUMP_FILE_ERR;
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
            TC_LockT<typename LockPolicy::Mutex> lock(_Mutex);
            FILE *fp = fopen(sFile.c_str(), "rb");
            if (fp == NULL)
            {
                return TC_Multi_HashMap_Malloc::RT_LOAD_FILE_ERR;
            }
            fseek(fp, 0L, SEEK_END);
            size_t fs = ftell(fp);
            if (fs != _shm.size())
            {
                fclose(fp);
                return TC_Multi_HashMap_Malloc::RT_LOAD_FILE_ERR;
            }

            fseek(fp, 0L, SEEK_SET);

            size_t iSize = 1024 * 1024 * 10;
            size_t iLen = 0;
            char *pBuffer = new char[iSize];
            bool bEof = false;
            while (true)
            {
                int ret = fread(pBuffer, 1, iSize, fp);
                if (ret != (int)iSize)
                {
                    if (feof(fp))
                    {
                        bEof = true;
                    }
                    else
                    {
                        delete[] pBuffer;
                        return TC_Multi_HashMap_Malloc::RT_LOAD_FILE_ERR;
                    }

                }
                memcpy((char*)_shm.getPointer() + iLen, pBuffer, ret);
                iLen += ret;
                if (bEof)
                    break;
            }
            fclose(fp);
            delete[] pBuffer;
            if (iLen == _shm.size())
            {
                return TC_Multi_HashMap_Malloc::RT_OK;
            }

            return TC_Multi_HashMap_Malloc::RT_LOAD_FILE_ERR;
        }



        /**
         * 清空hash map
         * 所有map中的数据都被清空
         */
        void clear()
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->clear();
            }
        }


        int checkDirty(const string &mk, const string &uk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->checkDirty(mk, uk);
        }


        int checkDirty(const string &mk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->checkDirty(mk);
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
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->setClean(mk, uk);
        }


        int setDirty(const string &mk, const string &uk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->setDirty(mk, uk);
        }


        int setSyncTime(const string &mk, const string &uk, uint32_t iSyncTime)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->setSyncTime(mk, uk, iSyncTime);
        }





        int get(const string &mk, const string &uk, Value &v, bool bCheckExpire = false, uint32_t iExpireTime = -1)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->get(mk, uk, v, bCheckExpire, iExpireTime);
        }
        int get(const string &mk, const string &uk, Value &v, uint32_t &iSyncTime, uint32_t& iDateExpireTime, uint8_t& iVersion, bool& bDirty, bool bCheckExpire = false, uint32_t iExpireTime = -1)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->get(mk, uk, v, iSyncTime, iDateExpireTime, iVersion, bDirty, bCheckExpire, iExpireTime);
        }

        int get(const string& mk, vector<Value> &vs, size_t iCount = -1, size_t iStart = 0, bool bHead = true, bool bCheckExpire = false, uint32_t iExpireTime = -1)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->get(mk, vs, iCount, iStart, bHead, bCheckExpire, iExpireTime);
        }


        int get(const string &mk, const string &uk, vector<Value> &vs, size_t iCount = 1, bool bForward = true, bool bCheckExpire = false, uint32_t iExpireTime = -1)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->get(mk, uk, vs, iCount, bForward, bCheckExpire, iExpireTime);
        }

        int get(const string& mk, vector<Value> &vs, bool bHead)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->get(mk, vs, bHead);
        }

        int get(const string& mk, size_t &iDataCount)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->get(mk, iDataCount);
        }

        int get(const string& mk, vector<Value>& vs, bool bCheckExpire, uint32_t iExpireTime)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->get(mk, vs, bCheckExpire, iExpireTime);
        }


        template<typename C>
        int getHashM(uint32_t h, map<string, vector<Value> > &mv, C c)
        {
            return _multiHashMapVec[h%_jmemNum]->template getHashM<C>(h, mv, c);
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
            return _multiHashMapVec[h%_jmemNum]->template getHashM<C>(h, mv, c);
        }

        //插入一个onlykey数据，用于删除数据库
        int setForDel(const string &mk, const string &uk, const time_t t, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->setForDel(mk, uk, t, eType, bHead);
        }
        int set(const string &mk, const string &uk, const string &v, uint32_t iExpireTime, uint8_t iVersion, TC_Multi_HashMap_Malloc::DELETETYPE deleteType, bool bDirty = true, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true, bool bUpdateOrder = false, const uint32_t iMaxDataCount = 0, bool bDelDirty = false, bool bCheckExpire = false, uint32_t iNowTime = -1)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->set(mk, uk, v, iExpireTime, iVersion, bDirty, eType, bHead, bUpdateOrder, iMaxDataCount, deleteType, bDelDirty, bCheckExpire, iNowTime);
        }

        int set(const string &mk, const string &uk, TC_Multi_HashMap_Malloc::DELETETYPE deleteType, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true, bool bUpdateOrder = false)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->set(mk, uk, eType, bHead, bUpdateOrder, deleteType);
        }

        int set(const string &mk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->set(mk);
        }

        //对此接口的调用必须保障vs中所有的主key完全相同
        int set(const vector<Value> &vs, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true, bool bUpdateOrder = false, bool bOrderByItem = false, bool bForce = true)
        {
            if (vs.empty())
                return TC_Multi_HashMap_Malloc::RT_OK;
            return _multiHashMapVec[_pHash->HashRawString(vs[0]._mkey) % _jmemNum]->set(vs, eType, bHead, bUpdateOrder, bOrderByItem, bForce);
        }

        int update(const string &mk, const string &uk, const map<std::string, DCache::UpdateValue> &mpValue,
            const vector<DCache::Condition> & vtUKCond, const TC_Multi_HashMap_Malloc::FieldConf *fieldInfo, bool bLimit, size_t iIndex, size_t iCount, string &retValue, bool bCheckExpire, uint32_t iNowTime, uint32_t iExpireTime,
            bool bDirty = true, TC_Multi_HashMap_Malloc::DATATYPE eType = TC_Multi_HashMap_Malloc::AUTO_DATA, bool bHead = true, bool bUpdateOrder = false, const uint32_t iMaxDataCount = 0, bool bDelDirty = false)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->update(mk, uk, mpValue, vtUKCond, fieldInfo, bLimit, iIndex, iCount, retValue, bCheckExpire, iNowTime, iExpireTime, bDirty, eType, bHead, bUpdateOrder, iMaxDataCount, bDelDirty);
        }

        //不是真正的删除数据，只是把标记位设置
        int delSetBit(const string &mk, const string &uk, const time_t t)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delSetBit(mk, uk, t);
        }
        //不是真正的删除数据，只是把标记位设置
        int delSetBit(const string &mk, const string &uk, uint8_t iVersion, const time_t t)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delSetBit(mk, uk, iVersion, t);
        }
        //不是真正的删除数据，只是把标记位设置
        int delSetBit(const string &mk, const time_t t, uint64_t &delCount)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delSetBit(mk, t, delCount);
        }

        //真正删除数据，并且删除的时候检查删除标记是否设置
        int delReal(const string &mk, const string &uk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delReal(mk, uk);
        }

        int erase(const string &mk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->erase(mk);
        }


        int erase(const string &mk, const string &uk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->erase(mk, uk);
        }

        int erase(int ratio, unsigned int uJmemIndex, unsigned int uMaxEraseOneTime, bool bCheckDirty = false)
        {
            if (uJmemIndex >= _jmemNum)
            {
                return -1;
            }

            return _multiHashMapVec[uJmemIndex]->erase(ratio, uMaxEraseOneTime, bCheckDirty);
        }

        //强制删除数据，包括标记为del的数据
        int eraseByForce(const string &mk, const string &uk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->eraseByForce(mk, uk);
        }

        //强制删除主key下的所有数据，包括标记为del的数据
        int eraseByForce(const string &mk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->eraseByForce(mk);
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
            return _multiHashMapVec[h%_jmemNum]->eraseHashMByForce(h, c, vDelMK);
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
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->erase(ratio, false);
            }
            return 0;
        }

        int pushList(const string &mk, const vector<pair<uint32_t, string> > &v, const bool bHead, const bool bReplace, const uint64_t iPos, const uint32_t iNowTime)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->pushList(mk, v, bHead, bReplace, iPos, iNowTime);
        }

        int pushList(const string &mk, const vector<Value> &vt)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->pushList(mk, vt);
        }

        int getList(const string &mk, const uint64_t iStart, const uint64_t iEnd, const uint32_t iNowTime, vector<string> &vs)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->getList(mk, iStart, iEnd, iNowTime, vs);
        }

        int trimList(const string &mk, const bool bPop, const bool bHead, const bool bTrim, const uint64_t iStart, const uint64_t iCount, const uint32_t iNowTime, string &value, uint64_t &delSize)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->trimList(mk, bPop, bHead, bTrim, iStart, iCount, iNowTime, value, delSize);
        }

        int addSet(const string &mk, const string &v, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, TC_Multi_HashMap_Malloc::DELETETYPE deleteType)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->addSet(mk, v, iExpireTime, iVersion, bDirty, deleteType);
        }

        int addSet(const string &mk, const vector<Value> &vt, TC_Multi_HashMap_Malloc::DATATYPE eType)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->addSet(mk, vt, eType);
        }

        int addSet(const string &mk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->addSet(mk);
        }

        int getSet(const string &mk, const uint32_t iNowTime, vector<Value> &vtData)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->getSet(mk, iNowTime, vtData);
        }

        int delSetSetBit(const string &mk, const string &v, const time_t t)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delSetSetBit(mk, v, t);
        }

        int delSetSetBit(const string &mk, const time_t t)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delSetSetBit(mk, t);
        }

        int delSetReal(const string &mk, const string &v)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delSetReal(mk, v);
        }

        int addZSet(const string &mk, const string &v, double iScore, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, bool bInc, TC_Multi_HashMap_Malloc::DELETETYPE deleteType)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->addZSet(mk, v, iScore, iExpireTime, iVersion, bDirty, bInc, deleteType);
        }

        int addZSet(const string &mk, const vector<Value> &vt, TC_Multi_HashMap_Malloc::DATATYPE eType)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->addZSet(mk, vt, eType);
        }

        int addZSet(const string &mk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->addZSet(mk);
        }

        int getZSet(const string &mk, const uint64_t iStart, const uint64_t iEnd, const bool bUp, const uint32_t iNowTime, list<Value> &vtData)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->getZSet(mk, iStart, iEnd, bUp, iNowTime, vtData);
        }

        int getZSet(const string &mk, const string &v, const uint32_t iNowTime, Value &vData)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->getZSet(mk, v, iNowTime, vData);
        }

        int getZSetLimit(const string &mk, const size_t iStart, const size_t iDataCount, const bool bUp, const uint32_t iNowTime, list<Value> &vtData)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->getZSetLimit(mk, iStart, iDataCount, bUp, iNowTime, vtData);
        }

        int getScoreZSet(const string &mk, const string &v, const uint32_t iNowTime, double &iScore)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->getScoreZSet(mk, v, iNowTime, iScore);
        }

        int getRankZSet(const string &mk, const string &v, const bool order, const uint32_t iNowTime, long &iPos)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->getRankZSet(mk, v, order, iNowTime, iPos);
        }

        int getZSetByScore(const string &mk, const double iMin, const double iMax, const uint32_t iNowTime, list<Value> &vtData)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->getZSetByScore(mk, iMin, iMax, iNowTime, vtData);
        }

        int delZSetSetBit(const string &mk, const string &v, const time_t t)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delZSetSetBit(mk, v, t);
        }

        int delRangeZSetSetBit(const string &mk, const double iMin, const double iMax, const uint32_t iNowTime, const time_t t)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delRangeZSetSetBit(mk, iMin, iMax, iNowTime, t);
        }

        int delZSetReal(const string &mk, const string &v)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delZSetReal(mk, v);
        }

        int delZSetSetBit(const string &mk, const time_t t)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->delZSetSetBit(mk, t);
        }

        int updateZSet(const string &mk, const string &sOldValue, const string &sNewValue, double iScore, uint32_t iExpireTime, char iVersion, bool bDirty, bool bOnlyScore)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->updateZSet(mk, sOldValue, sNewValue, iScore, iExpireTime, iVersion, bDirty, bOnlyScore);
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
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->sync(mk, uk);
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
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->sync(iNowTime);
            }
            return 0;
        }


        /**
        *将脏数据尾指针赋给回写尾指针
        */
        void sync()
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->sync();
            }
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
            vector<int> iRetVec(_jmemNum, 0);
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                iRetVec[i] = _multiHashMapVec[i]->syncOnce(iNowTime);
            }

            bool bOK = true;
            bool bError = false;
            for (size_t i = 0; i < iRetVec.size(); i++)
            {
                if (iRetVec[i] != TC_Multi_HashMap_Malloc::RT_OK)
                    bOK = false;
                else if (iRetVec[i] != TC_Multi_HashMap_Malloc::RT_OK && iRetVec[i] != TC_Multi_HashMap_Malloc::RT_NONEED_SYNC && iRetVec[i] != TC_Multi_HashMap_Malloc::RT_ONLY_KEY&&iRetVec[i] != TC_Multi_HashMap_Malloc::RT_NEED_SYNC &&iRetVec[i] != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    bError = true;
            }
            if (bError)
            {
                return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;
            }
            else if (bOK)
            {
                return TC_Multi_HashMap_Malloc::RT_OK;
            }
            else
            {
                return TC_Multi_HashMap_Malloc::RT_NEED_SYNC;
            }
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

        template<typename C>
        int syncOnce(uint32_t iNowTime, C &c)
        {
            vector<int> iRetVec(_jmemNum, 0);
            for (size_t i = 0; i < _jmemNum; i++)
            {
                iRetVec[i] = _multiHashMapVec[i]->template syncOnce<C>(iNowTime, c);
            }

            bool bOK = true;
            bool bError = false;
            for (size_t i = 0; i < iRetVec.size(); i++)
            {
                if (iRetVec[i] != TC_Multi_HashMap_Malloc::RT_OK)
                    bOK = false;
                else if (iRetVec[i] != TC_Multi_HashMap_Malloc::RT_OK && iRetVec[i] != TC_Multi_HashMap_Malloc::RT_NONEED_SYNC && iRetVec[i] != TC_Multi_HashMap_Malloc::RT_ONLY_KEY&&iRetVec[i] != TC_Multi_HashMap_Malloc::RT_NEED_SYNC&&iRetVec[i] != TC_Multi_HashMap_Malloc::RT_DATA_DEL)
                    bError = true;
            }
            if (bError)
            {
                return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;
            }
            else if (bOK)
            {
                return TC_Multi_HashMap_Malloc::RT_OK;
            }
            else
            {
                return TC_Multi_HashMap_Malloc::RT_NEED_SYNC;
            }
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
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->backup(bForceFromBegin);
            }
            return 0;
        }

        int calculateData(int index, uint32_t &count, bool &isEnd)
        {
            return _multiHashMapVec[index]->calculateData(count, isEnd);
        }

        int resetCalculateData()
        {
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                _multiHashMapVec[i]->resetCalculateData();
            }
            return 0;
        }

        /**
         * 描述
         *
         * @return string
         */

        void desc(vector<string>& v_desc)
        {
            v_desc.clear();

            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                v_desc.push_back(_multiHashMapVec[i]->desc());
            }
        }

        /**
        * 描述
        *
        * @return string
        */

        void descWithHash(vector<string>& v_desc)
        {
            v_desc.clear();

            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                v_desc.push_back(_multiHashMapVec[i]->descWithHash());
            }
        }

        ///////////////////////////////////////////////////////////////////////////////

        /**
        * 查找主key下所有数据数量
        * @param mk, 主key
        *
        * @return size_t, 主key下的记录数
        */
        size_t count(const string &mk)
        {
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->count(mk);
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
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->checkMainKey(mk);
        }

        int getMainKeyType(TC_Multi_HashMap_Malloc::MainKey::KEYTYPE &keyType)
        {
            return _multiHashMapVec[0]->getMainKeyType(keyType);
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
            return _multiHashMapVec[_pHash->HashRawString(mk) % _jmemNum]->setFullData(mk, bFull);
        }

        /**
        * 检查坏block，并可进行修复
        * @param bRepaire, 是否进行修复
        *
        * @return size_t, 返回坏数据个数
        */
        size_t checkBadBlock(bool bRepair)
        {
            size_t totalBadBlock = 0;
            for (unsigned int i = 0; i < _jmemNum; ++i)
            {
                totalBadBlock += _multiHashMapVec[i]->checkBadBlock(bRepair);
            }
            return totalBadBlock;
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
            return MultiHashIterator(this, this->_multiHashMapVec[0]->hashBegin(), 0, _jmemNum);
        }


        hash_iterator hashEnd()
        {
            return MultiHashIterator(this, this->_multiHashMapVec[_jmemNum - 1]->hashEnd(), _jmemNum - 1, _jmemNum);
        }

        mk_hash_iterator mHashByPos(uint32_t index)
        {
            uint32_t tmpIndex = index;
            for (unsigned int i = 0; i < _jmemNum; i++)
            {
                MultiHashMKIterator it(this, this->_multiHashMapVec[i]->mHashBegin(), i, _jmemNum);
                uint32_t hashCount = it->getHashCount();
                if (tmpIndex <= (hashCount - 1))
                {
                    if (it.setIndex(tmpIndex) == false)
                        return mHashEnd();
                    else
                    {
                        return it;
                    }
                }
                else
                    tmpIndex -= hashCount;
            }
            //到这里表示有问题了
            return mHashEnd();
        }

        mk_hash_iterator mHashBegin()
        {
            return MultiHashMKIterator(this, this->_multiHashMapVec[0]->mHashBegin(), 0, _jmemNum);

        }



        mk_hash_iterator mHashEnd()
        {
            return MultiHashMKIterator(this, this->_multiHashMapVec[_jmemNum - 1]->mHashEnd(), _jmemNum - 1, _jmemNum);

        }











    private:
        vector<JmemMultiHashMap *> _multiHashMapVec;
        //jmem个数
        unsigned int _jmemNum;
        //共享内存
        TC_Shm _shm;
        //信号量集
        typename LockPolicy::Mutex _Mutex;

        NormalHash * _pHash;
    };
}

#endif

