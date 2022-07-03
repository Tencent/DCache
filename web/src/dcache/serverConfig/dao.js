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
const { toString } = Object.prototype;

const { tApplyAppBase, tApplyCacheServerConf, tApplyCacheModuleConf } = require('./../db').db_cache_web;

const serverConf = {};

serverConf.add = async function (option) {
  if (toString.call(option) === '[object Array]') {
    const promiseArray = [];
    option.forEach(item => promiseArray.push(serverConf.destroy({
      where: {
        apply_id: item.apply_id,
        module_name: item.module_name,
        group_name: item.group_name,
        server_name: item.server_name,
      },
    })));
    await Promise.all(promiseArray);
  } else if (toString.call(option) === '[object Object]') {
    serverConf.destroy({
      where: {
        apply_id: option.apply_id,
        module_name: option.module_name,
        group_name: option.group_name,
        server_name: option.server_name,
      },
    });
  }
  return tApplyCacheServerConf.bulkCreate(option);
};

serverConf.update = function ({ where, values }) {
  return tApplyCacheServerConf.update(values, { where });
};

serverConf.destroy = function (option) {
  return tApplyCacheServerConf.destroy(option);
};

serverConf.findOne = function ({ where = {}, attributes = [] }) {
  const option = {
    where,
  };
  if (attributes.length) option.attributes = attributes;
  return tApplyCacheServerConf.findOne(option);
};

serverConf.findAll = function ({
  where = {}, attributes = [], queryBase = [], include = [], queryModule = [],
}) {
  if (queryBase.length > 0) {
    const item = {
      model: tApplyAppBase,
      attributes: queryBase,
      as: 'applyBase',
      raw: true,
    };
    include.push(item);
  }
  if (queryModule.length > 0) {
    const item = {
      model: tApplyCacheModuleConf,
      attributes: queryModule,
      as: 'moduleBase',
      raw: true,
    };
    include.push(item);
  }

  const option = {
    where,
    include,
  };
  if (attributes.length) option.attributes = attributes;
  return tApplyCacheServerConf.findAll(option);
};

module.exports = serverConf;
