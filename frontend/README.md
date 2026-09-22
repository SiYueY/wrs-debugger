# WRS Debugger Frontend

WRS Debugger 的前端工作区。`web/` 是唯一业务 UI；`desktop/` 仅提供 Electron 桌面壳，因此浏览器与桌面版始终加载同一套 Vue 页面。

## Prerequisites

- Node.js 22（见 `.nvmrc`）
- pnpm 11.27.x

在本目录安装依赖：

```bash
pnpm install
```

## Development

| Command              | Purpose                                                      |
| -------------------- | ------------------------------------------------------------ |
| `pnpm dev:web`       | 启动 Vite Web UI，默认地址为 `http://127.0.0.1:5173`。       |
| `pnpm dev:desktop`   | 同时启动 Vite、Electron Main/Preload 的监听构建和 Electron。 |
| `pnpm typecheck`     | 对 Web 与 Desktop 执行严格 TypeScript 检查。                 |
| `pnpm format`        | 使用 Prettier 格式化前端源码与配置。                         |
| `pnpm format:check`  | 检查前端文件是否符合 Prettier 格式。                         |
| `pnpm build:web`     | 构建 Web 生产产物至 `web/dist`。                             |
| `pnpm build:desktop` | 构建 Web 与 Electron Main/Preload。                          |
| `pnpm clean`         | 清理各 workspace 的构建产物。                                |

开发桌面壳时，仍需在独立终端启动本地后端：

```bash
cd ../backend
uv run uvicorn wrs_debugger.main:app --host 127.0.0.1 --port 8000
```

构建自包含 Linux AppImage 请从项目根目录执行独立发布脚本：

```bash
./scripts/package-desktop-appimage.sh
```

该脚本会构建 Web、将 Python 3.12 后端冻结为 PyInstaller `onedir` 产物、构建 Electron，
然后生成 AppImage。最终可交付产物位于项目根目录的 `dist/`：

```text
dist/WRS-Debugger-<version>-<YYYYMMDD>.AppImage
```

`frontend/desktop/release/` 仅保留 Electron Builder 的同名构建副本，不作为发布路径。
目标机无需安装 Python、uv 或 WRS 后端依赖。

## Structure

```text
frontend/
├── web/                 # Vue 3 + Vite 的唯一业务 UI
│   └── src/
│       ├── api/         # `/api/v1` HTTP client 与 WRS 资源接口
│       ├── components/  # 布局与可复用表单
│       ├── layouts/     # 应用壳
│       ├── models/      # 强类型 UI 数据模型与校验
│       ├── stores/      # Pinia 状态
│       └── views/       # 连接、急停盒、接收板页面
└── desktop/             # Electron Main 与 Preload；不包含 renderer
```

## Runtime API configuration

- 前端只通过 `web/src/api/` 调用 `/api/v1`；页面不直接访问设备协议或 API `fetch`。
- 浏览器开发模式从 `web/.env.development` 读取 `VITE_WRS_API_BASE`，默认连接 `http://127.0.0.1:8000`。
- Electron 开发与打包模式均由安全 preload 提供 loopback API 地址；打包模式的后端端口随机分配。
- `desktop/` 不处理业务状态，也不向 renderer 暴露 Node 或设备操作；保持 `nodeIntegration: false`、`contextIsolation: true` 与 `sandbox: true`。

## UI notes

- 路由使用 Hash History，生产产物可以由 Electron 的本地文件加载。
- 页面路由使用 lazy import；Vue、Element Plus 与业务页面在生产构建中分块输出。
- LoRa 与 GFSK 维护独立字段模型和表单，避免错误复用不同调制方式的协议参数。
