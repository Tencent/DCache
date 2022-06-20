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

const RouterDao = require('./dao');

const RouterService = {};


RouterService.createRouter = async function (params) {
  const {
    apply_id,
    server_name,
    server_ip,
    template_file,
    router_db_name,
    router_db_ip,
    router_db_port,
    router_db_user,
    router_db_pass,
    create_person,
  } = params;
  return RouterDao.createRouter({
    apply_id,
    server_name,
    server_ip,
    template_file,
    router_db_name,
    router_db_ip,
    router_db_port,
    router_db_user,
    router_db_pass,
    create_person,
  });
};

RouterService.createOrUpdate = function (whereProperties, {
  apply_id,
  server_name,
  server_ip,
  template_file,
  router_db_name,
  router_db_ip,
  router_db_port,
  router_db_user,
  router_db_pass,
  create_person,

}) {
  return RouterDao.createOrUpdate(whereProperties, {
    apply_id,
    server_name,
    server_ip,
    template_file,
    router_db_name,
    router_db_ip,
    router_db_port,
    router_db_user,
    router_db_pass,
    create_person,
  });
};

RouterService.find = function (apply_id ) {
  return RouterDao.findOne({ where: { apply_id: apply_id } });
};

RouterService.update = function (Router) {
  return RouterDao.update(Router);
};

RouterService.removeRouter = async function ({ server_name }) {
  return RouterDao.destroy({ where: { server_name } });
};

RouterService.findByServerName = function ({ serverName }) {
  return RouterDao.findOne({ where: { server_name: serverName } });
};


module.exports = RouterService;
