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
const cwd = process.cwd();
const path = require('path');
const fs = require('fs-extra');

let conf = {
    webConf: {
        port: 16535, //服务启动端口
        defaultLanguage: 'cn', //cn 或 en ，用户默认的语言环境
        alter: true, //变更db结构
    },
    dbConf: {
        port: '3306', // 数据库端口
        user: 'tars', // 用户名
        password: 'tars2015', // 密码
        charset: 'utf8', // 数据库编码
        pool: {
            max: 10, // 连接池中最大连接数量
            min: 0, // 连接池中最小连接数量
            idle: 10000 // 如果一个线程 10 秒钟内没有被使用过的话，那么就释放线程
        }
    },
    path: "/plugins/dcache"
};

if (process.env.NODE_ENV == "local") {

    conf.dbConf = {
        host: '127.0.0.1', // 数据库地址
        port: '3306', // 数据库端口
        user: 'tarsAdmin', // 用户名
        password: 'Tars@2019', // 密码
        charset: 'utf8', // 数据库编码
        pool: {
            max: 10, // 连接池中最大连接数量
            min: 0, // 连接池中最小连接数量
            idle: 10000 // 如果一个线程 10 秒钟内没有被使用过的话，那么就释放线程
        }
    };

    conf.webConf.host = '0.0.0.0';
    conf.webConf.port = 29104;
    conf.webConf.alter = true;

} else if (process.env.NODE_ENV == "remote") {
    conf.ENABLE_LOCAL_CONFIG = true;

    conf.dbConf = {
        host: '172.30.0.47', // 数据库地址
        port: '3306', // 数据库端口
        user: 'tarsAdmin', // 用户名
        password: 'Tars@2019', // 密码
        charset: 'utf8', // 数据库编码
        pool: {
            max: 10, // 连接池中最大连接数量
            min: 0, // 连接池中最小连接数量
            idle: 10000 // 如果一个线程 10 秒钟内没有被使用过的话，那么就释放线程
        }
    };

    conf.webConf.host = '0.0.0.0';
    conf.webConf.port = 5001;

    conf.webConf.alter = false;

}

module.exports = conf;