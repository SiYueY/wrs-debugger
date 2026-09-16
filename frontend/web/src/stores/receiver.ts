import { defineStore } from 'pinia';
import * as api from '../api/receiver';
import { copyRadioConfig, defaultRadioConfig, type RadioConfig } from '../models/radio';
export const useReceiverStore = defineStore('receiver', {
  state: () => ({
    boundDeviceId: 'WRS-BOX-00142',
    config: copyRadioConfig(defaultRadioConfig),
    loading: false,
  }),
  actions: {
    async read() {
      this.loading = true;
      try {
        this.config = await api.getReceiverConfig();
      } finally {
        this.loading = false;
      }
    },
    async apply(config: RadioConfig) {
      this.loading = true;
      try {
        await api.setReceiverConfig(config);
        this.config = copyRadioConfig(config);
      } finally {
        this.loading = false;
      }
    },
    async restoreDefaults() {
      this.loading = true;
      try {
        this.config = await api.restoreReceiverDefaults();
      } finally {
        this.loading = false;
      }
    },
    async syncFromBox(config: RadioConfig) {
      this.loading = true;
      try {
        await api.syncReceiverConfig(config);
        this.config = copyRadioConfig(config);
      } finally {
        this.loading = false;
      }
    },
    async factoryBind() {
      this.loading = true;
      try {
        await api.startFactoryBinding();
      } finally {
        this.loading = false;
      }
    },
  },
});
