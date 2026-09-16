# WRS Debugger Backend

WRS Debugger 的本地业务后端。它为唯一 Vue UI 提供 REST 与 WebSocket 接口，用于连接配置、无线急停盒与机器人接收板参数、同步和出厂绑定流程。

这是一个可替换的 mock 实现：Route 只表达产品业务，Service 管理当前内存状态。后续接入 NativeFacade/C++ 时，应替换 Service 底部实现，而非暴露 ROS、USB Serial、CDU、RS485、帧、CRC 或 Kbind。

## Boundary

```text
frontend/web → /api/v1 → FastAPI route → service → mock state
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
| `WS /api/v1/events` | robot/box connection 与 operation 状态变更。 |

## HTTP contract

所有 REST endpoint 位于 `/api/v1`，使用 JSON 与 snake_case。配置模型由公共无线字段及 `phy` 判别联合构成：`phy.modulation` 为 `lora` 或 `gfsk`；频率使用 Hz、同步字为整数。具体 OpenAPI schema 是契约的权威来源。

主要资源：

- `/connection/robot` 与 `/connection/box`：连接读取、连接/断开，及 Robot apply/reload、串口设备列表。
- `/box`、`/box/config`、`/box/pin`：急停盒信息、配置、PIN、恢复默认。
- `/receiver`、`/receiver/config`：接收板信息、配置、恢复默认。
- `/receiver/sync-from-box`、`/receiver/factory-bind`：异步业务 Operation，返回 `202` 与 `operation_id`。
- `/operations/{operation_id}`：查询 Operation。

业务错误使用 `{ "code": "…", "message": "…" }`；Pydantic 请求校验保持 FastAPI 的 `422` 响应。

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
├── main.py          # 应用装配、lifespan 与 HTTP/WS entrypoints
├── models/          # Pydantic request/response contract
├── services/        # 业务约束、mock state 与 operation lifecycle
└── websocket/       # 最小连接管理器
tests/               # 不依赖 ROS、USB 或真实设备的 API tests
```
