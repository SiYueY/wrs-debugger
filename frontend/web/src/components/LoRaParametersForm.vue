<script setup lang="ts">
import type { LoRaParameters } from '../api/wrs';
import { t } from '../i18n';
const value = defineModel<LoRaParameters>({ required: true });
const disabled = defineProps<{ disabled?: boolean }>().disabled;
const bands = [0, 1, 2];
const sf = [5, 6, 7, 8, 9, 10, 11, 12];
const coding = [0, 1, 2, 3, 4, 5, 6];
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
        <option v-for="item in bands" :key="item" :value="item">{{ [125, 250, 500][item] }}</option>
      </select></label
    ><span>uint8</span><span>kHz</span
    ><label
      >{{ t('spreadingFactor')
      }}<select v-model.number="value.spreading_factor" :disabled="disabled">
        <option v-for="item in sf" :key="item" :value="item">SF{{ item }}</option>
      </select></label
    ><span>uint8</span><span>-</span
    ><label
      >{{ t('codingRate')
      }}<select v-model.number="value.coding_rate" :disabled="disabled">
        <option v-for="item in coding" :key="item" :value="item">{{ item }}</option>
      </select></label
    ><span>uint8</span><span>-</span
    ><label
      >{{ t('headerType')
      }}<select v-model.number="value.header_type" :disabled="disabled">
        <option :value="0">{{ t('explicit') }}</option>
        <option :value="1">{{ t('implicit') }}</option>
      </select></label
    ><span>uint8</span><span>-</span
    ><label
      >{{ t('preambleLength')
      }}<input
        v-model.number="value.preamble_len"
        type="number"
        min="10"
        max="50"
        :disabled="disabled" /></label
    ><span>uint8</span><span>symbol</span
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
span {
  font-variant-numeric: tabular-nums;
}
</style>
