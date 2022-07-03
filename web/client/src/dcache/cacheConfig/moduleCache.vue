<template>
  <section class="moduleCache">
    <h4>
      {{ this.$t("serverList.title.serverList") }}
      <i class="el-icon-refresh-right" @click="getServerList"></i>
    </h4>
    <br />
    <!-- 服务列表 -->
    <let-table
      v-if="serverList"
      :data="serverList"
      :empty-msg="$t('common.nodata')"
      ref="serverListLoading"
    >
      <let-table-column
        :title="$t('serverList.table.th.service')"
        prop="server_name"
      ></let-table-column>
      <let-table-column
        :title="$t('serverList.table.th.ip')"
        prop="node_name"
        width="140px"
      ></let-table-column>
      <let-table-column :title="$t('serverList.table.th.zb')" width="140px">
        <template slot-scope="{ row: { server_type } }">
          <span v-if="server_type === 'M'">{{ $t("cache.mainEngine") }}</span>
          <span v-else-if="server_type === 'S'">{{
            $t("cache.standByEngine")
          }}</span>
          <span v-else-if="server_type === 'I'">{{ $t("cache.mirror") }}</span>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('serverList.table.th.configStatus')"
        width="90px"
      >
        <template slot-scope="scope">
          <span
            :class="
              scope.row.setting_state === 'active'
                ? 'status-active'
                : 'status-off'
            "
          ></span>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('serverList.table.th.currStatus')"
        width="65px"
      >
        <template slot-scope="scope">
          <span
            :class="
              scope.row.present_state === 'active'
                ? 'status-active'
                : scope.row.present_state === 'activating'
                ? 'status-activating'
                : 'status-off'
            "
          ></span>
        </template>
      </let-table-column>
      <let-table-column :title="$t('serverList.table.th.time')">
        <template slot-scope="scope">
          <span style="word-break: break-word">{{
            handleNoPublishedTime(scope.row.patch_time)
          }}</span>
        </template>
      </let-table-column>
      <let-table-column :title="$t('operate.operates')" width="260px">
        <template slot-scope="{ row }">
          <let-table-operation @click="editServerConfig(row)">{{
            $t("cache.config.edit")
          }}</let-table-operation>
          <let-table-operation @click="checkServerConfigList(row)">{{
            $t("cache.config.view")
          }}</let-table-operation>
          <let-table-operation @click="addServerConfig(row)">{{
            $t("cache.config.add")
          }}</let-table-operation>
        </template>
      </let-table-column>
    </let-table>
    <let-table
      :data="showConfigList"
      :title="$t('cache.config.tableTitle')"
      :empty-msg="$t('common.nodata')"
      :stripe="true"
    >
      <let-table-column width="50">
        <template slot="head" slot-scope="props">
          <let-checkbox
            v-model="isCheckedAll"
            :value="isCheckedAll"
            :change="checkedAllChange"
          ></let-checkbox>
        </template>
        <template slot-scope="scope">
          <let-checkbox
            v-model="scope.row.isChecked"
            :value="scope.row.id"
          ></let-checkbox>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('cache.config.remark')"
        prop="remark"
        width="300"
      ></let-table-column>
      <let-table-column
        :title="$t('cache.config.path')"
        prop="path"
        width="100"
      ></let-table-column>
      <let-table-column
        :title="$t('cache.config.item')"
        prop="item"
        width="100"
      ></let-table-column>
      <let-table-column
        :title="$t('cache.config.config_value')"
        prop="config_value"
        width="200"
      >
        <template slot-scope="{ row: { config_value } }"
          ><div style="white-space: pre-wrap;">
            {{ config_value }}
          </div></template
        >
      </let-table-column>
      <let-table-column
        :title="$t('cache.config.modify_value')"
        prop="period"
        width="200"
      >
        <template slot-scope="{ row }">
          <let-input
            size="small"
            v-model="row.modify_value"
            type="textarea"
            @focus="inputFocus"
            @blur="inputBlur"
            :rows="1"
          >
          </let-input>
        </template>
      </let-table-column>
      <let-table-column :title="$t('operate.operates')">
        <template slot-scope="{ row }">
          <let-table-operation @click="saveConfig(row)">{{
            $t("operate.save")
          }}</let-table-operation>
          <let-table-operation @click="deleteConfig(row)" class="danger">{{
            $t("operate.delete")
          }}</let-table-operation>
        </template>
      </let-table-column>
      <!--分页-->
      <let-pagination
        :total="total"
        :page="pagination.page"
        show-sums
        :sum="configList.length"
        jump
        @change="changePage"
        slot="pagination"
        align="right"
        v-if="total"
      >
      </let-pagination>
      <!--批量操作-->
      <template slot="operations">
        <let-button
          theme="success"
          :disabled="!hasCheckedItem"
          @click="saveConfigBatch"
          >{{ $t("cache.config.batchUpdate") }}</let-button
        >
        <let-button
          theme="danger"
          :disabled="!hasCheckedItem"
          @click="deleteServerConfigItemBatch"
          >{{ $t("cache.config.batchDelete") }}</let-button
        >
        <let-button
          theme="primary"
          @click="addServerConfig({ appName, moduleName })"
          >{{ $t("cache.config.addModuleConfig") }}</let-button
        >
      </template>
    </let-table>

    <!-- 查看服务列表配置-->
    <let-modal
      v-model="serverConfigListVisible"
      :footShow="false"
      :closeOnClickBackdrop="true"
      width="80%"
      height="80%"
      :title="$t('cache.config.tableTitle')"
      class="server_config_list_modal"
    >
      <server-config-list
        v-if="serverConfigListVisible"
        :moduleName="moduleName"
        v-bind="checkServer"
      ></server-config-list>
    </let-modal>

    <!-- 修改服务配置-->
    <let-modal
      v-model="serverConfigVisible"
      :footShow="false"
      :closeOnClickBackdrop="true"
      width="80%"
      height="80%"
      :title="$t('cache.config.tableTitle')"
      class="server_config_list_modal"
    >
      <server-config
        v-if="serverConfigVisible"
        v-bind="checkServer"
      ></server-config>
    </let-modal>

    <!-- 添加服务配置-->
    <let-modal
      v-model="addServerConfigVisible"
      :footShow="false"
      :closeOnClickBackdrop="true"
    >
      <add-server-config
        v-if="addServerConfigVisible"
        v-bind="checkServer"
        @call-back="getModuleConfig"
      ></add-server-config>
    </let-modal>
  </section>
</template>
<script>
import ServerConfigList from "./ServerConfigList.vue";
import ServerConfig from "./ServerConfig.vue";
import addServerConfig from "./addServerConfig.vue";
import { getModuleConfig, getCacheServerList } from "@/plugins/interface.js";
export default {
  components: {
    ServerConfigList,
    ServerConfig,
    addServerConfig,
  },
  data() {
    return {
      appName: "",
      moduleName: "",
      configList: [],
      serverList: [],
      serverConfigListVisible: false,
      serverConfigVisible: false,
      addServerConfigVisible: false,
      checkServer: {},
      isCheckedAll: false,
      pagination: {
        page: 1,
      },
    };
  },
  computed: {
    showConfigList() {
      return this.configList.slice(
        100 * (this.pagination.page - 1),
        100 * this.pagination.page
      );
    },
    total() {
      return Math.ceil(this.configList.length / 100);
    },
    hasCheckedItem() {
      return (
        this.showConfigList.filter((item) => item.isChecked === true).length !==
        0
      );
    },
  },
  watch: {
    isCheckedAll() {
      const isCheckedAll = this.isCheckedAll;
      this.showConfigList.forEach((item) => {
        item.isChecked = isCheckedAll;
      });
    },
  },
  methods: {
    checkedAllChange() {
      // console.log(arguments);
    },
    changePage(page) {
      this.pagination.page = page;
    },
    inputFocus(e) {
      e.target.rows = 4;
    },
    inputBlur(e) {
      e.target.rows = 1;
    },
    async getModuleConfig() {
      try {
        const { appName, moduleName } = this;
        let configItemList = await getModuleConfig({ appName, moduleName });
        // 添加被修改的空值           // 默认全部不选中
        configItemList.forEach((item) => {
          item.modify_value = "";
          item.isChecked = false;
        });
        this.configList = configItemList;
      } catch (err) {
        // console.error(err)
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      }
    },
    // 获取服务列表
    async getServerList() {
      const loading = this.$Loading.show();
      try {
        const { appName, moduleName } = this;
        const data = await getCacheServerList({ appName, moduleName });
        this.serverList = data;
      } catch (err) {
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      }

      loading.hide();
    },
    async saveConfig(row) {
      let { id, modify_value } = row;
      try {
        let configItemList = await this.$ajax.getJSON(
          "/server/api/cache/updateServerConfigItem",
          {
            id,
            configValue: modify_value,
          }
        );
        await this.getModuleConfig();
      } catch (err) {
        // console.error(err)
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      }
    },
    async saveConfigBatch() {
      let serverConfigList = this.showConfigList
        .filter((item) => item.isChecked)
        .map((item) => {
          return {
            indexId: item.id,
            configValue: item.modify_value,
          };
        });
      try {
        let configItemList = await this.$ajax.postJSON(
          "/server/api/cache/updateServerConfigItemBatch",
          { serverConfigList }
        );
        await this.getModuleConfig();
      } catch (err) {
        // console.error(err)
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      }
    },
    deleteConfig({ id }) {
      this.$confirm(
        this.$t("cache.config.deleteConfig"),
        this.$t("common.alert")
      ).then(async () => {
        try {
          let configItemList = await this.$ajax.getJSON(
            "/server/api/cache/deleteServerConfigItem",
            { id }
          );
          await this.getModuleConfig();
        } catch (err) {
          console.error(err);
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        }
      });
    },
    deleteServerConfigItemBatch({ id }) {
      let serverConfigList = this.showConfigList
        .filter((item) => item.isChecked)
        .map((item) => {
          return {
            indexId: item.id,
          };
        });
      this.$confirm(
        this.$t("cache.config.deleteConfig"),
        this.$t("common.alert")
      ).then(async () => {
        try {
          let configItemList = await this.$ajax.postJSON(
            "/server/api/cache/deleteServerConfigItemBatch",
            { serverConfigList }
          );
          await this.getModuleConfig();
        } catch (err) {
          // console.error(err)
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        }
      });
    },

    // 处理未发布时间显示
    handleNoPublishedTime(timeStr, noPubTip = this.$t("pub.dlg.unpublished")) {
      if (timeStr === "0000:00:00 00:00:00") {
        return noPubTip;
      }
      return timeStr;
    },
    checkServerConfigList(row) {
      this.serverConfigListVisible = true;
      this.checkServer = {
        appName: this.appName,
        serverName: row.server_name,
        nodeName: row.node_name,
      };
    },
    editServerConfig(row) {
      this.serverConfigVisible = true;
      this.checkServer = {
        serverName: row.server_name,
        nodeName: row.node_name,
      };
    },
    addServerConfig(row) {
      // 有moduleName 的是模块添加配置，  只有 serverName 和 nodeName 的是服务添加配置
      this.addServerConfigVisible = true;
      this.checkServer = {
        serverName: row.server_name,
        nodeName: row.node_name,
        moduleName: row.moduleName,
        appName: row.appName,
      };
    },
  },
  created() {
    const { application, module_name } = this.$parent.getServerData();
    this.appName = application;
    this.moduleName = module_name;
    this.getModuleConfig();
    this.getServerList();
  },
};
</script>

<style>
.server_config_list_modal .let_modal__body {
  max-height: 500px;
  overflow-y: auto;
  margin-top: 20px;
}
.moduleCache .let-input textarea {
  padding: 8px;
}
</style>
