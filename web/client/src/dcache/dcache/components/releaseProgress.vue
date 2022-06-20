<template>
  <div class="progressTable">
    <let-table
      ref="ProgressTable"
      :data="releaseProgress"
      :empty-msg="$t('common.nodata')"
    >
      <let-table-column
        :title="$t('service.serverName')"
        prop="serverName"
        width="30%"
      ></let-table-column>
      <let-table-column
        :title="$t('service.serverIp')"
        prop="nodeName"
        width="30%"
      ></let-table-column>
      <let-table-column
        :title="$t('publishLog.releaseId')"
        prop="releaseId"
        width="15%"
      ></let-table-column>
      <let-table-column
        :title="$t('publishLog.releaseProgress')"
        prop="percent"
      ></let-table-column>
    </let-table>
    <p ref="progressTip">{{ success ? "发布完成" : "发布中..." }}</p>
  </div>
</template>

<script>
import { getReleaseProgress } from "@/plugins/interface.js";
export default {
  props: {
    releaseId: {
      type: [Number, String],
      required: true,
    },
  },
  data() {
    return {
      releaseProgress: [],
      timer: "",
      progressTip: "",
      success: false,
    };
  },
  methods: {
    async getProgress() {
      try {
        let { percent, progress } = await getReleaseProgress({
          releaseId: this.releaseId,
        });

        this.releaseProgress = progress;

        if (+percent !== 100) {
          this.timer = window.setTimeout(this.getProgress, 1000);
        } else {
          if (this.timer) window.clearTimeout(this.timer);
          this.success = true;
          this.$emit("done-fn");
        }
      } catch (err) {
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
  },
};
</script>

<style scoped>
.progressTable {
  margin-top: 20px;
}
</style>
