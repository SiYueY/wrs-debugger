<script setup lang="ts">
import { storeToRefs } from 'pinia';
import { computed, ref } from 'vue';
import { t } from '../i18n';
import { useDebuggerStore } from '../stores/debugger';
import SettingsDialog from './SettingsDialog.vue';
const store = useDebuggerStore();
const { diagnostic, domainId, ports, selectedPort, transmitter, receiver, loading } =
  storeToRefs(store);
const settingsOpen = ref(false);
const labels = computed(() => ({
  disconnected: t('disconnected'),
  connecting: t('connecting'),
  connected: t('connected'),
  failed: t('failed'),
}));
const hasDiagnosticError = computed(() => diagnostic.value.code !== null);
const diagnosticLabel = computed(() => {
  if (!hasDiagnosticError.value) return t('noError');
  return diagnostic.value.detail
    ? `${diagnostic.value.code}: ${diagnostic.value.detail}`
    : diagnostic.value.code!;
});
</script>
<template>
  <header class="toolbar">
    <div class="group">
      <label>
        {{ t('domainId') }}
        <input
          v-model.number="domainId"
          type="number"
          min="0"
          :disabled="store.receiverConnected || loading"
        />
      </label>
      <span class="status" :class="receiver.state">{{ labels[receiver.state] }}</span>
      <button class="dbg-btn" :disabled="loading" @click="store.toggleReceiver">
        {{ store.receiverConnected ? t('disconnect') : t('connect') }}
      </button>
      <button
        class="dbg-btn"
        :disabled="loading || store.receiverConnected"
        @click="store.saveSettings"
      >
        {{ t('apply') }}
      </button>
      <button
        class="dbg-btn"
        :disabled="loading || store.receiverConnected"
        @click="store.reloadSettings"
      >
        {{ t('reload') }}
      </button>
      <label>
        {{ t('usb') }}
        <select
          :value="selectedPort"
          :disabled="loading"
          @change="store.selectPort(($event.target as HTMLSelectElement).value)"
        >
          <option v-for="port in ports" :key="port.device" :value="port.device">
            {{ port.device }}
          </option>
        </select>
      </label>
      <span class="status" :class="transmitter.state">{{ labels[transmitter.state] }}</span>
      <button
        v-if="store.transmitterConnected"
        class="dbg-btn"
        :disabled="loading"
        @click="store.disconnectTransmitter"
      >
        {{ t('disconnect') }}
      </button>
    </div>
    <div class="toolbar-spacer"></div>
    <div class="group diagnostic-group">
      <div class="diagnostic" :class="{ active: hasDiagnosticError }" :title="diagnosticLabel">
        <span aria-hidden="true">ⓘ</span>{{ diagnosticLabel }}
      </div>
      <button
        class="clear-btn"
        :disabled="loading || !hasDiagnosticError"
        @click="store.clearDiagnostic"
      >
        {{ t('clearError') }}
      </button>
      <SettingsDialog v-model:open="settingsOpen" />
    </div>
  </header>
</template>
<style scoped>
.toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  min-height: var(--toolbar-height);
  padding: 0 14px;
  background: var(--toolbar);
  color: #000;
}
.group {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
}
.toolbar-spacer {
  flex: 1;
}
.diagnostic-group {
  margin-left: auto;
  flex-wrap: nowrap;
}
label {
  display: flex;
  align-items: center;
  gap: 6px;
}
input,
select {
  height: 26px;
  min-width: 76px;
  border: 0;
  border-radius: 4px;
  padding: 0 6px;
}
.status {
  min-width: 60px;
}
.connected {
  color: var(--ok);
  font-weight: 700;
}
.failed {
  color: var(--danger);
}
.connecting {
  color: #2166b5;
}
.diagnostic {
  display: flex;
  align-items: center;
  gap: 6px;
  width: min(250px, 20vw);
  height: 30px;
  padding: 0 9px;
  overflow: hidden;
  border: 1px solid #c5c5c5;
  background: #ededed;
  color: #333;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.diagnostic.active {
  border-color: #d56464;
  color: var(--danger);
}
.clear-btn {
  min-width: 58px;
  height: 30px;
  border: 0;
  border-radius: 15px;
  background: #c7c7c7;
  color: #fff;
  cursor: pointer;
}
.clear-btn:not(:disabled) {
  background: var(--brand);
}
.toolbar {
  flex-wrap: wrap;
}
@media (max-width: 760px) {
  .toolbar {
    align-content: center;
    padding: 6px 10px;
  }
  .diagnostic-group {
    width: 100%;
  }
  .diagnostic {
    flex: 1;
    width: auto;
  }
}
</style>
