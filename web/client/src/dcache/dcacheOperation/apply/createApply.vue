<template>
  <section class="page_operation_create_apply">
    <let-form inline ref="detailForm">
      <let-form-item :label="$t('apply.title')" itemWidth="240px" required>
        <let-input
          size="small"
          v-model="model.name"
          required
          :required-tip="$t('deployService.table.tips.empty')"
        >
        </let-input>
      </let-form-item>
      <let-form-item :label="$t('region.idcArea')" itemWidth="240px">
        <let-select
          size="small"
          v-model="model.idcArea"
          @change="changeSelect"
          required
          :required-tip="$t('deployService.table.tips.empty')"
        >
          <let-option v-for="d in regions" :key="d.label" :value="d.label">
            {{ d.region }}
          </let-option>
        </let-select>
      </let-form-item>
      <let-form-item :label="$t('region.setArea')" itemWidth="240px">
        <let-select size="small" :multiple="true" v-model="model.setArea">
          <let-option v-for="d in setRegions" :key="d.label" :value="d.label">
            {{ d.region }}
          </let-option>
        </let-select>
      </let-form-item>
      <div>
        <let-button size="small" theme="primary" @click="createApply">{{
          $t("apply.createApply")
        }}</let-button>
      </div>
    </let-form>
  </section>
</template>

<script>
import Ajax from "@/plugins/tars";
import { getRegionList } from "@/plugins/interface.js";
const getInitialModel = () => ({
  name: "",
  admin: "",
  idcArea: "",
  setArea: [],
});
export default {
  data() {
    return {
      regions: [],
      setRegions: [],
      model: getInitialModel(),
    };
  },
  methods: {
    changeSelect() {
      this.model.setArea = [];
      let setRegions = this.regions.concat();
      setRegions.splice(
        setRegions.indexOf(
          setRegions.find((item) => item.label == this.model.idcArea)
        ),
        1
      );
      this.setRegions = setRegions;
    },
    createApply() {
      if (this.$refs.detailForm.validate()) {
        const model = this.model;
        const url = "/server/api/add_apply";
        const loading = this.$Loading.show();
        this.$ajax
          .postJSON(url, model)
          .then((data) => {
            loading.hide();

            if (data.hasApply) {
              this.$confirm(
                this.$t("dcache.applyExists"),
                this.$t("common.alert")
              )
                .then(async () => {
                  loading.show();

                  this.$ajax
                    .postJSON("/server/api/overwrite_apply", model)
                    .then((data) => {
                      loading.hide();
                      this.$tip.success(this.$t("common.success"));
                      this.$router.push(
                        "/operation/apply/createService/" + data.id
                      );
                    })
                    .catch((err) => {
                      loading.hide();
                      this.$tip.error(
                        `${this.$t("common.error")}: ${err.message ||
                          err.err_msg}`
                      );
                    });
                })
                .catch(() => {});
            } else {
              loading.hide();

              this.$tip.success(this.$t("common.success"));
              this.$router.push("/operation/apply/createService/" + data.id);
            }
          })
          .catch((err) => {
            loading.hide();
            this.$tip.error(
              `${this.$t("common.error")}: ${err.message || err.err_msg}`
            );
          });
      }
    },
    async getRegionList() {
      try {
        let regions = await getRegionList();
        this.regions = regions;
      } catch (err) {
        // console.error(err);
      }
    },
  },
  created() {
    this.getRegionList();
  },
  beforeRouteEnter(to, from, next) {
    Ajax.getJSON("/server/api/has_dcache_patch_package")
      .then((hasDefaultPackage) => {
        if (!hasDefaultPackage) {
          // console.log(hasDefaultPackage);
          next((vm) => {
            vm.$tip.warning(
              `${vm.$t("common.warning")}: ${vm.$t("apply.uploadPatchPackage")}`
            );
            vm.$router.push("/releasePackage/proxyList");
          });
        } else {
          next();
        }
      })
      .catch((err) => {
        console.error(err.message || err.err_msg);
      });
  },
};
</script>

<style></style>
