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

module.exports = function (sequelize, DataTypes) {
  return sequelize.define('t_apply_app_dbaccess_conf', {
    id: {
      type: DataTypes.INTEGER(11),
      allowNull: false,
      primaryKey: true,
      autoIncrement: true,
    },
    module_id: {
      type: DataTypes.INTEGER(11),
      allowNull: false,
    },
    isSerializated: {
      type: DataTypes.INTEGER(11),
      allowNull: false,
    },
    servant: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: '',
    },
    dbaccess_ip: {
      type: DataTypes.TEXT,
      allowNull: false,
      defaultValue: '',
    },
    db_num: {
      type: DataTypes.INTEGER(11),
      allowNull: false,
      defaultValue: 1,
    },
    db_prefix: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: '',
    },
    table_num: {
      type: DataTypes.INTEGER(11),
      allowNull: false,
      defaultValue: 1,
    },
    table_prefix: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: '',
    },
    db_host: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: '',
    },
    db_port: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: '',
    },
    db_pwd: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: '',
    },
    db_user: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: '',
    },
    db_charset: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: '',
    },
    modify_time: {
      type: DataTypes.DATE,
      defaultValue: DataTypes.NOW,
    },
  }, {
    tableName: 't_apply_app_dbaccess_conf',
    timestamps: false,
  });
};
