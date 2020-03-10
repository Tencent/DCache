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
#ifndef DCACHE_JMEM_HASHMAP_MALLOC_H
#define DCACHE_JMEM_HASHMAP_MALLOC_H

#include "jmem_hashmap_malloc.h"
#include "tc_hashmap_malloc.h"
#include "util/tc_shm.h"
#include "NormalHash.h"
#include <math.h>

namespace DCache
{
    template<typename LockPolicy,
        template<class, class> class StorePolicy>
    class HashMapMallocDCache
    {
    public:
        typedef JmemHashMapMalloc<LockPolicy, StorePolicy> JmemHashMap;
        typedef typename JmemHashMap::ToDoFunctor CacheToDoFunctor;
        typedef typename JmemHashMap::ToDoFunctor::DataRecord CacheDataRecord;
        typedef typename JmemHashMap::hash_iterator Jmem_hash_iterator;
        typedef typename JmemHashMap::JhmItem  Jmem_hash_Item;
        ///////////////////////////////////////////////////////////////
        /**
         * 迭代器
         */
        struct DCacheJmemHashIterator
        {
        public:

            /**
             * 构造
             * @param it
             * @param lock
             */
            DCacheJmemHashIterator(HashMapMallocDCache<LockPolicy, StorePolicy> * pMap, const Jmem_hash_iterator & itr, unsigned short jmemIndex, unsigned short jmemNum)
                : _pMap(pMap), _itr(itr), _jmemIndex(jmemIndex), _jmemNum(jmemNum)
            {
            }

            /**
             * 拷贝构造
             * @param it
             */
            DCacheJmemHashIterator(const DCacheJmemHashIterator &it)
                : _pMap(it._pMap), _itr(it._itr), _jmemIndex(it._jmemIndex), _jmemNum(it._jmemNum)
            {
            }

            /**
             * 复制
             * @param it
             *
             * @return JhmIterator&
             */
            DCacheJmemHashIterator& operator=(const DCacheJmemHashIterator &it)
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
            bool operator==(const DCacheJmemHashIterator& it)
            {
                return (_pMap == it._pMap && _itr == it._itr && _jmemIndex == it._jmemIndex && _jmemNum == it._jmemNum);
            }

            /**
             *
             * @param mv
             *
             * @return bool
             */
            bool operator!=(const DCacheJmemHashIterator& it)
            {
                return !((*this) == it);
            }

            /**
             * 前置++
             *
             * @return JhmIterator&
             */
            DCacheJmemHashIterator& operator++()
            {
                ++_itr;
                if (_itr == _pMap->_hashMapVec[_jmemIndex]->hashEnd())
                {
                    if (_jmemIndex != (_jmemNum - 1))
                    {
                        _jmemIndex++;
                        _itr = _pMap->_hashMapVec[_jmemIndex]->hashBegin();
                    }
                }

                return (*this);
            }

            /**
             * 后置++
             *
             * @return JhmIterator&
             */
            DCacheJmemHashIterator operator++(int)
            {
                DCacheJmemHashIterator itTmp(*this);
                ++_itr;
                if (_itr == _pMap->_hashMapVec[_jmemIndex]->hashEnd())
                {
                    if (_jmemIndex != (_jmemNum - 1))
                    {
                        _jmemIndex++;
                        _itr = _pMap->_hashMapVec[_jmemIndex]->hashBegin();
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
            Jmem_hash_Item& operator*() { return *_itr; }

            /**
             * 获取数据项
             *
             * @return JhmItem*
             */
            Jmem_hash_Item* operator->() { return &(*_itr); }

        protected:
            HashMapMallocDCache<LockPolicy, StorePolicy> * _pMap;

            Jmem_hash_iterator  _itr;

            unsigned short _jmemIndex;

            unsigned short _jmemNum;
        };

        typedef DCacheJmemHashIterator dcache_hash_iterator;
    public:
        HashMapMallocDCache()
        {
            _pHash = new NormalHash();
        }
        ~HashMapMallocDCache()
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                if (_hashMapVec[i] != NULL)
                {
                    delete _hashMapVec[i];
                }
            }
            if (_pHash != NULL)
            {
                delete _pHash;
            }
        }
        void init(unsigned int jmemNum)
        {
            _hashMapVec.clear();
            _jmemNum = jmemNum;
            for (unsigned int i = 0; i < jmemNum; i++)
            {
                JmemHashMap * hashMapPtr = new JmemHashMap();
                _hashMapVec.push_back(hashMapPtr);
            }
        }
        void initHashRadio(float fRadio)
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->initHashRadio(fRadio);

            }
        }
        void initAvgDataSize(size_t iAvgDataSize)
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->initAvgDataSize(iAvgDataSize);
            }
        }

        void initStore(key_t keyShm, size_t length)
        {
            TLOGDEBUG("initStore start" << endl);
            _shm.init(length, keyShm);
            size_t oneJmemLength = length / _jmemNum;
            TLOGDEBUG("_jmemNum=" << _jmemNum << "|oneJmemLength=" << oneJmemLength << endl);

            if (_shm.iscreate())
            {
                for (size_t i = 0; i < _jmemNum; i++)
                {
                    _hashMapVec[i]->initStore(i*oneJmemLength, oneJmemLength, _shm.getPointer(), true);
                }
            }
            else
            {
                for (size_t i = 0; i < _jmemNum; i++)
                {
                    _hashMapVec[i]->initStore(i*oneJmemLength, oneJmemLength, _shm.getPointer(), false);
                }
            }
            TLOGDEBUG("initStore finish" << endl);
        }

        void initLock(key_t iKey, unsigned short SemNum, short index)
        {
            _Mutex.init(iKey, SemNum, index);
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->initLock(iKey, SemNum, i);
            }
            TLOGDEBUG("initLock finish" << endl);
        }
        void setSyncTime(uint32_t iSyncTime)
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->setSyncTime(iSyncTime);
            }
            TLOGDEBUG("setSyncTime finish" << endl);
        }

        void setToDoFunctor(CacheToDoFunctor *todo_of)
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->setToDoFunctor(todo_of);
            }
            TLOGDEBUG("setToDoFunctor finish" << endl);
        }

        void setHashFunctor(TC_HashMapMalloc::hash_functor hashf)
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->setHashFunctor(hashf);
            }
            TLOGDEBUG("setHashFunctor finish" << endl);
        }
        void setAutoErase(bool bAutoErase)
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->setAutoErase(bAutoErase);
            }
            TLOGDEBUG("setAutoErase finish" << endl);
        }

        int eraseByForce(const string& k)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->eraseByForce(k);
        }
        int set(const string& k, const string& v, bool bDirty = true, uint32_t iExpireTime = 0, uint8_t iVersion = 0, bool bCheckExpire = false, uint32_t iNowTime = -1)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->set(k, v, bDirty, iExpireTime, iVersion, bCheckExpire, iNowTime);
        }

        int set(const string& k, uint8_t iVersion = 0)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->set(k, iVersion);
        }

//        int update(const string& k, const string& v, Op option, bool bDirty = true, uint32_t iExpireTime = 0, bool bCheckExpire = false, uint32_t iNowTime = -1, string &retValue = "")
	    int update(const string& k, const string& v, Op option, bool bDirty, uint32_t iExpireTime , bool bCheckExpire, uint32_t iNowTime , string &retValue)
	    {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->update(k, v, option, bDirty, iExpireTime, bCheckExpire, iNowTime, retValue);
        }

        int del(const string& k)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->del(k);
        }

        int del(const string& k, const uint8_t iVersion)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->del(k, iVersion);
        }

        int delExpire(const string& k)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->delExpire(k);
        }

        int get(const string& k, string &v, uint32_t &iSyncTime, uint32_t& iExpireTime, uint8_t& iVersion, bool bCheckExpire = false, uint32_t iNowTime = -1)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->get(k, v, iSyncTime, iExpireTime, iVersion, bCheckExpire, iNowTime);
        }

        int get(const string& k, string &v, uint32_t &iSyncTime, uint32_t& iExpireTime, uint8_t& iVersion, bool& bDirty, bool bCheckExpire = false, uint32_t iNowTime = -1)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->get(k, v, iSyncTime, iExpireTime, iVersion, bDirty, bCheckExpire, iNowTime);
        }

        int get(const string& k, string &v, bool bCheckExpire = false, uint32_t iNowTime = -1)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->get(k, v, bCheckExpire, iNowTime);
        }

        int checkDirty(const string &k, bool bCheckExpire = false, uint32_t iNowTime = -1)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->checkDirty(k, bCheckExpire, iNowTime);
        }

        int erase(const string& k)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->erase(k);
        }

        int erase(const string& k, const uint8_t iVersion)
        {
            return _hashMapVec[_pHash->HashRawString(k) % _jmemNum]->erase(k, iVersion);
        }

        template<typename C>
        int eraseHashByForce(size_t h, C c)
        {
            return _hashMapVec[h%_jmemNum]->template eraseHashByForce<C>(h, c);
        }

        template<typename C>
        int eraseHashByForce(size_t h, C c, vector<string>& vDelK)
        {
            return _hashMapVec[h%_jmemNum]->template eraseHashByForce<C>(h, c, vDelK);
        }

        template<typename C>
        int getHash(size_t h, vector<pair<string, string> > &vv, C c)
        {
            return _hashMapVec[h%_jmemNum]->template getHash<C>(h, vv, c);
        }

        template<typename C>
        int getHash(size_t h, vector<CacheDataRecord> &vv, C c)
        {
            return _hashMapVec[h%_jmemNum]->template getHash<C>(h, vv, c);
        }

        template<typename C>
        int getHashWithOnlyKey(size_t h, vector<CacheDataRecord> &vv, C c)
        {
            return _hashMapVec[h%_jmemNum]->template getHashWithOnlyKey<C>(h, vv, c);
        }

        void getMapHead(vector<TC_HashMapMalloc::tagMapHead> & headVtr)
        {
            headVtr.clear();
            for (size_t i = 0; i < _jmemNum; i++)
            {
                headVtr.push_back(_hashMapVec[i]->getMapHead());
            }
        }

        size_t getHashCount()
        {
            size_t hashCount = 0;
            for (size_t i = 0; i < _jmemNum; i++)
            {
                hashCount += _hashMapVec[i]->getHashCount();
            }
            return hashCount;
        }

        void clear()
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->clear();
            }
        }

        int erase(int radio, bool bCheckDirty = false)
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->erase(radio, bCheckDirty);
            }
            return 0;
        }

        int getUseRatio(unsigned int uJmemIndex, int& iRatio)
        {
            if (uJmemIndex >= _jmemNum)
            {
                return -1;
            }

            iRatio = _hashMapVec[uJmemIndex]->getMapHead()._iUsedDataMem * 100 / _hashMapVec[uJmemIndex]->getDataMemSize();
            return 0;
        }

        int eraseByID(int radio, unsigned int uJmemIndex, unsigned int uMaxEraseOneTime, bool bCheckDirty = false)
        {
            if (uJmemIndex >= _jmemNum)
            {
                return -1;
            }
            return _hashMapVec[uJmemIndex]->erase(radio, uMaxEraseOneTime, bCheckDirty);
        }

        size_t dirtyCount()
        {
            size_t dirtyCount = 0;
            for (size_t i = 0; i < _jmemNum; i++)
            {
                dirtyCount += _hashMapVec[i]->dirtyCount();
            }
            return dirtyCount;
        }

        size_t size()
        {
            size_t eleCount = 0;
            for (size_t i = 0; i < _jmemNum; i++)
            {
                eleCount += _hashMapVec[i]->size();
            }
            return eleCount;
        }

        size_t onlyKeyCount()
        {
            size_t onlyKeyCount = 0;
            for (size_t i = 0; i < _jmemNum; i++)
            {
                onlyKeyCount += _hashMapVec[i]->onlyKeyCount();
            }
            return onlyKeyCount;
        }

        size_t usedChunkCount()
        {
            size_t usedChunkCount = 0;
            for (size_t i = 0; i < _jmemNum; i++)
            {
                usedChunkCount += _hashMapVec[i]->usedChunkCount();
            }
            return usedChunkCount;
        }

        void sync()
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->sync();
            }
        }

        template<typename C>
        int syncOnce(uint32_t iNowTime, C &c)
        {
            vector<int> iRetVec(_jmemNum, 0);
            for (size_t i = 0; i < _jmemNum; i++)
            {
                iRetVec[i] = _hashMapVec[i]->template syncOnce<C>(iNowTime, c);
            }
            bool bOK = true;
            bool bError = false;
            for (size_t i = 0; i < iRetVec.size(); i++)
            {
                if (iRetVec[i] != TC_HashMapMalloc::RT_OK)
                    bOK = false;
                else if (iRetVec[i] != TC_HashMapMalloc::RT_OK && iRetVec[i] != TC_HashMapMalloc::RT_NONEED_SYNC && iRetVec[i] != TC_HashMapMalloc::RT_ONLY_KEY&&iRetVec[i] != TC_HashMapMalloc::RT_NEED_SYNC)
                    bError = true;
            }
            if (bError)
            {
                return TC_HashMapMalloc::RT_EXCEPTION_ERR;
            }
            else if (bOK)
            {
                return TC_HashMapMalloc::RT_OK;
            }
            else
            {
                return TC_HashMapMalloc::RT_NEED_SYNC;
            }
        }

        int calculateData(int index, uint32_t &count, bool &isEnd)
        {
            return _hashMapVec[index]->calculateData(count, isEnd);
        }

        int resetCalculateData()
        {
            for (size_t i = 0; i < _jmemNum; i++)
            {
                _hashMapVec[i]->resetCalculateData();
            }
            return 0;
        }

        string descDetail()
        {
            string descStr("");
            for (size_t i = 0; i < _jmemNum; i++)
            {
                descStr += "--------------------------------------------\n";
                descStr += "JMEMNUM: " + TC_Common::tostr(i) + "\n";
                descStr += _hashMapVec[i]->desc();
            }
            return descStr;
        }

        size_t getDataMemSize()
        {
            size_t dataMemSize = 0;
            for (size_t i = 0; i < _jmemNum; i++)
            {
                dataMemSize += _hashMapVec[i]->getDataMemSize();
            }
            return dataMemSize;
        }
        size_t getMemSize()
        {
            size_t iMemSize = 0;
            for (size_t i = 0; i < _jmemNum; i++)
            {
                iMemSize += _hashMapVec[i]->getMapHead()._iMemSize;
            }
            return iMemSize;
        }
        size_t getUsedDataMem()
        {
            size_t iUsedDataMem = 0;
            for (size_t i = 0; i < _jmemNum; i++)
            {
                iUsedDataMem += _hashMapVec[i]->getMapHead()._iUsedDataMem;
            }
            return iUsedDataMem;
        }

        size_t getTotalElementCount()
        {
            size_t totalCount = 0;
            for (size_t i = 0; i < _jmemNum; i++)
            {
                totalCount += _hashMapVec[i]->getMapHead()._iElementCount;
            }

            return totalCount;
        }
        dcache_hash_iterator hashByPos(uint32_t index)
        {
            uint32_t tmpIndex = index;
            for (unsigned int i = 0; i < _jmemNum; i++)
            {
                dcache_hash_iterator it(this, this->_hashMapVec[i]->hashBegin(), i, _jmemNum);
                uint32_t hashCount = it->getHashCount();
                if (tmpIndex <= (hashCount - 1))
                {
                    if (it.setIndex(tmpIndex) == false)
                        return hashEnd();
                    else
                    {
                        return it;
                    }
                }
                else
                    tmpIndex -= hashCount;
            }
            //到这里表示有问题了
            return hashEnd();
        }
        dcache_hash_iterator hashBegin()
        {
            return dcache_hash_iterator(this, this->_hashMapVec[0]->hashBegin(), 0, _jmemNum);
        }
        dcache_hash_iterator hashEnd()
        {
            return dcache_hash_iterator(this, this->_hashMapVec[_jmemNum - 1]->hashEnd(), _jmemNum - 1, _jmemNum);
        }
        int dump2file(const string &sFile)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(_Mutex);
            FILE *fp = fopen(sFile.c_str(), "wb");
            if (fp == NULL)
            {
                return TC_HashMapMalloc::RT_DUMP_FILE_ERR;
            }

            size_t ret = fwrite((void*)_shm.getPointer(), 1, _shm.size(), fp);
            fclose(fp);

            if (ret == _shm.size())
            {
                return TC_HashMapMalloc::RT_OK;
            }
            return TC_HashMapMalloc::RT_DUMP_FILE_ERR;
        }
        int load5file(const string &sFile)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(_Mutex);
            FILE *fp = fopen(sFile.c_str(), "rb");
            if (fp == NULL)
            {
                return TC_HashMapMalloc::RT_LOAL_FILE_ERR;
            }
            fseek(fp, 0L, SEEK_END);
            size_t fs = ftell(fp);
            if (fs != _shm.size())
            {
                fclose(fp);
                return TC_HashMapMalloc::RT_LOAL_FILE_ERR;
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
                        return TC_HashMapMalloc::RT_LOAL_FILE_ERR;
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
                return TC_HashMapMalloc::RT_OK;
            }

            return TC_HashMapMalloc::RT_LOAL_FILE_ERR;
        }
    private:
        vector<JmemHashMap *> _hashMapVec;
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

