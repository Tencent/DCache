<template>
  <div class="page_operation_apply">
    <let-steps :current="step" class="apply_steps">
      <let-step :title="$t(title1)"></let-step>
      <let-step :title="$t(title2)"></let-step>
      <let-step :title="$t(title3)"></let-step>
      <!--<let-step :title="$t(title4)"></let-step>-->
    </let-steps>

    <router-view class="page_operation_apply_children"></router-view>
  </div>
</template>

<script>

  export default {
    data () {
      let {fullPath} = this.$route;
      let step = 1;
      if (fullPath.indexOf('createApply') > -1) {
        step = 1
      } else if (fullPath.indexOf('createService') > -1) {
        step = 2
      } else if (fullPath.indexOf('installAndPublish') > -1) {
        step = 3
      }
      return {
        title1: 'apply.createApply',
        title2: 'apply.createRouterProxyService',
        title3: 'apply.installAndPublish',
//        title4: 'apply.publish',
        step
      }
    },
    watch: {
      $route(newR, oldR) {
        let {fullPath} = newR;
        if (fullPath.indexOf('createApply') > -1) {
          this.step = 1
        } else if (fullPath.indexOf('createService') > -1) {
          this.step = 2
        } else if (fullPath.indexOf('installAndPublish') > -1) {
          this.step = 3
        }
      },
    },
    methods: {},
  };
</script>

<style>
  .apply_steps {
    margin: 10px 0 20px;
    padding-bottom: 20px;
    border-bottom: 1px solid #d7dae0;
  }
</style>
