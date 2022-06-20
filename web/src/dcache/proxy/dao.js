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

const { tApplyAppProxyConf } = require('./../db').db_cache_web;

const ProxyDao = {};

// const modelDesc = {
//   id: 'id',
//   apply_id: '应用id',
//   server_name: '服务名',
//   server_ip: '服务ip',
//   template_file: '服务模版',
//   idc_area: '异地镜像 例如: [sz, bj]',
//   create_person: '创建人',
// };
ProxyDao.createProxy = async function ({
  apply_id, server_name, server_ip, template_file, idc_area, create_person,
}) {
  return await tApplyAppProxyConf.create({
    apply_id,
    server_name,
    server_ip,
    template_file,
    idc_area,
    create_person,
  });
};

ProxyDao.createOrUpdate = async function (whereProperties, params) {
  try {
    const self = tApplyAppProxyConf;
    const where = {};
    whereProperties.forEach((key) => {
      where[key] = params[key];
    });
    let record = await self.findOne({ where });
    if (!record) {
      record = await self.create(params);
    } else {
      // record.updateAttributes(params);
      await record.update(params);
    }
    return record;
  } catch (err) {
    throw new Error(err);
  }
};

ProxyDao.update = async function ({
  apply_id, server_name, server_ip, template_file, idc_area, create_person, id,
}) {
  return await tApplyAppProxyConf.update({
    apply_id,
    server_name,
    server_ip,
    template_file,
    idc_area,
    create_person,
  }, { where: { id } });
};

ProxyDao.destroy = async function (option) {
  return await tApplyAppProxyConf.destroy(option);
};

ProxyDao.findOne = async function ({ where }) {
  return await tApplyAppProxyConf.findOne({ where });
};


module.exports = ProxyDao;
