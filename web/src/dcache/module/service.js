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

const moduleDao = require('./dao.js');

const ModuleService = {};

/**
 * 获取特性监控数据
 * @param moduleName
 * @param serverName
 * @param date
 * @param startTime
 * @param endTime
 * @returns {Promise<void>}
 * struct QueryPropReq
 {
    0 require string moduleName;    //Dcache模块名
    1 require string serverName;    //服务名 DCache.xxx, 不填表示查询模块下所有服务合并统计数据，填"*"表示列出所有服务的独立数据
    2 require vector<string> date;  //需要查询的日期，日期格式20190508
    3 require string startTime;     //e.g. 0800
    4 require string endTime;       //e.g. 2360
};
 */
ModuleService.queryProperptyData = async function ({
    moduleName,
    serverName = '',
    date = ['thedate', 'predate'],
    startTime = '0000',
    endTime = '2360',
    get
}) {
    let formatRsp = [];
    const option = new DCacheOptStruct.QueryPropReq();

    option.readFromObject({
        moduleName,
        serverName,
        date,
        startTime,
        endTime
    });
    const args = await DCacheOptPrx.queryProperptyData(option);

    const {
        __return,
        rsp
    } = args;
    assert(__return === 0, 'get data error');
    // if (get) return rsp;

    //console.log('ModuleService:', rsp);

    if (serverName === '') {
        // 获取服务列表
        // formatRsp = { data: rsp, keys: [] };
        formatRsp = ModuleService.findServersFormat(rsp, date);
    } else {
        // 获取正常数据
        formatRsp = ModuleService.normalFormat(rsp);
    }

    //console.log('ModuleService formatRsp:', formatRsp);
    // return { rsp, formatRsp };
    return formatRsp;
};

/**
 * 正常格式化特性监控数据，获取特性监控参数 serverName 为空的时候
 * @param monitorData
 * @returns {{data: Array, keys: Array}}
 */
ModuleService.normalFormat = function propertyMonitorNormalFormat(monitorData = [{
    moduleName: '',
    serverName: '',
    date: '',
    data: [{
        propData: {},
        timeStamp: '0000'
    }]
}]) {
    const data = [];
    let keys = [];

    //console.log('normalFormat 1', monitorData);

    // if (!monitorData.data || monitorData.data.length == 0) {
    //     return {};
    // }
    const [{
        moduleName,
        serverName,
        data: theData
    }, {
        data: preData
    }] = monitorData;

    //console.log('normalFormat 2', data, serverName);

    for (let hour = 0; hour < 24; hour++) {
        for (let min = 0; min <= 60; min += 5) {
            // 格式化时间为 000 =》 2360
            const hourStr = hour < 10 ? `0${hour}` : `${hour}`;
            const minStr = min < 10 ? `0${min}` : `${min}`;
            const timeStr = hourStr + minStr;
            // 当日、对比日的该时间戳是否有数据， 某一天有数据，就添加一个时间戳数据
            const theItem = theData.find(item => item.timeStamp === timeStr);
            const preItem = preData.find(item => item.timeStamp === timeStr);
            const hasItem = theItem || preItem;
            if (hasItem) {
                const {
                    propData,
                    timeStamp
                } = hasItem;
                const option = {
                    show_time: timeStamp,
                    moduleName,
                    serverName: serverName || '%'
                };
                keys = Object.keys(propData);
                keys.forEach((item) => {
                    option[`the_${item}`] = theItem ? theItem.propData[item] : '--';
                    option[`pre_${item}`] = preItem ? preItem.propData[item] : '--';
                });
                data.push(option);
            }
        }
    }

    //console.log('normalFormat result:', data);

    return {
        data,
        keys
    };
};

ModuleService.findServersFormat = function propertyMonitorFindServersFormat(monitorData = [{
    moduleName: '',
    serverName: '',
    date: '',
    data: [{
        propData: {},
        timeStamp: ''
    }]
}], [thedate, predate]) {
    const theData = monitorData.filter(item => item.date === thedate);
    const preData = monitorData.filter(item => item.date === predate);

    // console.log('findServersFormat:', data, timeStamp);

    let keys = [];
    const formatData = [];
    theData.forEach((theItem) => {
        const {
            data,
            date,
            moduleName,
            serverName
        } = theItem;
        const preItem = preData.find(item => item.serverName === serverName);
        const {
            data: preItemData
        } = preItem || {};
        data.forEach(({
            propData = {}
        }, index) => {
            const option = {
                show_time: data[index].timeStamp,
                date,
                moduleName,
                serverName
            };
            keys = Object.keys(propData);
            keys.forEach((property) => {
                option[`the_${property}`] = propData[property] || '--';
                option[`pre_${property}`] = preItemData ? preItemData[index].propData[property] : '--';
            });
            formatData.push(option);
        });
    });
    return {
        data: formatData,
        keys
    };
};


ModuleService.addModuleBaseInfo = async function ({
    apply_id,
    module_name,
    follower,
    update,
    create_person,
    modify_time
}) {
    return moduleDao.addModuleBaseInfo({
        apply_id,
        module_name,
        follower,
        update,
        create_person,
        modify_time,
    });
};

ModuleService.getModuleInfo = async function ({
    id
}) {
    return moduleDao.findOne({
        where: {
            id
        }
    });
};


module.exports = ModuleService;