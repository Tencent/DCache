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

const service = require('./service');

module.exports = {
  async moduleCreate(ctx) {
    try {
      const result = await service.moduleCreate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[moduleCreate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async moduleDestroy(ctx) {
    try {
      const result = await service.moduleDestroy(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[moduleDestroy]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async moduleUpdate(ctx) {
    try {
      const result = await service.moduleUpdate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[moduleUpdate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async moduleFindOne(ctx) {
    try {
      const result = await service.moduleFindOne(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[moduleFindOne]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async moduleFindAndCountAll(ctx) {
    try {
      const result = await service.moduleFindAndCountAll(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[moduleFindAndCountAll]:', err);
      ctx.makeResObj(500, err.message);
    }
  },

  async groupCreate(ctx) {
    try {
      const result = await service.groupCreate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[groupCreate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async groupDestroy(ctx) {
    try {
      const result = await service.groupDestroy(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[groupDestroy]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async groupUpdate(ctx) {
    try {
      const result = await service.groupUpdate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[groupUpdate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async groupFindOne(ctx) {
    try {
      const result = await service.groupFindOne(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[groupFindOne]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async groupFindAndCountAll(ctx) {
    try {
      const result = await service.groupFindAndCountAll(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[groupFindAndCountAll]:', err);
      ctx.makeResObj(500, err.message);
    }
  },

  async recordCreate(ctx) {
    try {
      const result = await service.recordCreate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[recordCreate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async recordDestroy(ctx) {
    try {
      const result = await service.recordDestroy(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[recordDestroy]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async recordUpdate(ctx) {
    try {
      const result = await service.recordUpdate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[recordUpdate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async recordFindOne(ctx) {
    try {
      const result = await service.recordFindOne(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[recordFindOne]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async recordFindAndCountAll(ctx) {
    try {
      const result = await service.recordFindAndCountAll(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[recordFindAndCountAll]:', err);
      ctx.makeResObj(500, err.message);
    }
  },

  async serverCreate(ctx) {
    try {
      const result = await service.serverCreate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[serverCreate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async serverDestroy(ctx) {
    try {
      const result = await service.serverDestroy(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[serverDestroy]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async serverUpdate(ctx) {
    try {
      const result = await service.serverUpdate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[serverUpdate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async serverFindOne(ctx) {
    try {
      const result = await service.serverFindOne(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[serverFindOne]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async serverFindAndCountAll(ctx) {
    try {
      const result = await service.serverFindAndCountAll(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[serverFindAndCountAll]:', err);
      ctx.makeResObj(500, err.message);
    }
  },

  async transferCreate(ctx) {
    try {
      const result = await service.transferCreate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[transferCreate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async transferDestroy(ctx) {
    try {
      const result = await service.transferDestroy(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[transferDestroy]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async transferUpdate(ctx) {
    try {
      const result = await service.transferUpdate(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[transferUpdate]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async transferFindOne(ctx) {
    try {
      const result = await service.transferFindOne(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[transferFindOne]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
  async transferFindAndCountAll(ctx) {
    try {
      const result = await service.transferFindAndCountAll(ctx.paramsObj)
      ctx.makeResObj(200, '', result);
    } catch (err) {
      logger.error('[transferFindAndCountAll]:', err);
      ctx.makeResObj(500, err.message);
    }
  },
}