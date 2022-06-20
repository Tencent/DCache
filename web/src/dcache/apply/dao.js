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

const { tApplyAppBase, tApplyAppRouterConf, tApplyAppProxyConf, tApplyAppDbaccessConf } = require('./../db').db_cache_web;
const Sequelize = require('sequelize');
const applyDao = {};

applyDao.findRouterDbAll = async function () {
  return await tApplyAppRouterConf.findAll({
    attributes: [[Sequelize.literal('distinct concat(`router_db_ip`, ":", `router_db_port`, "-", `router_db_user`)'), 'router_db_flag'], 'id'],
    limit: 1,
    offset: 0,
  });
}

applyDao.findRouter = async function (id) {
  return await tApplyAppRouterConf.findOne({ id });
}

applyDao.findAll = async function ({
  where = {},
  raw = true,
  queryRouter = [],
  queryProxy = [],
  // queryDbAccess = [],
  attributes = ['id', 'status', 'idc_area', 'set_area', 'admin', 'name', 'create_person'],
  include = [],
}) {
  if (queryRouter.length > 0) {
    const routerModelItem = {
      model: tApplyAppRouterConf,
      attributes: queryRouter,
      as: 'Router',
      raw: true,
    };
    include.push(routerModelItem);
  }
  if (queryProxy.length > 0) {
    const proxyModelItem = {
      model: tApplyAppProxyConf,
      attributes: queryProxy,
      as: 'Proxy',
      raw: true,
    };
    include.push(proxyModelItem);
  }
  // if (queryDbAccess.length > 0) {
  //   const dbAccessModelItem = {
  //     model: tApplyAppDbaccessConf,
  //     attributes: queryDbAccess,
  //     as: 'DbAccess',
  //   };
  //   include.push(dbAccessModelItem);
  // }

  // 一般来说，查找的都是安装成功的、即status=2
  if (!where.status) where.status = 2;
  const data = await tApplyAppBase.findAll({
    where,
    raw,
    attributes,
    include,
  });
  return data;
};

applyDao.findOne = async function ({
  where = {},
  attributes = ['id', 'status', 'idc_area', 'set_area', 'admin', 'name', 'create_person'],
  queryRouter = [],
  queryProxy = [],
  include = [],
}) {
  if (queryRouter.length > 0) {
    const routerModelItem = {
      model: tApplyAppRouterConf,
      attributes: queryRouter,
      as: 'Router',
    };
    include.push(routerModelItem);
  }
  if (queryProxy.length > 0) {
    const proxyModelItem = {
      model: tApplyAppProxyConf,
      attributes: queryProxy,
      as: 'Proxy',
    };
    include.push(proxyModelItem);
  }

  const data = await tApplyAppBase.findOne({ where, attributes, include });
  return data;
};

applyDao.addApply = function ({
  idc_area, set_area, admin, name, create_person,
}) {
  // 1 创建过程中， 2 已安装，可以在资源树展示
  return tApplyAppBase.create({
    status: 1,
    idc_area,
    set_area,
    admin,
    name,
    create_person,
  });
};

/**
 * Performs an "upsert" - That is, does an update if a matching record already exists, otherwise does an insert
 *
 * @param whereProperties Names of the properties to use in the WHERE clause for matching  []
 * @param params Hash of parameters to use in the INSERT or UPDATE
 */
applyDao.createOrUpdate = async function (whereProperties, params) {
  try {
    const self = tApplyAppBase;
    const where = {};
    whereProperties.forEach((key) => {
      where[key] = params[key];
    });
    // console.log("createOrUpdate:", where, params);
    
    let record = await self.findOne({ where });
    if (!record) {
      record = await self.create(params);
    } else {

      // console.log("record:", record);

      // record.updateAttributes(params);
      record.update(params);
    }
    return record;
  } catch (err) {
    throw new Error(err);
  }
};

applyDao.destroy = function (option) {
  return tApplyAppBase.destroy(option);
};

module.exports = applyDao;
