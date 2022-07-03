<template>
  <section class="">
    <let-form
      ref="editConfigForm"
      type="medium"
      :title="$t('cache.config.editConfig')"
      :columns="2"
    >
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
        <let-button theme="primary" @click="submit">{{$t('cache.modification')}}</let-button>
      </let-form-item>
    </let-form>
  </section>
</template>

<script>
export default {
    props: {
      id: {type: String, required: true},
      item: {type: String, required: true},
      path: {type: String, required: true},
      period: {type: String, required: true},
      reload: {type: String, required: true},
      remark: {type: String, required: true},
    },
  data () {
    return {
      config: {
        id: this.id,
        item: this.item,
        path: this.path,
        period: this.period,
        reload: this.reload,
        remark: this.remark,
      }
    }
  },
  methods: {
    async submit () {
      if (this.$refs.editConfigForm.validate()) {
        try {
          await this.$ajax.postJSON('/server/api/cache/editConfig', this.config);
          this.$tip.success(`${this.$t('cache.config.addSuccess')}`);
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
