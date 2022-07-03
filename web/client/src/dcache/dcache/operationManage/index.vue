<template>
  <div class="operation-manage">
    <let-tabs @click="onTabClick" :activekey="getTabKey()">
      <let-tab-pane :tab="$t('dcache.operationManage.expand')" tabkey="/operationManage/expand"></let-tab-pane>
      <let-tab-pane :tab="$t('dcache.operationManage.shrinkage')" tabkey="/operationManage/shrinkage"></let-tab-pane>
      <let-tab-pane :tab="$t('dcache.operationManage.migration')" tabkey="/operationManage/migration"></let-tab-pane>
      <let-tab-pane :tab="$t('dcache.operationManage.mainBackup')" tabkey="/operationManage/mainBackup"></let-tab-pane>
      <let-tab-pane :tab="$t('dcache.operationManage.router')" tabkey="/operationManage/router"></let-tab-pane>
    </let-tabs>
    <router-view class="operation-manage-children"></router-view>
  </div>
</template>
<script>
export default{
  methods: {
    onTabClick (tabkey) {
      if(this.$route.path != tabkey){
      this.$router.replace(tabkey)
      }
    },
    getTabKey() {
      const { path } = this.$route
      const pathIndex = path.indexOf('/', path.indexOf('/', path.indexOf('/') + 1) + 1)
      let result = path
      if(pathIndex > 0){
        result = path.substr(0, pathIndex)
      }
      return result
    }
  }
}
</script>
<style>
.operation-manage {
  display: flex;
  flex: 1;
  flex-flow: column;
  overflow: hidden;
  padding-top: 30px;
  padding-bottom: 20px;
  width: 100%;

  .let-tabs__content{
    padding-top: 20px;
  }

  &-children {
    display: flex;
    flex: 1;
    flex-flow: column;
    overflow: auto;
    padding:0 20px 20px 0;
    position: relative;
  }
  &-children::-webkit-scrollbar{border-radius:10px;}
}
</style>

