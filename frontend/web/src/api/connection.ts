export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'failed';
const pause = (milliseconds: number): Promise<void> =>
  new Promise((resolve) => window.setTimeout(resolve, milliseconds));
export const connectRobot = async (): Promise<ConnectionState> => {
  await pause(420);
  return 'connected';
};
export const disconnectRobot = async (): Promise<ConnectionState> => {
  await pause(250);
  return 'disconnected';
};
export const connectBox = async (): Promise<ConnectionState> => {
  await pause(350);
  return 'connected';
};
export const disconnectBox = async (): Promise<ConnectionState> => {
  await pause(220);
  return 'disconnected';
};
export const refreshSerialDevices = async (): Promise<string[]> => {
  await pause(300);
  return ['/dev/ttyACM0', '/dev/ttyUSB0'];
};
