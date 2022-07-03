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

const cwd = process.cwd();

const Sequelize = require('sequelize');

const _ = require('lodash');

const fs = require('fs-extra');

const webConf = require('../../config/webConf');

const logger = require('../../logger');

const Db = {};

const databases = ['db_cache_web'];

databases.forEach((database) => {
  const {
    host,
    port,
    user,
    password,
    charset,
    pool,
  } = webConf.dbConf;

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
      return (offset >= 0 ? '+' : '-') + (Math.abs(parseInt(offset / 60)) + '').padStart(2, '0') + ':' + (offset % 60 + '').padStart(2, '0');
    })() //获取当前时区并做转换
  });

  // 测试是否连接成功
  (async function () {
    try {
      await sequelize.authenticate();
    } catch (err) {
      /* Ignore */ }
  }());

  const tableObj = {};
  const dbModelsPath = `${__dirname}/${database}_models`;
  const dbModels = fs.readdirSync(dbModelsPath);
  dbModels.forEach(async (dbModel) => {
    const tableName = dbModel.replace(/\.js$/g, '');
    try {
      tableObj[_.camelCase(tableName)] = sequelize.import(`${dbModelsPath}/${tableName}`);
      // sync 无表创建表， alter 新增字段
      // tableObj[_.camelCase(tableName)].sync();
      if (webConf.webConf.alter) {
        await tableObj[_.camelCase(tableName)].sync({
          alter: true
        });
      }

      // logger.info('sync ' + database + '.' + tableName + ' succ');

    } catch (e) {
      logger.info('sync ' + database + '.' + tableName + ' error:', e);
    }
  });

  Db[database] = tableObj;
  Db[database].sequelize = sequelize;
  sequelize.sync();
});

const {
  tApplyAppBase
} = Db.db_cache_web;
const {
  tApplyAppRouterConf
} = Db.db_cache_web;
const {
  tApplyAppProxyConf
} = Db.db_cache_web;
tApplyAppBase.hasOne(tApplyAppRouterConf, {
  foreignKey: 'apply_id',
  as: 'Router',
});
tApplyAppBase.hasMany(tApplyAppProxyConf, {
  foreignKey: 'apply_id',
  as: 'Proxy',
});

const {
  tApplyCacheModuleBase
} = Db.db_cache_web;
const {
  tApplyCacheModuleConf
} = Db.db_cache_web;
const {
  tApplyCacheServerConf
} = Db.db_cache_web;

tApplyCacheModuleConf.belongsTo(tApplyAppBase, {
  foreignKey: 'apply_id',
  as: 'AppBase',
});

tApplyCacheModuleConf.belongsTo(tApplyCacheModuleBase, {
  foreignKey: 'module_id',
  as: 'ModuleBase',
});



module.exports = Db;