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

const { tApplyAppDbaccessConf } = require('./../db').db_cache_web;
const Sequelize = require('sequelize');

const DbAccessDao = {};

DbAccessDao.createDbAccess = function ({
  module_id, servant, dbaccess_ip, db_num, db_prefix, table_num, table_prefix, db_host, db_port, db_pwd, db_user, db_charset, create_person
}) {
  return tApplyAppDbaccessConf.create({
    module_id, servant, dbaccess_ip, db_num, db_prefix, table_num, table_prefix, db_host, db_port, db_pwd, db_user, db_charset, create_person
  });
};

DbAccessDao.createOrUpdate = async function (whereProperties, params) {
  // console.log(whereProperties, params);

  try {
    const self = tApplyAppDbaccessConf;
    const where = {};

    whereProperties.forEach((key) => {
      where[key] = params[key];
    });

    // console.log(where);

    let record = await self.findOne({ where });
   
    // console.log(record);

    if (!record) {
      
      record = await self.create(params);
    } else {
      record.update(params);
    }
    return record;
  } catch (err) {
    throw new Error(err);
  }
};

DbAccessDao.update = function ({
  module_id, servant, dbaccess_ip, db_num, db_prefix, table_num, table_prefix, db_host, db_port, db_pwd, db_user, db_charset, create_person, id
}) {
  return tApplyAppDbaccessConf.update({
    module_id, servant, dbaccess_ip, db_num, db_prefix, table_num, table_prefix, db_host, db_port, db_pwd, db_user, db_charset, create_person
  }, { where: { id } });
};

DbAccessDao.destroy = function (option) {
  return tApplyAppDbaccessConf.destroy(option);
};

DbAccessDao.findOne = function ({ where }) {
  return tApplyAppDbaccessConf.findOne({ where });
};


DbAccessDao.findAccessDbAll = async function () {
  return await tApplyAppDbaccessConf.findAll({
    attributes: [[Sequelize.literal('distinct concat(`db_host`, ":", `db_port`, "-", `db_user`)'), 'access_db_flag'], 'id'],
    limit: 1,
    offset: 0,
  });
}

DbAccessDao.findAccess = async function (id) {
  return await tApplyAppDbaccessConf.findOne({ id });
}

module.exports = DbAccessDao;
