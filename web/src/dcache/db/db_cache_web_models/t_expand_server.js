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
    return sequelize.define('t_expand_server', {
        id: {
            autoIncrement: true,
            type: DataTypes.INTEGER(11),
            allowNull: false,
            primaryKey: true
        },
        area: {
            type: DataTypes.STRING(50),
            allowNull: false,
            defaultValue: ""
        },
        patch_version: {
            type: DataTypes.STRING(50),
            allowNull: false,
            defaultValue: ""
        },
        operation_id: {
            type: DataTypes.INTEGER(11),
            allowNull: false,
        },
        app_name: {
            type: DataTypes.STRING(100),
            allowNull: false,
            defaultValue: ""
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
        server_name: {
            type: DataTypes.STRING(100),
            allowNull: false,
            defaultValue: ""
        },
        server_ip: {
            type: DataTypes.STRING(100),
            allowNull: false,
            defaultValue: ""
        },
        server_type: {
            type: DataTypes.INTEGER(4),
            allowNull: false,
            defaultValue: 0
        },
        memory: {
            type: DataTypes.INTEGER(4),
            allowNull: false,
            defaultValue: 0
        },
        shmKey: {
            type: DataTypes.STRING(100),
            allowNull: false,
            defaultValue: ""
        },
        idc_area: {
            type: DataTypes.STRING(50),
            allowNull: false,
            defaultValue: ""
        },
        status: {
            type: DataTypes.INTEGER(4),
            allowNull: false,
            defaultValue: 0
        },
        modify_person: {
            type: DataTypes.STRING(50),
            allowNull: false,
            defaultValue: ""
        },
        modify_time: {
            type: DataTypes.DATE,
            defaultValue: DataTypes.NOW,
        },
        is_docker: {
            type: DataTypes.INTEGER(1),
            allowNull: false,
            defaultValue: 0
        }
    }, {
        sequelize,
        tableName: 't_expand_server',
        timestamps: false,
		indexes: [{
		        name: 't_expand_server_operation_id_foreign_idx',
		        unique: false,
		        fields: [`operation_id`]
		    }
		]
    });
};