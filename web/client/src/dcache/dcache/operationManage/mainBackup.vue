<template>
  <div>
    <let-table
      :data="showList"
      :empty-msg="$t('common.nodata')"
      ref="pageTable"
    >
      <let-table-column
        :title="$t('dcache.operationManage.appName')"
        prop="appName"
      ></let-table-column>
      <let-table-column
        :title="$t('dcache.operationManage.moduleName')"
        prop="moduleName"
      ></let-table-column>
      <let-table-column
        :title="$t('dcache.operationManage.groupName')"
        prop="groupName"
      ></let-table-column>
      <!-- <let-table-column :title="$t('dcache.operationManage.masterServer')" prop="masterServer"></let-table-column>
      <let-table-column :title="$t('dcache.operationManage.slaveServer')" prop="slaveServer"></let-table-column> -->
      <let-table-column
        :title="$t('dcache.operationManage.switchTime')"
        prop="switchTime"
      ></let-table-column>
      <let-table-column
        :title="$t('dcache.operationManage.modifyTime')"
        prop="modifyTime"
      ></let-table-column>
      <!-- <let-table-column :title="$t('dcache.operationManage.dbFlag')" prop="dbFlag"></let-table-column> -->
      <!-- <let-table-column :title="$t('dcache.operationManage.allowedOut')" prop="enableErase"></let-table-column> -->
      <let-table-column
        :title="$t('dcache.operationManage.switchType')"
        prop="switchType"
      >
        <template slot-scope="{ row: { switchType } }">
          <span :style="{ color: color[switchType] }">{{
            $t(`dcache.${switchTypeText[switchType]}`)
          }}</span>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('dcache.operationManage.switchResult')"
        prop="switchResult"
      >
        <template slot-scope="{ row: { switchResult } }">
          <span :style="{ color: color[switchResult] }">{{
            $t(`dcache.${switchResultText[switchResult]}`)
          }}</span>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('dcache.operationManage.groupStatus')"
        prop="groupStatus"
      >
        <template slot-scope="{ row: { groupStatus } }">
          <span :style="{ color: color[groupStatus] }">{{
            $t(`dcache.${groupStatusText[groupStatus]}`)
          }}</span>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('dcache.operationManage.switchMethod')"
        prop="switchProperty"
      >
        <template slot-scope="{ row: { switchProperty } }">
          <span>{{ $t(`dcache.${switchPropertyText[switchProperty]}`) }}</span>
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('operate.operates')"
        prop="appName"
        width="100px"
      >
        <template slot-scope="{ row }">
          <!-- <let-table-operation :title="$t('dcache.amig')" @click="ensureDelete(row)">{{$t('dcache.omig')}}</let-table-operation> -->
          <let-table-operation
            :title="$t('dcache.areset')"
            @click="recoverMirrorStatus(row)"
            :disabled="!(row.switchType === 2 && row.groupStatus === 2)"
            >{{ $t("dcache.oreset") }}</let-table-operation
          >
          <!-- <let-table-operation :title="$t('dcache.switch')" @click="switchServer(row)">{{$t('dcache.oswitch')}}</let-table-operation> -->
        </template>
      </let-table-column>
      <let-pagination
        slot="pagination"
        align="right"
        :total="total"
        :page="page"
        @change="changePage"
      >
      </let-pagination>
    </let-table>
  </div>
</template>
<script>
import Router from "vue-router";
import {
  getSwitchInfo,
  switchServer,
  recoverMirrorStatus,
} from "@/plugins/interface.js";

export default {
  data() {
    return {
      list: [],
      total: 0,
      page: 1,
      // 切换类型: 0:主备切换, 1:镜像主备切换, 2:镜像切换， 3：备机不可读
      switchTypeText: [
        "switch",
        "mirrorSwitch",
        "mirrorOffSwitch",
        "readFail",
        "switchMirrorAsMaster",
      ],
      // 切换状态: 0:正在切换, 1:切换成功, 2:未切换, 3:切换失败
      switchResultText: [
        "switching",
        "switchSuccess",
        "notSwitch",
        "switchFailure",
      ],
      // 组的访问状态, 0标识读写，1标识只读,2镜像不可用
      groupStatusText: ["rw", "ro", "imageUnavailable"],
      // 切换属性:'auto'自动切换, 'manual'手动切换
      switchPropertyText: { auto: "auto", manual: "manual" },
      color: ["#00AA90", "#6accab", "#9096a3", "#f56c77"],
    };
  },
  computed: {
    showList() {
      let { page, list } = this;
      return list.slice((page - 1) * 10, page * 10);
    },
  },
  methods: {
    /**
     * 获取 列表数据
     */
    async getSwitchInfo() {
      try {
        let { totalNum, switchRecord } = await getSwitchInfo({});
        this.list = switchRecord.map((item) => {
          return item;
        });
        this.total = Math.ceil(totalNum / 10);
      } catch (err) {
        console.error(err);
        this.$tip.error(err.message);
      }
    },
    /**
     * 改变页数
     * @page
     */
    changePage(page) {
      this.page = page;
    },
    /**
     * 主备切换
     * @param row
     * @returns {Promise<void>}
     */
    async switchServer(row) {
      try {
        const { appName, moduleName, groupName } = row;
        await switchServer({ appName, moduleName, groupName });
        this.getSwitchInfo();
      } catch (err) {
        console.error(err);
        this.$tip.error(err.message);
      }
    },
    /**
     * 恢复镜像状态
     * @param row
     * @returns {Promise<void>}
     */
    async recoverMirrorStatus(row) {
      if (row.switchType === 2) {
        try {
          const {
            appName,
            moduleName,
            groupName,
            mirrorIdc,
            dbFlag,
            enableErase,
          } = row;
          if (!mirrorIdc) throw new Error(this.$t("dcache.mirrorEmpty"));
          await recoverMirrorStatus({
            appName,
            moduleName,
            groupName,
            mirrorIdc,
            dbFlag,
            enableErase,
          });
          this.getSwitchInfo();
        } catch (err) {
          console.error(err);
          this.$tip.error(err.message);
        }
      }
    },
  },
  created() {
    this.getSwitchInfo();
  },
};
</script>
