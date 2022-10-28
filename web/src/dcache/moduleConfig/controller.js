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

const logger = require('../../logger');
const TarsStream = require('@tars/stream');
const ApplyService = require('./../apply/service.js');
const ModuleConfigService = require('./service.js');
const DbAccessService = require('../dbaccess/service.js');

const AdminService = require('../../common/AdminService');
const {
    DCacheOptPrx
} = require('../../common/rpc');
const {
    DCacheOptStruct
} = require('../../common/rpc/struct');

function mapCacheType(key) {
    if (key === 1) return 'hash';
    if (key === 2) return 'set';
    if (key === 3) return 'list';
    return 'zset';
}

const ModuleConfigController = {

    overwriteModuleConfig: async (ctx) => {
        try {
            const {
                admin,
                cache_module_type,
                cache_type,
                dbAccessServant,
                idc_area,
                key_type,
                max_read_flow,
                max_write_flow,
                apply_id,
                module_id,
                module_name,
                module_remark,
                per_record_avg,
                set_area,
                total_record,
                open_backup,
                cache_version,
                mkcache_struct
            } = ctx.paramsObj;
            const create_person = ctx.uid;
            const option = {
                admin,
                cache_module_type: +cache_module_type || 0,
                cache_type,
                dbAccessServant,
                idc_area,
                key_type: +key_type || 0,
                max_read_flow,
                max_write_flow,
                apply_id,
                module_id,
                module_name,
                module_remark,
                per_record_avg,
                set_area,
                total_record,
                create_person,
                open_backup,
                cache_version,
                mkcache_struct
            };
            const item = await ModuleConfigService.overwriteModuleConfig(option);
            ctx.makeResObj(200, '', item);
        } catch (err) {
            logger.error('[addModuleConfig]:', err);
            ctx.makeResObj(500, err.message);
        }
    },
    addModuleConfig: async (ctx) => {

        try {
            const {
                admin,
                cache_module_type,
                cache_type,
                dbAccessConf,
                dbAccessServant,
                idc_area,
                key_type,
                max_read_flow,
                max_write_flow,
                apply_id,
                module_id,
                module_name,
                module_remark,
                per_record_avg,
                set_area,
                total_record,
                open_backup,
                cache_version,
                mkcache_struct
            } = ctx.paramsObj;

            const hasModule = await ModuleConfigService.hasModule({
                module_name
            });

            if (hasModule) {
                ctx.makeResObj(200, '', {
                    hasModule: true
                });
            } else {

                const create_person = ctx.uid;
                const option = {
                    admin,
                    cache_module_type: +cache_module_type || 0,
                    cache_type,
                    dbAccessConf,
                    dbAccessServant,
                    idc_area,
                    key_type: +key_type || 0,
                    max_read_flow,
                    max_write_flow,
                    apply_id,
                    module_id,
                    module_name,
                    module_remark,
                    per_record_avg,
                    set_area,
                    total_record,
                    create_person,
                    open_backup,
                    cache_version,
                    mkcache_struct
                };
                const item = await ModuleConfigService.addModuleConfig(option);
                item.hasModule = false;

                ctx.makeResObj(200, '', item);
            }
        } catch (err) {
            logger.error('[addModuleConfig]:', err);
            ctx.makeResObj(500, err.message);
        }
    },
    getModuleConfigInfo: async (ctx) => {
        try {
            const {
                moduleId
            } = ctx.paramsObj;
            const queryModuleBase = ['update', 'follower'];
            const item = await ModuleConfigService.getModuleConfigInfo({
                moduleId,
                queryModuleBase
            });
            ctx.makeResObj(200, '', item);
        } catch (err) {
            logger.error('[getModuleConfigInfo]:', err);
            ctx.makeErrResObj();
        }
    },
    getModuleConfigAndServerInfo: async (ctx) => {
        try {
            const {
                moduleId
            } = ctx.paramsObj;
            const queryModuleBase = ['update', 'follower'];

            const item = await ModuleConfigService.getModuleConfigInfo({
                moduleId,
                queryModuleBase
            });

            const serverConf = await ModuleConfigService.getCacheServerConfigInfo(item.module_name);

            const dbAccess = await DbAccessService.findByModuleId({
                moduleId
            });
            ctx.makeResObj(200, '', {
                item,
                serverConf,
                dbAccess
            });
        } catch (err) {
            logger.error('[getModuleConfigAndServerInfo]:', err);
            ctx.makeErrResObj();
        }
    },
    getModuleGroup: async (ctx) => {
        try {
            const rootNode = await ApplyService.getCacheList();
            const moduleName = ctx.paramsObj.module_name;
            let cache = [];
            for (let i = 0; i < rootNode.length; i++) {
                if (moduleName.indexOf(rootNode[i].name) == 0) {
                    for (let j = 0; j < rootNode[i].children.length; j++) {
                        const item = rootNode[i].children[j];
                        if (item.moduleName == moduleName) {
                            cache = item.children;
                            break;
                        }
                    }
                }
                if (cache.length > 0) {
                    break;
                }
            }
            let group = {};
            //group-name, cache-server, type
            for (var i = 0; i < cache.length; i++) {
                if (cache[i].name.indexOf("DbAccessServer") != -1) {
                    continue;
                }
                let name = cache[i].name.split('-')[0].replace("ServerBak", "");
                let server_type;
                if (new RegExp("-1").test(cache[i].name)) {
                    server_type = 0;
                } else if (new RegExp("-2").test(cache[i].name)) {
                    server_type = 1;
                } else if (new RegExp("-3").test(cache[i].name)) {
                    server_type = 2;
                } else if (new RegExp("ServerBak").test(cache[i].name)) {
                    server_type = 1;
                }
                let groupName = name.charAt(name.length - 1);
                group[groupName] = group[groupName] || [];
                group[groupName].push({
                    server_name: cache[i].name,
                    server_type: server_type
                })
            }
            ctx.makeResObj(200, '', group);
        } catch (err) {
            logger.error('[getModuleGroup]:', err);
            ctx.makeErrResObj();
        }
    },
    getModuleConfigByModuleName: async (ctx) => {
        try {
            const {
                module_name
            } = ctx.paramsObj;
            const item = await ModuleConfigService.getModuleConfigInfoByModuleName(module_name);
            ctx.makeResObj(200, '', item);
        } catch (err) {
            logger.error('[getModuleConfigByModuleName]:', err);
            ctx.makeErrResObj();
        }
    },
    installAndPublish: async (ctx) => {
        try {
            let {
                moduleId,
                mkCache
            } = ctx.paramsObj;
            const queryModuleBase = ['update', 'follower'];
            const moduleInfo = await ModuleConfigService.getModuleConfigInfo({
                moduleId,
                queryModuleBase
            });

            const serverConf = await ModuleConfigService.getCacheServerConfigInfo(moduleInfo.module_name);
            const {
                apply_id,
                module_name,
                per_record_avg,
                ModuleBase,
                dbAccessServant,
                cache_module_type,
                key_type,
            } = moduleInfo;
            const applyInfo = await ApplyService.getApply({
                applyId: apply_id
            });
            const isMKCache = moduleInfo.cache_version === 2;
            const CacheHost = [];
            const releaseInfoOption = new TarsStream.List(DCacheOptStruct.ReleaseInfo);
            const releaseArr = [];
            let args = {
                __return: 0
            };

            mkCache = mkCache && JSON.parse(mkCache);
            // 先获取发布包id
            const defaultCachePackage = await AdminService.getPatchPackage("DCache", moduleInfo.cache_version == 1 ? "KVCacheServer" : "MKVCacheServer");
            //   where: {
            //     package_type: moduleInfo.cache_version,
            //     server: 'DCache.DCacheServerGroup',
            //     default_version: 1,
            //   },
            // });
            if (!defaultCachePackage.id) throw new Error('#module.noDefaultCachePackage#');

            const fieldParam = [];

            if (mkCache) {
                mkCache.mainKey.forEach((item) => {
                    const record = new DCacheOptStruct.RecordParam();
                    record.readFromObject({
                        fieldName: item.fieldName,
                        keyType: item.keyType,
                        dataType: item.dataType,
                        DBType: item.DBType,
                        property: item.property,
                        defaultValue: item.defaultValue,
                        maxLen: item.maxLen,
                    });
                    fieldParam.push(record);
                });
                if (isMKCache && moduleInfo.mkcache_struct === 1) {
                    mkCache.unionKey.forEach((item) => {
                        const record = new DCacheOptStruct.RecordParam();
                        record.readFromObject({
                            fieldName: item.fieldName,
                            keyType: item.keyType,
                            dataType: item.dataType,
                            DBType: item.DBType,
                            property: item.property,
                            defaultValue: item.defaultValue,
                            maxLen: item.maxLen,
                        });
                        fieldParam.push(record);
                    });
                }
                mkCache.value.forEach((item) => {
                    const record = new DCacheOptStruct.RecordParam();
                    record.readFromObject({
                        fieldName: item.fieldName,
                        keyType: item.keyType,
                        dataType: item.dataType,
                        DBType: item.DBType,
                        property: item.property,
                        defaultValue: item.defaultValue,
                        maxLen: item.maxLen,
                    });
                    fieldParam.push(record);
                });
            }
            // 主机、镜像、备机
            const optServerType = ['M', 'S', 'I'];
            let bakSrcName = {};
            serverConf.forEach((item) => {
                if (item.server_type == 0) {
                    bakSrcName[item.group_name] = `DCache.${item.server_name}`;
                }
            });

            serverConf.forEach((item) => {

                // console.log(item.dataValues);

                // for install use
                const host = new DCacheOptStruct.CacheHostParam();
                host.readFromObject({
                    serverName: `DCache.${item.server_name}`,
                    serverIp: item.server_ip,
                    templateFile: item.template_name || 'tars.default',
                    type: optServerType[item.server_type],
                    bakSrcServerName: item.server_type ? (bakSrcName[item.group_name] ? bakSrcName[item.group_name] : '') : '',
                    idc: item.area,
                    priority: item.server_type ? '2' : '1',
                    groupName: item.group_name,
                    shmSize: item.memory.toString(),
                    // 共享内存key?
                    shmKey: item.shmKey,
                    isContainer: (!!item.is_docker).toString(),
                });
                CacheHost.push(host);

                // for publish use
                const releaseInfo = new DCacheOptStruct.ReleaseInfo();
                releaseInfo.readFromObject({
                    appName: 'DCache',
                    serverName: item.server_name,
                    nodeName: item.server_ip,
                    groupName: moduleInfo.cache_version == 1 ? "KVCacheServer" : "MKVCacheServer",
                    version: defaultCachePackage.id.toString(),
                    user: ctx.uid,
                    md5: '',
                    status: 0,
                    error: '',
                    ostype: '',
                });
                releaseArr.push(releaseInfo);
            });
            if (!isMKCache && serverConf.length > 0) {
                // 一期模块
                const kvCacheConf = new DCacheOptStruct.SingleKeyConfParam();
                kvCacheConf.readFromObject({
                    avgDataSize: per_record_avg.toString(),
                    readDbFlag: 'Y',
                    enableErase: (key_type == 3 || cache_module_type == 2) ? 'Y' : 'N',
                    eraseRadio: '85%',
                    saveOnlyKey: cache_module_type == 2 ? 'Y' : 'N',
                    dbFlag: cache_module_type == 2 ? 'Y' : 'N',
                    dbAccessServant: cache_module_type === 2 ? dbAccessServant : '',
                    startExpireThread: 'Y',
                    expireDb: 'Y',
                    hotBackupDays: '3',
                    coldBackupDays: '3',
                });
                const option = new DCacheOptStruct.InstallKVCacheReq();
                option.readFromObject({
                    appName: applyInfo.name,
                    moduleName: module_name,
                    kvCacheHost: CacheHost,
                    kvCacheConf,
                    version: moduleInfo.cache_version.toString(),
                    // replace: moduleInfo.status === 2,
                    replace: true,
                });
                args = await DCacheOptPrx.installKVCacheModule(option);
            } else if (serverConf.length > 0) {
                // 二期模块
                const mkvCacheConf = new DCacheOptStruct.MultiKeyConfParam();
                mkvCacheConf.readFromObject({
                    avgDataSize: per_record_avg.toString(),
                    readDbFlag: 'Y',
                    enableErase: (key_type == 3 || cache_module_type == 2) ? 'Y' : 'N',
                    eraseRadio: '85%',
                    saveOnlyKey: cache_module_type == 2 ? 'Y' : 'N',
                    dbFlag: cache_module_type == 2 ? 'Y' : 'N',
                    dbAccessServant: cache_module_type == 2 ? dbAccessServant : '',
                    startExpireThread: 'Y',
                    expireDb: 'Y',
                    cacheType: mapCacheType(moduleInfo.mkcache_struct),
                    hotBackupDays: '3',
                    coldBackupDays: '3',
                });

                const fieldParam = [];
                mkCache.mainKey.forEach((item) => {
                    const record = new DCacheOptStruct.RecordParam();
                    record.readFromObject({
                        fieldName: item.fieldName,
                        keyType: item.keyType,
                        dataType: item.dataType,
                        property: item.property,
                        defaultValue: item.defaultValue,
                        maxLen: item.maxLen,
                    });
                    fieldParam.push(record);
                });
                if (moduleInfo.mkcache_struct === 1) {
                    mkCache.unionKey.forEach((item) => {
                        const record = new DCacheOptStruct.RecordParam();
                        record.readFromObject({
                            fieldName: item.fieldName,
                            keyType: item.keyType,
                            dataType: item.dataType,
                            property: item.property,
                            defaultValue: item.defaultValue,
                            maxLen: item.maxLen,
                        });
                        fieldParam.push(record);
                    });
                }
                mkCache.value.forEach((item) => {
                    const record = new DCacheOptStruct.RecordParam();
                    record.readFromObject({
                        fieldName: item.fieldName,
                        keyType: item.keyType,
                        dataType: item.dataType,
                        property: item.property,
                        defaultValue: item.defaultValue,
                        maxLen: item.maxLen,
                    });
                    fieldParam.push(record);
                });
                const option = new DCacheOptStruct.InstallMKVCacheReq();
                option.readFromObject({
                    appName: applyInfo.name,
                    moduleName: module_name,
                    mkvCacheHost: CacheHost,
                    mkvCacheConf,
                    fieldParam,
                    version: moduleInfo.cache_version.toString(),
                    replace: true,
                    // replace: moduleInfo.status === 2,
                });
                args = await DCacheOptPrx.installMKVCacheModule(option);
            }
            // 安装成功， 进入发布
            if (args.__return === 0) {


                if (cache_module_type == 2 && (ModuleBase.dataValues.update == 0 || ModuleBase.dataValues.update == 1)) {
                    const dbAccess = await DbAccessService.findByModuleId({
                        moduleId
                    });

                    //安装DBAccess
                    const dbAccessOption = new DCacheOptStruct.InstallDbAccessReq();
                    dbAccessOption.readFromObject({
                        appName: applyInfo.name,
                        serverName: 'DCache.' + dbAccess.servant.split(".")[1],
                        serverIp: dbAccess.dbaccess_ip.split(";"),
                        serverTemplate: "DCache.Cache",
                        isSerializated: dbAccess.isSerializated,
                        vtModuleRecord: fieldParam,
                        conf: {},
                        replace: true,
                        cacheType: moduleInfo.cache_version,
                    });

                    dbAccessOption.conf = new DCacheOptStruct.DBAccessConf();
                    dbAccessOption.conf.readFromObject({
                        isDigital: false,
                        DBNum: dbAccess.db_num,
                        DBPrefix: dbAccess.db_prefix,
                        tableNum: dbAccess.table_num,
                        tablePrefix: dbAccess.table_prefix,
                        tableEngine: 'InnoDB',
                        tableCharset: dbAccess.db_charset,
                        vDBInfo: [],
                    });

                    dbAccessOption.conf.vDBInfo = new TarsStream.List(DCacheOptStruct.DBInfo);
                    const dbInfo = new DCacheOptStruct.DBInfo();
                    dbInfo.readFromObject({
                        ip: dbAccess.db_host,
                        user: dbAccess.db_user,
                        pwd: dbAccess.db_pwd,
                        port: dbAccess.db_port,
                        charset: dbAccess.db_charset,
                    })
                    dbAccessOption.conf.vDBInfo.push(dbInfo);
                    argsDbAccess = await DCacheOptPrx.installDBAccess(dbAccessOption);
                    if (argsDbAccess.__return !== 0) {
                        throw new Error(argsDbAccess.rsp.errMsg);
                    }

                    const defaultDbAccessPackage = await AdminService.getPatchPackage("DCache", "CombinDbAccessServer", 0);

                    // const defaultDbAccessPackage = await PatchService.find({
                    //   where: {
                    //     package_type: 0,
                    //     server: 'DCache.CombinDbAccessServer',
                    //     default_version: 1,
                    //   },
                    // });
                    if (!defaultDbAccessPackage.id) throw new Error('#module.noDefaultCachePackage#');
                    const serverIps = dbAccess.dbaccess_ip.split(";");
                    for (var i = 0; i < serverIps.length; i++) {
                        const dbAccessReleaseInfo = new DCacheOptStruct.ReleaseInfo();

                        //注意, 这里和开源的不同, 为了兼容老版本!!!
                        dbAccessReleaseInfo.readFromObject({
                            appName: 'DCache',
                            serverName: dbAccess.servant.split(".")[1],
                            nodeName: serverIps[i],
                            groupName: 'CombinDbAccessServer',
                            version: defaultDbAccessPackage.id.toString(),
                            user: ctx.uid,
                            md5: '',
                            status: 0,
                            error: '',
                            ostype: '',
                        });
                        releaseArr.push(dbAccessReleaseInfo);
                    }
                }
                releaseInfoOption.readFromObject(releaseArr);
                const argsPublish = await DCacheOptPrx.releaseServer(releaseInfoOption);
                logger.info('[DCacheOptPrx.publishApp]:', argsPublish);
                if (argsPublish.__return !== 0) {
                    // 发布失败
                    throw new Error(argsPublish.releaseRsp.errMsg);
                }
                await moduleInfo.update({
                    status: 2
                });
                ctx.makeResObj(200, '', {
                    releaseRsp: argsPublish.releaseRsp,
                });
            } else {
                // 安装失败
                const {
                    errMsg
                } = args.kvCacheRsp || args.mkvCacheRsp;
                throw new Error(errMsg);
            }
        } catch (err) {
            logger.error('[installAndPublish]:', err);
            ctx.makeResObj(500, err.message);
        }
    },
    /**
     *  获取发布进度
     *  @param ctx
     * @returns {Promise.<void>}
     * */
    getReleaseProgress: async (ctx) => {
        try {
            const {
                releaseId
            } = ctx.paramsObj;
            const {
                progress,
                percent
            } = await ModuleConfigService.getReleaseProgress(releaseId);
            ctx.makeResObj(200, '', {
                progress,
                percent
            });
        } catch (err) {
            logger.error('[getReleaseProgress]:', err);
            ctx.makeResObj(500, err.message);
        }
    },
};

module.exports = ModuleConfigController;