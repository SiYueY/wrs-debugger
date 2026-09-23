import { computed, ref, watch } from 'vue';
import type { SdoResponse } from '../api/wrs';
import { findSdoObject, type RadioEditorConfig } from '../domain/radio';

export function useSdoState(config: RadioEditorConfig) {
  const objectAddress = ref(0);
  const objectData = ref(0);
  const operation = ref<'read' | 'write'>('read');
  const responseStatus = ref<number | null>(null);
  const resultCode = ref<number | null>(null);
  const selectedObject = computed(() => findSdoObject(config, objectAddress.value));
  const canWrite = computed(() => selectedObject.value?.access === 'rw');

  function applyResponse(response: SdoResponse) {
    objectData.value = response.object_data;
    responseStatus.value = response.status;
    resultCode.value = response.result_code;
  }

  function markParameterSuccess(nextOperation: 'read' | 'write') {
    operation.value = nextOperation;
    responseStatus.value = nextOperation === 'read' ? 0x4 : 0x6;
    resultCode.value = 0;
  }

  watch(objectAddress, (address) => {
    objectData.value = findSdoObject(config, address)?.defaultWriteValue ?? 0;
    responseStatus.value = null;
    resultCode.value = null;
  });

  return {
    objectAddress,
    objectData,
    operation,
    responseStatus,
    resultCode,
    selectedObject,
    canWrite,
    applyResponse,
    markParameterSuccess,
  };
}
