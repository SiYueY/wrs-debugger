<script setup lang="ts">
import { computed } from 'vue';
import type { GfskParameters, LoRaParameters } from '../api/wrs';
import { t } from '../i18n';

type RadioParameters = LoRaParameters | GfskParameters;
type RadioField = { key: string; label: string; unit: string; width?: number };
const props = defineProps<{ kind: 'lora' | 'gfsk'; disabled?: boolean; objectAddress: number; objectData: number; operation: 'read' | 'write'; responseStatus: number | null; resultCode: number | null }>();
const parameters = defineModel<RadioParameters>({ required: true });
const emit = defineEmits<{ 'update:objectAddress': [value: number]; 'update:objectData': [value: number] }>();
const isLora = computed(() => props.kind === 'lora');
const radioFields = computed<RadioField[]>(() => [
  { key: 'tx_power', label: t('txPower'), unit: 'dBm', width: 4 },
  { key: 'freq_offset', label: t('frequencyOffset'), unit: 'kHz', width: 4 },
  { key: 'payload_len', label: t('payloadLength'), unit: 'Byte' },
  { key: 'rssi_threshold', label: t('rssiThreshold'), unit: 'dBm' },
  { key: 'heartbeat_interval', label: t('heartbeatInterval'), unit: 'ms', width: 4 },
  { key: 'heartbeat_loss', label: t('heartbeatLoss'), unit: t('packets') },
  { key: 'bandwidth', label: t('receiveBandwidth'), unit: 'kHz' },
  ...(isLora.value
    ? [{ key: 'spreading_factor', label: t('spreadingFactor'), unit: '' }, { key: 'coding_rate', label: t('codingRate'), unit: '' }, { key: 'header_type', label: t('headerType'), unit: '' }]
    : [{ key: 'bitrate', label: t('bitrate'), unit: 'bps', width: 8 }, { key: 'freq_deviation', label: t('frequencyDeviation'), unit: 'Hz', width: 8 }, { key: 'pulse_shaping', label: t('pulseShaping'), unit: '' }]),
  { key: 'preamble_len', label: t('preambleLength'), unit: isLora.value ? 'symbol' : 'bit' },
  { key: 'sync_word', label: t('syncWord'), unit: 'bit', width: 4 },
]);
const hex = (value: number, width = 2) => Math.max(0, value).toString(16).toUpperCase().padStart(width, '0');
const displayHex = (value: number, width = 2) => `0X${hex(value, width)}`;
function parseHex(event: Event): number | null {
  const text = (event.target as HTMLInputElement).value.trim();
  if (!/^[0-9a-fA-F]+$/.test(text)) return null;
  const value = Number.parseInt(text, 16);
  return Number.isFinite(value) ? value : null;
}
function normalize(event: Event, value: number, width: number) { (event.target as HTMLInputElement).value = hex(value, width); }
function setNumber(field: string, event: Event, width = 2) { const value = parseHex(event); if (value !== null) { parameters.value = { ...parameters.value, [field]: value } as RadioParameters; normalize(event, value, width); } }
function setObjectData(event: Event) { const value = parseHex(event); if (value !== null) { emit('update:objectData', value); normalize(event, value, 8); } }
function isSet(bit: number) { return (parameters.value.param_flags & (1 << bit)) !== 0; }
function setBit(bit: number, enabled: boolean) { const typeBits = isLora.value ? 0 : 0x4000; let flags = parameters.value.param_flags & 0x003f; flags = enabled ? flags | (1 << bit) : flags & ~(1 << bit); parameters.value = { ...parameters.value, param_flags: typeBits | flags } as RadioParameters; }
const objects = computed(() => {
  const result: Array<{ value: number; label: string }> = [{ value: 0, label: `0X000 (${t('communicationParametersObject')})` }, { value: 1, label: `0X001 (${t('productCode')})` }, { value: 2, label: `0X002 (${t('versionNumber')})` }, { value: 3, label: `0X003 (${t('serialNumber')})` }];
  const groups = [[0x008, 0x011, t('appFirmwareVersion')], [0x012, 0x01b, t('bootloaderFirmwareVersion')], [0x01c, 0x025, t('appBranchName')], [0x026, 0x02f, t('appTagSha1Id')], [0x030, 0x039, t('bootBranchName')], [0x03a, 0x043, t('bootTagSha1Id')]] as const;
  for (const [start, end, name] of groups) for (let value = start; value <= end; value += 1) result.push({ value, label: `${displayHex(value, 3)} (${name} #${value - start + 1})` });
  result.push({ value: 0x102, label: `0X102 (${t('battery')})` }, { value: 0x202, label: `0X202 (${t('upgradeRequestFlag')})` }); return result;
});
</script>
<template>
  <div class="parameter-grid">
    <section class="parameter-column">
      <h3>{{ t('sdo') }}</h3>
      <div class="field"><label>{{ t('requestOpcode') }}</label><output>{{ operation === 'read' ? `0X0 (${t('readOperation')})` : `0X1 (${t('writeOperation')})` }}</output><span /></div>
      <div class="field"><label>{{ t('objectAddress') }}</label><select :disabled="disabled" :value="objectAddress" @change="emit('update:objectAddress', Number(($event.target as HTMLSelectElement).value))"><option v-for="item in objects" :key="item.value" :value="item.value">{{ item.label }}</option></select><span /></div>
      <div class="field"><label>{{ t('objectData') }}</label><div class="hex-input"><span>0X</span><input :disabled="disabled || objectAddress === 0" :value="hex(objectData, 8)" @change="setObjectData" /></div><span /></div>
      <div class="field"><label>{{ t('responseSdoStatus') }}</label><output>{{ responseStatus === null ? '—' : displayHex(responseStatus) }}</output><span /></div>
      <div class="field"><label>{{ t('resultCode') }}</label><output>{{ resultCode === null ? '—' : displayHex(resultCode) }}</output><span /></div>

      <h3>{{ t('parameterFlags') }}</h3>
      <div class="field"><label>{{ t('rawParameterFlags') }}</label><output>{{ displayHex(parameters.param_flags, 4) }}</output><span /></div>
      <div class="field"><label>{{ t('operationType') }}</label><output>{{ isLora ? '0X0 (LoRa)' : '0X1 (GFSK)' }}</output><span /></div>
      <div class="field"><label>{{ t('channelScan') }}</label><select :disabled="disabled" :value="isSet(5)" @change="setBit(5, ($event.target as HTMLSelectElement).value === 'true')"><option :value="false">0X0 ({{ t('singleChannel') }})</option><option :value="true">0X1 ({{ t('hopping') }})</option></select><span /></div>
      <div class="field"><label>{{ t('groupMode') }}</label><select :disabled="disabled" :value="isSet(4)" @change="setBit(4, ($event.target as HTMLSelectElement).value === 'true')"><option :value="false">0X0 ({{ t('oneToOne') }})</option><option :value="true">0X1 ({{ t('oneToMany') }})</option></select><span /></div>
      <div class="field"><label>{{ t('heartbeat') }}</label><select :disabled="disabled" :value="isSet(3)" @change="setBit(3, ($event.target as HTMLSelectElement).value === 'true')"><option :value="false">0X0 ({{ t('enabled') }})</option><option :value="true">0X1 ({{ t('disabled') }})</option></select><span /></div>
      <div class="field"><label>{{ t('wirelessEstop') }}</label><select :disabled="disabled" :value="isSet(2)" @change="setBit(2, ($event.target as HTMLSelectElement).value === 'true')"><option :value="false">0X0 ({{ t('enabled') }})</option><option :value="true">0X1 ({{ t('disabled') }})</option></select><span /></div>
      <div class="field"><label>{{ t('physicalCrc') }}</label><select :disabled="disabled" :value="isSet(1)" @change="setBit(1, ($event.target as HTMLSelectElement).value === 'true')"><option :value="false">0X0 ({{ t('enabled') }})</option><option :value="true">0X1 ({{ t('disabled') }})</option></select><span /></div>
      <div class="field"><label>{{ t('band') }}</label><select :disabled="disabled" :value="isSet(0)" @change="setBit(0, ($event.target as HTMLSelectElement).value === 'true')"><option :value="false">0X0 (433 MHz)</option><option :value="true">0X1 (915 MHz)</option></select><span /></div>
    </section>
    <section class="parameter-column radio-parameters">
      <div v-for="field in radioFields" :key="field.key" class="field">
        <label>{{ field.label }}</label>
        <div class="hex-input"><span>0X</span><input :disabled="disabled" :value="hex((parameters as any)[field.key], field.width)" @change="setNumber(field.key, $event, field.width)" /></div>
        <span>{{ field.unit }}</span>
      </div>
    </section>
  </div>
</template>
<style scoped>
.parameter-grid{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:24px;align-items:start}.parameter-column{display:grid;gap:5px;align-content:start}.parameter-column h3{grid-column:1 / -1;margin:3px 0 1px;color:#262626;font-size:13px;font-weight:700}.field{display:grid;grid-template-columns:minmax(144px,1fr) minmax(150px,1.2fr) 54px;gap:8px;align-items:center;min-width:0}.field>label{color:#262626;font-size:13px}.field>span{color:#4b5563;font-size:12px;white-space:nowrap}input,select,output,.hex-input{box-sizing:border-box;width:100%;height:var(--field-height);border:1px solid var(--input-border);border-radius:3px;background:#fff;font:inherit}output{display:flex;align-items:center;padding:3px 7px;background:#f4f6f8;color:#4b5563;font-variant-numeric:tabular-nums}.hex-input{display:flex;align-items:center;overflow:hidden;padding-left:7px;gap:3px}.hex-input>span{color:#4b5563;font-family:monospace;font-size:13px;font-weight:700;user-select:none}.hex-input>input{height:100%;min-width:0;border:0;border-radius:0;padding:3px 0;font-family:monospace;outline:0}.hex-input:focus-within{border-color:#4d90fe;box-shadow:0 0 0 1px #4d90fe}@media(max-width:920px){.parameter-grid{grid-template-columns:1fr}}
</style>
