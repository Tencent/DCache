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

const DbAccessDao = require('./dao');

const DbAccessService = {};


DbAccessService.getAccessDb = async function () {
  return await DbAccessDao.findAccessDbAll();
};

DbAccessService.getAccessDbById = async function (id) {
  return await DbAccessDao.findAccess(id);
};

DbAccessService.createDbAccess = function ({
  module_id, servant, dbaccess_ip, isSerializated, db_num, db_prefix, table_num, table_prefix, db_host, db_port, db_pwd, db_user, db_charset, create_person
}) {
  return DbAccessDao.createDbAccess({
    module_id, servant, dbaccess_ip, isSerializated, db_num, db_prefix, table_num, table_prefix, db_host, db_port, db_pwd, db_user, db_charset, create_person
  });
};

DbAccessService.createOrUpdate = function (whereProperties, {
  module_id, servant, dbaccess_ip, isSerializated, db_num, db_prefix, table_num, table_prefix, db_host, db_port, db_pwd, db_user, db_charset, create_person
}) {
  return DbAccessDao.createOrUpdate(whereProperties, {
    module_id, servant, dbaccess_ip, isSerializated, db_num, db_prefix, table_num, table_prefix, db_host, db_port, db_pwd, db_user, db_charset, create_person
  });
};

DbAccessService.findByModuleId = function ({ moduleId }) {
  return DbAccessDao.findOne({ where: { module_id: moduleId } });
};


module.exports = DbAccessService;
