<script setup lang="ts">
import { onBeforeUnmount, onMounted } from 'vue';
import AppNavTabs from '../components/AppNavTabs.vue';
import AppToolbar from '../components/AppToolbar.vue';
import { useDebuggerStore } from '../stores/debugger';
const store = useDebuggerStore();
onMounted(() => {
  void store.bootstrap();
});
const stopEvents = store.startEvents();
onBeforeUnmount(stopEvents);
</script>
<template>
  <main class="app">
    <AppToolbar />
    <div class="operation">
      <AppNavTabs />
      <section class="workspace"><RouterView /></section>
    </div>
  </main>
</template>
<style scoped>
.app {
  display: flex;
  height: 100dvh;
  min-height: 0;
  flex-direction: column;
  background: var(--outer);
}
.operation {
  display: flex;
  min-height: 0;
  flex: 1;
  flex-direction: column;
  gap: 8px;
  padding: 10px;
}
.workspace {
  min-height: 0;
  flex: 1;
  overflow: hidden;
  padding: 14px;
  background: var(--inner);
  border-radius: 5px;
}
.workspace :deep(.view) {
  height: 100%;
  min-height: 100%;
}
</style>
