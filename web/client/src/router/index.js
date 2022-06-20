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

import Vue from 'vue';
import Router from 'vue-router';

Vue.use(Router);

// 重写路由的push方法
const routerPush = Router.prototype.push;
Router.prototype.push = function push(location) {
  return routerPush.call(this, location).catch(error => error)
}
const routerReplace = Router.prototype.replace;
Router.prototype.replace = function replace(location) {
  return routerReplace.call(this, location).catch(error => error)
}

// 服务管理
import Server from '@/dcache/server/dcacheIndex';

// import ServerManage from '@/dcache/server/manage';
import dcacheServerManage from '@/dcache/server/dcacheManage.vue';
import dcacheModuleManage from '@/dcache/dcache/moduleManage/index.vue';
import PropertyMonitor from '@/dcache/dcache/propertyMonitor/index.vue'

// import ServerPublish from '@/dcache/server/publish';
import ServerPublish from '@/dcache/server/dcachePublish';
import ServerConfig from '@/dcache/server/config';

import ServerServerMonitor from '@/common/monitor-server';
import ServerPropertyMonitor from '@/common/monitor-property';
// import userManage from '@/common/user-manage';
// import InterfaceDebuger from '@/common/interface-debuger';

// dcache 运维管理
import Operation from '@/dcache/dcacheOperation/index';
import Apply from '@/dcache/dcacheOperation/apply/index';
import CreateApply from '@/dcache/dcacheOperation/apply/createApply';
import CreateService from '@/dcache/dcacheOperation/apply/createService.vue';
import installAndPublish from '@/dcache/dcacheOperation/apply/installAndPublish.vue';
import Module from '@/dcache/dcacheOperation/module/index.vue';
import CreateModule from '@/dcache/dcacheOperation/module/CreateModule.vue';
import ModuleConfig from '@/dcache/dcacheOperation/module/ModuleConfig.vue';
import ModuleServerConfig from '@/dcache/dcacheOperation/module/ServerConfig.vue';
import ModuleInstallAndPublish from '@/dcache/dcacheOperation/module/InstallAndPublish.vue';
import Region from '@/dcache/dcacheOperation/region';

// 发布包管理
import releasePackage from '@/dcache/releasePackage/index';
import proxyList from '@/dcache/releasePackage/proxyList';
import accessList from '@/dcache/releasePackage/accessList';
import routerList from '@/dcache/releasePackage/routerList';
import cacheList from '@/dcache/releasePackage/cacheList';

// cache 配置中心
import CacheConfig from '@/dcache/cacheConfig/config'
import ModuleCache from '@/dcache/cacheConfig/moduleCache'

// 操作管理
import OperationManage from '@/dcache/dcache/operationManage/index.vue'
import OperationManageTypeList from '@/dcache/dcache/operationManage/typeList.vue'
import MainBackup from '@/dcache/dcache/operationManage/mainBackup.vue'
import OperationManageRouter from '@/dcache/dcache/routerManage/index'
import OperationManageRouterModule from '@/dcache/dcache/routerManage/module'
import OperationManageRouterRecord from '@/dcache/dcache/routerManage/record'
import OperationManageRouterGroup from '@/dcache/dcache/routerManage/group'
import OperationManageRouterServer from '@/dcache/dcache/routerManage/server'
import OperationManageRouterTransfer from '@/dcache/dcache/routerManage/transfer'



export default new Router({
  routes: [{
      path: '/server',
      name: 'Server',
      component: Server,
      children: [{
          path: ':treeid/manage',
          component: dcacheServerManage,
        },
        {
          path: ':treeid/manage/:serverType',
          component: dcacheServerManage,
        },
        {
          path: ':treeid/publish/:serverType',
          component: ServerPublish,
        },
        {
          path: ':treeid/config/:serverType',
          component: ServerConfig,
        },
        {
          path: ':treeid/server-monitor/:serverType',
          component: ServerServerMonitor,
        },
        {
          path: ':treeid/property-monitor/:serverType',
          component: ServerPropertyMonitor,
        },
        // {
        //   path: ':treeid/interface-debuger/:serverType',
        //   component: InterfaceDebuger,
        // },
        // {
        //   path: ':treeid/user-manage/:serverType',
        //   component: userManage,
        // },
        {
          path: ':treeid/cache',
          component: dcacheModuleManage,
        },
        {
          path: ':treeid/moduleCache',
          component: ModuleCache,
        },
        {
          path: ':treeid/propertyMonitor',
          component: PropertyMonitor,
          fn: '特性监控',
        },
      ],
    },
    {
      path: '/operation',
      name: 'Operation',
      component: Operation,
      redirect: '/operation/apply',
      children: [{
          path: 'apply',
          name: 'apply',
          component: Apply,
          redirect: '/operation/apply/createApply',
          children: [{
              path: 'createApply',
              component: CreateApply,
            },
            {
              path: 'createService/:applyId',
              component: CreateService,
            },
            {
              path: 'installAndPublish/:applyId',
              component: installAndPublish,
            },
          ]
        },
        {
          path: 'module',
          component: Module,
          redirect: '/operation/module/createModule',
          children: [{
              path: 'createModule',
              component: CreateModule,
            },
            {
              path: 'moduleConfig/:moduleId',
              component: ModuleConfig,
            },
            {
              path: 'serverConfig/:moduleId',
              component: ModuleServerConfig,
            },
            {
              path: 'installAndPublish/:moduleId',
              component: ModuleInstallAndPublish,
            },
          ]
        },
        {
          path: 'region',
          name: 'region',
          component: Region,
        },
      ],
    },
    {
      path: '/releasePackage',
      name: 'releasePackage',
      component: releasePackage,
      redirect: '/releasePackage/proxyList',
      children: [{
          path: 'proxyList',
          component: proxyList,
        },
        {
          path: 'accessList',
          component: accessList,
        },
        {
          path: 'routerList',
          component: routerList,
        },
        {
          path: 'cacheList',
          component: cacheList,
        },
      ],
    },
    {
      path: '/config',
      component: CacheConfig
    },
    {
      path: '/operationManage',
      name: 'operationManage',
      component: OperationManage,
      redirect: '/operationManage/expand',
      children: [{
          path: 'mainBackup',
          component: MainBackup,
        },
        {
          path: 'router',
          component: OperationManageRouter,
          children: [{
              path: ':treeid/module',
              component: OperationManageRouterModule,
            },
            {
              path: ':treeid/record',
              component: OperationManageRouterRecord,
            },
            {
              path: ':treeid/group',
              component: OperationManageRouterGroup,
            },
            {
              path: ':treeid/server',
              component: OperationManageRouterServer,
            },
            {
              path: ':treeid/transfer',
              component: OperationManageRouterTransfer,
            },
          ]
        },
        {
          path: ':type',
          component: OperationManageTypeList,
        },
      ],
    },
    {
      path: '*',
      redirect: '/server',
    },
  ],
  scrollBehavior(to, from, savedPosition) {
    return {
      x: 0,
      y: 0
    }
  }
});