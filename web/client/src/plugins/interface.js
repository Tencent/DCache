import Axios from '@/lib/axios.js'

/**
 * 扩容
 * @param servers
 * @param appName
 * @param moduleName
 * @param status
 * @param cache_version
 */
export function expandModule({
    servers,
    appName,
    moduleName,
    status = '1',
    cache_version,
    srcGroupName = [],
    dstGroupName = []
}) {
    return Axios({
        method: 'post',
        url: '/cache/expandModule',
        data: {
            appName,
            moduleName,
            servers,
            status,
            cache_version,
            srcGroupName,
            dstGroupName
        }
    })
}

/**
 * 部署迁移
 * @param servers
 * @param appName
 * @param moduleName
 * @param status
 * @param cache_version
 * @param srcGroupName
 * @param dstGroupName
 */
export function transferDCache({
    servers,
    appName,
    moduleName,
    cache_version,
    srcGroupName,
    dstGroupName,
    transferData
}) {
    return Axios({
        method: 'post',
        url: '/cache/transferDCache',
        data: {
            servers,
            appName,
            moduleName,
            cache_version,
            srcGroupName,
            dstGroupName,
            transferData
        }
    })
}

/**
 * 非部署迁移
 * @param appName
 * @param moduleName
 * @param srcGroupName
 * @param dstGroupName
 * @param transferData
 */
export function transferDCacheGroup({
    appName,
    moduleName,
    srcGroupName,
    dstGroupName,
    transferData
}) {
    return Axios({
        method: 'post',
        url: '/cache/transferDCacheGroup',
        data: {
            appName,
            moduleName,
            srcGroupName,
            dstGroupName,
            transferData
        },
        headers: {
            'Content-Type': 'application/json'
        }
    })
}

/**
 * 获取opt发布进度
 * @param releaseId
 */
export function getReleaseProgress({
    releaseId
}) {
    return Axios({
        method: 'get',
        url: '/cache/getReleaseProgress',
        params: {
            releaseId
        }
    })
}

export function configTransfer({
    appName,
    moduleName,
    type = '1',
    srcGroupName = [],
    dstGroupName = []
}) {
    return Axios({
        method: 'post',
        url: '/cache/configTransfer',
        data: {
            appName,
            moduleName,
            type,
            srcGroupName,
            dstGroupName,
        }
    })
}

/**
 * 获取操作记录
 * @type String 0、1、2 迁移、扩容、缩容
 */
export function getRouterChange({
    type = '1'
}) {
    return Axios({
        method: 'get',
        url: '/cache/getRouterChange',
        params: {
            type
        }
    })
}

export function hasOperation({
    appName,
    moduleName,
    type,
    srcGroupName
}) {
    return Axios({
        method: 'get',
        url: '/cache/hasOperation',
        params: {
            appName,
            moduleName,
            type,
            srcGroupName,
        }
    })
}

/**
 * 停止迁移、扩容、缩容操作
 * @appName     应用名
 * @moduleName  模块名
 * @type        '0' 是迁移， '1' 是扩容， '2' 是缩容
 * @srcGroupName 原组
 * @dstGroupName 目标组
 *
 */
export function stopTransfer({
    appName = '',
    moduleName = '',
    type = '1',
    srcGroupName = [],
    dstGroupName = []
}) {
    return Axios({
        method: 'post',
        url: '/cache/stopTransfer',
        data: {
            appName,
            moduleName,
            type,
            srcGroupName,
            dstGroupName
        }
    })
}

/**
 * 重试迁移、扩容、缩容操作
 * @appName     应用名
 * @moduleName  模块名
 * @type        '0' 是迁移， '1' 是扩容， '2' 是缩容
 * @srcGroupName 原组
 * @dstGroupName 目标组
 *
 */
export function restartTransfer({
    appName = '',
    moduleName = '',
    type = '1',
    srcGroupName = [],
    dstGroupName = []
}) {
    return Axios({
        method: 'post',
        url: '/cache/restartTransfer',
        data: {
            appName,
            moduleName,
            type,
            srcGroupName,
            dstGroupName
        }
    })
}

/**
 * 删除迁移、扩容、缩容操作记录
 * @appName     应用名
 * @moduleName  模块名
 * @type        '0' 是迁移， '1' 是扩容， '2' 是缩容
 * @srcGroupName 原组
 * @dstGroupName 目标组
 *
 */
export function deleteTransfer({
    appName = '',
    moduleName = '',
    type = '1',
    srcGroupName = [],
    dstGroupName = []
}) {
    return Axios({
        method: 'post',
        url: '/cache/deleteTransfer',
        data: {
            appName,
            moduleName,
            type,
            srcGroupName,
            dstGroupName
        }
    })
}

/**
 * 缩容
 * @param appName
 * @param moduleName
 * @param srcGroupName
 */
export function reduceDcache({
    appName = '',
    moduleName = '',
    srcGroupName = []
}) {
    return Axios({
        method: 'post',
        url: '/cache/reduceDcache',
        data: {
            appName,
            moduleName,
            srcGroupName,
        }
    })
}

/**
 * 主备切换
 * @param appName
 * @param moduleName
 * @param groupName
 */
export function switchServer({
    appName = '',
    moduleName = '',
    groupName = ''
}) {
    return Axios({
        method: 'post',
        url: '/cache/switchServer',
        data: {
            appName,
            moduleName,
            groupName,
        }
    })
}

/**
 * 镜像切主
 * @param appName
 * @param moduleName
 * @param groupName
 * @param imageIdc
 */
export function switchMIServer({
    appName = '',
    moduleName = '',
    groupName = '',
    imageIdc = ''
}) {
    return Axios({
        method: 'post',
        url: '/cache/switchMIServer',
        data: {
            appName,
            moduleName,
            groupName,
            imageIdc,
        }
    })
}

/**
 * 查询主备切换
 * @param appName
 * @param moduleName
 * @param groupName
 */
export function getSwitchInfo({
    appName = '',
    moduleName = '',
    groupName = ''
}) {
    return Axios({
        method: 'get',
        url: '/cache/getSwitchInfo',
        params: {
            appName,
            moduleName,
            groupName,
        }
    })
}

/**
 * 恢复镜像
 * @param appName
 * @param moduleName
 * @param groupName
 * @param mirrorIdc
 * @param dbFlag
 * @param enableErase
 */
export function recoverMirrorStatus({
    appName,
    moduleName,
    groupName,
    mirrorIdc,
    dbFlag,
    enableErase
}) {
    return Axios({
        method: 'post',
        url: '/cache/recoverMirrorStatus',
        data: {
            appName,
            moduleName,
            groupName,
            mirrorIdc,
            dbFlag,
            enableErase
        }
    })
}

/**
 * 下线 cache 服务
 * @param appName
 * @param moduleName
 * @param serversNames
 */
export function uninstall4DCache({
    unType,
    appName,
    moduleName,
    serverNames
}) {
    return Axios({
        method: 'post',
        url: '/cache/uninstall4DCache',
        data: {
            unType,
            appName,
            moduleName,
            serverNames
        }
    })
}

/**
 * 获取 cache 服务
 * @param appName
 * @param moduleName
 */
export function getCacheServerList({
    appName,
    moduleName
}) {
    return Axios({
        method: 'get',
        url: '/cache/getCacheServerList',
        params: {
            appName,
            moduleName
        }
    })
}

/**
 * 获取模块配置
 * @param appName
 * @param moduleName
 */
export function getModuleConfig({
    appName,
    moduleName
}) {
    return Axios({
        method: 'get',
        url: '/cache/getModuleConfig',
        params: {
            appName,
            moduleName
        }
    })
}

/**
 * 获取所有的服务配置
 */
export function getConfig() {
    return Axios({
        method: 'get',
        url: '/cache/getConfig',
    })
}

/**
 * 安装发布应用的 router 和 proxy 服务
 * @param applyId
 */
export function installAndPublish({
    applyId,
    replace
}) {
    return Axios({
        method: 'get',
        url: '/cache/install_and_publish',
        params: {
            applyId,
            replace
        }
    })
}

/**
 * 获取模块的特性监控
 * @param thedate
 * @param predate
 * @param startshowtime
 * @param endshowtime
 * @param moduleName
 * @param serverName
 */
export function queryProperptyData({
    thedate,
    predate,
    startshowtime,
    endshowtime,
    moduleName,
    serverName
}) {
    return Axios({
        method: 'get',
        url: '/cache/queryProperptyData',
        params: {
            thedate,
            predate,
            startshowtime,
            endshowtime,
            moduleName,
            serverName
        }
    })
}

/**
 * 获取模块信息
 * @param moduleId
 */
export function getModuleConfigInfo({
    moduleId
}) {
    return Axios({
        method: 'get',
        url: '/get_module_config_info',
        params: {
            moduleId
        }
    })
}

/**
 * 获取模块信息
 * @param moduleId
 */
export function getModuleGroup({
    module_name
}) {
    return Axios({
        method: 'get',
        url: '/get_module_group',
        params: {
            module_name
        }
    })
}

// /**
//  * 获取模版信息
//  */
// export function templateNameList() {
//     return Axios({
//         method: 'get',
//         url: '/template_name_list',
//         params: {},
//     })
// }

export function getRegionList() {
    return Axios({
        method: 'get',
        url: '/get_region_list'
    })
}

// export function expandServerPreview({
//     application = "",
//     server_name = "",
//     set = "",
//     node_name = "",
//     expand_nodes = [""],
//     enable_set = false,
//     set_name = "",
//     set_area = "",
//     set_group = "",
//     copy_node_config = false
// }) {
//     return Axios({
//         method: 'post',
//         url: '/expand_server_preview',
//         data: {
//             application,
//             server_name,
//             set,
//             node_name,
//             expand_nodes,
//             enable_set,
//             set_name,
//             set_area,
//             set_group,
//             copy_node_config
//         },
//         headers: {
//             'Content-Type': 'application/json'
//         }
//     })
// }

// export function autoPort({
//     node_name
// }) {
//     return Axios({
//         method: 'get',
//         url: '/auto_port',
//         params: {
//             node_name
//         }
//     })
// }

// export function expandServer({
//     application = "",
//     server_name = "",
//     set = "",
//     node_name = "",
//     copy_node_config = false,
//     expand_preview_servers = [{
//         "bind_ip": "",
//         "node_name": "",
//         "obj_name": "",
//         "port": "",
//         "set": ""
//     }]
// }) {
//     return Axios({
//         method: 'post',
//         url: '/expand_server',
//         data: {
//             application,
//             server_name,
//             set,
//             node_name,
//             copy_node_config,
//             expand_preview_servers
//         },
//         headers: {
//             'Content-Type': 'application/json'
//         }
//     })
// }

export function getRouterTree() {
    return Axios({
        method: 'get',
        url: '/routerTree'
    })
}
export function routerModuleCreate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerModule/create',
        data
    })
}
export function routerModuleDelete({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerModule/delete',
        data
    })
}
export function routerModuleUpdate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerModule/update',
        data
    })
}
export function routerModuleFindOne({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerModule/find',
        params: data
    })
}
export function routerModuleList({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerModule/list',
        params: data
    })
}
export function routerGroupCreate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerGroup/create',
        data
    })
}
export function routerGroupDelete({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerGroup/delete',
        data
    })
}
export function routerGroupUpdate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerGroup/update',
        data
    })
}
export function routerGroupFindOne({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerGroup/find',
        params: data
    })
}
export function routerGroupList({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerGroup/list',
        params: data
    })
}
export function routerRecordCreate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerRecord/create',
        data
    })
}
export function routerRecordDelete({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerRecord/delete',
        data
    })
}
export function routerRecordUpdate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerRecord/update',
        data
    })
}
export function routerRecordFindOne({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerRecord/find',
        params: data
    })
}
export function routerRecordList({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerRecord/list',
        params: data
    })
}
export function routerServerCreate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerServer/create',
        data
    })
}
export function routerServerDelete({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerServer/delete',
        data
    })
}
export function routerServerUpdate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerServer/update',
        data
    })
}
export function routerServerFindOne({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerServer/find',
        params: data
    })
}
export function routerServerList({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerServer/list',
        params: data
    })
}
export function routerTransferCreate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerTransfer/create',
        data
    })
}
export function routerTransferDelete({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerTransfer/delete',
        data
    })
}
export function routerTransferUpdate({
    data = {}
}) {
    return Axios({
        method: 'post',
        url: '/routerTransfer/udpate',
        data
    })
}
export function routerTransferFindOne({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerTransfer/find',
        params: data
    })
}
export function routerTransferList({
    data = {}
}) {
    return Axios({
        method: 'get',
        url: '/routerTransfer/list',
        params: data
    })
}