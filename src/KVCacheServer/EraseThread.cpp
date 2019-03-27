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
#include "EraseThread.h"

void EraseThread::init(const string &sConf)
{
    _config = sConf;
    _tcConf.parseFile(_config);
    _shmNum = TC_Common::strto<unsigned int>(_tcConf.get("/Main/Cache<JmemNum>", "10"));

    string sEnableErase = _tcConf.get("/Main/Cache<EnableErase>", "Y");
    _enableErase = (sEnableErase == "Y" || sEnableErase == "y") ? true : false;
    if (_enableErase)
    {
        _eraseInterval = TC_Common::strto<int>(_tcConf["/Main/Cache<EraseInterval>"]);
        _eraseRatio = TC_Common::strto<int>(_tcConf["/Main/Cache<EraseRadio>"]);
        _maxEraseCountOneTime = TC_Common::strto<unsigned int>(_tcConf.get("/Main/Cache<MaxEraseCountOneTime>", "500"));
        _eraseThreadCount = TC_Common::strto<unsigned int>(_tcConf.get("/Main/Cache<EraseThreadCount>", "2"));
        if (_eraseThreadCount > _shmNum)
        {
            _eraseThreadCount = _shmNum;
        }
    }
    TLOGDEBUG("EraseThread::init succ" << endl);
}

void EraseThread::reload()
{
    _tcConf.parseFile(_config);

    string sEnableErase = _tcConf.get("/Main/Cache<EnableErase>");
    if (!sEnableErase.empty())
    {
        _enableErase = (sEnableErase == "Y" || sEnableErase == "y") ? true : false;
    }
    if (_enableErase)
    {
        _eraseInterval = TC_Common::strto<int>(_tcConf["/Main/Cache<EraseInterval>"]);
        _eraseRatio = TC_Common::strto<int>(_tcConf["/Main/Cache<EraseRadio>"]);
        _maxEraseCountOneTime = TC_Common::strto<unsigned int>(_tcConf.get("/Main/Cache<MaxEraseCountOneTime>", "500"));
    }

    TLOGDEBUG("EraseThread::reload succ" << endl);
}

class Arg
{
public:
    EraseThread* pthis;
    unsigned int uThreadIndex;

};

void EraseThread::createThread()
{
    if (!_enableErase)
        return;
    //创建线程
    TLOGDEBUG("EraseThread::createThread enter." << endl);
    if (_isStart)
    {
        TLOGDEBUG("EraseThread::createThread has start before." << endl);
        return;
    }


    _isStart = true;
    setRuning(true);
    _activeJmemCount.set(_eraseThreadCount);
    pthread_t* pThread = new pthread_t[_eraseThreadCount];
    Arg* pArg = new Arg[_eraseThreadCount];
    for (unsigned int i = 0; i < _eraseThreadCount; ++i)
    {
        pArg[i].pthis = this;
        pArg[i].uThreadIndex = i;
        if (pthread_create(&pThread[i], NULL, EraseData, (void*)(pArg + i)) != 0)
        {
            throw runtime_error("Create EraseThread fail");
        }
        TLOGDEBUG("EraseThread::createThread create thread " << i << " succ." << endl);
    }
}

void* EraseThread::EraseData(void* pArg)
{
    Arg* p = (Arg*)(pArg);
    unsigned int uThreadIndex = p->uThreadIndex;

    EraseThread* pthis = p->pthis;

    while (pthis->isStart())
    {
        if (!(pthis->_enableErase))
        {
            sleep(60);
        }

        bool bDoErase = false;
        for (unsigned int uJmemIndex = 0; uJmemIndex < pthis->_shmNum; ++uJmemIndex)
        {
            if (uJmemIndex % (pthis->_eraseThreadCount) != uThreadIndex)
            {
                continue;
            }

            int iRatio = pthis->_eraseRatio;
            g_sHashMap.getUseRatio(uJmemIndex, iRatio);
            if (iRatio < pthis->_eraseRatio)
            {
                //no need to erase data.
                continue;
            }

            g_sHashMap.eraseByID(pthis->_eraseRatio, uJmemIndex, pthis->_maxEraseCountOneTime, true);
            bDoErase = true;
        }
        if (!bDoErase)
        {
            sleep(pthis->_eraseInterval);
        }
    }

    if (pthis->_activeJmemCount.dec() < 1)
    {
        TLOGDEBUG("EraseThread all thread finished." << endl);
        pthis->setRuning(false);
        pthis->setStart(false);
    }

    return NULL;
}




