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

const _ = require('lodash');
const validator = require('validator');

//api入参出参中间件
const paramsDealMidware = (validParams) => {
	return async (ctx, next) => {
		var params = _.extend(ctx.query || {}, ctx.request.body || {}, ctx.req.body || {});
		if (validParams && _.isArray(validParams)) {
			ctx.paramsObj = {};
			validParams.forEach(function (v) {
				if (params[v] !== undefined) {
					ctx.paramsObj[v] = params[v];
				}
			});
		} else {
			ctx.paramsObj = params;
		}

		ctx.makeResObj = (retCode, errMsg, result) => {
			result = result == undefined ? {} : result;
			ctx.body = {
				data: result,
				ret_code: retCode,
				err_msg: errMsg
			};
		};
		ctx.makeErrResObj = () => {
			ctx.body = {
				data: {},
				ret_code: 500,
				err_msg: '#common.systemError#'
			};
		};
		ctx.makeNotAuthResObj = () => {
			ctx.body = {
				data: {},
				ret_code: 500,
				err_msg: '#common.noPrivilage#'
			};
		};
		await next();
	}
};

const paramsCheckMidware = (checkRule) => {
	return async (ctx, next) => {
		var params = ctx.paramsObj === undefined ? ctx.paramsObj : _.extend(ctx.query || {}, ctx.request.body || {}, ctx.req.body || {});
		checkRule = checkRule || {};
		var hasError = false;
		_.each(checkRule, (rules, paramName) => {
			if (rules) {
				var value = params[paramName] != undefined ? params[paramName].toString() : '';
				_.each(rules.split(';'), (rule) => {
					if (rule === 'notEmpty' && validator.isEmpty(value)) {
						hasError = true;
						ctx.makeResObj(500, paramName + '#common.notEmpty#');
						return false;
					} else if (rule === 'number' && !validator.isFloat(value)) {
						hasError = true;
						ctx.makeResObj(500, paramName + '#common.needNumber#');
						return false;
					} else if (rule === 'array' && (!validator.isJSON(value) || Object.prototype.toString.call(JSON.parse(value)) !== '[object Array]')) {
						hasError = true;
						ctx.makeResObj(500, paramName + '#common.needArray#');
						return false;
					} else if (rule === 'object' && (!validator.isJSON(value) || Object.prototype.toString.call(JSON.parse(value)) !== '[object Object]')) {
						hasError = true;
						ctx.makeResObj(500, paramName + '#common.needObject#');
						return false;
					} else if (rule === 'boolean' && !validator.isBoolean(value)) {
						hasError = true;
						ctx.makeResObj(500, paramName + '#common.needBoolean#');
						return false;
					}
				});
				if (hasError) {
					return false;
				}
			}
		})
		if (!hasError) {
			await next();
		}
	}
};

module.exports = {
	paramsDealMidware,
	paramsCheckMidware
}