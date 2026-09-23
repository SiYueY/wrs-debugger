import { mount } from '@vue/test-utils';
import { describe, expect, it } from 'vitest';
import {
  defaultGfskParameters,
  defaultLoRaParameters,
  receiverRadioEditorConfig,
  transmitterRadioEditorConfig,
} from '../domain/radio';
import RadioParametersEditor from './RadioParametersEditor.vue';

function mountEditor(config = receiverRadioEditorConfig, objectAddress = 0, disabled = false) {
  return mount(RadioParametersEditor, {
    props: {
      modelValue: defaultLoRaParameters(),
      kind: 'lora',
      config,
      objectAddress,
      objectData: 0,
      disabled,
    },
  });
}

describe('RadioParametersEditor', () => {
  it('renders complete disabled parameters and SDO controls', () => {
    const wrapper = mountEditor(receiverRadioEditorConfig, 0, true);
    expect(wrapper.text()).toContain('SDO');
    expect(wrapper.text()).toContain('发射功率');
    expect(
      wrapper.findAll('input').every((input) => input.attributes('disabled') !== undefined),
    ).toBe(true);
  });

  it('uses receiver object permissions', async () => {
    const wrapper = mountEditor(receiverRadioEditorConfig, 0x201);
    const objectInput = wrapper.find('.hex-input input');
    expect(objectInput.attributes('disabled')).toBeDefined();
    await wrapper.setProps({ objectAddress: 0x202 });
    expect(wrapper.find('.hex-input input').attributes('disabled')).toBeUndefined();
    expect(wrapper.text()).toContain('通信 ID');
    expect(wrapper.text()).not.toContain('电池');
  });

  it('keeps transmitter-specific SDO objects', () => {
    const wrapper = mountEditor(transmitterRadioEditorConfig);
    expect(wrapper.text()).toContain('电池');
    expect(wrapper.text()).not.toContain('通信 ID');
  });

  it('switches modulation-specific fields', async () => {
    const wrapper = mountEditor();
    expect(wrapper.text()).toContain('扩频因子');
    await wrapper.setProps({ kind: 'gfsk', modelValue: defaultGfskParameters() });
    expect(wrapper.text()).toContain('脉冲整形');
    expect(wrapper.text()).not.toContain('扩频因子');
  });
});
