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
const assert = require('assert');
const {
  DCacheOptPrx
} = require('../../common/rpc');
const {
  DCacheOptStruct
} = require('../../common/rpc/struct');
const moduleConfigDao = require('./dao.js');
const moduleDao = require('./../module/dao.js');
const serverConfigDao = require('./../serverConfig/dao.js');
const ModuleConfigService = {};

ModuleConfigService.hasModule = async function ({
  module_name
}) {
  const item = await moduleConfigDao.findOne({
    where: {
      module_name
    }
  });

  if (item && item.status == 2) {
    return true;
  }
  return false;
}


ModuleConfigService.overwriteModuleConfig = async function (option) {
  const {
    module_name
  } = option;
  // const item = await moduleConfigDao.findOne({ where: { module_name } });
  // if (item && item.status === 2) {
  //   // 如果是安装成功的， 提醒模块存在
  //   throw new Error('#module.hasExist#');
  // } else if (item) {
  // 如果模块已存在但没安装成功，删除原有模块记录
  await moduleConfigDao.destroy({
    where: {
      module_name
    },
    force: true
  });
  // await moduleDao.destroy({ where: { id: item.module_id }, force: true });
  await serverConfigDao.destroy({
    where: {
      module_name
    },
    force: true
  });
  // }
  return moduleConfigDao.add(option);
};

ModuleConfigService.addModuleConfig = async function (option) {
  const {
    module_name
  } = option;
  const item = await moduleConfigDao.findOne({
    where: {
      module_name
    }
  });
  if (item && item.status === 2) {
    // 如果是安装成功的， 提醒模块存在
    throw new Error('#module.hasExist#');
  } else if (item) {
    // 如果模块已存在但没安装成功，删除原有模块记录
    await moduleConfigDao.destroy({
      where: {
        module_name
      },
      force: true
    });
    // await moduleDao.destroy({ where: { id: item.module_id }, force: true });
    await serverConfigDao.destroy({
      where: {
        module_name
      },
      force: true
    });
  }
  return moduleConfigDao.add(option);
};

ModuleConfigService.getModuleConfigInfo = async function ({
  moduleId,
  queryModuleBase
}) {
  return moduleConfigDao.findOne({
    where: {
      module_id: +moduleId
    },
    queryModuleBase
  });
};
ModuleConfigService.getModuleConfigInfoByModuleName = async function (moduleName) {
  return moduleConfigDao.findOne({
    where: {
      module_name: moduleName
    }
  });
};
ModuleConfigService.getCacheServerConfigInfo = async function (module_name) {
  return await serverConfigDao.findAll({
    where: {
      module_name: module_name
    }
  });
};

ModuleConfigService.getPublishSuccessModuleConfig = async function () {
  const queryAppBase = ['name', 'set_area'];
  const queryServerConf = ['id', 'area', 'apply_id', 'module_name', 'group_name', 'server_name', 'server_ip', 'server_type', 'memory', 'shmKey', 'status', 'is_docker'];
  return moduleConfigDao.findAll({
    where: {
      status: 2
    },
    queryAppBase,
    queryServerConf
  });
};

ModuleConfigService.getModuleConfigByName = async function ({
  moduleName,
  queryAppBase = ['name', 'set_area']
}) {
  return moduleConfigDao.findOne({
    where: {
      module_name: moduleName
    },
    queryAppBase
  });
};

ModuleConfigService.removeModuleConfig = async function ({
  module_name,
  module_id
}) {
  moduleConfigDao.destroy({
    where: {
      module_name
    },
    force: true
  });
  moduleDao.destroy({
    where: {
      id: module_id
    },
    force: true
  });
  serverConfigDao.destroy({
    where: {
      module_name
    },
    force: true
  });
};

ModuleConfigService.findOne = async function ({
  ...where
}) {
  return moduleConfigDao.findOne({
    where: {
      status: 2,
      ...where
    }
  });
};

ModuleConfigService.getReleaseProgress = async function (releaseId) {
  const moduleReleaseProgressReq = new DCacheOptStruct.ReleaseProgressReq();
  moduleReleaseProgressReq.readFromObject({
    releaseId
  });
  const {
    __return,
    progressRsp,
    progressRsp: {
      errMsg,
      percent,
      releaseInfo
    }
  } = await DCacheOptPrx.getReleaseProgress(moduleReleaseProgressReq);
  assert(__return === 0, errMsg);
  const progress = [];
  releaseInfo.forEach((item) => {
    progress.push({
      serverName: item.serverName,
      nodeName: item.nodeName,
      releaseId: progressRsp.releaseId,
      percent: item.percent,
    });
  });
  return {
    progress,
    percent
  };
};

module.exports = ModuleConfigService;