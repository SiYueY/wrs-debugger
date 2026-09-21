# WRS Debugger Backend V1

WRS Debugger 的本地业务后端。FastAPI Route 只表达产品业务，Service 通过
`WrsGateway` 访问设备；当前使用 `MockWrsGateway`，未来由 `PybindWrsGateway`
适配 C++ WRS Application。REST 不暴露串口帧、CRC、DDS、Kbind 或其他协议控制字段。

## Boundary

```text
frontend/web → /api/v1 → FastAPI route → service → WrsGateway → MockWrsGateway
```

当前 Backend 不访问真实 USB/DDS 硬件，也不承担无线急停运行时安全控制。

- Transmitter 与 Receiver 独立连接。
- Transmitter 的串口下拉框选择动作直接触发 `/transmitter/connect`。
- Receiver 由 Domain ID 和页面连接按钮触发 `/receiver/connect`。
- LoRa 与 GFSK 是两套独立资源，不存在 `RadioParameters`/`RadioConfig`。
- 参数 JSON 保留协议原始数值；Pydantic 只约束底层整数类型可表示范围。

## Prerequisites and startup

- Python 3.12
- [uv](https://docs.astral.sh/uv/)

```bash
cd backend
uv sync
uv run uvicorn wrs_debugger.main:app --reload --host 127.0.0.1 --port 8000
```

主要入口：

| Endpoint | Purpose |
| --- | --- |
| `GET /api/v1/health` | Backend liveness/readiness，不依赖设备连接。 |
| `GET /api/v1/system/info` | API/Event/Backend 版本和 Backend instance 信息。 |
| `GET /api/v1/snapshot` | 轻量连接状态与当前 Operation。 |
| `/docs` | OpenAPI/Swagger UI。 |
| `/redoc` | ReDoc。 |
| `WS /api/v1/events` | 连接、参数失效通知和 Operation 状态变更。 |

## HTTP contract

所有 REST endpoint 位于 `/api/v1`，使用 JSON 与 `snake_case`。

主要资源：

- `/transmitter`：串口、连接、信息、PIN、LoRa 参数、GFSK 参数。
- `/receiver`：Domain ID、连接、信息、LoRa 参数、GFSK 参数、同步和出厂绑定。
- `/operations`：查询异步同步和出厂绑定 Operation。
- `/snapshot`：只返回内存中的连接和 Operation 状态，不触发 USB/DDS I/O。

所有业务错误和请求校验错误都使用 `ProblemDetails`。OpenAPI 中 4xx/5xx 响应也显式引用
同一 schema，Frontend 不应依赖 FastAPI 默认的 `HTTPValidationError`。请求校验错误只返回
`field/code/context`，不会回显用户原始输入。

参数写入和恢复默认值成功后返回 `204 No Content`。参数 changed WebSocket event 是资源失效通知；
Frontend 如需最新值，应重新 GET 对应 LoRa/GFSK 资源。Backend 不为了构造事件执行额外 readback。

## Runtime model

所有同步 native 调用都必须经过唯一 `NativeExecutor` seam。Mock 调用内联执行；未来 pybind
调用使用 `ThreadedNativeExecutor`，避免阻塞 FastAPI event loop。生产部署应保持单进程、单
Uvicorn worker，因为设备连接与 Operation 是进程内唯一状态。

WebSocket Hub 为每个客户端维护有界队列。慢客户端队列溢出时会被主动关闭并移除，避免阻塞
其他客户端。

Receiver Domain ID 保存到 XDG config 下的 `settings.json`，使用临时文件 + `fsync` +
`os.replace` 原子更新。PIN、Kbind、串口设备节点和参数历史不持久化。

## Development workflow

```bash
uv sync
uv run ruff format .
uv run ruff check .
uv run mypy wrs_debugger
uv run pytest
uv run python -m compileall wrs_debugger
```

测试通过 `create_app()` 注入临时 `SettingsRepository`，不会写入开发机真实
`$XDG_CONFIG_HOME`。

## Layout

```text
wrs_debugger/
├── api/             # routers、依赖及统一 ProblemDetails
├── gateway/         # WrsGateway、Mock 与未来 pybind seam
├── models/          # 严格 Pydantic public contract
├── operations/      # 单活动跨设备 Operation 管理
├── services/        # Transmitter、Receiver、同步和绑定业务
└── websocket/       # 有界队列 WebSocket hub
```
