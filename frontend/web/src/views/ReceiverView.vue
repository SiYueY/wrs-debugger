<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { api } from '../api/wrs';
import RadioConfigurationShell from '../components/RadioConfigurationShell.vue';
import RadioParametersEditor from '../components/RadioParametersEditor.vue';
import { useOperationFeedback } from '../composables/useOperationFeedback';
import { useRadioParameterState } from '../composables/useRadioParameterState';
import { useSdoState } from '../composables/useSdoState';
import { receiverRadioEditorConfig, validateReceiverRadioParameters } from '../domain/radio';
import { t } from '../i18n';
import { useDebuggerStore } from '../stores/debugger';

const store = useDebuggerStore();
const boundId = ref('-');
const radio = useRadioParameterState();
const sdo = useSdoState(receiverRadioEditorConfig);
const feedback = useOperationFeedback(() => store.refreshDiagnostic());
const ready = computed(() => store.receiverConnected && !feedback.busy.value);
const crossReady = computed(() => ready.value && store.transmitterConnected);

async function refreshInfo() {
  await feedback.run(async () => {
    boundId.value = (await api.receiverInfo()).bound_device_id ?? '-';
  });
}

async function readParameters() {
  await feedback.run(async () => {
    sdo.operation.value = 'read';
    if (sdo.objectAddress.value !== 0) {
      sdo.applyResponse(await api.readReceiverSdo(sdo.objectAddress.value));
      return;
    }
    if (radio.mode.value === 'lora') radio.lora.value = await api.receiverLora();
    else radio.gfsk.value = await api.receiverGfsk();
    sdo.markParameterSuccess('read');
  });
}

async function writeParameters() {
  if (sdo.objectAddress.value === 0) {
    const error = validateReceiverRadioParameters(radio.mode.value, radio.current.value);
    if (error) return void (feedback.message.value = t(error.messageKey));
  }
  await feedback.run(async () => {
    sdo.operation.value = 'write';
    if (sdo.objectAddress.value !== 0) {
      sdo.applyResponse(await api.writeReceiverSdo(sdo.objectAddress.value, sdo.objectData.value));
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
  });
}

async function restoreParameters() {
  if (sdo.objectAddress.value !== 0)
    return void (feedback.message.value = t('restoreCommunicationOnly'));
  await feedback.run(async () => {
    if (radio.mode.value === 'lora') {
      await api.restoreReceiverLora();
      radio.lora.value = await api.receiverLora();
    } else {
      await api.restoreReceiverGfsk();
      radio.gfsk.value = await api.receiverGfsk();
    }
  });
}

async function syncParameters() {
  await feedback.run(async () => {
    const operation =
      radio.mode.value === 'lora' ? await api.syncReceiverLora() : await api.syncReceiverGfsk();
    await store.trackOperation(operation.operation_id);
    if (radio.mode.value === 'lora') radio.lora.value = await api.receiverLora();
    else radio.gfsk.value = await api.receiverGfsk();
  });
}

async function bind() {
  await feedback.run(async () => {
    await store.trackOperation((await api.factoryBind()).operation_id);
    boundId.value = (await api.receiverInfo()).bound_device_id ?? '-';
  });
}

watch(
  () => store.receiverConnected,
  (connected) => {
    if (connected) {
      void refreshInfo();
      void readParameters();
    } else {
      boundId.value = '-';
      radio.reset();
      feedback.message.value = '';
    }
  },
  { immediate: true },
);

watch(radio.mode, () => {
  feedback.message.value = '';
  if (store.receiverConnected) void readParameters();
});
</script>

<template>
  <RadioConfigurationShell v-model:mode="radio.mode.value" :message="feedback.message.value">
    <template #identity-primary>
      <div class="identity-group">
        <label>{{ t('boundDeviceId') }}</label>
        <output>{{ boundId }}</output>
        <button class="dbg-btn" :disabled="!ready" @click="refreshInfo">{{ t('refresh') }}</button>
      </div>
    </template>
    <template #identity-secondary>
      <div class="identity-group">
        <label>{{ t('binding') }}</label>
        <output>{{ store.transmitterConnected ? t('connected') : '-' }}</output>
        <button class="dbg-btn" :disabled="!crossReady" @click="bind">{{ t('bind') }}</button>
      </div>
    </template>

    <RadioParametersEditor
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
