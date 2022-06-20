<template>
  <section class="page_operation">
    <div>
      <let-button theme="primary" size="small" @click="addConfig">{{$t('cache.config.addConfig')}}</let-button>
    </div>
    
    <let-table :data="list" :title="$t('cache.config.tableTitle')" :empty-msg="$t('common.nodata')">
      <let-table-column title="ID" prop="id"></let-table-column>
      <let-table-column :title="$t('cache.config.remark')" prop="remark"></let-table-column>
      <let-table-column :title="$t('cache.config.path')" prop="path"></let-table-column>
      <let-table-column :title="$t('cache.config.item')" prop="item"></let-table-column>
      <let-table-column :title="$t('cache.config.reload')" prop="reload"></let-table-column>
      <let-table-column :title="$t('cache.config.period')" prop="period"></let-table-column>
      <let-table-column :title="$t('operate.operates')">
        <template slot-scope="{row}">
          <let-table-operation @click="editConfig(row)">{{$t('operate.update')}}</let-table-operation>
          <let-table-operation @click="deleteConfig(row)" class="danger">{{$t('operate.delete')}}</let-table-operation>
        </template>
      </let-table-column>
    </let-table>
    <let-modal
      v-model="addConfigVisible"
      :footShow="false"
      :closeOnClickBackdrop="true"
    >
      <add-config v-if="addConfigVisible" @call-back="getConfig"></add-config>
    </let-modal>
    <let-modal
      v-model="editConfigVisible"
      :footShow="false"
      :closeOnClickBackdrop="true"
    >
      <edit-config v-if="editConfigVisible" v-bind="editConfigObj" @call-back="getConfig"></edit-config>
    </let-modal>
  </section>
</template>

<script>
  import AddConfig from './add.vue'
  import EditConfig from './edit.vue'
  export default {
    components: {
      AddConfig,
      EditConfig
    },
    data () {
      return {
        list: [],
        addConfigVisible: false,
        editConfigVisible: false,
        editConfigObj: null,
      }
    },
    methods: {
      addConfig () {
        this.addConfigVisible = true
      },
      editConfig (config) {
        this.editConfigVisible = true;
        this.editConfigObj = config
      },
      deleteConfig ({id}) {
        this.$confirm(this.$t('cache.config.deleteConfig'), this.$t('common.alert')).then(async () => {
          try {
            let configItemList = await this.$ajax.getJSON('/server/api/cache/deleteConfig', {id});
            await this.getConfig();
          } catch (err) {
            console.error(err)
            this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
          }
        });
      },
      async getConfig () {
        try {
          let configItemList = await this.$ajax.getJSON('/server/api/cache/getConfig');
          this.list = configItemList;
        } catch (err) {
          console.error(err)
          this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
        }
      }
    },
    async created () {
      this.getConfig();
    }
  }
</script>

<style>
  .page_operation{overflow:auto;}
</style>
