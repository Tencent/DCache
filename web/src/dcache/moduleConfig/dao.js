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

const {
  tApplyCacheModuleConf, tApplyCacheServerConf, tApplyCacheModuleBase, tApplyAppBase, 
} = require('./../db').db_cache_web;

const moduleConf = {};

moduleConf.add = function (option) {
  return tApplyCacheModuleConf.create(option);
};

moduleConf.destroy = function (option) {
  return tApplyCacheModuleConf.destroy(option);
};

moduleConf.findOne = function ({
  where = {},
  attributes = ['id', 'module_id', 'apply_id', 'module_name', 'status', 'area', 'idc_area', 'set_area', 'admin', 'cache_module_type', 'per_record_avg', 'total_record', 'max_read_flow', 'key_type', 'max_write_flow', 'module_remark', 'dbAccessServant', 'open_backup', 'cache_version', 'mkcache_struct'],
  queryModuleBase = [],
  queryServerConf = [],
  queryAppBase = [],
  include = [],
}) {
  if (queryModuleBase.length > 0) {
    const moduleBaseModelItem = {
      model: tApplyCacheModuleBase,
      attributes: queryModuleBase,
      as: 'ModuleBase',
    };
    include.push(moduleBaseModelItem);
  }
  if (queryServerConf.length > 0) {
    const serverConfModelItem = {
      model: tApplyCacheServerConf,
      attributes: queryServerConf,
      as: 'ServerConf',
    };
    include.push(serverConfModelItem);
  }
  if (queryAppBase.length > 0) {
    const serverConfModelItem = {
      model: tApplyAppBase,
      attributes: queryAppBase,
      as: 'AppBase',
    };
    include.push(serverConfModelItem);
  }

  return tApplyCacheModuleConf.findOne({ where, attributes, include });
};

moduleConf.findAll = function ({
  where = {},
  attributes = ['id', 'module_id', 'apply_id', 'module_name', 'status', 'area', 'idc_area', 'set_area', 'admin', 'cache_module_type', 'per_record_avg', 'total_record', 'max_read_flow', 'key_type', 'max_write_flow', 'module_remark', 'dbAccessServant', 'open_backup', 'cache_version', 'mkcache_struct'],
  queryModuleBase = [],
  queryServerConf = [],
  queryAppBase = [],
  include = [],
}) {
  if (queryModuleBase.length > 0) {
    const moduleBaseModelItem = {
      model: tApplyCacheModuleBase,
      attributes: queryModuleBase,
      as: 'ModuleBase',
    };
    include.push(moduleBaseModelItem);
  }
  if (queryServerConf.length > 0) {
    const serverConfModelItem = {
      model: tApplyCacheServerConf,
      attributes: queryServerConf,
      as: 'ServerConf',
    };
    include.push(serverConfModelItem);
  }
  if (queryAppBase.length > 0) {
    const serverConfModelItem = {
      model: tApplyAppBase,
      attributes: queryAppBase,
      as: 'AppBase',
    };
    include.push(serverConfModelItem);
  }

  return tApplyCacheModuleConf.findAll({ where, attributes, include });
};

module.exports = moduleConf;
