import { defineStore } from 'pinia';
import {
  api,
  type Connection,
  type ConnectionState,
  type DiagnosticError,
  type Operation,
  type SerialPort,
} from '../api/wrs';
import { t } from '../i18n';
import { runtimeConfig } from '../runtime';

const disconnected = (): Connection => ({ state: 'disconnected' });

function selectedTransmitterPort(connection: Connection) {
  return connection.state === 'connected' ? (connection.device ?? '') : '';
}

function errorMessage(error: unknown) {
  return error instanceof Error ? error.message : t('operationFailed');
}
let notificationTimer: ReturnType<typeof setTimeout> | undefined;
export const useDebuggerStore = defineStore('debugger', {
  state: () => ({
    domainId: 0,
    ports: [] as SerialPort[],
    selectedPort: '',
    pendingTransmitterPort: '',
    transmitter: disconnected(),
    receiver: disconnected(),
    loading: false,
    operation: null as Operation | null,
    diagnostic: { code: null, detail: null } as DiagnosticError,
    error: '',
    notification: { message: '', error: false },
  }),
  getters: {
    transmitterConnected: (state) => state.transmitter.state === 'connected',
    receiverConnected: (state) => state.receiver.state === 'connected',
    stateLabel: () => (state: ConnectionState) => state,
  },
  actions: {
    showNotification(message: string, error = false) {
      if (notificationTimer) window.clearTimeout(notificationTimer);
      this.notification = { message, error };
      notificationTimer = window.setTimeout(() => {
        this.notification = { message: '', error: false };
      }, 3_000);
    },
    async bootstrap() {
      this.loading = true;
      try {
        const [settings, ports, transmitter, receiver] = await Promise.all([
          api.receiverSettings(),
          api.transmitterPorts(),
          api.transmitterConnection(),
          api.receiverConnection(),
        ]);
        this.domainId = settings.domain_id;
        this.ports = ports.ports;
        this.transmitter = transmitter;
        this.receiver = receiver;
        try {
          this.diagnostic = await api.diagnosticError();
        } catch {
          this.diagnostic = { code: null, detail: null };
        }
        this.selectedPort = selectedTransmitterPort(transmitter);
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
      this.pendingTransmitterPort = device;
      this.transmitter = { state: 'connecting', device };
      this.loading = true;
      try {
        const connection = await api.connectTransmitter(device);
        this.transmitter = connection;
        this.selectedPort = selectedTransmitterPort(connection);
        if (connection.state === 'connected' && connection.device)
          this.showNotification(`${t('transmitterConnectSucceeded')}：${connection.device}`);
      } catch (error) {
        this.transmitter = disconnected();
        this.selectedPort = '';
        this.showNotification(`${t('transmitterConnectFailed')}：${errorMessage(error)}`, true);
      } finally {
        this.pendingTransmitterPort = '';
        this.loading = false;
      }
    },
    async disconnectTransmitter() {
      this.loading = true;
      try {
        this.transmitter = await api.disconnectTransmitter();
        this.selectedPort = '';
      } finally {
        this.loading = false;
      }
    },
    async syncSnapshot() {
      const snapshot = await api.snapshot();
      this.transmitter = snapshot.transmitter.connection;
      const isPendingConnection =
        this.transmitter.state === 'connecting' &&
        this.transmitter.device === this.pendingTransmitterPort;
      if (!isPendingConnection) this.selectedPort = selectedTransmitterPort(this.transmitter);
      this.receiver = snapshot.receiver.connection;
      this.operation = snapshot.active_operation;
    },
    startEvents() {
      let retryMs = 500;
      let stopped = false;
      let socket: WebSocket | undefined;
      const connect = () => {
        if (stopped) return;
        socket = new WebSocket(runtimeConfig.apiBase.replace('http', 'ws') + '/api/v1/events');
        socket.onopen = () => {
          retryMs = 500;
          void this.syncSnapshot();
        };
        socket.onmessage = (message) => {
          const event = JSON.parse(message.data) as { event: string; data: Connection | Operation };
          if (event.event === 'transmitter.connection.changed') {
            this.transmitter = event.data as Connection;
            const isPendingConnection =
              this.transmitter.state === 'connecting' &&
              this.transmitter.device === this.pendingTransmitterPort;
            if (!isPendingConnection) this.selectedPort = selectedTransmitterPort(this.transmitter);
          }
          if (event.event === 'receiver.connection.changed')
            this.receiver = event.data as Connection;
          if (event.event === 'operation.updated') this.operation = event.data as Operation;
        };
        socket.onclose = () => {
          if (stopped) return;
          window.setTimeout(connect, retryMs);
          retryMs = Math.min(retryMs * 2, 10_000);
        };
      };
      connect();
      return () => {
        stopped = true;
        socket?.close();
      };
    },
    async trackOperation(operationId: string) {
      this.operation = await api.operation(operationId);
      const deadline = Date.now() + 60_000;
      while (this.operation.state === 'pending' || this.operation.state === 'running') {
        if (Date.now() >= deadline) throw new Error('Operation timed out');
        await new Promise<void>((resolve) => window.setTimeout(resolve, 500));
        this.operation = await api.operation(operationId);
      }
      if (this.operation.state !== 'succeeded')
        throw new Error(this.operation.error?.detail ?? 'Operation failed');
    },
  },
});
