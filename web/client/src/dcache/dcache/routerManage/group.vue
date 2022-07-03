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
        :title="$t('routerManage.moduleName')"
        prop="module_name"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.groupName')"
        prop="group_name"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.accessStatus')"
        prop="access_status"
      >
        <template slot-scope="scope">
          <div
            v-for="item in accessStatus"
            :key="item.val"
            v-if="item.val === scope.row.access_status"
          >
            {{ item.label }}
          </div>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('routerManage.serverName')"
        prop="server_name"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.serverStatus')"
        prop="server_status"
      >
        <template slot-scope="scope">
          <div
            v-for="item in serverStatus"
            :key="item.val"
            v-if="item.val === scope.row.server_status"
          >
            {{ item.label }}
          </div>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('routerManage.pri')"
        prop="pri"
      ></let-table-column>
      <let-table-column
        :title="$t('routerManage.sourceServerName')"
        prop="source_server_name"
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
        <let-form-item :label="$t('routerManage.moduleName')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.module_name"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.groupName')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.group_name"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.accessStatus')" required>
          <let-select
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.access_status"
          >
            <let-option
              v-for="item in accessStatus"
              :key="item.val"
              :value="item.val"
              >{{ item.label }}</let-option
            >
          </let-select>
        </let-form-item>
        <let-form-item :label="$t('routerManage.serverName')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.server_name"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.serverStatus')" required>
          <let-select
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.server_status"
          >
            <let-option
              v-for="item in serverStatus"
              :key="item.val"
              :value="item.val"
              >{{ item.label }}</let-option
            >
          </let-select>
        </let-form-item>
        <let-form-item :label="$t('routerManage.pri')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.pri"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('routerManage.sourceServerName')" required>
          <let-input
            size="small"
            required
            :required-tip="$t('common.notEmpty')"
            v-model="dialogModal.model.source_server_name"
          ></let-input>
        </let-form-item>
      </let-form>
    </let-modal>
  </div>
</template>

<script>
import {
  routerGroupCreate,
  routerGroupDelete,
  routerGroupUpdate,
  routerGroupFindOne,
  routerGroupList,
} from "@/plugins/interface.js";

export default {
  data() {
    return {
      accessStatus: [
        { val: 0, label: "正常状态" },
        { val: 1, label: "只读状态" },
        { val: 2, label: "镜像切换状态" },
      ],
      serverStatus: [
        { val: "M", label: "主机" },
        { val: "S", label: "备机" },
        { val: "I", label: "镜像" },
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
        const result = await routerGroupDelete({
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
        const result = await routerGroupFindOne({
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
        const result = await routerGroupList({
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
          let result = {};
          if (isNew) {
            result = await routerGroupCreate({ data: model });
          } else {
            result = await routerGroupUpdate({ data: model });
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
