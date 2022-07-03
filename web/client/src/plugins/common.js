import Vue from 'vue';

class CommonUtil {

  constructor(vue) {
    this.vue = vue;
  }

  showSucc(msg) {
    if (typeof (msg) == "string") {
      this.vue.$message({
        message: msg,
        showClose: true,
        type: "success",
      });
    } else {
      this.vue.$message({
        message: `${this.vue.$t("common.success")}`,
        showClose: true,
        type: "success",
      });
    }
  }

  showWarning(msg) {
    this.vue.$message({
      message: msg,
      showClose: true,
      type: "warning",
    });
  }

  showError(err) {
    if (typeof (err) == "string") {
      this.vue.$message({
        message: err,
        showClose: true,
        type: "error",
      });
    } else {
      if (err && (err.message || err.err_msg)) {
        this.vue.$message({
          message: `${this.vue.$t("common.error")}: ${err.message || err.err_msg}`,
          showClose: true,
          type: "error",
        });
      } else {
        this.vue.$message({
          message: `${this.vue.$t("common.error")}`,
          showClose: true,
          type: "error",
        });
      }
    }
  }

  showCloudError(kind, err) {
    this.vue.$message({
      message: this.vue.$t(
        `cloud.${kind}.${err ? err.tars_ret || "-1" : "-1"}`
      ),
      showClose: true,
      type: "error",
    });
  }

}

Object.defineProperty(Vue.prototype, '$common', {
  get() {
    if (!this.commonUtil) {
      this.commonUtil = new CommonUtil(this);
    }

    return this.commonUtil;
  },
});

// export {
//   validateEmail,
//   validatePass,
//   validatePass2
// }