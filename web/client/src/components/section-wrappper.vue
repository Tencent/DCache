<template>
  <section class="section">
    <slot></slot>
  </section>
</template>

<style scoped>
  @import '../assets/css/variable.css';

  .section {
    margin-bottom: var(--gap-normal);
    position: relative;
  }
</style>
