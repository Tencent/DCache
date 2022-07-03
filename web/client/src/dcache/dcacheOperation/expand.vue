<template>
  <div class="page_operation_expand">
    <!-- 预扩容配置 -->
    <let-form
      ref="configForm"
      inline
      label-position="top"
      itemWidth="480px"
      @submit.native.prevent="previewExpand"
    >
      <let-form-item
        :label="$t('deployService.form.app')"
        itemWidth="240px"
        required
      >
        <let-select
          size="small"
          v-model="model.application"
          required
          :required-tip="$t('deployService.table.tips.empty')"
          @change="changeSelect('application')"
        >
          <let-option v-for="d in applications" :key="d" :value="d">
            {{ d }}
          </let-option>
        </let-select>
      </let-form-item>
      <let-form-item
        :label="$t('serverList.table.th.service')"
        itemWidth="240px"
        required
      >
        <let-select
          size="small"
          v-model="model.server_name"
          required
          :required-tip="$t('deployService.table.tips.empty')"
          @change="changeSelect('server_name')"
        >
          <let-option v-for="d in serverNames" :key="d" :value="d">{{
            d
          }}</let-option>
        </let-select>
      </let-form-item>

      <let-form-item label="Set" itemWidth="240px" required>
        <let-select
          size="small"
          v-model="model.set"
          required
          :required-tip="$t('deployService.table.tips.empty')"
          @change="changeSelect('set')"
        >
          <let-option v-for="d in sets" :key="d" :value="d ? d : -1">
            {{ d ? d : $t("serviceExpand.form.disableSet") }}
          </let-option>
        </let-select>
      </let-form-item>

      <let-form-item
        :label="$t('serverList.table.th.ip')"
        itemWidth="240px"
        required
      >
        <let-select
          size="small"
          v-model="model.node_name"
          filterable
          required
          :required-tip="$t('deployService.table.tips.empty')"
        >
          <let-option v-for="d in nodeNames" :key="d" :value="d">{{
            d
          }}</let-option>
        </let-select>
      </let-form-item>

      <let-form-item
        :label="$t('serviceExpand.form.tarIP')"
        itemWidth="100%"
        required
      >
        <let-input
          type="textarea"
          :rows="3"
          v-model="expandIpStr"
          :placeholder="$t('serviceExpand.form.placeholder')"
          required
          :required-tip="$t('deployService.table.tips.empty')"
        >
        </let-input>
      </let-form-item>

      <let-form-item :label="$t('serverList.table.th.enableSet')">
        <SetInputer
          :enabled.sync="model.enable_set"
          :name.sync="model.set_name"
          :area.sync="model.set_area"
          :group.sync="model.set_group"
        ></SetInputer>
      </let-form-item>

      <let-form-item
        :label="$t('serviceExpand.form.nodeConfig')"
        itemWidth="100%"
      >
        <let-checkbox v-model="model.enable_node_copy">
          {{ $t("serviceExpand.form.copyNodeConfig") }}
        </let-checkbox>
      </let-form-item>

      <let-button type="sumbit" theme="primary">{{
        $t("serviceExpand.form.preExpand")
      }}</let-button>
    </let-form>
    <!-- 预扩容列表 -->
    <let-form
      ref="expandForm"
      inline
      @submit.native.prevent="expand"
      class="mt40"
      v-show="previewItems.length > 0"
    >
      <let-table
        ref="table"
        :data="previewItems"
        :empty-msg="$t('common.nodata')"
      >
        <let-table-column>
          <template slot="head" slot-scope="props">
            <let-checkbox v-model="isCheckedAll"></let-checkbox>
          </template>
          <template slot-scope="scope">
            <let-checkbox
              v-model="scope.row.isChecked"
              v-if="scope.row.status == $t('serviceExpand.form.noExpand')"
            ></let-checkbox>
          </template>
        </let-table-column>
        <let-table-column
          :title="$t('historyList.dlg.th.c2')"
          prop="application"
        ></let-table-column>
        <let-table-column
          :title="$t('historyList.dlg.th.c3')"
          prop="server_name"
        ></let-table-column>
        <let-table-column title="Set" prop="set"></let-table-column>
        <let-table-column
          :title="$t('serverList.servant.objName')"
          prop="obj_name"
        ></let-table-column>
        <let-table-column
          :title="$t('historyList.dlg.th.c4')"
          prop="node_name"
        ></let-table-column>
        <let-table-column :title="$t('deployService.table.th.endpoint')">
          <template slot-scope="scope">
            <let-input
              v-model="scope.row.bind_ip"
              :pattern="scope.row.isChecked ? ipReg : null"
              :pattern-tip="$t('serviceExpand.form.preExpand')"
            ></let-input>
          </template>
        </let-table-column>
        <let-table-column :title="$t('deployService.table.th.port')">
          <template slot-scope="scope">
            <let-input v-model="scope.row.port"></let-input>
          </template>
        </let-table-column>
        <let-table-column
          :title="$t('deployService.form.template')"
          prop="template_name"
        ></let-table-column>
        <let-table-column
          :title="$t('historyList.dlg.th.c8')"
          prop="status"
        ></let-table-column>
      </let-table>

      <let-button type="button" theme="sub-primary" @click="getAutoPort()">{{
        $t("deployService.form.getPort")
      }}</let-button>
      <let-button type="sumbit" theme="primary">{{
        $t("deployService.title.expand")
      }}</let-button>
    </let-form>

    <let-modal
      v-model="resultModal.show"
      width="700px"
      class="more-cmd"
      :footShow="false"
      @close="closeResultModal"
      @on-cancel="closeResultModal"
    >
      <p class="result-text">
        {{ $t("serviceExpand.form.errTips.success")
        }}{{ $t("resource.installRstMsg") }}
      </p>
      <let-table
        :data="resultModal.resultList"
        :empty-msg="$t('common.nodata')"
        :row-class-name="resultModal.rowClassName"
      >
        <let-table-column title="ip" prop="ip"> </let-table-column>
        <let-table-column :title="$t('resource.installResult')" prop="rst">
          <template slot-scope="scope">
            <p
              v-text="scope.row.rst ? $t('common.success') : $t('common.error')"
            ></p>
          </template>
        </let-table-column>
        <let-table-column
          :title="$t('common.message')"
          prop="msg"
        ></let-table-column>
      </let-table>
    </let-modal>
  </div>
</template>

<script>
import SetInputer from "@/components/set-inputer";

const ipReg =
  "((\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])";
const getInitialModel = () => ({
  application: "",
  server_name: "",
  set: "",
  node_name: "",
  expand_nodes: [],
  enable_set: false,
  set_name: "",
  set_area: "",
  set_group: "",
  enable_node_copy: false,
});

export default {
  name: "OperationExpand",

  components: {
    SetInputer,
  },

  data() {
    return {
      model: getInitialModel(),
      applications: [],
      serverNames: [],
      sets: [],
      nodeNames: [],
      expandIpStr: "",
      previewItems: [],
      ipReg: `^${ipReg}$`,
      isCheckedAll: false,
      resultModal: {
        show: false,
        resultList: [],
        rowClassName: (rowData) => {
          if (rowData && rowData.row && !rowData.row.rst) {
            return "err-row";
          }
          return "";
        },
      },
    };
  },

  mounted() {
    this.getCascadeSelectServer(
      {
        level: 1,
      },
      this.$t("common.error")
    ).then((data) => {
      this.applications = data;
    });
  },

  methods: {
    changeSelect(attr) {
      switch (attr) {
        case "application":
          // 修改应用
          this.model.server_name = "";
          this.serverNames = [];
          if (this.model.application) {
            this.getCascadeSelectServer(
              {
                level: 2,
                application: this.model.application,
              },
              this.$t("common.error")
            ).then((data) => {
              this.serverNames = data;
            });
          }
          break;
        case "server_name":
          // 修改服务
          this.model.set = "";
          this.sets = [];
          if (this.model.server_name) {
            this.getCascadeSelectServer(
              {
                level: 3,
                application: this.model.application,
                server_name: this.model.server_name,
              },
              this.$t("common.error")
            ).then((data) => {
              this.sets = data;
            });
          }
          break;
        case "set": // 修改set
          this.model.node_name = "";
          this.model.nodeName = [];
          if (this.model.set) {
            const modelSet =
              parseInt(this.model.set, 10) === -1 ? "" : this.model.set;
            this.getCascadeSelectServer(
              {
                level: 4,
                application: this.model.application,
                server_name: this.model.server_name,
                set: modelSet,
              },
              this.$t("common.error")
            ).then((data) => {
              this.nodeNames = data;
            });
          }
          break;
        default:
          break;
      }
    },
    getCascadeSelectServer(params, prefix = this.$t("common.error")) {
      return this.$tars
        .getJSON("/server/api/cascade_select_server", params)
        .then((data) => data)
        .catch((err) => {
          this.$tip.error(`${prefix}: ${err.message || err.err_msg}`);
        });
    },
    previewExpand() {
      // 预扩容
      if (this.$refs.configForm.validate()) {
        const model = Object.assign({}, this.model);
        model.set = parseInt(model.set, 10) === -1 ? "" : model.set;
        model.expand_nodes = this.expandIpStr.trim().split(/[,;\n]/);

        const loading = this.$Loading.show();
        this.$tars
          .postJSON("/server/api/expand_server_preview", model)
          .then((data) => {
            loading.hide();
            const items = data || [];
            items.forEach((item) => {
              item.isChecked = false;
            });
            // hw-todo: let-ui bug
            this.isCheckedAll = false;
            this.previewItems = items;
          })
          .catch((err) => {
            loading.hide();
            this.$tip.error(
              `${this.$t("common.error")}: ${err.message || err.err_msg}`
            );
          });
      }
    },
    getAutoPort() {
      const loading = this.$Loading.show();
      var adapters = this.previewItems.filter(
        (item) =>
          item.status === this.$t("serviceExpand.form.noExpand") &&
          item.isChecked
      );
      var bindIps = [];
      adapters.forEach((adapter) => {
        bindIps.push(adapter.bind_ip);
      });
      this.$tars
        .getJSON("/server/api/auto_port", { node_name: bindIps.join(";") })
        .then((data) => {
          loading.hide();
          data.forEach((node, index) => {
            this.$set(adapters[index], "port", String(node.port || ""));
          });
        })
        .catch((err) => {
          loading.hide();
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    expand() {
      // 扩容
      if (this.$refs.expandForm.validate()) {
        const previewItems = this.previewItems.filter(
          (item) =>
            item.status === this.$t("serviceExpand.form.noExpand") &&
            item.isChecked
        );
        if (previewItems.length > 0) {
          const previewServers = [];
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
            application: this.model.application,
            server_name: this.model.server_name,
            set: parseInt(this.model.set, 10) === -1 ? "" : this.model.set,
            node_name: this.model.node_name,
            enable_node_copy: this.model.enable_node_copy,
            expand_preview_servers: previewServers,
          };

          const loading = this.$Loading.show();
          this.$tars
            .postJSON("/server/api/expand_server", params)
            .then((data) => {
              // hw-todo: 是否需要更新状态待定
              /*
            this.previewItems.forEach((item) => {
              if (item.status === '未扩容' && item.isChecked) {
                item.status = '已存在';
              }
            }); */
              loading.hide();
              if (data.tars_node_rst && data.tars_node_rst.length) {
                this.showResultModal(data.tars_node_rst);
              } else {
                this.$tip.success(
                  this.$t("serviceExpand.form.errTips.success")
                );
              }
            })
            .catch((err) => {
              loading.hide();
              this.$tip.error(
                `${this.$t("common.error")}: ${err.message || err.err_msg}`
              );
            });
        } else {
          this.$tip.error(this.$t("serviceExpand.form.errTips.noneNodes"));
        }
      }
    },

    showResultModal(data) {
      this.resultModal.resultList = data;
      this.resultModal.show = true;
    },

    closeResultModal() {
      this.resultModal.show = false;
      this.resultModal.resultList = [];
    },
  },
  watch: {
    isCheckedAll() {
      const isCheckedAll = this.isCheckedAll;
      this.previewItems.forEach((item) => {
        item.isChecked = isCheckedAll;
      });
    },
  },
};
</script>

<style>
.mt40 {
  margin-top: 40px;
}
tr.err-row td {
  background: #f56c77 !important;
  color: #fff;
}
.result-text {
  margin-top: 20px;
  margin-bottom: 10px;
}
</style>
