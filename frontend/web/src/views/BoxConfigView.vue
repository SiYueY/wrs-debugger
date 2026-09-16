<script setup lang="ts">
import { ref } from 'vue';
import { ElMessage, ElMessageBox } from 'element-plus';
import { storeToRefs } from 'pinia';
import RadioConfigForm from '../components/RadioConfigForm.vue';
import ConnectionStatus from '../components/ConnectionStatus.vue';
import { copyRadioConfig, validateRadioConfig } from '../models/radio';
import { useBoxStore } from '../stores/box';
import { useConnectionStore } from '../stores/connection';
const box = useBoxStore();
const connection = useConnectionStore();
const { deviceId, config, loading } = storeToRefs(box);
const { serialState } = storeToRefs(connection);
const draft = ref(copyRadioConfig(config.value));
const pin = ref('');
async function read(): Promise<void> {
  await box.read();
  draft.value = copyRadioConfig(config.value);
  ElMessage.success('Box configuration read successfully.');
}
async function apply(): Promise<void> {
  const error = validateRadioConfig(draft.value);
  if (error) {
    ElMessage.error(error);
    return;
  }
  await box.apply(draft.value);
  ElMessage.success('Box configuration applied.');
}
async function applyPin(): Promise<void> {
  if (!/^\d{6}$/.test(pin.value)) {
    ElMessage.error('PIN must contain exactly 6 digits.');
    return;
  }
  await box.setPin(pin.value);
  pin.value = '';
  ElMessage.success('PIN applied.');
}
async function restore(): Promise<void> {
  try {
    await ElMessageBox.confirm(
      'Restore all wireless settings to their default values?',
      'Restore Default',
      { type: 'warning' },
    );
    await box.restoreDefaults();
    draft.value = copyRadioConfig(config.value);
    ElMessage.success('Default configuration restored.');
  } catch {}
}
</script>
<template>
  <div class="page">
    <div class="page-heading">
      <div>
        <h1>无线急停盒</h1>
        <p>通过模拟服务配置无线急停盒参数。</p>
      </div>
      <div class="actions">
        <el-button :loading="loading" @click="read">Read</el-button
        ><el-button type="primary" :loading="loading" @click="apply">Apply</el-button
        ><el-button type="danger" plain :loading="loading" @click="restore"
          >Restore Default</el-button
        >
      </div>
    </div>
    <el-card shadow="never" class="info"
      ><template #header>Device Information</template
      ><el-descriptions :column="2" border
        ><el-descriptions-item label="Device ID">{{ deviceId }}</el-descriptions-item
        ><el-descriptions-item label="Connection State"
          ><ConnectionStatus
            :state="serialState" /></el-descriptions-item></el-descriptions></el-card
    ><el-card shadow="never" class="pin"
      ><template #header>PIN Configuration</template
      ><el-input
        v-model="pin"
        maxlength="6"
        inputmode="numeric"
        placeholder="Enter 6-digit PIN"
      /><el-button :loading="loading" @click="applyPin">Apply</el-button></el-card
    ><RadioConfigForm v-model="draft" :disabled="loading" />
  </div>
</template>
<style scoped>
.page {
  max-width: 1180px;
}
.page-heading {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  margin-bottom: 22px;
}
h1 {
  margin: 0 0 7px;
  font-size: 24px;
}
p {
  margin: 0;
  color: var(--app-muted);
  font-size: 14px;
}
.actions {
  display: flex;
  gap: 10px;
}
.info,
.pin {
  margin-bottom: 18px;
}
.pin :deep(.el-card__body) {
  display: flex;
  max-width: 480px;
  gap: 10px;
}
.pin .el-input {
  max-width: 260px;
}
</style>
