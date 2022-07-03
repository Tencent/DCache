<template>
  <div class="page_server_config">
    <div style="margin-bottom:5px">
      <h4>
        {{ $t("cfg.title.a") }}
        <i class="el-icon-refresh-right" @click="getConfigList(serverData)"></i>

        <let-button
          size="mini"
          theme="primary"
          style="float:right"
          @click="addConfig"
          >{{ $t("cfg.btn.add") }}</let-button
        >
      </h4>
    </div>

    <!-- 服务列表 -->
    <wrapper ref="configListLoading">
      <let-table :data="configList" :empty-text="$t('common.nodata')">
        <let-table-column width="40px">
          <template slot-scope="scope">
            <let-radio v-model="checkedConfigId" :label="scope.row.id"
              ><span>&nbsp;</span></let-radio
            >
          </template>
        </let-table-column>
        <let-table-column
          :title="$t('serverList.table.th.service')"
          prop="server_name"
        ></let-table-column>
        <let-table-column
          :title="$t('cfg.btn.fileName')"
          prop="filename"
        ></let-table-column>
        <let-table-column
          :title="$t('serverList.table.th.lastUser')"
          prop="lastuser"
        ></let-table-column>
        <let-table-column
          :title="$t('cfg.btn.lastUpdate')"
          prop="posttime"
        ></let-table-column>
        <let-table-column :title="$t('operate.operates')" width="260px">
          <template slot-scope="scope">
            <let-table-operation
              @click="changeConfig(scope.row, 'configList')"
              >{{ $t("operate.update") }}</let-table-operation
            >
            <let-table-operation @click="deleteConfig(scope.row.id)">{{
              $t("operate.delete")
            }}</let-table-operation>
            <let-table-operation @click="showDetail(scope.row)">{{
              $t("cfg.title.viewConf")
            }}</let-table-operation>
            <let-table-operation @click="showHistory(scope.row.id)">{{
              $t("pub.btn.history")
            }}</let-table-operation>
          </template>
        </let-table-column>
      </let-table>
    </wrapper>

    <div style="margin-bottom:5px" v-if="refFileList && showOthers">
      <h4>
        {{ $t("cfg.title.b") }}
        <let-button
          size="mini"
          theme="primary"
          style="float:right"
          @click="openRefFileModal"
          >{{ $t("cfg.btn.addRef") }}</let-button
        >
      </h4>
    </div>

    <!-- 引用文件列表 -->
    <wrapper v-if="refFileList && showOthers" ref="refFileListLoading">
      <let-table :data="refFileList" :empty-msg="$t('common.nodata')">
        <let-table-column
          :title="$t('serverList.table.th.service')"
          prop="server_name"
        ></let-table-column>
        <let-table-column
          :title="$t('cfg.btn.fileName')"
          prop="filename"
        ></let-table-column>
        <let-table-column
          :title="$t('serverList.table.th.ip')"
          prop="node_name"
        ></let-table-column>
        <let-table-column
          :title="$t('cfg.btn.lastUpdate')"
          prop="posttime"
        ></let-table-column>
        <let-table-column :title="$t('operate.operates')" width="260px">
          <template slot-scope="scope">
            <let-table-operation @click="deleteRef(scope.row.id)">{{
              $t("operate.delete")
            }}</let-table-operation>
            <let-table-operation @click="showDetail(scope.row)">{{
              $t("operate.view")
            }}</let-table-operation>
          </template>
        </let-table-column>
      </let-table>
    </wrapper>

    <div style="margin-bottom:5px" v-if="nodeConfigList && showOthers">
      <h4>
        {{ $t("cfg.title.c") }}
        <let-button
          size="mini"
          theme="primary"
          style="float:right"
          @click="pushNodeConfig"
          >{{ $t("cfg.btn.pushFile") }}</let-button
        >
      </h4>
    </div>

    <!-- 节点配置列表 -->
    <wrapper v-if="nodeConfigList && showOthers" ref="nodeConfigListLoading">
      <let-checkbox
        class="check-all"
        v-model="nodeCheckAll"
        v-if="nodeConfigList.length"
      ></let-checkbox>

      <let-table :data="nodeConfigList" :empty-msg="$t('common.nodata')">
        <let-table-column width="40px">
          <template slot-scope="scope">
            <let-checkbox
              v-model="nodeCheckList"
              :label="scope.row.id"
            ></let-checkbox>
          </template>
        </let-table-column>
        <let-table-column
          :title="$t('serverList.table.th.service')"
          prop="server_name"
        ></let-table-column>
        <let-table-column
          :title="$t('serverList.table.th.ip')"
          prop="node_name"
        ></let-table-column>
        <let-table-column
          :title="$t('cfg.btn.fileName')"
          prop="filename"
        ></let-table-column>
        <let-table-column
          :title="$t('cfg.btn.lastUpdate')"
          prop="posttime"
        ></let-table-column>
        <let-table-column :title="$t('operate.operates')" width="400px">
          <template slot-scope="scope">
            <let-table-operation
              @click="changeConfig(scope.row, 'nodeConfigList')"
              >{{ $t("cfg.table.modCfg") }}</let-table-operation
            >
            <let-table-operation @click="showMergedDetail(scope.row.id)">{{
              $t("cfg.table.viewMerge")
            }}</let-table-operation>
            <let-table-operation @click="showDetail(scope.row)">{{
              $t("cfg.table.viewIpContent")
            }}</let-table-operation>
            <let-table-operation @click="showHistory(scope.row.id)">{{
              $t("pub.btn.history")
            }}</let-table-operation>
            <let-table-operation @click="handleRefFiles(scope.row.id)">{{
              $t("cfg.table.mangeRefFile")
            }}</let-table-operation>
          </template>
        </let-table-column>
      </let-table>
    </wrapper>

    <!-- 添加、修改配置弹窗 -->
    <let-modal
      v-model="configModal.show"
      :title="
        configModal.isNew
          ? `${$t('operate.add')} ${$t('common.config')}`
          : `${$t('operate.update')} ${$t('common.config')}`
      "
      width="80%"
      @on-confirm="configDiff"
      @close="closeConfigModal"
      @on-cancel="closeConfigModal"
    >
      <let-form v-if="configModal.model" ref="configForm" itemWidth="100%">
        <let-form-item :label="$t('cfg.btn.fileName')" required>
          <let-input
            size="small"
            :disabled="!configModal.isNew"
            v-model="configModal.model.filename"
            required
          ></let-input>
        </let-form-item>
        <let-form-item
          :label="$t('cfg.btn.reason')"
          v-if="!configModal.isNew"
          required
        >
          <let-input
            size="small"
            v-model="configModal.model.reason"
            required
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('cfg.btn.content')" required>
          <let-input
            size="large"
            type="textarea"
            :rows="20"
            v-model="configContent"
            required
          ></let-input>
        </let-form-item>
      </let-form>
    </let-modal>
    <!-- 配置对比弹窗 -->
    <let-modal
      v-model="configDiffModal.show"
      :title="$t('filter.title.diffTxt')"
      width="1200px"
      @on-confirm="updateConfigDiff"
      @close="closeConfigDiffModal"
      @on-cancel="closeConfigDiffModal"
    >
      <div v-if="configDiffModal.model" style="padding-top:30px;">
        <div class="codediff_head">
          <div class="codediff_th">旧内容:</div>
          <div class="codediff_th">新内容:</div>
        </div>
        <diff
          :old-string="configDiffModal.model.oldData"
          :new-string="configDiffModal.model.newData"
          :context="10"
          output-format="side-by-side"
        ></diff>
      </div>
    </let-modal>

    <!-- 查看弹窗 -->
    <let-modal
      v-model="detailModal.show"
      :title="detailModal.title"
      width="80%"
      :footShow="false"
      @close="closeDetailModal"
    >
      <let-table
        class="history-table"
        v-if="detailModal.model && detailModal.model.table"
        :data="detailModal.model.table"
        :empty-msg="$t('common.nodata')"
      >
        <let-table-column
          :title="$t('common.time')"
          prop="posttime"
        ></let-table-column>
        <let-table-column
          :title="$t('cfg.btn.reason')"
          prop="reason"
        ></let-table-column>
        <let-table-column :title="$t('cfg.btn.content')" width="90px">
          <template slot-scope="scope">
            <let-table-operation @click="showTableDeatil(scope.row)">{{
              $t("operate.view")
            }}</let-table-operation>
          </template>
        </let-table-column>
      </let-table>
      <div class="pre_con">
        <pre
          v-if="
            (detailModal.model && !detailModal.model.table) ||
              (detailModal.model &&
                detailModal.model.table &&
                detailModal.model.detail)
          "
          >{{ detailModal.model.detail || $t("cfg.msg.empty") }}</pre
        >
      </div>
      <div class="detail-loading" ref="detailModalLoading"></div>
    </let-modal>

    <!-- 添加引用文件弹窗 -->
    <let-modal
      v-if="refFileModal.model"
      v-model="refFileModal.show"
      :title="this.$t('operate.add')"
      width="80%"
      @on-confirm="addRefFile"
      @close="closeRefFileModal"
    >
      <let-form itemWidth="100%" ref="refForm">
        <let-form-item :label="$t('cfg.msg.refFile')" required>
          <let-select
            size="small"
            :placeholder="$t('pub.dlg.defaultValue')"
            v-model="refFileModal.model.filename"
            required
          >
            <let-option
              v-for="item in refFileModal.model.fileList"
              :key="item.id"
              :value="item.id"
              >{{ item.filename }}</let-option
            >
          </let-select>
        </let-form-item>
      </let-form>
    </let-modal>

    <!-- 节点配置列表的管理引用文件弹窗 -->
    <let-modal
      v-model="nodeRefFileListModal.show"
      :title="$t('cfg.table.mangeRefFile')"
      width="700"
      :footShow="false"
      @close="closeDetailModal"
    >
      <wrapper v-if="nodeRefFileListModal.model">
        <let-button
          size="small"
          theme="primary"
          class="add-btn"
          @click="openNodeRefFileModal"
          >{{ $t("cfg.btn.addRef") }}</let-button
        >

        <let-table
          :data="nodeRefFileListModal.model.refFileList"
          :title="$t('cfg.title.b')"
          :empty-msg="$t('common.nodata')"
        >
          <let-table-column
            :title="$t('serverList.table.th.service')"
            prop="server_name"
          ></let-table-column>
          <let-table-column
            :title="$t('cfg.btn.fileName')"
            prop="filename"
          ></let-table-column>
          <let-table-column
            :title="$t('serverList.table.th.ip')"
            prop="node_name"
          ></let-table-column>
          <let-table-column
            :title="$t('cfg.btn.lastUpdate')"
            prop="posttime"
          ></let-table-column>
          <let-table-column :title="$t('operate.operates')" width="260px">
            <template slot-scope="scope">
              <let-table-operation
                @click="deleteRef(scope.row.id, 'nodeRef', scope.row.config_id)"
                >{{ $t("operate.delete") }}</let-table-operation
              >
              <let-table-operation @click="showDetail(scope.row)">{{
                $t("operate.view")
              }}</let-table-operation>
              <let-table-operation @click="showHistory(scope.row.config_id)">{{
                $t("pub.btn.history")
              }}</let-table-operation>
            </template>
          </let-table-column>
        </let-table>
      </wrapper>
    </let-modal>

    <!-- 推送结果弹窗 -->
    <let-modal
      v-model="pushResultModal.show"
      width="700px"
      :footShow="false"
      @close="closePushResultModal"
    >
      <let-table
        v-if="pushResultModal.model"
        :data="pushResultModal.model"
        :title="$t('serverList.table.th.result')"
        :empty-msg="$t('common.nodata')"
      >
        <let-table-column :title="$t('serverList.table.th.service')">
          <template slot-scope="scope">
            <span :class="scope.row.ret_code !== 0 ? 'danger' : 'success'">{{
              scope.row.server_name
            }}</span>
          </template>
        </let-table-column>
        <let-table-column :title="$t('serverList.table.th.result')">
          <template slot-scope="scope">
            <span
              class="result"
              :class="scope.row.ret_code !== 0 ? 'danger' : 'success'"
              >{{ scope.row.err_msg }}</span
            >
          </template>
        </let-table-column>
      </let-table>
    </let-modal>
  </div>
</template>

<script>
import wrapper from "@/components/section-wrappper";
import diff from "@/components/diff/index";

export default {
  name: "ServerConfig",
  components: {
    wrapper,
    diff,
  },
  data() {
    return {
      oldStr: "old code",
      newStr: "new code",
      // 当前页面信息
      serverData: {
        level: 5,
        application: "",
        server_name: "",
        set_name: "",
        set_area: "",
        set_group: "",
      },

      // 服务列表
      checkedConfigId: "",
      configList: [],

      // 引用文件列表
      refFileList: [],

      // 节点配置列表
      nodeConfigList: null,
      nodeCheckList: [],

      // 添加、修改配置弹窗
      configContent: "",
      configModal: {
        show: false,
        isNew: true,
        model: null,
      },
      configDiffModal: {
        type: "",
        show: false,
        isNew: true,
        model: null,
      },

      // 查看弹窗
      detailModal: {
        show: false,
        title: "",
        model: null,
      },

      // 引用文件弹窗
      refFileModal: {
        show: false,
        model: { fileList: [] },
      },

      // 节点配置列表的管理引用文件弹窗
      nodeRefFileListModal: {
        show: false,
        model: null,
      },

      // 节点配置列表的管理引用文件弹窗
      pushResultModal: {
        show: false,
        model: null,
      },
    };
  },
  computed: {
    showOthers() {
      return this.serverData.level === 5;
    },
    nodeCheckAll: {
      get() {
        return this.nodeConfigList && this.nodeConfigList.length
          ? this.nodeCheckList.length === this.nodeConfigList.length
          : false;
      },
      set(value) {
        if (value) {
          this.nodeCheckList = this.nodeConfigList.map((item) => item.id);
        } else {
          this.nodeCheckList = [];
        }
      },
    },
  },
  watch: {
    checkedConfigId() {
      this.$nextTick(() => {
        this.getRefFileList();
        this.getNodeConfigList();
      });
    },
  },
  methods: {
    // 配置列表
    getConfigList(query) {
      const loading = this.$refs.configListLoading.$loading.show();

      this.$tars
        .getJSON("/server/api/config_file_list", query)
        .then((data) => {
          loading.hide();
          this.configList = data;
          this.refFileList = [];
          this.nodeConfigList = [];
          if (data[0] && data[0].id) this.checkedConfigId = data[0].id;
        })
        .catch((err) => {
          loading.hide();
          this.$confirm(
            err.err_msg || err.message || this.$t("common.error"),
            this.$t("common.retry"),
            this.$t("common.alert")
          ).then(() => {
            this.getConfigList();
          });
        });
    },
    addConfig() {
      this.configModal.model = {
        filename: this.serverData.server_name + ".conf",
        config: "",
      };

      if (this.serverData.server_name == "") {
        this.configModal.model.filename = this.serverData.application + ".conf";
      }

      this.configModal.isNew = true;
      this.configModal.show = true;
    },
    changeConfig(config, array) {
      this.configModal.model = Object.assign(
        {
          reason: "",
        },
        config
      );
      this.configContent = config.config;
      this.configModal.target = array;
      this.configModal.isNew = false;
      this.configModal.show = true;
    },
    updateConfigFile() {
      if (this.$refs.configForm.validate()) {
        const loading = this.$Loading.show();
        this.configModal.model.filename = this.configModal.model.filename.replace(
          /(^\s*)|(\s*$)/g,
          ""
        );
        const model = this.configModal.model;
        // 新增
        if (this.configModal.isNew) {
          const query = Object.assign(
            {
              application: this.serverData.application,
              level: this.serverData.level,
              server_name: this.serverData.server_name,
              set_name: this.serverData.set_name,
              set_area: this.serverData.set_area,
              set_group: this.serverData.set_group,
            },
            model
          );
          this.$tars
            .postJSON("/server/api/add_config_file", query)
            .then((res) => {
              loading.hide();
              this.configList.unshift(res);
              if (this.configList.length === 1) {
                this.checkedConfigId = res.id;
              }
              this.$tip.success(this.$t("common.success"));
              this.closeConfigModal();
              this.closeConfigDiffModal();
            })
            .catch((err) => {
              loading.hide();
              this.$tip.error(
                `${this.$t("common.error")}: ${err.err_msg || err.message}`
              );
            });
          // 修改
        } else {
          this.$tars
            .postJSON("/server/api/update_config_file", {
              config: model.config,
              id: model.id,
              reason: model.reason,
            })
            .then((res) => {
              loading.hide();
              this[this.configModal.target] = this[this.configModal.target].map(
                (item) => {
                  if (item.id === res.id) {
                    return res;
                  }
                  return item;
                }
              );
              if (this.checkedConfigId === res.id) {
                this.getRefFileList();
                this.getNodeConfigList();
              }
              this.$tip.success(this.$t("common.success"));
              this.closeConfigModal();
              this.closeConfigDiffModal();
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
    closeConfigModal() {
      if (this.$refs.configForm) this.$refs.configForm.resetValid();
      this.configModal.show = false;
    },
    deleteConfig(id) {
      this.$confirm(
        this.$t("cfg.msg.confirmCfg"),
        this.$t("common.alert")
      ).then(() => {
        const loading = this.$Loading.show();
        this.$tars
          .getJSON("/server/api/delete_config_file", {
            id,
          })
          .then((res) => {
            loading.hide();
            this.getConfigList(this.serverData);
            this.getNodeConfigList();
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

    // 引用文件列表
    getUnusedFileList(id) {
      if (!this.showOthers) return;
      this.$tars
        .getJSON("/server/api/unused_config_file_list", {
          config_id: id,
          application: this.serverData.application,
        })
        .then((data) => {
          this.refFileModal.model.fileList = data;
        })
        .catch((err) => {
          this.refFileModal.model.fileList = [];
          this.$tip.error({
            title: this.$t("common.error"),
            message: err.err_msg || err.message || this.$t("common.networkErr"),
          });
        });
    },
    getRefFileList() {
      if (!this.showOthers) return;
      const loading = this.$refs.refFileListLoading.$loading.show();
      this.$tars
        .getJSON("/server/api/config_ref_list", {
          config_id: this.checkedConfigId,
        })
        .then((data) => {
          loading.hide();
          data.map((item) => {
            let id = item.id;
            item = Object.assign(item, item.reference);
            item.refrence_id = item.id;
            item.id = id;
            return item;
          });
          this.refFileList = data;
        })
        .catch((err) => {
          loading.hide();
          this.refFileList = [];
          this.$tip.error({
            title: this.$t("common.error"),
            message: err.err_msg || err.message || this.$t("common.networkErr"),
          });
        });
    },
    openRefFileModal() {
      this.refFileModal.show = true;
      this.refFileModal.isNodeRef = false;
      this.getUnusedFileList(this.checkedConfigId);
    },
    openNodeRefFileModal() {
      this.refFileModal.show = true;
      this.refFileModal.isNodeRef = true;
      this.getUnusedFileList(this.refFileModal.id);
    },
    addRefFile() {
      if (this.$refs.refForm.validate()) {
        const loading = this.$Loading.show();
        this.$tars
          .getJSON("/server/api/add_config_ref", {
            config_id: this.refFileModal.isNodeRef
              ? this.refFileModal.id
              : this.checkedConfigId,
            reference_id: this.refFileModal.model.filename,
          })
          .then((data) => {
            loading.hide();
            this.refFileModal.show = false;
            this.refFileModal.isNodeRef
              ? this.getNodeRefFileList(this.refFileModal.id)
              : this.getRefFileList();
          })
          .catch((err) => {
            loading.hide();

            this.$tip.error({
              title: this.$t("common.error"),
              message:
                err.err_msg || err.message || this.$t("common.networkErr"),
            });
          });
      }
    },
    closeRefFileModal() {
      this.refFileModal.show = false;
    },
    deleteRef(id, type, configId) {
      this.$confirm(this.$t("cfg.msg.confirm"), this.$t("common.alert")).then(
        () => {
          const loading = this.$Loading.show();
          this.$tars
            .getJSON("/server/api/delete_config_ref", {
              id,
            })
            .then((res) => {
              loading.hide();
              if (type == "nodeRef") {
                this.getNodeRefFileList(configId);
              } else {
                this.getRefFileList();
              }
              this.$tip.success(this.$t("common.success"));
            })
            .catch((err) => {
              loading.hide();

              this.$tip.error({
                title: this.$t("common.error"),
                message:
                  err.err_msg || err.message || this.$t("common.networkErr"),
              });
            });
        }
      );
    },

    // 节点配置列表
    getNodeConfigList() {
      if (!this.showOthers) return;
      const loading = this.$refs.nodeConfigListLoading.$loading.show();

      const query = Object.assign(
        {
          config_id: this.checkedConfigId,
        },
        this.serverData
      );
      this.$tars
        .getJSON("/server/api/node_config_file_list", query)
        .then((data) => {
          loading.hide();
          this.nodeConfigList = data;
        })
        .catch((err) => {
          loading.hide();
          this.nodeConfigList = [];
          this.$tip.error({
            title: this.$t("common.error"),
            message: err.err_msg || err.message || this.$t("common.networkErr"),
          });
        });
    },
    pushNodeConfig() {
      if (!this.nodeCheckList.length) {
        this.$tip.warning(this.$t("cfg.msg.selectNode"));
        return;
      }
      const loading = this.$Loading.show();
      this.$tars
        .getJSON("/server/api/push_config_file", {
          ids: this.nodeCheckList.join(";"),
        })
        .then((res) => {
          loading.hide();
          this.pushResultModal.model = res;
          this.pushResultModal.show = true;
        })
        .catch((err) => {
          loading.hide();
          this.$tip.error(
            `${this.$t("common.error")}: ${err.err_msg || err.message}`
          );
        });
    },
    closePushResultModal() {
      this.pushResultModal.model = null;
      this.pushResultModal.show = false;
    },

    // 显示详情弹窗
    showDetail(item) {
      this.detailModal.title = this.$t("cfg.title.viewConf");
      this.detailModal.model = {
        detail: item.config,
      };
      this.detailModal.show = true;
    },
    showMergedDetail(id) {
      this.detailModal.title = this.$t("cfg.title.viewMerged");
      this.detailModal.show = true;
      const loading = this.$loading.show({
        target: this.$refs.detailModalLoading,
      });

      this.$tars
        .getJSON("/server/api/merged_node_config", {
          id,
        })
        .then((data) => {
          loading.hide();
          this.detailModal.model = {
            detail: data,
          };
        })
        .catch((err) => {
          loading.hide();
          this.$tip.error(
            `${this.$t("common.error")}: ${err.err_msg || err.message}`
          );
        });
    },
    showHistory(id) {
      this.detailModal.title = this.$t("cfg.title.viewHistory");
      this.detailModal.show = true;
      const loading = this.$loading.show({
        target: this.$refs.detailModalLoading,
      });

      this.$tars
        .getJSON("/server/api/config_file_history_list", {
          config_id: id,
        })
        .then((data) => {
          loading.hide();
          this.detailModal.model = {
            table: data.rows,
            detail: "",
          };
        })
        .catch((err) => {
          loading.hide();
          this.$tip.error(
            `${this.$t("common.error")}: ${err.err_msg || err.message}`
          );
        });
    },
    showTableDeatil(item) {
      this.detailModal.model.detail = item.content;
    },
    closeDetailModal() {
      this.detailModal.show = false;
      this.detailModal.model = null;
    },
    getNodeRefFileList(id) {
      this.$tars
        .getJSON("/server/api/config_ref_list", {
          config_id: id,
        })
        .then((data) => {
          data.map((item) => {
            let id = item.id;
            item = Object.assign(item, item.reference);
            item.refrence_id = item.id;
            item.id = id;
            return item;
          });
          this.nodeRefFileListModal.model = { refFileList: data };
        })
        .catch((err) => {
          this.nodeRefFileListModal.model = { refFileList: [] };
          this.$tip.error({
            title: this.$t("common.error"),
            message: err.err_msg || err.message || this.$t("common.networkErr"),
          });
        });
    },
    // 节点管理配置文件
    handleRefFiles(id) {
      this.nodeRefFileListModal.show = true;
      this.nodeRefFileListModal.model = null;
      this.refFileModal.id = id;
      this.getNodeRefFileList(id);
    },
    configDiff() {
      if (this.$refs.configForm.validate()) {
        this.configDiffModal.type = "config";
        this.configDiffModal.show = true;
        this.configDiffModal.isNew = false;
        this.configDiffModal.model = {
          oldData: this.configModal.model.config,
          newData: this.configContent,
        };
      }
    },
    updateConfigDiff() {
      let { type } = this.configDiffModal;

      const model = this.configModal.model;

      if (model.filename.toLowerCase().endsWith(".json")) {
        //json格式, 检查配置文件格式
        try {
          JSON.parse(this.configContent);
        } catch (e) {
          alert("config format error:" + e.toString());
          return;
        }
      }

      if (model.filename.toLowerCase().endsWith(".xml")) {
        //json格式, 检查配置文件格式
        try {
          // (new DOMParser()).parseFromString(this.configContent,"text/xml");
          var parser = new DOMParser();
          var xmlDoc = parser.parseFromString(this.configContent, "text/xml");
          var error = xmlDoc.getElementsByTagName("parsererror");
          let errorMessage = "";
          if (error.length > 0) {
            if (xmlDoc.documentElement.nodeName == "parsererror") {
              errorMessage = xmlDoc.documentElement.childNodes[0].nodeValue;
            } else {
              errorMessage = xmlDoc.getElementsByTagName("parsererror")[0]
                .innerHTML;
            }

            alert("config format error:" + errorMessage);
            return;
          }
        } catch (e) {
          alert("config format error:" + e.toString());
          return;
        }
      }

      if (type === "config") {
        this.configModal.model.config = this.configContent;
        this.updateConfigFile();
      }
    },
    closeConfigDiffModal() {
      this.configDiffModal.show = false;
    },
    nodeConfigDiff() {
      if (this.$refs.nodeConfigForm.validate()) {
        this.configDiffModal.type = "nodeConfig";
        this.configDiffModal.show = true;
        this.configDiffModal.isNew = false;
        this.configDiffModal.model = {
          oldData: this.nodeConfigModal.model.config,
          newData: this.nodeConfigContent,
        };
      }
    },
  },
  created() {
    this.serverData = this.$parent.getServerData();
  },
  mounted() {
    this.getConfigList(this.serverData);
  },
};
</script>

<style>
@import "../../assets/css/variable.css";

.page_server_config {
  .add-btn {
    position: absolute;
    right: 0;
    top: 0;
    z-index: 2;
  }
  .check-all {
    position: absolute;
    z-index: 2;
    top: 60px;
    left: 16px;
  }
  .let-table caption {
    padding-bottom: 16px;
  }
  .danger {
    color: var(--off-color);
  }
  .success {
    color: var(--active-color);
  }
  .result {
    display: inline-block;
    max-width: 420px;
    word-break: break-word;
    padding-right: 10px;
  }

  .pre_con {
    display: block;
    overflow: hidden;
  }

  .pre_con pre {
    color: #909fa3;
    display: block;
    margin-top: 32px;
    word-break: break-word;
    white-space: pre-wrap;
  }

  .detail-loading {
    height: 28px;
    &:only-child {
      margin: 20px 0;
    }
  }
  .history-table {
    margin-top: 20px;
  }

  .let-checkbox {
    vertical-align: initial;
  }
  .btn_group {
    position: absolute;
    right: 0;
    top: 0;
    z-index: 2;
  }
  .btn_group .let-button + .let-button {
    margin-left: 10px;
  }
  .codediff_head {
    display: flex;
    flex: 1;
    margin-bottom: 5px;
  }
  .codediff_th {
    display: block;
    flex: 1;
  }
}
</style>
