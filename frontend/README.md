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
| `pnpm build:desktop` | 先构建 Web，再构建 Electron Main/Preload。                   |
| `pnpm clean`         | 清理各 workspace 的构建产物。                                |

构建 Linux 安装包时，先执行 `pnpm build:desktop`，再运行：

```bash
pnpm --filter @wrs-debugger/desktop package
```

Electron Builder 会将 `web/dist` 作为资源随应用打包。

## Structure

```text
frontend/
├── web/                 # Vue 3 + Vite 的唯一业务 UI
│   └── src/
│       ├── api/         # 业务语义的 mock adapter；未来替换为 REST/WebSocket
│       ├── components/  # 布局与可复用表单
│       ├── layouts/     # 应用壳
│       ├── models/      # 强类型 UI 数据模型与校验
│       ├── stores/      # Pinia 状态
│       └── views/       # 连接、急停盒、接收板页面
└── desktop/             # Electron Main 与 Preload；不包含 renderer
```

## Current scope

当前前端是可运行的 UI 骨架，包含连接管理、无线配置、LoRa/GFSK 动态表单、配置同步和出厂绑定的 mock 流程。

- 所有设备操作目前仅更新内存中的 mock state。
- 前端不访问 ROS 2、USB Serial 或浏览器 Web Serial API。
- `web/src/api/` 是未来替换为 `/api/v1/...` REST 与 WebSocket adapter 的唯一接入 seam；页面不应直接调用 `fetch` 或接触底层协议。
- `desktop/` 不处理业务状态，也不向 preload 暴露设备操作；保持 `nodeIntegration: false`、`contextIsolation: true` 与 `sandbox: true`。

## UI notes

- 路由使用 Hash History，生产产物可以由 Electron 的本地文件加载。
- 页面路由使用 lazy import；Vue、Element Plus 与业务页面在生产构建中分块输出。
- `RadioConfigForm` 维护本地配置草稿，并通过强类型 `v-model` 事件向页面回传副本，避免修改传入 props。
