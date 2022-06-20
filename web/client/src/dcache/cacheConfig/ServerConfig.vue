<template>
  <section>
    <let-table :data="configList" :empty-msg="$t('common.nodata')">
      <let-table-column :title="$t('cache.config.remark')" prop="remark"></let-table-column>
      <let-table-column :title="$t('cache.config.path')" prop="path"></let-table-column>
      <let-table-column :title="$t('cache.config.item')" prop="item"></let-table-column>
      <let-table-column :title="$t('cache.config.value')" prop="config_value"></let-table-column>
      <let-table-column :title="$t('cache.config.modify_value')" prop="config_value">
        <template slot-scope="{row}">
          <let-input size="small" v-model="row.modify_value"></let-input>
        </template>
      </let-table-column>
      <let-table-column :title="$t('operate.operates')">
        <template slot-scope="{row}">
          <let-table-operation @click="saveConfig(row)">{{$t('operate.save')}}</let-table-operation>
          <let-table-operation @click="deleteConfig(row)" class="danger">{{$t('operate.delete')}}</let-table-operation>
        </template>
      </let-table-column>
    </let-table>
  </section>
</template>

<script>
  export default {
    props: {
      serverName: {type: String, required: true},
      nodeName: {type: String, required: true},
    },
    data () {
      return {
        configList: []
      }
    },
    methods: {
      async getServerConfig () {
        try {
          let option = {
            serverName: this.serverName,
            nodeName: this.nodeName,
          };
          let configItemList = await this.$ajax.getJSON('/server/api/cache/getServerNodeConfig', option);
          // 添加被修改的空值
          configItemList.forEach(item => item.modify_value="");
          this.configList = configItemList;
        } catch (err) {
          // console.error(err)
          this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
        }
      },
      deleteConfig ({id}) {
        this.$confirm(this.$t('cache.config.deleteConfig'), this.$t('common.alert')).then(async () => {
          try {
            let configItemList = await this.$ajax.getJSON('/server/api/cache/deleteServerConfigItem', {id});
            await this.getServerConfig();
          } catch (err) {
            // console.error(err)
            this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
          }
        });
      },
      async saveConfig ({id, modify_value}) {
        try {
          let configItemList = await this.$ajax.getJSON('/server/api/cache/updateServerConfigItem', {id, configValue: modify_value});
          await this.getServerConfig();
        } catch (err) {
          // console.error(err)
          this.$tip.error(`${this.$t('common.error')}: ${err.message || err.err_msg}`);
        }
      }
    },
    created () {
      this.getServerConfig();
    }
  }
</script>

<style>

</style>
