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
#include "TimerThread.h"
#include "DbHandle.h"
#include "OuterProxyFactory.h"
#include "RouterServer.h"
#include "StatProperty.h"
#include "Transfer.h"
//#include "util/tc_functor.h"

using namespace std;

extern RouterServer g_app;

TimerThread::TimerThread()
    : _stop(false),
      _transferInterval(3),
      _clearProxyInterval(1800),
      _threadPollSize(0),
      _minSizeEachGroup(0),
      _maxSizeEachGroup(0),
      _lastTransferTime(0),
      _lastClearProxyTime(0)
{
}

void TimerThread::init(const RouterServerConfig &conf,
                       std::shared_ptr<DbHandle> dbHandle,
                       std::shared_ptr<OuterProxyFactory> outerProxy)
{
    try
    {
        _outerProxy = outerProxy;
        _stop = false;
        _lastTransferTime = _lastClearProxyTime = 0;

        _transferInterval = conf.getTransferInterval(3);
        _clearProxyInterval = conf.getClearProxyInterval(1800);
        _threadPollSize = conf.getTimerThreadSize(3);
        _minSizeEachGroup = conf.getMinTransferThreadEachGroup(5);
        _maxSizeEachGroup = conf.getMaxTransferThreadEachGroup(8);
        _dbHandle = dbHandle;
        FDLOG("TimeThread") << __LINE__ << "|" << __FUNCTION__ << "|"
                            << " thread pool size init: " << _threadPollSize << endl;

        _transferDispatcher =
            std::make_shared<TransferDispatcher>(std::bind(&DCache::Transfer::doTransfer,
                                                           g_app.getTransfer(),
                                                           std::placeholders::_1,
                                                           std::placeholders::_2),
                                                 _threadPollSize,
                                                 _minSizeEachGroup,
                                                 _maxSizeEachGroup);
    }
    catch (TC_Config_Exception &e)
    {
        cerr << "catch config exception: " << e.what() << endl;
        exit(-1);
    }
    catch (TC_ConfigNoParam_Exception &e)
    {
        cerr << "get domain error: " << e.what() << endl;
        exit(-1);
    }
    catch (TarsException &e)
    {
        cerr << "Tars exception:" << e.what() << endl;
        exit(-1);
    }
    catch (...)
    {
        cerr << "unkown error" << endl;
        exit(-1);
    }

    TLOGDEBUG("init TimerThread succ!" << endl);
}

void TimerThread::run()
{
    enum RouterType lastType = g_app.getRouterType();

    while (!_stop)
    {
        if (g_app.getRouterType() != ROUTER_MASTER)
        {
            if (lastType == ROUTER_MASTER)
            {  // 从主机降级而来的，要做降级处理
                downgrade();
                lastType = ROUTER_SLAVE;
            }

            TLOGDEBUG(FILE_FUN << "this is slave, just sleep" << endl);

            {
                TC_ThreadLock::Lock lock(*this);
                timedWait(3000);
            }
            continue;
        }

        lastType = g_app.getRouterType();

        time_t now = TC_TimeProvider::getInstance()->getNow();
        if (now - _lastTransferTime >= _transferInterval)
        {
            doTransferParallel();
            _lastTransferTime = TC_TimeProvider::getInstance()->getNow();
        }

        if (now - _lastClearProxyTime >= _clearProxyInterval)
        {
            doClearProxy();
            _lastClearProxyTime = TC_TimeProvider::getInstance()->getNow();
        }

        {
            TC_ThreadLock::Lock lock(*this);
            timedWait(1000);
        }
    }
}

void TimerThread::terminate()
{
    _transferDispatcher->terminate();
    
	TC_ThreadLock::Lock sync(*this);
    _stop = true;
    notifyAll();
}

void TimerThread::doClearProxy() { _outerProxy->clearProxy(); }

// 设置每个组分配的最小迁移线程数 _minSizeEachGroup 和最大迁移线程数 _maxSizeEachGroup；
// 获取迁移任务，根据组名添加进 _transferGroup；
// 有空闲工作线程时，每模块先分配最多 _minSizeEachGroup个迁移线程，
// 若还有空闲工作线程， 分配给剩下的有迁移任务的组，上限为_maxSizeEachGroup
void TimerThread::doTransferParallel()
{
	TransferInfo transInfo;

    //获取迁移任务
    while (_dbHandle->getTransferTask(transInfo) == 0)
    {
		auto transTask = std::make_shared<TransferInfo>(transInfo);

        _transferDispatcher->addTransferTask(transTask);
    }

    _transferDispatcher->doTransferTask();
}

void TimerThread::downgrade()
{
    assert(g_app.getRouterType() == ROUTER_SLAVE);
    _transferDispatcher->clearTasks();
    _lastTransferTime = _lastClearProxyTime = 0;
}