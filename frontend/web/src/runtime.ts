interface DesktopRuntime {
  readonly platform: string;
  readonly apiBase: string;
}

declare global {
  interface Window {
    desktop?: DesktopRuntime;
  }
}

function resolveApiBase(): string {
  const configuredBase = window.desktop?.apiBase ?? import.meta.env.VITE_WRS_API_BASE;
  if (!configuredBase) {
    throw new Error('WRS API base URL is not configured.');
  }

  const apiBase = new URL(configuredBase);
  if (apiBase.protocol !== 'http:' || apiBase.hostname !== '127.0.0.1' || !apiBase.port) {
    throw new Error('WRS API base URL must be a loopback HTTP address with a port.');
  }
  return apiBase.origin;
}

export const runtimeConfig = Object.freeze({
  apiBase: resolveApiBase(),
});
