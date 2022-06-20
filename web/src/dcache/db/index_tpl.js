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

const Sequelize = require('sequelize');
const _ = require('lodash');
const fs = require('fs-extra');

const logger = require('../../logger');
const webConf = require('../../config/webConf');

module.exports = {
  async conn(config) {
    const that = module.exports
    const {
      host,
      database,
      port,
      user,
      password,
      charset,
      pool
    } = config;
    // 初始化sequelize
    const sequelize = new Sequelize(database, user, password, {
      host,
      port,
      dialect: 'mysql',
      dialectOptions: {
        charset,
      },
      logging(sqlText) {
        // logger.sql(sqlText);
      },
      pool: {
        max: pool.max || 10,
        min: pool.min || 0,
        idle: pool.idle || 10000,
      },
      timezone: (() => {
        let offset = 0 - new Date().getTimezoneOffset();
        return (offset >= 0 ? '+' : '-') + (Math.abs(offset / 60) + '').padStart(2, '0') + ':00';
      })() //获取当前时区并做转换
    });

    // 测试是否连接成功
    try {
      await sequelize.authenticate();
    } catch (err) {
      throw new Error(err);
    }

    const tableObj = {};
    const dbModelsPath = `${__dirname}/db_cache_tpl_models`;
    const dbModels = fs.readdirSync(dbModelsPath);
    dbModels.forEach(async (dbModel) => {
      const tableName = dbModel.replace(/\.js$/g, '');
      try {
        tableObj[_.camelCase(tableName)] = sequelize.import(`${dbModelsPath}/${tableName}`);
        // sync 无表创建表， alter 新增字段
        // if (webConf.webConf.alter) {
        //   await tableObj[_.camelCase(tableName)].sync({ alter: true });
        // }

      } catch (e) {
        logger.info('sync ' + database + '.' + tableName + ' error:', e);
      }
    });

    that[database] = tableObj;
    that[database].sequelize = sequelize;
    // await sequelize.sync();
  }
}