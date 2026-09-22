<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { api, type GfskParameters, type LoRaParameters } from '../api/wrs';
import GfskParametersForm from '../components/GfskParametersForm.vue';
import LoRaParametersForm from '../components/LoRaParametersForm.vue';
import { t } from '../i18n';
import { useDebuggerStore } from '../stores/debugger';
const store = useDebuggerStore();
const tab = ref<'lora' | 'gfsk'>('lora');
const boundId = ref('-');
const lora = ref<LoRaParameters>();
const gfsk = ref<GfskParameters>();
const busy = ref(false);
const message = ref('');
const ready = computed(() => store.receiverConnected && !busy.value);
const crossReady = computed(() => ready.value && store.transmitterConnected);
function validate() {
  const value = tab.value === 'lora' ? lora.value : gfsk.value;
  if (!value) return t('readFirst');
  if (value.payload_len !== 12) return t('payloadFixed');
  if (value.tx_power < 0 || value.tx_power > (value.param_flags & 1 ? 20 : 10))
    return t('powerOutOfRange');
  if (tab.value === 'lora' && lora.value!.spreading_factor <= 6 && lora.value!.preamble_len !== 12)
    return t('loraPreambleInvalid');
  if (tab.value === 'gfsk' && 2 * gfsk.value!.freq_deviation < gfsk.value!.bitrate / 2)
    return t('gfskRateInvalid');
  return '';
}
async function run(action: () => Promise<void>) {
  busy.value = true;
  message.value = '';
  try {
    await action();
    message.value = t('operationSucceeded');
  } catch (error) {
    message.value = error instanceof Error ? error.message : t('operationFailed');
  } finally {
    busy.value = false;
  }
}
async function refreshInfo() {
  await run(async () => {
    boundId.value = (await api.receiverInfo()).bound_device_id ?? '-';
  });
}
async function read() {
  await run(async () => {
    if (tab.value === 'lora') lora.value = await api.receiverLora();
    else gfsk.value = await api.receiverGfsk();
  });
}
async function write() {
  const error = validate();
  if (error) {
    message.value = error;
    return;
  }
  await run(async () => {
    if (tab.value === 'lora') await api.writeReceiverLora(lora.value!);
    else await api.writeReceiverGfsk(gfsk.value!);
    await read();
  });
}
async function restore() {
  await run(async () => {
    if (tab.value === 'lora') await api.restoreReceiverLora();
    else await api.restoreReceiverGfsk();
    await read();
  });
}
async function sync() {
  await run(async () => {
    const created =
      tab.value === 'lora' ? await api.syncReceiverLora() : await api.syncReceiverGfsk();
    await store.trackOperation(created.operation_id);
    await read();
  });
}
async function bind() {
  await run(async () => {
    await store.trackOperation((await api.factoryBind()).operation_id);
    await refreshInfo();
  });
}
onMounted(() => {
  if (store.receiverConnected) void refreshInfo();
});
</script>
<template>
  <div class="view">
    <section class="panel identity">
      <div class="panel-tag">{{ t('binding') }}</div>
      <div class="row">
        <label>{{ t('boundDeviceId') }}</label
        ><span>{{ boundId }}</span
        ><button class="dbg-btn" :disabled="!ready" @click="refreshInfo">{{ t('refresh') }}</button
        ><button class="dbg-btn" :disabled="!crossReady" @click="bind">{{ t('bind') }}</button>
      </div>
    </section>
    <section class="panel parameter-panel">
      <div class="tabs">
        <button :class="{ active: tab === 'lora' }" @click="tab = 'lora'">LoRa</button
        ><button :class="{ active: tab === 'gfsk' }" @click="tab = 'gfsk'">GFSK</button>
      </div>
      <LoRaParametersForm
        v-if="tab === 'lora' && lora"
        v-model="lora"
        :disabled="!ready"
      /><GfskParametersForm v-if="tab === 'gfsk' && gfsk" v-model="gfsk" :disabled="!ready" />
      <p v-if="(tab === 'lora' && !lora) || (tab === 'gfsk' && !gfsk)" class="empty">
        {{ store.receiverConnected ? t('readFirst') : t('connectReceiver') }}
      </p>
      <div class="actions">
        <span :class="{ error: message && message !== t('operationSucceeded') }">{{ message }}</span
        ><button class="dbg-btn" :disabled="!ready" @click="read">{{ t('read') }}</button
        ><button class="dbg-btn" :disabled="!ready" @click="write">{{ t('write') }}</button
        ><button class="dbg-btn" :disabled="!ready" @click="restore">{{ t('restore') }}</button
        ><button class="dbg-btn" :disabled="!crossReady" @click="sync">{{ t('sync') }}</button>
      </div>
    </section>
  </div>
</template>
<style scoped>
.view {
  height: 100%;
  min-height: 0;
  display: grid;
  grid-template-rows: auto minmax(0, 1fr);
  gap: 14px;
}
.identity {
  max-width: 850px;
}
.parameter-panel {
  min-height: 0;
  overflow: auto;
}
.row {
  display: grid;
  grid-template-columns: 220px 200px auto auto;
  align-items: center;
  gap: 10px;
  margin: 8px 0;
}
.tabs {
  display: flex;
  gap: 6px;
  margin-bottom: 14px;
}
.tabs button {
  width: 110px;
  height: 32px;
  border: 0;
  border-radius: 16px;
  background: var(--tab);
  cursor: pointer;
}
.tabs .active {
  background: var(--brand);
  color: #fff;
}
.actions {
  display: flex;
  position: sticky;
  bottom: 0;
  justify-content: flex-end;
  align-items: center;
  gap: 10px;
  margin-top: 16px;
  padding-top: 8px;
  background: #fff;
}
.actions .dbg-btn {
  min-width: var(--parameter-button-width);
}
.actions span {
  margin-right: auto;
  color: var(--ok);
}
.actions .error {
  color: var(--danger);
}
.empty {
  color: #666;
}
@media (max-width: 760px) {
  .row {
    grid-template-columns: minmax(0, 1fr);
  }
}
</style>
