export type Modulation = 'lora' | 'gfsk';
export type FrequencyBand = 433 | 915;
export interface CommonRadioConfig {
  modulation: Modulation;
  frequencyBand: FrequencyBand;
  txPower: number;
  channelFrequencyOffset: number;
  payloadLength: 12;
  rssiThreshold: number;
  heartbeatInterval: number;
  heartbeatLossThreshold: number;
  channelScanMode: 'single' | 'scan';
  groupMode: 'one-to-one' | 'one-to-many';
  heartbeatEnabled: boolean;
  wirelessEstopEnabled: boolean;
  phyCrcEnabled: boolean;
}
export interface LoRaConfig {
  bandwidth: 125 | 250 | 500;
  spreadingFactor: 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12;
  codingRate: '4/5' | '4/6' | '4/7' | '4/8' | 'LI4/5' | 'LI4/6' | 'LI4/8';
  headerType: 'explicit' | 'implicit';
  preambleLength: number;
  syncWord: string;
}
export interface GfskConfig {
  receiveBandwidth: 117.3 | 234.3 | 467;
  bitRate: number;
  frequencyDeviation: number;
  pulseShaping: 'none' | 'bt-0.3' | 'bt-0.5' | 'bt-0.7' | 'bt-1.0';
  preambleLength: number;
  syncWord: string;
}
export interface RadioConfig extends CommonRadioConfig {
  lora: LoRaConfig;
  gfsk: GfskConfig;
}
export const defaultRadioConfig: RadioConfig = {
  modulation: 'lora',
  frequencyBand: 433,
  txPower: 10,
  channelFrequencyOffset: 0.25,
  payloadLength: 12,
  rssiThreshold: -110,
  heartbeatInterval: 200,
  heartbeatLossThreshold: 3,
  channelScanMode: 'single',
  groupMode: 'one-to-one',
  heartbeatEnabled: true,
  wirelessEstopEnabled: true,
  phyCrcEnabled: true,
  lora: {
    bandwidth: 250,
    spreadingFactor: 6,
    codingRate: 'LI4/5',
    headerType: 'explicit',
    preambleLength: 12,
    syncWord: '0x1424',
  },
  gfsk: {
    receiveBandwidth: 234.3,
    bitRate: 50000,
    frequencyDeviation: 25000,
    pulseShaping: 'bt-0.5',
    preambleLength: 16,
    syncWord: '0x1424',
  },
};

export function copyRadioConfig(config: RadioConfig): RadioConfig {
  return structuredClone(config);
}
export function validateRadioConfig(config: RadioConfig): string | undefined {
  const maxPower = config.frequencyBand === 433 ? 10 : 20;
  if (config.txPower < 0 || config.txPower > maxPower)
    return `TX Power must be between 0 and ${maxPower} dBm for this band.`;
  if (config.heartbeatInterval <= 0 || config.heartbeatLossThreshold < 1)
    return 'Heartbeat values must be positive.';
  if (
    !/^0x[0-9a-fA-F]{4}$/.test(config.lora.syncWord) ||
    !/^0x[0-9a-fA-F]{4}$/.test(config.gfsk.syncWord)
  )
    return 'Sync Word must use the 0xFFFF format.';
  if (config.modulation === 'lora') {
    if (config.lora.preambleLength < 10 || config.lora.preambleLength > 50)
      return 'LoRa Preamble Length must be between 10 and 50 symbols.';
    if (
      (config.lora.spreadingFactor === 5 || config.lora.spreadingFactor === 6) &&
      config.lora.preambleLength !== 12
    )
      return 'SF5 and SF6 require a Preamble Length of 12 symbols.';
    return undefined;
  }
  const { bitRate, frequencyDeviation, preambleLength } = config.gfsk;
  if (bitRate < 600 || bitRate > 150000) return 'GFSK Bit Rate must be between 600 and 150000 bps.';
  if (frequencyDeviation < 600 || frequencyDeviation > 300000)
    return 'GFSK Frequency Deviation must be between 600 and 300000 Hz.';
  if ((2 * frequencyDeviation) / bitRate < 0.5)
    return 'GFSK requires 2 × Frequency Deviation / Bit Rate to be at least 0.5.';
  if (preambleLength < 16 || preambleLength > 255)
    return 'GFSK Preamble Length must be between 16 and 255 bits.';
  return undefined;
}
