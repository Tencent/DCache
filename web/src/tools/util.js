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
const moment = require('moment');
const request = require('request-promise-any');
const util = {};
const crypto = require("crypto")

//根据相应格式和方法过滤单一对象
util._viewFilterObj = (obj, filterSturct) => {
	if (obj.hasOwnProperty('dataValues')) {
		obj = obj.dataValues;
	}
	if (!_.isPlainObject(obj)) {
		return obj;
	}
	var newObj = {};
	_.each(filterSturct, (v, key) => {
		if (obj[key] !== undefined) {
			v = v || {};
			let newKey = v.key || key;
			let formatter = v.formatter || '';
			if (formatter && _.isFunction(formatter)) {
				newObj[newKey] = formatter(obj[key]);
			} else {
				newObj[newKey] = obj[key];
			}
		}
	});
	return newObj;
}

//根据相应格式和方法过滤结果集，可过滤对象数组，或单一对象
util.viewFilter = (data, filterSturct) => {
	if (!data || !filterSturct) {
		return data;
	}
	var newData;
	if (_.isArray(data)) {
		newData = []
		data.forEach((obj) => {
			newData.push(util._viewFilterObj(obj, filterSturct));
		});
	} else if (data.hasOwnProperty('dataValues') || _.isPlainObject(data)) {
		newData = util._viewFilterObj(data, filterSturct);
	}
	return newData;
};

//数据库时间戳转换
util.formatTimeStamp = (timeStamp) => {
	if (timeStamp == null) {
		return "";
	}
	return moment(timeStamp).format('YYYY-MM-DD HH:mm:ss');
};

util.getUUID = () => {
	var s = [];
	var hexDigits = "0123456789abcdef";
	for (var i = 0; i < 36; i++) {
		s[i] = hexDigits.substr(Math.floor(Math.random() * 0x10), 1);
	}
	s[14] = "4";
	s[19] = hexDigits.substr((s[19] & 0x3) | 0x8, 1);
	s[8] = s[13] = s[18] = s[23] = "-";

	var uuid = s.join("").replace(/-/g, "");
	return uuid;
};

util.leftAssign = (obj1, obj2) => {
	_.each(obj1, (value, key) => {
		if (obj2[key] !== undefined) {
			obj1[key] = obj2[key];
		}
	});
	return obj1;
};

util._jsonRequest = async (type, url, data) => {
	let options = {
		uri: url,
		json: true,
	};
	if (type === 'get') {
		options.qs = data;
		options.method = 'GET';
	} else {
		options.body = data;
		options.method = 'POST';
	}
	var rst = await request[type](options);
	return rst;
};

util.jsonRequest = {
	get: async (url, data) => {
		return await util._jsonRequest('get', url, data);
	},
	post: async (url, data) => {
		return await util._jsonRequest('post', url, data);
	}
};

util.sleep = async (time) => {
	return new Promise((resolve) => setTimeout(resolve, time));
}

//hash number计算(不直接用taskNo前几位，不耦合)
util.getHashNumber = (taskNo) => {
	let buf = crypto.createHash("md5").update(taskNo + "").digest();
	return ((buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0]) >>> 0
}

module.exports = {
	viewFilter: util.viewFilter,
	formatTimeStamp: util.formatTimeStamp,
	getUUID: util.getUUID,
	leftAssign: util.leftAssign,
	jsonRequest: util.jsonRequest,
	sleep: util.sleep,
	getHashNumber: util.getHashNumber
};