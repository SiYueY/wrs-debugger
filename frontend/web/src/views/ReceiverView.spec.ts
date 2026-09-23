import { flushPromises, mount } from '@vue/test-utils';
import { createPinia, setActivePinia } from 'pinia';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { api } from '../api/wrs';
import RadioParametersEditor from '../components/RadioParametersEditor.vue';
import { defaultLoRaParameters } from '../domain/radio';
import { useDebuggerStore } from '../stores/debugger';
import ReceiverView from './ReceiverView.vue';

vi.mock('../api/wrs', () => ({
  api: {
    receiverInfo: vi.fn(),
    receiverLora: vi.fn(),
    receiverGfsk: vi.fn(),
    readReceiverSdo: vi.fn(),
    writeReceiverSdo: vi.fn(),
    writeReceiverLora: vi.fn(),
    writeReceiverGfsk: vi.fn(),
    restoreReceiverLora: vi.fn(),
    restoreReceiverGfsk: vi.fn(),
    syncReceiverLora: vi.fn(),
    syncReceiverGfsk: vi.fn(),
    factoryBind: vi.fn(),
    diagnosticError: vi.fn(),
  },
}));

const mockedApi = vi.mocked(api);

function button(wrapper: ReturnType<typeof mount>, text: string) {
  const result = wrapper.findAll('button').find((item) => item.text() === text);
  if (!result) throw new Error(`Button not found: ${text}`);
  return result;
}

async function mountConnected() {
  const pinia = createPinia();
  setActivePinia(pinia);
  const store = useDebuggerStore();
  store.receiver = { state: 'connected', domain_id: 0 };
  store.transmitter = { state: 'connected', device: '/dev/ttyACM0' };
  store.trackOperation = vi.fn().mockResolvedValue(undefined);
  mockedApi.receiverInfo.mockResolvedValue({ bound_device_id: 'A1B2C3' });
  mockedApi.receiverLora.mockResolvedValue(defaultLoRaParameters());
  mockedApi.receiverGfsk.mockResolvedValue({
    param_flags: 0x4000,
    tx_power: 10,
    freq_offset: 250,
    payload_len: 12,
    rssi_threshold: 110,
    heartbeat_interval: 200,
    heartbeat_loss: 3,
    bandwidth: 1,
    bitrate: 50000,
    freq_deviation: 25000,
    pulse_shaping: 9,
    preamble_len: 16,
    sync_word: 0x1424,
  });
  const wrapper = mount(ReceiverView, { global: { plugins: [pinia] } });
  await flushPromises();
  return { wrapper, store };
}

describe('ReceiverView', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    mockedApi.diagnosticError.mockResolvedValue({ code: null, detail: null });
  });

  it('retains the current parameters when reading fails', async () => {
    const { wrapper } = await mountConnected();
    const before = wrapper.findComponent(RadioParametersEditor).props('modelValue');
    mockedApi.receiverLora.mockRejectedValueOnce(new Error('read failed'));
    await button(wrapper, '读取参数').trigger('click');
    await flushPromises();
    expect(wrapper.findComponent(RadioParametersEditor).props('modelValue')).toEqual(before);
    expect(wrapper.text()).toContain('read failed');
  });

  it('reads back parameters after writing and restoring', async () => {
    const { wrapper } = await mountConnected();
    mockedApi.receiverLora.mockClear();
    await button(wrapper, '配置参数').trigger('click');
    await flushPromises();
    expect(mockedApi.writeReceiverLora).toHaveBeenCalledOnce();
    expect(mockedApi.receiverLora).toHaveBeenCalledOnce();
    mockedApi.receiverLora.mockClear();
    await button(wrapper, '恢复默认参数').trigger('click');
    await flushPromises();
    expect(mockedApi.restoreReceiverLora).toHaveBeenCalledOnce();
    expect(mockedApi.receiverLora).toHaveBeenCalledOnce();
  });

  it('reads and writes SDO responses through the receiver API', async () => {
    const { wrapper } = await mountConnected();
    const editor = wrapper.findComponent(RadioParametersEditor);
    await editor.vm.$emit('update:objectAddress', 0x202);
    await wrapper.vm.$nextTick();
    mockedApi.readReceiverSdo.mockResolvedValue({
      object_address: 0x202,
      object_data: 0x454e,
      status: 4,
      result_code: 0,
    });
    await button(wrapper, '读取参数').trigger('click');
    await flushPromises();
    expect(mockedApi.readReceiverSdo).toHaveBeenCalledWith(0x202);
    expect(wrapper.findComponent(RadioParametersEditor).props('responseStatus')).toBe(4);
    mockedApi.writeReceiverSdo.mockResolvedValue({
      object_address: 0x202,
      object_data: 0,
      status: 6,
      result_code: 0,
    });
    await button(wrapper, '配置参数').trigger('click');
    await flushPromises();
    expect(mockedApi.writeReceiverSdo).toHaveBeenCalledWith(0x202, 0x454e);
    expect(wrapper.findComponent(RadioParametersEditor).props('responseStatus')).toBe(6);
  });

  it('reads back parameters after synchronization', async () => {
    const { wrapper } = await mountConnected();
    mockedApi.syncReceiverLora.mockResolvedValue({ operation_id: 'sync-1' });
    mockedApi.receiverLora.mockClear();
    await button(wrapper, '同步急停盒参数').trigger('click');
    await flushPromises();
    expect(mockedApi.syncReceiverLora).toHaveBeenCalledOnce();
    expect(mockedApi.receiverLora).toHaveBeenCalledOnce();
  });
});
