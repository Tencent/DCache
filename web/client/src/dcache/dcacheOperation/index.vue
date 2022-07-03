<template>
  <div class="page_operation">
    <let-tabs @click="onTabClick" :activekey="$route.matched[1]? $route.matched[1].path : '/operation/apply'">
      <let-tab-pane :tab="$t('apply.createTitle')" tabkey="/operation/apply"></let-tab-pane>
      <let-tab-pane :tab="$t('module.createTitle')" tabkey="/operation/module"></let-tab-pane>
      <let-tab-pane :tab="$t('region.title')" tabkey="/operation/region"></let-tab-pane>
    </let-tabs>
    <router-view class="page_operation_children"></router-view>
  </div>
</template>

<script>

export default {
  name: 'Oparetion',
  methods: {
    onTabClick(tabkey) {
      this.$router.replace(tabkey);
    },
  },
};
</script>

<style>
.page_operation {
  display: flex;
  flex: 1;
  flex-flow: column;
  overflow: hidden;
  padding-top: 30px;
  padding-bottom: 20px;
  width: 100%;

  &_children {
    display: flex;
    flex: 1;
    flex-flow: column;
    margin-top: 20px;
    overflow: auto;
    padding:0 40px 20px 0;
    position: relative;
  }
  &_children::-webkit-scrollbar{border-radius:10px;}
}

</style>
