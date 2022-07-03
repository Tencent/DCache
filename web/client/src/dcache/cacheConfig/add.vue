<template>
  <section>
    <let-form
      ref="addConfigForm"
      type="medium"
      :title="$t('cache.config.addConfig')"
      :columns="2"
    >
      <let-form-item :label="$t('cache.config.item')" required>
        <let-input size="small" v-model="config.item" required></let-input>
      </let-form-item>
      <let-form-item :label="$t('cache.config.path')" required>
        <let-input size="small" v-model="config.path" required></let-input>
      </let-form-item>
      <let-form-item :label="$t('cache.config.reload')" required>
        <let-input size="small" v-model="config.reload" required></let-input>
      </let-form-item>
      <let-form-item :label="$t('cache.config.period')" required>
        <let-input size="small" v-model="config.period" required></let-input>
      </let-form-item>
      <let-form-item :label="$t('cache.config.remark')" required>
        <let-input size="small" v-model="config.remark" required></let-input>
      </let-form-item>
      <br>
      <let-form-item label=" ">
        <let-button theme="primary" @click="submit">{{$t('cache.add')}}</let-button>
      </let-form-item>
    </let-form>
  </section>
</template>

<script>
  export default {
    data () {
      return {
        config: {
          "item": "",
          "path": "",
          "period": "",
          "reload": "",
          "remark": ""
        }
      }
    },
    methods: {
      async submit () {
        if (this.$refs.addConfigForm.validate()) {
          try {
            await this.$ajax.postJSON('/server/api/cache/addConfig', this.config);
            this.$tip.success(`${this.$t('cache.config.addSuccess')}`);
            Object.assign(this.config, {"item": ""});
            this.$emit('call-back');
          } catch (err) {
            console.error(err)
            this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
          }
        }
      }
    }
  }
</script>

<style>

</style>
