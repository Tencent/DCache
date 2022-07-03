<template>
  <div class="hours-filter">
    <h4>{{title}}</h4>
    <ul>
      <li
        v-for="hour in hours"
        :key="hour.val"
        :class="{active: hour.val === value}"
        @click="onChange(hour.val)"
      >{{hour.txt}}
      </li>
    </ul>
  </div>
</template>

<script>
  // eslint-disable-next-line prefer-spread
  const hours = Array.apply(null, {length: 24}).map((d, i) => i);
  const formattedHours = hours.map(d => ({val: d, txt: `0${d}`.slice(-2)}));
  const formattedHoursWithAll = [{val: -1, txt: 'all'}].concat(formattedHours);

  export default {
    name: 'HoursFilter',
    props: {
      title: {
        type: String,
        default: '',
      },
      value: Number,
    },

    filters: {
      format(val) {
        return `0${val}`.slice(-2);
      },
    },

    data() {
      return {
        hours: formattedHoursWithAll,
      };
    },

    methods: {
      onChange(val) {
        this.$emit('input', val);
      },
    },
  };
</script>

<style lang="postcss">
  .hours-filter {
    margin-bottom: 20px;
    overflow: hidden;
    h4 {
      color: #9096A3;
      font-weight: 700;
      margin-bottom: 12px;
    }

    ul {
      margin-bottom: -8px;
      overflow: hidden;
    }

    li {
      border: 1px solid #C0C4CC;
      border-radius: 14px;
      color: #454E66;
      cursor: pointer;
      float: left;
      font-size: 12px;
      line-height: 26px;
      margin-bottom: 8px;
      margin-right: 8px;
      min-width: 39px;
      padding: 0 11px;
      text-align: center;

      &.active {
        border-color: #3F5AE0;
        color: #3F5AE0;
      }

      &:last-child {
        margin-right: 0;
      }
    }
  }
</style>
