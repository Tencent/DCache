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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "dcache_sem_mutex.h"

namespace DCache
{

    DCache_SemMutex::DCache_SemMutex()
    {

    }

    DCache_SemMutex::DCache_SemMutex(key_t iKey, unsigned short SemNum, short index)
    {
        init(iKey, SemNum, index);
    }

    void DCache_SemMutex::init(key_t iKey, unsigned short SemNum, short index)
    {
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
        /* union semun is defined by including <sys/sem.h> */
#else
        /* according to X/OPEN we have to define it ourselves */
        union semun
        {
            int val;                  /* value for SETVAL */
            struct semid_ds *buf;     /* buffer for IPC_STAT, IPC_SET */
            unsigned short *array;    /* array for GETALL, SETALL */
                                      /* Linux specific part: */
            struct seminfo *__buf;    /* buffer for IPC_INFO */
        };
#endif

        int  iSemID;
        union semun arg;
        //u_short array[SemNum] = { 0, 0 };
        u_short * arrayShort = new u_short[SemNum];
        for (unsigned short i = 0; i < SemNum; i++)
        {
            arrayShort[i] = 0;
        }
        //生成信号量集
        if ((iSemID = semget(iKey, SemNum, IPC_CREAT | IPC_EXCL | 0666)) != -1)
        {
            arg.array = &arrayShort[0];

            //将所有信号量的值设置为0
            if (semctl(iSemID, 0, SETALL, arg) == -1)
            {
                delete[] arrayShort;
                throw DCache_SemMutex_Exception("[TC_SemMutex::init] semctl error:" + string(strerror(errno)));
            }
        }
        else
        {
            //信号量已经存在
            if (errno != EEXIST)
            {
                delete[] arrayShort;
                throw DCache_SemMutex_Exception("[TC_SemMutex::init] sem has exist error:" + string(strerror(errno)));
            }

            //连接信号量
            if ((iSemID = semget(iKey, SemNum, 0666)) == -1)
            {
                delete[] arrayShort;
                throw DCache_SemMutex_Exception("[TC_SemMutex::init] connect sem error:" + string(strerror(errno)));
            }
        }

        _semKey = iKey;
        _semID = iSemID;
        _index = index;
        _semNum = SemNum;
        delete[] arrayShort;
    }

    int DCache_SemMutex::wlock() const
    {
        if (_index != -1)
        {
            //加排他锁
            struct sembuf sops[2] = { {(unsigned short)_index, 0, SEM_UNDO}, {(unsigned short)_index, 1, SEM_UNDO} };
            size_t nsops = 2;

            int ret = -1;

            do
            {
                ret = semop(_semID, &sops[0], nsops);

            } while ((ret == -1) && (errno == EINTR));

            return ret;
        }
        else
        {
            struct sembuf * sops = (struct sembuf *)malloc(sizeof(struct sembuf)*_semNum * 2);
            for (int i = 0; i < _semNum * 2; i++)
            {
                if (i % 2 == 0)
                {
                    struct sembuf tmp[1] = { {(unsigned short)(i / 2), 0, SEM_UNDO} };
                    sops[i] = tmp[0];
                }
                else
                {
                    struct sembuf tmp[1] = { {(unsigned short)((i - 1) / 2), 1, SEM_UNDO} };
                    sops[i] = tmp[0];
                }
            }
            int ret = -1;
            size_t nsops = _semNum * 2;
            do
            {
                ret = semop(_semID, sops, nsops);

            } while ((ret == -1) && (errno == EINTR));
            free(sops);
            return ret;
        }
    }

    int DCache_SemMutex::unwlock() const
    {
        if (_index != -1)
        {
            //解除排他锁
            struct sembuf sops[1] = { {(unsigned short)_index, -1, SEM_UNDO} };
            size_t nsops = 1;

            int ret = -1;

            do
            {
                ret = semop(_semID, &sops[0], nsops);

            } while ((ret == -1) && (errno == EINTR));

            return ret;
        }
        else
        {
            struct sembuf * sops = (struct sembuf *)malloc(sizeof(struct sembuf)*_semNum);
            for (int i = 0; i < _semNum; i++)
            {
                struct sembuf tmp[1] = { {(unsigned short)i, -1, SEM_UNDO} };
                sops[i] = tmp[0];
            }
            int ret = -1;
            size_t nsops = _semNum;
            do
            {
                ret = semop(_semID, sops, nsops);

            } while ((ret == -1) && (errno == EINTR));
            free(sops);
            return ret;
        }
    }
//
//    ProcessSem::~ProcessSem()
//    {
//        if (_bInit)
//        {
//            sem_destroy(&_sem);
//            _bInit = false;
//        }
//    }
//
//    void ProcessSem::init(int pshared/* = 1*/, unsigned int value /*= 1*/)
//    {
//        if (_bInit)
//        {
//            return;
//        }
//        int ret = sem_init(&_sem, pshared, value);
//        if (0 != ret)
//        {
//            throw TC_ProcessSem_Exception("[ProcessSem::init] fail. error:" + string(strerror(errno)));
//        }
//        _bInit = true;
//    }
//
//    int ProcessSem::lock()
//    {
//        int ret = -1;
//        do
//        {
//            ret = sem_wait(&_sem);
//        } while ((ret == -1) && (errno == EINTR));
//        return ret;
//    }
//
//
//    int ProcessSem::unlock()
//    {
//        int ret = -1;
//        do
//        {
//            ret = sem_post(&_sem);
//
//        } while ((ret == -1) && (errno == EINTR));
//        return ret;
//    }
//
//    bool ProcessSem::tryLock()
//    {
//        int ret = sem_trywait(&_sem);
//        if (ret == -1)
//        {
//            if (errno == EAGAIN)
//            {
//                //无法获得锁
//                return false;
//            }
//            else
//            {
//                throw TC_ProcessSem_Exception("[ProcessSem::tryLock] fail. error:" + string(strerror(errno)));
//            }
//        }
//        return true;
//    }



}

