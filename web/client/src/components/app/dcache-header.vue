<template>
  <div class="app_index__header">
    <div class="main-width" style="float:left">
      <let-tabs
        class="tabs"
        :center="true"
        @click="clickTab"
        :activekey="$route.matched[0] ? $route.matched[0].path : '/server'"
      >
        <let-tab-pane
          :tab="$t('header.tab.tab1')"
          tabkey="/server"
        ></let-tab-pane>
        <let-tab-pane
          :tab="$t('header.tab.tab10')"
          tabkey="/operation"
        ></let-tab-pane>
        <let-tab-pane
          :tab="$t('header.tab.tab3')"
          tabkey="/releasePackage"
        ></let-tab-pane>
        <let-tab-pane
          :tab="$t('header.tab.tab4')"
          tabkey="/config"
        ></let-tab-pane>
        <let-tab-pane
          :tab="$t('header.tab.tab5')"
          tabkey="/operationManage"
        ></let-tab-pane>
      </let-tabs>
    </div>
  </div>
</template>

<script>
export default {
  data() {
    return {
      // 图标
    };
  },
  methods: {
    clickTab(tabkey) {
      this.$router.replace(tabkey);
    },
  },
  mounted() {},
};
</script>

<style lang="postcss">
@import "../../assets/css/variable.css";

.app_index__header {
  border-bottom: 1px solid var(--border-color);
  background-color: lightblue;

  .main-width {
    position: relative;
  }

  .tabs .let-tabs__header {
    border-bottom: none;
  }

  .fade-enter-active,
  .fade-leave-active {
    transition: opacity 0.4s;
  }

  .fade-enter,
  .fade-leave-to {
    opacity: 0;
  }
}
</style>
