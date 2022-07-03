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

const ProxyDao = require('./dao');

const ProxyService = {};

ProxyService.createProxy = function ({
  apply_id, server_name, server_ip, template_file, idc_area, create_person,
}) {
  return ProxyDao.createProxy({
    apply_id, server_name, server_ip, template_file, idc_area, create_person,
  });
};
ProxyService.createOrUpdate = function (whereProperties, {
  apply_id, server_name, server_ip, template_file, idc_area, create_person,
}) {
  return ProxyDao.createOrUpdate(whereProperties, {
    apply_id,
    server_name,
    server_ip,
    template_file,
    idc_area,
    create_person,
  });
};


ProxyService.update = async function (Proxy) {
  return await ProxyDao.update(Proxy);
};

ProxyService.removeProxy = async function ({ server_name }) {
  return await ProxyDao.destroy({ where: { server_name } });
};

ProxyService.removeProxyById = async function ({ id }) {
  return await ProxyDao.destroy({ where: { id } });
};

ProxyService.findByServerName = async function ({ serverName }) {
  return await ProxyDao.findOne({ where: { server_name: serverName } });
};

module.exports = ProxyService;
