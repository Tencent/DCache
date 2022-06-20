<template>
  <section class="container">
    <let-form inline ref="detailForm">
      <let-form-item required :label="$t('dcache.migrationMethod')">
        <let-radio-group v-model="transferData" :data="migrationMethods">
        </let-radio-group>
      </let-form-item>
      <let-form-group inline label-position="top">
        <let-table ref="table" :data="servers" :empty-msg="$t('common.nodata')">
          <let-table-column :title="$t('module.name')" prop="module_name">
            <template slot-scope="{ row }">
              {{ row.module_name }}
            </template>
          </let-table-column>
          <let-table-column :title="$t('module.serverGroup')" prop="group_name">
            <template slot-scope="{ row }">
              {{ row.group_name }}
            </template>
          </let-table-column>
          <let-table-column
            :title="$t('service.serverName')"
            prop="server_name"
          >
            <template slot-scope="{ row }">
              {{ row.server_name }}
            </template>
          </let-table-column>
          <let-table-column :title="$t('service.serverIp')" prop="server_ip">
            <template slot-scope="scope">
              <let-select
                v-model="scope.row.server_ip"
                size="small"
                filterable
                required
              >
                <let-option v-for="d in nodeList" :key="d" :value="d">
                  {{ d }}
                </let-option>
              </let-select>
            </template>
          </let-table-column>
          <let-table-column :title="$t('module.deployArea')" prop="area">
            <template slot-scope="{ row }">
              {{ row.area }}
            </template>
          </let-table-column>
          <let-table-column
            :title="$t('deployService.form.serviceType')"
            width="80px"
          >
            <template slot-scope="{ row: { server_type } }">
              <span v-if="server_type === 'M'">{{
                $t("cache.mainEngine")
              }}</span>
              <span v-else-if="server_type === 'S'">{{
                $t("cache.standByEngine")
              }}</span>
              <span v-else-if="server_type === 'I'">{{
                $t("cache.mirror")
              }}</span>
            </template>
          </let-table-column>
          <let-table-column :title="$t('module.memorySize')" prop="memory">
            <template slot-scope="{ row }">
              <!--{{row.memory}} -->
              <let-input
                size="small"
                v-model="row.memory"
                :placeholder="$t('module.shmKeyRule')"
              />
            </template>
          </let-table-column>
          <let-table-column :title="$t('module.shmKey')" prop="shmKey">
            <template slot-scope="scope">
              <let-input
                size="small"
                v-model="scope.row.shmKey"
                :placeholder="$t('module.shmKeyRule')"
              />
            </template>
          </let-table-column>
        </let-table>
      </let-form-group>
      <div>
        <let-button size="small" theme="primary" @click="submitServerConfig">{{
          $t("common.submit")
        }}</let-button>
      </div>
    </let-form>
  </section>
</template>

<script>
import { transferDCache } from "@/plugins/interface.js";
import Mixin from "./mixin.js";

export default {
  mixins: [Mixin],
  props: {
    checkSrcGroupName: {
      required: true,
      type: String,
    },
  },
  data() {
    return {
      nodeList: [],
    };
  },
  methods: {
    async submitServerConfig() {
      if (this.$refs.detailForm.validate()) {
        let {
          servers,
          appName,
          moduleName,
          cache_version,
          dstGroupName,
          transferData,
          checkSrcGroupName,
        } = this;
        try {
          await transferDCache({
            servers,
            appName,
            moduleName,
            cache_version,
            srcGroupName: checkSrcGroupName,
            dstGroupName,
            transferData,
          });

          this.$tip.success(
            this.$t("dcache.operationManage.hasServerMigration")
          );
          this.$emit("close");
        } catch (err) {
          this.$tip.error(err.message);
        }
      }
    },
  },
  mounted() {
    this.$tars
      .getJSON("/server/api/node_list")
      .then((data) => {
        this.nodeList = data;
      })
      .catch((err) => {
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      });
  },
  created() {
    this.getServers();
  },
};
</script>

<style></style>
