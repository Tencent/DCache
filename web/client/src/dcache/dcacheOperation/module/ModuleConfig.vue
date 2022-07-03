<template>
  <section class="page_operation_module_info">
    <let-form
      inline
      ref="detailForm"
    >
      <let-form-item :label="$t('module.name')" itemWidth="150px" required>
        <let-poptip placement="top" :content="$t('module.namingRule')" trigger="hover">
          <let-input
            disabled
            size="small"
            v-model="moduleInfo.module_name"
            :placeholder="$t('module.namingRule')"
            required
            :required-tip="$t('deployService.table.tips.empty')"
            pattern="^[a-zA-Z][a-zA-Z0-9]+$"
            :pattern-tip="$t('module.namingRule')"
          >
          </let-input>
        </let-poptip>
      </let-form-item>

      <let-form-item :label="$t('module.cacheType')" itemWidth="200px" required>
        <let-select
          size="small"
          :disabled="moduleInfo.update != 0"
          v-model="type"
          required
          :required-tip="$t('deployService.table.tips.empty')"
        >
          <let-option v-for="d in types" :key="d.key" :value="d.key">
            {{d.value}}
          </let-option>
        </let-select>
      </let-form-item>

      <let-form-item :label="$t('cache.perRecordAvg')" itemWidth="200px" required>
        <let-input
          size="small"
          :disabled="moduleInfo.update != 0"
          v-model="model.per_record_avg"
          :placeholder="$t('cache.perRecordAvgUnit')"
          required
          :required-tip="$t('deployService.table.tips.empty')"
        >
        </let-input>
      </let-form-item>

      <let-form-item :label="$t('module.deployArea')" itemWidth="100px">
        <let-select
          size="small"
          required
          :disabled="moduleInfo.update != 0"
          v-model="model.idc_area"
          @change="changeSelect"
        >
          <let-option v-for="d in regions" :key="d.label" :value="d.label">
            {{d.region}}
          </let-option>
        </let-select>
      </let-form-item>
      
      <let-form-item :label="$t('module.openBackup')" itemWidth="100px" required>
        <let-switch v-model="model.open_backup" :disabled="moduleInfo.update != 0" >
          <span slot="1">{{$t('module.openBackup')}}</span>
          <span slot="0">{{$t('module.closeBackup')}}</span>
        </let-switch>
      </let-form-item>

      <let-form-item :label="$t('region.setArea')" itemWidth="200px">
        <let-select
          size="small"
          :disabled="moduleInfo.update != 0 && moduleInfo.update != 3"
          multiple
          v-model="model.set_area"
        >
          <let-option v-for="d in setRegions" :key="d.label" :value="d.label">
            {{d.region}}
          </let-option>
        </let-select>
      </let-form-item>
      <br>

      <let-form-item :label="$t('module.scenario')" itemWidth="240px" required>
        <let-select
          size="small"
          v-model="model.cache_module_type"
          :disabled="moduleInfo.update != 0 && moduleInfo.update != 1"
          required
          :required-tip="$t('deployService.table.tips.empty')"
          @change="changeDbAccessServant"
        >
          <let-option v-for="d in cacheModuleType" :key="d.key" :value="d.key">
            {{$t(d.value)}}
          </let-option>
        </let-select>
      </let-form-item>
      <let-form-item :label="$t('cache.cacheType')" itemWidth="500px" v-if="model.cache_module_type == 1" required>
        <let-radio-group size="small" v-model="model.key_type" :disabled="moduleInfo.update != 0 && moduleInfo.update != 1" required
                         :data="cacheTypeOption">
        </let-radio-group>
      </let-form-item>
 
      <let-form-item :label="$t('cache.dbAccessServantObj')" itemWidth="400px"  v-if="model.cache_module_type == 2" required>
        <let-input
          size="small"
          v-model="model.dbAccessServant"
          :disabled="moduleInfo.update != 0 && moduleInfo.update != 1"
          :placeholder="$t('cache.dbAccessServantObjEx')"
          required
          :required-tip="$t('deployService.table.tips.empty')"
        >
        </let-input>
      </let-form-item>

      <br>

      </div>
      <br> 
      <let-form-item :label="$t('cache.moduleRemark')" itemWidth="516px" required>
        <let-input
          size="small"
          v-model="model.module_remark"
          type="textarea"
          :rows="4"
          required
          :required-tip="$t('deployService.table.tips.empty')"
        >
        </let-input>
      </let-form-item>

      <div>
        <let-button size="small" theme="primary" @click="addModuleConfig">{{$t('common.nextStep')}}</let-button>
      </div>
    </let-form>
  </section>
</template>

<script>

  const getInitialModel = () => ({
    admin: '',
    idc_area: '',
    key_type: null,
    module_name: '',    // 模块名
    cache_module_type: 2,  // 模块存储类型
    key_type: '',   // 存储类型
    dbAccessServant: '',  // 数据库servant obj
    per_record_avg: '100',
    total_record: '1000',
    max_read_flow: '100000',
    max_write_flow: '100000',
    module_remark: '',
    set_area: [],
    module_id: '',
    apply_id: '',
    open_backup: false,
    cache_version: 1,
    mkcache_struct: 0
  });

  const types = [
    {key: "1-0", value: 'key-value(KVCache)'},
    {key: "2-1", value: 'k-k-row(MKVCache)'},
    {key: "2-2", value: 'Set(MKVCache)'},
    {key: "2-3", value: 'List(MKVCache)'},
    {key: "2-4", value: 'Zset(MKVCache)'},
  ];

  const cacheModuleType = [
    { key: 1, value: 'cache.title' },
    { key: 2, value: 'cache.cachePersistent' },
  ];
  const keyTypeOption = [
    { key: '0', value: 'string' },
    // { key: '1', value: 'int' },
    // { key: '2', value: 'longlong' },
  ];
  import Ajax from '@/plugins/ajax'

  export default {
    data() {
      const cacheTypeOption = [
        { value: '1', text: this.$t('cache.cacheTypeTip1') },
        { value: '2', text: this.$t('cache.cacheTypeTip2') },
        { value: '3', text: this.$t('cache.cacheTypeTip3') },
      ];
      return {
        cacheTypeOption,
        keyTypeOption,
        cacheModuleType,
        regions: [],
        setRegions: [],
        types,
        moduleInfo: { update: 0 },
        model: getInitialModel()
      }
    },
    computed: {
      type: {
        get () {
          if (!this.model.cache_version) return '';
          return this.model.cache_version + '-' + this.model.mkcache_struct
        },
        set (type) {
          let cacheVersionArr = type.split('-');
          this.model.cache_version = cacheVersionArr[0];
          this.model.mkcache_struct = cacheVersionArr[1];
        }
      }
    },
    methods: {
      addModuleConfig() {

        if(this.moduleInfo.update == 3) {
          if(this.model.set_area.length == 0) {
            this.$tip.error(`${this.$t('common.error')}: ${this.$t('module.chooseMirror')}`);
            return;
          }
        }
        if (this.$refs.detailForm.validate()) {

          const model = this.model;
          const loading = this.$Loading.show();
          this.$ajax.postJSON('/server/api/overwrite_module_config', model).then((data) => {
            loading.hide();

            this.$router.push('/operation/module/serverConfig/' + data.module_id)
          }).catch((err) => {

            loading.hide();

            this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
            this.$router.push('/operation/module/serverConfig/' + data.module_id)
          });
        }
      },
      changeDbAccessServant() {
        if(this.model.cache_module_type == 1) {
          this.model.dbAccessServant = "";
        } else {
          this.model.dbAccessServant = "DCache." + this.moduleInfo.module_name + "DbAccessServer.DbAccessObj";
        }
      },
      changeSelect() {
        
        if(this.moduleInfo.update != 3) {
          let setRegions = this.regions.concat();
          setRegions.splice(setRegions.indexOf(setRegions.find(item => item.label == this.model.idc_area)), 1);
          this.setRegions = setRegions;
        } else {
          //安装镜像, 将使用过的去掉
          let area = this.model.set_area.slice(0);  //clone
          area.push(this.model.idc_area);

          let setRegions = this.regions.concat();

          setRegions = setRegions.filter(item=>{
            for(var i = 0; i < area.length; i++) {
              if(area[i] == item.label) {
                return false;
              } 
            }

            return true;
          });

          this.model.set_area = [];

          this.setRegions = setRegions;
        }
      },
      async getRegionList() {

        try {
          this.regions = await this.$ajax.getJSON('/server/api/get_region_list');
        }catch(err) {
          this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
        };
      },
      async getModuleInfo() {
        try {
          let { moduleId }  = this.$route.params;

          this.moduleInfo   = await this.$ajax.getJSON('/server/api/get_module_info', { moduleId });

          if(this.moduleInfo.update != 0) {
            //升级安装, 拿到之前安装的数据
            this.model = await this.$ajax.getJSON('/server/api/get_module_config_info_by_module_name', { module_name : this.moduleInfo.module_name });
          }
          this.model.apply_id   = this.moduleInfo.apply_id;
          this.model.module_id  = this.moduleInfo.id;
          this.model.module_name= this.moduleInfo.module_name;

          this.changeDbAccessServant();

          if(this.regions) {
            this.model.idc_area = this.regions[0].label;
          }
        }catch(err) {
          this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
        };
      },
    },
    async created() {
      await this.getRegionList();
      this.getModuleInfo();
    }
  }
</script>

<style lang="postcss">
  .let-form-cols-1 .let-form-item__label {
    width: auto;
  }
  .page_operation_module_info {
    .poptip__popper {
      max-width: 420px;
    }

    .let-poptip {
      display: block;
    }

    .let-poptip__trigger {
      display: block;
    }

    .let-poptip_top {
      width: auto !important;
      padding: 4px 8px;
      background: #f56c77;
      color: white;
      border-radius: 6px;
      top: -25px !important;

      .let-poptip__arrow:after {
        border-top-color: #f56c77;
      }

      .let-poptip__content {
        font-size: 14px;
        white-space: nowrap;
      }
    }
  }
</style>
