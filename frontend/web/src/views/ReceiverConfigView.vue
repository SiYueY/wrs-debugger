<script setup lang="ts">
import { ref } from 'vue';
import { ElMessage, ElMessageBox } from 'element-plus';
import { storeToRefs } from 'pinia';
import RadioConfigForm from '../components/RadioConfigForm.vue';
import ConnectionStatus from '../components/ConnectionStatus.vue';
import { copyRadioConfig, validateRadioConfig, type RadioConfig } from '../models/radio';
import { useBoxStore } from '../stores/box';
import { useConnectionStore } from '../stores/connection';
import { useReceiverStore } from '../stores/receiver';
const receiver = useReceiverStore();
const box = useBoxStore();
const connection = useConnectionStore();
const { boundDeviceId, config, loading } = storeToRefs(receiver);
const { robotState } = storeToRefs(connection);
const draft = ref(copyRadioConfig(config.value));
const bindingVisible = ref(false);
const bindingStep = ref(0);
function updateConfig(next: RadioConfig): void {
  draft.value = copyRadioConfig(next);
}
async function read(): Promise<void> {
  await receiver.read();
  draft.value = copyRadioConfig(config.value);
  ElMessage.success('Receiver configuration read successfully.');
}
async function apply(): Promise<void> {
  const error = validateRadioConfig(draft.value);
  if (error) {
    ElMessage.error(error);
    return;
  }
  await receiver.apply(draft.value);
  ElMessage.success('Receiver configuration applied.');
}
async function restore(): Promise<void> {
  try {
    await ElMessageBox.confirm(
      'Restore all receiver wireless settings to their default values?',
      'Restore Default',
      { type: 'warning' },
    );
    await receiver.restoreDefaults();
    draft.value = copyRadioConfig(config.value);
    ElMessage.success('Default configuration restored.');
  } catch {}
}
async function sync(): Promise<void> {
  try {
    await ElMessageBox.confirm(
      '将读取无线急停盒当前通信参数，并写入机器人接收板。',
      'Sync From E-Stop Box',
      { type: 'warning' },
    );
    const reading = ElMessage({ message: 'Reading box config…', duration: 0 });
    await box.read();
    reading.close();
    const writing = ElMessage({
      message: 'Writing receiver config…',
      duration: 0,
    });
    await receiver.syncFromBox(box.config);
    writing.close();
    draft.value = copyRadioConfig(config.value);
    ElMessage.success('Completed');
  } catch {}
}
async function bind(): Promise<void> {
  bindingVisible.value = true;
  bindingStep.value = 0;
  for (let step = 0; step < 4; step += 1) {
    await receiver.factoryBind();
    bindingStep.value = step + 1;
  }
  ElMessage.success('Binding Completed');
}
</script>
<template>
  <div class="page">
    <div class="page-heading">
      <div>
        <h1>机器人接收板</h1>
        <p>配置机器人侧 Sub_1G 接收板。</p>
      </div>
      <div class="actions">
        <el-button :loading="loading" @click="read">Read</el-button
        ><el-button type="primary" :loading="loading" @click="apply">Apply</el-button
        ><el-button :loading="loading" @click="sync">Sync From E-Stop Box</el-button
        ><el-button type="warning" plain :loading="loading" @click="bind">Factory Bind</el-button
        ><el-button type="danger" plain :loading="loading" @click="restore"
          >Restore Default</el-button
        >
      </div>
    </div>
    <el-card shadow="never" class="info"
      ><el-descriptions :column="2" border
        ><el-descriptions-item label="Robot Connection State"
          ><ConnectionStatus :state="robotState" /></el-descriptions-item
        ><el-descriptions-item label="Bound Device ID">{{
          boundDeviceId
        }}</el-descriptions-item></el-descriptions
      ></el-card
    ><RadioConfigForm
      v-model="draft"
      :disabled="loading"
      @update:model-value="updateConfig"
    /><el-dialog
      v-model="bindingVisible"
      title="Factory Bind"
      width="520px"
      :close-on-click-modal="false"
      ><el-steps :active="bindingStep" finish-status="success"
        ><el-step title="Prepare E-Stop Box" /><el-step title="Configure Receiver" /><el-step
          title="Verify Wireless Link" /><el-step title="Binding Completed" /></el-steps
      ><template #footer
        ><el-button :disabled="bindingStep < 4" type="primary" @click="bindingVisible = false"
          >Close</el-button
        ></template
      ></el-dialog
    >
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
  flex-wrap: wrap;
  justify-content: flex-end;
  gap: 10px;
}
.info {
  margin-bottom: 18px;
}
</style>
