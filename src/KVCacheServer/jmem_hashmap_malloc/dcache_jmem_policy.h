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
#ifndef DCache_JMEM_POLICY_H
#define DCache_JMEM_POLICY_H

#include "util/tc_thread_mutex.h"
#include "dcache_sem_mutex.h"
#include "util/tc_mmap.h"
namespace DCache
{
    //////////////////////////////////////////////////////////////////////
    // 存储策略: 内存, 共享内存, mmap(文件)

    /**
     * 内存存储
     */
    template<typename T, typename LockPolicy>
    class MemStorePolicyDCache : public LockPolicy
    {
    public:
        /**
        * 初始化
        * @param pAddr: 指令队列空间的指针
        * @param iSize: 空间的指针
        */
        void create(void *pAddr, size_t iSize)
        {
            _t.create(pAddr, iSize);
        }

        /**
        * 连接上队列
        * @param pAddr: 指令队列空间的指针
        * @param iSize: 空间的指针
        */
        void connect(void *pAddr, size_t iSize)
        {
            _t.connect(pAddr, iSize);
        }

    protected:
        T   _t;
    };

    /**
     * 共享内存存储
     */
    template<typename T, typename LockPolicy>
    class ShmStorePolicyDCache : public LockPolicy
    {
    public:
        /**
         * 初始化共享存储
         * @param iShmKey
         * @param iSize
         */
        void initStore(size_t iStart, size_t iSize, void*shmaddr, bool isCreate)
        {
            _shmPtr = shmaddr;
            if (isCreate)
            {
                _t.create((char*)_shmPtr + iStart, iSize);
            }
            else
            {
                _t.connect((char*)_shmPtr + iStart, iSize);
            }
        }
    protected:
        void * _shmPtr;
        T       _t;
    };

    /**
     * 文件存储
     */
    template<typename T, typename LockPolicy>
    class FileStorePolicyDCache : public LockPolicy
    {
    public:
        /**
         * 初始化文件
         * @param file, 文件路径
         * @param iSize, 文件大小
         */
        void initStore(const char *file, size_t iSize)
        {
            _file = file;
            _mmap.mmap(file, iSize);
            if (_mmap.iscreate())
            {
                _t.create(_mmap.getPointer(), iSize);
            }
            else
            {
                _t.connect(_mmap.getPointer(), iSize);
            }
        }

        /**
         * 扩展空间, 目前只对hashmap有效
         */
        int expand(size_t iSize)
        {
            TC_LockT<typename LockPolicy::Mutex> lock(LockPolicy::mutex());

            TC_Mmap m(false);
            m.mmap(_file.c_str(), iSize);

            int ret = _t.append(m.getPointer(), iSize);

            if (ret == 0)
            {
                _mmap.munmap();
                _mmap = m;
                _mmap.setOwner(true);
            }
            else
            {
                m.munmap();
            }

            return ret;
        }

    protected:
        string  _file;
        TC_Mmap _mmap;
        T       _t;
    };

    //////////////////////////////////////////////////////////////////////
    // 锁策略: 无锁, 线程锁, 进程锁

    /**
     * 无锁
     */
    class EmptyLockPolicyDCache
    {
    public:
        typedef TC_EmptyMutex Mutex;
        Mutex &mutex() { return _mutex; }

    protected:
        Mutex _mutex;
    };

    /**
     * 线程锁策略
     */
    class ThreadLockPolicyDCache
    {
    public:
        typedef TC_ThreadMutex Mutex;
        Mutex &mutex() { return _mutex; }

    protected:
        Mutex _mutex;
    };

    /**
     * 进程锁策略
     */

    class SemLockPolicyDCache
    {
    public:
        typedef DCache_SemMutex Mutex;
        void initLock(key_t iKey, unsigned short SemNum, short index) { return _mutex.init(iKey, SemNum, index); }
        Mutex &mutex() { return _mutex; }

    protected:
        Mutex _mutex;
    };

}

#endif
