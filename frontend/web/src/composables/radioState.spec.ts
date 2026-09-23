import { nextTick } from 'vue';
import { describe, expect, it } from 'vitest';
import { receiverRadioEditorConfig } from '../domain/radio';
import { useRadioParameterState } from './useRadioParameterState';
import { useSdoState } from './useSdoState';

describe('radio state composables', () => {
  it('keeps LoRa and GFSK values independent and resets both', () => {
    const state = useRadioParameterState();
    state.lora.value.tx_power = 3;
    state.mode.value = 'gfsk';
    state.current.value = { ...state.gfsk.value, tx_power: 7 };
    expect(state.lora.value.tx_power).toBe(3);
    expect(state.gfsk.value.tx_power).toBe(7);
    state.reset();
    expect(state.lora.value.tx_power).toBe(10);
    expect(state.gfsk.value.tx_power).toBe(10);
  });

  it('resets SDO response and applies configured write defaults', async () => {
    const state = useSdoState(receiverRadioEditorConfig);
    state.applyResponse({ object_address: 1, object_data: 9, status: 4, result_code: 0 });
    state.objectAddress.value = 0x202;
    await nextTick();
    expect(state.objectData.value).toBe(0x454e);
    expect(state.responseStatus.value).toBeNull();
    expect(state.canWrite.value).toBe(true);
    state.objectAddress.value = 0x201;
    await nextTick();
    expect(state.objectData.value).toBe(0);
    expect(state.canWrite.value).toBe(false);
  });
});
