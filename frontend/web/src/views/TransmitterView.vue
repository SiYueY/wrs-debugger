<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { api, type GfskParameters, type LoRaParameters } from '../api/wrs';
import TransmitterRadioParameters from '../components/TransmitterRadioParameters.vue';
import { t } from '../i18n';
import { useDebuggerStore } from '../stores/debugger';

const store = useDebuggerStore();
const defaultLoRaParameters = (): LoRaParameters => ({
  param_flags: 0,
  tx_power: 10,
  freq_offset: 250,
  payload_len: 12,
  rssi_threshold: 110,
  heartbeat_interval: 200,
  heartbeat_loss: 3,
  bandwidth: 1,
  spreading_factor: 6,
  coding_rate: 4,
  header_type: 0,
  preamble_len: 12,
  sync_word: 5156,
});
const defaultGfskParameters = (): GfskParameters => ({
  param_flags: 0x4000,
  tx_power: 10,
  freq_offset: 250,
  payload_len: 12,
  rssi_threshold: 110,
  heartbeat_interval: 200,
  heartbeat_loss: 3,
  bandwidth: 1,
  bitrate: 50000,
  freq_deviation: 25000,
  pulse_shaping: 9,
  preamble_len: 16,
  sync_word: 5156,
});
const mode = ref<'lora' | 'gfsk'>('lora');
const deviceId = ref('-');
const pin = ref('');
const lora = ref<LoRaParameters>(defaultLoRaParameters());
const gfsk = ref<GfskParameters>(defaultGfskParameters());
const busy = ref(false);
const message = ref('');
const ready = computed(() => store.transmitterConnected && !busy.value);
const current = computed({
  get: () => (mode.value === 'lora' ? lora.value : gfsk.value),
  set: (value: LoRaParameters | GfskParameters) => {
    if (mode.value === 'lora') lora.value = value as LoRaParameters;
    else gfsk.value = value as GfskParameters;
  },
});

function validate() {
  const value = current.value;
  if (!value) return t('readFirst');
  if (value.payload_len !== 12) return t('payloadFixed');
  if (value.tx_power < 0 || value.tx_power > (value.param_flags & 1 ? 20 : 10))
    return t('powerOutOfRange');
  const loraValue = lora.value;
  if (
    mode.value === 'lora' &&
    loraValue &&
    loraValue.spreading_factor <= 6 &&
    loraValue.preamble_len !== 12
  )
    return t('loraPreambleInvalid');
  if (mode.value === 'gfsk' && gfsk.value && 4 * gfsk.value.freq_deviation < gfsk.value.bitrate)
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
    void store.refreshDiagnostic();
  } finally {
    busy.value = false;
  }
}
async function readParameters() {
  await run(async () => {
    if (mode.value === 'lora') lora.value = await api.transmitterLora();
    else gfsk.value = await api.transmitterGfsk();
  });
}
async function writeParameters() {
  const validation = validate();
  if (validation) return void (message.value = validation);
  await run(async () => {
    if (mode.value === 'lora') {
      await api.writeTransmitterLora(lora.value!);
      lora.value = await api.transmitterLora();
    } else {
      await api.writeTransmitterGfsk(gfsk.value!);
      gfsk.value = await api.transmitterGfsk();
    }
  });
}
async function restoreParameters() {
  await run(async () => {
    if (mode.value === 'lora') {
      await api.restoreTransmitterLora();
      lora.value = await api.transmitterLora();
    } else {
      await api.restoreTransmitterGfsk();
      gfsk.value = await api.transmitterGfsk();
    }
  });
}
async function readDeviceId() {
  await run(async () => {
    deviceId.value = (await api.transmitterInfo()).device_id ?? '-';
  });
}
async function readPin() {
  await run(async () => {
    pin.value = (await api.readPin()).pin;
  });
}
async function writePin() {
  if (!/^\d{6}$/.test(pin.value) || pin.value === '000000')
    return void (message.value = t('pinInvalid'));
  await run(() => api.writePin(pin.value));
}
watch(
  () => store.transmitterConnected,
  (connected) => {
    if (connected) {
      void readDeviceId();
      void readPin();
      void readParameters();
    } else {
      lora.value = defaultLoRaParameters();
      gfsk.value = defaultGfskParameters();
      deviceId.value = '-';
      pin.value = '';
    }
  },
  { immediate: true },
);
</script>
<template>
  <div class="view">
    <section class="configuration-shell">
      <nav class="mode-nav" :aria-label="t('modulation')">
        <button :class="{ active: mode === 'lora' }" @click="mode = 'lora'">LoRa</button>
        <button :class="{ active: mode === 'gfsk' }" @click="mode = 'gfsk'">GFSK</button>
      </nav>
      <section class="configuration-card">
        <section class="identity-panel">
          <div class="identity-row">
            <div class="identity-group">
              <label>{{ t('transmitterDeviceId') }}</label
              ><output>{{ deviceId }}</output>
              <button class="dbg-btn" :disabled="!ready" @click="readDeviceId">
                {{ t('refresh') }}
              </button>
            </div>
            <div class="divider"></div>
            <div class="identity-group">
              <label>{{ t('transmitterPin') }}</label>
              <input v-model="pin" maxlength="6" inputmode="numeric" :disabled="!ready" />
              <div class="pin-actions">
                <button class="dbg-btn" :disabled="!ready" @click="readPin">
                  {{ t('readPin') }}
                </button>
                <button class="dbg-btn" :disabled="!ready" @click="writePin">
                  {{ t('writePin') }}
                </button>
              </div>
            </div>
          </div>
        </section>
        <section class="parameter-panel">
          <div class="parameter-header">
            <div class="panel-tag">{{ t('communicationParameters') }}</div>
          </div>
          <div class="parameter-content">
            <TransmitterRadioParameters v-model="current" :kind="mode" :disabled="!ready" />
          </div>
          <div class="parameter-actions">
            <span :class="{ error: message && message !== t('operationSucceeded') }">{{
              message
            }}</span>
            <button class="dbg-btn" :disabled="!ready" @click="readParameters">
              {{ t('read') }}
            </button>
            <button class="dbg-btn" :disabled="!ready" @click="writeParameters">
              {{ t('write') }}
            </button>
            <button class="dbg-btn" :disabled="!ready" @click="restoreParameters">
              {{ t('restore') }}
            </button>
          </div>
        </section>
      </section>
    </section>
  </div>
</template>
<style scoped>
.view {
  min-width: 0;
  height: 100%;
  min-height: 0;
}
.configuration-shell {
  position: relative;
  height: 100%;
  min-height: 0;
}
.mode-nav {
  display: flex;
  position: absolute;
  z-index: 2;
  top: 0;
  left: 0;
  width: 82px;
  flex-direction: column;
  gap: 10px;
  padding: 0;
}
.mode-nav button {
  width: 82px;
  height: 42px;
  border: 0;
  border-radius: 21px 0 0 21px;
  background: #c7c7c7;
  color: #202020;
  font-size: 16px;
  cursor: pointer;
}
.mode-nav button.active {
  z-index: 1;
  width: 82px;
  border: 1px solid #d0d0d0;
  border-right: 0;
  background: #f3f3f3;
  color: #202020;
  box-shadow: none;
}
.configuration-card {
  display: flex;
  position: relative;
  z-index: 1;
  height: 100%;
  margin-left: 82px;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
  gap: 10px;
  padding: 12px;
  border: 1px solid #d0d0d0;
  border-left: 0;
  border-radius: 0 6px 6px 0;
  background: #f3f3f3;
}
.identity-panel,
.parameter-panel {
  min-height: 0;
  border: 1px solid #e1e1e1;
  border-radius: 6px;
  background: #fff;
}
.identity-panel {
  flex: 0 0 72px;
}
.parameter-panel {
  display: flex;
  flex: 1;
  flex-direction: column;
  overflow: hidden;
}
.identity-row {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 1px minmax(0, 1fr);
  align-items: center;
  gap: 28px;
  min-height: 70px;
  padding: 8px 34px;
}
.identity-group {
  display: grid;
  grid-template-columns: max-content 108px max-content;
  align-items: center;
  gap: 12px;
}
.identity-group label {
  border-radius: 3px;
  background: #050505;
  color: #fff;
  padding: 4px 8px;
  font-size: 14px;
  white-space: nowrap;
}
.identity-group output,
.identity-group input {
  box-sizing: border-box;
  width: 108px;
  height: var(--field-height);
  border: 1px solid var(--input-border);
  border-radius: 3px;
  padding: 4px 7px;
  color: #555;
  text-align: center;
}
.identity-group output {
  display: flex;
  align-items: center;
  justify-content: center;
  background: #f4f4f4;
}
.divider {
  width: 1px;
  height: 42px;
  background: #d5d5d5;
}
.pin-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  white-space: nowrap;
}
.parameter-header {
  flex: 0 0 auto;
  padding: 12px 32px 0;
}
.parameter-content {
  min-width: 0;
  min-height: 0;
  flex: 1;
  overflow: auto;
  padding: 0 32px 10px;
}
.panel-tag {
  display: inline-block;
  margin-bottom: 12px;
  padding: 2px 12px;
  border-radius: 4px;
  background: #050505;
  color: #fff;
  font-size: 12px;
  line-height: 1.6;
}
.parameter-content :deep(.parameter-grid) {
  align-content: start;
  align-items: start;
}
.parameter-content :deep(.parameter-column) {
  grid-template-rows: repeat(10, var(--field-height));
  align-content: start;
}
.parameter-actions {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 10px;
  flex: 0 0 48px;
  padding: 6px 32px 10px;
  background: #fff;
}
.parameter-actions .dbg-btn {
  min-width: var(--parameter-button-width);
}
.parameter-actions span {
  margin-right: auto;
  color: var(--ok);
}
.parameter-actions .error {
  color: var(--danger);
}
@media (max-width: 920px) {
  .identity-panel {
    flex-basis: auto;
  }
  .identity-row {
    grid-template-columns: minmax(0, 1fr);
    gap: 10px;
    min-height: 0;
    padding: 12px 24px;
  }
  .divider {
    display: none;
  }
  .identity-group {
    grid-template-columns: max-content 108px max-content;
  }
  .parameter-header {
    padding: 12px 24px 0;
  }
  .parameter-content {
    padding: 0 24px 10px;
  }
  .parameter-actions {
    padding: 6px 24px 10px;
  }
}
@media (max-width: 680px) {
  .mode-nav,
  .mode-nav button,
  .mode-nav button.active {
    width: 70px;
  }
  .configuration-card {
    margin-left: 70px;
  }
  .identity-group {
    grid-template-columns: max-content minmax(0, 108px);
  }
  .identity-group .dbg-btn,
  .pin-actions {
    grid-column: 1 / -1;
    justify-self: start;
  }
}
</style>
