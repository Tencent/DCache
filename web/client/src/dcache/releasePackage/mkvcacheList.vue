<template>
  <section class="container">
    <div>
      <let-button
        theme="primary"
        size="small"
        @click="uploadModal.show = !uploadModal.show"
      >
        {{ $t("releasePackage.uploadPackage") }}
      </let-button>
    </div>

    <!-- 服务列表 -->
    <let-table
      :data="packages.rows"
      :title="$t('serverList.title.serverList')"
      :empty-msg="$t('common.nodata')"
    >
      <let-table-column title="ID" prop="id">
        <template slot-scope="scope">
          <span
            :title="$t('releasePackage.default')"
            style="color: green; display: inline-block;"
          >
            <i
              v-if="scope.row.default_version == 1"
              class="el-icon-refresh-right"
            ></i>
          </span>
          {{ scope.row.id }}
        </template>
      </let-table-column>
      <let-table-column
        :title="$t('releasePackage.moduleName')"
        prop="server"
      ></let-table-column>
      <!-- <let-table-column :title="$t('releasePackage.cacheType')">
        <template slot-scope="scope">
          <span v-if="scope.row.package_type == 1">KVCache</span>
          <span v-else-if="scope.row.package_type == 2">MKVCache</span>
        </template>
      </let-table-column> -->
      <let-table-column
        :title="$t('serverList.servant.comment')"
        prop="comment"
      ></let-table-column>
      <let-table-column
        :title="$t('releasePackage.uploadTime')"
        prop="posttime"
      ></let-table-column>
      <let-table-column :title="$t('operate.operates')" width="260px">
        <template slot-scope="scope">
          <let-table-operation @click="setDefault(scope.row)">{{
            $t("operate.default")
          }}</let-table-operation>
          <let-table-operation @click="deletePackage(scope.row.id)">{{
            $t("operate.delete")
          }}</let-table-operation>
        </template>
      </let-table-column>
    </let-table>

    <!-- 上传发布包弹出框 -->
    <let-modal
      v-model="uploadModal.show"
      :title="$t('pub.dlg.upload')"
      width="880px"
      :footShow="false"
      @on-cancel="closeUploadModal"
    >
      <let-form
        v-if="uploadModal.model"
        ref="uploadForm"
        itemWidth="100%"
        @submit.native.prevent="uploadPatchPackage"
      >
        <let-form-item :label="$t('releasePackage.moduleName')">
          {{ uploadModal.model.application }}.{{
            uploadModal.model.module_name
          }}
        </let-form-item>
        <!-- <let-form-item :label="$t('releasePackage.cacheType')">
          <let-select v-model="uploadModal.model.package_type" size="small">
            <let-option value="1">KVCache</let-option>
            <let-option value="2">MKVCache</let-option>
          </let-select>
        </let-form-item> -->
        <let-form-item :label="$t('pub.dlg.releasePkg')" itemWidth="400px">
          <let-uploader
            :placeholder="$t('pub.dlg.defaultValue')"
            @upload="uploadFile"
            >{{ $t("common.submit") }}
          </let-uploader>
          <span v-if="uploadModal.model.file">{{
            uploadModal.model.file.name
          }}</span>
        </let-form-item>
        <let-form-item :label="$t('serverList.servant.comment')">
          <let-input
            type="textarea"
            :rows="3"
            v-model="uploadModal.model.comment"
          >
          </let-input>
        </let-form-item>
        <let-button type="submit" theme="primary">{{
          $t("serverList.servant.upload")
        }}</let-button>
      </let-form>
    </let-modal>
  </section>
</template>

<script>
import "@/assets/icon/default.svg";
export default {
  data() {
    return {
      uploadModal: {
        show: false,
        model: {
          application: "DCache",
          module_name: "MKVCacheServer",
          // package_type: "1",
          file: {},
          comment: "",
        },
      },
      packages: {
        count: 0,
        rows: [],
      },
    };
  },
  methods: {
    closeUploadModal() {
      this.uploadModal.show = false;
    },
    uploadFile(file) {
      this.uploadModal.model.file = file;
    },
    uploadPatchPackage() {
      // 上传发布包
      if (this.$refs.uploadForm.validate()) {
        const loading = this.$Loading.show();
        const formdata = new FormData();
        formdata.append("application", this.uploadModal.model.application);
        formdata.append("module_name", this.uploadModal.model.module_name);
        formdata.append("task_id", new Date().getTime());
        // formdata.append("package_type", this.uploadModal.model.package_type);
        formdata.append("suse", this.uploadModal.model.file);
        formdata.append("comment", this.uploadModal.model.comment);
        this.$tars
          .postForm("/server/api/upload_patch_package", formdata)
          .then(() => {
            loading.hide();
            this.closeUploadModal();
            this.getPatchPackage();
          })
          .catch((err) => {
            loading.hide();
            this.$tip.error(
              `${this.$t("common.error")}: ${err.message || err.err_msg}`
            );
          });
      }
    },
    getPatchPackage() {
      this.$tars
        .getJSON("/server/api/server_patch_list", {
          application: "DCache",
          module_name: "MKVCacheServer",
        })
        .then((data) => {
          this.packages = data;
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    deletePackage(id) {
      this.$confirm(
        this.$t("releasePackage.confirmDeleteTip"),
        this.$t("common.alert")
      )
        .then(() => {
          this.$tars
            .postJSON("/server/api/delete_patch_package", { id })
            .then((data) => {
              this.getPatchPackage();
            });
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    setDefault(row) {
      let { id} = row;
      this.$tars
        .postJSON("/server/api/set_patch_package_default", {
          id,
          // package_type,
          application: this.uploadModal.model.application,
          module_name: this.uploadModal.model.module_name,
        })
        .then((data) => {
          this.$tip.success(`${this.$t("common.success")}`);
          this.getPatchPackage();
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
  },
  created() {
    this.getPatchPackage();
  },
};
</script>

<style></style>
