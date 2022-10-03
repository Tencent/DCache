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
// const path = require('path');

// const cwd = process.cwd();

const Sequelize = require('sequelize');

const _ = require('lodash');

const fs = require('fs-extra');

const webConf = require('../../config/webConf');

const logger = require('../../logger');

const Update = require('./update');

const Db = {};

const databases = ['db_cache_web', 'db_dcache_relation'];

databases.forEach((db) => {

    let databaseConf;

    if (db == "db_cache_web") {
        databaseConf = webConf.dbConf;
    } else {
        databaseConf = webConf.relationDb;
    }
    if (!databaseConf) {
        return;
    }

    const {
        host,
        port,
        user,
        database = db,
        password,
        charset,
        pool,
    } = databaseConf;

    // 初始化sequelize
    const sequelize = new Sequelize(database, user, password, {
        host,
        port,
        dialect: 'mysql',
        charset: charset,
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

    const tableObj = {};
    const dbModelsPath = `${__dirname}/${database}_models`;
    const dbModels = fs.readdirSync(dbModelsPath);
    for (let i = 0; i < dbModels.length; i++) {
        let dbModel = dbModels[i];
        const tableName = dbModel.replace(/\.js$/g, '');
        tableObj[_.camelCase(tableName)] = sequelize.import(`${dbModelsPath}/${tableName}`);
    };

    Db[database] = tableObj;
    Db[database].sequelize = sequelize;

    // 测试是否连接成功
    (async function () {
        try {
            // 初始化sequelize
            const s = new Sequelize("", user, password, {
                host,
                port,
                dialect: 'mysql',
                charset: charset,
                dialectOptions: {
                    charset: charset,
                },
                pool: {
                    max: pool.max || 10,
                    min: pool.min || 0,
                    idle: pool.idle || 10000,
                }
            });

            await s.authenticate();

            await s.query(`CREATE DATABASE IF NOT EXISTS ${database} DEFAULT CHARSET ${charset};`);

            console.log(`${database} authenticate succ, charset:${charset}`);

            if (webConf.webConf.alter) {
                try {

                    for (let o in tableObj) {
                        tableObj[o].sync({
                            alter: true
                        });
                    }

                } catch (e) {
                    console.log('database sync error:', e);
                    process.exit(-1);
                }
            }

            if (database == "db_dcache_relation") {
                await Update.update(tableObj);
            }

        } catch (err) {
            console.log(err);

        }
    }());

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
// const {
//     tApplyCacheServerConf
// } = Db.db_cache_web;

tApplyCacheModuleConf.belongsTo(tApplyAppBase, {
    foreignKey: 'apply_id',
    as: 'AppBase',
});

tApplyCacheModuleConf.belongsTo(tApplyCacheModuleBase, {
    foreignKey: 'module_id',
    as: 'ModuleBase',
});


module.exports = Db;