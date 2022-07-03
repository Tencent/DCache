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
const path = require('path');
const assert = require('assert');

const {
  DCacheOptPrx
} = require('../../common/rpc');
const {
  DCacheOptStruct
} = require('../../common/rpc/struct');

const Dao = require('./dao.js');

const Service = {};

/**
 * 从 opt 获取 cache 服务列表
 * @param appName
 * @param moduleName
 * @returns {Promise<void>}
 * struct CacheServerListRsp
 * {
 *    0 require string errMsg;
 *    //1 require string moduleName;
 *    1 require vector<CacheServerInfo> cacheServerList;
 *    2 require DCacheType cacheType;
 * };
 * serverType; // M-主机, S-备机, I-镜像
 */
Service.getCacheServerListFromOpt = async function ({
  appName,
  moduleName
}) {
  const option = new DCacheOptStruct.CacheServerListReq();
  option.readFromObject({
    appName,
    moduleName
  });
  const {
    __return,
    rsp: {
      errMsg,
      cacheServerList,
      cacheType
    }
  } = await DCacheOptPrx.getCacheServerList(option);
  cacheServerList.forEach(item => Object.assign(item, {
    appName,
    moduleName,
    cacheType
  }));
  assert(__return === 0, errMsg);
  return cacheServerList;
};

Service.addServerConfig = function (option = []) {
  return Dao.add(option);
};

Service.addExpandServer = function (expandServer) {
  const option = expandServer.map(item => ({
    area: item.area,
    apply_id: item.apply_id,
    module_name: item.module_name,
    group_name: item.group_name,
    server_name: item.server_name,
    server_ip: item.server_ip,
    server_type: item.server_type,
    template_name: item.template_name,
    memory: item.memory,
    shmKey: item.shmKey,
    idc_area: item.idc_area,
    status: 0,
    modify_person: item.modify_person,
    modify_time: item.modify_time,
    is_docker: item.is_docker,
  }));
  return Dao.add(option);
};

Service.getServerConfigInfo = function ({
  moduleId
}) {
  return Dao.findOne({
    where: {
      module_id: moduleId
    }
  });
};

Service.getCacheServerList = function ({
  moduleName,
  attributes = [],
  queryBase = [],
  queryModule = [],
}) {
  return Dao.findAll({
    where: {
      module_name: moduleName
    },
    attributes,
    queryBase,
    queryModule,
  });
};
Service.removeServer = function ({
  server_name
}) {
  return Dao.destroy({
    where: {
      server_name
    }
  });
};

Service.findByApplyId = function ({
  applyId
}) {
  return Dao.findOne({
    where: {
      apply_id: applyId
    }
  });
};

Service.findOne = function ({
  where
}) {
  return Dao.findOne({
    where
  });
};

Service.update = function ({
  where = {},
  values = {}
}) {
  return Dao.update({
    where,
    values
  });
};

module.exports = Service;