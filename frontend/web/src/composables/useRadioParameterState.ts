import { computed, ref } from 'vue';
import type { GfskParameters, LoRaParameters } from '../api/wrs';
import {
  defaultGfskParameters,
  defaultLoRaParameters,
  type RadioKind,
  type RadioParameters,
} from '../domain/radio';

export function useRadioParameterState() {
  const mode = ref<RadioKind>('lora');
  const lora = ref<LoRaParameters>(defaultLoRaParameters());
  const gfsk = ref<GfskParameters>(defaultGfskParameters());
  const current = computed<RadioParameters>({
    get: () => (mode.value === 'lora' ? lora.value : gfsk.value),
    set: (value) => {
      if (mode.value === 'lora') lora.value = value as LoRaParameters;
      else gfsk.value = value as GfskParameters;
    },
  });

  function reset() {
    lora.value = defaultLoRaParameters();
    gfsk.value = defaultGfskParameters();
  }

  return { mode, lora, gfsk, current, reset };
}
