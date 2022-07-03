<template>
  <section class="page_operation_region">
    <div>
      <let-button theme="primary" size="mini" @click="addRegion">{{
        $t("region.add")
      }}</let-button>
    </div>

    <let-table
      ref="table"
      :data="items"
      :empty-msg="$t('common.nodata')"
      :title="$t('region.list')"
    >
      <let-table-column :title="$t('region.serial')" width="25%"
        ><template slot-scope="scope">{{
          scope.$index + 1
        }}</template></let-table-column
      >
      <let-table-column
        :title="$t('region.title')"
        prop="region"
        width="25%"
      ></let-table-column>
      <let-table-column
        :title="$t('region.label')"
        prop="label"
      ></let-table-column>
      <let-table-column :title="$t('operate.operates')" width="180px">
        <template slot-scope="scope">
          <let-table-operation @click="editRegion(scope.row)">{{
            $t("operate.update")
          }}</let-table-operation>
          <let-table-operation @click="removeItem(scope.row)">{{
            $t("operate.delete")
          }}</let-table-operation>
        </template>
      </let-table-column>
    </let-table>
    <let-modal
      :title="$t(title)"
      v-model="showModel"
      width="600px"
      @on-confirm="saveItem"
    >
      <let-form ref="detailForm">
        <let-form-item :label="$t('region.title')" required>
          <let-input
            size="small"
            v-model="model.region"
            required
            :placeholder="$t('region.regionTips')"
          ></let-input>
        </let-form-item>
        <let-form-item :label="$t('region.label')" required>
          <let-input
            size="small"
            v-model="model.label"
            required
            :placeholder="$t('region.labelTips')"
          ></let-input>
        </let-form-item>
      </let-form>
    </let-modal>
  </section>
</template>

<script>
export default {
  data() {
    return {
      title: "region.add",
      items: [],
      showModel: false,
      model: {},
    };
  },
  mounted() {
    this.fetchData();
  },
  methods: {
    fetchData() {
      this.$ajax
        .getJSON("/server/api/get_region_list")
        .then((items) => {
          this.items = items || [];
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    addRegion() {
      this.showModel = true;
      this.title = "region.add";
      this.model = { region: "", label: "" };
    },
    editRegion(model) {
      this.showModel = true;
      this.showModel = true;
      this.title = "region.modify";
      this.model = model;
    },
    saveItem() {
      if (this.$refs.detailForm.validate()) {
        const model = this.model;
        const url = model.id
          ? "/server/api/update_region"
          : "/server/api/add_region";

        const loading = this.$Loading.show();
        this.$ajax
          .postJSON(url, model)
          .then(() => {
            loading.hide();
            this.$tip.success(this.$t("common.success"));
            this.closeDetailModal();
            this.fetchData();
          })
          .catch((err) => {
            loading.hide();
            this.$tip.error(
              `${this.$t("common.error")}: ${err.message || err.err_msg}`
            );
          });
      }
    },
    removeItem({ id }) {
      this.$ajax
        .getJSON("/server/api/delete_region", { id })
        .then((items) => {
          this.$tip.success(this.$t("common.success"));
          this.fetchData();
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    closeDetailModal() {
      this.$refs.detailForm.resetValid();
      this.showModel = false;
      this.model = {};
    },
  },
};
</script>

<style></style>
