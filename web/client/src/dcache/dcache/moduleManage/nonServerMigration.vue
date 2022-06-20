<template>
  <section style="display: inline">
    <let-button theme="primary" :disabled="disabled" @click="init">{{
      $t("dcache.nonServerMigration")
    }}</let-button>
    <let-modal
      v-model="show"
      :footShow="false"
      :closeOnClickBackdrop="true"
      :width="'1000px'"
      :title="$t('dcache.nonServerMigration')"
    >
      <let-form
        ref="detailForm"
        labelPosition="right"
        class="non-server-migration"
        v-if="show"
      >
        <let-form-item
          required
          :label="$t('dcache.operationManage.appName') + '：'"
        >
          {{ appName }}
        </let-form-item>
        <let-form-item
          required
          :label="$t('dcache.operationManage.moduleName') + '：'"
        >
          {{ moduleName }}
        </let-form-item>
        <let-form-item
          required
          :label="$t('dcache.operationManage.srcGroupName') + '：'"
        >
          {{ srcGroupName }}
        </let-form-item>
        <let-form-item
          itemWidth="100%"
          required
          :label="$t('dcache.operationManage.dstGroupName') + '：'"
        >
          <let-select required v-model="groupName" size="small">
            <let-option
              v-for="groupName in dstGroupNames"
              :value="groupName"
              :key="groupName"
              >{{ groupName }}</let-option
            >
          </let-select>
        </let-form-item>
        <let-form-item required :label="$t('dcache.migrationMethod') + '：'">
          <let-radio-group
            v-model="transferData"
            style="width: 300px"
            :data="migrationMethods"
          >
          </let-radio-group>
        </let-form-item>
        <div>
          <let-button
            size="small"
            theme="primary"
            @click="submitServerConfig"
            >{{ $t("common.submit") }}</let-button
          >
        </div>
      </let-form>
    </let-modal>
  </section>
</template>
<script>
import { transferDCacheGroup, hasOperation } from "@/plugins/interface.js";
import Mixin from "./mixin.js";
export default {
  mixins: [Mixin],
  props: {
    disabled: Boolean,
    expandServers: {
      required: false,
      type: Array,
    },
  },
  data() {
    return {
      show: false,
      groupName: "",
      dstGroupNames: [],
    };
  },
  computed: {
    srcGroupName() {
      let checkServer = this.expandServers.find((server) => server.isChecked);
      return checkServer.group_name;
    },
  },
  methods: {
    async init() {
      try {
        this.groupName = "";

        /**
         * 只有一个服务组，不允许非部署迁移
         */
        let allGroupNames = this.expandServers.map(
          (server) => server.group_name
        );
        // 去重
        allGroupNames = Array.from(new Set(allGroupNames));
        if (allGroupNames.length === 1)
          return this.$tip.error(this.$t("dcache.onlyOneServiceGroup"));

        /**
         * 迁移服务， 只能选一个组
         */
        const checkServers = this.expandServers.filter(
          (server) => server.isChecked
        );
        let groupNames = checkServers.map((server) => server.group_name);
        groupNames = Array.from(new Set(groupNames));
        if (groupNames.length > 1)
          return this.$tip.error(this.$t("dcache.oneGroup"));

        // 去掉源组的目标组
        this.dstGroupNames = allGroupNames.filter(
          (groupName) => groupName !== groupNames[0]
        );

        // 该模块已经有任务在迁移操作了， 不允许再迁移， 请去操作管理停止再迁移
        const { appName, moduleName, srcGroupName } = this;
        let hasOperationRecord = await hasOperation({
          appName,
          moduleName,
          srcGroupName,
        });
        if (hasOperationRecord)
          throw new Error(this.$t("dcache.hasMigrationOperation"));

        this.show = true;
      } catch (err) {
        console.error(err);
        this.$tip.error(err.message);
      }
    },
    async submitServerConfig() {
      if (this.$refs.detailForm.validate()) {
        try {
          let {
            appName,
            moduleName,
            srcGroupName,
            groupName,
            transferData,
          } = this;

          // 扩容取到发布 id
          const data = await transferDCacheGroup({
            appName,
            moduleName,
            srcGroupName,
            dstGroupName: groupName,
            transferData,
          });

          this.$tip.success(
            this.$t("dcache.operationManage.hasnonServerMigration")
          );
        } catch (err) {
          console.error(err);
          this.$tip.error(err.message);
        }
      }
    },
  },
};
</script>
<style>
.non-server-migration .let-form-item__label {
  margin-left: 0px;
}
.non-server-migration.let-form {
  background-color: white;
}
.non-server-migration .let-select.let-select_single {
  width: 300px;
}
</style>
