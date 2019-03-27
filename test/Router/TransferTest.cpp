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
#include <gtest/gtest.h>
#include <unistd.h>
#include "Transfer.h"

namespace
{
constexpr int MinThreadPerGroup = 3;
constexpr int MaxThreadPerGroup = 5;
constexpr int SecondToSleep = 2;

int instantlyTransfer(const TransferInfo &, std::string &) { return 0; }

int continuouslyTransfer(const TransferInfo &, std::string &)
{
    ::sleep(SecondToSleep);
    return 0;
}

inline std::shared_ptr<TransferInfo> makeTask(const std::string &groupName)
{
    auto t = std::make_shared<TransferInfo>();
    t->groupName = groupName;
    return t;
}
}  // namespace

TEST(TransferDispatcher, sanity)
{
    std::shared_ptr<TransferDispatcher> td = std::make_shared<TransferDispatcher>(
        instantlyTransfer, 20, MinThreadPerGroup, MaxThreadPerGroup);
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group3"));

    EXPECT_EQ(td->getQueueTaskNum("group1"), 3);
    EXPECT_EQ(td->getQueueTaskNum("group2"), 2);
    EXPECT_EQ(td->getQueueTaskNum("group3"), 1);
    EXPECT_EQ(td->getQueueTaskNum("not in queue"), 0);
    EXPECT_EQ(td->getTotalQueueTaskNum(), 6);
    td->clearTasks();
    EXPECT_EQ(td->getQueueTaskNum("group1"), 0);
    EXPECT_EQ(td->getQueueTaskNum("group2"), 0);
    EXPECT_EQ(td->getQueueTaskNum("group3"), 0);
    EXPECT_EQ(td->getQueueTaskNum("not in queue"), 0);
    EXPECT_EQ(td->getTotalQueueTaskNum(), 0);
}

TEST(TransferDispatcher, threads_num_smaller_than_MIN)
{
    std::shared_ptr<TransferDispatcher> td = std::make_shared<TransferDispatcher>(
        continuouslyTransfer, 5, MinThreadPerGroup, MaxThreadPerGroup);

    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group2"));
    EXPECT_EQ(td->getTotalQueueTaskNum(), 6);
    td->doTransferTask();
    EXPECT_EQ(td->getTotalTransferingThreadNum(), 5);
    EXPECT_EQ(td->getTotalQueueTaskNum(), 1);
    td->terminate();
    EXPECT_EQ(td->getTotalTransferingThreadNum(), 0);
    EXPECT_EQ(td->getTransferingTaskNum("group1"), 0);
    EXPECT_EQ(td->getTransferingTaskNum("group2"), 0);
}

TEST(TransferDispatcher, threads_num_larger_than_MIN)
{
    std::shared_ptr<TransferDispatcher> td = std::make_shared<TransferDispatcher>(
        continuouslyTransfer, 7, MinThreadPerGroup, MaxThreadPerGroup);

    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group2"));
    EXPECT_EQ(td->getTotalQueueTaskNum(), 8);
    td->doTransferTask();
    EXPECT_EQ(td->getTotalTransferingThreadNum(), 7);
    EXPECT_EQ(td->getTotalQueueTaskNum(), 1);
    EXPECT_GE(td->getTransferingTaskNum("group1"), MinThreadPerGroup);
    EXPECT_LT(td->getTransferingTaskNum("group1"), MaxThreadPerGroup);
    EXPECT_GE(td->getTransferingTaskNum("group2"), MinThreadPerGroup);
    EXPECT_LT(td->getTransferingTaskNum("group2"), MaxThreadPerGroup);
    td->terminate();
    EXPECT_EQ(td->getTotalTransferingThreadNum(), 0);
    EXPECT_EQ(td->getTransferingTaskNum("group1"), 0);
    EXPECT_EQ(td->getTransferingTaskNum("group2"), 0);
}

TEST(TransferDispatcher, threads_num_larger_than_MAX)
{
    std::shared_ptr<TransferDispatcher> td = std::make_shared<TransferDispatcher>(
        continuouslyTransfer, 20, MinThreadPerGroup, MaxThreadPerGroup);

    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group2"));
    EXPECT_EQ(td->getTotalQueueTaskNum(), 8);
    td->doTransferTask();
    EXPECT_EQ(td->getTotalTransferingThreadNum(), 8);
    EXPECT_EQ(td->getTransferingTaskNum("group1"), 4);
    EXPECT_EQ(td->getTransferingTaskNum("group2"), 4);
    EXPECT_EQ(td->getTotalQueueTaskNum(), 0);
    td->terminate();
    EXPECT_EQ(td->getTotalTransferingThreadNum(), 0);
    EXPECT_EQ(td->getTransferingTaskNum("group1"), 0);
    EXPECT_EQ(td->getTransferingTaskNum("group2"), 0);
}

TEST(TransferDispatcher, group_num_larger_than_thread_num)
{
    std::shared_ptr<TransferDispatcher> td = std::make_shared<TransferDispatcher>(
        continuouslyTransfer, 2, MinThreadPerGroup, MaxThreadPerGroup);
    td->addTransferTask(makeTask("group1"));
    td->addTransferTask(makeTask("group2"));
    td->addTransferTask(makeTask("group3"));
    EXPECT_EQ(td->getTotalQueueTaskNum(), 3);
    td->doTransferTask();
    EXPECT_EQ(td->getTotalTransferingThreadNum(), 2);
    EXPECT_EQ(td->getTotalQueueTaskNum(), 1);
    td->terminate();
    EXPECT_EQ(td->getTotalTransferingThreadNum(), 0);
    EXPECT_EQ(td->getTransferingTaskNum("group1"), 0);
    EXPECT_EQ(td->getTransferingTaskNum("group2"), 0);
    EXPECT_EQ(td->getTransferingTaskNum("group3"), 0);
}
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}