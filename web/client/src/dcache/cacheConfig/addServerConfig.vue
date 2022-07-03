<template>
  <section>
    <let-form
      ref="addConfigForm"
      type="medium"
      :title="$t('cache.config.addConfig')"
    >
      <let-form-item :label="$t('cache.config.item')" required>
        <let-select v-model="itemId" size="small">
          <let-option v-for="item in list" :value="item.id" :key="item.id"
            >{{ item.path }}__{{ item.item }}({{ item.remark }})</let-option
          >
        </let-select>
      </let-form-item>
      <let-form-item :label="$t('cache.config.itemValue')" required>
        <let-input size="small" v-model="configValue" required></let-input>
      </let-form-item>
      <br />
      <let-form-item label=" ">
        <let-button theme="primary" @click="submit">{{
          $t("cache.add")
        }}</let-button>
      </let-form-item>
    </let-form>
  </section>
</template>

<script>
import { getConfig, getModuleConfig } from "@/plugins/interface.js";
export default {
  props: {
    serverName: { type: String, required: false },
    nodeName: { type: String, required: false },
    appName: { type: String, required: false },
    moduleName: { type: String, required: false },
  },
  computed: {
    isModule() {
      return !!(this.appName && this.moduleName);
    },
  },
  data() {
    return {
      itemId: "",
      list: [],
      configValue: "",
    };
  },
  methods: {
    async submit() {
      if (this.$refs.addConfigForm.validate()) {
        try {
          let option = {
            itemId: this.itemId,
            configValue: this.configValue,
            serverName: this.serverName,
            nodeName: this.nodeName,
            appName: this.appName,
            moduleName: this.moduleName,
          };
          await this.$ajax.postJSON(
            "/server/api/cache/addServerConfigItem",
            option
          );
          this.$tip.success(`${this.$t("cache.config.addSuccess")}`);
          this.configValue = "";
          this.itemId = "";

          this.getConfig();
          // 添加模块配置的时候，回调获取新的模块配置列表
          if (this.moduleName) this.$emit("call-back");
        } catch (err) {
          console.error(err);
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        }
      }
    },
    async getConfig() {
      try {
        let configItemList = await getConfig();
        // 配置中心”，“添加模块配置“，模块配置不能重复。 单个服务添加配置， 可以添加重复值， 不过在查看的时候，只能看最后一个。
        const { isModule, appName, moduleName } = this;
        if (isModule) {
          const moduleConfig = await getModuleConfig({ appName, moduleName });
          // 选出module 没有的配置，让用户添加
          configItemList = configItemList.filter((configItem) => {
            const len = moduleConfig.filter(
              (item) =>
                `${item.path}__${item.item}` ===
                `${configItem.path}__${configItem.item}`
            ).length;
            return !len;
          });
        }
        this.list = configItemList;
      } catch (err) {
        console.error(err);
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      }
    },
  },
  created() {
    this.getConfig();
    window.config = this;
  },
};
</script>

<style></style>
