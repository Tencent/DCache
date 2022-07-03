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

const DbAccessService = require('./service.js');

const DbAccessController = {
  // 服务名 DCache.xxx, 不填表示查询模块下所有服务合并统计数据，填"*"表示列出所有服务的独立数据
  async loadAccessDb(ctx) {
    try {
      const access = await DbAccessService.getAccessDb();

      let data = [];

      access.forEach((item) => {
        data.push({
          id: item.dataValues.id,
          access_db_flag: item.dataValues.access_db_flag
        });
      })

      ctx.makeResObj(200, '', data);
    } catch (err) {
      logger.error('[loadAccessDb]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
};

module.exports = DbAccessController;