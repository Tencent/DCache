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
#include <functional>

#define protected public //无奈之举，为了测试私有数据

#include "servant/Global.h"
#include "servant/Application.h"
#include "servant/Current.h"
#include "servant/ServantHelper.h"
#include "util/tc_epoll_server.h"

#include "Global.h"
#include "Proxy.h"
#include "Cache.h"
#include "WCache.h"

#include "MockClass/MockCacheProxy.h"
//#include "MockCommunicator.h"
#include "KVCacheCallback.h"

using namespace std;
using namespace tars;
using namespace DCache;

using ::testing::_;

namespace
{
// shared_ptr<GetKVCallback> makeGetKVCallback()
// {
//     // GetKVReq req;
// 	// 	req.moduleName = "moduleName";
// 	// 	req.keyItem = "keyID";
// 	// 	req.idcSpecified = "";
//     TarsCurrentPtr current = new TarsCurrent();
//     current->getContext()[CONTEXT_CALLER] = "getKV";
//     return make_shared<GetKVCallback>(current, GetKVReq, "", TNOWMS);
// }
}

//
//// 假设cache端正常响应，验证在callback_getKV函数中调用了Proxy::async_response_getKV函数一次,并返回期待的数据
//TEST(GetKVCallback, callback_getKV_with_Correct_Rsp)
//{
//    // 创建TarsCurrent对象（较难，因为和tars紧耦合，需要阅读tars代码）
////	uint32_t uid, const string &ip, int64_t port, int fd, const BindAdapterPtr &adapter
//
//	shared_ptr<TC_EpollServer::RecvContext> recvContext = std::make_shared<TC_EpollServer::RecvContext>();
////    TC_EpollServer::tagRecvData recvDataItem;
//    ServerConfig::ReportFlow = 0;
//	recvContext->buffer() = "ContentsToSend";
//    recvDataItem.adapter = new TC_EpollServer::BindAdapter(new TC_EpollServer());
//    (recvDataItem.adapter)->setName("TestAdapter");
//    (recvDataItem.adapter)->setProtocolName("NoTars"); // 之所以设置成非tars协议，是因为tars协议需要构造符合该协议的数据结构！
//
//    ServantHelperManager::getInstance()->setAdapterServant("TestAdapter", "TestServant");
//
//    TarsCurrentPtr current = new TarsCurrent(new ServantHandle());
//    current->initialize(recvContext);
//
//    current->getContext()[CONTEXT_CALLER] = "getKV";
//    const GetKVReq req;
//    function<void(TarsCurrentPtr, Int32, const GetKVRsp &)> dealWithRsp = FakeProxy::async_response_getKV;
//    shared_ptr<GetKVCallback> getKVcb = make_shared<GetKVCallback>(current, req, "", TNOWMS, dealWithRsp);
//
//    auto share_proxy = make_shared<MockProxy>();
//    MockProxy *proxy = share_proxy.get();
//    const GetKVRsp rsp;
//    EXPECT_CALL(*proxy, async_response_getKV(_, ET_SUCC, rsp))  // TarsCurrent不支持比较运算符：==，因此第一个参数可以是任意值
//        .Times(1);
//    FakeProxy::setProxy(proxy);
//
//    getKVcb->callback_getKV(ET_SUCC, rsp);
//
//}

// TEST(GetKVCallback, callback_getKV_with_Wrong_Rsp)
// {
// 	HMockCommunicator1<CachePrx> t;
//     auto mock_stringToProxy = [](const string &objectName, const string &setName) -> CachePrx {};
// 	EXPECT_CALL(t, stringToProxy(_, _))
//         .Times(1)
//         .WillOnce(Invoke(mock_stringToProxy));
// 	demoClassFunTemp df;
// 	EXPECT_EQ(df.getDemoClassFunTempLog2(),"getDemoClassFunTempLog2-->f(R1* p1,T p2 ,R3 *p3):getDemoClassFunTempLog2");
// }




int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
