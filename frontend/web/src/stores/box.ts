import { defineStore } from 'pinia';
import * as api from '../api/box';
import { copyRadioConfig, defaultRadioConfig, type RadioConfig } from '../models/radio';
export const useBoxStore = defineStore('box', {
  state: () => ({
    deviceId: 'WRS-BOX-00142',
    config: copyRadioConfig(defaultRadioConfig),
    loading: false,
  }),
  actions: {
    async read() {
      this.loading = true;
      try {
        this.config = await api.getBoxConfig();
      } finally {
        this.loading = false;
      }
    },
    async apply(config: RadioConfig) {
      this.loading = true;
      try {
        await api.setBoxConfig(config);
        this.config = copyRadioConfig(config);
      } finally {
        this.loading = false;
      }
    },
    async setPin(pin: string) {
      this.loading = true;
      try {
        await api.setBoxPin(pin);
      } finally {
        this.loading = false;
      }
    },
    async restoreDefaults() {
      this.loading = true;
      try {
        this.config = await api.restoreBoxDefaults();
      } finally {
        this.loading = false;
      }
    },
  },
});
