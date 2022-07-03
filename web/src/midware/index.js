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

const WebConf = require('../config/webConf');

const {
	pageApiConf,
	localeApiConf,
} = require('../index');

const {
	dcacheApiConf
} = require('../dcache');

const Router = require('koa-router');
const _ = require('lodash');
const noCacheMidware = require('./noCacheMidware');
const {
	paramsDealMidware,
	paramsCheckMidware
} = require('./paramsMidware');

const loginMidware = require('./loginMidware');

//获取路由
const getRouter = (router, routerConf) => {

	routerConf.forEach(function (conf) {
		var [method, url, controller, checkRule, validParams] = conf;

		//前置参数合并校验相关中间件
		router[method](url, paramsDealMidware(validParams)); //上下文入参出参处理中间件
		router[method](url, paramsCheckMidware(checkRule)); //参数校验中间件

		router[method](url, noCacheMidware); //禁用缓存中间件

		router[method](url, loginMidware);

		//业务逻辑控制器
		router[method](url, async (ctx, next) => {

			await controller.call({}, ctx);
		});

	});
};

//页面类型路由
const pageRouter = new Router();
pageRouter.prefix(WebConf.path);
getRouter(pageRouter, pageApiConf);

//页面接口类型路由
const localeApiRouter = new Router();
localeApiRouter.prefix(WebConf.path);
getRouter(localeApiRouter, localeApiConf);

//dcache类型路由
const dcacheApiRouter = new Router();
dcacheApiRouter.prefix(`${WebConf.path}/server/api`);

getRouter(dcacheApiRouter, dcacheApiConf);

module.exports = {
	pageRouter,
	localeApiRouter,
	dcacheApiRouter,
};