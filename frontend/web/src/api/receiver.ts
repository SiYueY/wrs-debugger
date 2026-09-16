import { copyRadioConfig, defaultRadioConfig, type RadioConfig } from '../models/radio';
const pause = (milliseconds: number): Promise<void> =>
  new Promise((resolve) => window.setTimeout(resolve, milliseconds));
let config = copyRadioConfig(defaultRadioConfig);
export const getReceiverConfig = async (): Promise<RadioConfig> => {
  await pause(400);
  return copyRadioConfig(config);
};
export const setReceiverConfig = async (next: RadioConfig): Promise<void> => {
  await pause(350);
  config = copyRadioConfig(next);
};
export const restoreReceiverDefaults = async (): Promise<RadioConfig> => {
  await pause(300);
  config = copyRadioConfig(defaultRadioConfig);
  return copyRadioConfig(config);
};
export const syncReceiverConfig = async (next: RadioConfig): Promise<void> => {
  await pause(450);
  config = copyRadioConfig(next);
};
export const startFactoryBinding = async (): Promise<void> => {
  await pause(350);
};
