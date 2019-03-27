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
#ifndef __DCACHE_SEM_MUTEX_H
#define __DCACHE_SEM_MUTEX_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "util/tc_lock.h"
#include <semaphore.h>


namespace tars
{
    /////////////////////////////////////////////////
    // 说明: 信号量锁类
    // Author : jarodruan@tencent.com              
    /////////////////////////////////////////////////
    /**
    * 信号量锁异常类
    */
    struct DCache_SemMutex_Exception : public TC_Lock_Exception
    {
        DCache_SemMutex_Exception(const string &buffer) : TC_Lock_Exception(buffer) {};
        DCache_SemMutex_Exception(const string &buffer, int err) : TC_Lock_Exception(buffer, err) {};
        ~DCache_SemMutex_Exception() throw() {};
    };

    /**
    * 进程间锁, 排斥锁
    * 1 对于相同的key, 不同进程初始化时连接到相同的sem上
    * 2 采用IPC的信号量实现
    * 3 信号量采用了SEM_UNDO参数, 当进程结束时会自动调整信号量
    */
    class DCache_SemMutex
    {
    public:
        /**
         * 构造函数
         */
        DCache_SemMutex();

        /**
        * 构造函数
        * @param iKey, key
        * @throws TC_SemMutex_Exception
        */
        DCache_SemMutex(key_t iKey, unsigned short SemNum, short index = -1);

        /**
        * 初始化
        * @param iKey, key
        * @throws TC_SemMutex_Exception
        * @return 无
         */
        void init(key_t iKey, unsigned short SemNum, short index = -1);

        /**
        * 获取共享内存Key
        * @return key_t ,共享内存key
        */
        key_t getkey() const { return _semKey; }

        /**
        * 获取共享内存ID
        * @return int ,共享内存Id
        */
        int getid() const { return _semID; }

        /**
        * 加写锁
        * @return int
        */
        int wlock() const;

        /**
        * 解写锁
        */
        int unwlock() const;


        /**
        * 写锁
        * @return int, 0 正确
        */
        int lock() const { return wlock(); };

        /**
        * 解写锁
        */
        int unlock() const { return unwlock(); };

        //信号量编号
        short _index;

    protected:
        /**
         * 信号量ID
         */
        int _semID;

        /**
         * 信号量key
         */
        key_t _semKey;

        /**
       * 信号量个数
       */
        int _semNum;


    };



    struct TC_ProcessSem_Exception : public TC_Lock_Exception
    {
        TC_ProcessSem_Exception(const string &buffer) : TC_Lock_Exception(buffer) {};
        TC_ProcessSem_Exception(const string &buffer, int err) : TC_Lock_Exception(buffer, err) {};
        ~TC_ProcessSem_Exception() throw() {};
    };


    class ProcessSem
    {
    public:
        ProcessSem() {};
        ~ProcessSem();
        void init(int pshared = 1, unsigned int value = 1);
        int lock();
        int unlock();
        bool tryLock();

    private:
        sem_t _sem;
        bool _bInit;
    };



}

#endif
