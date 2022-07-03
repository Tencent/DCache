<template>
  <div class="page_router">
    <div class="left-view">
      <div class="tree_wrap">
        <let-tree
          class="left-tree"
          v-if="treeData && treeData.length"
          :data="treeData"
          :activeKey="getTreeKey()"
          @on-select="selectTree"
        />
      </div>
    </div>
    <div class="right-view">
      <div class="empty" style="width: 300px" v-if="!$route.params.treeid">
        <img class="package" src="@/assets/img/package.png" />
        <p class="notice" v-html="$t('index.rightView.tips')"></p>
      </div>
      <router-view class="router-children" v-else></router-view>
    </div>
  </div>
</template>

<script>
import { getRouterTree } from "@/plugins/interface.js";

export default {
  name: "router",

  data() {
    return {
      menu: [
        { title: this.$t("routerManage.routerModule"), nodeKey: "module" },
        { title: this.$t("routerManage.routerRecord"), nodeKey: "record" },
        { title: this.$t("routerManage.routerGroup"), nodeKey: "group" },
        { title: this.$t("routerManage.routerServer"), nodeKey: "server" },
        { title: this.$t("routerManage.routerTransfer"), nodeKey: "transfer" },
      ],
      treeData: [],
    };
  },
  mounted() {
    this.loadData();
  },
  methods: {
    loadData() {
      this.getTree();
    },
    selectTree(nodeKey, nodeInfo) {
      let isTrue = false;
      let app = nodeKey.split(".")[0] || "";
      let appPath = nodeKey.split(".")[1] || "";
      this.menu.forEach((item) => {
        if (item.nodeKey === appPath) {
          isTrue = true;
        }
      });
      if (isTrue) {
        const { treeid } = this.$route.params;
        const { path } = this.$route;
        const currentPath = path.substr(path.lastIndexOf("/") + 1, path.length);
        if (treeid && treeid === app) {
          if (currentPath !== appPath) {
            this.$router.push(appPath);
          }
        } else {
          if (treeid) {
            this.$router.push(
              {
                params: {
                  treeid: app,
                },
              },
              () => {
                if (currentPath !== appPath) {
                  this.$router.push(appPath);
                }
              }
            );
          } else {
            this.$router.replace(`router/${app}/${appPath}`);
          }
        }
      }
    },
    getTreeKey() {
      const { treeid } = this.$route.params;
      const { path } = this.$route;
      const currentPath = path.substr(path.lastIndexOf("/") + 1, path.length);
      return `${treeid}.${currentPath}`;
    },
    async getTree() {
      const result = await getRouterTree();
      const arr = [];
      if (result && result.length > 0) {
        result.forEach((item) => {
          const childArr = [];
          this.menu.forEach((jitem) => {
            childArr.push({
              label: jitem.title,
              app: item.name,
              path: jitem.nodeKey,
              nodeKey: `${item.name}.${jitem.nodeKey}`,
            });
          });

          arr.push({
            label: item.name,
            nodeKey: item.name,
            children: childArr,
          });
        });
      }
      this.treeData = arr;
    },
  },
};
</script>

<style>
.page_router {
  display: flex;
  flex: 1;
  flex-flow: row;
  width: 100%;
  overflow: hidden;

  &-children {
    display: flex;
    flex: 1;
    flex-flow: column;
    overflow: auto;
    padding: 0 20px 20px 0;
    position: relative;
  }
  &-children::-webkit-scrollbar {
    border-radius: 10px;
  }

  .left-view {
    display: flex;
    flex: 1;
    flex-flow: column;
    max-width: 260px;
  }

  .tree_wrap {
    display: flex;
    flex: 1;
    overflow: auto;
    position: relative;
  }
  .tree_wrap::-webkit-scrollbar {
    border-radius: 10px;
  }

  /*目录树*/
  .left-tree {
    flex: 0 0 auto;
    line-height: 1.8;
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
      position: relative;
      top: -1px;
    }

    ul.let-tree__node {
      font-size: 14px;
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
      margin-left: 0;

      & > li > ul.let-tree__node {
        /* margin-left: 0; */

        li .pointer:first-of-type {
          /* margin-left: 20px; */
        }

        li .pointer:first-of-type:empty {
          margin-left: 18px;
        }
      }
    }

    .tree-icon {
      width: 16px;
      height: 16px;
      background-repeat: no-repeat;
      background-position: center;
      background-size: 100%;
      background-image: url("../../../assets/img/tree-icon-2.png");
      margin-right: 3px;
      margin-left: 0;
      vertical-align: middle;

      &.down {
        transform: rotate(0);
      }
      &:before {
        content: "";
      }
    }
  }
  /*目录树 end*/

  /*右侧窗口*/
  .right-view {
    display: flex;
    flex: 1;
    flex-flow: column;
    margin-left: 20px;
    overflow: auto;

    .empty {
      margin: 88px 0 0 calc((100% - 240px) / 2 - 108px);
      width: 240px;
      text-align: center;
      .package {
        width: 180px;
        height: 114px;
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
}
</style>
