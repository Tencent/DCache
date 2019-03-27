<h1> Proxy API doc </h1>

* [key-value](#0)
  * [read](#0)
    * [getKV](#0)
    * [getKVBatch](#1)
    * [checkKey](#2)
    * [getAllKeys](#3)
  * [write](#4)
    * [setKV](#4)
    * [insertKV](#5)
    * [updateKV](#6)
    * [eraseKV](#7)
    * [delKV](#8)
    * [setKVBatch](#9)
    * [eraseKVBatch](#10)
    * [delKVBatch](#11)
* [k-k-row](#12)
  * [read](#12)
    * [getMKV](#12)
    * [getMainKeyCount](#13)
    * [getMKVBatch](#14)
    * [getMKVBatchEx](#15)
    * [getMUKBatch](#16)
  * [write](#17)
    * [insertMKV](#17)
    * [updateMKV](#18)
    * [updateMKVAtom](#19)
    * [eraseMKV](#20)
    * [delMKV](#21)
    * [insertMKVBatch](#22)
    * [updateMKVBatch](#23)
    * [delMKVBatch](#24)
* [list](#25)
  * [read](#25)
    * [getList](#25)
    * [getRangeList](#26)
  * [write](#27)
    * [pushList](#27)
    * [popList](#28)
    * [replaceList](#29)
    * [trimList](#30)
    * [remList](#31)
* [set](#set-1)
  * [read](#set-1)
    * [getSet](#set-1)
  * [write](#set-2)
    * [addSet](#set-2)
    * [delSet](#set-3)
* [zset](#zset-1)
  * [read](#zset-1)
    * [getZSetScore](#zset-1)
    * [getZSetPos](#zset-2)
    * [getZSetByPos](#zset-3)
    * [getZSetByScore](#zset-4)
  * [write](#zset-5)
    * [addZSet](#zset-5)
    * [incScoreZSet](#zset-6)
    * [delZSet](#zset-7)
    * [delZSetByScore](#zset-8)
    * [updateZSet](#zset-9)


# <a id="0"></a> getKV

```C++
int getKV(const GetKVReq &req, GetKVRsp &rsp)
```
Get value and other data by key

**Parameters:**  

```C++
struct GetKVReq  
{  
  1 require string moduleName;  //module name  
  2 require string keyItem;
  3 require string idcSpecified = ""; //idc area  
};

struct GetKVRsp  
{  
  1 require string value;
  2 require byte ver;  //Data version number  
  3 require int expireTime = 0;  //expiration time 
}; 
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | module error
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_DB_ERR     | access database error
 ET_KEY_INVALID| key is invalid
 ET_INPUT_PARAM_ERROR | parameter error, for example, the key is empty
 ET_SYS_ERR    | system error
 ET_NO_DATA                  | data does not exist
 ET_SUCC   | query data successfully
 

  
# <a id="1"></a> getKVBatch
 
```C++
int getKVBatch(const GetKVBatchReq &req, GetKVBatchRsp &rsp)
```

Bulk query


**Parameters:**  

```C++
struct GetKVBatchReq
{
  1 require string moduleName; //module name
  2 require vector<string> keys; //set of keys
  3 require string idcSpecified = ""; //idc area 
};

struct GetKVBatchRsp
{
  1 require vector<SKeyValue> values;  //set of results
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | module error
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_CACHE_ERR| access cache error
 ET_INPUT_PARAM_ERROR | parameter error, for example, the number of keys exceeds the limit or a key is empty.
 ET_SYS_ERR    | system error
 ET_SUCC   | query data in batch successfully
 
 
# <a id="2"></a> checkKey
 
```C++
int checkKey(const CheckKeyReq &req, CheckKeyRsp &rsp)
```

Check status of multiple key-values


**Parameters:**  

```C++
struct CheckKeyReq
{
  1 require string moduleName;  //module name
  2 require vector<string> keys;  //set of keys
  3 require string idcSpecified = "";  //idc area
};

struct CheckKeyRsp
{
  1 require map<string, SKeyStatus> keyStat; 
};

struct SKeyStatus
{
  1 require bool exist;  //whether the data exists
  2 require bool dirty;  //whether the data is dirty
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | module error
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_CACHE_ERR| access cache error
 ET_INPUT_PARAM_ERROR | parameter error, for example, the number of keys exceeds the limit or a key is empty.
 ET_SYS_ERR    | system error
 ET_SUCC   | query data in batch successfully
 
 

# <a id="3"></a> getAllKeys
 
```C++
int getAllKeys(const GetAllKeysReq &req, GetAllKeysRsp &rsp)
```

Get all keys in cache


**Parameters:**  

```C++
struct GetAllKeysReq
{
  1 require string moduleName;  //module name
  2 require int index;  //traversing from the hash bucket specified by index, starting from 0
  3 require int count;  //the number of hash buckets to traverse
  4 require string idcSpecified = "";  //idc area
}；

struct GetAllKeysRsp
{
  1 require vector<string> keys;  //set of keys
  2 require bool isEnd;   //since the number of buckets is uncertain, the isEnd parameter is used to indicate whether there is any hash bucket that has not been traversed
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | module error
 ET_CACHE_ERR| access cache error
 ET_SYS_ERR    | system error
 ET_SUCC   | query data successfully
 
 
# <a id="4"></a> setKV
 
```C++
int setKV(const SetKVReq &req)
```

insert data


**Parameters:**  

```C++
struct SetKVReq
{
  1 require string moduleName;  //module name
  2 require SSetKeyValue data;
};

struct SSetKeyValue
{
  1 require string keyItem;  
  2 require string value;  
  3 require byte   version = 0;  //data version number
  4 require bool   dirty = true;  //whether to make it dirty data
  5 require int    expireTimeSecond = 0;  //expiration time, default is 0, indicating never expired
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID                  | module error
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_PARAM_TOO_LONG | the length of key exceeds limit
 ET_FORBID_OPT	| write disabled during migration
 ET_DATA_VER_MISMATCH | different version number
 ET_MEM_FULL | no free space in memory
 ET_CACHE_ERR| cache error
 ET_INPUT_PARAM_ERROR | parameter error, for example, the key is empty
 ET_SYS_ERR    | system error
 ET_SUCC   | insert successfully
 
 
# <a id="5"></a> insertKV
 
```C++
int insertKV(const SetKVReq &req)
```

Insert when data does not exist, otherwise it will fail


**Parameters:**  

```C++
struct SetKVReq
{
  1 require string moduleName;  //module name
  2 require SSetKeyValue data;
};

struct SSetKeyValue
{
  1 require string keyItem;  
  2 require string value;  
  3 require byte   version = 0;  //data version number
  4 require bool   dirty = true;  //whether to make it dirty data
  5 require int    expireTimeSecond = 0;  //expiration time, default is 0, indicating never expired
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_DATA_EXIST | data already exists, nsertion is not allowed
 ET_MODULE_NAME_INVALID                  | module error
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_PARAM_TOO_LONG | the length of key exceeds limit
 ET_FORBID_OPT	| write disabled during migration
 ET_DATA_VER_MISMATCH | different version number
 ET_MEM_FULL | no free space in memory
 ET_CACHE_ERR| cache error
 ET_INPUT_PARAM_ERROR | parameter error, for example, the key is empty
 ET_SYS_ERR    | system error
 ET_SUCC   | insert successfully
 
 
 
# <a id="6"></a> updateKV
 
```C++
int updateKV(const UpdateKVReq &req, UpdateKVRsp &rsp)
```

Update data


**Parameters:**  

```C++
struct UpdateKVReq
{
  1 require string moduleName;  //module name
  2 require SSetKeyValue data;  //data used for updating
  3 require Op option;    //update action，including ADD/SUB/ADD_INSERT/SUB_INSERT
};

struct UpdateKVRsp
{
  1 require string retValue;
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_NO_DATA | data does not exist
 ET_MODULE_NAME_INVALID | module error
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_PARAM_TOO_LONG | the length of key exceeds limit
 ET_FORBID_OPT	| write disabled during migration
 ET_PARAM_OP_ERR | option error
 ET_DATA_VER_MISMATCH | different version number
 ET_MEM_FULL | no free space in memory
 ET_CACHE_ERR| cache error
 ET_INPUT_PARAM_ERROR | parameter error, for example, the key is empty
 ET_SYS_ERR    | system error
 ET_SUCC   | update successfully
 
 
# <a id="7"></a> eraseKV
 
```C++
int eraseKV(const RemoveKVReq &req)
```

Delete data that is not dirty


**Parameters:**  

```C++
struct RemoveKVReq
{
  1 require string moduleName;  //module name
  2 require KeyInfo keyInfo;
};

struct KeyInfo
{
  1 require string keyItem;  
  2 require byte version;  //Data version number
};

```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT	| write disabled during migration
 ET_ERASE_DIRTY_ERR | can't be used to delete dirty data
 ET_DATA_VER_MISMATCH | different version number
 ET_CACHE_ERR| cache error
 ET_INPUT_PARAM_ERROR | parameter error, for example, the key is empty
 ET_SYS_ERR    | system error
 ET_SUCC   | delete successfully
 
 
# <a id="8"></a> delKV
 
```C++
int delKV(const RemoveKVReq &req)
```

Delete data, including dirty data


**Parameters:**  

```C++
struct RemoveKVReq
{
  1 require string moduleName;  //module name
  2 require KeyInfo keyInfo;
};

struct KeyInfo
{
  1 require string keyItem;  
  2 require byte version;  //Data version number
};

```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT	| write disabled during migration
 ET_DB_ERR | access database error
 ET_DATA_VER_MISMATCH | different version number
 ET_CACHE_ERR| cache error
 ET_INPUT_PARAM_ERROR | parameter error, for example, the key is empty
 ET_SYS_ERR    | system error
 ET_SUCC   | delete successfully
 
 
# <a id="9"></a> setKVBatch
 
```C++
int setKVBatch(const SetKVBatchReq &req, SetKVBatchRsp &rsp)
```

Insert data in batch


**Parameters:**  

```C++
struct SetKVBatchReq
{
  1 require string moduleName;  //module name
  2 require vector<SSetKeyValue> data;  //batch data
};

struct SetKVBatchRsp
{
  1 require map<string, int> keyResult;   //results of insert key-values, WRITE_SUCC = 0/WRITE_ERROR = -1/WRITE_DATA_VER_MISMATCH = -2
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_INPUT_PARAM_ERROR | parameter error, for example, the number of keys exceeds the limit or a key is empty.
 ET_SYS_ERR | system error
 ET_PARTIAL_FAIL | only part of data was inserted successfully
 ET_SUCC   | all data is successfully inserted
 
 
# <a id="10"></a> eraseKVBatch
 
```C++
int eraseKVBatch(const RemoveKVBatchReq &req, RemoveKVBatchRsp &rsp)
```

Delete data in batches, this interface is not allowed to delete dirty data


**Parameters:**  

```C++
struct RemoveKVBatchReq
{
  1 require string moduleName;  //module name
  2 require vector<KeyInfo> data;  
};

struct KeyInfo
{
  1 require string keyItem; 
  2 require byte version;  //Data version number
};

struct RemoveKVBatchRsp
{
  1 require map<string, int> keyResult;  //results of delete operation, WRITE_SUCC = 0/WRITE_ERROR = -1/WRITE_DATA_VER_MISMATCH = -2
};

```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_INPUT_PARAM_ERROR | parameter error, for example, the number of keys exceeds the limit or a key is empty.
 ET_SYS_ERR | system error
 ET_PARTIAL_FAIL | only part of the data was deleted successfully
 ET_SUCC   | all data is successfully deleted
 

# <a id="11"></a> delKVBatch
 
```C++
int delKVBatch(const RemoveKVBatchReq &req, RemoveKVBatchRsp &rsp)
```

Delete data in batches, including dirty data


**Parameters:**  

```C++
struct RemoveKVBatchReq
{
  1 require string moduleName;  //module name
  2 require vector<KeyInfo> data;  
};

struct KeyInfo
{
  1 require string keyItem;  
  2 require byte version;  //Data version number
};

struct RemoveKVBatchRsp
{
  1 require map<string, int> keyResult;  //results of insert key-values WRITE_SUCC = 0/WRITE_ERROR = -1/WRITE_DATA_VER_MISMATCH = -2
};

```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_AREA_ERR| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_INPUT_PARAM_ERROR | parameter error, for example, the number of keys exceeds the limit or a key is empty.
 ET_SYS_ERR | system error
 ET_PARTIAL_FAIL | only part of the data was deleted successfully
 ET_SUCC   | all data is successfully deleted
 
 
# <a id="12"></a> getMKV
 
```C++
int getMKV(const GetMKVReq &req, GetMKVRsp &rsp)
```

Query data based on fields and conditions


**Parameters:**  

```C++
struct GetMKVReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require string field;  //the set of fields to be queried, multiple fields separated by ',' such as "a, b", "*" for all fields
  4 require vector<Condition> cond;  //conditions used to filter data when query
  5 require bool retEntryCnt = false;  //whether to return the number of records where the primary key is mainKey
  6 require string idcSpecified = "";  //idc area
};

struct GetMKVRsp
{
  1 require vector<map<string, string> > data;  //result set
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_PARAM_LIMIT_VALUE_ERR	| The value  of field named limit in a condition is incorrect.
 ET_PARAM_NOT_EXIST	| invalid field in conditions
 ET_PARAM_REDUNDANT	| duplicate field or field is invalid
 ET_SYS_ERR	| system error
 other values (greater than or equal to 0)|if the retEntryCnt attribute of the request structure is set to true, the return value indicates the total number of records under the primary key. Otherwise, only the operation is successful.
 
 
# <a id="13"></a> getMainKeyCount
 
```C++
int getMainKeyCount(const MainKeyReq &req)
```

Get the total number of data records under the given mainKey


**Parameters:**  

```C++
struct MainKeyReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require string idcSpecified = "";  //idc area
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_KEY_INVALID	| mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 other values (greater than or equal to 0)|the total number of data records under the given mainKey
 
 
# <a id="14"></a> getMKVBatch
 
```C++
int getMKVBatch(const MKVBatchReq &req, MKVBatchRsp &rsp)
```

Query in batch

**Parameters:**  

```C++
struct MKVBatchReq
{
  1 require string moduleName; //module name
  2 require vector<string> mainKeys;  
  3 require string field;  //the set of fields to be queried, multiple fields separated by ',' such as "a, b", "*" for all fields
  4 require vector<Condition> cond;  //conditions used to filter data when query
  5 require string idcSpecified = "";  //idc area
};

struct Condition
{
  1 require string fieldName;  
  2 require Op op;  //condition operators, support ==/!=/</>/<=/>=
  3 require string value;  
};

struct MKVBatchRsp
{
  1require vector<MainKeyValue> data;  //result set
};

struct MainKeyValue
{
  1 require string mainKey;  
  2 require vector<map<string, string> > value;  //records queried under the specified mainKey
  3 require int ret; //ret>=0, success; other values, failed
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_KEY_INVALID | some mainKeys are invalid
 ET_INPUT_PARAM_ERROR | invalid mainKey or too many mainKeys
 ET_PARAM_LIMIT_VALUE_ERR	| The value  of field named limit in a condition is incorrect.
 ET_PARAM_NOT_EXIST	| invalid field in conditions
 ET_PARAM_REDUNDANT	| duplicate field or field is invalid
 ET_SYS_ERR	| system error
 ET_SUCC | query data in batch successfully
 
 
# <a id="15"></a> getMKVBatchEx
 
```C++
int getMKVBatchEx(const MKVBatchExReq &req, MKVBatchExRsp &rsp)
```

Query in batch, each mainKey has its specific query conditions

**Parameters:**  

```C++
struct MKVBatchExReq
{
  1 require string moduleName;  //module name
  2 require vector<MainKeyCondition> cond;
  3 require string idcSpecified = "";  //idc area
};

struct MainKeyCondition
{
  1 require string mainKey;  
  2 require string field;  //the set of fields to be queried, multiple fields separated by ',' such as "a, b", "*" for all fields
  3 require vector<vector<Condition> > cond;
  4 require Condition limit;  //op = DCache::LIMIT, value = "startIndex:countLimit"
  5 require bool bGetMKCout = false;  //whether to return the number of records where the primary key is mainKey
};

struct MKVBatchExRsp
{
  1 require vector<MainKeyValue> data;  //result set
};

struct MainKeyValue
{
  1 require string mainKey;  
  2 require vector<map<string, string> > value;  //records queried under the specified mainKey
  3 require int ret; //ret>=0, success; other values, failed
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_KEY_INVALID | invalid mainKey in some conditions
 ET_INPUT_PARAM_ERROR | empty mainKey in some conditions or too many mainKes
 ET_PARAM_LIMIT_VALUE_ERR	| the value  of field named limit in a condition is incorrect
 ET_PARAM_NOT_EXIST	| invalid field in conditions
 ET_PARAM_REDUNDANT	| duplicate field or field is invalid
 ET_SYS_ERR	| system error
 ET_SUCC | query data in batch successfully


# <a id="16"></a> getMUKBatch
 
```C++
int getMUKBatch(const MUKBatchReq &req, MUKBatchRsp &rsp)
```

Query in batch

**Parameters:**  

```C++
struct MUKBatchReq
{
  1 require string moduleName;  //module name
  2 require vector<Record> primaryKeys;  
  3 require string field;  //the set of fields to be queried, multiple fields separated by ',' such as "a, b", "*" for all fields
  4 require string idcSpecified = "";  //idc area
};

struct MUKBatchRsp
{
  1 require vector<Record> data;
};

struct Record
{
  1 require string mainKey;  
  2 require map<string, string> mpRecord; // In the request structure, this field represents the query condition and must contain all secondary keys, in the response structure, this field represents result set 
  3 require int ret;  //in the request structure, ignore it; in the response structure, ret>=0, success, other values, fail
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_KEY_INVALID | some mainKeys are invalid
 ET_INPUT_PARAM_ERROR | invalid mainKey or too many mainKeys
 ET_PARAM_MISSING	| not all secondary keys are included in conditions
 ET_SYS_ERR	| system error
 ET_SUCC | query data in batch successfully


# <a id="17"></a> insertMKV
 
```C++
int insertMKV(const InsertMKVReq &req)
```

insert data

**Parameters:**  

```C++
struct InsertMKVReq
{
  1 require string moduleName;  //module name
  2 require InsertKeyValue data;
};

struct InsertKeyValue
{
  1 require string mainKey;  
  2 require map<string, UpdateValue> mpValue;  //Other fields except mainKey
  3 require byte ver = 0;  //data version number
  4 require bool dirty = true;  //whether to make it dirty data
  5 require bool replace = false;  //overwrite old record if the record already exists and replace is true
  6 require  int  expireTimeSecond = 0;  //expiration time 
};
```

**Returns**：

Returns | Description
------------------ | ----------------
ET_SERVER_TYPE_ERR	| service is not provided since it is a slave
ET_MODULE_NAME_INVALID	| module error
ET_FORBID_OPT | data is being migrated
ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
ET_INPUT_PARAM_ERROR | parameter error, for example, mainKey is empty.
ET_PARAM_DIGITAL_ERR | field error, such as a field with a numeric type containing non-numeric characters
ET_PARAM_TOO_LONG | the length of field exceeds limit
ET_PARAM_MISSING | did not contain all secondary keys
ET_PARAM_OP_ERR	| op error, only support SET
ET_PARAM_NOT_EXIST | field does not exist
ET_DATA_EXIST | Data already exists
ET_DATA_VER_MISMATCH | different version number
ET_MEM_FULL	| no free space in memory
ET_SYS_ERR	| system error
ET_SUCC	| insert successfully


# <a id="18"></a> updateMKV
 
```C++
int updateMKV(const UpdateMKVReq &req)
```

Update data

**Parameters:**  

```C++
struct UpdateMKVReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require map<string, UpdateValue> value;  //Fields that need to be updated, exclude secondary key 
  4 require vector<Condition> cond;  
  5 require byte ver = 0;  //data version number
  6 require bool dirty = true;  //whether to make it dirty data
  7 require bool insert = false; //insert a record if the unique record to be modified does not exist and insert == true
  8 require int expireTimeSecond = 0;  //expiration time
};
```

**Returns**：

Returns | Description
------------------ | ----------------
ET_SERVER_TYPE_ERR	| service is not provided since it is a slave
ET_MODULE_NAME_INVALID	| module error
ET_FORBID_OPT | data is being migrated
ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
ET_INPUT_PARAM_ERROR | parameter error, for example, mainKey is empty.
ET_PARAM_UPDATE_UKEY_ERR | fields to be updated contain secondary key field
ET_PARAM_DIGITAL_ERR | field error, such as a field with a numeric type containing non-numeric characters
ET_PARAM_TOO_LONG | the length of field exceeds limit
ET_PARAM_NOT_EXIST | field does not exist
ET_PARAM_REDUNDANT	| there are multiple secondary key fields with the same name in the condition.
ET_PARAM_LIMIT_VALUE_ERR | 	the value  of field named limit in a condition is incorrect
ET_DATA_VER_MISMATCH | different version number
ET_SYS_ERR	| system error
other values (greater than or equal to 0)	|update successful, also this value indicates the number of records affected by the update operation


# <a id="19"></a> updateMKVAtom
 
```C++
int updateMKVAtom(const UpdateMKVAtomReq &req)
```

Atomically update data

**Parameters:**  

```C++
struct UpdateMKVAtomReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require map<string, UpdateValue> value;  //fields that need to be updated, do not include the secondary key field, op supports SET, ADD, SUB operations.
  4 require vector<Condition> cond;  
  5 require bool dirty = true;  //whether to make it dirty data
  6 require int  expireTimeSecond = 0;  //expiration time
};
```

**Returns**：

Returns | Description
------------------ | ----------------
ET_SERVER_TYPE_ERR	| service is not provided since it is a slave
ET_MODULE_NAME_INVALID	| module error
ET_FORBID_OPT | data is being migrated
ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
ET_INPUT_PARAM_ERROR | parameter error, for example, mainKey is empty.
ET_PARAM_UPDATE_UKEY_ERR | fields to be updated contain secondary key field
ET_PARAM_DIGITAL_ERR | field error, such as a field with a numeric type containing non-numeric characters
ET_PARAM_TOO_LONG | the length of field exceeds limit
ET_PARAM_NOT_EXIST | field does not exist
ET_PARAM_REDUNDANT	| there are multiple secondary key fields with the same name in the condition.
ET_PARAM_LIMIT_VALUE_ERR | 	the value  of field named limit in a condition is incorrect
ET_DATA_VER_MISMATCH | different version number
ET_SYS_ERR	| system error
other values (greater than or equal to 0)	|update successful, also this value indicates the number of records affected by the update operation


# <a id="20"></a> eraseMKV
 
```C++
int eraseMKV(const MainKeyReq &req)
```

Delete data which is not dirty

**Parameters:**  

```C++
struct MainKeyReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require string idcSpecified = "";  //idc area
};
```

**Returns**：

Returns | Description
------------------ | ----------------
ET_SERVER_TYPE_ERR	| service is not provided since it is a slave
ET_MODULE_NAME_INVALID	| module error
ET_FORBID_OPT | data is being migrated
ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
ET_INPUT_PARAM_ERROR | parameter error, for example, mainKey is empty.
ET_ERASE_DIRTY_ERR	| trying to delete dirty data
ET_SYS_ERR	| system error
ET_SUCC	| delete successfully


# <a id="21"></a> delMKV
 
```C++
int delMKV(const DelMKVReq &req)
```

Delete data, including dirty data

**Parameters:**  

```C++
struct DelMKVReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require vector<Condition> cond;
};

struct Condition
{
  1 require string fieldName;  
  2 require Op op;  //condition operators, support ==/!=/</>/<=/>=
  3 require string value;  
};
```

**Returns**：

Returns | Description
------------------ | ----------------
ET_SERVER_TYPE_ERR	| service is not provided since it is a slave
ET_MODULE_NAME_INVALID	| module error
ET_FORBID_OPT | data is being migrated
ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
ET_INPUT_PARAM_ERROR | parameter error, for example, mainKey is empty.
ET_PARAM_LIMIT_VALUE_ERR | the value  of field named limit in a condition is incorrect
ET_PARAM_NOT_EXIST | field does not exist
ET_PARAM_REDUNDANT	| multiple fields with the same name
ET_SYS_ERR	| system error
ET_SUCC	| delete successfully


# <a id="22"></a> insertMKVBatch
 
```C++
int insertMKVBatch(const InsertMKVBatchReq &req, MKVBatchWriteRsp &rsp)
```

Bulk insert

**Parameters:**  

```C++
struct InsertMKVBatchReq
{
  1 require string moduleName;  //module name
  2 require vector<InsertKeyValue> data;  
};

struct InsertKeyValue
{
  1 require string mainKey;  
  2 require map<string, UpdateValue> mpValue;  //values of fields exclude mainKey
  3 require byte ver = 0;  //data version number
  4 require bool dirty = true;  //whether to make it dirty data
  5 require bool replace = false; //overwrite old record if the record already exists and replace is true
  6 require int expireTimeSecond = 0;  //expiration time
};

struct MKVBatchWriteRsp
{
  1 require map<int, int> rspData;  //key:the index of the data field in the request structure，value:the reason for inserting failure, rspData is empty if all inserts succeed
};

```

**Returns**：

Returns | Description
------------------ | ----------------
ET_MODULE_NAME_INVALID	| module error
ET_INPUT_PARAM_ERROR | invalid mainKey or too many mainKeys
ET_KEY_INVALID | mainKey is invalid
ET_SYS_ERR	| system error
ET_SUCC	 | operation of bulk insert completed


# <a id="23"></a> updateMKVBatch
 
```C++
int updateMKVBatch(const UpdateMKVBatchReq &req, MKVBatchWriteRsp &rsp)
```

Bulk update

**Parameters:**  

```C++
struct UpdateMKVBatchReq
{
  1 require string moduleName;  //module name
  2 require vector<UpdateKeyValue> data;  
};

struct UpdateKeyValue
{
  1 require  string mainKey;  
  2 require  map<string, UpdateFieldInfo> mpValue;
  3 require  byte ver = 0;  //Data version number
  4 require  bool dirty = true;  //whether to make it dirty data
  5 require  bool insert = false; //insert a record if the unique record to be modified does not exist and insert == true
  6 require  int  expireTimeSecond = 0;  //expiration time
};

struct MKVBatchWriteRsp
{
  1 require map<int, int> rspData;  //key:the index of the data field in the request structure，value:the reason for inserting failure, rspData is empty if all inserts succeed
};

```

**Returns**：

Returns | Description
------------------ | ----------------
ET_MODULE_NAME_INVALID	| module error
ET_INPUT_PARAM_ERROR | invalid mainKey or too many mainKeys
ET_KEY_INVALID | mainKey is invalid
ET_SYS_ERR	| system error
ET_SUCC	 | operation of bulk update completed


# <a id="24"></a> delMKVBatch
 
```C++
int delMKVBatch(const DelMKVBatchReq &req, MKVBatchWriteRsp &rsp)
```

bulk delete

**Parameters:**  

```C++
struct DelMKVBatchReq
{
  1 require string moduleName;  //module name
  2 require vector<DelCondition> data;  
};

struct DelCondition
{
  1 require string mainKey;  
  2 require vector<Condition> cond;
  3 require byte ver = 0; //Data version number
};

struct MKVBatchWriteRsp
{
  1 require map<int, int> rspData;  //key: The index of the data in the request structure, value: the result of the deletion. The value greater than or equal to 0 indicates the number of records deleted, other values indicate why the deletion failed
};

```

**Returns**：

Returns | Description
------------------ | ----------------
ET_MODULE_NAME_INVALID	| module error
ET_INPUT_PARAM_ERROR | invalid mainKey or too many mainKeys
ET_KEY_INVALID | mainKey is invalid
ET_SYS_ERR	| system error
ET_SUCC	 | operation of bulk delete completed


# <a id="25"></a> getList

```C++
int getList(const GetListReq &req, GetListRsp &rsp)
```

Query data based on specified mainKey and index

**Parameters:**

```C++
struct GetListReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require string field;  //the set of fields to be queried, multiple fields separated by ',' such as "a, b", "*" for all fields
  4 require long index;
  5 require string idcSpecified = "";  //idc area
};

struct GetListRsp
{
  1 require Entry entry;  //result set
};

struct Entry
{
  1 require map<string, string> data;
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | query successfully


# <a id="26"></a> getRangeList

```C++
int getRangeList(const GetRangeListReq &req, BatchEntry &rsp)
```

Get a range of elements from a list

**Parameters:**

```C++
struct GetRangeListReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require string field;  //the set of fields to be queried, multiple fields separated by ',' such as "a, b", "*" for all fields
  4 require long startIndex;  
  5 require long endIndex;
  6 require string idcSpecified = "";  //idc area
};

struct BatchEntry
{
1 require vector<map<string, string>> entries;  //result set
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | query successfully


# <a id="27"></a> pushList

```C++
int pushList(const PushListReq &req)
```

Add one or more data to the head or tail of the list

**Parameters:**

```C++
struct PushListReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require vector<InsertKeyValue> data;  
  4 require bool atHead = true;   //Insert data in the head of the list if it is true, otherwise insert data in the tail of the list
};

struct InsertKeyValue
{
  1 require string mainKey;  
  2 require map<string, UpdateValue> mpValue;
  3 require byte ver = 0;  //Data version number
  4 require bool dirty = true;  //whether to make it dirty data
  5 require bool replace = false; //overwrite old record if the record already exists and replace is true
  6 require int expireTimeSecond = 0;  
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_PARAM_DIGITAL_ERR | field error, such as a field with a numeric type containing non-numeric characters
 ET_PARAM_TOO_LONG | the length of field exceeds limit
 ET_PARAM_OP_ERR	| op error, only support SET
 ET_PARAM_NOT_EXIST | field does not exist
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | push successfully


# <a id="28"></a> popList

```C++
int popList(const PopListReq &req, PopListRsp &rsp)
```

Remove one element from the head or tail of the list

**Parameters:**

```C++
struct PopListReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require bool atHead = true;   //remove from the head if it true, otherwise remove from the tail
};

struct PopListRsp
{
  1 require Entry entry;  //data that was removed
};

struct Entry
{
  1 require map<string, string> data;
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | reomve successfully

# <a id="29"></a> replaceList

```C++
int replaceList(const ReplaceListReq &req)
```

Replace data based on specified mainKey and specified index

**Parameters:**

```C++
struct ReplaceListReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require map<string, UpdateValue> data;  //data used for updating
  4 require long index;  //specified index
  5 require int expireTime;  //expiration time
};

struct UpdateValue
{
  1 require Op op;  //update operator, only support SET
  2 require string value;  
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_PARAM_DIGITAL_ERR | field error, such as a field with a numeric type containing non-numeric characters
 ET_PARAM_TOO_LONG | the length of field exceeds limit
 ET_PARAM_OP_ERR	| op error, only support SET
 ET_PARAM_NOT_EXIST | field does not exist
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | replace successfully


# <a id="30"></a> trimList

```C++
int trimList(const TrimListReq &req)
```

Trim list

**Parameters:**

```C++
struct TrimListReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require long startIndex;  
  4 require long endIndex;  
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | success


# <a id="31"></a> remList

```C++
int remList(const RemListReq &req)
```

Remove one or more elements from the head or tail of the list

**Parameters:**

```C++
struct RemListReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require bool atHead = true;   //remove from the head if it is true, otherwise from the tail
  4 require long count;  //indicate how many records to be removed
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | delete successfully


# <a id="set-1"></a> getSet

```C++
int getSet(const GetSetReq &req, BatchEntry &rsp)
```

get all members from a set

**Parameters:**

```C++
struct GetSetReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require string field;  //the set of fields to be queried, multiple fields separated by ',' such as "a, b", "*" for all fields
  4 require string idcSpecified = "";  //idc area
};

struct BatchEntry
{
  1 require vector<map<string, string>> entries;  //result set
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | query successfully

# <a id="set-2"></a> addSet

```C++
int addSet(const AddSetReq &req)
```

Add one member to a set

**Parameters:**

```C++
struct AddSetReq
{
  1 require string moduleName;  //module name
  2 require AddSetKeyValue value;  
};

struct AddSetKeyValue
{
  1 require string mainKey;  
  2 require map<string, UpdateValue> data;  
  3 require int expireTime;  //expiration time
  4 require bool dirty = true;  //whether to make it dirty data
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_PARAM_DIGITAL_ERR | field error, such as a field with a numeric type containing non-numeric characters
 ET_PARAM_TOO_LONG | the length of field exceeds limit
 ET_PARAM_OP_ERR	| op error, only support SET
 ET_PARAM_NOT_EXIST | field does not exist
 ET_DATA_EXIST | data already exists
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | add successfully

# <a id="set-3"></a> delSet

```C++
int delSet(const DelSetReq &req)
```

remove one member from a set

**Parameters:**

```C++
struct DelSetReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require vector<Condition> cond;  
};

struct Condition
{
  1 require string fieldName;  //all fields, excluding mainKey
  2 require Op op;  //only suuport EQ
  3 require string value;  
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_PARAM_REDUNDANT | multiple fields with the same name
 ET_PARAM_MISSING | some fields are missing
 ET_DB_ERR | access database error
 ET_PARAM_OP_ERR	| only support EQ
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | remove successfully

# <a id="zset-1"></a> getZSetScore

```C++
int getZSetScore(const GetZsetScoreReq &req, double &score)
```

Get the score associated with the given memeber in a sorted set

**Parameters:**

```C++
struct GetZsetScoreReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require vector<Condition> cond;  
  4 require string idcSpecified = "";  //idc area
};

struct Condition
{
  1 require string fieldName;  //all fields, excluding mainKey
  2 require Op op;  //only support EQ
  3 require string value;  
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_PARAM_REDUNDANT | multiple fields with the same name
 ET_PARAM_MISSING | some fields are missing
 ET_DB_ERR | access database error
 ET_PARAM_OP_ERR	| only support EQ
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_NO_DATA | eligible member does not exist
 ET_SUCC | success


# <a id="zset-2"></a> getZSetPos

```C++
int getZSetPos(const GetZsetPosReq &req, long &pos)
```

Get the index of the member of the specified condition in a sorted set

**Parameters:**

```C++
struct GetZsetPosReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require vector<Condition> cond; //specified conditions 
  4 require bool positiveOrder = true;  //positive sequence lookup if if is true, otherwise reverse sequence lookup
  5 require string idcSpecified = "";  //idc area
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_PARAM_REDUNDANT | multiple fields with the same name
 ET_PARAM_MISSING | some fields are missing
 ET_DB_ERR | access database error
 ET_PARAM_OP_ERR	| only support EQ
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_NO_DATA | eligible member does not exist
 ET_SUCC | success


# <a id="zset-3"></a> getZSetByPos

```C++
int getZSetByPos(const GetZsetByPosReq &req, BatchEntry &rsp)
```

Get a range of memebers in a sorted set by index

**Parameters:**

```C++
struct GetZsetByPosReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require string field;  //the set of fields to be queried, multiple fields separated by ',' such as "a, b", "*" for all fields
  4 require long start;  
  5 require long end;  
  6 require bool positiveOrder = true; //results are sorted in ascending order by score if it is true, otherwise sorted in descending order of score
  7 require string idcSpecified = "";  //idc area
};

struct BatchEntry
{
  1 require vector<map<string, string>> entries;
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_DB_ERR | access database error
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | success


 
# <a id="zset-4"></a> getZSetByScore

```C++
int getZSetByScore(const GetZsetByScoreReq &req, BatchEntry &rsp)
```

Get a range of memebers in a sorted set by score

**Parameters:**

```C++
struct GetZsetByScoreReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require string field;  //the set of fields to be queried, multiple fields separated by ',' such as "a, b", "*" for all fields
  4 require double minScore;
  5 require double maxScore
  6 require string idcSpecified = "";  //idc area
};

struct BatchEntry
{
  1 require vector<map<string, string>> entries;
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_DB_ERR | access database error
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | success
 

# <a id="zset-5"></a> addZSet

```C++
int addZSet(const AddZSetReq &req)
```

Add one member to a sorted set, or update its score if it already exists

**Parameters:**

```C++
AddZSetReq
{
  1 require string moduleName;  //module name
  2 require AddSetKeyValue value;  
  3 require double score;
};

struct AddSetKeyValue
{
  1 require string mainKey;  
  2 require map<string, UpdateValue> data; 
  3 require int expireTime;  //expiration time 
  4 require bool dirty = true;  //whether to make it dirty data
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_PARAM_DIGITAL_ERR | field error, such as a field with a numeric type containing non-numeric characters
 ET_PARAM_TOO_LONG | the length of field exceeds limit
 ET_PARAM_OP_ERR | op error, only support SET
 ET_PARAM_NOT_EXIST | field does not exist
 ET_MEM_FULL | no free space in memory
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | success


# <a id="zset-6"></a> incScoreZSet

```C++
int incScoreZSet(const IncZSetScoreReq &req)
```

Modify the score of a member in a sorted set, if the member does not exist, it is added with scoreDiff as its score(as if its previous score was 0.0)

**Parameters:**

```C++
struct IncZSetScoreReq
{
  1 require string moduleName;  //module name
  2 require AddSetKeyValue value;  //
  3 require double scoreDiff;  //The increment of the score, which can be negative
};

struct AddSetKeyValue
{
  1 require string mainKey;  
  2 require map<string, UpdateValue> data;
  3 require int expireTime;  //expiration time 
  4 require bool dirty = true;  //whether to make it dirty data
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_PARAM_DIGITAL_ERR | field error, such as a field with a numeric type containing non-numeric characters
 ET_PARAM_TOO_LONG | the length of field exceeds limit
 ET_PARAM_OP_ERR | op error, only support SET
 ET_PARAM_NOT_EXIST | field does not exist
 ET_MEM_FULL | no free space in memory
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | success


# <a id="zset-7"></a> delZSet

```C++
int delZSet(const DelZSetReq &req)
```

Remove a member in a sorted set that meets the specified conditions

**Parameters:**

```C++
struct DelZSetReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require vector<Condition> cond;
};

struct Condition
{
  1 require string fieldName;  //all fields, excluding mainKey
  2 require Op op;  //only support EQ
  3 require string value;  
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_PARAM_REDUNDANT | multiple fields with the same name
 ET_PARAM_MISSING | some fields are missing
 ET_DB_ERR | access database error
 ET_PARAM_OP_ERR	| only support EQ
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | delete successfully


# <a id="zset-8"></a> delZSetByScore

```C++
int delZSetByScore(const DelZSetByScoreReq &req)
```

Remove members in a sorted set within the given scores

**Parameters:**

```C++
struct DelZSetByScoreReq
{
  1 require string moduleName;  //module name
  2 require string mainKey;  
  3 require double minScore;
  4 require double maxScore;
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_DB_ERR | access database error
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | delete successfully


# <a id="zset-9"></a> updateZSet

```C++
int updateZSet(const UpdateZSetReq &req)
```

Update a member in a sorted set that meets the specified conditions

**Parameters:**

```C++
struct UpdateZSetReq
{
  1 require string moduleName;  //module name
  2 require AddSetKeyValue value;  //data used for updating
  3 require vector<Condition> cond;
};

struct AddSetKeyValue
{
  1 require string mainKey;  
  2 require map<string, UpdateValue> data;  //data used for updating
  3 require int expireTime;  //expiration time 
  4 require bool dirty = true;  //whether to make it dirty data
};

struct Condition
{
  1 require string fieldName;  //all fields, excluding mainKey
  2 require Op op;  //only support EQ
  3 require string value;  
};
```

**Returns**：

Returns | Description
------------------ | ----------------
 ET_MODULE_NAME_INVALID | module error
 ET_KEY_AREA_ERR	| The current Key should not be assigned to this node, need to update the routing table to re-access
 ET_FORBID_OPT | data is being migrated
 ET_SERVER_TYPE_ERR | service is not provided since it is a slave
 ET_PARAM_DIGITAL_ERR | field error, such as a field with a numeric type containing non-numeric characters
 ET_PARAM_TOO_LONG | the length of field exceeds limit
 ET_PARAM_NOT_EXIST | field does not exist
 ET_PARAM_MISSING | some fields are missing
 ET_PARAM_REDUNDANT | multiple fields with the same name
 ET_PARAM_OP_ERR | only support EQ
 ET_DB_ERR | access database error
 ET_KEY_INVALID | mainKey is invalid
 ET_INPUT_PARAM_ERROR | mainKey is empty
 ET_SYS_ERR	| system error
 ET_SUCC | update successfully