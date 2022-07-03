/**
 * Tencent is pleased to support the open source community by making Tars available.
 *
 * Copyright (C) 2016THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software distributed
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

const cwd = process.cwd();
const path = require('path');
const assert = require('assert');


const TarsStream = require('@tars/stream');
const {
  DCacheOptPrx
} = require('../../common/rpc');
const {
  DCacheOptStruct
} = require('../../common/rpc/struct');

const Service = {};

module.exports = Service;

Service.getConfig = async function () {
  const option = new DCacheOptStruct.CacheConfigReq();
  option.readFromObject({
    id: '',
    remark: '',
    item: '',
    path: '',
    reload: '',
    period: '',
  });
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.getCacheConfigItemList(option);
  assert(__return === 0, errMsg);
  return configItemList;
};

Service.addConfig = async function ({
  remark,
  item,
  path,
  reload,
  period,
}) {
  const option = new DCacheOptStruct.CacheConfigReq();
  option.readFromObject({
    remark,
    item,
    path,
    reload,
    period,
  });
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.addCacheConfigItem(option);
  assert(__return === 0, errMsg);
  return configItemList;
};

Service.deleteConfig = async function ({
  id
}) {
  const option = new DCacheOptStruct.CacheConfigReq();
  option.readFromObject({
    id,
  });
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.deleteCacheConfigItem(option);
  assert(__return === 0, errMsg);
  return configItemList;
};

Service.editConfig = async function ({
  id,
  remark,
  item,
  path,
  reload,
  period,
}) {
  const option = new DCacheOptStruct.CacheConfigReq();
  option.readFromObject({
    id,
    remark,
    item,
    path,
    reload,
    period,
  });
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.updateCacheConfigItem(option);
  assert(__return === 0, errMsg);
  return configItemList;
};

Service.getServerConfigItemList = async function ({
  appName = '',
  moduleName = '',
  serverName = '',
  nodeName = '',
  itemId = '',
  configValue = '',
  configFlag = '',
  lastUser = '',
  indexId = '',
}) {
  const option = new DCacheOptStruct.ServerConfigReq();
  option.readFromObject({
    appName,
    moduleName,
    serverName: serverName ? `DCache.${serverName}` : '',
    nodeName,
    itemId,
    configValue,
    configFlag,
    lastUser,
    indexId,
  });
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.getServerConfigItemList(option);
  assert(__return === 0, errMsg);
  return configItemList;
};


Service.getServerNodeConfigItemList = async function ({
  appName = '',
  moduleName = '',
  serverName = '',
  nodeName = '',
  itemId = '',
  configValue = '',
  configFlag = '',
  lastUser = '',
  indexId = '',
}) {
  const option = new DCacheOptStruct.ServerConfigReq();
  option.readFromObject({
    appName,
    moduleName,
    serverName: serverName ? `DCache.${serverName}` : '',
    nodeName,
    itemId,
    configValue,
    configFlag,
    lastUser,
    indexId,
  });
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.getServerNodeConfigItemList(option);
  assert(__return === 0, errMsg);
  return configItemList;
};

Service.addServerConfigItem = async function ({
  appName = '',
  moduleName = '',
  serverName = '',
  nodeName = '',
  itemId = '',
  configValue = '',
  configFlag = '0',
  lastUser = 'system',
  indexId = '',
}) {
  const option = new DCacheOptStruct.ServerConfigReq();
  option.readFromObject({
    appName,
    moduleName,
    serverName: serverName ? `DCache.${serverName}` : '',
    nodeName,
    itemId,
    configValue,
    configFlag,
    lastUser,
    indexId,
  });
  try {
    const {
      __return,
      configRsp: {
        errMsg,
        configItemList
      }
    } = await DCacheOptPrx.addServerConfigItem(option);
    assert(__return === 0, errMsg);
    return configItemList;
  } catch (err) {
    throw new Error(err);
  }
};


Service.deleteServerConfigItem = async function ({
  appName = '',
  moduleName = '',
  serverName = '',
  nodeName = '',
  itemId = '',
  configValue = '',
  configFlag = '0',
  lastUser = 'system',
  indexId = '',
}) {
  const option = new DCacheOptStruct.ServerConfigReq();
  option.readFromObject({
    appName,
    moduleName,
    serverName: serverName ? `DCache.${serverName}` : '',
    nodeName,
    itemId,
    configValue,
    configFlag,
    lastUser,
    indexId,
  });
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.deleteServerConfigItem(option);
  assert(__return === 0, errMsg);
  return configItemList;
};


Service.updateServerConfigItem = async function ({
  appName = '',
  moduleName = '',
  serverName = '',
  nodeName = '',
  itemId = '',
  configValue = '',
  configFlag = '0',
  lastUser = 'system',
  indexId = '',
}) {
  const option = new DCacheOptStruct.ServerConfigReq();
  option.readFromObject({
    appName,
    moduleName,
    serverName: serverName ? `DCache.${serverName}` : '',
    nodeName,
    itemId,
    configValue,
    configFlag,
    lastUser,
    indexId,
  });
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.updateServerConfigItem(option);
  assert(__return === 0, errMsg);
  return configItemList;
};

Service.updateServerConfigItemBatch = async function ({
  serverConfigList
}) {
  const defaultOption = {
    appName: '',
    moduleName: '',
    serverName: '',
    nodeName: '',
    itemId: '',
    configValue: '',
    configFlag: '0',
    lastUser: 'system',
    indexId: '',
  };
  const ServerConfigReqOption = new TarsStream.List(DCacheOptStruct.ServerConfigReq);
  const array = [];
  serverConfigList.forEach((configItem) => {
    const option = new DCacheOptStruct.ServerConfigReq();
    const item = Object.assign({}, defaultOption, configItem);
    const {
      appName,
      moduleName,
      serverName,
      nodeName,
      itemId,
      configValue,
      configFlag,
      lastUser,
      indexId,
    } = item;
    option.readFromObject({
      appName,
      moduleName,
      serverName: serverName ? `DCache.${serverName}` : '',
      nodeName,
      itemId,
      configValue,
      configFlag,
      lastUser,
      indexId,
    });
    array.push(option);
  });
  ServerConfigReqOption.readFromObject(array);
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.updateServerConfigItemBatch(ServerConfigReqOption);
  assert(__return === 0, errMsg);
  return configItemList;
};


Service.deleteServerConfigItemBatch = async function ({
  serverConfigList
}) {
  const defaultOption = {
    appName: '',
    moduleName: '',
    serverName: '',
    nodeName: '',
    itemId: '',
    configValue: '',
    configFlag: '0',
    lastUser: 'system',
    indexId: '',
  };
  const ServerConfigReqOption = new TarsStream.List(DCacheOptStruct.ServerConfigReq);
  const array = [];
  serverConfigList.forEach((configItem) => {
    const option = new DCacheOptStruct.ServerConfigReq();
    const item = Object.assign({}, defaultOption, configItem);
    const {
      appName,
      moduleName,
      serverName,
      nodeName,
      itemId,
      configValue,
      configFlag,
      lastUser,
      indexId,
    } = item;
    option.readFromObject({
      appName,
      moduleName,
      serverName: serverName ? `DCache.${serverName}` : '',
      nodeName,
      itemId,
      configValue,
      configFlag,
      lastUser,
      indexId,
    });
    array.push(option);
  });
  ServerConfigReqOption.readFromObject(array);
  const {
    __return,
    configRsp: {
      errMsg,
      configItemList
    }
  } = await DCacheOptPrx.deleteServerConfigItemBatch(ServerConfigReqOption);
  assert(__return === 0, errMsg);
  return configItemList;
};