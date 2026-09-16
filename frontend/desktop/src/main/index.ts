import { app, BrowserWindow } from 'electron';

import path from 'node:path';

function createWindow(): void {
  const window = new BrowserWindow({
    width: 1280,
    height: 800,

    minWidth: 1024,
    minHeight: 640,

    webPreferences: {
      preload: path.join(__dirname, '../preload/index.cjs'),

      nodeIntegration: false,
      contextIsolation: true,
      sandbox: true,
    },
  });

  if (!app.isPackaged) {
    void window.loadURL('http://127.0.0.1:5173');

    return;
  }

  void window.loadFile(path.join(process.resourcesPath, 'web', 'index.html'));
}

app.whenReady().then(() => {
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
