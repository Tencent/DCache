/*
 * Tars config 读取
 */
'use strict';
const ConfigParser = require('@tars/tars-utils').Config;
let tarsConfigHelper = require("@tars/tars-config");
const fs = require("fs");
let config = {};


//在tars环境 会在全局变量里面存入 服务配置 CONFIG 与 tars框架配置 TARSCONFIG 本地环境 则直接读取传入的服务配置
config.loadConfig = async function (config, configFormat) {
    if (process.env.TARS_CONFIG) {
        let tarsdata = fs.readFileSync(process.env.TARS_CONFIG, { encoding: 'utf-8' });
        if (tarsdata) {
            tarsdata = parseConf(tarsdata, configFormat);
            global.TARSCONFIG = tarsdata;
            console.log(`TARSCONFIG ReadAnd Parse Sucesss!`);
        }
        //////////////////////////////////  上面是解析TARS框架配置 下面是解析服务配置
        let helper = new tarsConfigHelper();
        let data = await helper.loadConfig(config, configFormat);
        data = parseConf(data, configFormat);
        global.CONFIG = data;
        return data;
    } else {
        let data = fs.readFileSync(config, { encoding: 'utf-8' });
        if (data) data = parseConf(data, configFormat);
        global.CONFIG = data;
        return data;
    }
}
function parseConf(content, configFormat) {
    let ret = content;
    if (configFormat === 'c') {
        let configParser = new ConfigParser();
        configParser.parseText(content, 'utf8');
        ret = configParser.data;
    } else if (configFormat == 'json') {
        ret = JSON.parse(content);
    }
    return ret;
}

module.exports = config;

