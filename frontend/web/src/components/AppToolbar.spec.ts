import { flushPromises, mount } from '@vue/test-utils';
import { createPinia, setActivePinia } from 'pinia';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { api } from '../api/wrs';
import { useDebuggerStore } from '../stores/debugger';
import AppToolbar from './AppToolbar.vue';

vi.mock('../api/wrs', () => ({
  api: {
    receiverSettings: vi.fn(),
    transmitterPorts: vi.fn(),
    transmitterConnection: vi.fn(),
    receiverConnection: vi.fn(),
    diagnosticError: vi.fn(),
    connectTransmitter: vi.fn(),
    disconnectTransmitter: vi.fn(),
  },
}));

const mockedApi = vi.mocked(api);

function mountToolbar() {
  const pinia = createPinia();
  setActivePinia(pinia);
  const store = useDebuggerStore();
  const wrapper = mount(AppToolbar, {
    global: { plugins: [pinia], stubs: { SettingsDialog: true } },
  });
  return { store, wrapper };
}

describe('AppToolbar', () => {
  afterEach(() => {
    vi.useRealTimers();
  });

  beforeEach(() => {
    vi.clearAllMocks();
    mockedApi.receiverSettings.mockResolvedValue({ domain_id: 0 });
    mockedApi.transmitterPorts.mockResolvedValue({
      ports: [{ device: '/dev/ttyUSB0' }, { device: '/dev/ttyUSB1' }],
    });
    mockedApi.transmitterConnection.mockResolvedValue({ state: 'disconnected' });
    mockedApi.receiverConnection.mockResolvedValue({ state: 'disconnected' });
    mockedApi.diagnosticError.mockResolvedValue({ code: null, detail: null });
    mockedApi.connectTransmitter.mockResolvedValue({ state: 'connected', device: '/dev/ttyUSB0' });
    mockedApi.disconnectTransmitter.mockResolvedValue({ state: 'disconnected' });
  });

  it('keeps USB unspecified after bootstrap and lists it first', async () => {
    const { store, wrapper } = mountToolbar();
    await store.bootstrap();
    await wrapper.vm.$nextTick();

    const options = wrapper.find('.transmitter-select').findAll('option');
    expect(store.selectedPort).toBe('');
    expect(options[0]?.text()).toBe('未指定');
    expect(options[0]?.attributes('value')).toBe('');
  });

  it('shows a selected port without green styling while connecting', async () => {
    let resolveConnection: (value: { state: 'connected'; device: string }) => void;
    mockedApi.connectTransmitter.mockImplementation(
      () =>
        new Promise((resolve) => {
          resolveConnection = resolve;
        }),
    );
    const { store, wrapper } = mountToolbar();
    store.ports = [{ device: '/dev/ttyUSB0' }];
    await wrapper.vm.$nextTick();

    const selection = wrapper.find('.transmitter-select');
    const pending = selection.setValue('/dev/ttyUSB0');
    await wrapper.vm.$nextTick();

    expect(mockedApi.connectTransmitter).toHaveBeenCalledWith('/dev/ttyUSB0');
    expect(store.selectedPort).toBe('/dev/ttyUSB0');
    expect(selection.classes()).not.toContain('transmitter-connected');

    resolveConnection!({ state: 'connected', device: '/dev/ttyUSB0' });
    await pending;
    await flushPromises();

    expect(selection.classes()).toContain('transmitter-connected');
    expect(store.notification).toEqual({
      message: '已连接急停盒串口：/dev/ttyUSB0',
      error: false,
    });
  });

  it('resets to unspecified and notifies when connecting fails', async () => {
    mockedApi.connectTransmitter.mockRejectedValue(new Error('device unavailable'));
    const { store, wrapper } = mountToolbar();
    store.ports = [{ device: '/dev/ttyUSB0' }];
    await wrapper.vm.$nextTick();

    await wrapper.find('.transmitter-select').setValue('/dev/ttyUSB0');
    await flushPromises();

    expect(store.selectedPort).toBe('');
    expect(store.notification).toEqual({
      message: '连接急停盒串口失败：device unavailable',
      error: true,
    });
  });

  it('resets to unspecified when a connection failure event is received', () => {
    class WebSocketMock {
      static instance: WebSocketMock | undefined;
      onclose: (() => void) | null = null;
      onmessage: ((event: MessageEvent<string>) => void) | null = null;

      constructor() {
        WebSocketMock.instance = this;
      }

      close() {}
    }

    vi.stubGlobal('WebSocket', WebSocketMock);
    const { store } = mountToolbar();
    store.selectedPort = '/dev/ttyUSB0';
    store.transmitter = { state: 'connecting', device: '/dev/ttyUSB0' };
    const stopEvents = store.startEvents();

    WebSocketMock.instance?.onmessage?.({
      data: JSON.stringify({
        event: 'transmitter.connection.changed',
        data: { state: 'failed', device: '/dev/ttyUSB0' },
      }),
    } as MessageEvent<string>);

    expect(store.transmitter.state).toBe('failed');
    expect(store.selectedPort).toBe('');
    stopEvents();
  });

  it('disconnects when USB is changed to unspecified', async () => {
    const { store, wrapper } = mountToolbar();
    store.ports = [{ device: '/dev/ttyUSB0' }];
    store.transmitter = { state: 'connected', device: '/dev/ttyUSB0' };
    store.selectedPort = '/dev/ttyUSB0';
    await wrapper.vm.$nextTick();

    await wrapper.find('.transmitter-select').setValue('');
    await flushPromises();

    expect(mockedApi.disconnectTransmitter).toHaveBeenCalledOnce();
    expect(store.selectedPort).toBe('');
    expect(store.transmitter.state).toBe('disconnected');
  });
});
