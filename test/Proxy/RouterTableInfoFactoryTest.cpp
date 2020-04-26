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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "RouterTableInfoFactory.h"
#include "MockClass/MockRouterHandle.h"
#include "ProxyShare.h"

using namespace std;

using ::testing::_;
using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::InSequence;

namespace
{
const string CFG_FOR_ROUTERTABLEINFOFACTORY =
    R"(<Main>
            BaseLocalRouterFile = RouterTableDataPrefix  #保存在本地的路由表文件名前缀
            RouterTableMaxUpdateFrequency = 3 #模块路由表每秒钟的最大更新频率
        </Main>)";

const string NEW_CFG_FOR_ROUTERTABLEINFOFACTORY =
    R"(<Main>
            BaseLocalRouterFile = Router.dat  #保存在本地的路由表文件名前缀
            RouterTableMaxUpdateFrequency = 7 #模块路由表每秒钟的最大更新频率
        </Main>)";

} //namespace

class RouterTableInfoFactoryTest : public ::testing::Test
{
  protected:
    virtual void SetUp() override
    {
        MockRouterHandle *pMock = new MockRouterHandle;
        FakeRouterHandle1 *pFake1 = new FakeRouterHandle1;
        ON_CALL(*pMock, getModuleList(_)).WillByDefault(Invoke(pFake1, &FakeRouterHandle1::getModuleList));
        EXPECT_CALL(*pMock, getModuleList(_)).Times(AtLeast(1));
        this->init(pMock);

    }

    void init(MockRouterHandle *pRouterHandle)
    {
        _pRouterTableInfoFactory = make_shared<RouterTableInfoFactory>(pRouterHandle);
        TC_Config conf;
        conf.parseString(CFG_FOR_ROUTERTABLEINFOFACTORY);
        EXPECT_EQ(DCache::ET_SUCC, _pRouterTableInfoFactory->init(conf));
    }

  protected:
    shared_ptr<RouterTableInfoFactory> _pRouterTableInfoFactory;
};


TEST_F(RouterTableInfoFactoryTest, reloadConf)
{
    TC_Config conf;
    conf.parseString(NEW_CFG_FOR_ROUTERTABLEINFOFACTORY);
    EXPECT_EQ(string(""), _pRouterTableInfoFactory->reloadConf(conf));
}

TEST_F(RouterTableInfoFactoryTest, showStatus)
{

    MockRouterHandle *pMock = new MockRouterHandle;
    FakeRouterHandle1 *pFake1 = new FakeRouterHandle1;
    ON_CALL(*pMock, getModuleList(_)).WillByDefault(Invoke(pFake1, &FakeRouterHandle1::getModuleList));

    init(pMock);

    EXPECT_NE(string(""), _pRouterTableInfoFactory->showStatus()) << "you should print sth but not empty string when invoking RouterTableInfoFactory::showStatus";
}

TEST_F(RouterTableInfoFactoryTest, supportModule)
{
    EXPECT_TRUE(_pRouterTableInfoFactory->supportModule(MODULE1));
    EXPECT_TRUE(_pRouterTableInfoFactory->supportModule(MODULE2));
}

TEST_F(RouterTableInfoFactoryTest, isNeedUpdateFactory)
{
    // 新增模块
    EXPECT_TRUE(_pRouterTableInfoFactory->isNeedUpdateFactory(SUPPORT_MODULES_AFTER_ADD_NEW_MODULE));

    // 卸载模块
    EXPECT_TRUE(_pRouterTableInfoFactory->isNeedUpdateFactory(SUPPORT_MODULES_AFTER_UPLOAD_MODULE));

    // 模块顺序打乱
    EXPECT_FALSE(_pRouterTableInfoFactory->isNeedUpdateFactory({MODULE2, MODULE1}));
}





TEST_F(RouterTableInfoFactoryTest, checkModuleChange)
{

    auto getModuleList1 = [] (vector<string> &moduleList) -> int
    {
        moduleList = SUPPORT_MODULES;
        return 0;
    };

    auto getModuleList2 = [] (vector<string> &moduleList) -> int
    {
        moduleList = SUPPORT_MODULES_AFTER_ADD_NEW_MODULE;
        return 0;
    };

    auto getModuleList3 = [] (vector<string> &moduleList) -> int
    {
        moduleList = SUPPORT_MODULES_AFTER_UPLOAD_MODULE;
        return 0;
    };



    MockRouterHandle *pMock = new MockRouterHandle;

    // 期待函数getModuleList在_pRouterTableInfoFactory->checkModuleChange()运行时被调用两次，每次有不同的实现
    EXPECT_CALL(*pMock, getModuleList(_))
        .Times(2)
        .WillOnce(Invoke(getModuleList1))
        .WillOnce(Invoke(getModuleList2));


    // vector<string> modules;
    // pMock->getModuleList(modules);
    // cout << modules.size() << endl;
    // modules.clear();
    // pMock->getModuleList(modules);
    // cout << modules.size() << endl;

    init(pMock);
    
    // 新增模块： "module3"
    _pRouterTableInfoFactory->checkModuleChange();
    EXPECT_TRUE(_pRouterTableInfoFactory->supportModule(MODULE1));
    EXPECT_TRUE(_pRouterTableInfoFactory->supportModule(MODULE2));
    EXPECT_TRUE(_pRouterTableInfoFactory->supportModule(NEW_MODULE));

    EXPECT_CALL(*pMock, getModuleList(_))
        .Times(1)
        .WillOnce(Invoke(getModuleList3));

    // 下线模块："module2" 和 "module3";  
    _pRouterTableInfoFactory->checkModuleChange();
    EXPECT_TRUE(_pRouterTableInfoFactory->supportModule(MODULE1));
    EXPECT_FALSE(_pRouterTableInfoFactory->supportModule(MODULE2));
    EXPECT_FALSE(_pRouterTableInfoFactory->supportModule(NEW_MODULE));

}



TEST_F(RouterTableInfoFactoryTest, synRouterTableInfo)
{

    MockRouterHandle *pMock = new MockRouterHandle;
    FakeRouterHandle1 *pFake1 = new FakeRouterHandle1;
    ON_CALL(*pMock, getModuleList(_)).WillByDefault(Invoke(pFake1, &FakeRouterHandle1::getModuleList));

    init(pMock);

    EXPECT_NE(string(""), _pRouterTableInfoFactory->showStatus()) << "you should print sth but not empty string when invoking RouterTableInfoFactory::showStatus";
}



int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}