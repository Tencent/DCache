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

module.exports = {
  moduleCreate(params) {
    return RouterDao.moduleCreate(params)
  },
  moduleDestroy(params) {
    return RouterDao.moduleDestroy(params)
  },
  moduleUpdate(params) {
    return RouterDao.moduleUpdate(params)
  },
  moduleFindOne(params) {
    return RouterDao.moduleFindOne(params)
  },
  moduleFindAndCountAll(params) {
    return RouterDao.moduleFindAndCountAll(params)
  },

  groupCreate(params) {
    return RouterDao.groupCreate(params)
  },
  groupDestroy(params) {
    return RouterDao.groupDestroy(params)
  },
  groupUpdate(params) {
    return RouterDao.groupUpdate(params)
  },
  groupFindOne(params) {
    return RouterDao.groupFindOne(params)
  },
  groupFindAndCountAll(params) {
    return RouterDao.groupFindAndCountAll(params)
  },

  recordCreate(params) {
    return RouterDao.recordCreate(params)
  },
  recordDestroy(params) {
    return RouterDao.recordDestroy(params)
  },
  recordUpdate(params) {
    return RouterDao.recordUpdate(params)
  },
  recordFindOne(params) {
    return RouterDao.recordFindOne(params)
  },
  recordFindAndCountAll(params) {
    return RouterDao.recordFindAndCountAll(params)
  },

  serverCreate(params) {
    return RouterDao.serverCreate(params)
  },
  serverDestroy(params) {
    return RouterDao.serverDestroy(params)
  },
  serverUpdate(params) {
    return RouterDao.serverUpdate(params)
  },
  serverFindOne(params) {
    return RouterDao.serverFindOne(params)
  },
  serverFindAndCountAll(params) {
    return RouterDao.serverFindAndCountAll(params)
  },

  transferCreate(params) {
    return RouterDao.transferCreate(params)
  },
  transferDestroy(params) {
    return RouterDao.transferDestroy(params)
  },
  transferUpdate(params) {
    return RouterDao.transferUpdate(params)
  },
  transferFindOne(params) {
    return RouterDao.transferFindOne(params)
  },
  transferFindAndCountAll(params) {
    return RouterDao.transferFindAndCountAll(params)
  },
}