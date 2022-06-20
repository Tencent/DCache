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

const { tApplyAppRouterConf } = require('./../db').db_cache_web
const Db = require('../db')
const dbTpl = require('./../db/index_tpl')

const getDBInfo = async (app) => {
  return await tApplyAppRouterConf.findOne({
    where: {
      server_name: `${app}RouterServer`,
    }
  })
}
const connDB = async (config) => {
  return await dbTpl.conn(config)
}
const initDB = async (treeid) => {
  const DBInfo = await getDBInfo(treeid)
  let result = dbTpl[DBInfo.router_db_name] || ''
  if(!result){
    const config = {
      host: DBInfo.router_db_ip,
      database: DBInfo.router_db_name,
      port: DBInfo.router_db_port,
      user: DBInfo.router_db_user,
      password: DBInfo.router_db_pass,
      charset: 'utf8',
      pool: {
        max: 10,
        min: 0,
        idle: 10000,
      },
    }
    await connDB(config)
    result = dbTpl[DBInfo.router_db_name]
  }
  return result
}

module.exports = {
  async moduleCreate(params) {
    const { treeid, module_name, version, switch_status, remark, modify_time } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterModule.create({ module_name, version, switch_status, remark, modify_time })
  },
  async moduleDestroy(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterModule.destroy({ where: { id } })
  },
  async moduleUpdate(params) {
    const { treeid, id, module_name, version, switch_status, remark, modify_time } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterModule.update({ module_name, version, switch_status, remark, modify_time }, { where: { id } })
  },
  async moduleFindOne(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterModule.findOne({ where: { id } })
  },
  async moduleFindAndCountAll(params) {
    const { treeid, page = 1, size = 12 } = params
    const dbModel = await initDB(treeid)
    let result = await dbModel.tRouterModule.findAndCountAll({
      order: [['modify_time', 'desc']],
      limit: size,
      offset: size * (page - 1),
    })
    result.page = Math.floor(page)
    result.size = Math.floor(size)
    return result
  },
  async groupCreate(params) {
    const { treeid, module_name, group_name, access_status, server_name, server_status, modify_time, pri, source_server_name } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterGroup.create({ module_name, group_name, access_status, server_name, server_status, modify_time, pri, source_server_name })
  },
  async groupDestroy(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterGroup.destroy({ where: { id } })
  },
  async groupUpdate(params) {
    const { treeid, id, module_name, group_name, access_status, server_name, server_status, modify_time, pri, source_server_name } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterGroup.update({ module_name, group_name, access_status, server_name, server_status, modify_time, pri, source_server_name }, { where: { id } })
  },
  async groupFindOne(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterGroup.findOne({ where: { id } })
  },
  async groupFindAndCountAll(params) {
    const { treeid, page = 1, size = 12 } = params
    const dbModel = await initDB(treeid)
    let result = await dbModel.tRouterGroup.findAndCountAll({
      order: [['modify_time', 'desc']],
      limit: size,
      offset: size * (page - 1),
    })
    result.page = Math.floor(page)
    result.size = Math.floor(size)
    return result
  },
  async recordCreate(params) {
    const { treeid, module_name, from_page_no, to_page_no, group_name, modify_time } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterRecord.create({ module_name, from_page_no, to_page_no, group_name, modify_time })
  },
  async recordDestroy(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterRecord.destroy({ where: { id } })
  },
  async recordUpdate(params) {
    const { treeid, id, module_name, from_page_no, to_page_no, group_name, modify_time } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterRecord.update({ module_name, from_page_no, to_page_no, group_name, modify_time }, { where: { id } })
  },
  async recordFindOne(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterRecord.findOne({ where: { id } })
  },
  async recordFindAndCountAll(params) {
    const { treeid, page = 1, size = 12 } = params
    const dbModel = await initDB(treeid)
    let result = await dbModel.tRouterRecord.findAndCountAll({
      order: [['modify_time', 'desc']],
      limit: size,
      offset: size * (page - 1),
    })
    result.page = Math.floor(page)
    result.size = Math.floor(size)
    return result
  },
  async serverCreate(params) {
    const { treeid, idc_area, server_name, ip, binlog_port, cache_port, wcache_port, backup_port, routerclient_port, remark, modify_time } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterServer.create({ idc_area, server_name, ip, binlog_port, cache_port, wcache_port, backup_port, routerclient_port, remark, modify_time })
  },
  async serverDestroy(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterServer.destroy({ where: { id } })
  },
  async serverUpdate(params) {
    const { treeid, id, idc_area, server_name, ip, binlog_port, cache_port, wcache_port, backup_port, routerclient_port, remark, modify_time } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterServer.update({ idc_area, server_name, ip, binlog_port, cache_port, wcache_port, backup_port, routerclient_port, remark, modify_time }, { where: { id } })
  },
  async serverFindOne(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterServer.findOne({ where: { id } })
  },
  async serverFindAndCountAll(params) {
    const { treeid, page = 1, size = 12 } = params
    const dbModel = await initDB(treeid)
    let result = await dbModel.tRouterServer.findAndCountAll({
      order: [['modify_time', 'desc']],
      limit: size,
      offset: size * (page - 1),
    })
    result.page = Math.floor(page)
    result.size = Math.floor(size)
    return result
  },
  async transferCreate(params) {
    const { treeid, module_name, from_page_no, to_page_no, group_name, trans_group_name, transfered_page_no, remark, state, modify_time, startTime, endTime } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterTransfer.create({ module_name, from_page_no, to_page_no, group_name, trans_group_name, transfered_page_no, remark, state, modify_time, startTime, endTime })
  },
  async transferDestroy(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterTransfer.destroy({ where: { id } })
  },
  async transferUpdate(params) {
    const { treeid, id, module_name, from_page_no, to_page_no, group_name, trans_group_name, transfered_page_no, remark, state, modify_time, startTime, endTime } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterTransfer.update({ module_name, from_page_no, to_page_no, group_name, trans_group_name, transfered_page_no, remark, state, modify_time, startTime, endTime }, { where: { id } })
  },
  async transferFindOne(params) {
    const { treeid, id } = params
    const dbModel = await initDB(treeid)
    return dbModel.tRouterTransfer.findOne({ where: { id } })
  },
  async transferFindAndCountAll(params) {
    const { treeid, page = 1, size = 12 } = params
    const dbModel = await initDB(treeid)
    let result = await dbModel.tRouterTransfer.findAndCountAll({
      order: [['modify_time', 'desc']],
      limit: size,
      offset: size * (page - 1),
    })
    result.page = Math.floor(page)
    result.size = Math.floor(size)
    return result
  },
}
