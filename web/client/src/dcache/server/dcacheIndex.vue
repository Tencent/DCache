<template>
  <div class="page_server">
    <div class="left-view">
      <div class="tree_wrap">
        <i
          class="el-icon-refresh-right"
          :class="{ active: isIconPlay }"
          @click="treeSearch(1)"
        ></i>
        <let-tree
          class="left-tree"
          v-if="treeData && treeData.length"
          :data="treeData"
          :activeKey="treeid"
          @on-select="selectTree"
        />
        <div class="left-tree" v-if="treeData && !treeData.length">
          <p class="loading">{{ $t("common.noService") }}</p>
        </div>
        <div class="left-tree" v-if="!treeData" ref="treeLoading">
          <div class="loading" v-if="treeData === false">
            <p>{{ treeErrMsg }}</p>
            <a href="javascript:;" @click="getTreeData()">{{
              $t("common.reTry")
            }}</a>
          </div>
        </div>
      </div>
    </div>

    <!-- <div class="right-view" v-if="!this.$route.params.treeid"> -->
    <div class="right-view" v-if="!treeid">
      <div class="empty" style="width: 300px;">
        <img class="package" src="@/assets/img/package.png" />
        <p class="title">{{ $t("index.rightView.title") }}</p>
        <p class="notice" v-html="$t('index.rightView.tips')"></p>
        <p class="notice">https://github.com/Tencent/DCache</p>
      </div>
    </div>

    <div class="right-view" v-else>
      <div class="btabs_wrap">
        <ul ref="btabs" class="btabs" v-vscroll>
          <li
            class="btabs_item"
            :class="{
              active: item.id === treeid,
            }"
            v-for="item in BTabs"
            :key="item.id"
          >
            <a
              class="btabs_link"
              href="javascript:;"
              @click="clickBTabs($event, item.id)"
              >{{ getNewServerName(item.id) }}</a
            >
            <a
              class="btabs_close"
              href="javascript:;"
              @click="closeBTabs(item.id)"
              >Close</a
            >
          </li>
        </ul>
        <a
          class="btabs_all"
          href="javascript:;"
          title="CloseAll"
          @click="closeAllBTabs"
          >CloseAll</a
        >
      </div>

      <div class="btabs_con">
        <div
          class="btabs_con_item"
          v-for="item in BTabs"
          :key="item.id"
          v-show="item.id === treeid"
        >
          <let-tabs @click="clickTab" :activekey="item.path">
            <template v-if="serverData.level === 6">
              <let-tab-pane
                :tabkey="`${base}/cache`"
                :tab="$t('index.rightView.tab.manage')"
              ></let-tab-pane>
              <let-tab-pane
                :tabkey="base + '/moduleCache'"
                :tab="$t('cache.config.configuration')"
              ></let-tab-pane>
              <let-tab-pane
                :tabkey="base + '/propertyMonitor'"
                :tab="$t('index.rightView.tab.propertyMonitor')"
              ></let-tab-pane>
            </template>
            <template v-else-if="serverData.level === 5">
              <let-tab-pane
                :tabkey="`${base}/manage`"
                :tab="$t('index.rightView.tab.manage')"
              ></let-tab-pane>
              <let-tab-pane
                :tabkey="`${base}/publish`"
                :tab="$t('index.rightView.tab.patch')"
              ></let-tab-pane>
              <let-tab-pane
                :tabkey="`${base}/config`"
                :tab="$t('index.rightView.tab.serviceConfig')"
              ></let-tab-pane>
              <let-tab-pane
                :tabkey="`${base}/server-monitor`"
                :tab="$t('index.rightView.tab.statMonitor')"
              ></let-tab-pane>
              <!-- <let-tab-pane
                :tabkey="`${base}/user-manage`"
                :tab="$t('index.rightView.tab.privileage')"
                v-if="enableAuth"
              ></let-tab-pane> -->
              <let-tab-pane
                :tabkey="`${base}/property-monitor`"
                :tab="$t('index.rightView.tab.propertyMonitor')"
                v-if="serverType != 'dcache'"
              ></let-tab-pane>
            </template>
            <template v-else-if="serverData.level === 1">
              <let-tab-pane
                :tabkey="`${base}/manage`"
                :tab="$t('index.rightView.tab.manage')"
              ></let-tab-pane>
              <let-tab-pane
                :tabkey="`${base}/config`"
                :tab="$t('index.rightView.tab.appConfig')"
              ></let-tab-pane>
            </template>
          </let-tabs>

          <router-view
            :is="getName(item.path)"
            :treeid="item.id"
            :servertype="serverType"
            ref="childView"
            class="page_server_child"
          ></router-view>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import manage from "./dcacheManage";
import publish from "./dcachePublish";
import config from "./config";

import serverMonitor from "@/common/monitor-server";
import propertyMonitor from "@/common/monitor-property";
// import userManage from "@/common/user-manage";

import cache from "@/dcache/dcache/moduleManage/index.vue";
import moduleCache from "@/dcache/cacheConfig/moduleCache";
import DPropertyMonitor from "@/dcache/dcache/propertyMonitor/index.vue";

export default {
  name: "Server",
  components: {
    manage,
    publish,
    config,
    "server-monitor": serverMonitor,
    "property-monitor": propertyMonitor,
    // "user-manage": userManage,
    cache,
    moduleCache,
    propertyMonitor: DPropertyMonitor,
  },
  data() {
    return {
      treeErrMsg: "load failed",
      treeData: null,
      treeid: "",
      // enableAuth: false,
      isIconPlay: false,
      // 当前页面信息
      serverData: {
        level: 5,
        application: "",
        server_name: "",
        set_name: "",
        set_area: "",
        set_group: "",
        module_name: "",
      },
      serverType: "",

      BTabs: [],
    };
  },
  computed: {
    base() {
      return `/server/${this.treeid}`;
    },
  },
  watch: {
    treeid() {
      this.serverData = this.getServerData();
      // this.isTrueTreeLevel();
    },
    $route(to, from) {
      if (to.path === "/server") {
        this.getTreeData();
      }
    },
  },
  directives: {
    vscroll: {
      componentUpdated(el) {
        let boxEl = el || "";
        let itemEl = el.children || [];
        let currEl = "";

        itemEl.forEach((item) => {
          const iclass = item.getAttribute("class");
          if (iclass.indexOf("active") > -1) {
            currEl = item;
          }
        });

        if (currEl.offsetLeft < boxEl.scrollLeft) {
          const x = currEl.offsetLeft;
          boxEl.scrollTo(x, 0);
        } else if (
          currEl.offsetLeft + currEl.offsetWidth >
          boxEl.scrollLeft + boxEl.offsetWidth
        ) {
          const x = currEl.offsetLeft + currEl.offsetWidth - boxEl.offsetWidth;
          boxEl.scrollTo(x, 0);
        }
      },
    },
  },
  methods: {
    iconLoading() {
      const that = this;
      if (!that.isIconPlay) {
        that.isIconPlay = true;
        setTimeout(function() {
          that.isIconPlay = false;
        }, 1000);
      }
    },
    getNewServerName(id) {
      const v = id && id.split(".");
      if (!v) {
        return id;
      }
      if (v.length == 1) {
        const app = id && id.split(".")[0].substring(1);
        return `${app}`;
      }
      if (v.length > 1) {
        const app = id && id.split(".")[0].substring(1);
        const server = id && id.split(".")[1].substring(1);
        return `${app}.${server}`;
      }
    },
    getName(val) {
      let result = "";
      if (val.lastIndexOf("/") > -1) {
        result = val.substring(val.lastIndexOf("/") + 1, val.length);
      }
      return result;
    },
    treeSearch(type) {
      // this.iconLoading();
      this.getTreeData(type);
    },
    selectTree(nodeKey, nodeInfo) {
      if (nodeInfo.children && nodeInfo.children.length) {
      } else {
        // 正常的服务展示, tars 的就是正常的，只有dcache、router、proxy才有serverType
        let { serverType } = nodeInfo;
        !serverType ? (serverType = "tars") : "";
        this.serverType = serverType;
      }

      this.selectBTabs(nodeKey, nodeInfo);
      this.checkCurrBTabs();
    },
    // 处理接口返回数据
    handleData(res, isFirstLayer) {
      if (!res || !res.length) return;
      res.forEach((node) => {
        node.label = node.name; //eslint-disable-line
        node.nodeKey = node.id; //eslint-disable-line

        // 如果是当前菜单，则展开
        if (this.treeid === node.nodeKey) {
          node.expand = true; //eslint-disable-line
        }

        if (node.children && node.children.length) {
          this.handleData(node.children);
        }
      });
    },
    getTreeData(type) {
      this.treeData = null;
      type = type || 1;

      this.$nextTick(() => {
        const loading = this.$loading.show();

        this.$ajax
          .getJSON("/server/api/dtree", { type })
          .then((res) => {
            loading.hide();
            this.treeData = res;
            this.handleData(this.treeData, true);
          })
          .catch((err) => {
            loading.hide();
            // console.error(err)
            this.treeErrMsg = err.err_msg || err.message || "加载失败";
            this.treeData = false;
          });
      });
    },
    getServerData() {
      if (!this.treeid) {
        return {};
      }
      const treeArr = this.treeid.split(".");
      const serverData = {
        level: 5,
        application: "",
        server_name: "",
        set_name: "",
        set_area: "",
        set_group: "",
      };

      treeArr.forEach((item) => {
        // console.log('item', item);
        const level = +item.substr(0, 1);
        const name = item.substr(1);
        switch (level) {
          case 1:
            serverData.application = name;
            break;
          case 2:
            serverData.set_name = name;
            break;
          case 3:
            serverData.set_area = name;
            break;
          case 4:
            serverData.set_group = name;
            break;
          case 5:
            serverData.server_name = name;
            break;
          case 6:
            serverData.module_name = name;
            break;
          default:
            break;
        }
        serverData.level = level;
      });

      return serverData;
    },

    checkTreeid() {
      this.treeid = this.getLocalStorage("tars_dcache_treeid") || "";
    },

    clickTab(tabkey) {
      let { treeid, BTabs } = this;
      BTabs &&
        BTabs.forEach((item) => {
          if (item.id === treeid) {
            item.path = tabkey;
          }
        });

      this.setLocalStorage("tars_dcache_tabs", JSON.stringify(BTabs));
    },

    // 有些目录层级不显示某些标签，处理之
    isTrueTreeLevel() {
      const routeArr = this.$route.path.split("/");
      const route = routeArr[routeArr.length - 1];

      // 默认不处理
      let shouldRedirect = false;
      // publish、server-monitor、property-monitor 只有 level 5 可访问
      if (
        this.serverData.level !== 5 &&
        (route === "publish" ||
          route === "server-monitor" ||
          route === "property-monitor" ||
          route === "user-manage")
      ) {
        shouldRedirect = true;
      }
      // config 有 level 5、4、1 可访问
      if (
        this.serverData.level !== 5 &&
        this.serverData.level !== 4 &&
        this.serverData.level !== 1 &&
        route === "config"
      ) {
        shouldRedirect = true;
      }
      // 命中不可访问进行跳转
      if (shouldRedirect) {
        this.$router.replace("manage");
      }
    },

    checkBTabs() {
      let { BTabs } = this;
      const tabs = this.getLocalStorage("tars_dcache_tabs");
      if (tabs && tabs.length > 0) {
        tabs.forEach((item) => {
          BTabs.push({
            id: item.id,
            path: item.path,
          });
        });
      }
    },

    checkCurrBTabs() {
      this.$nextTick(() => {
        let boxEl = this.$refs.btabs || "";
        let itemEl = boxEl.children || [];
        let currEl = "";

        itemEl.forEach((item) => {
          const iclass = item.getAttribute("class");
          if (iclass.indexOf("active") > -1) {
            currEl = item;
          }
        });

        if (currEl.offsetLeft < boxEl.scrollLeft) {
          const x = currEl.offsetLeft;
          boxEl.scrollTo(x, 0);
        } else if (
          currEl.offsetLeft + currEl.offsetWidth >
          boxEl.scrollLeft + boxEl.offsetWidth
        ) {
          const x = currEl.offsetLeft + currEl.offsetWidth - boxEl.offsetWidth;
          boxEl.scrollTo(x, 0);
        }
      });
    },

    selectBTabs(nodeKey, nodeInfo) {
      let { BTabs } = this;
      let isBTabTrue = false;
      BTabs.forEach((item) => {
        if (item.id === nodeKey) {
          isBTabTrue = true;
          item.path = nodeInfo.moduleName
            ? `/server/${nodeKey}/cache`
            : `/server/${nodeKey}/manage`;
        }
      });
      if (!isBTabTrue) {
        this.BTabs.push({
          id: nodeKey,
          path: nodeInfo.moduleName
            ? `/server/${nodeKey}/cache`
            : `/server/${nodeKey}/manage`,
        });
      }

      this.treeid = nodeKey;
      this.setLocalStorage("tars_dcache_treeid", JSON.stringify(nodeKey));
      this.setLocalStorage("tars_dcache_tabs", JSON.stringify(BTabs));
    },

    clickBTabs(e, nodeKey) {
      this.treeid = nodeKey;
      this.setLocalStorage("tars_dcache_treeid", JSON.stringify(nodeKey));
    },

    closeBTabs(nodeKey) {
      let { BTabs } = this;
      let BIndex = 0;

      BTabs.forEach((item, index) => {
        if (item.id === nodeKey) {
          BIndex = index;
        }
      });
      BTabs.splice(BIndex, 1);

      this.setLocalStorage("tars_dcache_tabs", JSON.stringify(BTabs));

      if (BTabs.length > 0) {
        this.treeid = BTabs[BTabs.length - 1].id;
      } else {
        this.treeid = "";
      }
      this.setLocalStorage("tars_dcache_treeid", JSON.stringify(this.treeid));

      this.getTreeData();
    },

    closeAllBTabs() {
      this.BTabs = [];
      this.treeid = "";
      this.setLocalStorage("tars_dcache_tabs", JSON.stringify(this.BTabs));
      this.setLocalStorage("tars_dcache_treeid", JSON.stringify(this.treeid));
      this.getTreeData();
    },

    getLocalStorage(key) {
      let result = "";
      if (window.localStorage) {
        result = JSON.parse(JSON.parse(localStorage.getItem(key)));
      }
      return result;
    },

    setLocalStorage(key, val) {
      let result = "";
      if (window.localStorage) {
        result = localStorage.setItem(key, JSON.stringify(val));
      }
      return result;
    },
  },
  created() {
    this.serverData = this.getServerData();
    this.isTrueTreeLevel();
  },
  mounted() {
    this.checkTreeid();
    this.checkBTabs();
    this.getTreeData();
    window.dcacheIndex = this;
  },
};
</script>

<style lang="postcss">
@import "../../assets/css/variable.css";

.el-icon-refresh-right {
  cursor: pointer;
}
.el-icon-refresh-right.active {
  animation: icon_loading 1s;
}

@-webkit-keyframes icon_loading {
  0% {
    transform: rotateZ(0deg);
  }
  100% {
    transform: rotateZ(360deg);
  }
}

.page_server {
  padding-bottom: var(--gap-small);
  padding-top: var(--gap-big);
  display: flex;
  flex: 1;
  width: 100%;
  overflow: hidden;

  .left-view {
    display: flex;
    flex: 1;
    flex-flow: column;
    max-width: 260px;
  }

  /**/
  .tree_wrap {
    display: flex;
    flex: 1;
    overflow: auto;
    position: relative;
  }
  .tree_wrap::-webkit-scrollbar {
    border-radius: 10px;
  }
  .tree_icon {
    color: #565b66;
    position: absolute;
    right: 10px;
    top: 10px;
  }
  /*目录树*/
  .left-tree {
    flex: 0 0 auto;
    width: 250px;
    min-height: 380px;

    .loading {
      display: block;
      text-align: center;
      margin: 180px auto 0;
    }

    .let-icon-caret-right-small {
      margin-right: 2px;
      margin-left: 4px;
    }

    ul.let-tree__node {
      font-size: 14px;
      line-height: var(--gap-small);
      margin-left: 18px;

      li {
        text-overflow: ellipsis;
        overflow: hidden;
        word-break: break-all;
        white-space: pre;
      }
    }

    & > ul.let-tree__node {
      font-size: 16px;
      margin-bottom: 10px;
      margin-left: 0;

      & > li > ul.let-tree__node {
        margin-left: 0;

        li .pointer:first-of-type {
          margin-left: 20px;
        }

        li .pointer:first-of-type:empty {
          margin-left: 20px;
        }
      }
    }
  }
  /*目录树 end*/

  /*右侧窗口*/
  .right-view {
    display: flex;
    flex: 1;
    flex-flow: column;
    margin-left: 40px;
    overflow: hidden;
    position: relative;

    .empty {
      margin: 88px 0 0 calc((100% - 240px) / 2 - 108px);
      width: 240px;
      text-align: center;
      .package {
        width: 180px;
        height: 114px;
        margin-bottom: var(--gap-small);
      }
      .title {
        font-size: 18px;
        font-weight: bold;
        margin-bottom: 12px;
      }
      .notice {
        line-height: 22px;
        color: #a2a9b8;
      }
    }
  }

  .page_server_child {
    display: flex;
    flex: 1;
    flex-flow: column;
    margin-top: 20px;
    overflow: auto;
    padding-right: 20px;
  }
  .page_server_child::-webkit-scrollbar {
    border-radius: 10px;
  }

  .loading-placeholder {
    min-height: 80px;
    text-align: center;
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
  }

  .let_modal .let-loading__parent-relative .let-align__inner {
    padding-top: 0;
  }
  .let-form.two-columns.let-form-cols-2 {
    margin-right: -30px;
    .let-form-item {
      padding-right: 30px;
    }
  }
  .let-table__operation {
    padding-left: 0;
    padding-right: 0px;
    margin-right: 10px;

    &:last-of-type {
      margin-right: 0;
    }
  }

  /*服务状态*/
  .status-active,
  .status-off,
  .status-activating,
  .status-flowactive {
    display: flex;
    align-items: center;

    &:before {
      content: "";
      display: inline-block;
      width: 4px;
      height: 4px;
      border-radius: 50%;
      background: currentColor;
      margin-right: 4px;
    }
    &:after {
      display: inline-block;
    }
  }
  .status-active {
    color: var(--active-color);
    &:after {
      content: "Active";
    }
  }
  .status-off {
    color: var(--off-color);
    &:after {
      content: "Off";
    }
  }

  .status-activating {
    color: var(--off-color);
    &:after {
      content: "Activating";
    }
  }

  .status-flowactive {
    &:after {
      content: "Active";
    }
  }
  /*服务状态 end*/

  /*右侧窗口 end*/
  .btabs_wrap {
    display: block;
    height: 32px;
    margin-bottom: 10px;
    position: relative;
  }
  .btabs {
    display: block;
    margin-right: 32px;
    overflow-x: auto;
    overflow-y: hidden;
    position: relative;
    white-space: nowrap;
  }
  .btabs::-webkit-scrollbar {
    border-radius: 10px;
    height: 8px;
  }
  .btabs:before {
    border-bottom: 1px solid #d7dae0;
    bottom: 0;
    content: "";
    left: 0;
    position: absolute;
    right: 0;
  }
  .btabs_item {
    border: 1px solid #d7dae0;
    display: inline-block;
    position: relative;
    z-index: 10;
  }
  .btabs_item + .btabs_item {
    margin-left: -1px;
  }
  .btabs_item:first-child {
    border-top-left-radius: 5px;
  }
  .btabs_item:last-child {
    border-top-right-radius: 5px;
  }
  .btabs_item.active {
    background: #457ff5;
  }
  .btabs_item.active .btabs_link {
    color: #fff;
  }
  .btabs_item:hover .btabs_close,
  .btabs_item.active .btabs_close {
    -webkit-transform: translateY(-50%) scale(1);
  }
  .btabs_item.active .btabs_close:before,
  .btabs_item.active .btabs_close:after {
    border-color: #fff;
  }
  .btabs_link {
    color: #9096a3;
    display: block;
    font-weight: bold;
    height: 30px;
    line-height: 20px;
    padding: 5px 30px 5px 10px;
  }
  .btabs_link:hover {
    color: #222329;
  }
  .btabs_close {
    display: block;
    font-size: 0;
    height: 30px;
    overflow: hidden;
    position: absolute;
    right: 0;
    top: 50%;
    width: 30px;
    z-index: 20;
    -webkit-transform: translateY(-50%) scale(0.5) rotateZ(-180deg);
    -webkit-transition: all 0.5s ease-in-out;
  }
  .btabs_close:before,
  .btabs_close:after {
    border-top: 1px solid #9096a3;
    content: "";
    height: 0;
    left: 50%;
    position: absolute;
    top: 50%;
    width: 13px;
  }
  .btabs_close:before {
    -webkit-transform: translate3d(-50%, -50%, 0) rotateZ(-45deg);
  }
  .btabs_close:after {
    -webkit-transform: translate3d(-50%, -50%, 0) rotateZ(45deg);
  }
  .btabs_all {
    background: #e36654;
    bottom: 0;
    border-radius: 5px;
    display: block;
    font-size: 0;
    overflow: hidden;
    position: absolute;
    right: 0;
    top: 0;
    text-indent: -9999em;
    width: 32px;
    z-index: 30;
  }
  .btabs_all:before,
  .btabs_all:after {
    border-top: 1px solid #fff;
    content: "";
    height: 0;
    left: 50%;
    position: absolute;
    top: 50%;
    width: 16px;
  }
  .btabs_all:before {
    -webkit-transform: translate3d(-50%, -50%, 0) rotateZ(-45deg);
  }
  .btabs_all:after {
    -webkit-transform: translate3d(-50%, -50%, 0) rotateZ(45deg);
  }
  .btabs_all:hover {
    opacity: 0.7;
  }

  .btabs_close:hover:before,
  .btabs_close:hover:after {
    border-color: #222329;
  }
  .btabs_item.active .btabs_link:hover {
    color: #222329;
  }
  .btabs_item.active .btabs_close:hover:before,
  .btabs_item.active .btabs_close:hover:after {
    border-color: #222329;
  }
  .btabs_con {
    display: flex;
    flex: 1;
    flex-flow: column;
    overflow: hidden;
  }
  .btabs_con_item {
    display: flex;
    flex: 1;
    flex-flow: column;
    overflow: hidden;
  }
  .btabs_con_home {
    display: flex;
    overflow: hidden;
  }
}
</style>
