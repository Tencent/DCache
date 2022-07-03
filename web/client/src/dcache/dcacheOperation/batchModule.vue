<template>
  <div class="page-batch-module">
    <div>
      <div style="width: 49%;float: left">
        <div>
          <h2>{{ $t("deployService.batchDeploy.title.dcacheConf") }}</h2>
        </div>
        <div class="borderdiv" style="height: 620px">
          <yaml-editor v-model="yamlModeal"></yaml-editor>
        </div>
        <div class="buttonDiv" style="margin-top: 10px">
          <let-uploader
            @upload="uploadFile"
            style="width: 150px"
            id="uploaderBtn"
          >
            {{ $t("deployService.batchDeploy.btn.uploadConf") }}
          </let-uploader>
          <let-button
            type="button"
            theme="primary"
            style="margin-left: 5px"
            @click="shutdown"
            size="small"
            :disabled="shutdownBtn"
          >
            {{ $t("deployService.batchDeploy.btn.shutdown") }}
          </let-button>
          <let-button
            type="button"
            theme="primary"
            style="margin-left: 5px"
            @click="batchInstall"
            size="small"
            :disabled="installBtn"
          >
            {{ $t("deployService.batchDeploy.btn.dcacheApply") }}
          </let-button>
        </div>
      </div>
      <div style="width: 49%;padding-left:1%; ; float: left">
        <div>
          <h2>{{ $t("deployService.batchDeploy.title.out") }}</h2>
        </div>
        <div
          id="log"
          class="borderdiv pre_con"
          style="height: 620px;padding-bottom: 50px"
        >
          <pre v-for="log in outPut" v-show="log != '->end'">{{ log }}</pre>
        </div>
        <div class="buttonDiv" style="margin-top: 10px">
          <let-button
            type="button"
            theme="primary"
            style="margin-left: 5px"
            @click="clearLog"
            size="small"
          >
            {{ $t("deployService.batchDeploy.btn.clearLog") }}
          </let-button>
        </div>
      </div>
      <div style="clear: both"></div>
    </div>
    <let-modal
      :title="$t('dcache.moduleExists')"
      v-model="showUpdate"
      width="600px"
      @on-confirm="install"
      @on-cancel="closeCheck"
    >
      <let-form label-position="top">
        <div v-for="item in hasModuleList">
          {{ `${item.appName}.${item.moduleName}` }}
        </div>
      </let-form>
    </let-modal>
  </div>
</template>

<script>
import YamlEditor from "@/components/editor/yaml-editor";
import jsYaml from "js-yaml";
import Axios from "axios";
import uuidv4 from "uuid/v4";

export default {
  name: "batchCreateModule",
  data() {
    return {
      batchDeployModal: {
        show: false,
      },
      yamlModeal: "",
      outPut: [],
      showUpdate: false,
      hasModuleList: [],
      installBtn: false,
      shutdownBtn: true,
      installToken: "",
    };
  },
  components: {
    YamlEditor,
  },
  watch: {
    //监视日志结束 修改按钮状态
    outPut() {
      if (this.outPut[this.outPut.length - 1] == "->end") {
        this.installBtn = false;
      }
    },
    installBtn() {
      this.shutdownBtn = !this.installBtn;
    },
  },
  updated: function() {
    //log持续最底端
    this.$nextTick(function() {
      let p = document.getElementById("log");
      p.scrollTop = p.scrollHeight;
    });
  },
  mounted() {
    Axios.create({ baseURL: "/" })({
      method: "get",
      url: "/static/installExamples/dcache-install-examples.yml",
    }).then((response) => {
      this.yamlModeal = response.data;
    });
    this.outPut = [];

    //获取节点,修改默认样式
    let btn = document.getElementById("uploaderBtn").firstChild
      .lastElementChild;
    btn.style.width = "150px";
    btn.style.marginLeft = "-150px";
  },
  methods: {
    uploadFile(file) {
      let filename = file.name;
      if (filename.lastIndexOf(".") == -1) {
        this.$tip.error(`${this.$t("deployService.batchDeploy.errFile")}`);
        return false;
      }
      let AllImgExt = ".yml|.yaml|";
      let extName = filename.substring(filename.lastIndexOf(".")).toLowerCase();
      if (AllImgExt.indexOf(extName + "|") == -1) {
        this.$tip.error(`${this.$t("deployService.batchDeploy.errFile")}`);
        return false;
      }
      let reader = new FileReader();
      reader.readAsText(file, "UTF-8");
      let _this = this;
      reader.onload = function(evt) {
        let str = evt.target.result;
        _this.$nextTick(() => {
          _this.yamlModeal = reader.result;
        });
      };
    },
    batchInstall: function() {
      this.installBtn = true;
      try {
        let data = jsYaml.load(this.yamlModeal);
      } catch (e) {
        this.$tip.error(`${this.$t("deployService.batchDeploy.errConf")}`);
        return;
      }
      const that = this;
      this.$ajax
        .postJSON("/server/api/check_module", {
          content: this.yamlModeal,
        })
        .then((data) => {
          let ret = data.map((item) => {
            return item.hasModule;
          });
          this.hasModuleList = data.filter((item) => item.hasModule === true);
          if (this.hasModuleList.length > 0) {
            this.showUpdate = true;
          } else {
            this.install();
          }
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("deployLog.failed")}: ${err.err_msg || err.message}`
          );
        });
    },
    closeCheck() {
      this.installBtn = false;
    },

    install() {
      this.showUpdate = false;
      this.outPut = [];
      this.installBtn = true;
      this.installToken = uuidv4();

      this.$ajax
        .postJSON("/server/api/add_module_by_yaml", {
          content: this.yamlModeal,
          token: this.installToken,
        })
        .then((data) => {})
        .catch((err) => {
          this.$tip.error(
            `${this.$t("deployLog.failed")}: ${err.err_msg || err.message}`
          );
        });
      let timer = () => {
        this.$ajax
          .getJSON("/server/api/get_install_log", { token: this.installToken })
          .then((data) => {
            let length = data.length;
            if (length > 0) {
              this.outPut = Object.assign(data, this.outPut);
            }
            if (data[length - 1] != "->end") {
              setTimeout(timer, 2000);
            } else {
              this.installToken = "";
            }
          })
          .catch((err) => {
            this.$tip.error(
              `${this.$t("common.error")}: ${err.err_msg || err.message}`
            );
          });
      };
      setTimeout(timer, 100);
    },
    shutdown() {
      this.$ajax
        .getJSON("/server/api/shutdown_install", {
          content: this.yamlModeal,
          token: this.installToken,
        })
        .then((data) => {
          console.log("data:" + JSON.stringify(data, null, 4));
          this.$tip.success(this.$t("deployService.batchDeploy.tip.stopSucc"));
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("deployLog.failed")}: ${err.err_msg || err.message}`
          );
        });
    },
    clearLog() {
      this.outPut = [];
    },
  },
};
</script>

<style scoped>
.page-batch-module {
  .borderdiv {
    border: 1px solid #84867f;
  }

  .buttonDiv {
    display: flex;
    flex-direction: row;
    justify-content: flex-start;
  }

  .pre_con {
    display: block;
    overflow: scroll;
  }

  .pre_con pre {
    margin-left: 20px;
    color: #2f2f97;
    display: block;
    word-break: break-word;
    white-space: pre-wrap;
  }
}
</style>
