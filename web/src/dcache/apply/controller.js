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
const TarsStream = require('@tars/stream');

const logger = require('../../logger');

const AdminService = require('../../common/AdminService');
const TreeService = require('../tree/TreeService');

const {
    DCacheOptPrx
} = require('../../common/rpc');
const {
    DCacheOptStruct
} = require('../../common/rpc/struct');

const ApplyService = require('./service');

const ApplyController = {}

ApplyController.dtree = async (ctx) => {
    try {
        // console.log("dtree");

        if (ctx.paramsObj.type && ctx.paramsObj.type === '1') {
            await TreeService.setCacheData(1)
        }

        // 获取 Dcache 的三个服务
        let tarsDcache = await TreeService.getCacheData(ctx.uid, '2');

        const rootNode = await ApplyService.buildCacheList();

        if (!rootNode) {
            ctx.makeResObj(500, "#common.dcacheSystem#", {});
            return;
        }

        tarsDcache = tarsDcache.concat(rootNode);

        ctx.makeResObj(200, '', tarsDcache);
    } catch (e) {
        logger.error('[dtree]', e.message);
        ctx.makeResObj(500, "#common.dcacheSystem#", {});
    }
}

ApplyController.routerTree = async (ctx) => {
    try {
        let rootNode = await ApplyService.buildCacheList();

        if (!rootNode || rootNode.length == 0) {
            ctx.makeResObj(500, "#common.dcacheSystem#", {});
            return;
        }

        // 只需要根目录
        rootNode = rootNode.filter(item => {
            item.children = []
            return item
        })

        ctx.makeResObj(200, '', rootNode);
    } catch (e) {
        logger.error('[dtree]', e, ctx);
        ctx.makeResObj(500, "#common.dcacheSystem#", {});
    }
}

ApplyController.getPublishSuccessModuleConfig = async (ctx) => {
    ctx.makeResObj(200, '', await getPublishSuccessModuleConfig());
}

ApplyController.getApplyList = async (ctx) => {
    try {
        const applys = await ApplyService.getApplyList();
        ctx.makeResObj(200, '', applys);
    } catch (err) {
        logger.error('[getApplyList]:', err);
        ctx.makeErrResObj();
    }
}

ApplyController.getApplyModuleList = async (ctx) => {
    try {

        if (ApplyService.rootNode.length == 0) {
            await ApplyService.buildCacheList();
        }
        // console.log(ApplyController.rootNode);

        const applyId = ctx.paramsObj.applyId;

        let modules = [];
        for (let i = 0; i < ApplyService.rootNode.length; i++) {

            if (ApplyService.rootNode[i].name == applyId) {

                ApplyService.rootNode[i].children.forEach(item => {
                    if (item.moduleName) {
                        modules.push(item);
                    }
                })

                break;
            }
        }
        ctx.makeResObj(200, '', modules);
    } catch (err) {
        logger.error('[getApplyModuleList]:', err);
        ctx.makeErrResObj();
    }
}

ApplyController.overwriteApply = async (ctx) => {
    try {
        const {
            admin,
            name,
            idcArea,
            setArea,
        } = ctx.paramsObj;
        const create_person = ctx.uid;
        let data = await ApplyService.overwriteApply({
            idc_area: idcArea,
            set_area: setArea,
            admin,
            name,
            create_person,
        });
        data.hasApply = false;
        ctx.makeResObj(200, '', data);
    } catch (err) {
        logger.error('[overwriteApply]:', err);
        ctx.makeResObj(500, err.message);
    }
}

ApplyController.addApply = async (ctx) => {
    try {
        const {
            admin,
            name,
            idcArea,
            setArea,
        } = ctx.paramsObj;
        // const create_person = 'adminUser';
        const create_person = ctx.uid;
        const hasApply = await ApplyService.hasApply({
            name
        });
        if (hasApply) {
            ctx.makeResObj(200, '', {
                hasApply: true
            });
        } else if (name.toLowerCase() == "web") {
            logger.error("error: DCache app's name can not be 'web'");
            ctx.makeErrResObj();
        } else {
            // 创建应用
            let data = await ApplyService.addApply({
                idc_area: idcArea,
                set_area: setArea,
                admin,
                name,
                create_person,
            });

            data.hasApply = false;
            ctx.makeResObj(200, '', data);
        }
    } catch (err) {
        logger.error('[addApply]:', err);
        ctx.makeResObj(500, err.message);
    }
}

ApplyController.installAndPublish = async (ctx) => {
    try {
        const {
            applyId
        } = ctx.paramsObj;

        const queryRouter = ['id', 'apply_id', 'server_name', 'server_ip', 'template_file', 'router_db_name', 'router_db_ip', 'router_db_port', 'router_db_user', 'router_db_pass', 'create_person'];
        const queryProxy = ['id', 'apply_id', 'server_name', 'server_ip', 'template_file', 'idc_area', 'create_person'];
        const apply = await ApplyService.getApply({
            applyId,
            queryRouter,
            queryProxy
        });

        const {
            name,
            Router,
            Proxy
        } = apply;
        const proxyServerIps = [];
        Proxy.forEach((proxy) => {
            // idc_area:"sz"
            // server_ip:""
            proxy.server_ip.split(';').forEach((ip) => {
                if (ip) {
                    proxyServerIps.push({
                        ip,
                        idcArea: proxy.idc_area,
                    });
                }
            });
        });
        const option = new DCacheOptStruct.InstallAppReq();
        option.readFromObject({
            appName: name,
            routerParam: {
                installRouter: true,
                serverName: `DCache.${Router.server_name}`,
                appName: name,
                serverIp: [Router.server_ip],
                templateFile: Router.template_file,
                dbName: Router.router_db_name,
                dbIp: Router.router_db_ip.trim(),
                dbPort: Router.router_db_port,
                dbUser: Router.router_db_user,
                dbPwd: Router.router_db_pass,
                remark: '',
            },
            proxyParam: {
                installProxy: true,
                serverName: `DCache.${Proxy[0].server_name}`,
                appName: name,
                serverIp: proxyServerIps,
                templateFile: Proxy[0].template_file,
            },
            version: '1.0',
            replace: true,
            // replace: apply.status === 2,
        });

        // console.log(option);

        const args = await DCacheOptPrx.installApp(option);
        // logger.info('[DCacheOptPrx.installApp]:', args);
        // {"__return":0,"instalRsp":{"errMsg":""}}
        const {
            __return,
            instalRsp
        } = args;
        if (__return === 0) {
            //  安装成功， 应用进入目录树
            await apply.update({
                status: 2
            });

            // 先获取发布包id
            const defaultProxyPackage = await AdminService.getPatchPackage("DCache", "ProxyServer", 0);

            // console.log(defaultProxyPackage);
            // const defaultProxyPackage = await PatchService.find({
            //     where: {
            //         server: 'DCache.ProxyServer',
            //         default_version: 1,
            //     },
            // });
            if (!defaultProxyPackage.id) throw new Error('#apply.noDefaultProxyPackage#');
            const defaultRouterPackage = await AdminService.getPatchPackage("DCache", "RouterServer", 0);


            // const defaultRouterPackage = await PatchService.find({
            //     where: {
            //         server: 'DCache.RouterServer',
            //         default_version: 1,
            //     },
            // });
            if (!defaultRouterPackage.id) throw new Error('#apply.noDefaultRouterPackage#');

            const defaultDbAccessPackage = await AdminService.getPatchPackage("DCache", "CombinDbAccessServer", 0);

            // const defaultDbAccessPackage = await PatchService.find({

            //     where: {
            //         server: 'DCache.CombinDbAccessServer',
            //         default_version: 1,
            //     },
            // });
            if (!defaultDbAccessPackage.id) throw new Error('#apply.noDefaultDbAccessPackage#');
            // 发布流程

            // 先发布 proxy
            let releaseInfoOption = new TarsStream.List(DCacheOptStruct.ReleaseInfo);
            let releaseArr = [];
            Proxy.forEach((proxy) => {
                proxy.server_ip.split(";").forEach((serverIp) => {

                    const releaseInfo = new DCacheOptStruct.ReleaseInfo();

                    releaseInfo.readFromObject({
                        appName: 'DCache',
                        serverName: proxy.server_name,
                        nodeName: serverIp,
                        groupName: 'ProxyServer',
                        version: `${defaultProxyPackage.id}`,
                        user: ctx.uid,
                        md5: defaultProxyPackage.md5,
                        status: 0,
                        error: '',
                        ostype: '',
                    });
                    releaseArr.push(releaseInfo);
                })

            });
            releaseInfoOption.readFromObject(releaseArr);
            const argsProxy = await DCacheOptPrx.releaseServer(releaseInfoOption);
            // {"__return":0,"releaseRsp":{"releaseId":1,"errMsg":"sucess to release server"}}
            logger.info('[DCacheOptPrx.publishApp] argsProxy:', argsProxy);
            if (argsProxy.__return !== 0) {
                throw new Error(argsProxy.releaseRsp.errMsg);
            }

            // 发布 router
            releaseInfoOption = new TarsStream.List(DCacheOptStruct.ReleaseInfo);
            const releaseInfo = new DCacheOptStruct.ReleaseInfo();
            releaseArr = [];
            releaseInfo.readFromObject({
                appName: 'DCache',
                serverName: Router.server_name,
                nodeName: Router.server_ip,
                groupName: 'RouterServer',
                version: `${defaultRouterPackage.id}`,
                user: ctx.uid,
                md5: defaultRouterPackage.md5,
                status: 0,
                error: '',
                ostype: '',
            });
            releaseArr.push(releaseInfo);
            releaseInfoOption.readFromObject(releaseArr);
            const argsRouter = await DCacheOptPrx.releaseServer(releaseInfoOption);
            logger.info('[DCacheOptPrx.publishApp] argsRouter:', argsRouter);

            if (argsRouter.__return !== 0) {
                // 发布失败
                throw new Error(argsRouter.releaseRsp.errMsg);
            }
            ctx.makeResObj(200, '', {
                proxy: argsProxy.releaseRsp,
                router: argsRouter.releaseRsp,
            });
        } else {
            // 安装失败
            throw new Error(instalRsp.errMsg);
        }
    } catch (err) {
        logger.error('[installAndPublish]:', err);
        ctx.makeResObj(500, err.message);
    }
}

ApplyController.loadRouterDb = async (ctx) => {
    try {
        const router = await ApplyService.getRouterDb();
        let data = [];
        router.forEach((item) => {
            data.push({
                id: item.dataValues.id,
                router_db_flag: item.dataValues.router_db_flag
            });
        })
        ctx.makeResObj(200, '', data);
    } catch (err) {
        logger.error('[loadRouterDb]:', err);
        ctx.makeResObj(500, err.message);
    }
}

ApplyController.getApplyAndRouterAndProxy = async (ctx) => {
    try {
        const {
            applyId
        } = ctx.paramsObj;
        const queryRouter = ['id', 'apply_id', 'server_name', 'server_ip', 'template_file', 'router_db_name', 'router_db_ip', 'router_db_port', 'router_db_user', 'router_db_pass', 'create_person'];
        const queryProxy = ['id', 'apply_id', 'server_name', 'server_ip', 'template_file', 'idc_area', 'create_person'];
        const data = await ApplyService.getApply({
            applyId,
            queryRouter,
            queryProxy
        });

        // console.log(data);

        data.Proxy.forEach((item) => {
            item.server_ip = item.server_ip.split(";").filter((x) => {
                return x && x != '';
            });
        });
        ctx.makeResObj(200, '', data);
    } catch (err) {
        logger.error('[getApplyAndRouterAndProxy]:', err);
        ctx.makeResObj(500, err.message);
    }
}

ApplyController.saveRouterProxy = async (ctx) => {
    try {
        const {
            Proxy,
            Router,
            dbMethod,
            routerDbId
        } = ctx.paramsObj;
        Proxy.forEach((item) => {
            item.server_ip = item.server_ip.join(";");
        });
        if (dbMethod) {
            const data = await ApplyService.getRouterDbById(routerDbId);
            Router.router_db_ip = data.router_db_ip;
            Router.router_db_pass = data.router_db_pass;
            Router.router_db_port = data.router_db_port;
            Router.router_db_user = data.router_db_user;
        }
        await ApplyService.saveRouterProxy({
            Proxy,
            Router
        });
        ctx.makeResObj(200, '', {});
    } catch (err) {
        logger.error('[saveRouterProxy]:', err);
        ctx.makeResObj(500, err.message);
    }
}

ApplyController.hasModule = async (ctx) => {
    try {
        const {
            serverType,
            serverName
        } = ctx.paramsObj;
        const hasModule = await ApplyService.hasModule({
            serverType,
            serverName
        });
        ctx.makeResObj(200, '', hasModule);
    } catch (err) {
        logger.error('[hasModule]:', err);
        ctx.makeResObj(500, err.message);
    }
}

module.exports = ApplyController;