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
#include "MockRouterHandle.h"

using namespace std;

using ::testing::_;
using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::InSequence;

namespace
{

} //namespace

TEST(RouterTableInfoTest,  showStatus)
{
    RouterTableInfo::setMaxUpdateFrequency(6);
    EXPECT_NE(string(""), RouterTableInfo::showStatus());
}


TEST(RouterTableInfoTest,  initRouterTable)
{   
    RouterHandle *pMock = new MockRouterHandle;
    RouterTableInfo routerTableInfo(MODULE1, "Path2StoreRouterData", shared_ptr<RouterHandle>(pMock));
    EXPECT_EQ(0, routerTableInfo.initRouterTable());
} 



TEST(RouterTableInfoTest,  update)
{   
    // lambda大法好
    auto getRouterInfo1 = [] (const string &moduleName, PackTable &packTable, bool needRetry = false) -> int
    {
        packTable.info.version = 0;
        return 0;
    };

    auto getRouterInfo2 = [] (const string &moduleName, PackTable &packTable, bool needRetry = false) -> int
    {
        packTable.info.version = 1;
        return 0;
    };

    auto getRouterVersion1 = [] (const string &moduleName, int &version, bool needRetry = false) -> int
    {
        version = 1;
        return 0;
    };

    MockRouterHandle *pMock = new MockRouterHandle;
    EXPECT_CALL(*pMock, getRouterInfo(_, _, _))
        .Times(2)
        .WillOnce(Invoke(getRouterInfo1))
        .WillOnce(Invoke(getRouterInfo2));

    EXPECT_CALL(*pMock, getRouterVersion(_, _, _))
        .Times(1)
        .WillOnce(Invoke(getRouterVersion1));

    RouterTableInfo routerTableInfo(MODULE1, "Path2StoreRouterData", shared_ptr<RouterHandle>(pMock));
    EXPECT_EQ(0, routerTableInfo.initRouterTable());
    EXPECT_EQ(ET_SUCC, routerTableInfo.update());

}


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}