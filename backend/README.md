# WRS Debugger Backend V1

WRS Debugger 的本地业务后端。FastAPI Route 只表达产品业务，Service 通过 `WrsGateway` 访问设备；当前使用完整的 `MockWrsGateway`，未来仅替换为 pybind11/C++ adapter。REST 不暴露帧、CRC、DDS、Kbind 或其他协议控制字段。

## Boundary

```text
frontend/web → /api/v1 → FastAPI route → service → WrsGateway → MockWrsGateway
```

当前不访问真实硬件、不使用 ROS 2/pyserial、不保存数据库状态，也不承担无线急停运行时安全控制。

## Prerequisites and startup

- Python 3.12（由 `.python-version` 与 `pyproject.toml` 固定）
- [uv](https://docs.astral.sh/uv/)

```bash
cd backend
uv sync
uv run uvicorn wrs_debugger.main:app --reload --host 127.0.0.1 --port 8000
```

服务默认只监听 loopback。可用入口：

| Endpoint | Purpose |
| --- | --- |
| `GET /api/v1/health` | Backend liveness，不依赖任何设备。 |
| `/docs` | OpenAPI/Swagger UI。 |
| `/redoc` | ReDoc。 |
| `WS /api/v1/events` | Transmitter/Receiver、参数及 Operation 状态变更。 |

## HTTP contract

所有 REST endpoint 位于 `/api/v1`，使用 JSON 与 snake_case。LoRa 与 GFSK 是独立资源，不存在通用 `RadioConfig` 或 modulation discriminator；具体 OpenAPI schema 是契约的权威来源。

主要资源：

- `/transmitter`：串口、连接、信息、PIN 和独立 LoRa/GFSK 参数。
- `/receiver`：Domain ID、连接、信息、独立 LoRa/GFSK 参数和跨设备操作。
- `/operations`：查询异步同步和出厂绑定 Operation。
- `/snapshot`：仅返回连接与活动 Operation 的轻量内存状态。

所有业务和请求校验错误均为 Problem Details：`type`、`title`、`status`、`detail`、`code`、`request_id`、`context`。Public JSON 字段使用协议原始值；`bitrate` 和 `freq_deviation` 为 uint32 范围。

`MockWrsGateway` 同时镜像开发所需的连接、Receiver 设置和活动 Operation 状态。Mock 调用内联执行；未来 pybind gateway 通过唯一的 `ThreadedNativeExecutor` seam 调度同步 USB/DDS 调用，避免阻塞 FastAPI event loop。

## Development workflow

```bash
uv sync
uv run ruff format .
uv run ruff check .
uv run mypy wrs_debugger
uv run pytest
uv run python -m compileall wrs_debugger
```

`uv.lock` 是受版本控制的可复现依赖锁定文件；修改 `pyproject.toml` 后必须运行 `uv lock` 或 `uv sync` 并提交其更新。

## Layout

```text
wrs_debugger/
├── api/             # routers、依赖及统一错误转换
├── gateway/         # WrsGateway protocol 与 MockWrsGateway
├── models/          # 严格 Pydantic public contract
├── operations/      # 单活动跨设备 Operation 管理
├── services/        # Transmitter、Receiver、同步和绑定业务
└── websocket/       # 有界队列 WebSocket hub
```
