<template>
  <section class="page_operation_create_module">

    <let-form
      inline
      ref="detailForm"
    >
      <let-form-item :label="$t('apply.title')" itemWidth="240px" required>
        <let-select
          size="small"
          v-model="model.apply_id"
          required
          filterable
          :required-tip="$t('deployService.table.tips.empty')"
        >
          <let-option v-for="d in applys" :key="d.id" :value="d.id">
            {{d.name}}
          </let-option>
        </let-select>
      </let-form-item>
      <let-form-item :label="$t('module.name')" itemWidth="240px" required>
        <let-input
          size="small"
          v-model="model.module_name"
          required
          :required-tip="$t('deployService.table.tips.empty')"
          pattern="^[a-zA-Z0-9]+$"
        >
        </let-input>
      </let-form-item>
      <div>
        <let-button size="small" theme="primary" @click="createModule">{{$t('common.nextStep')}}</let-button>
      </div>

    </let-form>

    <let-modal :title="$t('module.update')" v-model="showUpdate" width="800px" @on-confirm="onConfirmUpdate" >
      <let-form label-position="top">
        <let-select
          size="small"
          v-model="model.update"
          required
          :required-tip="$t('deployService.table.tips.empty')"
          @change="changeUpdate"
        >
          <let-option v-for="d in updateModuleType" :key="d.key" :value="d.key">
            {{$t(d.value)}}
          </let-option>
        </let-select>

        <let-tag size="small" v-if="info.length > 0" style="margin-top: 20px">{{info}}</let-tag>
      </let-form>
    </let-modal>

  </section>
</template>

<script>
  import Ajax from '@/plugins/ajax'

  const getInitialModel = () => ({
    apply_id: '',
    module_name: '',
    follower: '',
    update: 0,
  });

  const updateModuleType = [
    { key: 0, value: 'cache.installFull', info: 'cache.installFullInfo' },
    { key: 1, value: 'cache.installDbAccess', info: 'cache.installDbAccessInfo' },
    { key: 2, value: 'cache.installBackup', info: '' },
    { key: 3, value: 'cache.installMirror', info: '' }
  ];

  export default {
    data () {
      return {
        model: getInitialModel(),
        applys: [],
        updateModuleType,
        showUpdate: false,
        info: '',
        moduleInfo: {}
      }
    },
    methods: {
      changeUpdate() {
        this.info = this.$t(this.updateModuleType[this.model.update].info);
      },
      async onConfirmUpdate() {
        try
        {
          //全新安装
          const url = '/server/api/add_module_base_info';

          let data = await this.$ajax.postJSON(url, this.model);

          this.$router.push('/operation/module/moduleConfig/' + data.id);
        } catch(err) {
          this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
        }
      },
      async createModule () {

        let applyName = this.applys.filter(item=>{
          return item.id == this.model.apply_id;
        })[0].name;

        if(!(this.model.module_name.indexOf(applyName) == 0 && this.model.module_name.length > applyName.length)) {
            this.$tip.error(`${this.$t('module.namingError')}`);
            return;
        }

        if (this.$refs.detailForm.validate()) {

          const loading = this.$Loading.show();

          try
          {
            const model = this.model;

            const data = await this.$ajax.getJSON('/server/api/has_module_info', model);

            if(!data.hasModule) {
              //全新安装
              const url = '/server/api/add_module_base_info';

              let data = await this.$ajax.postJSON(url, model);

              this.$router.push('/operation/module/moduleConfig/' + data.id)

            } else {
              //模块已经存在
              this.changeUpdate();
              this.showUpdate = true;
            }
          } catch(err) {
            this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
          } finally {
            loading.hide();
          }
        }
      }
    },
    beforeRouteEnter (to, from, next) {
      Ajax.getJSON('/server/api/get_apply_list').then((applys) => {
        if (applys.length) {
          next(vm => {
            applys = applys.sort((a, b) => {
              const a1 = a.name.toLowerCase()
              const b1 = b.name.toLowerCase()
              if(a1 < b1) return -1
              if(a1 > b1) return 1
              return 0
            });
            vm.applys = applys
          })
        } else {
          next(vm => {
            vm.$tip.warning(`${vm.$t('common.warning')}: ${vm.$t('module.createApplyTips')}`);
            vm.$router.push('/operation/apply/createApply')
          })
        }
      }).catch((err) => {
        alert(err.message || err.err_msg);
      });
    }
  }
</script>

<style>

</style>
