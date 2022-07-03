<template>
  <div class="page_server_manage">
    <let-form inline itemWidth="200px" @submit.native.prevent="search">
      <let-form-group>
        <let-form-item :label="$t('monitor.search.a')">
          <let-date-picker
            size="small"
            v-model="query.thedate"
            :formatter="formatter"
          ></let-date-picker>
        </let-form-item>
        <let-form-item :label="$t('monitor.search.b')">
          <let-date-picker
            size="small"
            v-model="query.predate"
            :formatter="formatter"
          ></let-date-picker>
        </let-form-item>
        <let-form-item :label="$t('monitor.search.start')">
          <let-input size="small" v-model="query.startshowtime"></let-input>
        </let-form-item>
        <let-form-item :label="$t('monitor.search.end')">
          <let-input size="small" v-model="query.endshowtime"></let-input>
        </let-form-item>
      </let-form-group>
      <let-form-group>
        <tars-form-item
          :label="$t('module.name')"
          @onLabelClick="groupBy('moduleName')"
        >
          <let-input size="small" v-model="query.moduleName"></let-input>
        </tars-form-item>
        <tars-form-item
          :label="$t('service.serverName')"
          @onLabelClick="groupBy('serverName')"
        >
          <let-select v-model="query.serverName" size="small" filterable>
            <let-option
              v-for="d in cacheServerList"
              :key="d.id"
              :value="d.application + '.' + d.server_name"
            >
              {{ d.application + "." + d.server_name }}
            </let-option>
          </let-select>
        </tars-form-item>
        <let-form-item>
          <let-button size="small" type="submit" theme="primary">{{
            $t("operate.search")
          }}</let-button>
        </let-form-item>
      </let-form-group>
    </let-form>

    <!--    <compare-chart ref="chart" class="chart" v-bind="chartOptions" v-if="showChart"></compare-chart>-->
    <let-row ref="charts" class="charts" v-if="showChart">
      <let-col v-for="d in charts" :key="d.title" :span="12">
        <compare-chart v-bind="d" v-if="allItems.length > 0"></compare-chart>
      </let-col>
    </let-row>

    <hours-filter v-model="hour"></hours-filter>

    <let-table
      ref="table"
      :data="pagedItems"
      :empty-msg="$t('common.nodata')"
      stripe
    >
      <let-table-column
        :title="$t('common.time')"
        prop="show_time"
      ></let-table-column>
      <let-table-column
        :title="$t('module.name')"
        prop="moduleName"
      ></let-table-column>
      <let-table-column
        :title="$t('service.serverName')"
        prop="serverName"
      ></let-table-column>
      <template v-for="item in keys">
        <let-table-column
          :title="
            $t('monitor.table.curr') +
              '/' +
              $t('monitor.table.contrast') +
              ' ' +
              item
          "
          prop="value"
        ></let-table-column>
      </template>
      <let-pagination
        slot="pagination"
        v-if="pageCount"
        :total="pageCount"
        :page="page"
        :sum="itemsCount"
        show-sums
        jump
        @change="changePage"
      ></let-pagination>
    </let-table>
  </div>
</template>

<script>
import { formatDate, ONE_DAY } from "@/lib/date";
import HoursFilter from "@/components/hours-filter";
import CompareChart from "@/components/charts/compare-chart";
import { getCacheServerList, queryProperptyData } from "@/plugins/interface.js";

const pageSize = 20;
const formatter = "YYYYMMDD";
const dataFormatter = (data) => {
  if (data && data.length > 0) {
    return data.map((item) => {
      const result = { ...item };
      const keys = Object.keys(item);
      const preRegex = /^pre_.*/;
      const theRegex = /^the_.*/;
      keys.forEach((key) => {
        if (preRegex.test(key) || theRegex.test(key)) {
          if (item[key] === "--") {
            result[key] = "0";
          }
        }
      });
      return result;
    });
  } else {
    return data;
  }
};

export default {
  name: "ServerPropertyMonitor",

  components: {
    HoursFilter,
    CompareChart,
  },

  data() {
    const treeId = this.treeid;

    return {
      query: {
        thedate: formatDate(new Date(), formatter),
        predate: formatDate(Date.now() - ONE_DAY, formatter),
        startshowtime: "0000",
        endshowtime: "2360",
        moduleName: treeId.split(".")[1].substr(1),
        serverName: "",
      },
      appName: treeId.split(".")[0].substr(1),
      cacheServerList: [],
      formatter,
      allItems: [],
      hour: -1,
      page: 1,
      showChart: true,
      keys: [],
    };
  },

  props: ["treeid"],

  computed: {
    filteredItems() {
      const hour = this.hour;
      return hour >= 0
        ? this.allItems.filter((d) => +d.show_time.slice(0, 2) === hour)
        : this.allItems;
    },
    itemsCount() {
      return this.filteredItems.length;
    },
    pageCount() {
      return Math.ceil(this.filteredItems.length / pageSize);
    },
    pagedItems() {
      return this.filteredItems.slice(
        pageSize * (this.page - 1),
        pageSize * this.page
      );
    },
    chartOptions() {
      return {
        title: this.$t("monitor.table.total"),
        timeColumn: "show_time",
        dataColumns: [
          { name: "the_value", label: this.$t("monitor.property.property") },
          { name: "pre_value", label: this.$t("monitor.property.propertyC") },
        ],
        data: this.allItems,
      };
    },
    charts() {
      return this.keys.map((item) => ({
        title: item,
        timeColumn: "show_time",
        dataColumns: [
          { name: `the_${item}`, label: this.$t("monitor.table.curr") },
          { name: `pre_${item}`, label: this.$t("monitor.table.contrast") },
        ],
        data: this.allItems,
      }));
    },
  },

  mounted() {
    // this.fetchData();
    this.fetchCacheServerList();
  },

  methods: {
    async fetchCacheServerList() {
      try {
        this.cacheServerList = await getCacheServerList({
          appName: this.appName,
          moduleName: this.query.moduleName,
        });
      } catch (err) {
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      }
    },
    async fetchData() {
      const chartLoading =
        this.$refs.charts && this.$refs.charts.$loading.show();
      const tableLoading = this.$refs.table.$loading.show();
      try {
        const {
          query,
          query: { group_by, serverName },
        } = this;
        let option = { ...query };
        if (group_by === "serverName" && serverName === "")
          option = { ...option, serverName: "" };
        const { data, keys } = await queryProperptyData(option);
        // console.log(data, keys);
        this.allItems = data || [];

        this.keys = keys || [];

        this.keys.forEach((k, v) => {
          this.allItems.forEach((item) => {
            item.value = item[`the_${k}`] + "/" + item[`pre_${k}`];
          });
        });
      } catch (err) {
        this.$tip.error(
          `${this.$t("common.error")}: ${err.message || err.err_msg}`
        );
      } finally {
        chartLoading && chartLoading.hide();
        tableLoading.hide();
      }
    },

    groupBy(name) {
      this.query.group_by = name;
      this.showChart = false;

      this.fetchData();
    },

    search() {
      delete this.query.group_by;
      this.showChart = true;
      this.fetchData();
    },

    changePage(page) {
      this.page = page;
    },
  },
};
</script>

<style lang="postcss">
.page_server_manage {
  .chart {
    margin-top: 20px;
  }

  .let-table {
    th {
      word-break: keep-all;
    }
    td {
      word-break: keep-all;
    }
  }

  .charts {
    margin-top: 20px;
  }
}
</style>
