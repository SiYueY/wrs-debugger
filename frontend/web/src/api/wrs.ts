import { request } from './client';

export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'failed';
export interface Connection {
  state: ConnectionState;
  device?: string | null;
  domain_id?: number | null;
}
export interface SerialPort {
  device: string;
  description?: string | null;
}
export interface LoRaParameters {
  param_flags: number;
  tx_power: number;
  freq_offset: number;
  payload_len: number;
  rssi_threshold: number;
  heartbeat_interval: number;
  heartbeat_loss: number;
  bandwidth: number;
  spreading_factor: number;
  coding_rate: number;
  header_type: number;
  preamble_len: number;
  sync_word: number;
}
export interface GfskParameters {
  param_flags: number;
  tx_power: number;
  freq_offset: number;
  payload_len: number;
  rssi_threshold: number;
  heartbeat_interval: number;
  heartbeat_loss: number;
  bandwidth: number;
  bitrate: number;
  freq_deviation: number;
  pulse_shaping: number;
  preamble_len: number;
  sync_word: number;
}
export interface SdoResponse {
  object_address: number;
  object_data: number;
  status: number;
  result_code: number;
}
export interface Operation {
  operation_id: string;
  state: 'pending' | 'running' | 'succeeded' | 'failed' | 'interrupted' | 'unknown';
  stage: string;
  progress: number;
  error?: { detail: string } | null;
}
export interface DiagnosticError {
  code: string | null;
  detail: string | null;
}
export interface Snapshot {
  transmitter: { connection: Connection };
  receiver: { connection: Connection };
  active_operation: Operation | null;
}

export const api = {
  diagnosticError: () => request<DiagnosticError>('/diagnostics/error'),
  clearDiagnosticError: () => request<void>('/diagnostics/error/clear', { method: 'POST' }),
  transmitterPorts: () => request<{ ports: SerialPort[] }>('/transmitter/serial-ports'),
  transmitterConnection: () => request<Connection>('/transmitter/connection'),
  connectTransmitter: (device: string) =>
    request<Connection>('/transmitter/connect', {
      method: 'POST',
      body: JSON.stringify({ device }),
    }),
  disconnectTransmitter: () => request<Connection>('/transmitter/disconnect', { method: 'POST' }),
  transmitterInfo: () => request<{ device_id: string | null }>('/transmitter'),
  readPin: () => request<{ pin: string }>('/transmitter/pin'),
  writePin: (pin: string) =>
    request<void>('/transmitter/pin', { method: 'PUT', body: JSON.stringify({ pin }) }),
  readTransmitterSdo: (object_address: number) =>
    request<SdoResponse>('/transmitter/sdo/read', {
      method: 'POST', body: JSON.stringify({ object_address }),
    }),
  writeTransmitterSdo: (object_address: number, object_data: number) =>
    request<SdoResponse>('/transmitter/sdo', {
      method: 'PUT', body: JSON.stringify({ object_address, object_data }),
    }),
  transmitterLora: () => request<LoRaParameters>('/transmitter/lora-parameters'),
  writeTransmitterLora: (body: LoRaParameters) =>
    request<void>('/transmitter/lora-parameters', { method: 'PUT', body: JSON.stringify(body) }),
  restoreTransmitterLora: () =>
    request<void>('/transmitter/lora-parameters/restore-defaults', { method: 'POST' }),
  transmitterGfsk: () => request<GfskParameters>('/transmitter/gfsk-parameters'),
  writeTransmitterGfsk: (body: GfskParameters) =>
    request<void>('/transmitter/gfsk-parameters', { method: 'PUT', body: JSON.stringify(body) }),
  restoreTransmitterGfsk: () =>
    request<void>('/transmitter/gfsk-parameters/restore-defaults', { method: 'POST' }),
  receiverSettings: () => request<{ domain_id: number }>('/receiver/settings'),
  saveReceiverSettings: (domain_id: number) =>
    request<{ domain_id: number }>('/receiver/settings', {
      method: 'PUT',
      body: JSON.stringify({ domain_id }),
    }),
  receiverConnection: () => request<Connection>('/receiver/connection'),
  connectReceiver: (domain_id: number) =>
    request<Connection>('/receiver/connect', {
      method: 'POST',
      body: JSON.stringify({ domain_id }),
    }),
  disconnectReceiver: () => request<Connection>('/receiver/disconnect', { method: 'POST' }),
  receiverInfo: () => request<{ bound_device_id: string | null }>('/receiver'),
  receiverLora: () => request<LoRaParameters>('/receiver/lora-parameters'),
  writeReceiverLora: (body: LoRaParameters) =>
    request<void>('/receiver/lora-parameters', { method: 'PUT', body: JSON.stringify(body) }),
  restoreReceiverLora: () =>
    request<void>('/receiver/lora-parameters/restore-defaults', { method: 'POST' }),
  syncReceiverLora: () =>
    request<{ operation_id: string }>('/receiver/lora-parameters/sync-from-transmitter', {
      method: 'POST',
    }),
  receiverGfsk: () => request<GfskParameters>('/receiver/gfsk-parameters'),
  writeReceiverGfsk: (body: GfskParameters) =>
    request<void>('/receiver/gfsk-parameters', { method: 'PUT', body: JSON.stringify(body) }),
  restoreReceiverGfsk: () =>
    request<void>('/receiver/gfsk-parameters/restore-defaults', { method: 'POST' }),
  syncReceiverGfsk: () =>
    request<{ operation_id: string }>('/receiver/gfsk-parameters/sync-from-transmitter', {
      method: 'POST',
    }),
  factoryBind: () =>
    request<{ operation_id: string }>('/receiver/factory-bind', { method: 'POST' }),
  operation: (id: string) => request<Operation>(`/operations/${id}`),
  snapshot: () => request<Snapshot>('/snapshot'),
};
