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

const path = require('path');
const assert = require('assert');

const {
    DCacheOptPrx
} = require('../../common/rpc');
const {
    DCacheOptStruct
} = require('../../common/rpc/struct');
const logger = require('../../logger');

const TarsStream = require('@tars/stream');

const serverConfigService = require('./../serverConfig/service.js');
const applyService = require('./../apply/service.js');
const ModuleConfigService = require('../moduleConfig/service');

const service = {};

/**
 * 扩容服务
 * @param appName
 * @param moduleName
 * @param expandServers
 * @param cache_version
 * @param replace
 * @returns {Promise<void>}
 */
service.optExpandDCache = async function ({
    appName,
    moduleName,
    expandServers,
    cache_version,
    replace = true
}) {
    const hostServer = expandServers.find(item => item.server_type === 'M');
    const cacheHost = expandServers.map(item => ({
        serverName: `DCache.${item.server_name}`,
        serverIp: item.server_ip,
        templateFile: item.template_name || 'tars.default',
        type: item.server_type,
        // 主机的 bakSrcServerName 为 空， 备机和镜像为主机的名称
        bakSrcServerName: item.server_type === 'M' ? '' : `DCache.${hostServer.server_name}`,
        idc: item.area,
        priority: item.server_type === 'M' ? '1' : '2',
        groupName: item.group_name,
        shmSize: item.memory.toString(),
        // 共享内存key?
        shmKey: item.shmKey,
        isContainer: (!!item.is_docker).toString(),
    }));
    const option = new DCacheOptStruct.ExpandReq();
    option.readFromObject({
        appName,
        moduleName,
        cacheHost,
        cacheType: cache_version,
        version: '',
        replace,
    });
    const {
        __return,
        expandRsp,
        expandRsp: {
            errMsg
        }
    } = await DCacheOptPrx.expandDCache(option);
    assert(__return === 0, errMsg);
    return expandRsp;
};

service.releaseServer = async function ({
    expandServers
}) {
    const serverList = [];
    expandServers.forEach((item) => {
        const releaseInfo = new DCacheOptStruct.ReleaseInfo();
        releaseInfo.readFromObject({
            appName: 'DCache',
            serverName: item.server_name,
            nodeName: item.server_ip,
            groupName: cache_version == 1 ? "KVCacheServer" : "MKVCacheServer",
            version: item.patch_version,
            user: 'adminUser',
            md5: '',
            status: 0,
            error: '',
            ostype: '',
        });
        serverList.push(releaseInfo);
    });
    const option = new TarsStream.List(DCacheOptStruct.ReleaseInfo);
    option.readFromObject(serverList);

    const {
        __return,
        releaseRsp,
        releaseRsp: {
            errMsg
        }
    } = await DCacheOptPrx.releaseServer(option);
    assert(__return === 0, errMsg);
    return releaseRsp;
};

service.putInServerConfig = async function ({
    appName,
    servers
}) {
    const apply = await applyService.findApplyByName({
        appName
    });
    servers.forEach((item) => {
        item.apply_id = apply.id;
    });
    return serverConfigService.addExpandServer(servers);
};

/**
 * 创建服务迁移
 * @param TransferReq
 * @returns {Promise<void>}
 struct TransferReq
 0 require string appName;
 1 require string moduleName;
 2 require string srcGroupName;
 3 require vector<CacheHostParam> cacheHost;
 4 require DCacheType cacheType;
 5 require string version;
 */
service.transferDCache = async function ({
    appName,
    moduleName,
    srcGroupName,
    servers,
    cacheType
}) {
    const hostServer = servers.find(item => item.server_type === 'M');
    const cacheHost = servers.map(item => ({
        serverName: `DCache.${item.server_name}`,
        serverIp: item.server_ip,
        templateFile: item.template_name || 'tars.default',
        type: item.server_type,
        // 主机的 bakSrcServerName 为 空， 备机和镜像为主机的名称
        bakSrcServerName: item.server_type === 'M' ? '' : `DCache.${hostServer.server_name}`,
        idc: item.area,
        priority: item.server_type === 'M' ? '1' : '2',
        groupName: item.group_name,
        shmSize: item.memory.toString(),
        // 共享内存key?
        shmKey: item.shmKey,
        isContainer: (!!item.is_docker).toString(),
    }));
    const option = new DCacheOptStruct.TransferReq();
    option.readFromObject({
        appName,
        moduleName,
        srcGroupName,
        cacheHost,
        cacheType,
        version: '1.1.0',
    });
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = await DCacheOptPrx.transferDCache(option);
    assert(__return === 0, errMsg);
    return rsp;
};

/**
 * // 向已存在的组迁移
 * struct TransferGroupReq
 * {
 *    0 require string appName;
 *    1 require string moduleName;
 *    2 require string srcGroupName;  // 源组组名
 *    3 require string dstGroupName;  // 目的组组名
 *    4 require bool transferData;    // 是否迁移数据
 * };
 * @returns {Promise<void>}
 */
service.transferDCacheGroup = async function ({
    appName,
    moduleName,
    srcGroupName,
    dstGroupName,
    transferData
}) {
    const option = new DCacheOptStruct.TransferGroupReq();
    option.readFromObject({
        appName,
        moduleName,
        srcGroupName,
        dstGroupName,
        transferData
    });
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = await DCacheOptPrx.transferDCacheGroup(option);
    assert(__return === 0, errMsg);
    return rsp;
};

/**
 *
 * @param appName
 * @param moduleName
 * @param type // TRANSFER(0): 迁移， EXPAND(1): 扩容， REDUCE(2): 缩容
 * @param srcGroupName // 源组组名
 * @param dstGroupName // 目的组组名
 * 0 require string appName;
 * 1 require string moduleName;
 * 2 require TransferType type; // TRANSFER(0): 迁移， EXPAND(1): 扩容， REDUCE(2): 缩容
 * 3 optional vector<string> srcGroupName; // 源组组名
 * 4 optional vector<string> dstGroupName; // 目的组组名
 * 5 optional bool transferData=true;      // 是否迁移数据
 * @returns {Promise.<void>}
 */
service.configTransfer = async function ({
    appName,
    moduleName,
    type = 1,
    srcGroupName = [],
    dstGroupName = [],
    transferData = true,
}) {
    const option = new DCacheOptStruct.ConfigTransferReq();
    option.readFromObject({
        appName,
        moduleName,
        type,
        srcGroupName,
        dstGroupName,
        transferData,
    });
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = await DCacheOptPrx.configTransfer(option);
    assert(__return === 0, errMsg);
    return rsp;
};
/**
 * 查询路由变更(迁移，扩容，缩容)
 * cond 查询条件: <"appName", "Test">表示查询应用名为Test的变更信息
 *   map的key是TransferRecord中的成员: appName, moduleName, srcGroupName, dstGroupName, status, type
 *   map的key都是字符串
 *   status: "0"-新增迁移任务，"1"-配置阶段完成，"2"-发布完成，"3"-正在迁移，"4"-完成，5""-停止
 *   type: "0"-迁移, "1"-扩容, "2"-缩容, "3"-路由整理
 * index: 获取数据的索引(从0开始)
 * number: 一次获取数据的个数(获取全部数据 number设置为-1)
 */
service.getRouterChange = async function ({
    appName = '',
    moduleName = '',
    srcGroupName = '',
    dstGroupName = '',
    status = '',
    type = '',
}) {
    const option = new DCacheOptStruct.RouterChangeReq();
    const cond = {};
    if (appName) cond.appName = appName;
    if (moduleName) cond.moduleName = moduleName;
    if (status) cond.status = status;
    if (srcGroupName) cond.srcGroupName = srcGroupName;
    if (dstGroupName) cond.dstGroupName = dstGroupName;
    if (type) cond.type = type;
    // cond.type = type;
    option.readFromObject({
        index: 0,
        number: -1,
        cond,
    });
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = await DCacheOptPrx.getRouterChange(option);
    assert(__return === 0, errMsg);
    return rsp;
};

/**
 * 缩容 opt 接口
 * 0 require string appName;
 * 1 require string moduleName;
 * 2 require vector<string> srcGroupName; // 源组组名
 *
 * */
service.reduceDCache = async function ({
    appName = '',
    moduleName = '',
    srcGroupName = []
}) {
    const option = new DCacheOptStruct.ReduceReq();
    option.readFromObject({
        appName,
        moduleName,
        srcGroupName,
    });
    const {
        __return,
        reduceRsp,
        reduceRsp: {
            errMsg
        }
    } = await DCacheOptPrx.reduceDCache(option);
    assert(__return === 0, errMsg);
    const configTransferRsp = await service.configTransfer({
        appName,
        moduleName,
        type: 2,
        srcGroupName,
        dstGroupName: [],
    });
    return {
        reduceRsp,
        configTransferRsp,
    };
};

/**
 * 停止迁移、扩容、缩容操作
 * @appName     应用名
 * @moduleName  模块名
 * @type        '0' 是迁移， '1' 是扩容， '2' 是缩容
 * @srcGroupName 原组
 * @dstGroupName 目标组
 *
 */
service.stopTransfer = async function ({
    appName = '',
    moduleName = '',
    type = '1',
    srcGroupName = '',
    dstGroupName = '',
}) {
    const option = new DCacheOptStruct.StopTransferReq();
    option.readFromObject({
        appName,
        moduleName,
        type: `${type}`,
        srcGroupName,
        dstGroupName,
    });
    const res = await DCacheOptPrx.stopTransfer(option);
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = res;
    assert(__return === 0, errMsg);
    return rsp;
};

/**
 * 重试迁移、扩容、缩容操作
 * @appName     应用名
 * @moduleName  模块名
 * @type        '0' 是迁移， '1' 是扩容， '2' 是缩容
 * @srcGroupName 原组
 * @dstGroupName 目标组
 *
 */
service.restartTransfer = async function ({
    appName = '',
    moduleName = '',
    type = '1',
    srcGroupName = '',
    dstGroupName = ''
}) {
    const option = new DCacheOptStruct.RestartTransferReq();
    option.readFromObject({
        appName,
        moduleName,
        type,
        srcGroupName,
        dstGroupName,
    });
    const res = await DCacheOptPrx.restartTransfer(option);
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = res;
    assert(__return === 0, errMsg);
    return rsp;
};

/**
 * 删除迁移、扩容、缩容操作记录
 * @appName     应用名
 * @moduleName  模块名
 * @type        '0' 是迁移， '1' 是扩容， '2' 是缩容
 * @srcGroupName 原组
 * @dstGroupName 目标组
 *
 */
service.deleteTransfer = async function ({
    appName = '',
    moduleName = '',
    type = '1',
    srcGroupName = '',
    dstGroupName = '',
}) {
    const option = new DCacheOptStruct.DeleteTransferReq();
    option.readFromObject({
        appName,
        moduleName,
        type: `${type}`,
        srcGroupName,
        dstGroupName,
    });
    const res = await DCacheOptPrx.deleteTransfer(option);
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = res;
    assert(__return === 0, errMsg);
    return rsp;
};

/**
 * 主备切换
 * 0 require string appName;
 * 1 require string moduleName;
 * 2 require string groupName;
 * 3 optional bool forceSwitch; // 是否强制切换
 * 4 optional int diffBinlogTime;
 * @returns {Promise<void>}
 */
service.switchServer = async function ({
    appName,
    moduleName,
    groupName,
    forceSwitch = false,
    diffBinlogTime = 5,
}) {
    const option = new DCacheOptStruct.SwitchReq();
    option.readFromObject({
        appName,
        moduleName,
        groupName,
        forceSwitch,
        diffBinlogTime,
    });
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = await DCacheOptPrx.switchServer(option);
    assert(__return === 0, errMsg);
    return rsp;
};

// 主备切换
service.switchMainBackup = async function ({
    moduleName,
    groupName
}) {
    const main = await serverConfigService.findOne({
        where: {
            module_name: moduleName,
            group_name: groupName,
            server_type: 1,
        },
    });
    const backup = await serverConfigService.findOne({
        where: {
            module_name: moduleName,
            group_name: groupName,
            server_type: 0,
        },
    });

    if (main && backup) {
        return Promise.all([
            main.update({
                server_type: 0
            }),
            backup.update({
                server_type: 1
            }),
        ]);
    } else {
        return {};
    }
};

/**
 * 镜像切主
 * 0 require string appName;
 * 1 require string moduleName;
 * 2 require string groupName;
 * 3 optional bool forceSwitch; // 是否强制切换
 * 4 optional int diffBinlogTime;
 * @returns {Promise<void>}
 */
service.switchMIServer = async function ({
    appName,
    moduleName,
    groupName,
    imageIdc,
    forceSwitch = false,
    diffBinlogTime = 5,
}) {
    const option = new DCacheOptStruct.SwitchMIReq();
    option.readFromObject({
        appName,
        moduleName,
        groupName,
        imageIdc,
        forceSwitch,
        diffBinlogTime,
    });
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = await DCacheOptPrx.switchMIServer(option);
    assert(__return === 0, errMsg);
    return rsp;
};

// 主备切换
service.switchMainImage = async function ({
    moduleName,
    groupName,
    imageIdc
}) {
    const main = await serverConfigService.findOne({
        where: {
            module_name: moduleName,
            group_name: groupName,
            server_type: 2,
            area: imageIdc,
        },
    });
    const image = await serverConfigService.findOne({
        where: {
            module_name: moduleName,
            group_name: groupName,
            server_type: 0,
        },
    });

    if (main && image) {
        return Promise.all([
            main.update({
                server_type: 0
            }),
            image.update({
                server_type: 2
            }),
        ]);
    } else {
        return {};
    }
};

/*
* 发生镜像切换后，恢复镜像状态时使用
* // 发生镜像切换后，恢复镜像状态时使用
struct RecoverMirrorReq
{
    0 require string appName;
    1 require string moduleName;
    2 require string groupName;
    3 require string mirrorIdc;
};
*/
service.recoverMirrorStatus = async function ({
    appName,
    moduleName,
    groupName,
    mirrorIdc
}) {
    const option = new DCacheOptStruct.RecoverMirrorReq();
    option.readFromObject({
        appName,
        moduleName,
        groupName,
        mirrorIdc
    });
    const {
        __return,
        rsp: {
            errMsg
        }
    } = await DCacheOptPrx.recoverMirrorStatus(option);
    assert(__return === 0, errMsg);
    return true;
};

/*
* 查询切换信息请求
* cond 查询条件，<"appName", "Test">表示查询应用名为Test的切换信息，
*   索引值为 SwitchRecord 中的成员: appName, moduleName, groupName, msterServer, slaveServer, mirrorIdc, masterMirror, slaveMirror, switchTime, switchType, switchResult, groupStatus,
*   除switchTime外其它为字符串值，switchTime为时间范围，其条件格式为从小到大的时间且以"|"分隔:"2019-01-01 12:00:00|2019-01-01 13:00:00"
*   switchType: "0"-主备切换，"1"-镜像主备切换，"2"-无镜像备机切换，"3"-备机不可读
*   switchResult: "0"-正在切换, "1"-切换成功, "2"-未切换, "3"-切换失败
*   groupStatus: "0"-标识读写, "1"-标识只读, "2"-镜像不可用
* index: 获取数据的索引(从0开始)
* number: 一次获取数据的个数(获取全部数据 number设置为-1)
* struct SwitchInfoReq
{
  0 require map<string, string> cond;
  1 require int index;
  2 require int number;
};
*/
service.getSwitchInfo = async function ({
    appName = '',
    moduleName = '',
    groupName = '',
    msterServer = '',
    slaveServer = '',
}) {
    const option = new DCacheOptStruct.SwitchInfoReq();
    const cond = {};
    if (appName) cond.appName = appName;
    if (moduleName) cond.moduleName = moduleName;
    if (groupName) cond.groupName = groupName;
    if (msterServer) cond.msterServer = msterServer;
    if (slaveServer) cond.slaveServer = slaveServer;
    option.readFromObject({
        index: 0,
        number: -1,
        cond,
    });
    const {
        __return,
        rsp,
        rsp: {
            errMsg
        }
    } = await DCacheOptPrx.getSwitchInfo(option);
    assert(__return === 0, errMsg);
    return rsp;
};

service.getReleaseProgress = async function (releaseId, appName, moduleName, type, srcGroupName, dstGroupName, transferData) {
    try {
        const {
            percent
        } = await ModuleConfigService.getReleaseProgress(releaseId);
        if (+percent !== 100) {
            setTimeout(service.getReleaseProgress.bind(this, releaseId, appName, moduleName, type, srcGroupName, dstGroupName, transferData), 1500);
        } else {
            await service.configTransfer({
                appName,
                moduleName,
                type,
                srcGroupName,
                dstGroupName,
                transferData
            });
        }
    } catch (err) {
        logger.error('[getReleaseProgress]:', err);
    }
};

/**
 * 下线服务
 * @returns {Promise<void>}
 * struct UninstallReq
 * {
 *    0 require UninstallType unType;     //下线类型,0为单个Cache服务 1为按服务组,2为按模块,3为配额单服务下线
 *    1 require string appName;
 *    2 require string moduleName;        //模块名
 *    3 optional string serverName;       //cache服务名, 当unType!=0时可为空
 *    4 optional string groupName;        //cache服务组名, 当unType!=1时可为空
 * };
 */
service.uninstallModule4DCache = async function ({
    unType = 2,
    appName,
    moduleName,
    serverName = '',
    groupName = ''
}) {
    const option = new DCacheOptStruct.UninstallReq();
    option.readFromObject({
        unType,
        appName,
        moduleName,
        serverName,
        groupName,
    });
    const {
        __return,
        uninstallRsp,
        uninstallRsp: {
            errMsg
        }
    } = await DCacheOptPrx.uninstall4DCache(option);
    assert(__return === 0, errMsg);
    const module = await ModuleConfigService.getModuleConfigByName({
        moduleName
    });
    if (module) await ModuleConfigService.removeModuleConfig({
        module_name: moduleName,
        module_id: module.module_id
    });
    await service.getUninstallPercent(option);
    return uninstallRsp;
};

service.uninstallServer4DCache = async function ({
    unType = 0,
    appName,
    moduleName,
    serverName,
    groupName = ''
}) {
    const option = new DCacheOptStruct.UninstallReq();
    option.readFromObject({
        unType,
        appName,
        moduleName,
        serverName,
        groupName,
    });
    const {
        __return,
        uninstallRsp,
        uninstallRsp: {
            errMsg
        }
    } = await DCacheOptPrx.uninstall4DCache(option);
    assert(__return === 0, errMsg);
    await serverConfigService.removeServer({
        server_name: serverName.replace('DCache.', '')
    });
    await service.getUninstallPercent(option);
    return uninstallRsp;
};

service.getUninstallPercent = async function (option) {
    const {
        __return,
        progressRsp: {
            percent,
            errMsg
        }
    } = await DCacheOptPrx.getUninstallPercent(option);
    assert(__return === 0, errMsg);
    if (percent !== 100) {
        await new Promise((resolve) => {
            setTimeout(async () => {
                await service.getUninstallPercent(option);
                resolve();
            }, 500);
        });
    }
};

module.exports = service;