<template>
  <section class="page_operation_create_service">
    <let-form inline ref="detailForm">
      <let-form-group
        :title="$t('module.serverInfo')"
        inline
        label-position="top"
        v-if="moduleData.length > 0"
      >
        <let-table
          ref="table"
          :data="moduleData"
          :empty-msg="$t('common.nodata')"
        >
          <let-table-column :title="$t('module.name')" prop="module_name">
            <template slot-scope="scope">
              {{ scope.row.module_name }}
            </template>
          </let-table-column>
          <let-table-column :title="$t('module.serverGroup')" prop="group_name">
            <template slot-scope="scope">
              {{ scope.row.group_name }}
            </template>
          </let-table-column>
          <let-table-column
            :title="$t('service.serverName')"
            prop="server_name"
          >
            <template slot-scope="scope">
              {{ scope.row.server_name }}
            </template>
          </let-table-column>
          <let-table-column
            :title="$t('deployService.form.serviceType')"
            prop="server_type"
          >
            <template slot-scope="scope">
              {{ mapServerType(scope.row.server_type) }}
            </template>
          </let-table-column>
          <let-table-column :title="$t('service.serverIp')" prop="server_ip">
            <template slot-scope="scope">
              <let-select v-model="scope.row.server_ip" required size="small">
                <let-option v-for="d in nodeList" :key="d" :value="d">
                  {{ d }}
                </let-option>
              </let-select>
            </template>
          </let-table-column>
          <let-table-column :title="$t('module.shmKey')" prop="shmKey">
            <template slot-scope="scope">
              <let-input
                size="small"
                v-model="scope.row.shmKey"
                :placeholder="$t('module.shmKeyRule')"
              />
            </template>
          </let-table-column>
          <let-table-column :title="$t('deployService.form.template')">
            <template slot-scope="scope">
              <let-select
                size="small"
                v-model="scope.row.template_name"
                required
                :required-tip="$t('deployService.form.templateTips')"
              >
                <let-option v-for="d in templates" :key="d" :value="d">{{
                  d
                }}</let-option>
              </let-select>
            </template>
          </let-table-column>
          <let-table-column :title="$t('module.memorySize')" prop="memory">
            <template slot-scope="scope">
              <let-input
                size="small"
                v-model="scope.row.memory"
                :placeholder="$t('module.memorySize')"
                required
                :required-tip="$t('deployService.table.tips.empty')"
                style="width: 60px"
              />
            </template>
          </let-table-column>
          <let-table-column :title="$t('module.deployArea')" prop="area">
            <template slot-scope="scope">
              {{ scope.row.area }}
            </template>
          </let-table-column>
        </let-table>
      </let-form-group>

      <div v-if="isDbAccess()">
        <let-form-group
          :title="$t('module.dbAccessInfo') + '(' + dbAccess.servant + ')'"
          inline
          label-position="top"
        >
          <let-form-item
            :label="$t('service.isSerializated')"
            itemWidth="400px"
            v-if="this.cacheVersion == 1"
            required
          >
            <let-radio-group
              size="small"
              v-model="dbAccess.isSerializated"
              :data="cacheTypeOption"
            >
            </let-radio-group>
          </let-form-item>

          <br />
          <let-form-item
            :label="$t('service.multipleIp')"
            itemWidth="350px"
            required
          >
            <let-select
              v-model="dbAccess.dbaccess_ip"
              size="small"
              required
              multiple
              placeholder="Please Choose"
            >
              <let-option v-for="d in nodeList" :key="d" :value="d">
                {{ d }}
              </let-option>
            </let-select>
          </let-form-item>

          <br />
          <let-form-item
            :label="$t('cache.db.DBPrefix')"
            itemWidth="200px"
            required
          >
            <let-input
              size="small"
              v-model="dbAccess.db_prefix"
              required
              :required-tip="$t('deployService.table.tips.empty')"
            >
            </let-input>
          </let-form-item>

          <let-form-item
            :label="$t('cache.db.tablePrefix')"
            itemWidth="200px"
            required
          >
            <let-input
              size="small"
              v-model="dbAccess.table_prefix"
              required
              :required-tip="$t('deployService.table.tips.empty')"
            >
            </let-input>
          </let-form-item>

          <let-form-item
            :label="$t('cache.db.dbNum')"
            itemWidth="200px"
            required
          >
            <let-select
              size="small"
              v-model="dbAccess.db_num"
              required
              :required-tip="$t('deployService.table.tips.empty')"
            >
              <let-option key="1" value="1"> 1 </let-option>
              <let-option key="10" value="10"> 10 </let-option>
            </let-select>
          </let-form-item>

          <let-form-item
            :label="$t('cache.db.tableNum')"
            itemWidth="200px"
            required
          >
            <let-select
              size="small"
              v-model="dbAccess.table_num"
              required
              :required-tip="$t('deployService.table.tips.empty')"
            >
              <let-option key="1" value="1"> 1 </let-option>
              <let-option key="10" value="10"> 10 </let-option>
            </let-select>
          </let-form-item>

          <let-form-item
            :label="$t('cache.db.tableCharset')"
            itemWidth="200px"
            required
          >
            <let-select
              size="small"
              v-model="dbAccess.db_charset"
              required
              :required-tip="$t('deployService.table.tips.empty')"
            >
              <let-option key="utf8mb4" value="utf8mb4">utf8mb4</let-option>
              <let-option key="utf8" value="utf8">utf8</let-option>
              <let-option key="gbk" value="gbk">gbk</let-option>
              <let-option key="latin1" value="latin1">latin1</let-option>
            </let-select>
          </let-form-item>

          <div v-if="accessDb.length > 0">
            <let-form-item
              :label="$t('cache.accessDbName')"
              itemWidth="240px"
              required
            >
              <let-radio v-model="dbAccess.dbMethod" :label="true">{{
                $t("cache.chooseAccessDb")
              }}</let-radio>
            </let-form-item>
            <let-form-item itemWidth="200px" v-if="dbAccess.dbMethod">
              <let-select v-model="dbAccess.accessDbId" size="small" required>
                <let-option v-for="d in accessDb" :key="d.id" :value="d.id">
                  {{ d.access_db_flag }}
                </let-option>
              </let-select>
            </let-form-item>
          </div>

          <br />
          <let-form-item
            :label="$t('cache.accessDbName')"
            itemWidth="240px"
            required
          >
            <let-radio v-model="dbAccess.dbMethod" :label="false">{{
              $t("cache.inputAccessDb")
            }}</let-radio>
          </let-form-item>
          <br />

          <span v-if="!dbAccess.dbMethod">
            <let-form-item
              :label="$t('cache.db.dbHost')"
              itemWidth="200px"
              required
            >
              <let-input
                size="small"
                v-model="dbAccess.db_host"
                required
                :required-tip="$t('deployService.table.tips.empty')"
              >
              </let-input>
            </let-form-item>
            <let-form-item
              :label="$t('cache.db.dbUser')"
              itemWidth="200px"
              required
            >
              <let-input
                size="small"
                v-model="dbAccess.db_user"
                required
                :required-tip="$t('deployService.table.tips.empty')"
              >
              </let-input>
            </let-form-item>
            <let-form-item
              :label="$t('cache.db.dbPass')"
              itemWidth="200px"
              required
            >
              <let-input
                size="small"
                v-model="dbAccess.db_pwd"
                required
                :required-tip="$t('deployService.table.tips.empty')"
              >
              </let-input>
            </let-form-item>
            <let-form-item
              :label="$t('cache.db.dbPort')"
              itemWidth="200px"
              required
            >
              <let-input
                size="small"
                v-model="dbAccess.db_port"
                required
                :required-tip="$t('deployService.table.tips.empty')"
              >
              </let-input>
            </let-form-item>
          </span>
        </let-form-group>
      </div>
      <div>
        <let-button size="small" theme="primary" @click="submitServerConfig">{{
          $t("common.nextStep")
        }}</let-button>
      </div>
    </let-form>

    <let-modal
      :title="$t('module.MultiKeyConfig')"
      v-model="showMKModal"
      width="1000px"
      @on-confirm="submitMKCache"
    >
      <let-form label-position="top" ref="multiKeyForm">
        <let-form-group
          :title="$t('MKCache.mainKey')"
          inline
          label-position="top"
        >
          <let-table
            ref="mainKey"
            :data="mkCacheStructure.mainKey"
            :empty-msg="$t('common.nodata')"
          >
            <let-table-column
              :title="$t('MKCache.fieldName')"
              prop="fieldName"
              width="150"
            >
              <template slot-scope="scope">
                <let-input
                  size="small"
                  v-model="scope.row.fieldName"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                />
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.dataType')"
              prop="dataType"
              width="120"
            >
              <template slot-scope="scope">
                <let-select
                  size="small"
                  v-model="scope.row.dataType"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                >
                  <let-option
                    v-for="d in keyTypeOption"
                    :key="d.key"
                    :value="d.value"
                  >
                    {{ d.value }}
                  </let-option>
                </let-select>
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.dataType')"
              prop="dataType"
              width="150"
            >
              <template slot-scope="scope">
                <let-select
                  size="small"
                  v-model="scope.row.DBType"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                >
                  <let-option
                    v-for="d in keyDbTypeOption"
                    :key="d.key"
                    :value="d.value"
                  >
                    {{ d.value }}
                  </let-option>
                </let-select>
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.fieldProperty')"
              prop="property"
              width="150"
            >
              <template slot-scope="scope">
                {{ scope.row.property }}
              </template>
            </let-table-column>
            <!--
            <let-table-column :title="$t('MKCache.defaultValue')" prop="defaultValue" width="15%">
              <template slot-scope="scope">
                <let-input
                  size="small"
                  v-model="scope.row.defaultValue"

                />
              </template>
            </let-table-column>
 
            <let-table-column :title="$t('MKCache.maxLen')" prop="maxLen">
              <template slot-scope="scope">
                <let-input
                  size="small"
                  v-model="scope.row.maxLen"
                />
              </template>
            </let-table-column>
            -->
          </let-table>
        </let-form-group>

        <let-form-group
          :title="$t('MKCache.unionKey')"
          inline
          label-position="top"
          v-if="multiKey"
        >
          <let-table
            ref="unionKey"
            :data="mkCacheStructure.unionKey"
            :empty-msg="$t('common.nodata')"
          >
            <let-table-column
              :title="$t('MKCache.fieldName')"
              prop="fieldName"
              width="150"
            >
              <template slot-scope="scope">
                <let-input
                  size="small"
                  v-model="scope.row.fieldName"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                />
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.dataType')"
              prop="dataType"
              width="150"
            >
              <template slot-scope="scope">
                <let-select
                  size="small"
                  v-model="scope.row.dataType"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                >
                  <let-option
                    v-for="d in keyTypeOption"
                    :key="d.key"
                    :value="d.value"
                  >
                    {{ d.value }}
                  </let-option>
                </let-select>
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.dataDbType')"
              prop="DBType"
              width="150"
            >
              <template slot-scope="scope">
                <let-select
                  size="small"
                  v-model="scope.row.DBType"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                >
                  <let-option
                    v-for="d in keyDbTypeOption"
                    :key="d.key"
                    :value="d.value"
                  >
                    {{ d.value }}
                  </let-option>
                </let-select>
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.fieldProperty')"
              prop="property"
              width="150"
            >
              <template slot-scope="scope">
                {{ scope.row.property }}
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.defaultValue')"
              prop="defaultValue"
              width="150"
            >
              <template slot-scope="scope">
                <let-input size="small" v-model="scope.row.defaultValue" />
                <!--required-->
                <!--:required-tip="$t('deployService.table.tips.empty')"-->
              </template>
            </let-table-column>
            <!--
            <let-table-column :title="$t('MKCache.maxLen')" prop="maxLen">
              <template slot-scope="scope">
                <let-input
                  size="small"
                  v-model="scope.row.maxLen"
                />
              </template>
            </let-table-column>
            -->
            <let-table-column :title="$t('operate.operates')" width="15%">
              <template slot-scope="scope">
                <let-table-operation @click="addUnionKey">{{
                  $t("operate.add")
                }}</let-table-operation>
                <let-table-operation @click="deleteUnionKey(scope.$index)">{{
                  $t("operate.delete")
                }}</let-table-operation>
              </template>
            </let-table-column>
          </let-table>
        </let-form-group>

        <let-form-group
          :title="$t('MKCache.dataValue')"
          inline
          label-position="top"
        >
          <let-table
            ref="dataValue"
            :data="mkCacheStructure.value"
            :empty-msg="$t('common.nodata')"
          >
            <let-table-column
              :title="$t('MKCache.fieldName')"
              prop="fieldName"
              width="20%"
            >
              <template slot-scope="scope">
                <let-input
                  size="small"
                  v-model="scope.row.fieldName"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                />
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.dataType')"
              prop="dataType"
              width="15%"
            >
              <template slot-scope="scope">
                <let-select
                  size="small"
                  v-model="scope.row.dataType"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                >
                  <let-option
                    v-for="d in dataTypeOption"
                    :key="d.key"
                    :value="d.value"
                  >
                    {{ d.value }}
                  </let-option>
                </let-select>
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.dataDbType')"
              prop="DBType"
              width="15%"
            >
              <template slot-scope="scope">
                <let-select
                  size="small"
                  v-model="scope.row.DBType"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                >
                  <let-option
                    v-for="d in dataDbTypeOption"
                    :key="d.key"
                    :value="d.value"
                  >
                    {{ d.value }}
                  </let-option>
                </let-select>
              </template>
            </let-table-column>

            <let-table-column
              :title="$t('MKCache.fieldProperty')"
              prop="property"
              width="15%"
            >
              <template slot-scope="scope">
                <let-select
                  size="small"
                  v-model="scope.row.property"
                  required
                  :required-tip="$t('deployService.table.tips.empty')"
                >
                  <let-option
                    v-for="d in propertyOption"
                    :key="d.key"
                    :value="d.value"
                  >
                    {{ d.value }}
                  </let-option>
                </let-select>
              </template>
            </let-table-column>
            <let-table-column
              :title="$t('MKCache.defaultValue')"
              prop="defaultValue"
              width="15%"
            >
              <template slot-scope="scope">
                <let-input
                  size="small"
                  v-model="scope.row.defaultValue"
                  :required-tip="$t('deployService.table.tips.empty')"
                />
              </template>
            </let-table-column>
            <!--
            <let-table-column :title="$t('MKCache.maxLen')" prop="maxLen">
              <template slot-scope="scope">
                <let-input
                  size="small"
                  v-model="scope.row.maxLen"
                  :required-tip="$t('deployService.table.tips.empty')"
                />
              </template>
            </let-table-column>
            -->
            <let-table-column
              :title="$t('operate.operates')"
              width="15%"
              v-if="dbAccess.isSerializated"
            >
              <template slot-scope="scope">
                <let-table-operation @click="addValue">{{
                  $t("operate.add")
                }}</let-table-operation>
                <let-table-operation @click="deleteValue(scope.$index)">{{
                  $t("operate.delete")
                }}</let-table-operation>
              </template>
            </let-table-column>
          </let-table>
        </let-form-group>
      </let-form>
    </let-modal>
  </section>
</template>

<script>
import _ from "lodash";
import {
  getModuleConfigInfo,
  getModuleGroup,
  // templateNameList,
} from "@/plugins/interface.js";

const getDBAccessConf = () => ({
  module_id: "",
  accessDbId: 0,
  dbMethod: true,
  isSerializated: false,
  dbaccess_ip: [],
  servant: "",
  db_num: "1",
  db_prefix: "db_",
  table_num: "1",
  table_prefix: "t_",
  db_charset: "utf8mb4",
  db_host: "",
  db_user: "",
  db_pwd: "",
  db_port: "3306",
});

const moduleModel = () => ({
  module_id: 17,
  module_name: "",
  group_name: "",
  server_name: "",
  server_ip: "",
  idc_area: "",
  service_type: "",
  memory: "",
});

const keyTypeOption = [{ key: "string", value: "string" }];

const dataTypeOption = [
  { key: "int", value: "int" },
  { key: "long", value: "long" },
  { key: "string", value: "string" },
  { key: "byte", value: "byte" },
  { key: "float", value: "float" },
  { key: "double", value: "double" },
];

const keyDbTypeOption = [
  { key: "varchar(64)", value: "varchar(64)" },
  { key: "varchar(128)", value: "varchar(128)" },
  { key: "varchar(180)", value: "varchar(180)" },
  { key: "varchar(255)", value: "varchar(255)" },
  { key: "varchar(1000)", value: "varchar(1000)" },
];

const dataDbTypeOption = [
  { key: "int", value: "int" },
  { key: "bigint", value: "bigint" },
  { key: "varchar(128)", value: "varchar(128)" },
  { key: "text", value: "MEDIUMTEXT" },
  { key: "blob", value: "MEDIUMBLOB" },
  { key: "float", value: "float" },
  { key: "double", value: "double" },
];

const propertyOption = [
  { key: "require", value: "require" },
  { key: "optional", value: "optional" },
];

export default {
  data() {
    const cacheTypeOption = [
      { value: false, text: this.$t("cache.sTypeTip1") },
      { value: true, text: this.$t("cache.sTypeTip2") },
    ];

    let { moduleId } = this.$route.params;
    return {
      moduleId,
      accessDb: [],
      moduleInfo: { ModuleBase: { update: 0 } },
      dbAccess: getDBAccessConf(),
      moduleData: [],
      nodeList: [],
      templates: [],
      cacheVersion: 1,
      isMkCache: false,
      multiKey: false,
      cacheTypeOption,
      keyTypeOption,
      keyDbTypeOption,
      dataDbTypeOption,
      dataTypeOption,
      propertyOption,
      showMKModal: false,
      mkCacheStructure: {
        mainKey: [
          {
            fieldName: "USERID",
            keyType: "mkey",
            dataType: "string",
            DBType: "varchar(128)",
            property: "require",
            defaultValue: "",
            maxLen: "100",
          },
        ],
        unionKey: [
          {
            fieldName: "",
            keyType: "ukey",
            dataType: "string",
            DBType: "varchar(128)",
            property: "require",
            defaultValue: "",
            maxLen: "",
          },
        ],
        value: [
          {
            fieldName: "VALUE",
            keyType: "value",
            dataType: "string",
            DBType: "MEDIUMBLOB",
            property: "require",
            defaultValue: "",
            maxLen: "",
          },
        ],
      },
    };
  },
  mounted() {
    this.$tars
      .getJSON("/server/api/node_list")
      .then((data) => {
        this.nodeList = data;
      })
      .catch((err) => {
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      });
  },
  methods: {
    isNewInstall() {
      return this.moduleInfo.ModuleBase.update == 0;
    },
    isDbAccess() {
      return (
        this.dbAccess.servant != "" &&
        (this.moduleInfo.ModuleBase.update == 0 ||
          this.moduleInfo.ModuleBase.update == 1)
      );
    },
    isBackup() {
      return this.moduleInfo.ModuleBase.update == 2;
    },
    isMirror() {
      return this.moduleInfo.ModuleBase.update == 3;
    },
    submitServerConfig() {
      if (this.dbAccess.dbMethod == undefined) {
        this.$tip.error(`${this.$t("module.dbMethod")}`);
        return;
      }
      if (this.$refs.detailForm.validate()) {
        if (this.isMkCache) {
          if (this.isDbAccess() && this.dbAccess.dbaccess_ip.length == 0) {
            this.$tip.error(`${this.$t("cache.dbaccessIp")}`);
            return;
          }

          this.showMKModal = true;
        } else if (this.checkSameShmKey(this.moduleData)) {
          this.addServerConfig();
        } else {
          this.$tip.error(this.$t("module.shmKeyError"));
        }
      }
    },
    async loadAccessDb() {
      return this.$ajax
        .getJSON("/server/api/load_access_db")
        .then((data) => {
          this.accessDb = data;

          if (data.length > 0) {
            this.dbAccess.dbMethod = true;
            this.dbAccess.accessDbId = data[0].id;
          } else {
            this.dbAccess.dbMethod = false;
          }
        })
        .catch((err) => {
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    checkSameShmKey(data) {
      return true;
    },
    addServerConfig() {
      const moduleData = this.moduleData;
      const dbAccess = this.dbAccess;

      if (this.isDbAccess() && this.dbAccess.dbaccess_ip.length == 0) {
        this.$tip.error(`${this.$t("cache.dbaccessIp")}`);
        return;
      }

      const url = "/server/api/add_server_config";
      const loading = this.$Loading.show();
      this.$ajax
        .postJSON(url, { moduleData, dbAccess })
        .then((data) => {
          loading.hide();
          this.$tip.success(this.$t("common.success"));
          this.$router.push(
            "/operation/module/installAndPublish/" + this.moduleId
          );
        })
        .catch((err) => {
          loading.hide();
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },
    async templateNameList() {
      let data = await this.$tars.getJSON("/server/api/template_name_list");

      return data;
    },
    async getModuleConfigInfo() {
      try {
        //  加载模版
        let templates = await this.templateNameList();
        this.templates = templates;

        let { moduleId } = this.$route.params;
        let data = await getModuleConfigInfo({ moduleId });

        this.moduleInfo = data;

        let module_name = data.module_name;

        let groupInfo = await getModuleGroup({ module_name });

        var count = 0;
        for (var i in groupInfo) {
          if (groupInfo.hasOwnProperty(i)) {
            count++;
          }
        }

        data.area = data.idc_area;

        let cacheVersion = data.cache_version === 1 ? "KV" : "MKV";
        data.group_name = data.module_name + cacheVersion + "Group1";
        data.server_name = data.module_name + cacheVersion + "CacheServer1-1";
        data.server_type = 0;
        data.memory = 50;

        if (count == 0) {
          if (this.isNewInstall()) {
            //new install
            this.moduleData.push({ ...data });
          }

          if (this.isBackup() || (this.isNewInstall() && data.open_backup)) {
            //new install or iinstall backup
            let backData = {
              ...data,
              server_name: data.module_name + cacheVersion + "CacheServer1-2",
              server_type: 1,
            };
            this.moduleData.push(backData);
          }

          if (this.isMirror() || this.isNewInstall()) {
            //new install or install mirror
            if (data.set_area.length > 0) {
              data.set_area.forEach((item, index) => {
                let mirItem = {
                  ...data,
                  area: item,
                  server_name:
                    data.module_name +
                    cacheVersion +
                    "CacheServer1-" +
                    (index + 3),
                  server_type: 2,
                };
                this.moduleData.push(mirItem);
              });
            }
          }
        } else {
          if (this.isNewInstall()) {
            for (var group in groupInfo) {
              for (var i = 0; i < groupInfo[group].length; i++) {
                let cache = groupInfo[group][i];
                let cacheData = {
                  ...data,
                  group_name: data.module_name + cacheVersion + "Group" + group,
                  server_name: cache.server_name,
                  server_type: cache.server_type,
                };

                //new install
                this.moduleData.push(cacheData);
              }
            }
          } else if (this.isBackup()) {
            for (var group in groupInfo) {
              var hasBackup = false;

              for (var i = 0; i < groupInfo[group].length; i++) {
                if (groupInfo[group][i].server_type == 1) {
                  hasBackup = true;
                  break;
                }
              }

              for (var i = 0; i < groupInfo[group].length; i++) {
                let cache = groupInfo[group][i];
                let cacheData = {
                  ...data,
                  group_name: data.module_name + cacheVersion + "Group" + group,
                  server_name: cache.server_name,
                  server_type: cache.server_type,
                };

                if (hasBackup && cache.server_type == 1) {
                  this.moduleData.push(cacheData);
                } else if (!hasBackup) {
                  let backData = {
                    ...cacheData,
                    server_name:
                      cacheData.module_name +
                      cacheVersion +
                      "CacheServer" +
                      group +
                      "-2",
                    server_type: 1,
                  };

                  this.moduleData.push(backData);
                }
              }
            }
          } else if (this.isMirror()) {
            for (var group in groupInfo) {
              let cache = groupInfo[group][0];
              let cacheData = {
                ...data,
                group_name: data.module_name + cacheVersion + "Group" + group,
                server_name: cache.server_name,
                server_type: cache.server_type,
              };

              if (data.set_area.length > 0) {
                data.set_area.forEach((item, index) => {
                  let mirItem = {
                    ...cacheData,
                    area: item,
                    group_name:
                      data.module_name + cacheVersion + "Group" + group,
                    server_name:
                      cacheData.module_name +
                      cacheVersion +
                      "CacheServer" +
                      group +
                      "-" +
                      (index + 3),
                    server_type: 2,
                  };
                  this.moduleData.push(mirItem);
                });
              }
            }
          }
        }

        // 设置 cache 服务默认 DCache.Cache 模版
        this.moduleData = this.moduleData.map((item) => ({
          ...item,
          template_name: templates.includes("DCache.Cache")
            ? "DCache.Cache"
            : "tars.cpp.default",
        }));

        // 一期持久化或者二期Cache 都需要填写数据结构
        this.isMkCache = data.cache_version === 2; // || data.cache_module_type === 2;
        //多key的情况
        this.multiKey = data.cache_version === 2 && data.mkcache_struct === 1;

        this.cacheVersion = data.cache_version;
        if (data.cache_version === 1) {
          sessionStorage.setItem(
            "mkCache",
            JSON.stringify(this.mkCacheStructure)
          );
        }
        if (this.multiKey) {
          this.dbAccess.isSerializated = true;
        }
        this.dbAccess.module_id = this.moduleId;
        this.dbAccess.db_prefix = "db_" + data.module_name + "_";
        this.dbAccess.table_prefix = "t_" + data.module_name + "_";
        this.dbAccess.servant = data.dbAccessServant;
      } catch (err) {
        // console.error(err);
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      }
    },
    mapServerType(key) {
      if (key === 0) return this.$t("module.mainServer");
      else if (key === 1) return this.$t("module.backServer");
      else return this.$t("module.mirror");
    },
    addUnionKey() {
      this.mkCacheStructure.unionKey.push({
        fieldName: "",
        keyType: "ukey",
        dataType: "",
        property: "require",
        DBType: "",
        defaultValue: "",
        maxLen: "",
      });
    },
    deleteUnionKey(index) {
      if (this.mkCacheStructure.unionKey.length > 1) {
        this.mkCacheStructure.unionKey.splice(index, 1);
      } else {
        this.$tip.error(this.$t("MKCache.error"));
      }
    },
    addValue() {
      this.mkCacheStructure.value.push({
        fieldName: "",
        keyType: "value",
        dataType: "",
        DBType: "",
        property: "",
        defaultValue: "",
        maxLen: "",
      });
    },
    deleteValue(index) {
      if (this.mkCacheStructure.value.length > 1) {
        this.mkCacheStructure.value.splice(index, 1);
      } else {
        this.$tip.error(this.$t("MKCache.error"));
      }
    },
    submitMKCache() {
      if (this.dbAccess.dbMethod == undefined) {
        this.$tip.error(`${this.$t("module.dbMethod")}`);
        return;
      }
      if (this.$refs.multiKeyForm.validate()) {
        sessionStorage.setItem(
          "mkCache",
          JSON.stringify(this.mkCacheStructure)
        );
        this.showMKModal = false;
        document.body.classList.remove("has-modal-open");
        this.addServerConfig();
      }
    },
  },
  async created() {
    sessionStorage.clear();
    await this.getModuleConfigInfo();
    if (this.isDbAccess()) {
      await this.loadAccessDb();
    }
  },
};
</script>
