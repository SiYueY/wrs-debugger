<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { api } from '../api/wrs';
import RadioConfigurationShell from '../components/RadioConfigurationShell.vue';
import RadioParametersEditor from '../components/RadioParametersEditor.vue';
import { useOperationFeedback } from '../composables/useOperationFeedback';
import { useRadioParameterState } from '../composables/useRadioParameterState';
import { useSdoState } from '../composables/useSdoState';
import { transmitterRadioEditorConfig, validateTransmitterRadioParameters } from '../domain/radio';
import { t } from '../i18n';
import { useDebuggerStore } from '../stores/debugger';

const store = useDebuggerStore();
const deviceId = ref('-');
const pin = ref('');
const radio = useRadioParameterState();
const sdo = useSdoState(transmitterRadioEditorConfig);
const feedback = useOperationFeedback(() => store.refreshDiagnostic());
const ready = computed(() => store.transmitterConnected && !feedback.busy.value);

async function readParameters() {
  await feedback.run(async () => {
    sdo.operation.value = 'read';
    if (sdo.objectAddress.value !== 0) {
      sdo.applyResponse(await api.readTransmitterSdo(sdo.objectAddress.value));
      return;
    }
    if (radio.mode.value === 'lora') radio.lora.value = await api.transmitterLora();
    else radio.gfsk.value = await api.transmitterGfsk();
    sdo.markParameterSuccess('read');
  });
}

async function writeParameters() {
  if (sdo.objectAddress.value === 0) {
    const error = validateTransmitterRadioParameters(radio.mode.value, radio.current.value);
    if (error) return void (feedback.message.value = t(error.messageKey));
  }
  await feedback.run(async () => {
    sdo.operation.value = 'write';
    if (sdo.objectAddress.value !== 0) {
      sdo.applyResponse(
        await api.writeTransmitterSdo(sdo.objectAddress.value, sdo.objectData.value),
      );
      return;
    }
    if (radio.mode.value === 'lora') {
      await api.writeTransmitterLora(radio.lora.value);
      radio.lora.value = await api.transmitterLora();
    } else {
      await api.writeTransmitterGfsk(radio.gfsk.value);
      radio.gfsk.value = await api.transmitterGfsk();
    }
    sdo.markParameterSuccess('write');
  });
}

async function restoreParameters() {
  if (sdo.objectAddress.value !== 0)
    return void (feedback.message.value = t('restoreCommunicationOnly'));
  await feedback.run(async () => {
    if (radio.mode.value === 'lora') {
      await api.restoreTransmitterLora();
      radio.lora.value = await api.transmitterLora();
    } else {
      await api.restoreTransmitterGfsk();
      radio.gfsk.value = await api.transmitterGfsk();
    }
  });
}

async function readDeviceId() {
  await feedback.run(async () => {
    deviceId.value = (await api.transmitterInfo()).device_id ?? '-';
  });
}

async function readPin() {
  await feedback.run(async () => {
    pin.value = (await api.readPin()).pin;
  });
}

async function writePin() {
  if (!/^\d{6}$/.test(pin.value) || pin.value === '000000')
    return void (feedback.message.value = t('pinInvalid'));
  await feedback.run(() => api.writePin(pin.value));
}

watch(
  () => store.transmitterConnected,
  (connected) => {
    if (connected) {
      void readDeviceId();
      void readPin();
      void readParameters();
    } else {
      radio.reset();
      deviceId.value = '-';
      pin.value = '';
    }
  },
  { immediate: true },
);
</script>

<template>
  <RadioConfigurationShell v-model:mode="radio.mode.value" :message="feedback.message.value">
    <template #identity-primary>
      <div class="identity-group">
        <label>{{ t('transmitterDeviceId') }}</label>
        <output>{{ deviceId }}</output>
        <button class="dbg-btn" :disabled="!ready" @click="readDeviceId">{{ t('refresh') }}</button>
      </div>
    </template>
    <template #identity-secondary>
      <div class="identity-group">
        <label>{{ t('transmitterPin') }}</label>
        <input v-model="pin" maxlength="6" inputmode="numeric" :disabled="!ready" />
        <div class="pin-actions">
          <button class="dbg-btn" :disabled="!ready" @click="readPin">{{ t('readPin') }}</button>
          <button class="dbg-btn" :disabled="!ready" @click="writePin">{{ t('writePin') }}</button>
        </div>
      </div>
    </template>

    <RadioParametersEditor
      v-model="radio.current.value"
      v-model:object-address="sdo.objectAddress.value"
      v-model:object-data="sdo.objectData.value"
      :kind="radio.mode.value"
      :config="transmitterRadioEditorConfig"
      :disabled="!ready"
      :operation="sdo.operation.value"
      :response-status="sdo.responseStatus.value"
      :result-code="sdo.resultCode.value"
    />

    <template #actions>
      <button class="dbg-btn" :disabled="!ready" @click="readParameters">{{ t('read') }}</button>
      <button class="dbg-btn" :disabled="!ready || !sdo.canWrite.value" @click="writeParameters">
        {{ t('write') }}
      </button>
      <button class="dbg-btn" :disabled="!ready" @click="restoreParameters">
        {{ t('restore') }}
      </button>
    </template>
  </RadioConfigurationShell>
</template>
