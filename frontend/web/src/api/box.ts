import { copyRadioConfig, defaultRadioConfig, type RadioConfig } from '../models/radio';
const pause = (milliseconds: number): Promise<void> =>
  new Promise((resolve) => window.setTimeout(resolve, milliseconds));
let config = copyRadioConfig(defaultRadioConfig);
export const getBoxConfig = async (): Promise<RadioConfig> => {
  await pause(400);
  return copyRadioConfig(config);
};
export const setBoxConfig = async (next: RadioConfig): Promise<void> => {
  await pause(350);
  config = copyRadioConfig(next);
};
export const setBoxPin = async (_pin: string): Promise<void> => {
  await pause(250);
};
export const restoreBoxDefaults = async (): Promise<RadioConfig> => {
  await pause(300);
  config = copyRadioConfig(defaultRadioConfig);
  return copyRadioConfig(config);
};
