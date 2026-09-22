import { defineStore } from 'pinia';
import {
  api,
  type Connection,
  type ConnectionState,
  type DiagnosticError,
  type Operation,
  type SerialPort,
} from '../api/wrs';

const disconnected = (): Connection => ({ state: 'disconnected' });
export const useDebuggerStore = defineStore('debugger', {
  state: () => ({
    domainId: 0,
    ports: [] as SerialPort[],
    selectedPort: '',
    transmitter: disconnected(),
    receiver: disconnected(),
    loading: false,
    operation: null as Operation | null,
    diagnostic: { code: null, detail: null } as DiagnosticError,
    error: '',
  }),
  getters: {
    transmitterConnected: (state) => state.transmitter.state === 'connected',
    receiverConnected: (state) => state.receiver.state === 'connected',
    stateLabel: () => (state: ConnectionState) => state,
  },
  actions: {
    async bootstrap() {
      this.loading = true;
      try {
        const [settings, ports, transmitter, receiver, diagnostic] = await Promise.all([
          api.receiverSettings(),
          api.transmitterPorts(),
          api.transmitterConnection(),
          api.receiverConnection(),
          api.diagnosticError(),
        ]);
        this.domainId = settings.domain_id;
        this.ports = ports.ports;
        this.transmitter = transmitter;
        this.receiver = receiver;
        this.diagnostic = diagnostic;
        this.selectedPort = transmitter.device ?? ports.ports[0]?.device ?? '';
      } catch (error) {
        this.error = error instanceof Error ? error.message : 'Request failed';
      } finally {
        this.loading = false;
      }
    },
    async refreshDiagnostic() {
      this.diagnostic = await api.diagnosticError();
    },
    async clearDiagnostic() {
      this.loading = true;
      try {
        await api.clearDiagnosticError();
        await this.refreshDiagnostic();
      } finally {
        this.loading = false;
      }
    },
    async saveSettings() {
      this.loading = true;
      try {
        this.domainId = (await api.saveReceiverSettings(this.domainId)).domain_id;
      } finally {
        this.loading = false;
      }
    },
    async reloadSettings() {
      this.loading = true;
      try {
        this.domainId = (await api.receiverSettings()).domain_id;
      } finally {
        this.loading = false;
      }
    },
    async toggleReceiver() {
      this.loading = true;
      try {
        this.receiver = this.receiverConnected
          ? await api.disconnectReceiver()
          : await api.connectReceiver(this.domainId);
      } finally {
        this.loading = false;
      }
    },
    async selectPort(device: string) {
      this.selectedPort = device;
      this.loading = true;
      try {
        this.transmitter = await api.connectTransmitter(device);
      } finally {
        this.loading = false;
      }
    },
    async disconnectTransmitter() {
      this.loading = true;
      try {
        this.transmitter = await api.disconnectTransmitter();
      } finally {
        this.loading = false;
      }
    },
    async trackOperation(operationId: string) {
      this.operation = await api.operation(operationId);
      while (this.operation.state === 'pending' || this.operation.state === 'running') {
        await new Promise<void>((resolve) => window.setTimeout(resolve, 500));
        this.operation = await api.operation(operationId);
      }
      if (this.operation.state !== 'succeeded')
        throw new Error(this.operation.error?.detail ?? 'Operation failed');
    },
  },
});
