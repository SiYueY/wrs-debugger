<script setup lang="ts">
import { computed, ref } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { api } from '../api/wrs';
import { t } from '../i18n';
import { useDebuggerStore } from '../stores/debugger';

const route = useRoute();
const router = useRouter();
const store = useDebuggerStore();
const binding = ref(false);
const canBind = computed(
  () => store.receiverConnected && store.transmitterConnected && !binding.value,
);
const tabs = [
  { path: '/transmitter', key: 'box' as const },
  { path: '/receiver', key: 'receiver' as const },
];

async function factoryBind() {
  binding.value = true;
  try {
    await store.trackOperation((await api.factoryBind()).operation_id);
    store.showNotification(t('factoryBindSucceeded'));
  } catch (error) {
    store.error = error instanceof Error ? error.message : t('operationFailed');
    store.showNotification(store.error, true);
    await store.refreshDiagnostic().catch(() => undefined);
  } finally {
    binding.value = false;
  }
}
</script>

<template>
  <div class="navigation-row">
    <nav class="nav" role="tablist">
      <button
        v-for="tab in tabs"
        :key="tab.path"
        class="tab"
        :class="{ active: route.path === tab.path }"
        @click="router.push(tab.path)"
      >
        {{ t(tab.key) }}
      </button>
    </nav>
    <button
      v-if="route.path === '/receiver'"
      class="dbg-btn factory-bind"
      :disabled="!canBind"
      @click="factoryBind"
    >
      {{ t('bind') }}
    </button>
  </div>
</template>

<style scoped>
.navigation-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  min-width: 0;
}
.nav {
  display: inline-flex;
  width: fit-content;
  height: var(--navigation-height);
  border-radius: 22px;
  overflow: hidden;
  background: var(--track);
}
.tab {
  width: var(--navigation-width);
  border: 0;
  border-radius: 22px;
  background: transparent;
  color: #fff;
  font: inherit;
  cursor: pointer;
}
.tab.active {
  background: var(--brand);
}
.factory-bind {
  min-width: 92px;
}
</style>
