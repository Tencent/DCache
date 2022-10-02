[返回](../README.md)

<h1> Proxy接口描述文档 </h1>

* [key-value](#0)
  * [读](#0)
    * [getKV](#0)
    * [getKVBatch](#1)
    * [checkKey](#2)
    * [getAllKeys](#3)
  * [写](#4)
    * [setKV](#4)
    * [insertKV](#5)
    * [updateKV](#6)
    * [eraseKV](#7)
    * [delKV](#8)
    * [setKVBatch](#9)
    * [eraseKVBatch](#10)
    * [delKVBatch](#11)
* [k-k-row](#12)
  * [读](#12)
    * [getMKV](#12)
    * [getMainKeyCount](#13)
    * [getMKVBatch](#14)
    * [getMKVBatchEx](#15)
    * [getMUKBatch](#16)
  * [写](#17)
    * [insertMKV](#17)
    * [updateMKV](#18)
    * [updateMKVAtom](#19)
    * [eraseMKV](#20)
    * [delMKV](#21)
    * [insertMKVBatch](#22)
    * [updateMKVBatch](#23)
    * [delMKVBatch](#24)
* [list](#25)
  * [读](#25)
    * [getList](#25)
    * [getRangeList](#26)
  * [写](#27)
    * [pushList](#27)
    * [popList](#28)
    * [replaceList](#29)
    * [trimList](#30)
    * [remList](#31)
* [set](#set-1)
  * [读](#set-1)
    * [getSet](#set-1)
  * [写](#set-2)
    * [addSet](#set-2)
    * [delSet](#set-3)
* [zset](#zset-1)
  * [读](#zset-1)
    * [getZSetScore](#zset-1)
    * [getZSetPos](#zset-2)
    * [getZSetByPos](#zset-3)
    * [getZSetByScore](#zset-4)
  * [写](#zset-5)
    * [addZSet](#zset-5)
    * [incScoreZSet](#zset-6)
    * [delZSet](#zset-7)
    * [delZSetByScore](#zset-8)
    * [updateZSet](#zset-9)


# <a id="0"></a> getKV

```C++
int getKV(const GetKVReq &req, GetKVRsp &rsp)
```
**功能：** 根据key获取value和其他信息

**参数：**  

```C++
struct GetKVReq  
{  
  1 require string moduleName;  //模块名  
  2 require string keyItem;  //键  
  3 require string idcSpecified = ""; //idc区域  
};

struct GetKVRsp  
{  
  1 require string value;  //值  
  2 require byte ver;  //数据版本号  
  3 require int expireTime = 0;  //过期时间  
}; 
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | 模块名错误
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_DB_ERR     | 数据库读取错误
 ET_KEY_INVALID| key无效
 ET_INPUT_PARAM_ERROR | 参数错误，例如key为空
 ET_SYS_ERR    | 系统异常
 ET_NO_DATA                  | 数据不存在
 ET_SUCC   | 读取数据成功
 

  
# <a id="1"></a> getKVBatch
 
```C++
int getKVBatch(const GetKVBatchReq &req, GetKVBatchRsp &rsp)
```

**功能：** 批量查询数据


**参数：**  

```C++
struct GetKVBatchReq
{
  1 require string moduleName; //模块名
  2 require vector<string> keys; //键集合
  3 require string idcSpecified = ""; //idc区域 
};

struct GetKVBatchRsp
{
  1 require vector<SKeyValue> values;  //结果集合
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | 模块名错误
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_CACHE_ERR| cache读取错误
 ET_INPUT_PARAM_ERROR | 参数错误，例如key数量超过限制或者某个key为空等
 ET_SYS_ERR    | 系统异常
 ET_SUCC   | 批量读取成功
 
 
# <a id="2"></a> checkKey
 
```C++
int checkKey(const CheckKeyReq &req, CheckKeyRsp &rsp)
```

**功能：** 批量检查数据的状态，如：是否存在、是否脏数据等等


**参数：**  

```C++
struct CheckKeyReq
{
  1 require string moduleName;  //模块名
  2 require vector<string> keys;  //键集合
  3 require string idcSpecified = "";  //idc区域
};

struct CheckKeyRsp
{
  1 require map<string, SKeyStatus> keyStat; 
};

struct SKeyStatus
{
  1 require bool exist;  //是否存在
  2 require bool dirty;  //是否为脏数据
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | 模块名错误
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_CACHE_ERR| cache读取错误
 ET_INPUT_PARAM_ERROR | 参数错误，例如key数量超过限制或者某个key为空等
 ET_SYS_ERR    | 系统异常
 ET_SUCC   | 批量读取成功
 
 

# <a id="3"></a> getAllKeys
 
```C++
int getAllKeys(const GetAllKeysReq &req, GetAllKeysRsp &rsp)
```

**功能：** 获取所有key


**参数：**  

```C++
struct GetAllKeysReq
{
  1 require string moduleName;  //模块名
  2 require int index;  //从index指定的hash桶开始遍历，初始从0开始
  3 require int count;  //遍历多少个hash桶
  4 require string idcSpecified = "";  //idc区域
}；

struct GetAllKeysRsp
{
  1 require vector<string> keys;  //键集合
  2 require bool isEnd;   //是否还有数据，由于桶的数量不定，通过isEnd参数来表示后面是否还有hash桶
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | 模块名错误
 ET_CACHE_ERR| cache读取错误
 ET_SYS_ERR    | 系统异常
 ET_SUCC   | 读取成功
 
 
# <a id="4"></a> setKV
 
```C++
int setKV(const SetKVReq &req)
```

**功能：** 写入数据


**参数：**  

```C++
struct SetKVReq
{
  1 require string moduleName;  //模块名
  2 require SSetKeyValue data;
};

struct SSetKeyValue
{
  1 require string keyItem;  //键
  2 require string value;  //值
  3 require byte   version = 0;  //数据版本，缺省为0
  4 require bool   dirty = true;  //是否设置为脏数据，缺省为true
  5 require int    expireTimeSecond = 0;  //过期时间，缺省为0，表示永不过期
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | 模块名错误
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_PARAM_TOO_LONG | key长度超过限制
 ET_FORBID_OPT	| 迁移时禁止写入
 ET_DATA_VER_MISMATCH | 版本不一致
 ET_MEM_FULL | 内存满
 ET_CACHE_ERR| cache错误
 ET_INPUT_PARAM_ERROR | 参数错误，例如个key为空等
 ET_SYS_ERR    | 系统异常
 ET_SUCC   | 写入成功
 
 
# <a id="5"></a> insertKV
 
```C++
int insertKV(const SetKVReq &req)
```

**功能：** key不存在时写入，否则返回失败


**参数：**  

```C++
struct SetKVReq
{
  1 require string moduleName;  //模块名
  2 require SSetKeyValue data;
};

struct SSetKeyValue
{
  1 require string keyItem;  //键
  2 require string value;  //值
  3 require byte   version = 0;  //数据版本，缺省为0
  4 require bool   dirty = true;  //是否设置为脏数据，缺省为true
  5 require int    expireTimeSecond = 0;  //过期时间，缺省为0，表示永不过期
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_DATA_EXIST | 数据已存在，禁止写入
 ET_MODULE_NAME_INVALID                  | 模块名错误
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_PARAM_TOO_LONG | key长度超过限制
 ET_FORBID_OPT	| 迁移时禁止写入
 ET_DATA_VER_MISMATCH | 版本不一致
 ET_MEM_FULL | 内存满
 ET_CACHE_ERR| cache错误
 ET_INPUT_PARAM_ERROR | 参数错误，例如key为空等
 ET_SYS_ERR    | 系统异常
 ET_SUCC   | 写入成功
 
 
 
# <a id="6"></a> updateKV
 
```C++
int updateKV(const UpdateKVReq &req, UpdateKVRsp &rsp)
```

**功能：** 更新数据


**参数：**  

```C++
struct UpdateKVReq
{
  1 require string moduleName;  //模块名
  2 require SSetKeyValue data;  //新数据
  3 require Op option;    //更新动作，支持ADD/SUB/ADD_INSERT/SUB_INSERT
};

struct UpdateKVRsp
{
  1 require string retValue;  //更新之后的值
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_NO_DATA | 数据不存在，停止更新
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_PARAM_TOO_LONG | key长度超过限制
 ET_FORBID_OPT	| 迁移时禁止写入
 ET_PARAM_OP_ERR | option错误
 ET_DATA_VER_MISMATCH | 版本不一致
 ET_MEM_FULL | 内存满
 ET_CACHE_ERR| cache错误
 ET_INPUT_PARAM_ERROR | 参数错误，例如key为空等
 ET_SYS_ERR    | 系统异常
 ET_SUCC   | 更新成功
 
 
# <a id="7"></a> eraseKV
 
```C++
int eraseKV(const RemoveKVReq &req)
```

**功能：** 删除数据，此接口无法删除脏数据


**参数：**  

```C++
struct RemoveKVReq
{
  1 require string moduleName;  //模块名
  2 require KeyInfo keyInfo;
};

struct KeyInfo
{
  1 require string keyItem;  //键
  2 require byte version;  //数据版本号
};

```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT	| 迁移时禁止写入
 ET_ERASE_DIRTY_ERR | 不允许删除脏数据
 ET_DATA_VER_MISMATCH | 版本不一致
 ET_CACHE_ERR| cache错误
 ET_INPUT_PARAM_ERROR | 参数错误，例如key为空等
 ET_SYS_ERR    | 系统异常
 ET_SUCC   | 删除成功
 
 
# <a id="8"></a> delKV
 
```C++
int delKV(const RemoveKVReq &req)
```

**功能：** 删除数据，可以删除脏数据


**参数：**  

```C++
struct RemoveKVReq
{
  1 require string moduleName;  //模块名
  2 require KeyInfo keyInfo;
};

struct KeyInfo
{
  1 require string keyItem;  //键
  2 require byte version;  //数据版本号
};

```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT	| 迁移时禁止写入
 ET_DB_ERR | 数据库写入异常
 ET_DATA_VER_MISMATCH | 版本不一致
 ET_CACHE_ERR| cache错误
 ET_INPUT_PARAM_ERROR | 参数错误，例如key为空等
 ET_SYS_ERR    | 系统异常
 ET_SUCC   | 删除成功
 
 
# <a id="9"></a> setKVBatch
 
```C++
int setKVBatch(const SetKVBatchReq &req, SetKVBatchRsp &rsp)
```

**功能：** 批量写入数据


**参数：**  

```C++
struct SetKVBatchReq
{
  1 require string moduleName;  //模块名
  2 require vector<SSetKeyValue> data;  //批量写入的数据集合
};

struct SetKVBatchRsp
{
  1 require map<string, int> keyResult;   //每个key写入的结果 WRITE_SUCC = 0/WRITE_ERROR = -1/WRITE_DATA_VER_MISMATCH = -2
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_INPUT_PARAM_ERROR | 参数错误，例如key数量超过限制或者某个key为空等
 ET_SYS_ERR | 系统异常
 ET_PARTIAL_FAIL | 部分写入成功，即存在写入失败的键值对
 ET_SUCC   | 全部写入成功
 
 
# <a id="10"></a> eraseKVBatch
 
```C++
int eraseKVBatch(const RemoveKVBatchReq &req, RemoveKVBatchRsp &rsp)
```

**功能：** 批量删除数据，该接口不允许用来删除脏数据


**参数：**  

```C++
struct RemoveKVBatchReq
{
  1 require string moduleName;  //模块名
  2 require vector<KeyInfo> data;  
};

struct KeyInfo
{
  1 require string keyItem; //键
  2 require byte version;  //数据版本号
};

struct RemoveKVBatchRsp
{
  1 require map<string, int> keyResult;  //每个key写入的结果 WRITE_SUCC = 0/WRITE_ERROR = -1/WRITE_DATA_VER_MISMATCH = -2
};

```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_INPUT_PARAM_ERROR | 参数错误，例如key数量超过限制或者某个key为空等
 ET_SYS_ERR | 系统异常
 ET_PARTIAL_FAIL | 部分删除成功，即存在删除失败的键值对
 ET_SUCC   | 全部删除成功
 

# <a id="11"></a> delKVBatch
 
```C++
int delKVBatch(const RemoveKVBatchReq &req, RemoveKVBatchRsp &rsp)
```

**功能：** 批量删除数据，该接口可以用来删除脏数据


**参数：**  

```C++
struct RemoveKVBatchReq
{
  1 require string moduleName;  //模块名
  2 require vector<KeyInfo> data;  
};

struct KeyInfo
{
  1 require string keyItem;  //键
  2 require byte version;  //数据版本号
};

struct RemoveKVBatchRsp
{
  1 require map<string, int> keyResult;  //每个key写入的结果 WRITE_SUCC = 0/WRITE_ERROR = -1/WRITE_DATA_VER_MISMATCH = -2
};

```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_AREA_ERR| 当前key不属于本机服务，需要更新路由表重新访问
 ET_INPUT_PARAM_ERROR | 参数错误，例如key数量超过限制或者某个key为空等
 ET_SYS_ERR | 系统异常
 ET_PARTIAL_FAIL | 部分删除成功，即存在删除失败的键值对
 ET_SUCC   | 全部删除成功
 
 
# <a id="12"></a> getMKV
 
```C++
int getMKV(const GetMKVReq &req, GetMKVRsp &rsp)
```

**功能：** 根据指定的字段和限制条件查询数据


**参数：**  

```C++
struct GetMKVReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require string field;  //需要查询的字段集，多个字段用','分隔如 "a,b", "*"表示所有
  4 require vector<Condition> cond;  //查询条件集合，除主Key外的其他字段，多个条件直间为And关系
  5 require bool retEntryCnt = false;  //是否返回主key下的总记录条数
  6 require string idcSpecified = "";  //idc区域
};

struct GetMKVRsp
{
  1 require vector<map<string, string> > data;  //查询结果
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_PARAM_LIMIT_VALUE_ERR	| 查询条件集合的某个元素的limit属性值填写错误
 ET_PARAM_NOT_EXIST	| 查询条件集合字段填写错误，无效字段
 ET_PARAM_REDUNDANT	| 查询条件集合有重复字段或无效字段
 ET_SYS_ERR	| 系统异常
 其他值(大于等于0)|如果请求结构体的retEntryCnt属性设为true，则该返回值表示主key下的总记录条数，否则，仅表示操作成功
 
 
# <a id="13"></a> getMainKeyCount
 
```C++
int getMainKeyCount(const MainKeyReq &req)
```

**功能：** 获取主key下数据记录总数


**参数：**  

```C++
struct MainKeyReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require string idcSpecified = "";  //idc区域
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_KEY_INVALID	| mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 其他值(大于等于0)|主key下的记录总数
 
 
# <a id="14"></a> getMKVBatch
 
```C++
int getMKVBatch(const MKVBatchReq &req, MKVBatchRsp &rsp)
```

**功能：** 批量查询，不同的主key使用相同的查询条件

**参数：**  

```C++
struct MKVBatchReq
{
  1 require string moduleName; //模块名
  2 require vector<string> mainKeys;  //主key集合
  3 require string field;  //需要查询的字段集，多个字段用','分隔如 "a,b", "*"表示所有
  4 require vector<Condition> cond;  //查询条件集合，除主Key外的其他字段，多个条件直间为And关系
  5 require string idcSpecified = "";  //idc区域
};

struct Condition
{
  1 require string fieldName;  //字段名称
  2 require Op op;  //条件动作，支持==/!=/</>/<=/>=
  3 require string value;  //值
};

struct MKVBatchRsp
{
  1require vector<MainKeyValue> data;  //查询结果集合
};

struct MainKeyValue
{
  1 require string mainKey;  //主key
  2 require vector<map<string, string> > value;  //在该主key下查询到的记录的集合
  3 require int ret; //ret>=0 成功，其他 失败
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_KEY_INVALID | 主key集合中存在mainKey无效的元素
 ET_INPUT_PARAM_ERROR | 主key集合中存在mainKey为空的元素或者主key集合大小超过限制
 ET_PARAM_LIMIT_VALUE_ERR	| 查询条件集合的某个元素的limit属性值填写错误
 ET_PARAM_NOT_EXIST	| 查询条件集合字段填写错误，无效字段
 ET_PARAM_REDUNDANT	| 查询条件集合有重复字段或无效字段
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 批量读取成功
 
 
# <a id="15"></a> getMKVBatchEx
 
```C++
int getMKVBatchEx(const MKVBatchExReq &req, MKVBatchExRsp &rsp)
```

**功能：** 批量查询，每个主key都有特定的查询条件，查询条件支持“与或”逻辑

**参数：**  

```C++
struct MKVBatchExReq
{
  1 require string moduleName;  //模块名
  2 require vector<MainKeyCondition> cond;  //查询条件
  3 require string idcSpecified = "";  //idc区域
};

struct MainKeyCondition
{
  1 require string mainKey;  //主key
  2 require string field;  //需要查询的字段集，多个字段用','分隔如 "a,b", "*"表示所有
  3 require vector<vector<Condition> > cond;  //查询条件集合，内层为'and'关系，外层为'or'关系
  4 require Condition limit;  //查询起始和count限制，op = DCache::LIMIT, value = "startIndex:countLimit"
  5 require bool bGetMKCout = false;  //是否返回主key下的总记录条数
};

struct MKVBatchExRsp
{
  1 require vector<MainKeyValue> data;  //查询结果集合
};

struct MainKeyValue
{
  1 require string mainKey;  //主key
  2 require vector<map<string, string> > value;  //在该主key下查询到的记录的集合
  3 require int ret; //ret>=0 成功，其他 失败
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_KEY_INVALID | 查询条件中存在mainKey无效的元素
 ET_INPUT_PARAM_ERROR | 查询条件中存在mainKey为空的元素或者主key数量超过限制
 ET_PARAM_LIMIT_VALUE_ERR	| 查询条件集合的某个元素的limit属性值填写错误
 ET_PARAM_NOT_EXIST	| 查询条件集合字段填写错误，无效字段
 ET_PARAM_REDUNDANT	| 查询条件集合有重复字段或无效字段
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 批量读取成功


# <a id="16"></a> getMUKBatch
 
```C++
int getMUKBatch(const MUKBatchReq &req, MUKBatchRsp &rsp)
```

**功能：** 批量查询

**参数：**  

```C++
struct MUKBatchReq
{
  1 require string moduleName;  //模块名
  2 require vector<Record> primaryKeys;  //主key集合
  3 require string field;  //需要查询的字段集，多个字段用','分隔如 "a,b", "*"表示所有
  4 require string idcSpecified = "";  //idc区域
};

struct MUKBatchRsp
{
  1 require vector<Record> data;
};

struct Record
{
  1 require string mainKey;  //主key
  2 require map<string, string> mpRecord; // 在请求结构体中，表示查询条件，必须包含所有联合key，因此可以唯一确定一条记录；在响应结构体中表示查询到的数据
  3 require int ret;  //在请求结构体中，可忽略不填；在响应结构体中，ret>=0 成功，其他 失败
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_KEY_INVALID | 主key集合中存在mainKey无效的元素
 ET_INPUT_PARAM_ERROR | 主key集合中存在mainKey为空的元素或者主key集合大小超过限制
 ET_PARAM_MISSING	| 查询条件未包含所有联合key
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 批量读取成功


# <a id="17"></a> insertMKV
 
```C++
int insertMKV(const InsertMKVReq &req)
```

**功能：** 写入数据

**参数：**  

```C++
struct InsertMKVReq
{
  1 require string moduleName;  //模块名
  2 require InsertKeyValue data;  //待写入数据
};

struct InsertKeyValue
{
  1 require string mainKey;  //主key
  2 require map<string, UpdateValue> mpValue;  //除主key外的其他字段数据
  3 require byte ver = 0;  //版本号
  4 require bool dirty = true;  //是否设置为脏数据
  5 require bool replace = false;  //如果记录已存在且replace为true时则覆盖旧记录
  6 require  int  expireTimeSecond = 0;  //数据过期时间
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
ET_SERVER_TYPE_ERR	| SLAVE状态下不提供接口服务
ET_MODULE_NAME_INVALID	| 模块名错误
ET_FORBID_OPT | 正在迁移，禁止操作
ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
ET_INPUT_PARAM_ERROR | 参数错误，例如mainKey为空
ET_PARAM_DIGITAL_ERR | 字段错误，例如值为数字类型的字段包含了非数字字符
ET_PARAM_TOO_LONG | 字段大小超过限制
ET_PARAM_MISSING | 待写入数据未包含全部联合字段
ET_PARAM_OP_ERR	| op错误，只支持SET
ET_PARAM_NOT_EXIST | 字段不存在
ET_DATA_EXIST | 数据已存在，禁止insert
ET_DATA_VER_MISMATCH | 版本不一致
ET_MEM_FULL	| 内存满
ET_SYS_ERR	| 系统异常
ET_SUCC	| 写入成功


# <a id="18"></a> updateMKV
 
```C++
int updateMKV(const UpdateMKVReq &req)
```

**功能：** 更新数据

**参数：**  

```C++
struct UpdateMKVReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require map<string, UpdateValue> value;  //需要更新的字段和对应的值，不能填联合key字段
  4 require vector<Condition> cond;  //数据更新的条件
  5 require byte ver = 0;  //版本号
  6 require bool dirty = true;  //是否设置为脏数据
  7 require bool insert = false; //如果要修改的唯一记录不存在且insert为true时则插入一条数据
  8 require int expireTimeSecond = 0;  //过期时间
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
ET_SERVER_TYPE_ERR	| SLAVE状态下不提供接口服务
ET_MODULE_NAME_INVALID	| 模块名错误
ET_FORBID_OPT | 正在迁移，禁止操作
ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
ET_INPUT_PARAM_ERROR | 参数错误，例如mainKey为空
ET_PARAM_UPDATE_UKEY_ERR | 更新字段包含了联合字段
ET_PARAM_DIGITAL_ERR | 字段错误，例如值为数字类型的字段包含了非数字字符
ET_PARAM_TOO_LONG | 字段大小超过限制
ET_PARAM_NOT_EXIST | 字段不存在
ET_PARAM_REDUNDANT	| 更新条件中的联合字段重复填写
ET_PARAM_LIMIT_VALUE_ERR | 	更新条件集合的某个元素的limit属性值填写错误
ET_DATA_VER_MISMATCH | 版本不一致
ET_SYS_ERR	| 系统异常
其他值(大于等于0)	|更新成功，且此值表示更新操作影响的记录条数


# <a id="19"></a> updateMKVAtom
 
```C++
int updateMKVAtom(const UpdateMKVAtomReq &req)
```

**功能：** 原子更新数据

**参数：**  

```C++
struct UpdateMKVAtomReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require map<string, UpdateValue> value;  //需要更新的字段和对应的值，不能填联合key字段，op支持SET、ADD、SUB操作。OP为ADD和SUB时要求更新的字段为数值类型
  4 require vector<Condition> cond;  //数据更新的条件
  5 require bool dirty = true;  //是否设置为脏数据
  6 require int  expireTimeSecond = 0;  //过期时间
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
ET_SERVER_TYPE_ERR	| SLAVE状态下不提供接口服务
ET_MODULE_NAME_INVALID	| 模块名错误
ET_FORBID_OPT | 正在迁移，禁止操作
ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
ET_INPUT_PARAM_ERROR | 参数错误，例如mainKey为空
ET_PARAM_UPDATE_UKEY_ERR | 更新字段包含了联合字段
ET_PARAM_DIGITAL_ERR | 字段错误，例如值为数字类型的字段包含了非数字字符
ET_PARAM_TOO_LONG | 字段大小超过限制
ET_PARAM_NOT_EXIST | 字段不存在
ET_PARAM_REDUNDANT	| 更新条件中的联合字段重复填写
ET_PARAM_LIMIT_VALUE_ERR | 	更新条件集合的某个元素的limit属性值填写错误
ET_DATA_VER_MISMATCH | 版本不一致
ET_SYS_ERR	| 系统异常
其他值(大于等于0)	|更新成功，且此值表示更新操作影响的记录条数


# <a id="20"></a> eraseMKV
 
```C++
int eraseMKV(const MainKeyReq &req)
```

**功能：** 删除数据，但不能用来删除脏数据

**参数：**  

```C++
struct MainKeyReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require string idcSpecified = "";  //idc区域，
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
ET_SERVER_TYPE_ERR	| SLAVE状态下不提供接口服务
ET_MODULE_NAME_INVALID	| 模块名错误
ET_FORBID_OPT | 正在迁移，禁止操作
ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
ET_INPUT_PARAM_ERROR | 参数错误，例如mainKey为空
ET_ERASE_DIRTY_ERR	| 不允许使用该接口删除脏数据
ET_SYS_ERR	| 系统异常
ET_SUCC	| 删除成功


# <a id="21"></a> delMKV
 
```C++
int delMKV(const DelMKVReq &req)
```

**功能：** 删除数据，可以用来删除脏数据

**参数：**  

```C++
struct DelMKVReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require vector<Condition> cond;  //条件集合
};

struct Condition
{
  1 require string fieldName;  //字段名称
  2 require Op op;  //条件动作，支持==/!=/</>/<=/>=
  3 require string value;  //值
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
ET_SERVER_TYPE_ERR	| SLAVE状态下不提供接口服务
ET_MODULE_NAME_INVALID	| 模块名错误
ET_FORBID_OPT | 正在迁移，禁止操作
ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
ET_INPUT_PARAM_ERROR | 参数错误，例如mainKey为空
ET_PARAM_LIMIT_VALUE_ERR | 条件集合的某个元素的limit属性值填写错误
ET_PARAM_NOT_EXIST | 字段不存在
ET_PARAM_REDUNDANT	| 字段重复
ET_SYS_ERR	| 系统异常
ET_SUCC	| 删除成功


# <a id="22"></a> insertMKVBatch
 
```C++
int insertMKVBatch(const InsertMKVBatchReq &req, MKVBatchWriteRsp &rsp)
```

**功能：** 批量写入数据

**参数：**  

```C++
struct InsertMKVBatchReq
{
  1 require string moduleName;  //模块名
  2 require vector<InsertKeyValue> data;  //待写入数据集合
};

struct InsertKeyValue
{
  1 require string mainKey;  //主key
  2 require map<string, UpdateValue> mpValue;  //其他字段值
  3 require byte ver = 0;  //版本号
  4 require bool dirty = true;  //是否设置为脏数据
  5 require bool replace = false; //如果记录已存在且replace为true时则覆盖旧记录
  6 require int expireTimeSecond = 0;  //过期时间
};

struct MKVBatchWriteRsp
{
  1 require map<int, int> rspData;  //键:批量请求中data的index，值:写失败的原因；只返回写入失败的数据，如果rspData为空，则表示全部写入成功
};

```

**返回值**：

返回值 | 含义
------------------ | ----------------
ET_MODULE_NAME_INVALID	| 模块名错误
ET_INPUT_PARAM_ERROR | 待写入数据集合中存在mainKey为空的元素或者主key数量超过限制
ET_KEY_INVALID | mainKey无效
ET_SYS_ERR	| 系统异常
ET_SUCC	 | 批量写操作完成


# <a id="23"></a> updateMKVBatch
 
```C++
int updateMKVBatch(const UpdateMKVBatchReq &req, MKVBatchWriteRsp &rsp)
```

**功能：** 批量更新数据

**参数：**  

```C++
struct UpdateMKVBatchReq
{
  1 require string moduleName;  //模块名
  2 require vector<UpdateKeyValue> data;  //更新数据集合
};

struct UpdateKeyValue
{
  1 require  string mainKey;  //主key
  2 require  map<string, UpdateFieldInfo> mpValue;  //更新数据
  3 require  byte ver = 0;  //数据版本号
  4 require  bool dirty = true;  //是否设置为脏数据
  5 require  bool insert = false; //如果要修改的唯一记录不存在且insert为true时则插入一条数据
  6 require  int  expireTimeSecond = 0;  //过期时间
};

struct MKVBatchWriteRsp
{
  1 require map<int, int> rspData;  //键:批量请求中data的index，值:更新失败的原因；只返回更新失败的数据，如果rspData为空，则表示全部更新成功
};

```

**返回值**：

返回值 | 含义
------------------ | ----------------
ET_MODULE_NAME_INVALID	| 模块名错误
ET_INPUT_PARAM_ERROR | 更新数据集合中存在mainKey为空的元素或者主key数量超过限制
ET_KEY_INVALID | mainKey无效
ET_SYS_ERR	| 系统异常
ET_SUCC	 | 批量更新操作完成


# <a id="24"></a> delMKVBatch
 
```C++
int delMKVBatch(const DelMKVBatchReq &req, MKVBatchWriteRsp &rsp)
```

**功能：** 批量删除数据

**参数：**  

```C++
struct DelMKVBatchReq
{
  1 require string moduleName;  //模块名
  2 require vector<DelCondition> data;  //待删除数据集合
};

struct DelCondition
{
  1 require string mainKey;  //主key
  2 require vector<Condition> cond;  //删除条件集合
  3 require byte ver = 0; //数据版本号
};

struct MKVBatchWriteRsp
{
  1 require map<int, int> rspData;  //键:批量请求中data的index，值:删除结果，大于等于0表示在该mainKey删除的记录条数，其他指示删除失败的原因
};

```

**返回值**：

返回值 | 含义
------------------ | ----------------
ET_MODULE_NAME_INVALID	| 模块名错误
ET_INPUT_PARAM_ERROR | 待删除数据集合中存在mainKey为空的元素或者主key数量超过限制
ET_KEY_INVALID | mainKey无效
ET_SYS_ERR	| 系统异常
ET_SUCC	 | 批量删除操作完成


# <a id="25"></a> getList

```C++
int getList(const GetListReq &req, GetListRsp &rsp)
```

**功能：** 根据指定的主key和索引查询数据

**参数：**

```C++
struct GetListReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require string field;  //需要查询的字段集，多个字段用','分隔如 "a,b", "*"表示所有
  4 require long index;  //索引
  5 require string idcSpecified = "";  //idc区域
};

struct GetListRsp
{
  1 require Entry entry;  //查询结果
};

struct Entry
{
  1 require map<string, string> data;
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 查询成功


# <a id="26"></a> getRangeList

```C++
int getRangeList(const GetRangeListReq &req, BatchEntry &rsp)
```

**功能：** 根据指定的主key查找索引值在区间[startIndex, endIndex]的数据

**参数：**

```C++
struct GetRangeListReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require string field;  //需要查询的字段集，多个字段用','分隔如 "a,b", "*"表示所有
  4 require long startIndex;  //开始索引
  5 require long endIndex;  //结束索引
  6 require string idcSpecified = "";  //idc区域
};

struct BatchEntry
{
1 require vector<map<string, string>> entries;  //查询结果集合
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 查询成功


# <a id="27"></a> pushList

```C++
int pushList(const PushListReq &req)
```

**功能：** 在list头部或者尾部插入数据，支持批量操作

**参数：**

```C++
struct PushListReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require vector<InsertKeyValue> data;  //待插入数据
  4 require bool atHead = true;   //true表示插入到list头部，false表示插入尾部
};

struct InsertKeyValue
{
  1 require string mainKey;  //主key
  2 require map<string, UpdateValue> mpValue;  //其他字段数据
  3 require byte ver = 0;  //数据版本号
  4 require bool dirty = true;  //是否设置为脏数据
  5 require bool replace = false; //如果记录已存在且replace为true时则覆盖旧记录
  6 require int expireTimeSecond = 0;  //过期时间，0表示永不过期
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_PARAM_DIGITAL_ERR | 字段错误，例如值为数字类型的字段包含了非数字字符
 ET_PARAM_TOO_LONG | 字段大小超过限制
 ET_PARAM_OP_ERR	| op错误，只支持SET
 ET_PARAM_NOT_EXIST | 字段不存在
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 成功写入数据


# <a id="28"></a> popList

```C++
int popList(const PopListReq &req, PopListRsp &rsp)
```

**功能：** 从list头部或者尾部删除一条数据

**参数：**

```C++
struct PopListReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require bool atHead = true;   //true表示从list头部删除，false表示尾部删除
};

struct PopListRsp
{
  1 require Entry entry;  //被删除的数据
};

struct Entry
{
  1 require map<string, string> data;
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 成功删除

# <a id="29"></a> replaceList

```C++
int replaceList(const ReplaceListReq &req)
```

**功能：** 根据指定主key更新list上指定索引的数据

**参数：**

```C++
struct ReplaceListReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require map<string, UpdateValue> data;  //新数据
  4 require long index;  //待替换数据在列表中的索引
  5 require int expireTime;  //过期时间
};

struct UpdateValue
{
  1 require Op op;  //更新动作，只支持SET
  2 require string value;  //用来替换的值，字段为数值类型时，此值不能包含非数字字符
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_PARAM_DIGITAL_ERR | 字段错误，例如值为数字类型的字段包含了非数字字符
 ET_PARAM_TOO_LONG | 字段大小超过限制
 ET_PARAM_OP_ERR	| op错误，只支持SET
 ET_PARAM_NOT_EXIST | 字段不存在
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 替换成功


# <a id="30"></a> trimList

```C++
int trimList(const TrimListReq &req)
```

**功能：** 裁剪列表，只保留指定区间：[startIndex, endIndex]内的数据，删除区间外的数据

**参数：**

```C++
struct TrimListReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require long startIndex;  //开始索引
  4 require long endIndex;  //结束索引
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 操作成功


# <a id="31"></a> remList

```C++
int remList(const RemListReq &req)
```

**功能：** 从列表头部或者尾部删除一条或多条数据

**参数：**

```C++
struct RemListReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require bool atHead = true;   //true表示从list头部删除，false表示从尾部删除
  4 require long count;  //指定删除数据条数
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 删除成功


# <a id="set-1"></a> getSet

```C++
int getSet(const GetSetReq &req, BatchEntry &rsp)
```

**功能：** 查询数据

**参数：**

```C++
struct GetSetReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require string field;  //需要查询的字段集，多个字段用','分隔如 "a,b", "*"表示所有
  4 require string idcSpecified = "";  //idc区域
};

struct BatchEntry
{
  1 require vector<map<string, string>> entries;  //查询结果集合
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 查询成功

# <a id="set-2"></a> addSet

```C++
int addSet(const AddSetReq &req)
```

**功能：** 写入数据

**参数：**

```C++
struct AddSetReq
{
  1 require string moduleName;  //模块名
  2 require AddSetKeyValue value;  //待写入数据
};

struct AddSetKeyValue
{
  1 require string mainKey;  //主key
  2 require map<string, UpdateValue> data;  //其他字段数据
  3 require int expireTime;  //过期时间
  4 require bool dirty = true;  //是否设置为脏数据
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_PARAM_DIGITAL_ERR | 字段错误，例如值为数字类型的字段包含了非数字字符
 ET_PARAM_TOO_LONG | 字段大小超过限制
 ET_PARAM_OP_ERR	| op错误，只支持SET
 ET_PARAM_NOT_EXIST | 字段不存在
 ET_DATA_EXIST | 数据已存在
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 成功写入数据

# <a id="set-3"></a> delSet

```C++
int delSet(const DelSetReq &req)
```

**功能：** 删除数据，只能删除一条数据

**参数：**

```C++
struct DelSetReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require vector<Condition> cond;  //条件集合
};

struct Condition
{
  1 require string fieldName;  //字段名称，需填写所有字段
  2 require Op op;  //操作，只支持EQ
  3 require string value;  //值
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_PARAM_REDUNDANT | 字段重复或字段不存在
 ET_PARAM_MISSING | 字段缺失
 ET_DB_ERR | 数据库错误
 ET_PARAM_OP_ERR	| op错误，只支持EQ
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 成功删除数据

# <a id="zset-1"></a> getZSetScore

```C++
int getZSetScore(const GetZsetScoreReq &req, double &score)
```

**功能：** 根据指定条件，查询某条记录的分值

**参数：**

```C++
struct GetZsetScoreReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require vector<Condition> cond;  //条件集合
  4 require string idcSpecified = "";  //idc区域
};

struct Condition
{
  1 require string fieldName;  //字段名称，需填写所有字段
  2 require Op op;  //操作，只支持EQ
  3 require string value;  //值
};

double &score //查询结果：记录的分值
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_PARAM_REDUNDANT | 字段重复或字段不存在
 ET_PARAM_MISSING | 字段缺失
 ET_DB_ERR | 数据库错误
 ET_PARAM_OP_ERR	| op错误，只支持EQ
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_NO_DATA | 条件指定的数据不存在
 ET_SUCC | 查询成功


# <a id="zset-2"></a> getZSetPos

```C++
int getZSetPos(const GetZsetPosReq &req, long &pos)
```

**功能：** 根据指定条件，查询某条记录在已排序列表的索引位置

**参数：**

```C++
struct GetZsetPosReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require vector<Condition> cond;  //条件集合
  4 require bool positiveOrder = true;  //true表示按正序查找，false表示逆序查找
  5 require string idcSpecified = "";  //idc区域
};

long &pos //查询结果：记录的在已排序列表的索引位置
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_PARAM_REDUNDANT | 字段重复或字段不存在
 ET_PARAM_MISSING | 字段缺失
 ET_DB_ERR | 数据库错误
 ET_PARAM_OP_ERR	| op错误，只支持EQ
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_NO_DATA | 条件指定的数据不存在
 ET_SUCC | 查询成功


# <a id="zset-3"></a> getZSetByPos

```C++
int getZSetByPos(const GetZsetByPosReq &req, BatchEntry &rsp)
```

**功能：** 查询索引区间[start, end]内的数据

**参数：**

```C++
struct GetZsetByPosReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require string field;  //需要查询的字段集，多个字段用','分隔如 "a,b", "*"表示所有
  4 require long start;  //开始索引
  5 require long end;  //结束索引
  6 require bool positiveOrder = true; //true表示返回的结果按递增排序，false表示递减
  7 require string idcSpecified = "";  //idc区域
};

struct BatchEntry
{
  1 require vector<map<string, string>> entries;  //查询结果数据集合
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_DB_ERR | 数据库错误
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 查询成功


 
# <a id="zset-4"></a> getZSetByScore

```C++
int getZSetByScore(const GetZsetByScoreReq &req, BatchEntry &rsp)
```

**功能：** 查询分值区间[minScore, maxScore]内的数据

**参数：**

```C++
struct GetZsetByScoreReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require string field;  //需要查询的字段集，多个字段用','分隔如 "a,b", "*"表示所有
  4 require double minScore;  //最小分值
  5 require double maxScore;  //最大分值
  6 require string idcSpecified = "";  //idc区域
};

struct BatchEntry
{
  1 require vector<map<string, string>> entries;  //查询结果数据集合
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_DB_ERR | 数据库错误
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 查询成功
 

# <a id="zset-5"></a> addZSet

```C++
int addZSet(const AddZSetReq &req)
```

**功能：** 将带有给定分值的数据添加到有序集合中，如果数据已存在，则重置其分值为score

**参数：**

```C++
AddZSetReq
{
  1 require string moduleName;  //模块名
  2 require AddSetKeyValue value;  //待写入数据
  3 require double score;  //待写入数据分值
};

struct AddSetKeyValue
{
  1 require string mainKey;  //主key
  2 require map<string, UpdateValue> data; //其他字段数据
  3 require int expireTime;  //数据过期时间
  4 require bool dirty = true;  //是否设置为脏数据
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_PARAM_DIGITAL_ERR | 字段错误，例如值为数字类型的字段包含了非数字字符
 ET_PARAM_TOO_LONG | 字段大小超过限制
 ET_PARAM_OP_ERR | op错误，只支持SET
 ET_PARAM_NOT_EXIST | 字段不存在
 ET_MEM_FULL | 内存满
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 添加成功


# <a id="zset-6"></a> incScoreZSet

```C++
int incScoreZSet(const IncZSetScoreReq &req)
```

**功能：** 修改有序集合中某条记录的分值，若数据不存在，则新建一条数据，新数据的分值为scoreDiff

**参数：**

```C++
struct IncZSetScoreReq
{
  1 require string moduleName;  //模块名
  2 require AddSetKeyValue value;  //
  3 require double scoreDiff;  //score变化值，可以是负数
};

struct AddSetKeyValue
{
  1 require string mainKey;  //主key
  2 require map<string, UpdateValue> data;  //指定数据，可作为条件查找记录或者新数据
  3 require int expireTime;  //数据过期时间
  4 require bool dirty = true;  //是否设置为脏数据
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_PARAM_DIGITAL_ERR | 字段错误，例如值为数字类型的字段包含了非数字字符
 ET_PARAM_TOO_LONG | 字段大小超过限制
 ET_PARAM_OP_ERR | op错误，只支持SET
 ET_PARAM_NOT_EXIST | 字段不存在
 ET_MEM_FULL | 内存满
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 分值更新成功


# <a id="zset-7"></a> delZSet

```C++
int delZSet(const DelZSetReq &req)
```

**功能：** 删除有序集合中符合指定条件的某条数据

**参数：**

```C++
struct DelZSetReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require vector<Condition> cond;  //条件集合，用来确定唯一一条数据
};

struct Condition
{
  1 require string fieldName;  //字段名称，需填写所有字段
  2 require Op op;  //操作，只支持EQ
  3 require string value;  //值
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_PARAM_REDUNDANT | 字段重复或字段不存在
 ET_PARAM_MISSING | 字段缺失
 ET_DB_ERR | 数据库错误
 ET_PARAM_OP_ERR	| op错误，只支持EQ
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 删除成功


# <a id="zset-8"></a> delZSetByScore

```C++
int delZSetByScore(const DelZSetByScoreReq &req)
```

**功能：** 从有序集合中删除分值在区间[minScore, maxScore)的数据

**参数：**

```C++
struct DelZSetByScoreReq
{
  1 require string moduleName;  //模块名
  2 require string mainKey;  //主key
  3 require double minScore;  //最小分值
  4 require double maxScore;  //最大分值
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_DB_ERR | 数据库错误
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 删除成功


# <a id="zset-9"></a> updateZSet

```C++
int updateZSet(const UpdateZSetReq &req)
```

**功能：** 根据指定条件更新有序集合的某条数据

**参数：**

```C++
struct UpdateZSetReq
{
  1 require string moduleName;  //模块名
  2 require AddSetKeyValue value;  //新数据
  3 require vector<Condition> cond;  //条件集合，用来确定唯一一条数据
};

struct AddSetKeyValue
{
  1 require string mainKey;  //主key
  2 require map<string, UpdateValue> data;  //新数据
  3 require int expireTime;  //数据过期时间
  4 require bool dirty = true;  //是否设置为脏数据
};

struct Condition
{
  1 require string fieldName;  //字段名称，需填写所有字段
  2 require Op op;  //操作，只支持EQ
  3 require string value;  //值
};
```

**返回值**：

返回值 | 含义
------------------ | ----------------
 ET_MODULE_NAME_INVALID | 模块名错误
 ET_KEY_AREA_ERR	| 当前key不属于本机服务，需要更新路由表重新访问
 ET_FORBID_OPT | 正在迁移，禁止操作
 ET_SERVER_TYPE_ERR | SLAVE状态下不提供接口服务
 ET_PARAM_DIGITAL_ERR | 字段错误，例如值为数字类型的字段包含了非数字字符
 ET_PARAM_TOO_LONG | 字段大小超过限制
 ET_PARAM_NOT_EXIST | 字段不存在
 ET_PARAM_MISSING | 字段缺失
 ET_PARAM_REDUNDANT | 字段重复或字段不存在
 ET_PARAM_OP_ERR | op错误，只支持EQ
 ET_DB_ERR | 数据库错误
 ET_KEY_INVALID | mainKey无效
 ET_INPUT_PARAM_ERROR | mainKey为空
 ET_SYS_ERR	| 系统异常
 ET_SUCC | 更新成功