import { app, BrowserWindow, dialog, Menu, screen } from 'electron';
import { spawn, type ChildProcess } from 'node:child_process';
import http from 'node:http';
import net from 'node:net';
import path from 'node:path';

let backend: ChildProcess | undefined;
let backendPort: number | undefined;

const MIN_WINDOW_WIDTH = 1320;
const MIN_WINDOW_HEIGHT = 720;

function backendExecutable(): string {
  return path.join(process.resourcesPath, 'backend', 'wrs-debugger-backend');
}

function apiBase(): string {
  const port = app.isPackaged ? backendPort : 8000;
  if (!port) throw new Error('The desktop API port is not available.');
  return `http://127.0.0.1:${port}`;
}

function findFreePort(): Promise<number> {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      server.close((error) => {
        if (error) return reject(error);
        if (!address || typeof address === 'string')
          return reject(new Error('No free TCP port found.'));
        resolve(address.port);
      });
    });
  });
}

function waitForHealth(port: number, timeoutMs = 15_000): Promise<void> {
  const startedAt = Date.now();
  return new Promise((resolve, reject) => {
    const retry = () => {
      const request = http.get(`http://127.0.0.1:${port}/api/v1/health`, (response) => {
        response.resume();
        if (response.statusCode === 200) return resolve();
        retryLater();
      });
      request.once('error', retryLater);
      request.setTimeout(500, () => request.destroy());
    };
    const retryLater = () => {
      if (Date.now() - startedAt >= timeoutMs) {
        reject(
          new Error(`The bundled backend did not become ready within ${timeoutMs / 1000} seconds.`),
        );
        return;
      }
      setTimeout(retry, 200);
    };
    retry();
  });
}

async function startBackend(): Promise<void> {
  if (!app.isPackaged) return;
  backendPort = await findFreePort();
  backend = spawn(backendExecutable(), ['--host', '127.0.0.1', '--port', String(backendPort)], {
    stdio: 'ignore',
  });
  await waitForHealth(backendPort);
}

function stopBackend(): void {
  if (backend && !backend.killed) backend.kill();
  backend = undefined;
  backendPort = undefined;
}

function windowBounds() {
  const display = screen.getDisplayNearestPoint(screen.getCursorScreenPoint());
  const { x, y, width, height } = display.workArea;
  const windowWidth = Math.max(Math.floor((2 * width) / 3), MIN_WINDOW_WIDTH);
  const windowHeight = Math.max(Math.floor((2 * height) / 3), MIN_WINDOW_HEIGHT);
  return {
    width: windowWidth,
    height: windowHeight,
    x: x + Math.floor((width - windowWidth) / 2),
    y: y + Math.floor((height - windowHeight) / 2),
  };
}

function createWindow(): void {
  const window = new BrowserWindow({
    ...windowBounds(),
    title: 'WRS Debugger',
    minWidth: MIN_WINDOW_WIDTH,
    minHeight: MIN_WINDOW_HEIGHT,
    resizable: true,
    show: false,
    autoHideMenuBar: true,
    webPreferences: {
      preload: path.join(__dirname, '../preload/index.cjs'),
      additionalArguments: [`--wrs-api-base=${apiBase()}`],
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: true,
    },
  });
  window.once('ready-to-show', () => {
    window.setBounds(windowBounds());
    window.show();
    window.center();
  });
  if (!app.isPackaged) {
    void window.loadURL('http://127.0.0.1:5173');
    return;
  }
  void window.loadFile(path.join(process.resourcesPath, 'web', 'index.html'));
}

const gotLock = app.requestSingleInstanceLock();
if (!gotLock) {
  app.quit();
} else {
  app.on('second-instance', () => {
    const window = BrowserWindow.getAllWindows()[0];
    if (!window) return;
    if (window.isMinimized()) window.restore();
    window.focus();
  });
  app.whenReady().then(async () => {
    Menu.setApplicationMenu(null);
    try {
      await startBackend();
      createWindow();
    } catch (error) {
      stopBackend();
      dialog.showErrorBox(
        'WRS Debugger startup failed',
        error instanceof Error ? error.message : String(error),
      );
      app.exit(1);
      return;
    }
    app.on('activate', () => {
      if (BrowserWindow.getAllWindows().length === 0) createWindow();
    });
  });
}

app.on('window-all-closed', () => {
  stopBackend();
  if (process.platform !== 'darwin') app.quit();
});
