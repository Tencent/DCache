本文档对如何在 C++业务代码中使用 DCache 进行说明，并提供示例代码。

## 准备工作

请确保你已经安装并成功部署了 TarsCpp 开发环境、DCache 服务和相关模块。

相关内容可参考：[环境安装](install.md)

本文以 KV 为例说明。

成功发布模块后，如下图所示：

![安装成功](images/install_kv_succ.png)

## 开发流程

可以把 KV 模块当作 Tars 框架上已经发布的一个服务，像调用普通服务那样调用 KV 接口。

关于 KV 接口，请参考 [接口描述](proxy_api_guide.md)

具体流程如下：

- 创建的 KV 信息如下：

  - 模块类型：`KV`
  - 模块名：`testKvModule`
  - 代理 servant 名：`DCache.testProxyServer.ProxyObj`

- 需要包含的头文件：

```cpp
#include "servant/Communicator.h"
#include "Proxy.h"

// 为方便演示，直接导入整个命名空间
using namespace tars;
using namespace DCache;
```

- 创建通信器：

```cpp
Communicator comm;
ProxyPrx prx;
string locator = "tars.tarsregistry.QueryObj@tcp -h 192.168.1.2 -p 17890"; # 更换为实际地址
comm.setProperty("locator", locator);
comm.stringToProxy("DCache.testProxyServer.ProxyObj", prx); # 更换为实际的proxy servant
```

- 调用接口：

```cpp
const string module_name = "testKvModule";

// 写数据
SetKVReq setreq;
setreq.moduleName = module_name;
setreq.data.keyItem = "test_key";
setreq.data.value = "test_value";
setreq.data.version = 0;
setreq.data.dirty = false;
setreq.data.expireTimeSecond = 0;

auto ret = prx->setKV(setreq);
if (ret != ET_SUCC)
{
    cout << "setKV error|" << ret << endl;
    return;
}

// 读数据
GetKVReq getreq;
getreq.moduleName = module_name;
getreq.keyItem = "test_key";

GetKVRsp getrsp;
ret = prx->getKV(getreq, getrsp);
if (ret != ET_SUCC)
{
    cout << "getKV error|" << ret << endl;
    return;
}

// 批量写
SetKVBatchReq set_kv_batch_req;
set_kv_batch_req.moduleName = module_name;
for (int i = 1; i < 10; ++i)
{
    SSetKeyValue val;
    val.keyItem = to_string(i);
    val.value = to_string(i);
    val.version = 0;
    val.dirty = false;
    val.expireTimeSecond = 0;
    set_kv_batch_req.data.emplace_back(val);
}

SetKVBatchRsp set_kv_batch_rsp;
ret = prx->setKVBatch(set_kv_batch_req, set_kv_batch_rsp);
if (ret != ET_SUCC)
{
    cout << "setKVBatch error|" << ret << endl;
    return;
}

// 批量读
GetKVBatchReq get_kv_batch_req;
get_kv_batch_req.moduleName = module_name;
for (int i = 1; i < 10; ++i)
{
    get_kv_batch_req.keys.emplace_back(to_string(i));
}

GetKVBatchRsp get_kv_batch_rsp;
ret = prx->getKVBatch(get_kv_batch_req, get_kv_batch_rsp);
if (ret != ET_SUCC)
{
    cout << "getKVBatch error|" << ret << endl;
    return;
}
```

## 编译环境

业务代码需要的头文件位于 DCache 目录内，可以直接把 DCache 目录放在`/usr/local/include`下。

也可以在 Makefile 中指定头文件路径。

Makefile 示例如下：

```makefile
#-----------------------------------------------------------------------

APP       := test
TARGET    := demo
CONFIG    :=
STRIP_FLAG:= N
TARS2CPP_FLAG:=

# 根据DCache实际路径填写
INCLUDE   += -I/usr/local/tars/DCache/src/Proxy -I/usr/local/tars/DCache/src/TarsComm
LIB       += -L/usr/local/tars/cpp/thirdparty/lib

#-----------------------------------------------------------------------
include /usr/local/tars/cpp/makefile/makefile.tars
#-----------------------------------------------------------------------
```
