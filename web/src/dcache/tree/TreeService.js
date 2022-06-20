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

const AdminService = require("../../common/AdminService")
const logger = require("../../logger")
const cacheData = {
	// timer: '',
	serverData: [],
	dcacheData: [],
	// business: [],
	// businessRelation: [],
}

const TreeService = {};

// TreeService.getTreeNodes = async (searchKey, uid, type) => {
// 	return await TreeService.getCacheData(searchKey, uid, type)
// };

TreeService.hasDCacheServerName = (serverName) => {
	return cacheData.dcacheData.find((item) => {
		return item.server_name == serverName;
	});
}

/**
 * 将应用服务转换成层级数据
 * @param data
 */

TreeService.ArrayToTree = (data) => {
	let treeNodeList = []
	let treeNodeMap = {}
	let rootNode = []
	data.forEach(function (server) {
		// server = server;
		let id;
		if (server.enable_set == 'Y') {
			id = '1' + server.application + '.' + '2' + server.set_name + '.' + '3' + server.set_area + '.' + '4' + server.set_group + '.' + '5' + server.server_name;
		} else {
			id = '1' + server.application + '.' + '5' + server.server_name;
		}
		let treeNode = {};
		treeNode.id = id;
		treeNode.name = server.server_name;
		treeNode.pid = id.substring(0, id.lastIndexOf('.'));
		treeNode.is_parent = false;
		treeNode.open = false;
		treeNode.children = [];
		treeNodeList.push(treeNode);
		treeNodeMap[id] = treeNode;

		TreeService.parents(treeNodeMap, treeNode, rootNode)
	})

	return rootNode
}

/**
 * 将应用服务转换成层级数据
 * @param treeNodeMap
 * @param treeNode
 * @param rootNodes
 */
TreeService.parents = (treeNodeMap, treeNode, rootNodes) => {
	let id = treeNode.pid;

	if (id == 'root') {
		rootNodes.push(treeNode);
		return;
	}

	if (treeNodeMap[id]) {
		treeNodeMap[id].children.push(treeNode);
		return;
	}

	let pid, name;
	let idx = id.lastIndexOf('.');
	if (idx === -1) {
		pid = 'root';
		name = id;
	} else {
		pid = id.substring(0, idx);
		name = id.substring(idx + 1);
	}
	let newTreeNode = {};
	newTreeNode.id = id;
	newTreeNode.name = name.substring(1);
	newTreeNode.pid = pid;
	newTreeNode.is_parent = true;
	newTreeNode.open = false;
	newTreeNode.children = [];
	newTreeNode.children.push(treeNode);

	treeNodeMap[id] = newTreeNode;
	TreeService.parents(treeNodeMap, newTreeNode, rootNodes);

};

TreeService.parentsBusiness = async (data) => {
	//将app级别先排序(保证进入业务层后是按照顺序的)
	data = data.sort((a, b) => {
		if (b.order === a.order) {
			let aName = a.name.toLowerCase()
			let bName = b.name.toLowerCase()
			if (aName < bName) {
				return -1
			}
			if (aName > bName) {
				return 1
			}
			return 0
		}
		return b.order - a.order
	})

	return data;

	// // const businessList = cacheData.business
	// // const businessRelationList = cacheData.businessRelation
	// let result = []
	// data && data.forEach((item, index) => {
	// 	let relationIsTrue = false
	// 	// businessRelationList && businessRelationList.forEach(jitem => {
	// 	// 	businessList && businessList.forEach(kitem => {
	// 	// 		if (item.name === jitem.f_application_name && jitem.f_business_name === kitem.f_name) {
	// 	// 			relationIsTrue = true
	// 	// 			let obj = {
	// 	// 				name: kitem.f_show_name,
	// 	// 				pid: kitem.f_name,
	// 	// 				is_parent: true,
	// 	// 				open: false,
	// 	// 				children: [item],
	// 	// 				order: kitem.f_order,
	// 	// 			}
	// 	// 			let isTrue = false
	// 	// 			let resultIndex = 0
	// 	// 			result.forEach((litem, lindex) => {
	// 	// 				if (litem.pid === obj.pid) {
	// 	// 					isTrue = true
	// 	// 					resultIndex = lindex
	// 	// 				}
	// 	// 			})
	// 	// 			if (isTrue) {
	// 	// 				result[resultIndex].children.push(obj.children[0])
	// 	// 			} else {
	// 	// 				result.push(obj)
	// 	// 			}
	// 	// 		}
	// 	// 	})
	// 	// })
	// 	if (!relationIsTrue && item.name) {
	// 		item.order = 0
	// 		result.push(item)
	// 	}
	// })
	// result = result.sort((a, b) => {
	// 	if (b.order === a.order) {
	// 		let aName = a.name.toLowerCase()
	// 		let bName = b.name.toLowerCase()
	// 		if (aName < bName) {
	// 			return -1
	// 		}
	// 		if (aName > bName) {
	// 			return 1
	// 		}
	// 		return 0
	// 	}
	// 	return b.order - a.order
	// })
	// return result
}

/**
 * 写入缓存数据
 */
TreeService.setCacheData = async () => {
	// cacheData.business = await BusinessDao.getList() || []
	// cacheData.businessRelation = await BusinessRelationDao.getList() || []

	let tree = await AdminService.getServerTree();

	if (!tree) {
		logger.error("get server tree error");
		return;
	}

	let serverList = tree;

	// console.log(serverList);

	// // 去重
	// let arr = serverList || [],
	// 	newArr = [arr[0]]
	// for (let i = 1; i < arr.length; i++) {
	// 	let repeat = false
	// 	for (let j = 0; j < newArr.length; j++) {
	// 		if (arr[i].enable_set === 'Y') {
	// 			if (arr[i].application === newArr[j].application &&
	// 				arr[i].server_name === newArr[j].server_name &&
	// 				arr[i].enable_set === newArr[j].enable_set &&
	// 				arr[i].set_name === newArr[j].set_name &&
	// 				arr[i].set_area === newArr[j].set_area &&
	// 				arr[i].set_group === newArr[j].set_group) {
	// 				repeat = true
	// 				break
	// 			}
	// 		} else {
	// 			if (arr[i].application === newArr[j].application && arr[i].server_name === newArr[j].server_name && newArr[j].enable_set != 'Y') {
	// 				repeat = true
	// 				break
	// 			}
	// 		}
	// 	}
	// 	if (!repeat) {
	// 		newArr.push(arr[i])
	// 	}
	// }

	// 排序
	serverList = serverList.sort((a, b) => {
		const a1 = a.server_name.toLowerCase()
		const b1 = b.server_name.toLowerCase()
		if (a1 < b1) return -1
		if (a1 > b1) return 1
		return 0
	})

	// 写入缓存
	cacheData.serverData = serverList

	cacheData.dcacheData = serverList.filter(item => {
		return item.application == 'DCache';
	})

	// // 每10分钟更新
	// if (!isRefresh) {
	// 	cacheData.timer && clearTimeout(cacheData.timer)
	// 	cacheData.timer = setTimeout(() => {
	// 		TreeService.setCacheData()
	// 	}, 1000 * 60 * 10)
	// }
}
TreeService.setCacheData()

TreeService.getDCacheCommonServer = () => {
	return ['DCacheOptServer', 'ConfigServer', 'PropertyServer', 'DCacheWebServer'];
}

TreeService.isDCacheCommonServer = (server_name) => {
	return server_name === 'DCacheOptServer' ||
		server_name === 'ConfigServer' ||
		server_name === 'DCacheWebServer' ||
		server_name === 'PropertyServer';
}

/**
 * 读取缓存数据
 * @param  {String}  searchKey           搜索关键字
 * @param  {String}  uid                 操作用户
 * @param  {String}  type                类型(1: 服务, 2: DCache)
 * @return {Promise} 返回请求的promise对象
 */
TreeService.getCacheData = async (uid, type) => {
	let serverList = cacheData.serverData || []

	// 过滤Dcache
	if (type && type === '1') {
		// 应用服务
		serverList = serverList.filter(item => item.application !== 'DCache' || (item.application === 'DCache' && TreeService.isDCacheCommonServer(item.server_name)))
	} else if (type === '2') {
		// DCache
		serverList = serverList.filter(item => item.application === 'DCache' && TreeService.isDCacheCommonServer(item.server_name))
	}

	// console.log(type, serverList);

	const data = TreeService.ArrayToTree(serverList);

	console.log(type, data);
	return await TreeService.parentsBusiness(data)
}

module.exports = TreeService;