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

const ApplyDao = require('./dao');
const RouterService = require('../router/service');
const ProxyService = require('../proxy/service');
const ModuleConfigService = require('../moduleConfig/service');
const TreeService = require('../tree/TreeService');

const {
  DCacheOptPrx
} = require('../../common/rpc');
const {
  DCacheOptStruct
} = require('../../common/rpc/struct');
const ApplyService = {};

ApplyService.hasApply = async function ({
  name
}) {
  const item = await ApplyDao.findOne({
    where: {
      name
    }
  });
  if (item && item.status == 2) {
    return true;
  }
  return false;
}


ApplyService.rootNode = []

ApplyService.buildCacheList = async () => {

  let serverList = [];
  const treeNodeMap = {};

  // 获取 dache 的router、proxy 服务
  let timeout = DCacheOptPrx.getTimeout();
  DCacheOptPrx.setTimeout(10000);

  const args = await DCacheOptPrx.loadCacheApp();

  DCacheOptPrx.setTimeout(timeout);

  const {
    __return,
    cacheApps
  } = args;

  if (__return != 0) {
    return ApplyService.rootNode;
  }

  ApplyService.rootNode = [];

  // 如果没有 proxy、router 删除该应用
  cacheApps.forEach((item) => {

    const applyServer = [];

    if (TreeService.hasDCacheServerName(item.routerName)) {
      applyServer.push({
        name: item.routerName,
        id: `1DCache.5${item.routerName}`,
        pid: `1${item.name}`,
        is_parent: false,
        open: false,
        children: [],
        serverType: 'router',
      });
    }

    if (TreeService.hasDCacheServerName(item.proxyName)) {
      applyServer.push({
        name: item.proxyName,
        id: `1DCache.5${item.proxyName}`,
        pid: `1${item.name}`,
        is_parent: false,
        open: false,
        children: [],
        serverType: 'proxy',
      });
    }

    // 获取 cache 服务
    Object.keys(item.cacheModules).forEach(async (key) => {

      let value = item.cacheModules[key];

      let cacheNodeFloder = {
        name: key,
        id: `1${item.name}.6${key}`,
        pid: `1${item.name}`,
        is_parent: false,
        open: false,
        children: [],
        moduleName: key,
      };

      if (value.cacheServer && value.cacheServer.length > 0) {
        // console.log(value);

        value.cacheServer.forEach(server => {
          cacheNodeFloder.children.push({
            name: server,
            id: `1Dcache.5${server}`,
            pid: `1${value.moduleName}`,
            is_parent: false,
            open: false,
            children: [],
            serverType: value.cacheNode.indexOf(value.moduleName + "KVCacheServer") != -1?"kvcache": "mkvcache", //'dcache',
          })
        })
      }

      // 看看该应用是否已经有了存放 cache 的模块的节点
      if (value.dbAccessServer && value.dbAccessServer.length > 0) {
        const has = TreeService.hasDCacheServerName(value.dbAccessServer);

        if (has) {
          cacheNodeFloder.children.push({
            name: value.dbAccessServer,
            id: `1Dcache.5${value.dbAccessServer}`,
            pid: `1${item.name}`,
            is_parent: false,
            open: false,
            children: [],
            serverType: 'dbaccess',
          });
        }
      }

      if (cacheNodeFloder.children.length > 0) {
        applyServer.push(cacheNodeFloder);
      }

    });

    serverList = serverList.concat(applyServer);

    return true;
  });

  serverList.forEach((server) => {
    TreeService.parents(treeNodeMap, server, ApplyService.rootNode);
  });

  return ApplyService.rootNode;
}

ApplyService.getCacheList = async () => {
  if (ApplyService.rootNode.length == 0) {
    await ApplyService.buildCacheList();
  }

  return ApplyService.rootNode;
}

ApplyService.overwriteApply = async function ({
  idc_area,
  set_area,
  admin,
  name,
  create_person,
}) {
  // 如果注册过，但是没有安装成功， 就覆盖掉
  const data = await ApplyDao.createOrUpdate(['name'], {
    idc_area,
    set_area,
    admin,
    name,
    create_person,
    status: 1,
  });

  function decamelize(string, options) {
    options = options || {};
    var separator = options.separator || '_';
    var split = options.split || /(?=[A-Z])/;

    return string.split(split).join(separator).toLowerCase().replace("__", "_");
  };
  // 创建应用 RouterService
  let routerOption = {
    apply_id: data.id,
    server_name: `${data.name}RouterServer`,
    server_ip: '',
    template_file: '',
    router_db_name: `db_cache_${decamelize(data.name)}`,
    router_db_ip: ``,
    router_db_port: '3306',
    router_db_user: '',
    router_db_pass: '',
    create_person,
    status: 1,
  };

  const router = await RouterService.find(data.id);

  if (router) {
    routerOption.template_file = router.template_file;
    routerOption.router_db_name = router.router_db_name;
    routerOption.router_db_ip = router.router_db_ip;
    routerOption.router_db_port = router.router_db_port;
    routerOption.router_db_user = router.router_db_user;
    // routerOption.router_db_pass = router.router_db_pass;
  }

  await RouterService.createOrUpdate(['apply_id'], routerOption);

  // 创建 idc_area ProxyService
  let proxyOption = {
    apply_id: data.id,
    server_name: `${data.name}ProxyServer`,
    server_ip: '',
    template_file: '',
    idc_area: data.idc_area,
    create_person,
    status: 1,
  };
  await ProxyService.createOrUpdate(['apply_id', 'idc_area'], proxyOption);
  // 创建 set_area ProxyService
  const result = [];
  for (let i = 0; i < set_area.length; i++) {
    proxyOption = {
      apply_id: data.id,
      server_name: `${data.name}ProxyServer`,
      server_ip: '',
      template_file: '',
      idc_area: set_area[i],
      create_person,
      status: 1,
    };
    result.push(ProxyService.createOrUpdate(['apply_id', 'idc_area'], proxyOption));
  }

  await Promise.all(result);
  return data;

};


ApplyService.addApply = async function ({
  idc_area,
  set_area,
  admin,
  name,
  create_person,
}) {
  const item = await ApplyDao.findOne({
    where: {
      name
    }
  });
  if (item && item.status === 2) {
    // 如果是安装成功的， 提醒应用存在
    throw new Error('#apply.hasExist#');
  } else {
    return await ApplyService.overwriteApply({
      idc_area,
      set_area,
      admin,
      name,
      create_person
    });
  }
};

ApplyService.getRouterDb = async function () {
  return await ApplyDao.findRouterDbAll();
};

ApplyService.getRouterDbById = async function (id) {
  return await ApplyDao.findRouter(id);
};

ApplyService.getApplyById = function (applyId) {
  return ApplyDao.findOne({
    where: {
      id: applyId
    }
  });
};


ApplyService.getApply = function ({
  applyId,
  queryRouter,
  queryProxy
}) {
  return ApplyDao.findOne({
    where: {
      id: +applyId
    },
    queryRouter,
    queryProxy
  });
};

ApplyService.findApplyByName = function ({
  appName
}) {
  return ApplyDao.findOne({
    where: {
      name: appName
    }
  });
};

ApplyService.getApplyList = function (options = {}) {
  return ApplyDao.findAll(options);
};

ApplyService.saveRouterProxy = async function ({
  Proxy,
  Router
}) {
  const result = [];
  for (let i = 0; i < Proxy.length; i++) {
    result.push(ProxyService.update(Proxy[i]));
  }
  await Promise.all(result);
  await RouterService.update(Router);
  return {};
};

ApplyService.removeApply = async function ({
  id
}) {
  return ApplyDao.destroy({
    where: {
      id
    }
  });
};

ApplyService.hasModule = async function ({
  serverType,
  serverName
}) {
  // RouterService
  // ProxyService
  let applyId = null;
  if (serverType === 'router') {
    const routerServer = await RouterService.findByServerName({
      serverName
    });
    if (routerServer) applyId = routerServer.get('apply_id');
  } else if (serverType === 'proxy') {
    const proxyServer = await ProxyService.findByServerName({
      serverName
    });
    if (proxyServer) applyId = proxyServer.get('apply_id');
  }
  if (!applyId) throw new Error('不存在该应用');


  const moduleServer = await ModuleConfigService.findOne({
    apply_id: applyId
  });
  // let moduleServer = await serverConfigService.findByApplyId({applyId});
  return !!moduleServer;
};


module.exports = ApplyService;