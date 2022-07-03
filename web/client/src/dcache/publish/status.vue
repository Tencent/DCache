<template>
  <div>
    <!-- 发布结果弹出框 -->
    <let-modal
      v-model="finishModal.show"
      :title="$t('serverList.table.th.result')"
      width="70%"
      @on-cancel="onClose"
      @close="onClose"
      :footShow="false"
    >
      <let-table
        v-if="finishModal.model"
        :title="$t('serverList.servant.taskID') + finishModal.model.task_no"
        :data="finishModal.model.items"
      >
        <let-table-column
          :title="$t('serverList.table.th.ip')"
          prop="node_name"
          width="200px"
        ></let-table-column>
        <let-table-column :title="$t('common.status')" width="180px">
          <template slot-scope="scope">
            <let-tag
              :theme="
                scope.row.status == 3
                  ? 'success'
                  : scope.row.status == 4
                  ? 'danger'
                  : ''
              "
              checked
            >
              {{
                statusConfig[scope.row.status] +
                  (scope.row.status != 3 && scope.row.status != 4
                    ? scope.row.desc
                    : "")
              }}
            </let-tag>
          </template>
        </let-table-column>
        <let-table-column
          :title="$t('serverList.table.th.result')"
          prop="execute_info"
        >
        </let-table-column>
      </let-table>
    </let-modal>
  </div>
</template>

<script>
export default {
  name: "PublishStatus",

  data() {
    return {
      closeCallback: null,
      timerId: null,
      finishModal: {
        show: false,
        model: {
          task_no: "",
          items: [],
        },
      },
      statusConfig: {
        0: this.$t("serverList.restart.notStart"),
        1: this.$t("serverList.restart.preparing"),
        2: this.$t("serverList.restart.running"),
        3: this.$t("serverList.restart.success"),
        4: this.$t("serverList.restart.failed"),
        5: this.$t("serverList.restart.cancel"),
        6: this.$t("serverList.restart.pauseFlow"),
      },
      statusMap: {
        0: "EM_T_NOT_START",
        1: "EM_T_PREPARE",
        2: "EM_T_RUNNING",
        3: "EM_T_SUCCESS",
        4: "EM_T_FAILED",
        5: "EM_T_CANCEL",
        6: "EM_T_PARIAL",
      },
      endStatus: [3, 4],
    };
  },
  methods: {
    onClose() {
      if (this.closeCallback) {
        this.closeCallback();
      }
      clearTimeout(this.timerId);
    },
    savePublishServer(publishModal, callback, group_name, patch_id) {
      group_name = group_name || "";
      patch_id = patch_id || publishModal.model.patch_id.toString();
      this.closeCallback = callback;
      // 发布
      var items = [];
      let elegant = publishModal.elegant || false;
      publishModal.model.serverList.forEach((item) => {
        items.push({
          server_id: item.id.toString(),
          command: publishModal.command || "patch_tars",
          parameters: {
            patch_id: patch_id,
            bak_flag: item.bak_flag || false,
            update_text: publishModal.model.update_text || "",
            group_name: group_name,
            run_type: item.run_type || "",
          },
        });
      });

      // console.log(items);

      const loading = this.$Loading.show();
      this.$tars
        .postJSON("/server/api/add_task", {
          serial: !elegant,
          elegant: elegant,
          eachnum: publishModal.eachnum || 1,
          items,
        })
        .then((data) => {
          loading.hide();
          //   this.closePublishModal();
          this.finishModal.model.task_no = data;
          this.finishModal.show = true;
          // 实时更新状态
          this.getTaskRepeat(data);
        })
        .catch((err) => {
          loading.hide();
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    getTaskRepeat(taskId) {
      // let timerId;
      this.timerId && clearTimeout(this.timerId);
      const getTask = () => {
        this.$tars
          .getJSON("/server/api/task", {
            task_no: taskId,
          })
          .then((data) => {
            let done = true;
            data.items.forEach((item) => {
              if (!this.endStatus.includes(item.status)) {
                done = false;
              }

              if (item.percent) {
                item.desc = "(" + item.percent + "%)";
              } else {
                item.desc = "...";
              }
            });
            done
              ? clearTimeout(this.timerId)
              : (this.timerId = setTimeout(getTask, 2000));
            this.finishModal.model.items = data.items;
          })
          .catch((err) => {
            clearTimeout(this.timerId);
            this.timerId = null;
            this.$tip.error(
              `${this.$t("common.error")}: ${err.message || err.err_msg}`
            );
          });
      };
      getTask();
    },
  },
};
</script>
