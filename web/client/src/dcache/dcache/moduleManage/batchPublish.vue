<template>
  <section style="display: inline" class="batch-publish">
    <let-button theme="primary" :disabled="disabled" @click="init">{{
      $t("dcache.batchPublish")
    }}</let-button>
    <let-modal
      v-model="show"
      :footShow="false"
      :closeOnClickBackdrop="true"
      :width="'1000px'"
      :title="$t('dcache.batchPublish')"
    >
      <let-form>
        <let-form-item :label="$t('service.serverName')">
          <p v-for="server in checkedServers">{{ server.server_name }}</p>
        </let-form-item>
      </let-form>
      <let-table :data="versionList" :empty-msg="$t('common.nodata')">
        <let-table-column>
          <template slot-scope="scope">
            <let-radio v-model="publishId" :label="scope.row.id"
              >&nbsp;</let-radio
            >
          </template>
        </let-table-column>
        <let-table-column title="ID" prop="id">
          <template slot-scope="scope">
            {{ scope.row.id }}
            <span
              :title="$t('releasePackage.default')"
              style="color: green; display: inline-block;width:1em;height:1em;"
            >
              <icon v-if="scope.row.default_version == 1" name="default" />
            </span>
          </template>
        </let-table-column>
        <let-table-column
          :title="$t('releasePackage.moduleName')"
          prop="server"
        ></let-table-column>
        <let-table-column :title="$t('releasePackage.cacheType')">
          <template slot-scope="scope">
            <span>{{
              scope.row.package_type === 1 ? "KVcache" : "MKVcache"
            }}</span>
          </template>
        </let-table-column>
        <let-table-column
          :title="$t('serverList.servant.comment')"
          prop="comment"
        ></let-table-column>
        <let-table-column
          :title="$t('releasePackage.uploadTime')"
          prop="posttime"
        ></let-table-column>
        <let-pagination
          slot="pagination"
          align="right"
          :total="publishModal.totalPatchPage"
          :page="publishModal.currPage"
          @change="patchChangePage"
        >
          abc
        </let-pagination>
        <template slot="operations">
          <let-button :disabled="!publishId" theme="primary" @click="publish">{{
            $t("apply.publish")
          }}</let-button>
        </template>
      </let-table>
    </let-modal>
    <let-modal
      v-model="releaseIng"
      :footShow="false"
      :closeOnClickBackdrop="true"
      :width="'1000px'"
      :title="$t('dcache.batchPublish')"
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
import { serverPatchList } from "@/plugins/interface.js";
import tarsReleaseProgress from "./../components/tarsReleaseProgress.vue";
import Mixin from "./mixin.js";

export default {
  components: { tarsReleaseProgress },
  mixins: [Mixin],
  props: {
    disabled: Boolean,
    expandServers: {
      required: false,
      type: Array,
    },
    checkedServers: {
      required: false,
      type: Array,
    },
  },
  computed: {
    cacheVersion() {
      return this.checkedServers.length
        ? this.checkedServers[0].cache_version
        : 1;
    },
  },
  data() {
    return {
      versionList: [],
      releaseId: null,
      show: false,
      publishId: null, //发布包id
      publishModal: {
        moduleName: "DCacheServerGroup",
        application: "DCache",
        totalPatchPage: 0,
        pageSize: 5,
        currPage: 1,
      },
      releaseIng: false,
    };
  },
  methods: {
    init() {
      this.show = true;
      this.releaseId = null;
      this.releaseIng = false;
      this.publishId = null;
      this.getVersionList();
    },
    patchChangePage(page) {
      this.publishModal.currPage = page;
      this.getVersionList();
    },
    async serverPatchList({
      application = "DCache",
      moduleName = "DCacheServerGroup",
      currPage = 1,
      pageSize = 5,
      cacheVersion,
    }) {
      return await this.$tars.getJSON("/server/api/server_patch_list", {
        application,
        module_name: moduleName,
        curr_page: currPage,
        page_size: pageSize,
        package_type: cacheVersion,
      });
    },
    async getVersionList() {
      const { cacheVersion, publishModal } = this;
      const { moduleName, application, currPage, pageSize } = publishModal;
      let { count, rows } = await this.serverPatchList({
        moduleName,
        application,
        currPage,
        pageSize,
        cacheVersion,
      });
      this.publishModal.totalPatchPage = Math.ceil(count / pageSize);
      this.versionList = rows;
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
    async publish() {
      const items = [];
      const { publishId, checkedServers } = this;
      checkedServers.forEach((item) => {
        items.push({
          server_id: item.id.toString(),
          command: "patch_tars",
          parameters: {
            patch_id: publishId,
            bak_flag: item.bak_flag,
            update_text: "batch publish cache servers",
            group_name: "DCacheServerGroup",
          },
        });
      });
      // console.log(items);

      const releaseId = await this.addTask({ items });
      this.releaseIng = true;
      this.releaseId = releaseId;
    },
    getServerList() {
      this.show = false;
      this.$emit("success-fn");
    },
  },
};
</script>
<style lang="postcss">
.let-table .let-form {
  background-color: white;
}
</style>
