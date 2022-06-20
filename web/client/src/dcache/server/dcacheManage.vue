<template>
  <div class="page_server_manage">
    <!-- 服务列表 -->
    <div class="table_head">
      <h4>
        {{ this.$t("serverList.title.serverList") }}
        <i class="el-icon-refresh-right" @click="getServerList"></i>
      </h4>
    </div>

    <!-- 服务列表 -->
    <let-table
      class="dcache"
      v-if="serverList"
      :data="serverList"
      :empty-msg="$t('common.nodata')"
      ref="serverListLoading"
    >
      <let-table-column>
        <template slot="head" slot-scope="props">
          <let-checkbox
            v-model="isCheckedAll"
            :value="isCheckedAll"
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
        :title="$t('serverList.table.th.service')"
        prop="server_name"
      >
        <template slot-scope="scope">
          <a
            :href="
              '/static/logview/logview.html?app=' +
                [scope.row.application] +
                '&server_name=' +
                [scope.row.server_name] +
                '&node_name=' +
                [scope.row.node_name]
            "
            title="点击查看服务日志(view server logs)"
            target="_blank"
            class="buttonText"
          >
            {{ scope.row.server_name }}
          </a>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('serverList.table.th.ip')"
        prop="node_name"
        width="140px"
      ></let-table-column>
      <let-table-column :title="$t('serverList.table.th.enableSet')">
        <template slot-scope="scope">
          <span v-if="!scope.row.enable_set">{{ $t("common.disable") }}</span>
          <p v-else style="max-width: 200px">
            {{ $t("common.set.setName") }}：{{ scope.row.set_name }}<br />
            {{ $t("common.set.setArea") }}：{{ scope.row.set_area }}<br />
            {{ $t("common.set.setGroup") }}：{{ scope.row.set_group }}
          </p>
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
      <let-table-column
        :title="$t('serverList.table.th.processID')"
        prop="process_id"
        width="80px"
      ></let-table-column>
      <let-table-column
        :title="$t('serverList.table.th.version')"
        prop="patch_version"
        width="68px"
      ></let-table-column>
      <let-table-column :title="$t('serverList.table.th.time')">
        <template slot-scope="scope">
          <span style="word-break: break-word">{{
            handleNoPublishedTime(scope.row.patch_time)
          }}</span>
        </template>
      </let-table-column>
      <let-table-column :title="$t('operate.operates')" width="260px">
        <template slot-scope="scope">
          <let-table-operation @click="configServer(scope.row.id)">{{
            $t("operate.update")
          }}</let-table-operation>
          <let-table-operation
            @click="openExpandModal(scope.row)"
            v-if="serverType === 'proxy' || serverType === 'router'"
            >{{ $t("dcache.expand") }}</let-table-operation
          >
          <let-table-operation @click="restartServer(scope.row.id)">{{
            $t("operate.restart")
          }}</let-table-operation>
          <let-table-operation
            class="danger"
            @click="stopServer(scope.row.id)"
            >{{ $t("operate.stop") }}</let-table-operation
          >
          <let-table-operation @click="manageServant(scope.row)">{{
            $t("operate.servant")
          }}</let-table-operation>
          <let-table-operation @click="showMoreCmd(scope.row)">{{
            $t("operate.more")
          }}</let-table-operation>
          <let-table-operation @click="showStatus(scope.row.id)">{{
            $t("operate.status")
          }}</let-table-operation>
        </template>
      </let-table-column>
      <template slot="operations">
        <batch-operation
          size="small"
          :disabled="!hasCheckedServer"
          :checked-servers="checkedServers"
          @success-fn="getServerList"
          type="restart"
        ></batch-operation>
        <batch-operation
          size="small"
          :disabled="!hasCheckedServer"
          :checked-servers="checkedServers"
          @success-fn="getServerList"
          type="stop"
        ></batch-operation>
      </template>
    </let-table>

    <!-- 服务实时状态 -->
    <h4 v-if="serverNotifyList && showOthers">
      {{ this.$t("serverList.title.serverStatus") }}
      <i class="el-icon-refresh-right" @click="getServerNotifyList()"
        >&#xec08;</i
      >
    </h4>
    <let-table
      v-if="serverNotifyList && showOthers"
      :data="serverNotifyList"
      :empty-msg="$t('common.nodata')"
      ref="serverNotifyListLoading"
    >
      <let-table-column
        :title="$t('common.time')"
        prop="notifytime"
        width="200px"
      ></let-table-column>
      <let-table-column
        :title="$t('serverList.table.th.serviceID')"
        prop="server_id"
        width="200px"
      ></let-table-column>
      <let-table-column
        :title="$t('serverList.table.th.threadID')"
        prop="thread_id"
        width="200px"
      ></let-table-column>
      <let-table-column :title="$t('serverList.table.th.result')">
        <template slot-scope="scope">
          <span :style="statusStyle(scope.row.result)">{{
            scope.row.result
          }}</span>
        </template>
      </let-table-column>
    </let-table>
    <let-pagination
      :page="pageNum"
      @change="gotoPage"
      style="margin-bottom: 32px;"
      :total="total"
    >
    </let-pagination>

    <!-- 服务扩容弹窗 -->
    <let-modal
      v-model="expandModal.show"
      :title="$t('serviceExpand.title')"
      width="800px"
      :footShow="!!(expandModal.model && expandModal.model.server_name)"
      @on-confirm="expandService"
      @close="closeExpandModal"
      @on-cancel="closeExpandModal"
    >
      <let-form
        v-if="!!(expandModal.model && expandModal.model.server_name)"
        ref="expandForm"
        itemWidth="360px"
        :columns="2"
        class="two-columns"
      >
        <let-form-item :label="$t('common.service')">{{
          expandModal.model.server_name
        }}</let-form-item>
        <let-form-item :label="$t('common.ip')">{{
          expandModal.model.node_name
        }}</let-form-item>
        <let-form-item
          :label="$t('serviceExpand.form.tarIP')"
          itemWidth="100%"
          required
        >
          <let-input
            type="textarea"
            :rows="3"
            v-model="expandModal.expandIpStr"
            :placeholder="$t('serviceExpand.form.placeholder')"
            required
            :required-tip="$t('deployService.table.tips.empty')"
          >
          </let-input>
        </let-form-item>
        <let-form-item
          :label="$t('serviceExpand.form.nodeConfig')"
          itemWidth="100%"
        >
          <let-checkbox v-model="expandModal.model.copy_node_config">
            {{ $t("serviceExpand.form.copyNodeConfig") }}
          </let-checkbox>
        </let-form-item>
      </let-form>
    </let-modal>

    <!-- 编辑服务弹窗 -->
    <let-modal
      v-model="configModal.show"
      :title="$t('serverList.dlg.title.editService')"
      width="800px"
      :footShow="!!(configModal.model && configModal.model.server_name)"
      @on-confirm="saveConfig"
      @close="closeConfigModal"
      @on-cancel="closeConfigModal"
    >
      <let-form
        v-if="!!(configModal.model && configModal.model.server_name)"
        ref="configForm"
        itemWidth="360px"
        :columns="2"
        class="two-columns"
      >
        <let-form-item :label="$t('common.service')">{{
          configModal.model.server_name
        }}</let-form-item>
        <let-form-item :label="$t('common.ip')">{{
          configModal.model.node_name
        }}</let-form-item>
        <let-form-item :label="$t('serverList.dlg.isBackup')" required>
          <let-radio-group
            size="small"
            v-model="configModal.model.bak_flag"
            :data="[
              { value: true, text: $t('common.yes') },
              { value: false, text: $t('common.no') },
            ]"
          >
          </let-radio-group>
        </let-form-item>
        <let-form-item :label="$t('common.template')" required>
          <let-select
            size="small"
            v-model="configModal.model.template_name"
            v-if="
              configModal.model.templates && configModal.model.templates.length
            "
            required
          >
            <let-option
              v-for="t in configModal.model.templates"
              :key="t"
              :value="t"
              >{{ t }}</let-option
            >
          </let-select>
          <span v-else>{{ configModal.model.template_name }}</span>
        </let-form-item>
        <let-form-item :label="$t('serverList.dlg.serviceType')" required>
          <let-select
            size="small"
            v-model="configModal.model.server_type"
            required
          >
            <let-option v-for="t in serverTypes" :key="t" :value="t">{{
              t
            }}</let-option>
          </let-select>
        </let-form-item>
        <let-form-item :label="$t('serverList.table.th.enableSet')" required>
          <let-radio-group
            size="small"
            v-model="configModal.model.enable_set"
            :data="[
              { value: true, text: $t('common.enable') },
              { value: false, text: $t('common.disable') },
            ]"
          >
          </let-radio-group>
        </let-form-item>
        <let-form-item
          :label="$t('common.set.setName')"
          required
          v-if="configModal.model.enable_set"
        >
          <let-input
            size="small"
            v-model="configModal.model.set_name"
            :placeholder="$t('serverList.dlg.errMsg.setName')"
            required
            pattern="^[a-z]+$"
            :pattern-tip="$t('serverList.dlg.errMsg.setName')"
          ></let-input>
        </let-form-item>
        <let-form-item
          :label="$t('common.set.setArea')"
          required
          v-if="configModal.model.enable_set"
        >
          <let-input
            size="small"
            v-model="configModal.model.set_area"
            :placeholder="$t('serverList.dlg.errMsg.setArea')"
            required
            pattern="^[a-z]+$"
            :pattern-tip="$t('serverList.dlg.errMsg.setArea')"
          ></let-input>
        </let-form-item>
        <let-form-item
          :label="$t('common.set.setGroup')"
          required
          v-if="configModal.model.enable_set"
        >
          <let-input
            size="small"
            v-model="configModal.model.set_group"
            :placeholder="$t('serverList.dlg.errMsg.setGroup')"
            required
            pattern="^[0-9\*]+$"
            :pattern-tip="$t('serverList.dlg.errMsg.setGroup')"
          ></let-input>
        </let-form-item>

        <let-form-item :label="$t('serverList.dlg.asyncThread')" required>
          <let-input
            size="small"
            v-model="configModal.model.async_thread_num"
            :placeholder="$t('serverList.dlg.placeholder.thread')"
            required
            :pattern="
              configModal.model.server_type === 'tars_nodejs'
                ? '^[1-9][0-9]*$'
                : '^([3-9]|[1-9][0-9]+)$'
            "
            pattern-tip="$t('serverList.dlg.placeholder.thread')"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('serverList.dlg.defaultPath')">
          <let-input
            size="small"
            v-model="configModal.model.base_path"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('serverList.dlg.exePath')">
          <let-input
            size="small"
            v-model="configModal.model.exe_path"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('serverList.dlg.startScript')">
          <let-input
            size="small"
            v-model="configModal.model.start_script_path"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('serverList.dlg.stopScript')">
          <let-input
            size="small"
            v-model="configModal.model.stop_script_path"
          ></let-input>
        </let-form-item>
        <let-form-item
          :label="$t('serverList.dlg.monitorScript')"
          itemWidth="724px"
        >
          <let-input
            size="small"
            v-model="configModal.model.monitor_script_path"
          ></let-input>
        </let-form-item>
        <let-form-item
          :label="$t('serverList.dlg.privateTemplate')"
          labelWidth="150px"
          itemWidth="724px"
        >
          <let-input
            size="large"
            type="textarea"
            :rows="4"
            v-model="configModal.model.profile"
          ></let-input>
        </let-form-item>
      </let-form>
      <div v-else class="loading-placeholder" ref="configFormLoading"></div>
    </let-modal>

    <!-- Servant管理弹窗 -->
    <let-modal
      v-model="servantModal.show"
      :title="$t('serverList.table.servant.title')"
      width="1200px"
      :footShow="false"
      @close="closeServantModal"
    >
      <let-button
        size="small"
        theme="primary"
        class="tbm16"
        @click="configServant()"
        >{{ $t("operate.add") }} Servant</let-button
      >
      <let-table
        v-if="servantModal.model"
        :data="servantModal.model"
        :empty-msg="$t('common.nodata')"
      >
        <let-table-column
          :title="$t('operate.servant')"
          prop="servant"
        ></let-table-column>
        <let-table-column
          :title="$t('serverList.table.servant.adress')"
          prop="endpoint"
        ></let-table-column>
        <let-table-column
          :title="$t('serverList.table.servant.thread')"
          prop="thread_num"
        ></let-table-column>
        <let-table-column
          :title="$t('serverList.table.servant.connections')"
          prop="max_connections"
        ></let-table-column>
        <let-table-column
          :title="$t('serverList.table.servant.capacity')"
          prop="queuecap"
        ></let-table-column>
        <let-table-column
          :title="$t('serverList.table.servant.timeout')"
          prop="queuetimeout"
        ></let-table-column>
        <let-table-column :title="$t('operate.operates')" width="90px">
          <template slot-scope="scope">
            <let-table-operation @click="configServant(scope.row.id)">{{
              $t("operate.update")
            }}</let-table-operation>
            <let-table-operation
              class="danger"
              @click="deleteServant(scope.row.id)"
              >{{ $t("operate.delete") }}</let-table-operation
            >
          </template>
        </let-table-column>
      </let-table>
      <div v-else class="loading-placeholder" ref="servantModalLoading"></div>
    </let-modal>

    <!-- Servant新增、编辑弹窗 -->
    <let-modal
      v-model="servantDetailModal.show"
      :title="
        servantDetailModal.isNew
          ? `${$t('operate.add')} Servant`
          : `${$t('operate.update')} Servant`
      "
      width="800px"
      :footShow="!!servantDetailModal.model"
      @on-confirm="saveServantDetail"
      @close="closeServantDetailModal"
      @on-cancel="closeServantDetailModal"
    >
      <let-form
        v-if="servantDetailModal.model"
        ref="servantDetailForm"
        itemWidth="360px"
        :columns="2"
        class="two-columns"
      >
        <let-form-item
          :label="$t('serverList.servant.appService')"
          itemWidth="724px"
        >
          <span
            >{{ servantDetailModal.model.application }}·{{
              servantDetailModal.model.server_name
            }}</span
          >
        </let-form-item>
        <let-form-item :label="$t('serverList.servant.objName')" required>
          <let-input
            size="small"
            v-model="servantDetailModal.model.obj_name"
            :placeholder="$t('serverList.servant.c')"
            required
            pattern="^[A-Za-z]+$"
            :pattern-tip="$t('serverList.servant.obj')"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('serverList.servant.numOfThread')" required>
          <let-input
            size="small"
            v-model="servantDetailModal.model.thread_num"
            :placeholder="$t('serverList.servant.thread')"
            required
            pattern="^[1-9][0-9]*$"
            :pattern-tip="$t('serverList.servant.thread')"
          ></let-input>
        </let-form-item>
        <let-form-item
          :label="$t('serverList.table.servant.adress')"
          required
          itemWidth="724px"
        >
          <let-input
            ref="endpoint"
            size="small"
            v-model="servantDetailModal.model.endpoint"
            placeholder="tcp -h 127.0.0.1 -t 60000 -p 12000"
            required
            :extraTip="isEndpointValid ? '' : $t('serverList.servant.error')"
          ></let-input>
        </let-form-item>
        <let-form-item
          :label="$t('serverList.servant.connections')"
          labelWidth="150px"
        >
          <let-input
            size="small"
            v-model="servantDetailModal.model.max_connections"
          ></let-input>
        </let-form-item>
        <let-form-item
          :label="$t('serverList.servant.lengthOfQueue')"
          labelWidth="150px"
        >
          <let-input
            size="small"
            v-model="servantDetailModal.model.queuecap"
          ></let-input>
        </let-form-item>
        <let-form-item
          :label="$t('serverList.servant.queueTimeout')"
          labelWidth="150px"
        >
          <let-input
            size="small"
            v-model="servantDetailModal.model.queuetimeout"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('serverList.servant.allowIP')">
          <let-input
            size="small"
            v-model="servantDetailModal.model.allow_ip"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('serverList.servant.protocol')" required>
          <let-radio-group
            size="small"
            v-model="servantDetailModal.model.protocol"
            :data="[
              { value: 'tars', text: 'TARS' },
              { value: 'not_tars', text: $t('serverList.servant.notTARS') },
            ]"
          >
          </let-radio-group>
        </let-form-item>
        <let-form-item
          :label="$t('serverList.servant.treatmentGroup')"
          labelWidth="150px"
        >
          <let-input
            size="small"
            v-model="servantDetailModal.model.handlegroup"
          ></let-input>
        </let-form-item>
      </let-form>
    </let-modal>

    <!-- 更多命令弹窗 -->
    <let-modal
      v-model="moreCmdModal.show"
      :title="$t('operate.more')"
      width="700px"
      class="more-cmd"
      @on-confirm="invokeMoreCmd"
      @close="closeMoreCmdModal"
      @on-cancel="closeMoreCmdModal"
    >
      <let-form v-if="moreCmdModal.model" ref="moreCmdForm">
        <let-form-item itemWidth="100%">
          <let-radio
            v-model="moreCmdModal.model.selected"
            label="setloglevel"
            >{{ $t("serverList.servant.logLevel") }}</let-radio
          >
          <let-select
            size="small"
            :disabled="moreCmdModal.model.selected !== 'setloglevel'"
            v-model="moreCmdModal.model.setloglevel"
          >
            <let-option v-for="l in logLevels" :key="l" :value="l">{{
              l
            }}</let-option>
          </let-select>
        </let-form-item>
        <let-form-item itemWidth="100%">
          <let-radio v-model="moreCmdModal.model.selected" label="loadconfig">{{
            $t("serverList.servant.pushFile")
          }}</let-radio>
          <let-select
            size="small"
            :placeholder="
              moreCmdModal.model.configs && moreCmdModal.model.configs.length
                ? $t('pub.dlg.defaultValue')
                : $t('pub.dlg.noConfFile')
            "
            :disabled="
              !(
                moreCmdModal.model.configs && moreCmdModal.model.configs.length
              ) || moreCmdModal.model.selected !== 'loadconfig'
            "
            v-model="moreCmdModal.model.loadconfig"
            :required="moreCmdModal.model.selected === 'loadconfig'"
          >
            <let-option
              v-for="l in moreCmdModal.model.configs"
              :key="l.filename"
              :value="l.filename"
              >{{ l.filename }}</let-option
            >
          </let-select>
        </let-form-item>
        <let-form-item itemWidth="100%">
          <let-radio v-model="moreCmdModal.model.selected" label="command">{{
            $t("serverList.servant.sendCommand")
          }}</let-radio>
          <let-input
            size="small"
            :disabled="moreCmdModal.model.selected !== 'command'"
            v-model="moreCmdModal.model.command"
            :required="moreCmdModal.model.selected === 'command'"
          ></let-input>
        </let-form-item>
        <let-form-item itemWidth="100%">
          <let-radio v-model="moreCmdModal.model.selected" label="connection">{{
            $t("serverList.servant.serviceLink")
          }}</let-radio>
        </let-form-item>
        <let-form-item itemWidth="100%" v-if="serverType !== 'dcache'">
          <let-radio
            v-model="moreCmdModal.model.selected"
            label="undeploy_tars"
            class="danger"
            >{{ $t("operate.undeploy") }} {{ $t("common.service") }}</let-radio
          >
        </let-form-item>
      </let-form>
    </let-modal>
  </div>
</template>

<script>
// import {
//   expandServerPreview,
//   autoPort,
//   expandServer,
//   addTask,
// } from "@/plugins/interface.js";

import batchOperation from "./../dcache/moduleManage/batchOperation.vue";
const getInitialExpandModel = () => ({
  application: "DCache",
  server_name: "",
  set: "",
  node_name: "",
  expand_nodes: [],
  enable_set: false,
  set_name: "",
  set_area: "",
  set_group: "",
  copy_node_config: false,
});
export default {
  name: "ServerManage",
  components: { batchOperation },
  data() {
    return {
      // 当前页面信息
      isCheckedAll: false,
      serverType: this.servertype || "tars_cpp",
      serverData: {
        level: 5,
        application: "",
        server_name: "",
        set_name: "",
        set_area: "",
        set_group: "",
      },

      // 服务列表
      serverList: [],

      // 操作历史列表
      serverNotifyList: [],
      pageNum: 1,
      pageSize: 20,
      total: 1,

      // 编辑服务
      serverTypes: [
        "tars_cpp",
        "tars_java",
        "tars_php",
        "tars_nodejs",
        "not_tars",
        "tars_go",
      ],
      configModal: {
        show: false,
        model: null,
      },

      // 编辑servant
      servantModal: {
        show: false,
        model: null,
        currentServer: null,
      },
      servantDetailModal: {
        show: false,
        isNew: true,
        model: null,
      },

      // 更多命令
      logLevels: ["NONE", "TARS", "DEBUG", "INFO", "WARN", "ERROR"],
      moreCmdModal: {
        show: false,
        model: null,
        currentServer: null,
      },

      // 服务扩容
      expandModal: {
        show: false,
        model: getInitialExpandModel(),
        expandIpStr: "",
      },

      // 失败重试次数
      failCount: 0,
    };
  },
  props: ["treeid", "servertype"],
  computed: {
    showOthers() {
      return this.serverData.level === 5;
    },
    isEndpointValid() {
      if (
        !this.servantDetailModal.model ||
        !this.servantDetailModal.model.endpoint
      ) {
        return false;
      }
      return this.checkServantEndpoint(this.servantDetailModal.model.endpoint);
    },
    hasCheckedServer() {
      return (
        this.serverList.filter((item) => item.isChecked === true).length !== 0
      );
    },
    checkedServers() {
      return this.serverList.filter((item) => item.isChecked === true);
    },
  },
  watch: {
    isCheckedAll() {
      let isCheckedAll = this.isCheckedAll;
      this.serverList.forEach((item) => {
        item.isChecked = isCheckedAll;
      });
    },
    $route(to, from) {
      this.getServerList();
      this.getServerNotifyList(1);
    },
  },
  methods: {
    // 获取服务列表
    getServerList() {
      const loading = this.$refs.serverListLoading.$loading.show();

      this.$tars
        .getJSON("/server/api/server_list", {
          tree_node_id: this.treeid,
        })
        .then(async (data) => {
          loading.hide();
          data.forEach((item) => {
            item.isChecked = false;
          });

          this.serverList = data;

          //在模块服务节点列表，如果该服务没有节点了， 在目录树去掉该服务；
          if (this.serverType === "dcache") {
            let serverLength = this.serverList.length;
            if (serverLength > 0) return false;
            try {
              await this.$ajax.postJSON("/server/api/cache/removeServer", {
                server_name: this.treeid.split(".")[1].substr(1),
              });
              this.$parent.getTreeData();
            } catch (err) {
              console.error(err);
            }
          } else if (this.serverType === "proxy") {
            //在proxy服务节点列表，如果该服务没有节点了， 在目录树去掉该服务；
            let serverLength = this.serverList.length;
            if (serverLength > 0) return false;
            try {
              await this.$ajax.postJSON("/server/api/cache/removeProxy", {
                server_name: this.treeid.split(".")[1].substr(1),
              });
              this.$parent.getTreeData();
            } catch (err) {
              console.error(err);
            }
          } else if (this.serverType === "router") {
            //在proxy服务节点列表，如果该服务没有节点了， 在目录树去掉该服务；
            let serverLength = this.serverList.length;
            if (serverLength > 0) return false;
            try {
              await this.$ajax.postJSON("/server/api/cache/removeRouter", {
                server_name: this.treeid.split(".")[1].substr(1),
              });
              this.$parent.getTreeData();
            } catch (err) {
              console.error(err);
            }
          }
        })
        .catch((err) => {
          loading.hide();
          this.$confirm(
            err.err_msg || err.message || this.$t("serverList.msg.fail"),
            this.$t("common.alert")
          ).then(() => {
            this.getServerList();
          });
        });
    },
    // 获取服务实时状态
    getServerNotifyList(curr_page) {
      if (!this.showOthers) return;

      this.$tars
        .getJSON("/server/api/server_notify_list", {
          tree_node_id: this.treeid,
          page_size: this.pageSize,
          curr_page: curr_page,
        })
        .then((data) => {
          this.pageNum = curr_page;
          this.total = Math.ceil(data.count / this.pageSize);
          this.serverNotifyList = data.rows;
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("serverList.restart.failed")}: ${err.err_msg ||
              err.message}`
          );
        });
    },
    statusStyle(message) {
      message = message || "";

      if (message == "restart" || message.indexOf("[succ]") != -1) {
        return "color: green";
      } else if (
        message == "stop" ||
        message.indexOf("[alarm]") != -1 ||
        message.indexOf("error") != -1 ||
        message.indexOf("ERROR") != -1
      ) {
        return "color: red";
      }
      return "";
    },
    // 切换服务实时状态页码
    gotoPage(num) {
      this.getServerNotifyList(num);
    },

    // 获取模版列表
    getTemplateList() {
      this.$tars
        .getJSON("/server/api/template_name_list")
        .then((data) => {
          if (this.configModal.model) {
            this.configModal.model.templates = data;
          } else {
            this.configModal.model = { templates: data };
          }
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("serverList.restart.failed")}: ${err.err_msg ||
              err.message}`
          );
        });
    },
    // 获取服务数据
    getServerConfig(id) {
      const loading = this.$loading.show();

      this.$tars
        .getJSON("/server/api/server", {
          id,
        })
        .then((data) => {
          loading.hide();
          if (this.configModal.model) {
            this.configModal.model = Object.assign(
              {},
              this.configModal.model,
              data
            );
          } else {
            data.templates = [];
            this.configModal.model = data;
          }
        })
        .catch((err) => {
          loading.hide();
          this.closeConfigModal();
          this.$tip.error(
            `${this.$t("serverList.restart.failed")}: ${err.err_msg ||
              err.message}`
          );
        });
    },
    // 编辑服务
    configServer(id) {
      this.configModal.show = true;
      this.getTemplateList();
      this.getServerConfig(id);
    },
    saveConfig() {
      if (this.$refs.configForm.validate()) {
        const loading = this.$Loading.show();
        this.$tars
          .postJSON("/server/api/update_server", {
            isBak: this.configModal.model.bak_flag,
            ...this.configModal.model,
          })
          .then((res) => {
            loading.hide();
            this.serverList = this.serverList.map((item) => {
              if (item.id === res.id) {
                return res;
              }
              return item;
            });
            this.closeConfigModal();
            this.$tip.success(this.$t("serverList.restart.success"));
          })
          .catch((err) => {
            loading.hide();
            this.$tip.error(
              `${this.$t("serverList.restart.failed")}: ${err.message ||
                err.err_msg}`
            );
          });
      }
    },
    closeConfigModal() {
      if (this.$refs.configForm) this.$refs.configForm.resetValid();
      this.configModal.show = false;
      this.configModal.model = null;
    },

    // 检查任务状态
    checkTaskStatus(taskid, isRetry) {
      return new Promise((resolve, reject) => {
        this.$tars
          .getJSON("/server/api/task", {
            task_no: taskid,
          })
          .then((data) => {
            // 进行中，1秒后重试
            if (data.status === 1 || data.status === 0) {
              setTimeout(() => {
                resolve(this.checkTaskStatus(taskid));
              }, 3000);
              // 成功
            } else if (data.status === 2) {
              resolve(`taskid: ${data.task_no}`);
              // 失败
            } else {
              reject(new Error(`taskid: ${data.task_no}`));
            }
          })
          .catch((err) => {
            // 网络问题重试1次
            if (isRetry) {
              reject(
                new Error(
                  err.err_msg || err.message || this.$t("common.networkErr")
                )
              );
            } else {
              setTimeout(() => {
                resolve(this.checkTaskStatus(taskid, true));
              }, 3000);
            }
          });
      });
    },
    // 添加任务
    addTask(id, command, tipObj) {
      const loading = this.$Loading.show();
      return this.$tars
        .postJSON("/server/api/add_task", {
          serial: true, // 是否串行
          items: [
            {
              server_id: id,
              command,
            },
          ],
        })
        .then((res) => {
          // eslint-disable-line
          return this.checkTaskStatus(res)
            .then((info) => {
              loading.hide();
              // 任务成功重新拉取列表
              this.getServerList();
              this.$tip.success({
                title: tipObj.success,
                message: info,
              });
            })
            .catch((err) => {
              throw err;
            });
        })
        .catch((err) => {
          loading.hide();
          // 任务失败也重新拉取列表
          this.getServerList();
          this.$tip.error({
            title: tipObj.error,
            message: err.err_msg || err.message || this.$t("common.networkErr"),
          });
        });
    },
    // 重启服务
    restartServer(id) {
      this.addTask(id, "restart", {
        success: this.$t("serverList.restart.success"),
        error: this.$t("serverList.restart.failed"),
      });
    },
    // 停止服务
    stopServer(id) {
      this.$confirm(
        this.$t("serverList.stopService.msg.stopService"),
        this.$t("common.alert")
      ).then(() => {
        this.addTask(id, "stop", {
          success: this.$t("serverList.restart.success"),
          error: this.$t("serverList.restart.failed"),
        });
      });
    },
    // 下线服务
    async undeployServer({ id, server_name, ...a }) {
      this.$confirm(
        this.$t("serverList.dlg.msg.undeploy"),
        this.$t("common.alert")
      ).then(async () => {
        let { serverList, serverType } = this;
        //          console.log(serverList.length, serverType, id , server_name, a);
        if (
          ["proxy", "router"].indexOf(serverType) > -1 &&
          serverList.length === 1
        ) {
          // 如果是仅有的一个 proxy、 router 节点，要确保该应用下没有模块，提醒用户删除该应用下所有的模块才可以删除router、proxy
          let hasModule = await this.$ajax.getJSON(
            "/server/api/cache/hasModule",
            { serverType, serverName: server_name }
          );
          if (hasModule) return this.$tip.warning(this.$t("pub.dlg.hasModule"));

          // 如果下线的是仅有的节点,提醒用户下线后该应用将无用
          try {
            await this.$confirm(this.$t("pub.dlg.cantUseApply"));
          } catch (err) {
            return false;
          }
        }
        try {
          await this.addTask(id, "undeploy_tars", {
            success: this.$t("serverList.restart.success"),
            error: this.$t("serverList.restart.failed"),
          });
        } catch (err) {
        } finally {
          this.closeMoreCmdModal();
        }
      });
    },

    // 管理Servant弹窗
    manageServant(server) {
      this.servantModal.show = true;

      const loading = this.$loading.show({
        target: this.$refs.servantModalLoading,
      });

      this.$tars
        .getJSON("/server/api/adapter_conf_list", {
          id: server.id,
        })
        .then((data) => {
          loading.hide();
          this.servantModal.model = data;
          this.servantModal.currentServer = server;
        })
        .catch((err) => {
          loading.hide();
          this.$tip.error(
            `${this.$t("serverList.restart.failed")}: ${err.err_msg ||
              err.message}`
          );
        });
    },
    closeServantModal() {
      this.servantModal.show = false;
      this.servantModal.model = null;
      this.servantModal.currentServer = null;
    },
    // 新增、编辑 servant
    configServant(id) {
      // 新增
      this.servantDetailModal.model = {
        application: this.servantModal.currentServer.application,
        server_name: this.servantModal.currentServer.server_name,
        obj_name: "",
        node_name: "",
        endpoint: "",
        servant: "",
        thread_num: "",
        max_connections: "200000",
        queuecap: "10000",
        queuetimeout: "60000",
        allow_ip: "",
        protocol: "not_tars",
        handlegroup: "",
      };
      this.servantDetailModal.isNew = true;
      // 编辑
      if (id) {
        const old = this.servantModal.model.find((item) => item.id === id);
        old.obj_name = old.servant.split(".")[2];
        this.servantDetailModal.model = Object.assign(
          {},
          this.servantDetailModal.model,
          old
        );
        this.servantDetailModal.isNew = false;
      }
      this.servantDetailModal.show = true;
    },
    closeServantDetailModal() {
      if (this.$refs.servantDetailForm)
        this.$refs.servantDetailForm.resetValid();
      this.servantDetailModal.show = false;
      this.servantDetailModal.model = null;
    },
    // 检查绑定地址
    checkServantEndpoint(endpoint) {
      const tmp = endpoint.split(/\s-/);
      const regProtocol = /^tcp|udp$/i;
      let regHost = /^h\s(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$/i;
      let regT = /^t\s([1-9]|[1-9]\d+)$/i;
      let regPort = /^p\s\d{4,5}$/i;

      let check = true;
      if (regProtocol.test(tmp[0])) {
        let flag = 0;
        for (let i = 1; i < tmp.length; i++) {
          // eslint-disable-line
          // 验证 -h
          if (regHost && regHost.test(tmp[i])) {
            flag++; // eslint-disable-line
            // 提取参数
            this.servantDetailModal.model.node_name = tmp[i].split(/\s/)[1];
            regHost = null;
          }
          // 验证 -t
          if (regT && regT.test(tmp[i])) {
            flag++; // eslint-disable-line
            regT = null;
          }
          // 验证 -p
          if (regPort && regPort.test(tmp[i])) {
            const port = tmp[i].substring(2);
            if (!(port < 0 || port > 65535)) {
              flag++; // eslint-disable-line
            }
            regPort = null;
          }
        }
        check = flag === 3;
      } else {
        check = false;
      }
      return check;
    },
    // 保存 servant
    saveServantDetail() {
      if (this.$refs.servantDetailForm.validate()) {
        const loading = this.$Loading.show();
        // 新建
        if (this.servantDetailModal.isNew) {
          const query = this.servantDetailModal.model;
          query.servant = [
            query.application,
            query.server_name,
            query.obj_name,
          ].join(".");
          this.$tars
            .postJSON("/server/api/add_adapter_conf", query)
            .then((res) => {
              loading.hide();
              this.servantModal.model.unshift(res);
              this.$tip.success(this.$t("common.success"));
              this.closeServantDetailModal();
            })
            .catch((err) => {
              loading.hide();
              this.$tip.error(
                `${this.$t("common.error")}: ${err.err_msg || err.message}`
              );
            });
          // 修改
        } else {
          this.servantDetailModal.model.servant =
            this.servantDetailModal.model.application +
            "." +
            this.servantDetailModal.model.server_name +
            "." +
            this.servantDetailModal.model.obj_name;
          this.$tars
            .postJSON(
              "/server/api/update_adapter_conf",
              this.servantDetailModal.model
            )
            .then((res) => {
              loading.hide();
              this.servantModal.model = this.servantModal.model.map((item) => {
                if (item.id === res.id) {
                  return res;
                }
                return item;
              });
              this.$tip.success(this.$t("common.success"));
              this.closeServantDetailModal();
            })
            .catch((err) => {
              loading.hide();
              this.$tip.error(
                `${this.$t("common.error")}: ${err.err_msg || err.message}`
              );
            });
        }
      }
    },
    // 删除 servant
    deleteServant(id) {
      this.$confirm(
        this.$t("serverList.servant.a"),
        this.$t("common.alert")
      ).then(() => {
        const loading = this.$Loading.show();
        this.$tars
          .getJSON("/server/api/delete_adapter_conf", {
            id,
          })
          .then((res) => {
            loading.hide();
            this.servantModal.model = this.servantModal.model
              .map((item) => {
                // eslint-disable-line
                if (item.id !== id) return item;
              })
              .filter((item) => item);
            this.$tip.success(this.$t("common.success"));
          })
          .catch((err) => {
            loading.hide();
            this.$tip.error(
              `${this.$t("common.error")}: ${err.err_msg || err.message}`
            );
          });
      });
    },

    // 显示更多命令
    showMoreCmd(server) {
      this.moreCmdModal.model = {
        selected: "setloglevel",
        setloglevel: "NONE",
        loadconfig: "",
        command: "",
        configs: null,
      };
      this.moreCmdModal.unwatch = this.$watch(
        "moreCmdModal.model.selected",
        () => {
          if (this.$refs.moreCmdForm) this.$refs.moreCmdForm.resetValid();
        }
      );
      this.moreCmdModal.show = true;
      this.moreCmdModal.currentServer = server;

      this.$tars
        .getJSON("/server/api/config_file_list", {
          level: 5,
          application: server.application,
          server_name: server.server_name,
        })
        .then((data) => {
          if (this.moreCmdModal.model) this.moreCmdModal.model.configs = data;
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.err_msg || err.message}`
          );
        });
    },
    showStatus(id) {
      this.sendCommand(id, "status", true);
    },
    sendCommand(id, command, hold) {
      const loading = this.$Loading.show();
      this.$tars
        .getJSON("/server/api/send_command", {
          server_ids: id,
          command,
        })
        .then((res) => {
          loading.hide();
          const msg = res[0].err_msg.replace(/\n/g, "<br>");
          if (res[0].ret_code === 0) {
            const opt = {
              title: this.$t("common.success"),
              message: msg,
            };
            if (hold) opt.duration = 0;
            this.$tip.success(opt);
          } else {
            throw new Error(msg);
          }
        })
        .catch((err) => {
          loading.hide();
          this.$tip.error({
            title: this.$t("common.error"),
            message: err.err_msg || err.message,
          });
        });
    },
    invokeMoreCmd() {
      const model = this.moreCmdModal.model;
      const server = this.moreCmdModal.currentServer;
      // 下线服务
      if (model.selected === "undeploy_tars") {
        this.undeployServer(server);
        // 设置日志等级
      } else if (model.selected === "setloglevel") {
        this.sendCommand(server.id, `tars.setloglevel ${model.setloglevel}`);
        // push 日志文件
      } else if (
        model.selected === "loadconfig" &&
        this.$refs.moreCmdForm.validate()
      ) {
        this.sendCommand(server.id, `tars.loadconfig ${model.loadconfig}`);
        // 发送自定义命令
      } else if (
        model.selected === "command" &&
        this.$refs.moreCmdForm.validate()
      ) {
        this.sendCommand(server.id, model.command);
        // 查看服务链接
      } else if (model.selected === "connection") {
        this.sendCommand(server.id, `tars.connection`, true);
      }
    },
    closeMoreCmdModal() {
      if (this.$refs.moreCmdForm) this.$refs.moreCmdForm.resetValid();
      if (this.moreCmdModal.unwatch) this.moreCmdModal.unwatch();
      this.moreCmdModal.show = false;
      this.moreCmdModal.model = null;
    },

    // 处理未发布时间显示
    handleNoPublishedTime(timeStr, noPubTip = this.$t("pub.dlg.unpublished")) {
      if (timeStr === "0000:00:00 00:00:00") {
        return noPubTip;
      }
      return timeStr;
    },

    openExpandModal(rowData) {
      const { server_name, node_name } = rowData;
      this.expandModal.show = true;
      this.expandModal.model.server_name = server_name;
      this.expandModal.model.node_name = node_name;
    },
    async expandServerPreview({
      application = "",
      server_name = "",
      set = "",
      node_name = "",
      expand_nodes = [""],
      enable_set = false,
      set_name = "",
      set_area = "",
      set_group = "",
      copy_node_config = false,
    }) {
      return await this.$tars.postJSON("/server/api/expand_server_preview", {
        application,
        server_name,
        set,
        node_name,
        expand_nodes,
        enable_set,
        set_name,
        set_area,
        set_group,
        copy_node_config,
      });
    },
    async autoPort({ node_name }) {
      return await this.$tars.getJSON("/server/api/auto_port", {
        node_name,
      });
    },
    async expandServer({
      application = "",
      server_name = "",
      set = "",
      node_name = "",
      copy_node_config = false,
      expand_preview_servers = [
        {
          bind_ip: "",
          node_name: "",
          obj_name: "",
          port: "",
          set: "",
        },
      ],
    }) {
      return await this.$tars.postJSON("/server/api/expand_server", {
        application,
        server_name,
        set,
        node_name,
        copy_node_config,
        expand_preview_servers,
      });
    },
    async addTaskOptions({
      serial = false,
      elegant = false,
      eachnum = 1,
      items = [
        {
          server_id: "",
          command: "patch_tars",
          parameters: {
            patch_id: 0,
            bak_flag: "",
            group_name: "",
          },
        },
      ],
    }) {
      return await this.$tars.postJSON("/server/api/add_task", {
        serial,
        elegant,
        eachnum,
        items,
      });
    },
    async expandService() {
      if (this.$refs.expandForm.validate()) {
        const model = Object.assign({}, this.expandModal.model);
        model.expand_nodes = this.expandModal.expandIpStr
          .trim()
          .split(/[,;\n]/);
        const loading = this.$Loading.show();

        try {
          const data = await this.expandServerPreview(model);
          if (!data)
            throw new Error(this.$t("serviceExpand.form.errTips.noneNodes"));
          const previewItems = data.filter(
            (item) => item.status === this.$t("serviceExpand.form.noExpand")
          );
          const previewServers = [];
          const bindIps = [];
          previewItems.forEach((item) => {
            bindIps.push(item.bind_ip);
          });
          const portData = await this.autoPort({
            node_name: bindIps.join(";"),
          });
          portData.forEach((node, index) => {
            this.$set(previewItems[index], "port", String(node.port || ""));
          });
          previewItems.forEach((item) => {
            previewServers.push({
              bind_ip: item.bind_ip,
              node_name: item.node_name,
              obj_name: item.obj_name,
              port: item.port,
              set: item.set,
            });
          });
          const params = {
            application: model.application,
            server_name: model.server_name,
            set: model.set,
            node_name: model.node_name,
            copy_node_config: model.copy_node_config,
            expand_preview_servers: previewServers,
          };
          const { server_conf } = await this.expandServer(params);
          const addTaskOption = {
            serial: false,
            items: [],
          };
          let { serverType } = this;
          let group_name = "";
          if (serverType === "router") {
            group_name = "RouterServer";
          } else if (serverType === "proxy") {
            group_name = "ProxyServer";
          } else if (serverType === "dcache") {
            group_name = "DCacheServerGroup";
          }
          server_conf.forEach((item) => {
            addTaskOption.items.push({
              server_id: item.id.toString(),
              command: "patch_tars",
              parameters: {
                patch_id: item.patch_version,
                bak_flag: item.bak_flag,
                update_text: "",
                group_name,
              },
            });
          });
          const addTaskRsp = await this.addTaskOptions(addTaskOption);
          this.$tip.success(this.$t("serviceExpand.form.errTips.success"));
          this.closeExpandModal();
          this.getServerList();
          this.getServerNotifyList(1);
        } catch (err) {
          console.error(err);
          this.$tip.error(err);
        } finally {
          loading.hide();
        }
      }
    },
    closeExpandModal() {
      if (this.$refs.expandForm) this.$refs.expandForm.resetValid();
      this.expandModal.show = false;
      this.expandModal.model = getInitialExpandModel();
      this.expandModal.expandIpStr = "";
    },
  },
  created() {
    this.serverData = this.$parent.getServerData();
    window.manage = this;
  },
  mounted() {
    this.getServerList();
    this.getServerNotifyList(1);
  },
};
</script>

<style lang="postcss">
@import "../../assets/css/variable.css";

.page_server_manage {
  .tbm16 {
    margin: 16px 0;
  }
  .danger {
    color: var(--off-color);
  }

  /* .icon.iconfont {
    font-size: 10px;
    cursor: pointer;
    vertical-align: 0em;
  } */

  .more-cmd {
    .let-form-item__content {
      display: flex;
      align-items: center;
    }
    span.let-radio {
      margin-right: 5px;
    }
    label.let-radio {
      width: 200px;
    }
  }

  .table_head {
    padding: 10px 0;
  }
  .dcache .let-table__operations {
    position: absolute;
    right: -15px;
    top: -40px;
  }
}
</style>
