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
/* jshint indent: 2 */

module.exports = function(sequelize, DataTypes) {
    return sequelize.define('t_router_group', {
        id: {
            autoIncrement: true,
            type: DataTypes.INTEGER(11),
            allowNull: false,
            primaryKey: true
        },
        module_name: {
            type: DataTypes.STRING(100),
            allowNull: false,
            defaultValue: ""
        },
        group_name: {
            type: DataTypes.STRING(100),
            allowNull: false,
            defaultValue: ""
        },
        access_status: {
            type: DataTypes.INTEGER(11),
            allowNull: false,
        },
        server_name: {
            type: DataTypes.STRING(100),
            allowNull: false,
            defaultValue: ""
        },
        server_status: {
            type: DataTypes.STRING(100),
            allowNull: false,
        },
        modify_time: {
            type: DataTypes.DATE,
            defaultValue: DataTypes.NOW,
        },
        POSTTIME: {
            type: DataTypes.DATE,
            allowNull: true
        },
        LASTUSER: {
            type: DataTypes.STRING(60),
            allowNull: true
        },
        priority: {
            type: DataTypes.STRING(100),
            allowNull: false,
        },
        source_server_name: {
            type: DataTypes.STRING(100),
            allowNull: false,
            defaultValue: ""
        }
    }, {
        sequelize,
        tableName: 't_router_group',
            timestamps: false
    });
};