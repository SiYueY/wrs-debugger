<script setup lang="ts">
import { ElMessage } from 'element-plus';
import { storeToRefs } from 'pinia';
import ConnectionStatus from '../components/ConnectionStatus.vue';
import { useConnectionStore } from '../stores/connection';
const store = useConnectionStore();
const { robotIp, rosDomainId, serialDevice, robotState, serialState, serialDevices, loading } =
  storeToRefs(store);
function applyRobot(): void {
  ElMessage.success('Robot connection settings applied.');
}
function reloadRobot(): void {
  ElMessage.info('Robot connection settings reloaded.');
}
async function refreshDevices(): Promise<void> {
  await store.refreshDevices();
  ElMessage.success('Serial device list refreshed.');
}
</script>
<template>
  <div class="page">
    <div class="page-heading">
      <div>
        <h1>连接</h1>
        <p>管理机器人与无线急停盒的独立配置链路。</p>
      </div>
    </div>
    <el-card shadow="never"
      ><template #header
        ><div class="card-title">
          <span>Robot</span><ConnectionStatus :state="robotState" /></div></template
      ><el-form label-position="top" class="connection-form"
        ><el-form-item label="Robot IP"
          ><el-input v-model="robotIp" :disabled="robotState === 'connected'" /></el-form-item
        ><el-form-item label="ROS Domain ID"
          ><el-input-number
            v-model="rosDomainId"
            :min="0"
            :disabled="robotState === 'connected'" /></el-form-item
        ><el-form-item label="Connection State"
          ><ConnectionStatus :state="robotState" /></el-form-item
      ></el-form>
      <div class="actions">
        <el-button :loading="loading" @click="applyRobot">Apply</el-button
        ><el-button :loading="loading" @click="reloadRobot">Reload</el-button
        ><el-button
          v-if="robotState !== 'connected'"
          type="primary"
          :loading="loading"
          @click="store.connectRobot"
          >Connect</el-button
        ><el-button v-else type="danger" plain :loading="loading" @click="store.disconnectRobot"
          >Disconnect</el-button
        >
      </div></el-card
    ><el-card shadow="never"
      ><template #header
        ><div class="card-title">
          <span>Wireless E-Stop Box</span><ConnectionStatus :state="serialState" /></div></template
      ><el-form label-position="top" class="connection-form"
        ><el-form-item label="Serial Device"
          ><el-select v-model="serialDevice"
            ><el-option
              v-for="device in serialDevices"
              :key="device"
              :label="device"
              :value="device" /></el-select></el-form-item
        ><el-form-item label="Serial Connection State"
          ><ConnectionStatus :state="serialState" /></el-form-item
      ></el-form>
      <div class="actions">
        <el-button :loading="loading" @click="refreshDevices">Refresh</el-button
        ><el-button
          v-if="serialState !== 'connected'"
          type="primary"
          :loading="loading"
          @click="store.connectBox"
          >Connect</el-button
        ><el-button v-else type="danger" plain :loading="loading" @click="store.disconnectBox"
          >Disconnect</el-button
        >
      </div></el-card
    >
  </div>
</template>
<style scoped>
.page {
  max-width: 1180px;
}
.page-heading {
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
.el-card {
  margin-bottom: 18px;
}
.card-title {
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-weight: 600;
}
.connection-form {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 20px;
}
.connection-form .el-form-item {
  margin: 0;
}
.el-input-number,
.el-select {
  width: 100%;
}
.actions {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
  margin-top: 22px;
}
</style>
