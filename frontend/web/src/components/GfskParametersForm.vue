<script setup lang="ts">
import type { GfskParameters } from '../api/wrs';
import { t } from '../i18n';
const value = defineModel<GfskParameters>({ required: true });
const disabled = defineProps<{ disabled?: boolean }>().disabled;
</script>
<template>
  <div class="table">
    <div class="head">{{ t('parameterName') }}</div>
    <div class="head">{{ t('inputValue') }}</div>
    <div class="head">{{ t('unit') }}</div>
    <label
      >{{ t('parameterFlags')
      }}<input
        v-model.number="value.param_flags"
        type="number"
        min="0"
        max="65535"
        :disabled="disabled" /></label
    ><span>uint16</span><span>-</span
    ><label
      >{{ t('txPower')
      }}<input
        v-model.number="value.tx_power"
        type="number"
        min="0"
        :max="value.param_flags & 1 ? 20 : 10"
        :disabled="disabled" /></label
    ><span>int16</span><span>dBm</span
    ><label
      >{{ t('frequencyOffset')
      }}<input
        v-model.number="value.freq_offset"
        type="number"
        min="0"
        max="65535"
        :disabled="disabled" /></label
    ><span>uint16</span><span>kHz</span
    ><label>{{ t('payloadLength') }}<input :value="value.payload_len" disabled /></label
    ><span>uint8</span><span>Byte</span
    ><label
      >{{ t('rssiThreshold')
      }}<input
        v-model.number="value.rssi_threshold"
        type="number"
        min="10"
        max="148"
        :disabled="disabled" /></label
    ><span>uint8</span><span>dBm</span
    ><label
      >{{ t('heartbeatInterval')
      }}<input
        v-model.number="value.heartbeat_interval"
        type="number"
        min="200"
        max="10000"
        :disabled="disabled" /></label
    ><span>uint16</span><span>ms</span
    ><label
      >{{ t('heartbeatLoss')
      }}<input
        v-model.number="value.heartbeat_loss"
        type="number"
        min="1"
        max="255"
        :disabled="disabled" /></label
    ><span>uint8</span><span>{{ t('packets') }}</span
    ><label
      >{{ t('receiveBandwidth')
      }}<select v-model.number="value.bandwidth" :disabled="disabled">
        <option :value="0">117.3</option>
        <option :value="1">234.3</option>
        <option :value="2">467</option>
      </select></label
    ><span>uint8</span><span>kHz</span
    ><label
      >{{ t('bitrate')
      }}<input
        v-model.number="value.bitrate"
        type="number"
        min="600"
        max="150000"
        :disabled="disabled" /></label
    ><span>uint32</span><span>bps</span
    ><label
      >{{ t('frequencyDeviation')
      }}<input
        v-model.number="value.freq_deviation"
        type="number"
        min="600"
        max="300000"
        :disabled="disabled" /></label
    ><span>uint32</span><span>Hz</span
    ><label
      >{{ t('pulseShaping')
      }}<select v-model.number="value.pulse_shaping" :disabled="disabled">
        <option :value="0">{{ t('none') }}</option>
        <option :value="8">BT 0.3</option>
        <option :value="9">BT 0.5</option>
        <option :value="10">BT 0.7</option>
        <option :value="11">BT 1.0</option>
      </select></label
    ><span>uint8</span><span>-</span
    ><label
      >{{ t('preambleLength')
      }}<input
        v-model.number="value.preamble_len"
        type="number"
        min="16"
        max="255"
        :disabled="disabled" /></label
    ><span>uint8</span><span>bit</span
    ><label
      >{{ t('syncWord')
      }}<input
        v-model.number="value.sync_word"
        type="number"
        min="0"
        max="65535"
        :disabled="disabled" /></label
    ><span>uint16</span><span>hex</span>
  </div>
</template>
<style scoped>
.table {
  display: grid;
  grid-template-columns: minmax(240px, 1fr) minmax(170px, 240px) 100px;
  gap: 8px 14px;
  align-items: center;
}
.head {
  font-weight: 700;
  color: #666;
}
label {
  display: contents;
}
input,
select {
  width: 100%;
  height: 30px;
  padding: 0 8px;
  border: 1px solid var(--input-border);
  border-radius: 4px;
  background: #fff;
}
</style>
