import { describe, expect, it } from 'vitest';
import {
  defaultGfskParameters,
  defaultLoRaParameters,
  findSdoObject,
  receiverRadioEditorConfig,
  transmitterRadioEditorConfig,
  validateReceiverRadioParameters,
  validateTransmitterRadioParameters,
} from './radio';

describe('radio protocol domain', () => {
  it('creates independent protocol defaults', () => {
    const first = defaultLoRaParameters();
    const second = defaultLoRaParameters();
    first.tx_power = 0;
    expect(second.tx_power).toBe(10);
    expect(defaultGfskParameters()).toMatchObject({
      param_flags: 0x4000,
      payload_len: 12,
      bitrate: 50000,
    });
  });

  it('describes device-specific SDO permissions', () => {
    expect(findSdoObject(receiverRadioEditorConfig, 0x201)?.access).toBe('ro');
    expect(findSdoObject(receiverRadioEditorConfig, 0x202)).toMatchObject({
      access: 'rw',
      defaultWriteValue: 0x454e,
    });
    expect(findSdoObject(receiverRadioEditorConfig, 0x102)).toBeUndefined();
    expect(findSdoObject(transmitterRadioEditorConfig, 0x102)?.labelKey).toBe('battery');
  });

  it('returns structured transmitter validation errors', () => {
    const lora = defaultLoRaParameters();
    lora.payload_len = 11;
    expect(validateTransmitterRadioParameters('lora', lora)).toEqual({
      field: 'payload_len',
      messageKey: 'payloadFixed',
    });
    lora.payload_len = 12;
    lora.preamble_len = 11;
    expect(validateTransmitterRadioParameters('lora', lora)?.field).toBe('preamble_len');
  });

  it('applies receiver-only flag and sync-word validation', () => {
    const lora = defaultLoRaParameters();
    lora.param_flags = 0x0010;
    expect(validateReceiverRadioParameters('lora', lora)?.field).toBe('param_flags');
    lora.param_flags = 0;
    lora.sync_word = 0x1425;
    expect(validateReceiverRadioParameters('lora', lora)).toEqual({
      field: 'sync_word',
      messageKey: 'loraSyncWordInvalid',
    });
    const gfsk = defaultGfskParameters();
    gfsk.freq_deviation = 1000;
    expect(validateReceiverRadioParameters('gfsk', gfsk)?.field).toBe('freq_deviation');
  });
});
