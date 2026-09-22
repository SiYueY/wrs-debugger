<script setup lang="ts">
import { computed } from 'vue';
import type { GfskParameters, LoRaParameters } from '../api/wrs';
import { t } from '../i18n';

type RadioParameters = LoRaParameters | GfskParameters;
const props = defineProps<{ kind: 'lora' | 'gfsk'; disabled?: boolean }>();
const parameters = defineModel<RadioParameters>({ required: true });
const isLora = computed(() => props.kind === 'lora');
const loraParameters = computed(() => parameters.value as LoRaParameters);
const gfskParameters = computed(() => parameters.value as GfskParameters);
const rawFlags = computed(
  () => `0x${parameters.value.param_flags.toString(16).padStart(4, '0').toUpperCase()}`,
);

function isSet(bit: number) {
  return (parameters.value.param_flags & (1 << bit)) !== 0;
}
function setBit(bit: number, enabled: boolean) {
  const typeBits = props.kind === 'lora' ? 0 : 0x4000;
  let flags = parameters.value.param_flags & 0x003f;
  flags &= ~0x10;
  flags = enabled ? flags | (1 << bit) : flags & ~(1 << bit);
  parameters.value = { ...parameters.value, param_flags: typeBits | flags };
}
function setNumber(field: string, event: Event) {
  const input = event.target as HTMLInputElement;
  parameters.value = { ...parameters.value, [field]: Number(input.value) } as RadioParameters;
}
</script>
<template>
  <div class="parameter-grid">
    <div class="parameter-column">
      <div class="field">
        <label>{{ t('parameterFlags') }}</label
        ><output>{{ rawFlags }}</output
        ><span>bit</span>
      </div>
      <div class="field">
        <label>{{ t('operationType') }}</label
        ><output>{{ isLora ? 'LoRa' : 'GFSK' }}</output
        ><span></span>
      </div>
      <div class="field">
        <label>{{ t('channelScan') }}</label
        ><select
          :disabled="disabled"
          :value="isSet(5) ? 'hopping' : 'single'"
          @change="setBit(5, ($event.target as HTMLSelectElement).value === 'hopping')"
        >
          <option value="single">{{ t('singleChannel') }}</option>
          <option value="hopping">{{ t('hopping') }}</option></select
        ><span></span>
      </div>
      <div class="field">
        <label>{{ t('groupMode') }}</label
        ><output>{{ t('oneToOne') }}</output
        ><span></span>
      </div>
      <div class="field">
        <label>{{ t('heartbeat') }}</label
        ><select
          :disabled="disabled"
          :value="isSet(3) ? 'off' : 'on'"
          @change="setBit(3, ($event.target as HTMLSelectElement).value === 'off')"
        >
          <option value="on">{{ t('enabled') }}</option>
          <option value="off">{{ t('disabled') }}</option></select
        ><span></span>
      </div>
      <div class="field">
        <label>{{ t('wirelessEstop') }}</label
        ><select
          :disabled="disabled"
          :value="isSet(2) ? 'off' : 'on'"
          @change="setBit(2, ($event.target as HTMLSelectElement).value === 'off')"
        >
          <option value="on">{{ t('enabled') }}</option>
          <option value="off">{{ t('disabled') }}</option></select
        ><span></span>
      </div>
      <div class="field">
        <label>{{ t('physicalCrc') }}</label
        ><select
          :disabled="disabled"
          :value="isSet(1) ? 'off' : 'on'"
          @change="setBit(1, ($event.target as HTMLSelectElement).value === 'off')"
        >
          <option value="on">{{ t('enabled') }}</option>
          <option value="off">{{ t('disabled') }}</option></select
        ><span></span>
      </div>
      <div class="field">
        <label>{{ t('band') }}</label
        ><select
          :disabled="disabled"
          :value="isSet(0) ? 915 : 433"
          @change="setBit(0, Number(($event.target as HTMLSelectElement).value) === 915)"
        >
          <option :value="433">433</option>
          <option :value="915">915</option></select
        ><span>MHz</span>
      </div>
      <div class="field">
        <label>{{ t('txPower') }}</label
        ><input
          :disabled="disabled"
          type="number"
          :min="0"
          :max="isSet(0) ? 20 : 10"
          :value="parameters.tx_power"
          @input="setNumber('tx_power', $event)"
        /><span>dBm</span>
      </div>
      <div class="field">
        <label>{{ t('frequencyOffset') }}</label
        ><input
          :disabled="disabled"
          type="number"
          min="0"
          max="65535"
          :value="parameters.freq_offset"
          @input="setNumber('freq_offset', $event)"
        /><span>kHz</span>
      </div>
    </div>
    <div class="parameter-column">
      <div class="field">
        <label>{{ t('payloadLength') }}</label
        ><output>{{ parameters.payload_len }}</output
        ><span>Byte</span>
      </div>
      <div class="field">
        <label>{{ t('rssiThreshold') }}</label
        ><input
          :disabled="disabled"
          type="number"
          min="10"
          max="148"
          :value="parameters.rssi_threshold"
          @input="setNumber('rssi_threshold', $event)"
        /><span>dBm</span>
      </div>
      <div class="field">
        <label>{{ t('heartbeatInterval') }}</label
        ><input
          :disabled="disabled"
          type="number"
          min="200"
          max="10000"
          :value="parameters.heartbeat_interval"
          @input="setNumber('heartbeat_interval', $event)"
        /><span>ms</span>
      </div>
      <div class="field">
        <label>{{ t('heartbeatLoss') }}</label
        ><input
          :disabled="disabled"
          type="number"
          min="1"
          max="255"
          :value="parameters.heartbeat_loss"
          @input="setNumber('heartbeat_loss', $event)"
        /><span>{{ t('packets') }}</span>
      </div>
      <template v-if="isLora">
        <div class="field">
          <label>{{ t('receiveBandwidth') }}</label
          ><select
            :disabled="disabled"
            :value="parameters.bandwidth"
            @change="setNumber('bandwidth', $event)"
          >
            <option :value="0">{{ t('narrow') }}</option>
            <option :value="1">{{ t('medium') }}</option>
            <option :value="2">{{ t('wide') }}</option></select
          ><span>kHz</span>
        </div>
        <div class="field">
          <label>{{ t('spreadingFactor') }}</label
          ><input
            :disabled="disabled"
            type="number"
            min="5"
            max="12"
            :value="loraParameters.spreading_factor"
            @input="setNumber('spreading_factor', $event)"
          /><span></span>
        </div>
        <div class="field">
          <label>{{ t('codingRate') }}</label
          ><input
            :disabled="disabled"
            type="number"
            min="0"
            max="6"
            :value="loraParameters.coding_rate"
            @input="setNumber('coding_rate', $event)"
          /><span></span>
        </div>
        <div class="field">
          <label>{{ t('headerType') }}</label
          ><select
            :disabled="disabled"
            :value="loraParameters.header_type"
            @change="setNumber('header_type', $event)"
          >
            <option :value="0">{{ t('explicit') }}</option>
            <option :value="1">{{ t('implicit') }}</option></select
          ><span></span>
        </div>
      </template>
      <template v-else>
        <div class="field">
          <label>{{ t('receiveBandwidth') }}</label
          ><select
            :disabled="disabled"
            :value="parameters.bandwidth"
            @change="setNumber('bandwidth', $event)"
          >
            <option :value="0">{{ t('narrow') }}</option>
            <option :value="1">{{ t('medium') }}</option>
            <option :value="2">{{ t('wide') }}</option></select
          ><span>kHz</span>
        </div>
        <div class="field">
          <label>{{ t('bitrate') }}</label
          ><input
            :disabled="disabled"
            type="number"
            min="600"
            max="150000"
            :value="gfskParameters.bitrate"
            @input="setNumber('bitrate', $event)"
          /><span>bps</span>
        </div>
        <div class="field">
          <label>{{ t('frequencyDeviation') }}</label
          ><input
            :disabled="disabled"
            type="number"
            min="600"
            max="300000"
            :value="gfskParameters.freq_deviation"
            @input="setNumber('freq_deviation', $event)"
          /><span>Hz</span>
        </div>
        <div class="field">
          <label>{{ t('pulseShaping') }}</label
          ><select
            :disabled="disabled"
            :value="gfskParameters.pulse_shaping"
            @change="setNumber('pulse_shaping', $event)"
          >
            <option :value="0">{{ t('none') }}</option>
            <option :value="8">8</option>
            <option :value="9">9</option>
            <option :value="10">10</option>
            <option :value="11">11</option></select
          ><span></span>
        </div>
      </template>
      <div class="field">
        <label>{{ t('preambleLength') }}</label
        ><input
          :disabled="disabled"
          type="number"
          :min="isLora ? 10 : 16"
          :max="isLora ? 50 : 255"
          :value="parameters.preamble_len"
          @input="setNumber('preamble_len', $event)"
        /><span>symbol</span>
      </div>
      <div class="field">
        <label>{{ t('syncWord') }}</label
        ><input
          :disabled="disabled"
          type="number"
          min="0"
          max="65535"
          :value="parameters.sync_word"
          @input="setNumber('sync_word', $event)"
        /><span>bit</span>
      </div>
    </div>
  </div>
</template>
<style scoped>
.parameter-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 24px;
}
.parameter-column {
  display: grid;
  gap: 4px;
}
.field {
  display: grid;
  grid-template-columns: minmax(110px, 1fr) 110px 44px;
  align-items: center;
  gap: 6px;
  min-height: var(--field-height);
}
label {
  color: #262626;
  font-size: 13px;
}
input,
select,
output {
  box-sizing: border-box;
  width: 100%;
  height: var(--field-height);
  border: 1px solid var(--input-border);
  border-radius: 2px;
  padding: 3px 7px;
  background: #fff;
  font: inherit;
}
output {
  display: flex;
  align-items: center;
  background: #f3f3f3;
  color: #555;
}
.field > span {
  color: #262626;
  font-size: 12px;
}
@media (max-width: 840px) {
  .parameter-grid {
    grid-template-columns: 1fr;
  }
}
</style>
