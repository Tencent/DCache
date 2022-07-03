<template>
  <let-form-item class="tars-form-item">
    <label
      class="let-form-item__label let-label__position_top clickable"
      @click="$emit('onLabelClick')"
    >{{label}}</label>
    <slot></slot>
  </let-form-item>
</template>

<script>
  export default {
    name: 'TarsFormItem',
    props: {
      label: String,
    },
  };
</script>

<style>
  .tars-form-item {

  .clickable {
    color: #3f5ae0;
  }

  }
</style>
