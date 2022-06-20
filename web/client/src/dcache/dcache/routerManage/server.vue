<template>
  <div>
    <div class="btn_group">
      <let-button theme="primary" size="small" @click="addEvent">{{
        $t("operate.add")
      }}</let-button>
      <let-button theme="danger" size="small" @click="delEvent">{{
        $t("operate.delete")
      }}</let-button>
    </div>
    <let-table
      :data="tableData"
      :empty-msg="$t('common.nodata')"
      ref="pageTable"
    >
      <let-table-column>
        <template slot="head" slot-scope="props">
          <let-checkbox v-model="isCheckedAll"></let-checkbox>
        </template>
        <template slot-scope="scope">
          <let-checkbox
            v-model="scope.row.isChecked"
            :value="scope.row.id"
          ></let-checkbox>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('routerManage.idcArea')"
        prop="idc_area"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.serverName')"
        prop="server_name"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.ip')"
        prop="ip"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.binlogPort')"
        prop="binlog_port"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.cachePort')"
        prop="cache_port"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.wcachePort')"
        prop="wcache_port"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.backupPort')"
        prop="backup_port"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.routeclientPort')"
        prop="routerclient_port"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.remark')"
        prop="remark"
      ></let-table-column>
      <let-table-column :title="$t('operate.operates')" width="60px">
        <template slot-scope="scope">
          <let-table-operation @click="editEvent(scope.row.id)">{{
            $t("operate.update")
          }}</let-table-operation>
        </template>
      </let-table-column>
    </let-table>

    <div style="overflow:hidden;">
      <let-pagination
        align="right"
        style="float:right;"
        :page="pagination.page"
        @change="gotoPage"
        :total="pagination.total"
      >
      </let-pagination>
    </div>

    <!-- 编辑服务弹窗 -->
    <let-modal
      v-model="dialogModal.show"
      :title="dialogModal.isNew ? $t('operate.add') : $t('operate.modify')"
      width="800px"
      @on-confirm="saveDialog"
      @close="closeDialog"
      @on-cancel="closeDialog"
    >
      <let-form v-if="dialogModal.model" ref="dialogForm" itemWidth="100%">
        <let-form-item :label="$t('routerManage.idcArea')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.idc_area"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.serverName')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.server_name"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.ip')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.ip"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.binlogPort')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.binlog_port"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.cachePort')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.cache_port"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.wcachePort')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.wcache_port"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.backupPort')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.backup_port"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.routeclientPort')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.routerclient_port"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.remark')">
          <let-input
            type="textarea"
            :rows="3"
            v-model="dialogModal.model.remark"
          ></let-input>
        </let-form-item>
      </let-form>
    </let-modal>
  </div>
</template>

<script>
import {
  routerServerCreate,
  routerServerDelete,
  routerServerUpdate,
  routerServerFindOne,
  routerServerList,
} from "@/plugins/interface.js";

export default {
  data() {
    return {
      switchStatus: [
        { val: 0, label: "读写自动切换" },
        { val: 1, label: "只读自动切换" },
        { val: 2, label: "不支持自动切换" },
        { val: 3, label: "无效模块" },
      ],
      isCheckedAll: false,
      tableData: [],
      // 分页
      pagination: {
        page: 1,
        size: 10,
        total: 1,
      },
      // 弹框
      dialogModal: {
        show: false,
        model: {},
        isNew: false,
      },
    };
  },
  mounted() {
    this.loadData();
  },
  watch: {
    $route(to, from) {
      this.loadData();
    },
    isCheckedAll() {
      const isCheckedAll = this.isCheckedAll;
      this.tableData.forEach((item) => {
        item.isChecked = isCheckedAll;
      });
    },
  },
  methods: {
    getTreeId() {
      return this.$route.params.treeid;
    },
    async loadData() {
      await this.listEvent();
    },
    async addEvent() {
      this.dialogModal = {
        show: true,
        model: {},
        isNew: true,
      };
    },
    async delEvent() {
      try {
        const { tableData } = this;
        const ids = [];
        tableData.forEach((item) => {
          if (item.isChecked) {
            ids.push(item.id);
          }
        });
        const result = await routerServerDelete({
          data: {
            treeid: this.getTreeId(),
            id: ids,
          },
        });
        if (result) {
          this.listEvent();
          this.$tip.success(this.$t("common.success"));
        }
      } catch (err) {
        this.$tip.error(err.message);
      }
    },
    async editEvent(id) {
      try {
        const result = await routerServerFindOne({
          data: {
            treeid: this.getTreeId(),
            id,
          },
        });
        if (result) {
          this.dialogModal = {
            show: true,
            model: result,
            isNew: false,
          };
        }
      } catch (err) {
        this.$tip.error(err.message);
      }
    },
    async listEvent() {
      try {
        const result = await routerServerList({
          data: {
            treeid: this.getTreeId(),
            page: this.pagination.page,
          },
        });
        result.rows.forEach((item) => {
          item.isChecked = false;
        });
        this.tableData = result.rows;

        this.pagination = {
          page: result.page,
          size: result.size,
          total: Math.ceil(result.count / result.size),
        };
      } catch (err) {
        this.$tip.error(err.message);
      }
    },
    gotoPage(num) {
      this.pagination.page = num;
      this.listEvent();
    },
    async saveDialog() {
      try {
        if (this.$refs.dialogForm.validate()) {
          const { isNew, model } = this.dialogModal;
          model.treeid = this.getTreeId();
          let result = {};
          if (isNew) {
            result = await routerServerCreate({ data: model });
          } else {
            result = await routerServerUpdate({ data: model });
          }
          if (result) {
            this.listEvent();
            this.$tip.success(this.$t("common.success"));
          }
          this.closeDialog();
        }
      } catch (err) {
        this.$tip.error(err.message);
      }
    },
    closeDialog() {
      this.dialogModal.show = false;
      this.dialogModal.model = null;
    },
  },
};
</script>
<style lang="postcss">
.btn_group {
  display: block;
  margin-bottom: 20px;
}
.btn_group .let-button {
  margin-right: 10px;
}
</style>
