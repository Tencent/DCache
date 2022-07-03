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

// const WebConf = require('../config/webConf');

const RegionController = require('./region/controller.js');
const ApplyController = require('./apply/controller.js');
const ModuleController = require('./module/controller.js');
const ModuleConfigController = require('./moduleConfig/controller.js');
const ServerConfigController = require('./serverConfig/controller.js');
const ProxyController = require('./proxy/controller.js');
const RouterController = require('./router/controller.js');
const ModuleOperation = require('./moduleOperation/controller');
const DbAccessController = require('./dbaccess/controller.js');
const routerManageController = require('./routerManage/controller')

const {
    getConfig,
    addConfig,
    deleteConfig,
    editConfig,
    getModuleConfig,
    getServerConfig,
    getServerNodeConfig,
    addServerConfigItem,
    deleteServerConfigItem,
    updateServerConfigItem,
    updateServerConfigItemBatch,
    deleteServerConfigItemBatch,
} = require('./config/controller.js');

const dcacheApiConf = [

    ['get', '/dtree', ApplyController.dtree],
    // 地区
    ['get', '/get_region_list', RegionController.getRegionList],
    ['post', '/add_region', RegionController.addRegion],
    ['get', '/delete_region', RegionController.deleteRegion],
    ['post', '/update_region', RegionController.updateRegion],

    // 应用
    ['post', '/add_apply', ApplyController.addApply],
    ['post', '/overwrite_apply', ApplyController.overwriteApply],
    ['get', '/load_router_db', ApplyController.loadRouterDb],
    ['get', '/get_apply_and_router_and_proxy', ApplyController.getApplyAndRouterAndProxy],
    ['post', '/save_router_proxy', ApplyController.saveRouterProxy],
    ['get', '/get_apply_list', ApplyController.getApplyList],
    ['get', '/cache/install_and_publish', ApplyController.installAndPublish],
    ['get', '/cache/hasModule', ApplyController.hasModule],
    ['get', '/cache/getPublishSuccessModuleConfig', ApplyController.getPublishSuccessModuleConfig],

    // proxy
    ['post', '/delete_apply_proxy', ProxyController.removeProxyById],

    ['post', '/cache/removeProxy', ProxyController.removeProxy],

    // router
    ['post', '/cache/removeRouter', RouterController.removeRouter],


    ['get', '/load_access_db', DbAccessController.loadAccessDb],

    // 模块
    ['get', '/has_module_info', ModuleController.hasModuleInfo, {
        module_name: 'noEmpty'
    }],
    ['post', '/add_module_base_info', ModuleController.addModuleBaseInfo],
    ['get', '/get_module_info', ModuleController.getModuleInfo],
    // 模块特性监控
    ['get', '/cache/queryProperptyData', ModuleController.queryProperptyData, {
        thedate: 'notEmpty',
        predate: 'notEmpty',
        moduleName: 'notEmpty',
    }],

    ['get', '/get_module_config_info', ModuleConfigController.getModuleConfigInfo],
    ['get', '/get_module_component', ModuleConfigController.getModuleComponent],
    ['get', '/get_module_full_info', ModuleConfigController.getModuleConfigAndServerInfo],
    ['get', '/get_module_config_info_by_module_name', ModuleConfigController.getModuleConfigByModuleName, {
        module_name: 'noEmpty'
    }],
    ['get', '/get_module_group', ModuleConfigController.getModuleGroup, {
        module_name: 'noEmpty'
    }],
    ['post', '/overwrite_module_config', ModuleConfigController.overwriteModuleConfig],
    ['post', '/add_server_config', ServerConfigController.addServerConfig],
    ['post', '/add_module_config', ModuleConfigController.addModuleConfig],
    ['get', '/module_install_and_publish', ModuleConfigController.installAndPublish],
    ['get', '/get_module_release_progress', ModuleConfigController.getReleaseProgress],
    ['get', '/get_cache_server_list', ServerConfigController.getCacheServerListOld],
    ['post', '/cache/removeServer', ServerConfigController.removeServer],
    ['get', '/cache/getModuleStruct', ModuleController.getModuleStruct],
    ['get', '/cache/getReleaseProgress', ModuleConfigController.getReleaseProgress],

    // 模块操作
    ['post', '/cache/expandModule', ModuleOperation.expandDCache],
    ['post', '/cache/configTransfer', ModuleOperation.configTransfer, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        type: 'notEmpty',
    }],
    ['post', '/cache/reduceDcache', ModuleOperation.reduceDcache, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        srcGroupName: 'notEmpty',
    }],
    ['post', '/cache/stopTransfer', ModuleOperation.stopTransfer, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        type: 'notEmpty',
    }],
    ['post', '/cache/restartTransfer', ModuleOperation.restartTransfer, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        type: 'notEmpty',
    }],
    ['post', '/cache/deleteTransfer', ModuleOperation.deleteTransfer, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        type: 'notEmpty',
    }],
    ['post', '/cache/deleteOperation', ModuleOperation.deleteOperation, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        type: 'notEmpty',
    }],
    ['post', '/cache/switchServer', ModuleOperation.switchServer, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        groupName: 'notEmpty',
    }],
    ['post', '/cache/switchMIServer', ModuleOperation.switchMIServer, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        groupName: 'notEmpty',
        imageIdc: 'notEmpty',
    }],
    ['get', '/cache/hasOperation', ModuleOperation.hasOperation, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
    }],
    // 获取cache 服务列表
    ['get', '/cache/getCacheServerList', ServerConfigController.getCacheServerList, {
        appName: 'notEmpty',
        moduleName: 'notEmpty'
    }],
    // 查询迁移管理
    ['get', '/cache/getRouterChange', ModuleOperation.getRouterChange],
    // 查询主备切换
    ['get', '/cache/getSwitchInfo', ModuleOperation.getSwitchInfo],
    // 恢复镜像状态
    ['post', '/cache/recoverMirrorStatus', ModuleOperation.recoverMirrorStatus, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        groupName: 'notEmpty',
        mirrorIdc: 'notEmpty',
        dbFlag: 'notEmpty',
        enableErase: 'notEmpty',
    }],
    // 部署迁移
    ['post', '/cache/transferDCache', ModuleOperation.transferDCache],
    // 非部署迁移
    ['post', '/cache/transferDCacheGroup', ModuleOperation.transferDCacheGroup, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        srcGroupName: 'notEmpty',
        dstGroupName: 'notEmpty',
        transferData: 'notEmpty',
    }],
    // 下线 cache 服务
    ['post', '/cache/uninstall4DCache', ModuleOperation.uninstall4DCache, {
        appName: 'notEmpty',
        moduleName: 'notEmpty',
        serverNames: 'notEmpty',
    }],
    // cache 配置中心
    ['get', '/cache/getConfig', getConfig],
    ['post', '/cache/addConfig', addConfig, {
        item: 'notEmpty',
        path: 'notEmpty',
        period: 'notEmpty',
        reload: 'notEmpty',
        remark: 'notEmpty',
    }],
    ['get', '/cache/deleteConfig', deleteConfig, {
        id: 'notEmpty'
    }],
    ['post', '/cache/editConfig', editConfig, {
        id: 'notEmpty',
        item: 'notEmpty',
        path: 'notEmpty',
        period: 'notEmpty',
        reload: 'notEmpty',
        remark: 'notEmpty',
    }],
    ['get', '/cache/getModuleConfig', getModuleConfig, {
        moduleName: 'notEmpty'
    }],
    ['get', '/cache/getServerConfig', getServerConfig, {
        moduleName: 'notEmpty',
        serverName: 'notEmpty',
        nodeName: 'notEmpty',
    }],
    ['get', '/cache/getServerNodeConfig', getServerNodeConfig, {
        serverName: 'notEmpty',
        nodeName: 'notEmpty'
    }],
    ['post', '/cache/addServerConfigItem', addServerConfigItem, {
        itemId: 'notEmpty',
        configValue: 'notEmpty'
    }],
    ['get', '/cache/deleteServerConfigItem', deleteServerConfigItem, {
        id: 'notEmpty'
    }],
    ['get', '/cache/updateServerConfigItem', updateServerConfigItem, {
        id: 'notEmpty',
        configValue: 'notEmpty'
    }],
    ['post', '/cache/updateServerConfigItemBatch', updateServerConfigItemBatch, {
        serverConfigList: 'notEmpty'
    }],
    ['post', '/cache/deleteServerConfigItemBatch', deleteServerConfigItemBatch, {
        serverConfigList: 'notEmpty'
    }],


    ['get', '/routerTree', ApplyController.routerTree],
    ['post', '/routerModule/create', routerManageController.moduleCreate],
    ['post', '/routerModule/delete', routerManageController.moduleDestroy, {
        id: 'notEmpty'
    }],
    ['post', '/routerModule/update', routerManageController.moduleUpdate, {
        id: 'notEmpty'
    }],
    ['get', '/routerModule/find', routerManageController.moduleFindOne, {
        id: 'notEmpty'
    }],
    ['get', '/routerModule/list', routerManageController.moduleFindAndCountAll],
    ['post', '/routerGroup/create', routerManageController.groupCreate],
    ['post', '/routerGroup/delete', routerManageController.groupDestroy, {
        id: 'notEmpty'
    }],
    ['post', '/routerGroup/update', routerManageController.groupUpdate, {
        id: 'notEmpty'
    }],
    ['get', '/routerGroup/find', routerManageController.groupFindOne, {
        id: 'notEmpty'
    }],
    ['get', '/routerGroup/list', routerManageController.groupFindAndCountAll],
    ['post', '/routerRecord/create', routerManageController.recordCreate],
    ['post', '/routerRecord/delete', routerManageController.recordDestroy, {
        id: 'notEmpty'
    }],
    ['post', '/routerRecord/update', routerManageController.recordUpdate, {
        id: 'notEmpty'
    }],
    ['get', '/routerRecord/find', routerManageController.recordFindOne, {
        id: 'notEmpty'
    }],
    ['get', '/routerRecord/list', routerManageController.recordFindAndCountAll],
    ['post', '/routerServer/create', routerManageController.serverCreate],
    ['post', '/routerServer/delete', routerManageController.serverDestroy, {
        id: 'notEmpty'
    }],
    ['post', '/routerServer/update', routerManageController.serverUpdate, {
        id: 'notEmpty'
    }],
    ['get', '/routerServer/find', routerManageController.serverFindOne, {
        id: 'notEmpty'
    }],
    ['get', '/routerServer/list', routerManageController.serverFindAndCountAll],
    ['post', '/routerTransfer/create', routerManageController.transferCreate],
    ['post', '/routerTransfer/delete', routerManageController.transferDestroy, {
        id: 'notEmpty'
    }],
    ['post', '/routerTransfer/update', routerManageController.transferUpdate, {
        id: 'notEmpty'
    }],
    ['get', '/routerTransfer/find', routerManageController.transferFindOne, {
        id: 'notEmpty'
    }],
    ['get', '/routerTransfer/list', routerManageController.transferFindAndCountAll],
];

module.exports = {
    dcacheApiConf
};