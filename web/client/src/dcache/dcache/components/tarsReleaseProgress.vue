<template>
  <div class="progressTable">
    <let-table
      ref="ProgressTable"
      :data="items"
      :empty-msg="$t('common.nodata')"
    >
      <let-table-column
        :title="$t('service.serverName')"
        prop="server_name"
        width="30%"
      ></let-table-column>
      <let-table-column
        :title="$t('service.serverIp')"
        prop="node_name"
        width="30%"
      ></let-table-column>
      <let-table-column :title="$t('operate.status')">
        <template slot-scope="scope">
          <let-tag
            :theme="
              scope.row.status == 2
                ? 'success'
                : scope.row.status == 3
                ? 'danger'
                : ''
            "
            checked
          >
            {{
              statusConfig[scope.row.status] +
                (scope.row.status != 2 &&
                scope.row.status != 3 &&
                scope.row.status != 4
                  ? "..."
                  : "")
            }}
          </let-tag>
        </template>
      </let-table-column>
    </let-table>
    <p ref="progressTip">
      {{ $t(success ? "dcache.executeSuccess" : "dcache.execute") }}
    </p>
  </div>
</template>

<script>
// import { getTarsReleaseProgress } from "@/plugins/interface.js";
export default {
  props: {
    releaseId: {
      type: [Number, String],
      required: true,
    },
  },
  data() {
    return {
      items: [],
      timer: "",
      progressTip: "",
      success: false,
      statusConfig: {
        0: this.$t("serverList.restart.notStart"),
        1: this.$t("serverList.restart.preparing"),
        2: this.$t("serverList.restart.running"),
        3: this.$t("serverList.restart.success"),
        4: this.$t("serverList.restart.failed"),
        5: this.$t("serverList.restart.cancel"),
        6: this.$t("serverList.restart.pauseFlow"),
      },
    };
  },
  methods: {
    async getTarsReleaseProgress(releaseId) {
      return await this.$ajax.getJSON("/server/api/task", {
        task_no: releaseId,
      });
    },
    async getProgress() {
      try {
        let {
          items,
          serial,
          status,
          task_no,
        } = await this.getTarsReleaseProgress(this.releaseId);
        this.items = items;
        let done = true;

        items.forEach((item) =>
          ![2, 3, 4].includes(item.status) ? (done = false) : ""
        );
        //console.log("===>done:", done);
        if (done) {
          if (this.timer) window.clearTimeout(this.timer);
          this.success = true;
          this.$emit("done-fn");
        } else {
          this.timer = window.setTimeout(this.getProgress, 1000);
        }
      } catch (err) {
        this.success = true;
        console.error(err);
        this.$tip.error(err.message);
      }
    },
  },
  created() {
    this.getProgress();
  },
  destroyed() {
    if (this.timer) window.clearTimeout(this.timer);
    this.$emit("close-fn");
  },
};
</script>

<style scoped>
.progressTable {
  margin-top: 20px;
}
</style>
