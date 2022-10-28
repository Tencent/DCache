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

const Koa = require('koa');
const onerror = require('koa-onerror');
const bodyparser = require('koa-bodyparser');
const TarsConfig = require("@tars/config");
const Configure = require('@tars/utils').Config;
const localeMidware = require('./midware/localeMidware');
const http = require('http');
const path = require('path');
const logger = require('./logger');
let webConf = require('./config/webConf');

const {
    Serve
} = require("static-koa-router");
const KoaRouter = require("koa-router");

const app = new Koa();

//信任proxy头部，支持 X-Forwarded-Host
app.proxy = true;
// error handler
onerror(app);

const appInitialize = () => {
    app.use(bodyparser());

    //国际化多语言中间件
    app.use(localeMidware);

    const {
        pageRouter,
        localeApiRouter,
        dcacheApiRouter,
    } = require('./midware');

    app.use(pageRouter.routes(), pageRouter.allowedMethods({
        throw: true
    }));
    app.use(localeApiRouter.routes(), localeApiRouter.allowedMethods({
        throw: true
    }));
    app.use(dcacheApiRouter.routes()).use(dcacheApiRouter.allowedMethods());
    //注意这里路由前缀一样的, 接口处理以后不要next, 否则匹配到静态路由了
    const staticRouter = new KoaRouter({
        prefix: webConf.path
    });

    Serve(path.join(__dirname, '../client/dist'), staticRouter);

    app.use(staticRouter.routes());

}
const registerPlugin = async () => {

    logger.info("registerPlugin");
    const AdminService = require("./common/AdminService");

    if (process.env.TARS_CONFIG) {

        let config = new Configure();
        config.parseFile(process.env.TARS_CONFIG);


        try {
            const rst = await AdminService.registerPlugin("缓存管理平台", "DCacheWeb", config.get("tars.application.server.app") + "." + config.get("tars.application.server.server") + ".WebObj", 1, webConf.path);

            console.log(rst);
        } catch (e) {
            console.log(e.message);
        }

        webConf.dcacheObj = config.get("tars.application.server.app") + ".DCacheOptServer.DCachOptObj";


    } else {

        try {
            const rst = await AdminService.registerPlugin("缓存管理平台", "DCacheWeb", "DCache.DCacheWebServer.WebObj", 1, webConf.path);

            console.log(rst);
        } catch (e) {
            console.log("error:", e.message);
        }

        webConf.dcacheObj = "DCache.DCacheOptServer.DCachOptObj";

    }
}

const initialize = async () => {

    if (process.env.TARS_CONFIG) {
        const tarsConfig = new TarsConfig();

        let conf = await tarsConfig.loadConfig(`config.json`, {
            format: tarsConfig.FORMAT.JSON
        });

        Object.assign(webConf, conf);

    } else {
        const client = require("@tars/rpc/protal.js").client;

        client.setProperty("locator", webConf.webConf.locator);

    }

    await registerPlugin();

    console.log(webConf);

    const hostname = process.env.HTTP_IP || "0.0.0.0";
    const port = process.env.HTTP_PORT || webConf.webConf.port;

    let server = http.createServer(app.callback());
    server.listen(port, hostname, function () {

        appInitialize();

        console.log(`Server has been started successfully, hostname:${hostname}, port: ${port}`);

    });
    server.on('error',
        // 服务错误回调
        (error) => {
            if (error.syscall !== 'listen') {
                throw error;
            }

            switch (error.code) {
                case 'EACCES':
                    console.log('requires elevated privileges');
                    process.exit(1);
                    break;
                case 'EADDRINUSE':
                    console.log(port + ' is already in use');
                    process.exit(1);
                    break;
                default:
                    throw error;
            }
        });
}

initialize();