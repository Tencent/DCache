<template>
  <section style="display: inline" class="batch-publish">
    <let-button
      :size="size"
      theme="primary"
      :disabled="disabled"
      @click="init"
      >{{ $t(`dcache.batch.${type}`) }}</let-button
    >
    <let-modal
      v-model="show"
      @on-confirm="confirm"
      :closeOnClickBackdrop="true"
      :width="'1000px'"
      :title="$t(`dcache.batch.${type}`)"
    >
      <let-form>
        <let-form-item :label="$t('service.serverName')">
          <p v-for="server in checkedServers">
            {{
              server.application +
                "." +
                server.server_name +
                "_" +
                server.node_name
            }}
          </p>
        </let-form-item>
      </let-form>

      <let-form class="elegant-form">
        <div class="elegant-switch">
          <let-switch
            size="large"
            v-model="elegantChecked"
            @change="changeElegantStatus"
          >
            <span slot="open">{{ $t("dcache.batch.elegantTask") }}</span>
            <span slot="close">{{ $t("dcache.batch.commonTask") }}</span>
          </let-switch>
          &nbsp;&nbsp;&nbsp;
          <div class="elegant-num" v-if="elegantChecked">
            <span class="elegant-label">{{
              $t("dcache.batch.elegantEachNum")
            }}</span>
            <let-input-number
              class="elegant-box"
              :max="20"
              :min="1"
              :step="1"
              v-model="eachNum"
              size="small"
            ></let-input-number>
          </div>
        </div>
      </let-form>
    </let-modal>
    <let-modal
      v-model="releaseIng"
      :footShow="false"
      :closeOnClickBackdrop="true"
      :width="'1000px'"
      :title="$t(`dcache.batch.${type}`)"
    >
      <tars-release-progress
        v-if="!!releaseId && releaseIng"
        :release-id="releaseId"
        @close-fn="this.getServerList"
      ></tars-release-progress>
    </let-modal>
  </section>
</template>
<script>
// import { addTask } from "@/plugins/interface.js";
import tarsReleaseProgress from "./../components/tarsReleaseProgress.vue";
import Mixin from "./mixin.js";

export default {
  components: { tarsReleaseProgress },
  mixins: [Mixin],
  props: {
    size: String,
    disabled: Boolean,
    expandServers: {
      required: false,
      type: Array,
    },
    checkedServers: {
      required: false,
      type: Array,
    },
    type: String,
  },
  data() {
    return {
      versionList: [],
      releaseId: null,
      show: false,
      publishModal: {
        moduleName: "",
        application: "DCache",
        totalPatchPage: 0,
        pageSize: 5,
        currPage: 1,
      },
      releaseIng: false,
      elegantChecked: false,
      eachNum: 0,
    };
  },
  methods: {
    init() {
      this.show = true;
      this.releaseId = null;
      this.releaseIng = false;
    },
    async addTask({
      serial = false,
      elegant = false,
      eachnum = 1,
      items = [
        {
          server_id: "",
          command: "patch_tars",
          parameters: {
            patch_id: 0,
            bak_flag: "",
            group_name: "",
          },
        },
      ],
    }) {
      return await this.$tars.postJSON("/server/api/add_task", {
        serial,
        elegant,
        eachnum,
        items,
      });
    },
    async confirm() {
      const items = [];
      const { checkedServers } = this;
      checkedServers.forEach((item) => {
        items.push({
          server_id: item.id.toString(),
          command: this.type,
        });
      });

      try {
        let elegant = this.elegantChecked;
        let eachnum = this.eachNum;
        let serial = false;
        const releaseId = await this.addTask({
          serial,
          elegant,
          eachnum,
          items,
        });
        this.releaseIng = true;
        this.releaseId = releaseId;
      } catch (err) {
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      }
    },
    getServerList() {
      this.show = false;
      this.$emit("success-fn");
    },
    changeElegantStatus(status) {
      this.checked = status;
      if (status) {
        this.eachNum = 1;
      }
    },
  },
};
</script>
<style lang="postcss">
@import "../../../assets/css/variable.css";
.elegant-form {
  background-color: white !important;
}
.elegant-switch {
  display: block;
  overflow: hidden;
}
.elegant-switch .let-switch {
  display: block;
  float: left;
  margin-right: 10px;
  width: 105px;
}
.elegant-num {
  display: block;
  overflow: hidden;
}
.elegant-label {
  display: block;
  float: left;
  margin-right: 10px;
}
.elegant-box {
  display: block;
  overflow: hidden;
}
</style>
