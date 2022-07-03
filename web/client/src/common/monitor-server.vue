<template>
  <div class="page_server_server_monitor">
    <!-- <let-radio v-model="query.userpc" :label="0">JSON查询</let-radio>
    <let-radio v-model="query.userpc" :label="1">RPC查询</let-radio> -->
    <let-form inline itemWidth="200px" @submit.native.prevent="search">
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
      <tars-form-item
        :label="$t('monitor.search.interfaceName')"
        @onLabelClick="groupBy('interface_name')"
      >
        <let-input size="small" v-model="query.interface_name"></let-input>
      </tars-form-item>
      <tars-form-item
        :label="$t('monitor.search.master')"
        @onLabelClick="groupBy('master_name')"
      >
        <let-input size="small" v-model="query.master_name"></let-input>
      </tars-form-item>
      <tars-form-item
        :label="$t('monitor.search.slave')"
        @onLabelClick="groupBy('slave_name')"
      >
        <let-input size="small" v-model="query.slave_name"></let-input>
      </tars-form-item>
      <tars-form-item
        :label="$t('monitor.search.masterIP')"
        @onLabelClick="groupBy('master_ip')"
      >
        <let-input size="small" v-model="query.master_ip"></let-input>
      </tars-form-item>
      <tars-form-item
        :label="$t('monitor.search.slaveIP')"
        @onLabelClick="groupBy('slave_ip')"
      >
        <let-input size="small" v-model="query.slave_ip"></let-input>
      </tars-form-item>
      <let-form-item>
        <el-button
          size="small"
          type="primary"
          @click="search"
          :disabled="loading"
          :icon="loading ? 'el-icon-loading' : ''"
          >{{ $t("operate.search") }}</el-button
        >
      </let-form-item>
    </let-form>

    <let-row ref="charts" class="charts" v-if="showChart">
      <let-col v-for="d in charts" :key="d.title" :span="12">
        <compare-chart v-bind="d" v-if="allItems.length > 0"></compare-chart>
      </let-col>
    </let-row>

    <hours-filter v-if="enableHourFilter" v-model="hour"></hours-filter>

    <let-table ref="table" :data="pagedItems" :empty-msg="$t('common.nodata')">
      <let-table-column :title="$t('common.time')" width="90px">
        <template slot-scope="props">{{
          props.row.show_time || props.row.show_date
        }}</template>
      </let-table-column>
      <let-table-column :title="$t('monitor.search.master')">
        <template slot-scope="props">
          <span v-if="props.row.master_name != '%'">{{
            props.row.master_name
          }}</span>
          <span
            class="btn-link"
            v-if="props.row.master_name == '%'"
            @click="groupBy('master_name', props.row)"
            >{{ props.row.master_name }}</span
          >
        </template>
      </let-table-column>
      <let-table-column :title="$t('monitor.search.slave')">
        <template slot-scope="props">
          <span v-if="props.row.slave_name != '%'">{{
            props.row.slave_name
          }}</span>
          <span
            class="btn-link"
            v-if="props.row.slave_name == '%'"
            @click="groupBy('slave_name', props.row)"
            >{{ props.row.slave_name }}</span
          >
        </template>
      </let-table-column>
      <let-table-column :title="$t('monitor.search.interfaceName')">
        <template slot-scope="props">
          <span v-if="props.row.interface_name != '%'">{{
            props.row.interface_name
          }}</span>
          <span
            class="btn-link"
            v-if="props.row.interface_name == '%'"
            @click="groupBy('interface_name', props.row)"
            >{{ props.row.interface_name }}</span
          >
        </template>
      </let-table-column>
      <let-table-column :title="$t('monitor.search.masterIP')">
        <template slot-scope="props">
          <span v-if="props.row.master_ip != '%'">{{
            props.row.master_ip
          }}</span>
          <span
            class="btn-link"
            v-if="props.row.master_ip == '%'"
            @click="groupBy('master_ip', props.row)"
            >{{ props.row.master_ip }}</span
          >
        </template>
      </let-table-column>
      <let-table-column :title="$t('monitor.search.slaveIP')">
        <template slot-scope="props">
          <span v-if="props.row.slave_ip != '%'">{{ props.row.slave_ip }}</span>
          <span
            class="btn-link"
            v-if="props.row.slave_ip == '%'"
            @click="groupBy('slave_ip', props.row)"
            >{{ props.row.slave_ip }}</span
          >
        </template>
      </let-table-column>
      <let-table-column prop="the_total_count" align="right">
        <span slot="head" slot-scope="props"
          >{{ $t("monitor.table.curr") }}<br />{{
            $t("monitor.table.total")
          }}</span
        >
      </let-table-column>
      <let-table-column prop="pre_total_count" align="right">
        <span slot="head" slot-scope="props"
          >{{ $t("monitor.table.contrast") }}<br />{{
            $t("monitor.table.total")
          }}</span
        >
      </let-table-column>
      <let-table-column prop="total_count_wave" align="right">
        <span slot="head" slot-scope="props">{{
          $t("monitor.table.fluctuating")
        }}</span>
      </let-table-column>
      <let-table-column align="right">
        <span slot="head" slot-scope="props"
          >{{ $t("monitor.table.curr") }}<br />{{ $t("monitor.table.a") }}</span
        >
        <template slot-scope="props">{{ props.row.the_avg_time }}ms</template>
      </let-table-column>
      <let-table-column align="right">
        <span slot="head" slot-scope="props"
          >{{ $t("monitor.table.contrast") }}<br />{{
            $t("monitor.table.a")
          }}</span
        >
        <template slot-scope="props">{{ props.row.pre_avg_time }}ms</template>
      </let-table-column>
      <let-table-column prop="the_fail_rate" align="right">
        <span slot="head" slot-scope="props"
          >{{ $t("monitor.table.curr") }}<br />{{ $t("monitor.table.b") }}</span
        >
      </let-table-column>
      <let-table-column prop="pre_fail_rate" align="right">
        <span slot="head" slot-scope="props"
          >{{ $t("monitor.table.contrast") }}<br />{{
            $t("monitor.table.b")
          }}</span
        >
      </let-table-column>
      <let-table-column prop="the_timeout_rate" align="right">
        <span slot="head" slot-scope="props"
          >{{ $t("monitor.table.curr") }}<br />{{ $t("monitor.table.c") }}</span
        >
      </let-table-column>
      <let-table-column prop="pre_timeout_rate" align="right">
        <span slot="head" slot-scope="props"
          >{{ $t("monitor.table.contrast") }}<br />{{
            $t("monitor.table.c")
          }}</span
        >
      </let-table-column>

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
          if (item[key] === "0.00%") {
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
    let _slave_name_arr = treeId.split(".");

    let _slave_name = "";

    let k8s = true;
    if (location.pathname == "/k8s.html") {
      k8s = true;
      //k8s的服务
      if (_slave_name_arr.length == 2) {
        //没有启用set的被调名
        _slave_name = _slave_name_arr[0] + "." + _slave_name_arr[1];
      }
    } else {
      k8s = false;

      if (_slave_name_arr.length == 2) {
        //没有启用set的被调名
        _slave_name =
          _slave_name_arr[0].substring(1) +
          "." +
          _slave_name_arr[1].substring(1);
      } else if (_slave_name_arr.length == 5) {
        //启用set的被调名
        _slave_name =
          _slave_name_arr[0].substring(1) +
          "." +
          _slave_name_arr[4].substring(1) +
          "." +
          _slave_name_arr[1].substring(1) +
          _slave_name_arr[2].substring(1) +
          _slave_name_arr[3].substring(1);
      }
    }

    return {
      loading: false,
      query: {
        // userpc:0,
        thedate: formatDate(new Date(), "YYYYMMDD"),
        predate: formatDate(Date.now() - ONE_DAY, "YYYYMMDD"),
        startshowtime: "0000",
        endshowtime: "2360",
        master_name: "",
        slave_name: _slave_name,
        interface_name: "",
        master_ip: "",
        slave_ip: "",
        group_by: "",
        k8s: k8s,
      },
      formatter,
      allItems: [],
      hour: -1,
      page: 1,
      showChart: true,
    };
  },
  props: ["treeid"],
  computed: {
    enableHourFilter() {
      return this.allItems.length && this.allItems[0].show_time;
    },
    filteredItems() {
      const hour = this.hour;
      return hour >= 0 && this.enableHourFilter
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
    charts() {
      return [
        {
          title: this.$t("monitor.table.total"),
          timeColumn: "show_time",
          dataColumns: [
            { name: "the_total_count", label: this.$t("monitor.table.curr") },
            {
              name: "pre_total_count",
              label: this.$t("monitor.table.contrast"),
            },
          ],
          data: this.allItems,
        },
        {
          title: this.$t("monitor.table.a"),
          timeColumn: "show_time",
          dataColumns: [
            { name: "the_avg_time", label: this.$t("monitor.table.curr") },
            { name: "pre_avg_time", label: this.$t("monitor.table.contrast") },
          ],
          data: this.allItems,
        },
        {
          title: this.$t("monitor.table.b"),
          timeColumn: "show_time",
          dataColumns: [
            { name: "the_fail_rate", label: this.$t("monitor.table.curr") },
            { name: "pre_fail_rate", label: this.$t("monitor.table.contrast") },
          ],
          data: this.allItems,
        },
        {
          title: this.$t("monitor.table.c"),
          timeColumn: "show_time",
          dataColumns: [
            { name: "the_timeout_rate", label: this.$t("monitor.table.curr") },
            {
              name: "pre_timeout_rate",
              label: this.$t("monitor.table.contrast"),
            },
          ],
          data: this.allItems,
        },
      ];
    },
    chartOptions() {
      return {
        title: {
          show: true,
          text: `${this.$t("monitor.table.curr")} ${this.$t(
            "monitor.table.total"
          )}`,
        },
        grid: {
          bottom: 40,
          top: 50,
        },
        legend: {
          top: 5,
        },
        settings: {
          labelMap: {
            the_value: this.$t("monitor.property.property"),
            pre_value: this.$t("monitor.property.propertyC"),
          },
          scale: [true, false],
        },
        data: {
          columns: ["show_time", "the_value", "pre_value"],
          rows: this.allItems,
        },
      };
    },
  },

  mounted() {
    this.fetchData();
  },

  methods: {
    fetchData() {
      this.loading = true;
      // const chartLoading = this.$refs.chart && this.$refs.chart.$loading.show();
      // const tableLoading = this.$refs.table.$loading.show();
      return this.$tars
        .getJSON("/server/api/stat_monitor_data", this.query)
        .then((data) => {
          this.loading = false;
          // chartLoading && chartLoading.hide();
          // tableLoading.hide();
          this.allItems = dataFormatter(data);
        })
        .catch((err) => {
          this.loading = false;
          // chartLoading && chartLoading.hide();
          // tableLoading.hide();
          this.$tip.error(
            `${this.$t("common.error")}: ${err.message || err.err_msg}`
          );
        });
    },

    groupBy(name, row) {
      this.query.group_by = name;
      this.showChart = false;
      if (row) {
        if (row.show_time) {
          this.query.startshowtime = row.show_time;
          this.query.endshowtime = row.show_time;
        }
        if (row.interface_name && row.interface_name != "%")
          this.query.interface_name = row.interface_name;
        if (row.master_ip && row.master_ip != "%")
          this.query.master_ip = row.master_ip;
        if (row.master_name && row.master_name != "%")
          this.query.master_name = row.master_name;
        if (row.slave_ip && row.slave_ip != "%")
          this.query.slave_ip = row.slave_ip;
        if (row.slave_name && row.slave_name != "%")
          this.query.slave_name = row.slave_name;
      }
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
.btn-link {
  color: #3f5ae0;
  cursor: pointer;
}
.page_server_server_monitor {
  padding-bottom: 20px;

  .charts {
    margin-top: 20px;
  }

  .hours-filter {
    margin-bottom: 16px;
  }
}
</style>
