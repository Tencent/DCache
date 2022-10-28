<template>
  <section class="page_operation_create_service">
    <let-form label-position="top" ref="detailForm">
      <let-form-group
        :title="$t('apply.RouterConfigInfo')"
        inline
        label-position="top"
      >
        <let-form-item
          :label="$t('service.serverName')"
          itemWidth="240px"
          required
        >
          <let-input
            size="small"
            v-model="apply.Router.server_name"
            required
            :required-tip="$t('deployService.table.tips.empty')"
          >
          </let-input>
        </let-form-item>
        <let-form-item
          :label="$t('deployService.form.template')"
          itemWidth="240px"
          required
        >
          <let-select
            size="small"
            v-model="apply.Router.template_file"
            required
            :required-tip="$t('deployService.form.templateTips')"
          >
            <let-option v-for="d in templates" :key="d" :value="d">{{
              d
            }}</let-option>
          </let-select>
        </let-form-item>
        <let-form-item
          :label="$t('service.serverIp')"
          itemWidth="300px"
          required
        >
          <let-select
            v-model="apply.Router.server_ip"
            size="small"
            required
            placeholder="Please Choose"
          >
            <let-option v-for="d in nodeList" :key="d" :value="d">
              {{ d }}
            </let-option>
          </let-select>
        </let-form-item>

        <br />
        <span style="margin-left: 20px">{{
          $t("service.routerDbNameTip")
        }}</span
        ><br />
        <br />
        <let-form-item
          :label="$t('service.routerDbName')"
          itemWidth="240px"
          required
        >
          <let-input
            size="small"
            style="display: inline-block"
            v-model="apply.Router.router_db_name"
            required
            :required-tip="$t('deployService.table.tips.empty')"
          >
          </let-input>
        </let-form-item>
        <let-tag theme="danger" style="margin-left: 20px">{{
          $t("service.databaseTips")
        }}</let-tag>
        <br />
        <let-form-item
          :label="$t('service.routerDbName')"
          itemWidth="240px"
          required
        >
          <let-radio v-model="apply.dbMethod" :label="true">{{
            $t("service.chooseRouterDb")
          }}</let-radio>
        </let-form-item>
        <let-form-item
          :label="$t('service.routerDbName')"
          itemWidth="240px"
          v-if="apply.dbMethod"
          required
        >
          <let-select v-model="apply.routerDbId" size="small" required>
            <let-option v-for="d in routerDb" :key="d.id" :value="d.id">
              {{ d.router_db_flag }}
            </let-option>
          </let-select>
        </let-form-item>

        <br />
        <let-form-item
          :label="$t('service.routerDbName')"
          itemWidth="240px"
          required
        >
          <let-radio v-model="apply.dbMethod" :label="false">{{
            $t("service.inputRouterDb")
          }}</let-radio>
        </let-form-item>

        <span v-if="!apply.dbMethod">
          <let-form-item
            :label="$t('service.routerDbIp')"
            itemWidth="240px"
            required
          >
            <let-input
              size="small"
              v-model="apply.Router.router_db_ip"
              required
              :required-tip="$t('deployService.table.tips.empty')"
            >
            </let-input>
          </let-form-item>
          <let-form-item
            :label="$t('service.routerDbPort')"
            itemWidth="240px"
            required
          >
            <let-input
              size="small"
              v-model="apply.Router.router_db_port"
              required
              :required-tip="$t('deployService.table.tips.empty')"
            >
            </let-input>
          </let-form-item>
          <let-form-item
            :label="$t('service.routerDbUser')"
            itemWidth="240px"
            required
          >
            <let-input
              size="small"
              v-model="apply.Router.router_db_user"
              required
              :required-tip="$t('deployService.table.tips.empty')"
            >
            </let-input>
          </let-form-item>
          <let-form-item
            :label="$t('service.routerDbPass')"
            itemWidth="240px"
            required
          >
            <let-input
              size="small"
              v-model="apply.Router.router_db_pass"
              required
              :required-tip="$t('deployService.table.tips.empty')"
            >
            </let-input>
          </let-form-item>
        </span>
      </let-form-group>
      <let-form-group
        :title="$t('apply.ProxyConfigInfo')"
        inline
        label-position="top"
      >
        <let-table
          ref="table"
          :data="apply.Proxy"
          :empty-msg="$t('common.nodata')"
        >
          <let-table-column
            :title="$t('service.serverName')"
            prop="server_name"
            width="25%"
          >
            <template slot-scope="scope">
              <let-input
                size="small"
                v-model="scope.row.server_name"
                required
                :required-tip="$t('deployService.table.tips.empty')"
              >
              </let-input>
            </template>
          </let-table-column>
          <let-table-column
            :title="$t('service.multipleIp')"
            prop="server_ip"
            width="40%"
            required
          >
            <template slot-scope="scope">
              <let-select
                v-model="scope.row.server_ip"
                size="small"
                required
                multiple
                placeholder="Please Choose"
              >
                <let-option v-for="d in nodeList" :key="d" :value="d">
                  {{ d }}
                </let-option>
              </let-select>
            </template>
          </let-table-column>
          <let-table-column :title="$t('region.idcArea')" prop="idc_area">
            <template slot-scope="scope">
              {{ scope.row.idc_area }}
            </template>
          </let-table-column>
          <let-table-column
            :title="$t('deployService.form.template')"
            prop="template_file"
            width="10%"
          >
            <template slot-scope="scope">
              <let-select
                size="small"
                v-model="scope.row.template_file"
                required
                :required-tip="$t('deployService.form.templateTips')"
              >
                <let-option v-for="d in templates" :key="d" :value="d">{{
                  d
                }}</let-option>
              </let-select>
            </template>
          </let-table-column>
          <let-table-column
            :title="$t('operate.operates')"
            prop="appName"
            width="120px"
          >
            <template slot-scope="{ row }">
              <let-table-operation @click="ensureDelete(row)">{{
                $t("operate.delete")
              }}</let-table-operation>
            </template>
          </let-table-column>
        </let-table>
      </let-form-group>

      <let-button size="small" theme="primary" @click="createService"
        >{{ $t("apply.createRouterProxyService") }}
      </let-button>
    </let-form>
  </section>
</template>

<script>
import { checkServerIdentity } from "tls";
const routerModel = () => {
  return {
    apply_id: 17,
    create_person: "adminUser",
    router_db_ip: "",
    router_db_name: "",
    router_db_pass: "",
    router_db_port: "3306",
    router_db_user: "",
    server_ip: "",
    server_name: "",
    template_file: "",
  };
};
const proxyModel = () => {
  return {
    apply_id: 17,
    create_person: "adminUser",
    idc_area: "SZ",
    server_ip: [],
    server_name: "",
    template_file: "",
  };
};
export default {
  data() {
    let { applyId } = this.$route.params;
    return {
      nodeList: [],
      templates: [],
      applyId,
      apply: {
        dbMethod: false,
        routerDbId: 0,
        Router: routerModel(),
        Proxy: [proxyModel()],
      },
      routerDb: [],
    };
  },
  methods: {
    getNodeList() {
      return this.$tars
        .getJSON("/server/api/node_list")
        .then((data) => {
          // console.log(data);
          this.nodeList = data;
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    templateNameList() {
      return this.$tars
        .getJSON("/server/api/template_name_list")
        .then((data) => {
          this.templates = data;
          this.apply.Router.template_file = "tars.cpp.default";
          this.apply.Proxy.forEach(
            (item) => (item.template_file = "tars.cpp.default")
          );
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    changeStatus() {},
    getApplyInfo() {
      let { applyId } = this;
      return this.$ajax
        .getJSON("/server/api/get_apply_and_router_and_proxy", { applyId })
        .then((apply) => {
          this.apply = apply || {};
          // this.apply.dbMethod = true;
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    createService() {
      // alert(this.apply.dbMethod);
      if (this.apply.dbMethod == undefined) {
        this.$tip.error(`${this.$t("apply.dbMethod")}`);
        return;
      }

      for (var i = 0; i < this.apply.Proxy.length; i++) {
        let item = this.apply.Proxy[i];
        if (item.server_ip.length == 0) {
          this.$tip.error(`${this.$t("apply.proxyIp")}`);
          return;
        }
      }

      if (this.$refs.detailForm.validate()) {
        const model = this.apply;
        const url = "/server/api/save_router_proxy";
        let hasDuplicateIp = this.checkDuplicateIp(model.Proxy);
        if (hasDuplicateIp) this.$tip.error(this.$t("apply.duplicateIp"));
        else {
          const loading = this.$Loading.show();
          this.$ajax
            .postJSON(url, model)
            .then((data) => {
              loading.hide();
              let { applyId } = this;
              this.$router.push(
                "/operation/apply/installAndPublish/" + applyId
              );
            })
            .catch((err) => {
              loading.hide();
              this.$tip.error(
                `${this.$t("common.error")}: ${err.message || err.err_msg}`
              );
            });
        }
      }
    },
    checkDuplicateIp(proxy) {
      const ipList = [];
      let duplicate = false;
      proxy.forEach((item) => {
        item.server_ip
          .filter((i) => i)
          .forEach((ip) => {
            if (ipList.indexOf(ip) > -1) duplicate = true;
            else ipList.push(ip);
          });
      });
      return duplicate;
    },
    async loadRouterDb() {
      return this.$ajax
        .getJSON("/server/api/load_router_db")
        .then((data) => {
          this.routerDb = data;
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    async ensureDelete(row) {
      try {
        await this.$confirm(this.$t("dcache.operationManage.ensureDelete"));

        return this.$ajax
          .postJSON("/server/api/delete_apply_proxy", { id: row.id })
          .then((data) => {
            if (data == 1) {
              this.apply.Proxy.forEach((item, index, arr) => {
                if (item.id == row.id) {
                  arr.splice(index, 1);
                  this.apply.Proxy = arr;
                  return;
                }
              });
            }

            // this.apply.Proxy = proxy || {};
          })
          .catch((err) => {
            this.$tip.error(
              `${this.$t("common.error")}: ${err.message || err.err_msg}`
            );
          });
      } catch (err) {
        // console.error(err)
        this.$tip.error(err.message);
      }
    },
  },
  async created() {
    await this.getApplyInfo();
    await this.templateNameList();
    await this.getNodeList();
    await this.loadRouterDb();
  },
};
</script>

<style>
.let-form-cols-1 .let-form-item__label {
  width: 140px;
}
</style>
