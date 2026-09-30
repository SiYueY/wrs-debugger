<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { api } from '../api/wrs';
import RadioConfigurationShell from '../components/RadioConfigurationShell.vue';
import RadioParametersEditor from '../components/RadioParametersEditor.vue';
import { useOperationFeedback } from '../composables/useOperationFeedback';
import { useRadioParameterState } from '../composables/useRadioParameterState';
import { useSdoState } from '../composables/useSdoState';
import { receiverRadioEditorConfig, validateReceiverRadioParameters } from '../domain/radio';
import { t, type TranslationKey } from '../i18n';
import { useDebuggerStore } from '../stores/debugger';

const store = useDebuggerStore();
const boundId = ref<string | null>(null);
const readingBoundId = ref(false);
const radio = useRadioParameterState();
const sdo = useSdoState(receiverRadioEditorConfig);
const feedback = useOperationFeedback(() => store.refreshDiagnostic());
const ready = computed(() => store.receiverConnected && !feedback.busy.value);

function parameterSuccess(action: TranslationKey) {
  const modulation = radio.mode.value === 'lora' ? 'LoRa' : 'GFSK';
  return `${modulation} ${t(action)}`;
}

function sdoSuccess(action: TranslationKey) {
  const address = sdo.objectAddress.value.toString(16).toUpperCase().padStart(3, '0');
  return `${t(action)}（0x${address}）`;
}
const crossReady = computed(() => ready.value && store.transmitterConnected);
const boundIdText = computed(() => boundId.value ?? t('unbound'));

async function readBoundDeviceId() {
  readingBoundId.value = true;
  try {
    const deviceId = (await api.receiverInfo()).bound_device_id;
    boundId.value = deviceId;
    feedback.notify(
      deviceId ? `${t('readDeviceIdSucceeded')}：${deviceId}` : t('readDeviceIdUnbound'),
    );
  } catch (error) {
    feedback.notify(error instanceof Error ? error.message : t('operationFailed'), true);
    void store.refreshDiagnostic();
  } finally {
    readingBoundId.value = false;
  }
}

async function readParameters() {
  await feedback.run(
    async () => {
      sdo.operation.value = 'read';
      if (sdo.objectAddress.value !== 0) {
        sdo.applyResponse(await api.readReceiverSdo(sdo.objectAddress.value));
        return;
      }
      if (radio.mode.value === 'lora') radio.lora.value = await api.receiverLora();
      else radio.gfsk.value = await api.receiverGfsk();
      sdo.markParameterSuccess('read');
    },
    sdo.objectAddress.value === 0
      ? parameterSuccess('readParametersSucceeded')
      : sdoSuccess('readSdoSucceeded'),
  );
}

async function writeParameters() {
  if (sdo.objectAddress.value === 0) {
    const error = validateReceiverRadioParameters(radio.mode.value, radio.current.value);
    if (error) return void feedback.notify(t(error.messageKey), true);
  }
  await feedback.run(
    async () => {
      sdo.operation.value = 'write';
      if (sdo.objectAddress.value !== 0) {
        sdo.applyResponse(
          await api.writeReceiverSdo(sdo.objectAddress.value, sdo.objectData.value),
        );
        return;
      }
      if (radio.mode.value === 'lora') {
        await api.writeReceiverLora(radio.lora.value);
        radio.lora.value = await api.receiverLora();
      } else {
        await api.writeReceiverGfsk(radio.gfsk.value);
        radio.gfsk.value = await api.receiverGfsk();
      }
      sdo.markParameterSuccess('write');
    },
    sdo.objectAddress.value === 0
      ? parameterSuccess('writeParametersSucceeded')
      : sdoSuccess('writeSdoSucceeded'),
  );
}

async function restoreParameters() {
  if (sdo.objectAddress.value !== 0)
    return void feedback.notify(t('restoreCommunicationOnly'), true);
  await feedback.run(async () => {
    if (radio.mode.value === 'lora') {
      await api.restoreReceiverLora();
      radio.lora.value = await api.receiverLora();
    } else {
      await api.restoreReceiverGfsk();
      radio.gfsk.value = await api.receiverGfsk();
    }
  }, parameterSuccess('restoreParametersSucceeded'));
}

async function syncParameters() {
  await feedback.run(async () => {
    const operation =
      radio.mode.value === 'lora' ? await api.syncReceiverLora() : await api.syncReceiverGfsk();
    await store.trackOperation(operation.operation_id);
    if (radio.mode.value === 'lora') radio.lora.value = await api.receiverLora();
    else radio.gfsk.value = await api.receiverGfsk();
  }, parameterSuccess('syncParametersSucceeded'));
}

watch(
  () => store.receiverConnected,
  (connected) => {
    if (connected) {
      void readBoundDeviceId();
      void readParameters();
    } else {
      boundId.value = null;
      radio.reset();
      sdo.reset();
    }
  },
  { immediate: true },
);

watch(radio.mode, () => {
  if (store.receiverConnected) void readParameters();
});
</script>

<template>
  <RadioConfigurationShell v-model:mode="radio.mode.value">
    <template #identity-primary>
      <div class="identity-group">
        <label>{{ t('transmitterDeviceId') }}</label>
        <output>{{ boundIdText }}</output>
        <button
          class="dbg-btn"
          :disabled="!store.receiverConnected || readingBoundId"
          @click="readBoundDeviceId"
        >
          {{ t('refresh') }}
        </button>
      </div>
    </template>

    <RadioParametersEditor
      :unavailable="!store.receiverConnected"
      v-model="radio.current.value"
      v-model:object-address="sdo.objectAddress.value"
      v-model:object-data="sdo.objectData.value"
      :kind="radio.mode.value"
      :config="receiverRadioEditorConfig"
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
      <button class="dbg-btn" :disabled="!crossReady" @click="syncParameters">
        {{ t('sync') }}
      </button>
    </template>
  </RadioConfigurationShell>
</template>
