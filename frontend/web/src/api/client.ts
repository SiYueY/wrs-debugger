export class HttpError extends Error {
  readonly status: number;
  readonly code?: string;

  constructor(message: string, status: number, code?: string) {
    super(message);
    this.status = status;
    this.code = code;
  }
}

async function parseError(response: Response): Promise<HttpError> {
  try {
    const body = (await response.json()) as { detail?: string; title?: string; code?: string };
    return new HttpError(
      body.detail ?? body.title ?? response.statusText,
      response.status,
      body.code,
    );
  } catch {
    return new HttpError(response.statusText, response.status);
  }
}

export async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(`${runtimeConfig.apiBase}/api/v1${path}`, {
    ...init,
    headers: { Accept: 'application/json', 'Content-Type': 'application/json', ...init?.headers },
  });
  if (!response.ok) throw await parseError(response);
  if (response.status === 204) return undefined as T;
  return (await response.json()) as T;
}
import { runtimeConfig } from '../runtime';
