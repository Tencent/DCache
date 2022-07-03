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

const logger = require('../../logger');

const {
  DCacheOptPrx
} = require('../../common/rpc');
const {
  DCacheOptStruct
} = require('../../common/rpc/struct');

const ModuleService = require('./service.js');
const ApplyService = require('../apply/service');

const ModuleController = {
  // 服务名 DCache.xxx, 不填表示查询模块下所有服务合并统计数据，填"*"表示列出所有服务的独立数据
  async queryProperptyData(ctx) {
    try {
      const {
        thedate,
        predate,
        startshowtime,
        endshowtime,
        moduleName,
        serverName,
        get
      } = ctx.paramsObj;

      const option = {
        moduleName,
        serverName,
        date: [thedate, predate],
        startTime: startshowtime,
        endTime: endshowtime,
        get,
      };

      const rsp = await ModuleService.queryProperptyData(option);

      ctx.makeResObj(200, '', rsp);
    } catch (err) {
      logger.error('[queryProperptyData]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async addModuleBaseInfo(ctx) {
    try {
      let {
        follower,
        apply_id,
        module_name,
        update
      } = ctx.paramsObj;
      const create_person = ctx.uid;

      const modify_time = new Date();
      const item = await ModuleService.addModuleBaseInfo({
        apply_id,
        module_name,
        follower,
        update,
        create_person,
        modify_time,
      });
      ctx.makeResObj(200, '', item);
    } catch (err) {
      logger.error('[addModuleBaseInfo]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async hasModuleInfo(ctx) {
    try {
      const module_name = ctx.paramsObj.module_name;

      const rootNode = await ApplyService.getCacheList();

      let hasModule = false;
      for (let i = 0; i < rootNode.length; i++) {

        if (module_name.indexOf(rootNode[i].name) == 0) {
          for (let j = 0; j < rootNode[i].children.length; j++) {
            const item = rootNode[i].children[j];
            if (item.moduleName == module_name) {
              hasModule = true;
              break;
            }
          }
        }

        if (hasModule) {
          break;
        }
      }

      ctx.makeResObj(200, '', {
        hasModule
      });
    } catch (err) {
      logger.error('[hasModuleInfo]:', err);
      ctx.makeErrResObj();
    }

  },
  async getModuleInfo(ctx) {
    try {
      const {
        moduleId
      } = ctx.paramsObj;
      const item = await ModuleService.getModuleInfo({
        id: moduleId
      });
      ctx.makeResObj(200, '', item);
    } catch (err) {
      logger.error('[getModuleInfo]:', err);
      ctx.makeErrResObj();
    }
  },

  /**
   * 获取模块信息
   * @returns {Promise.<void>}
   */
  async getModuleStruct(ctx) {
    try {
      const {
        appName,
        moduleName
      } = ctx.paramsObj;
      const option = new DCacheOptStruct.ModuleStructReq();
      option.readFromObject({
        appName,
        moduleName,
      });
      const args = await DCacheOptPrx.getModuleStruct(option);
      const {
        __return,
        rsp,
        rsp: {
          errMsg
        },
      } = args;
      assert(__return === 0, errMsg);
      ctx.makeResObj(200, '', rsp);
    } catch (err) {
      logger.error('[getModuleStruct]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
};

module.exports = ModuleController;