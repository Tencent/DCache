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

const { tRegion } = require('./../db').db_cache_web;

const RegionDao = {};

RegionDao.getRegion = async function ({ where = {}, attributes = ['id'] }) {
  const data = await tRegion.findOne({ where, attributes });
  if (!data) throw new Error('地区不存在');
  return data;
};

RegionDao.getRegionList = function () {
  return tRegion.findAll();
};

RegionDao.addRegion = function ({ region, label }) {
  return tRegion.create({ region, label });
};

RegionDao.deleteRegion = async function ({ id }) {
  return tRegion.destroy({ where: { id } });
};

RegionDao.updateRegion = function ({ id, region, label }) {
  return tRegion.update({ region, label }, { where: { id } });
};


module.exports = RegionDao;
