<script setup lang="ts">
import { ElMessageBox } from 'element-plus';
import { ref, watch } from 'vue';
import { copyRadioConfig, type RadioConfig } from '../models/radio';
const props = defineProps<{ modelValue: RadioConfig; disabled?: boolean }>();
const emit = defineEmits<{ 'update:modelValue': [value: RadioConfig] }>();
const form = ref(copyRadioConfig(props.modelValue));

watch(
  () => props.modelValue,
  (value) => {
    form.value = copyRadioConfig(value);
  },
  { deep: true },
);

function update(): void {
  emit('update:modelValue', copyRadioConfig(form.value));
}
async function changeWirelessEstop(enabled: boolean): Promise<void> {
  if (!enabled) {
    try {
      await ElMessageBox.confirm(
        '关闭 Wireless E-Stop 会降低系统安全保障。确认继续？',
        '安全敏感配置',
        {
          type: 'warning',
          confirmButtonText: '确认关闭',
          cancelButtonText: '取消',
        },
      );
    } catch {
      form.value.wirelessEstopEnabled = true;
    }
  }
  update();
}
</script>
<template>
  <div class="radio-form">
    <section>
      <h3>General</h3>
      <el-form label-position="top" class="grid">
        <el-form-item label="Modulation"
          ><el-select v-model="form.modulation" :disabled="disabled" @change="update"
            ><el-option label="LoRa" value="lora" /><el-option
              label="GFSK"
              value="gfsk" /></el-select
        ></el-form-item>
        <el-form-item label="Frequency Band"
          ><el-select v-model="form.frequencyBand" :disabled="disabled" @change="update"
            ><el-option label="433 MHz" :value="433" /><el-option
              label="915 MHz"
              :value="915" /></el-select
        ></el-form-item>
        <el-form-item label="TX Power"
          ><el-input-number
            v-model="form.txPower"
            :min="0"
            :max="form.frequencyBand === 433 ? 10 : 20"
            :disabled="disabled"
            @change="update"
          /><span class="unit">dBm</span></el-form-item
        >
        <el-form-item label="Channel Frequency Offset"
          ><el-input-number
            v-model="form.channelFrequencyOffset"
            :precision="3"
            :step="0.025"
            :disabled="disabled"
            @change="update"
          /><span class="unit">MHz</span></el-form-item
        >
        <el-form-item label="Payload Length"
          ><el-input :model-value="`${form.payloadLength} Byte`" disabled
        /></el-form-item>
        <el-form-item label="RSSI Threshold"
          ><el-input-number
            v-model="form.rssiThreshold"
            :disabled="disabled"
            @change="update"
          /><span class="unit">dBm</span></el-form-item
        >
        <el-form-item label="Heartbeat Interval"
          ><el-input-number
            v-model="form.heartbeatInterval"
            :min="1"
            :disabled="disabled"
            @change="update"
          /><span class="unit">ms</span></el-form-item
        >
        <el-form-item label="Heartbeat Loss Threshold"
          ><el-input-number
            v-model="form.heartbeatLossThreshold"
            :min="1"
            :disabled="disabled"
            @change="update"
        /></el-form-item>
        <el-form-item label="Channel Scan Mode"
          ><el-select v-model="form.channelScanMode" :disabled="disabled" @change="update"
            ><el-option label="Single Channel" value="single" /><el-option
              label="Channel Scan"
              value="scan" /></el-select
        ></el-form-item>
        <el-form-item label="Group Mode"
          ><el-select v-model="form.groupMode" :disabled="disabled" @change="update"
            ><el-option label="One to One" value="one-to-one" /><el-option
              label="One to Many"
              value="one-to-many" /></el-select
        ></el-form-item>
        <el-form-item label="Heartbeat Enable"
          ><el-switch v-model="form.heartbeatEnabled" :disabled="disabled" @change="update"
        /></el-form-item>
        <el-form-item label="Wireless E-Stop Enable"
          ><el-switch
            :model-value="form.wirelessEstopEnabled"
            :disabled="disabled"
            @change="changeWirelessEstop"
        /></el-form-item>
        <el-form-item label="PHY CRC Enable"
          ><el-switch v-model="form.phyCrcEnabled" :disabled="disabled" @change="update"
        /></el-form-item>
      </el-form>
    </section>
    <section v-if="form.modulation === 'lora'">
      <h3>LoRa Parameters</h3>
      <el-form label-position="top" class="grid">
        <el-form-item label="Bandwidth"
          ><el-select v-model="form.lora.bandwidth" :disabled="disabled" @change="update"
            ><el-option
              v-for="value in [125, 250, 500]"
              :key="value"
              :label="`${value} kHz`"
              :value="value" /></el-select
        ></el-form-item>
        <el-form-item label="Spreading Factor"
          ><el-select v-model="form.lora.spreadingFactor" :disabled="disabled" @change="update"
            ><el-option
              v-for="value in [5, 6, 7, 8, 9, 10, 11, 12]"
              :key="value"
              :label="`SF${value}`"
              :value="value" /></el-select
        ></el-form-item>
        <el-form-item label="Coding Rate"
          ><el-select v-model="form.lora.codingRate" :disabled="disabled" @change="update"
            ><el-option
              v-for="value in ['4/5', '4/6', '4/7', '4/8', 'LI4/5', 'LI4/6', 'LI4/8']"
              :key="value"
              :label="value"
              :value="value" /></el-select
        ></el-form-item>
        <el-form-item label="Header Type"
          ><el-select v-model="form.lora.headerType" :disabled="disabled" @change="update"
            ><el-option label="Explicit Header" value="explicit" /><el-option
              label="Implicit Header"
              value="implicit" /></el-select
        ></el-form-item>
        <el-form-item label="Preamble Length"
          ><el-input-number
            v-model="form.lora.preambleLength"
            :min="10"
            :max="50"
            :disabled="disabled"
            @change="update"
          /><span class="unit">symbol</span></el-form-item
        >
        <el-form-item label="Sync Word"
          ><el-input v-model="form.lora.syncWord" :disabled="disabled" @change="update"
        /></el-form-item>
      </el-form>
    </section>
    <section v-else>
      <h3>GFSK Parameters</h3>
      <el-alert
        title="GFSK 参数为协议支持能力，是否纳入正式 V1 配置范围待确认。"
        type="info"
        :closable="false"
        show-icon
        class="notice"
      /><el-form label-position="top" class="grid">
        <el-form-item label="Receive Bandwidth"
          ><el-select v-model="form.gfsk.receiveBandwidth" :disabled="disabled" @change="update"
            ><el-option
              v-for="value in [117.3, 234.3, 467]"
              :key="value"
              :label="`${value} kHz`"
              :value="value" /></el-select
        ></el-form-item>
        <el-form-item label="Bit Rate"
          ><el-input-number
            v-model="form.gfsk.bitRate"
            :min="600"
            :max="150000"
            :disabled="disabled"
            @change="update"
          /><span class="unit">bps</span></el-form-item
        >
        <el-form-item label="Frequency Deviation"
          ><el-input-number
            v-model="form.gfsk.frequencyDeviation"
            :min="600"
            :max="300000"
            :disabled="disabled"
            @change="update"
          /><span class="unit">Hz</span></el-form-item
        >
        <el-form-item label="Pulse Shaping"
          ><el-select v-model="form.gfsk.pulseShaping" :disabled="disabled" @change="update"
            ><el-option label="None" value="none" /><el-option
              label="BT 0.3"
              value="bt-0.3" /><el-option label="BT 0.5" value="bt-0.5" /><el-option
              label="BT 0.7"
              value="bt-0.7" /><el-option label="BT 1.0" value="bt-1.0" /></el-select
        ></el-form-item>
        <el-form-item label="Preamble Length"
          ><el-input-number
            v-model="form.gfsk.preambleLength"
            :min="16"
            :max="255"
            :disabled="disabled"
            @change="update"
          /><span class="unit">bit</span></el-form-item
        >
        <el-form-item label="Sync Word"
          ><el-input v-model="form.gfsk.syncWord" :disabled="disabled" @change="update"
        /></el-form-item>
      </el-form>
    </section>
  </div>
</template>
<style scoped>
section {
  padding: 20px 22px;
  margin-bottom: 18px;
  background: var(--app-panel);
  border: 1px solid var(--app-border);
  border-radius: 6px;
}
h3 {
  margin: 0 0 14px;
  font-size: 15px;
}
.grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 0 18px;
}
.el-form-item {
  position: relative;
  margin-bottom: 14px;
}
.unit {
  position: absolute;
  right: 9px;
  bottom: 8px;
  color: var(--app-muted);
  font-size: 12px;
  pointer-events: none;
}
.el-input-number {
  width: 100%;
}
.notice {
  margin: -2px 0 16px;
}
</style>
