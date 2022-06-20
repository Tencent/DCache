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

const logger = require('../../logger');

const assert = require('assert');

const {
  getConfig,
  addConfig,
  deleteConfig,
  editConfig,
  getServerConfigItemList,
  getServerNodeConfigItemList,
  addServerConfigItem,
  deleteServerConfigItem,
  updateServerConfigItem,
  updateServerConfigItemBatch,
  deleteServerConfigItemBatch,
} = require('./service.js');

const {
  getModuleConfigByName
} = require('./../moduleConfig/service.js');


const controller = {};
controller.getConfig = async function (ctx) {
  try {
    const configList = await getConfig();
    ctx.makeResObj(200, '', configList);
  } catch (err) {
    logger.error('[getConfig]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.addConfig = async function (ctx) {
  try {
    const {
      remark,
      item,
      path,
      reload,
      period,
    } = ctx.paramsObj;
    const res = await addConfig({
      remark,
      item,
      path,
      reload,
      period,
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[addConfig]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.deleteConfig = async function (ctx) {
  try {
    const {
      id
    } = ctx.paramsObj;
    const res = await deleteConfig({
      id
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[deleteConfig]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.editConfig = async function (ctx) {
  try {
    const {
      id,
      remark,
      item,
      path,
      reload,
      period,
    } = ctx.paramsObj;
    const res = await editConfig({
      id,
      remark,
      item,
      path,
      reload,
      period,
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[deleteConfig]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.getModuleConfig = async function (ctx) {
  try {
    const {
      appName,
      moduleName
    } = ctx.paramsObj;
    const res = await getServerConfigItemList({
      appName,
      moduleName
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[getModuleConfig]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.getServerConfig = async function (ctx) {
  try {
    const {
      appName,
      moduleName,
      serverName,
      nodeName
    } = ctx.paramsObj;
    // const moduleInfo = await getModuleConfigByName({ moduleName, queryAppBase: ['name', 'set_area'] });
    // assert(moduleInfo, '#cache.config.noModuleExist#');
    // const appName = moduleInfo.AppBase.name;
    const res = await getServerConfigItemList({
      appName,
      moduleName,
      serverName,
      nodeName,
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[getServerConfig]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.getServerNodeConfig = async function (ctx) {
  try {
    const {
      serverName,
      nodeName
    } = ctx.paramsObj;
    const res = await getServerNodeConfigItemList({
      serverName,
      nodeName
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[getServerNodeConfigItemList]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.addServerConfigItem = async function (ctx) {
  try {
    const {
      appName,
      moduleName,
      serverName,
      nodeName,
      configValue,
      itemId
    } = ctx.paramsObj;
    // 有appName, moduleName 的是模块添加配置，  只有 serverName 和 nodeName 的是服务添加配置
    const res = await addServerConfigItem({
      appName,
      moduleName,
      serverName,
      nodeName,
      configValue,
      itemId
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[addServerConfigItem]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.deleteServerConfigItem = async function (ctx) {
  try {
    const {
      id
    } = ctx.paramsObj;
    const res = await deleteServerConfigItem({
      indexId: id
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[deleteServerConfigItem]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};


controller.updateServerConfigItem = async function (ctx) {
  try {
    const {
      id,
      configValue
    } = ctx.paramsObj;
    const res = await updateServerConfigItem({
      indexId: id,
      configValue
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[updateServerConfigItem]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.updateServerConfigItemBatch = async function (ctx) {
  try {
    const {
      serverConfigList
    } = ctx.paramsObj;
    const res = await updateServerConfigItemBatch({
      serverConfigList
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[updateServerConfigItemBatch]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};

controller.deleteServerConfigItemBatch = async function (ctx) {
  try {
    const {
      serverConfigList
    } = ctx.paramsObj;
    const res = await deleteServerConfigItemBatch({
      serverConfigList
    });
    ctx.makeResObj(200, '', res);
  } catch (err) {
    logger.error('[deleteServerConfigItemBatch]: ', err);
    ctx.makeResObj(500, err.message, {});
  }
};


module.exports = controller;