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
#include <stdlib.h>

#include "UnpackTable.h"
#include "Router.h"
#include "RouterShare.h"

using namespace DCache;
using namespace tars;
using namespace std;

typedef UnpackTable RouterTable;

namespace
{                                      

/******手动构造模块module1的路由表数据******/
// TODO(dengyouwang):使路由表数据更加复杂，尽量覆盖更多场景
PackTable getPackTable1()
{
    PackTable packTable1; // 模块module1对应的未压缩的路由表数据

    // 模块信息
    // struct ModuleInfo
    // {
    //     // 业务模块编号
    //     1 require int id;
    //     // 模块名，各模块的模块名不能重复
    //     2 require string moduleName;
    //     // 版本号
    //     3 require int version;
    //     //自动切换类型 0切读写 1切读 2不自动切换 3表示无效模块
    //     4 require int switch_status;
    // };
    packTable1.info.id = 1;
    packTable1.info.moduleName = "module1";
    packTable1.info.version = 17359;  // 路由表数据的版本号
    packTable1.info.switch_status = 0;

    // 模块所包含的所有服务器信息
    // struct ServerInfo
    // {
    //     // 服务器编号
    //     1 require int id;
    //     
    //     2 require string serverName;
    //     // 服务器IP
    //     3 require string ip;

    //     // BinLogServant对象名
    //     4 require string BinLogServant;
    //     // CacheServant对象名
    //     5 require string CacheServant;
    //      // CacheServant对象名
    //     6 require string WCacheServant;
    //     // RouteClientServant对象名
    //     7 require string RouteClientServant;

    //     // 服务器所在IDC
    //     8 require string idc;
    //     // 服务状态：主/备
    //     9 require string ServerStatus;
    //     // 服务所属模块
    //     10 require string moduleName;
    //     // 模块名
    //     11 optional string groupName;
    //     // 服务状态
    //     12 optional int status;
    // };
    ServerInfo server1;
    server1.id = 1;
    server1.moduleName = "module1";
    server1.groupName = "group1"; 
    server1.serverName = "m1_g1_s1"; 
    server1.idc = "SZ"; 
    server1.ServerStatus = "M"; 
    server1.status = 0;

    ServerInfo server2;
    server2.id = 2;
    server2.moduleName = "module1";
    server2.groupName = "group1"; 
    server2.serverName = "m1_g1_s2"; 
    server2.idc = "SZ";
    server2.ServerStatus = "S";

    ServerInfo server3;
    server3.id = 3;
    server3.moduleName = "module1";
    server3.groupName = "group1"; 
    server3.serverName = "m1_g1_s3"; 
    server3.idc = "SH";
    server3.ServerStatus = "M"; // 镜像的主机器，注意区分

    ServerInfo server4;
    server4.id = 4;
    server4.moduleName = "module1";
    server4.groupName = "group2"; 
    server4.serverName = "m1_g2_s1"; 
    server4.idc = "SZ"; 
    server4.ServerStatus = "M";

    ServerInfo server5;
    server5.id = 5;
    server5.moduleName = "module1";
    server5.groupName = "group2"; 
    server5.serverName = "m1_g2_s2"; 
    server5.idc = "SZ"; 
    server5.ServerStatus = "S";

    packTable1.serverList[server1.serverName] = server1;
    packTable1.serverList[server2.serverName] = server2;
    packTable1.serverList[server3.serverName] = server3;
    packTable1.serverList[server4.serverName] = server4;
    packTable1.serverList[server5.serverName] = server5;

    // 模块所包含的组信息
    // struct GroupInfo
    // {
    //     1 require int id;
    //     // 所属业务模块名
    //     2 require string moduleName;
    //     // 服务器组名
    //     3 require string groupName;
    //     // Master服务名
    //     4 require string masterServer; // 该组的主服务器的服务名，即ServerInfo.serverName
    //     //该组的proxy的访问权限，0标识读写，1标识只读,2镜像不可用
    //     5 require int accessStatus;

    //     // 按IDC分类服务器集合,vector顺序按优先级顺序排列  // 所谓优先级即master的机器位于列表第一位，slave机器位于列表第二位，通常这个vector的元素数量最多不超过2
    //     6 require map<string, vector<string>> idcList;
    //     // 备份对应关系
    //     7 require map<string, string> bakList;
    // };
    GroupInfo groupInfo1;
    groupInfo1.id = 1;
    groupInfo1.moduleName = "module1";
    groupInfo1.groupName = "group1";
    groupInfo1.masterServer = server1.serverName; // 该组的主服务器名字
    groupInfo1.accessStatus = 0;
    groupInfo1.idcList["SZ"] = vector<string>({"m1_g1_s1", "m1_g1_s2"});
    groupInfo1.idcList["SH"] = vector<string>({"m1_g1_s3"});

    GroupInfo groupInfo2;
    groupInfo2.id = 2;
    groupInfo2.moduleName = "module1";
    groupInfo2.groupName = "group2";
    groupInfo2.masterServer = server4.serverName; // 该组的主服务器名字
    groupInfo2.accessStatus = 0;
    groupInfo2.idcList["SZ"] = vector<string>({"m1_g2_s1", "m1_g2_s2"});

    packTable1.groupList[groupInfo1.groupName] = groupInfo1;
    packTable1.groupList[groupInfo2.groupName] = groupInfo2;
    

    // 路由记录
    // struct RecordInfo
    // {
    //     // 路由编号
    //     1 require int id;
    //     // 路由所属业务模块名
    //     2 require string moduleName;
    //     // 开始页
    //     3 require int fromPageNo;
    //     // 结束页
    //     4 require int toPageNo;
    //     // 当前的工作服务器组名
    //     5 require string groupName;
    //     // 当前记录是否在迁移
    //     6 require bool transfering;
    //     //如果正在迁移，迁移的目的服务器组编号
    //     7 require string transGroupName;
    // };
    RecordInfo recordInfo1;
    recordInfo1.id = 1;
    recordInfo1.moduleName = "module1";
    recordInfo1.fromPageNo = 0;
    recordInfo1.toPageNo = 240000 + 1000; // 其中240001-241000设置成正在迁移的页(通过结构体TransferInfo设置)
    recordInfo1.groupName = "group1";
    // recordInfo1.transfering = false;
    // recordInfo1.transGroupName = "";

    RecordInfo recordInfo2;
    recordInfo2.id = 2;
    recordInfo2.moduleName = "module1";
    recordInfo2.fromPageNo = 241001;
    recordInfo2.toPageNo = 429496;
    recordInfo2.groupName = "group2";
    // recordInfo2.transfering = false;
    // recordInfo2.transGroupName = "";

    packTable1.recordList.push_back(recordInfo1);
    packTable1.recordList.push_back(recordInfo2);

    return packTable1;
}

// 获取随机字符串
string getRandString()
{
    const string chars =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    srand(time(0));
    
    string randString = "";
    size_t randLen = rand() % 100;
    for(size_t i = 0; i < randLen; i++)
    {
        randString.append(1, chars[rand() % chars.size()]);
    }
    return randString;
}

} // namespace

// 路由表初始化测试
TEST(RouterTableTest, init)
{
    RouterTable rt;
    ASSERT_EQ(RouterTable::RET_SUCC, rt.init(getPackTable1(), ""));

    ASSERT_EQ(17359, rt.getVersion());
    /*
    string key = "key_1";
    ServerInfo serverInfo;
    rt.getMaster(key, serverInfo)
    */

    // EXPECT_EQ(proxy.setKV(req), 0) << "setkv err, key:" << data.keyItem << endl;
}


// 测试获取路由表中所有group的主服务器信息
TEST(RouterTableTest, getAllMasters)
{
    RouterTable rt;
    rt.init(getPackTable1(), "");

    vector<ServerInfo> masterServers;
    ASSERT_EQ(RouterTable::RET_SUCC, rt.getAllMasters(masterServers));

    ASSERT_EQ(2, masterServers.size()); // 因为构造的路由表数据有2个组，所以有此断言

    ServerInfo &serverInfo = masterServers[0];
    ASSERT_EQ("module1", serverInfo.moduleName);
    ASSERT_EQ("group1", serverInfo.groupName);
    ASSERT_EQ("m1_g1_s1", serverInfo.serverName);
    ASSERT_EQ("SZ", serverInfo.idc);

}

// getAllIdcServer这个接口实现和函数名词含义相差甚远，暂时不测
// TEST(RouterTableTest, getAllIdcServer)
// {
//     RouterTable rt;
//     rt.init(getPackTable1(), "");

//     vector<ServerInfo> servers;
//     EXPECT_EQ(RouterTable::RET_SUCC, rt.getAllIdcServer("SZ", servers));
//     EXPECT_EQ(4, servers.size());

//     servers.clear();
//     ASSERT_EQ(RouterTable::RET_SUCC, rt.getAllIdcServer("SH", servers));
//     ASSERT_EQ(1, servers.size());

//     ServerInfo &serverInfo = servers[0];
//     ASSERT_EQ("module1", serverInfo.moduleName);
//     ASSERT_EQ("group1", serverInfo.groupName);
//     ASSERT_EQ("m1_g1_s3", serverInfo.serverName);
//     ASSERT_EQ("SH", serverInfo.idc);
// }


TEST(RouterTableTest, getGroup)
{
    RouterTable rt;
    rt.init(getPackTable1(), "");

    GroupInfo group;
    ASSERT_EQ(RouterTable::RET_SUCC, rt.getGroup("m1_g1_s3", group));

    ASSERT_EQ(1, group.id);
    ASSERT_EQ("module1", group.moduleName);
    ASSERT_EQ("group1", group.groupName);
    ASSERT_EQ("m1_g1_s1", group.masterServer);
    ASSERT_EQ(0, group.accessStatus);
}


TEST(RouterTableTest, getTrans)
{
    uint32_t pageSize = 10000;
    uint64_t reloadTime = 10000;	// 默认路由加载间隔时间(us)

    // 设置正在迁移的路由页
    vector<TransferInfo> transferInfos;
    TransferInfo info1;
    info1.id = 1;
    info1.moduleName = "module1";
    info1.fromPageNo = 240001;
    info1.toPageNo = 241000;
    info1.groupName = "group1";
    info1.transGroupName = "group2";
    transferInfos.push_back(info1);

    RouterTable rt;
    rt.init(getPackTable1(), "", pageSize, reloadTime, transferInfos);

    ServerInfo srcServer;
    ServerInfo destServer;
    uint32_t pageNo = rand() % 240001;
    EXPECT_EQ(RouterTable::RET_NOT_TRANSFERING, rt.getTrans(pageNo, srcServer, destServer)) << "pageNo = " << pageNo << " was transfering, it should not be!"; // 该页未在迁移
    pageNo = 240001 + rand() % 1000;
    EXPECT_EQ(RouterTable::RET_SUCC, rt.getTrans(pageNo, srcServer, destServer)) << "pageNo = " << pageNo << " was not transfering, it should be!";  // 该页正在迁移
    pageNo = 241001 + rand() % 188496;
    EXPECT_EQ(RouterTable::RET_NOT_TRANSFERING, rt.getTrans(pageNo, srcServer, destServer)) << "pageNo = " << pageNo << " was transfering, it should not be!";   // 该页未在迁移
}


TEST(RouterTableTest, getMaster)
{
    RouterTable rt;  
    rt.init(getPackTable1(), "");

    ServerInfo serverInfo;
    string randStr = getRandString();
    EXPECT_EQ(RouterTable::RET_SUCC, rt.getMaster(randStr, serverInfo)) << "getMaster error from key = " << randStr;
}

// template<typename T> int getIdcServer(T key, const string &sIdc, bool bSlaveReadAble, ServerInfo &serverInfo)
// do not test when bSlaveReadAble == true, because it is deprecated!   
TEST(RouterTableTest, getIdcServer)
{
    RouterTable rt;  
    rt.init(getPackTable1(), "");

    ServerInfo serverInfo;
    const string KEY4TEST = "Key_For_Test"; // 这个key由group1服务！！！

    EXPECT_EQ(RouterTable::RET_SUCC, rt.getIdcServer(KEY4TEST, "SZ", false, serverInfo)) << "getIdcServer error from key = " << KEY4TEST;
    EXPECT_EQ(1, serverInfo.id);

    // EXPECT_EQ(RouterTable::RET_SUCC, rt.getIdcServer(KEY4TEST, "SZ", true, serverInfo)) << "getIdcServer error from key = " << KEY4TEST;
    // EXPECT_EQ(2, serverInfo.id);
    
    EXPECT_EQ(RouterTable::RET_SUCC, rt.getIdcServer(KEY4TEST, "SH", false, serverInfo)) << "getIdcServer error from key = " << KEY4TEST;
    EXPECT_EQ(3, serverInfo.id);

    // EXPECT_EQ(RouterTable::RET_SUCC, rt.getIdcServer(KEY4TEST, "SH", true, serverInfo)) << "getIdcServer error from key = " << KEY4TEST;
    // EXPECT_EQ(3, serverInfo.id);

    EXPECT_EQ(RouterTable::RET_SUCC, rt.getIdcServer(KEY4TEST, "UnknownIDC", false, serverInfo)) << "getIdcServer error from key = " << KEY4TEST;
    EXPECT_EQ(1, serverInfo.id);

    // EXPECT_EQ(RouterTable::RET_SUCC, rt.getIdcServer(KEY4TEST, "UnknownIDC", true, serverInfo)) << "getIdcServer error from key = " << KEY4TEST;
    // EXPECT_EQ(2, serverInfo.id);
}

// 路由页测试
TEST(EntryTest, all)
{
    RouterTable::Entry entry;
    entry.value = 0; // memset better?

    // 当前页是否属于本机服务范围
    entry.setSelf(true);
    EXPECT_TRUE(entry.getSelf());
    entry.setSelf(false);
    EXPECT_FALSE(entry.getSelf());

    // 当前页是否正在迁移
    entry.setTransfering(true);
    EXPECT_TRUE(entry.getTransfering());
    entry.setTransfering(false);
    EXPECT_FALSE(entry.getTransfering());

    // 设置当前页所在服务器ID
    entry.setIndex(0x00000027);
    EXPECT_EQ(entry.getIndex(), 39);

    // 设置当前页正在迁移的目的服务器ID
    entry.setTransIndex(0x00000072);
    EXPECT_EQ(entry.getTransIndex(), 114);

    EXPECT_EQ(entry.getIndex(), 39);
    EXPECT_FALSE(entry.getTransfering());
    EXPECT_FALSE(entry.getSelf());
    
}



int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}