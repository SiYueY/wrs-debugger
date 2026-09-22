import { contextBridge } from 'electron';

function desktopApiBase(): string {
  const argument = process.argv.find((value) => value.startsWith('--wrs-api-base='));
  if (!argument) throw new Error('Missing desktop API base URL.');

  const apiBase = new URL(argument.slice('--wrs-api-base='.length));
  if (apiBase.protocol !== 'http:' || apiBase.hostname !== '127.0.0.1' || !apiBase.port) {
    throw new Error('Invalid desktop API base URL.');
  }
  return apiBase.origin;
}

contextBridge.exposeInMainWorld('desktop', {
  platform: process.platform,
  apiBase: desktopApiBase(),
});
