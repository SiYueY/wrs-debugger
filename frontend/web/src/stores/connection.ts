import { defineStore } from 'pinia';
import * as api from '../api/connection';
import type { ConnectionState } from '../api/connection';
export const useConnectionStore = defineStore('connection', {
  state: () => ({
    robotIp: '192.168.1.100',
    rosDomainId: 0,
    serialDevice: '/dev/ttyACM0',
    robotState: 'disconnected' as ConnectionState,
    serialState: 'disconnected' as ConnectionState,
    serialDevices: ['/dev/ttyACM0', '/dev/ttyUSB0'],
    loading: false,
  }),
  actions: {
    async connectRobot() {
      this.loading = true;
      this.robotState = 'connecting';
      try {
        this.robotState = await api.connectRobot();
      } catch {
        this.robotState = 'failed';
      } finally {
        this.loading = false;
      }
    },
    async disconnectRobot() {
      this.loading = true;
      try {
        this.robotState = await api.disconnectRobot();
      } finally {
        this.loading = false;
      }
    },
    async connectBox() {
      this.loading = true;
      this.serialState = 'connecting';
      try {
        this.serialState = await api.connectBox();
      } catch {
        this.serialState = 'failed';
      } finally {
        this.loading = false;
      }
    },
    async disconnectBox() {
      this.loading = true;
      try {
        this.serialState = await api.disconnectBox();
      } finally {
        this.loading = false;
      }
    },
    async refreshDevices() {
      this.loading = true;
      try {
        this.serialDevices = await api.refreshSerialDevices();
      } finally {
        this.loading = false;
      }
    },
  },
});
