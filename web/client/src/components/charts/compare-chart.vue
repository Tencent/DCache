<template>
  <div class="compare-chart">
    <ve-line v-bind="chartOptions"></ve-line>
    <a href="javascript:" class="compare-chart-zoom-in" @click="enlarge = true">
      <icon name="zoom-in" />
    </a>
    <let-modal v-model="enlarge" @close="enlarge = false" width="80%">
      <ve-line v-if="enlarge" v-bind="largeChartOptions"></ve-line>
      <div slot="foot"></div>
    </let-modal>
  </div>
</template>

<script>
import '@/plugins/charts';
import '@/assets/icon/zoom-in.svg';

export default {
  props: {
    title: String,
    timeColumn: String,
    dataColumns: Array,
    data: Array,
  },

  data() {
    return {
      enlarge: false,
    };
  },

  computed: {
    chartOptions() {
      const labelMap = {};
      const columns = [this.timeColumn];

      this.dataColumns.forEach(({ name, label }) => {
        labelMap[name] = label;
        columns.push(name);
      });

      return {
        title: {
          show: true,
          text: this.title,
        },
        grid: {
          bottom: 40,
          top: 50,
        },
        legend: {
          top: 5,
        },
        colors: ['#f56c77', '#6accab'],
        settings: {
          labelMap,
          scale: [true, false],
          lineStyle: {
            width: 1,
          },
        },
        dataZoom:[
          {
            type:'inside',//slider表示有滑动块的，inside 表示内置的
            show:true,
            xAxisIndex:[0],
            zoomOnMouseWheel:true, //鼠标滚轮直接操作
            start:0,
            end:100
          }
        ],
        data: {
          columns,
          rows: this.data,
        },
      };
    },
    largeChartOptions() {
      const labelMap = {};
      const columns = [this.timeColumn];

      this.dataColumns.forEach(({ name, label }) => {
        labelMap[name] = label;
        columns.push(name);
      });

      return {
        title: {
          show: true,
          text: this.title,
          left: 'center',
          top: 'bottom',
        },
        grid: {
          bottom: 40,
          top: 50,
        },
        legend: {
          top: 5,
        },
        colors: ['#f56c77', '#6accab'],
        settings: {
          labelMap,
          scale: [true, false],
          lineStyle: {
            width: 1,
          },
        },
        data: {
          columns,
          rows: this.data,
        },
        dataZoom: [
          { type: 'inside', minValueSpan: 12, zoomOnMouseWheel: 'alt' },
          { minValueSpan: 12 },
        ],
      };
    },
  },
};
</script>

<style>
.compare-chart {
  position: relative;

  &-zoom-in {
    color: inherit;
    cursor: pointer;
    font-size: 16px;
    position: absolute;
    right: 35px;
    top: 5px;
  }

  .let_modal__dialog {
    max-width: 1200px;
  }
}
</style>
