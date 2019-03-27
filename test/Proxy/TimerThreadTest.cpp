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
//#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#include "util/tc_timeprovider.h"
#include "TimerThread.h"

using namespace std;


namespace
{
    const time_t NOW_TIME = TC_TimeProvider::getInstance()->getNow();

    // 定时线程的配置信息
    const string CFG_FOR_TIMER_THREAD = 
    R"(<Main>
        SynRouterTableInterval = 1 #同步路由表的时间间隔（秒）
        SynRouterTableFactoryInterval = 5 #同步模块变化（模块新增或者下线）的时间间隔（秒）
    </Main>)";

    const string NEWCFG_FOR_TIMER_THREAD = 
   R"(<Main>
        SynRouterTableInterval = 2 #同步路由表的时间间隔（秒）
        SynRouterTableFactoryInterval = 10 #同步模块变化（模块新增或者下线）的时间间隔（秒）
    </Main>)";
    
} //namaspace



class TimerThreadTest : public ::testing::Test 
{
protected:
    void SetUp() override
    {
        _pTimerThread = make_shared<TimerThread>(NOW_TIME);
        EXPECT_FALSE(_pTimerThread->checkStop());
        EXPECT_EQ(NOW_TIME, _pTimerThread->getLastSynRouterTime());
        EXPECT_EQ(NOW_TIME, _pTimerThread->getLastSynRouterTableFactoryTime());

        TC_Config conf;
        conf.parseString(CFG_FOR_TIMER_THREAD);
        _pTimerThread->init(conf);
        EXPECT_EQ(1, _pTimerThread->getIntervalToSyncRouterTable());
        EXPECT_EQ(5, _pTimerThread->getIntervalToSyncRouterTableFactory());        
    }

protected:
    shared_ptr<TimerThread> _pTimerThread;
};

TEST_F(TimerThreadTest, NOTHING)
{
    EXPECT_EQ(0,0);
}


TEST_F(TimerThreadTest, reloadConf)
{   
    TC_Config newConf;
    newConf.parseString(NEWCFG_FOR_TIMER_THREAD);
    _pTimerThread->reloadConf(newConf);
    EXPECT_EQ(2, _pTimerThread->getIntervalToSyncRouterTable());
    EXPECT_EQ(10, _pTimerThread->getIntervalToSyncRouterTableFactory());
}


TEST_F(TimerThreadTest, showStatus)
{   
    EXPECT_NE(string(""), _pTimerThread->showStatus()) << "you should print sth but not empty string when invoking TimerThread::showStatus";
}


TEST_F(TimerThreadTest, stopThread)
{   
    _pTimerThread->stop();
    EXPECT_TRUE(_pTimerThread->checkStop());
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


