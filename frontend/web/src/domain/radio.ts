import type { GfskParameters, LoRaParameters } from '../api/wrs';
import type { TranslationKey } from '../i18n';

export type RadioKind = 'lora' | 'gfsk';
export type RadioParameters = LoRaParameters | GfskParameters;
export type SdoAccess = 'ro' | 'rw';

export interface SdoObjectDefinition {
  address: number;
  labelKey: TranslationKey;
  access: SdoAccess;
  ordinal?: number;
  defaultWriteValue?: number;
}

export interface RadioEditorConfig {
  showSdo: boolean;
  groupMode: 'editable' | 'locked-off';
  sdoObjects: readonly SdoObjectDefinition[];
}

export interface RadioValidationError {
  field: keyof LoRaParameters | keyof GfskParameters;
  messageKey: TranslationKey;
}

export const defaultLoRaParameters = (): LoRaParameters => ({
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
  sync_word: 0x1424,
});

export const defaultGfskParameters = (): GfskParameters => ({
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
  sync_word: 0x1424,
});

const commonSdoObjects: SdoObjectDefinition[] = [
  { address: 0, labelKey: 'communicationParametersObject', access: 'rw' },
  { address: 0x001, labelKey: 'productCode', access: 'ro' },
  { address: 0x002, labelKey: 'versionNumber', access: 'ro' },
  { address: 0x003, labelKey: 'serialNumber', access: 'ro' },
];

function objectRange(start: number, end: number, labelKey: TranslationKey): SdoObjectDefinition[] {
  return Array.from({ length: end - start + 1 }, (_, index) => ({
    address: start + index,
    labelKey,
    access: 'ro' as const,
    ordinal: index + 1,
  }));
}

const firmwareSdoObjects = [
  ...objectRange(0x008, 0x011, 'appFirmwareVersion'),
  ...objectRange(0x012, 0x01b, 'bootloaderFirmwareVersion'),
  ...objectRange(0x01c, 0x025, 'appBranchName'),
  ...objectRange(0x026, 0x02f, 'appTagSha1Id'),
  ...objectRange(0x030, 0x039, 'bootBranchName'),
  ...objectRange(0x03a, 0x043, 'bootTagSha1Id'),
];

export const transmitterRadioEditorConfig: RadioEditorConfig = {
  showSdo: true,
  groupMode: 'editable',
  sdoObjects: [
    ...commonSdoObjects,
    ...firmwareSdoObjects,
    { address: 0x102, labelKey: 'battery', access: 'ro' },
    { address: 0x202, labelKey: 'upgradeRequestFlag', access: 'rw' },
  ],
};

export const receiverRadioEditorConfig: RadioEditorConfig = {
  showSdo: true,
  groupMode: 'locked-off',
  sdoObjects: [
    ...commonSdoObjects,
    ...firmwareSdoObjects,
    { address: 0x201, labelKey: 'communicationId', access: 'ro' },
    {
      address: 0x202,
      labelKey: 'upgradeRequestFlag',
      access: 'rw',
      defaultWriteValue: 0x0000454e,
    },
  ],
};

function commonValidation(parameters: RadioParameters): RadioValidationError | null {
  if (parameters.payload_len !== 12) return { field: 'payload_len', messageKey: 'payloadFixed' };
  if (parameters.tx_power < 0 || parameters.tx_power > (parameters.param_flags & 1 ? 20 : 10))
    return { field: 'tx_power', messageKey: 'powerOutOfRange' };
  return null;
}

function modulationValidation(
  kind: RadioKind,
  parameters: RadioParameters,
): RadioValidationError | null {
  if (kind === 'lora') {
    const value = parameters as LoRaParameters;
    if (value.spreading_factor <= 6 && value.preamble_len !== 12)
      return { field: 'preamble_len', messageKey: 'loraPreambleInvalid' };
  } else {
    const value = parameters as GfskParameters;
    if (4 * value.freq_deviation < value.bitrate)
      return { field: 'freq_deviation', messageKey: 'gfskRateInvalid' };
  }
  return null;
}

export function validateTransmitterRadioParameters(
  kind: RadioKind,
  parameters: RadioParameters,
): RadioValidationError | null {
  return commonValidation(parameters) ?? modulationValidation(kind, parameters);
}

export function validateReceiverRadioParameters(
  kind: RadioKind,
  parameters: RadioParameters,
): RadioValidationError | null {
  if (parameters.param_flags & 0x3fd0)
    return { field: 'param_flags', messageKey: 'parameterFlagsInvalid' };
  if (parameters.param_flags >> 14 !== (kind === 'lora' ? 0 : 1))
    return { field: 'param_flags', messageKey: 'modulationFlagsInvalid' };
  const commonError = commonValidation(parameters);
  if (commonError) return commonError;
  if (kind === 'lora' && ((parameters as LoRaParameters).sync_word & 0x0f0f) !== 0x0404)
    return { field: 'sync_word', messageKey: 'loraSyncWordInvalid' };
  return modulationValidation(kind, parameters);
}

export function findSdoObject(
  config: RadioEditorConfig,
  address: number,
): SdoObjectDefinition | undefined {
  return config.sdoObjects.find((item) => item.address === address);
}
