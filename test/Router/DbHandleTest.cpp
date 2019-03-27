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
#include "DbHandle.h"
#include "MockDbHandle.h"
#include "MockRouterServerConfig.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

class DbHandleTest : public ::testing::Test
{
protected:
    void prepareInitData()
    {
        _sqlData.clear();
        _sqlData.resize(7);
        // 路由信息
        std::map<std::string, std::string> item1 = {{"id", "1"},
                                                    {"module_name", "module1"},
                                                    {"from_page_no", "0"},
                                                    {"to_page_no", "2000"},
                                                    {"group_name", "group1"}};
        std::map<std::string, std::string> item2 = {{"id", "2"},
                                                    {"module_name", "module1"},
                                                    {"from_page_no", "2001"},
                                                    {"to_page_no", "429496"},
                                                    {"group_name", "group2"}};
        std::map<std::string, std::string> item3 = {{"id", "3"},
                                                    {"module_name", "module1"},
                                                    {"from_page_no", "2001"},
                                                    {"to_page_no", "429496"},
                                                    {"group_name", "group2"}};
        std::map<std::string, std::string> item4 = {{"id", "4"},
                                                    {"module_name", "module1"},
                                                    {"from_page_no", "2049"},
                                                    {"to_page_no", "429496"},
                                                    {"group_name", "group2"}};
        std::map<std::string, std::string> item5 = {{"id", "5"},
                                                    {"module_name", "module2"},
                                                    {"from_page_no", "0"},
                                                    {"to_page_no", "429496"},
                                                    {"group_name", "group1"}};
        // 组信息第一部分
        std::map<std::string, std::string> item6 = {
            {"module_name", "module1"}, {"group_name", "group1"}, {"access_status", "0"}};
        std::map<std::string, std::string> item7 = {
            {"module_name", "module1"}, {"group_name", "group2"}, {"access_status", "0"}};
        std::map<std::string, std::string> item8 = {
            {"module_name", "module2"}, {"group_name", "group1"}, {"access_status", "0"}};
        // 组信息第二部分
        std::map<std::string, std::string> item9 = {{"server_status", "M"},
                                                    {"server_name", "DCacheTest"},
                                                    {"source_server_name", "source_DCacheTest"},
                                                    {"idc_area", "SZ"}};
        std::map<std::string, std::string> item10 = {{"server_status", "M"},
                                                     {"server_name", "DCacheTest"},
                                                     {"source_server_name", "source_DCacheTest"},
                                                     {"idc_area", "SZ"}};
        std::map<std::string, std::string> item11 = {{"server_status", "M"},
                                                     {"server_name", "DCacheTest"},
                                                     {"source_server_name", "source_DCacheTest"},
                                                     {"idc_area", "SZ"}};

        std::map<std::string, std::string> item12 = {{"server_name", "DCacheTest"},
                                                     {"id", "1"},
                                                     {"ip", "111.111.111.111"},
                                                     {"idc_area", "SZ"},
                                                     {"server_status", "M"},
                                                     {"module_name", "module1"},
                                                     {"group_name", "group1"},
                                                     {"status", "1"}};
        std::map<std::string, std::string> item13 = {{"server_name", "DCacheTest"},
                                                     {"id", "1"},
                                                     {"ip", "111.111.111.111"},
                                                     {"idc_area", "SZ"},
                                                     {"server_status", "M"},
                                                     {"module_name", "module1"},
                                                     {"group_name", "group2"},
                                                     {"status", "1"}};
        std::map<std::string, std::string> item14 = {{"server_name", "DCacheTest2"},
                                                     {"id", "1"},
                                                     {"ip", "111.111.111.111"},
                                                     {"idc_area", "SZ"},
                                                     {"server_status", "M"},
                                                     {"module_name", "module2"},
                                                     {"group_name", "group1"},
                                                     {"status", "1"}};

        std::map<std::string, std::string> item15 = {
            {"id", "1"}, {"module_name", "module1"}, {"version", "10"}, {"switch_status", "0"}};
        std::map<std::string, std::string> item16 = {
            {"id", "2"}, {"module_name", "module2"}, {"version", "12"}, {"switch_status", "0"}};

        _sqlData[0].data().push_back(item1);
        _sqlData[0].data().push_back(item2);
        _sqlData[0].data().push_back(item3);
        _sqlData[0].data().push_back(item4);
        _sqlData[0].data().push_back(item5);
        _sqlData[1].data().push_back(item6);
        _sqlData[1].data().push_back(item7);
        _sqlData[1].data().push_back(item8);
        _sqlData[2].data().push_back(item9);
        _sqlData[3].data().push_back(item10);
        _sqlData[4].data().push_back(item11);
        _sqlData[5].data().push_back(item12);
        _sqlData[5].data().push_back(item13);
        _sqlData[5].data().push_back(item14);
        _sqlData[6].data().push_back(item15);
        _sqlData[6].data().push_back(item16);
    }

    void prepareReloadData()
    {
        std::map<std::string, std::string> item1 = {{"id", "1"},
                                                    {"module_name", "module1"},
                                                    {"from_page_no", "0"},
                                                    {"to_page_no", "2000"},
                                                    {"group_name", "group1"}};
        std::map<std::string, std::string> item2 = {{"id", "4"},
                                                    {"module_name", "module1"},
                                                    {"from_page_no", "2001"},
                                                    {"to_page_no", "429496"},
                                                    {"group_name", "group2"}};
        tars::TC_Mysql::MysqlData data1;  // reloadRouter(moduleName) -> getDBRecordList(moduleName)
        data1.data().push_back(item1);
        data1.data().push_back(item2);
        _sqlData.push_back(data1);

        std::map<std::string, std::string> item3 = {
            {"id", "1"}, {"module_name", "module1"}, {"version", "10"}, {"switch_status", "0"}};
        tars::TC_Mysql::MysqlData data2;  // getDBModuleList(moduleName)
        data2.data().push_back(item3);
        _sqlData.push_back(data2);

        _sqlData.push_back(data1);  // reloadRouter(moduleName) -> getDBRecordList(moduleName)

        std::map<std::string, std::string> item4 = {
            {"module_name", "module1"}, {"group_name", "group1"}, {"access_status", "0"}};
        std::map<std::string, std::string> item5 = {
            {"module_name", "module1"}, {"group_name", "group2"}, {"access_status", "0"}};
        tars::TC_Mysql::MysqlData data3;
        data3.data().push_back(item4);
        data3.data().push_back(item5);
        _sqlData.push_back(data3);  // getDBGroupList(moduleName)

        std::map<std::string, std::string> item6 = {{"server_status", "M"},
                                                    {"server_name", "DCacheTest"},
                                                    {"source_server_name", "source_DCacheTest"},
                                                    {"idc_area", "SZ"}};
        std::map<std::string, std::string> item7 = {{"server_status", "M"},
                                                    {"server_name", "DCacheTest"},
                                                    {"source_server_name", "source_DCacheTest"},
                                                    {"idc_area", "SZ"}};
        tars::TC_Mysql::MysqlData data4;  // getDBGroupList(moduleName)
        data4.data().push_back(item6);
        _sqlData.push_back(data4);
        tars::TC_Mysql::MysqlData data5;  // getDBGroupList(moduleName)
        data4.data().push_back(item7);
        _sqlData.push_back(data5);

        tars::TC_Mysql::MysqlData data6;  // getDBServerList(modulename)
        std::map<std::string, std::string> item8 = {{"server_name", "DCacheTest"},
                                                    {"id", "1"},
                                                    {"ip", "111.111.111.111"},
                                                    {"idc_area", "SZ"},
                                                    {"server_status", "M"},
                                                    {"module_name", "module1"},
                                                    {"group_name", "group1"},
                                                    {"status", "1"}};
        std::map<std::string, std::string> item9 = {{"server_name", "DCacheTest"},
                                                    {"id", "1"},
                                                    {"ip", "111.111.111.111"},
                                                    {"idc_area", "SZ"},
                                                    {"server_status", "M"},
                                                    {"module_name", "module1"},
                                                    {"group_name", "group2"},
                                                    {"status", "1"}};
        data6.data().push_back(item8);
        data6.data().push_back(item9);
        _sqlData.push_back(data6);
    }

    // void SetUp() override {}

    // void TearDown() override {}

    void init()
    {
        prepareInitData();
        _mysql.reset(new MockMySqlHandlerWrapper());
        _mysqlDBRelation.reset(new MockMySqlHandlerWrapper());
        _mysqlMigrate.reset(new MockMySqlHandlerWrapper());

        ON_CALL(*_mysql, deleteRecord(_, _)).WillByDefault(Return(0));  // init->checkRoute;
        ASSERT_EQ(_sqlData.size(), 7);
        EXPECT_CALL(*_mysql, queryRecord(_))
            .Times(8)
            .WillOnce(Return(_sqlData[0]))   // init->checkRoute->getDBRecordList
            .WillOnce(Return(_sqlData[1]))   // init->loadRouteToMem->getDBGroupList
            .WillOnce(Return(_sqlData[2]))   // init->loadRouteToMem->getDBGroupList
            .WillOnce(Return(_sqlData[3]))   // init->loadRouteToMem->getDBGroupList
            .WillOnce(Return(_sqlData[4]))   // init->loadRouteToMem->getDBGroupList
            .WillOnce(Return(_sqlData[5]))   // init->loadRouteToMem->getDBServerList
            .WillOnce(Return(_sqlData[6]))   // init->loadRouteToMem->getDBModuleList
            .WillOnce(Return(_sqlData[0]));  // init->loadRouteToMem->getDBRecordList
    }

protected:
    DbHandle _db;
    MockRouterServerConfig _cfg;
    std::unique_ptr<MockMySqlHandlerWrapper> _mysql;
    std::unique_ptr<MockMySqlHandlerWrapper> _mysqlDBRelation;
    std::unique_ptr<MockMySqlHandlerWrapper> _mysqlMigrate;
    std::vector<tars::TC_Mysql::MysqlData> _sqlData;
};

TEST_F(DbHandleTest, reloadRouter)
{
    prepareInitData();
    prepareReloadData();
    std::map<std::string, std::string> item1 = {{"id", "1"},
                                                {"module_name", "module1"},
                                                {"from_page_no", "0"},
                                                {"to_page_no", "2000"},
                                                {"group_name", "group1"}};
    std::map<std::string, std::string> item2 = {{"id", "2"},
                                                {"module_name", "module1"},
                                                {"from_page_no", "2001"},
                                                {"to_page_no", "429496"},
                                                {"group_name", "group2"}};
    std::map<std::string, std::string> item3 = {{"id", "3"},
                                                {"module_name", "module2"},
                                                {"from_page_no", "0"},
                                                {"to_page_no", "429496"},
                                                {"group_name", "group1"}};
    tars::TC_Mysql::MysqlData sqlData;
    sqlData.data().push_back(std::move(item1));
    sqlData.data().push_back(std::move(item2));
    sqlData.data().push_back(std::move(item3));

    _mysql.reset(new MockMySqlHandlerWrapper());
    _mysqlDBRelation.reset(new MockMySqlHandlerWrapper());
    _mysqlMigrate.reset(new MockMySqlHandlerWrapper());
    ASSERT_NE(_mysql, nullptr);
    ASSERT_NE(_mysqlDBRelation, nullptr);
    ASSERT_NE(_mysqlMigrate, nullptr);

    ON_CALL(*_mysql, deleteRecord(_, _)).WillByDefault(Return(0));  // init->checkRoute;
    ASSERT_EQ(_sqlData.size(), 14);
    EXPECT_CALL(*_mysql, queryRecord(_))
        .Times(23)
        .WillOnce(Return(_sqlData[0]))  // init->checkRoute->getDBRecordList
        .WillOnce(Return(_sqlData[1]))  // init->loadRouteToMem->getDBGroupList
        .WillOnce(Return(_sqlData[2]))  // init->loadRouteToMem->getDBGroupList
        .WillOnce(Return(_sqlData[3]))  // init->loadRouteToMem->getDBGroupList
        .WillOnce(Return(_sqlData[4]))  // init->loadRouteToMem->getDBGroupList
        .WillOnce(Return(_sqlData[5]))  // init->loadRouteToMem->getDBServerList
        .WillOnce(Return(_sqlData[6]))  // init->loadRouteToMem->getDBModuleList
        .WillOnce(Return(_sqlData[0]))  // init->loadRouteToMem->getDBRecordList
        .WillOnce(Return(sqlData))      // reloadRouter()->getDBRecordList
        .WillOnce(Return(_sqlData[1]))  // reloadRouter()->loadRouteToMem->getDBGroupList
        .WillOnce(Return(_sqlData[2]))  // reloadRouter()->loadRouteToMem->getDBGroupList
        .WillOnce(Return(_sqlData[3]))  // reloadRouter()->loadRouteToMem->getDBGroupList
        .WillOnce(Return(_sqlData[4]))  // reloadRouter()->loadRouteToMem->getDBGroupList
        .WillOnce(Return(_sqlData[5]))  // reloadRouter()->loadRouteToMem->getDBServerList
        .WillOnce(Return(_sqlData[6]))  // reloadRouter()->loadRouteToMem->getDBModuleList
        .WillOnce(Return(_sqlData[0]))  // reloadRouter()->loadRouteToMem->getDBRecordList
        .WillOnce(Return(_sqlData[7]))
        .WillOnce(Return(_sqlData[8]))
        .WillOnce(Return(_sqlData[9]))
        .WillOnce(Return(_sqlData[10]))
        .WillOnce(Return(_sqlData[11]))
        .WillOnce(Return(_sqlData[12]))
        .WillOnce(Return(_sqlData[13]));

    _db.init(_cfg.getDbReloadTime(100000),
             std::move(_mysql),
             std::move(_mysqlDBRelation),
             std::move(_mysqlMigrate),
             new MockPropertyReport());
    _db.initCheck();
    EXPECT_EQ(0, _db.reloadRouter());
    EXPECT_EQ(0, _db.reloadRouter("module1"));
}

TEST_F(DbHandleTest, getIdcMap)
{
    init();
    std::map<std::string, std::string> item1 = {{"city", "random1"}, {"idc", "SZ1"}};
    std::map<std::string, std::string> item2 = {{"city", "random2"}, {"idc", "SZ2"}};

    tars::TC_Mysql::MysqlData sqlData;
    sqlData.data().push_back(item1);
    sqlData.data().push_back(item2);
    EXPECT_CALL(*_mysqlDBRelation, queryRecord(_)).WillOnce(Return(sqlData));

    _db.init(_cfg.getDbReloadTime(100000),
             std::move(_mysql),
             std::move(_mysqlDBRelation),
             std::move(_mysqlMigrate),
             new MockPropertyReport());
    _db.initCheck();
    std::map<string, string> idcmap;
    EXPECT_EQ(0, _db.getIdcMap(idcmap));
}

TEST_F(DbHandleTest, getKindsOfInfo)
{
    init();
    EXPECT_CALL(*_mysql, execute(_)).Times(1);
    _db.init(_cfg.getDbReloadTime(100000),
             std::move(_mysql),
             std::move(_mysqlDBRelation),
             std::move(_mysqlMigrate),
             new MockPropertyReport());
    _db.initCheck();
    PackTable p;
    EXPECT_EQ(0, _db.getPackTable("module1", p));
    EXPECT_EQ(-1, _db.getPackTable("not_exist_module", p));

    EXPECT_EQ(10, _db.getVersion("module1"));
    EXPECT_EQ(12, _db.getVersion("module2"));
    EXPECT_EQ(-1, _db.getVersion("not_exist_module"));

    EXPECT_EQ(0, _db.getSwitchType("module1"));
    EXPECT_EQ(-1, _db.getSwitchType("not_exist_module"));

    EXPECT_EQ(0, _db.getSwitchType("module1"));
    EXPECT_EQ(-1, _db.getSwitchType("not_exist_module"));

    std::string idc;
    EXPECT_EQ(0, _db.getMasterIdc("module1", idc));
    EXPECT_EQ(idc, "SZ");
    EXPECT_EQ(-1, _db.getMasterIdc("not_exist_module", idc));

    std::map<string, ModuleInfo> info;
    EXPECT_EQ(0, _db.getModuleInfos(info));
    EXPECT_NE(0, info.size());
    EXPECT_NE(info.find("module1"), info.cend());
    EXPECT_NE(info.find("module2"), info.cend());
    EXPECT_EQ(info.find("not_exist_module"), info.cend());

    vector<string> moduleList;
    EXPECT_EQ(0, _db.getModuleList(moduleList));
    EXPECT_EQ(2, moduleList.size());


    EXPECT_EQ(0, _db.updateVersion("module1"));

    // getPackTable4SwitchRW
}

TEST_F(DbHandleTest, TransferInfo)
{
    // _db.clearTransferInfos();
    // std::map<string, map<string, TransferInfo>> info;
    // info.resize(10);
    // db_.getTransferInfos(info);
    // EXPECT_EQ(0, info.size());
}



int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}