<script setup lang="ts">
import { storeToRefs } from 'pinia';
import { useConnectionStore } from '../stores/connection';
import ConnectionStatus from './ConnectionStatus.vue';
const { robotState, serialState } = storeToRefs(useConnectionStore());
</script>
<template>
  <header>
    <span>设备配置与调试</span>
    <div class="statuses">
      <span>Robot <ConnectionStatus :state="robotState" /></span
      ><span>Box <ConnectionStatus :state="serialState" /></span>
    </div>
  </header>
</template>
<style scoped>
header {
  height: 56px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 28px;
  background: #fff;
  border-bottom: 1px solid var(--app-border);
  font-size: 14px;
  font-weight: 600;
}
.statuses {
  display: flex;
  gap: 16px;
  font-size: 12px;
  font-weight: 400;
}
.statuses span {
  display: flex;
  align-items: center;
  gap: 7px;
}
</style>
