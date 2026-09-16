<script setup lang="ts">
import type { ConnectionState } from '../api/connection';
const props = defineProps<{ state: ConnectionState }>();
const labels: Record<ConnectionState, string> = {
  disconnected: 'Disconnected',
  connecting: 'Connecting',
  connected: 'Connected',
  failed: 'Failed',
};
const types: Record<ConnectionState, 'info' | 'warning' | 'success' | 'danger'> = {
  disconnected: 'info',
  connecting: 'warning',
  connected: 'success',
  failed: 'danger',
};
</script>
<template>
  <el-tag :type="types[props.state]" effect="plain">{{ labels[props.state] }}</el-tag>
</template>
