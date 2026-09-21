你现在需要在以下项目中完整实现 Backend V1：

```text
Repository:
https://github.com/SiYueY/wrs-debugger

Branch:
develop
```

目标目录：

```text
backend/
```

在开始修改前，必须先完整阅读和分析：

```text
docs/
backend/
wrs/include/
wrs/src/
```

尤其必须重点分析：

```text
wrs/include/transmitter/protocol.hpp
wrs/include/transmitter/client.hpp
wrs/include/transmitter/device.hpp
wrs/include/transmitter/error.hpp

wrs/src/transmitter/protocol.cpp
wrs/src/transmitter/client.cpp
wrs/src/transmitter/discovery.cpp

wrs/include/serial/
wrs/src/serial/

wrs/include/receiver/
wrs/include/ddswrapper/
```

不要根据旧 Backend Mock 的结构推导最终设计。

当前 `backend/` 中的：

```text
Robot
Box
RadioConfig
connection/robot
connection/box
box/config
receiver/config
sync-from-box
```

等模型和 API 都属于旧 Mock 实现，应按照下面的正式设计重构，不需要保持兼容。

---

# 1. 实现目标

Backend 技术栈：

```text
Python 3.12
FastAPI
Pydantic v2
Uvicorn
pytest
pytest-asyncio
httpx
mypy strict
ruff
```

整体架构：

```text
Frontend
Vue 3 + TypeScript
        │
        │ REST / WebSocket
        ▼
Backend
FastAPI + Pydantic
        │
        │ WrsGateway
        ▼
MockWrsGateway
        │
        └── 当前开发与测试使用

未来：
        │
        ▼
PybindWrsGateway
        │
        ▼
C++ WRS Application
        ├── USB Serial → Transmitter
        └── DDS        → Receiver
```

当前任务重点是：

1. 完整实现 Backend；
2. 完整实现 `MockWrsGateway`；
3. 定义稳定的 Frontend API Contract；
4. 为未来 `PybindWrsGateway` 留出清晰接口；
5. 不要求当前完成真实 pybind11 集成；
6. 不依赖真实 USB、Transmitter、Receiver 或 DDS 硬件。

Backend API 一旦完成，应能够直接支持后续 Frontend 开发。

---

# 2. 核心设计原则

必须严格遵守以下规则。

## 2.1 Transmitter 和 Receiver 独立连接

不存在统一：

```text
robot_connected
wrs_connected
global_connected
```

状态。

Backend 分别维护：

```text
TransmitterConnection
ReceiverConnection
```

以下状态组合全部合法：

```text
Transmitter disconnected / Receiver disconnected
Transmitter connected    / Receiver disconnected
Transmitter disconnected / Receiver connected
Transmitter connected    / Receiver connected
```

单侧操作只能依赖对应设备。

只有：

```text
LoRa 参数同步
GFSK 参数同步
Factory Binding
```

需要同时连接 Transmitter 和 Receiver。

---

## 2.2 Transmitter 通过 USB Serial 连接

Frontend 页面有 USB 串口下拉框。

用户行为：

```text
刷新串口
    ↓
选择某个串口
    ↓
选择动作本身立即表示连接请求
```

**Transmitter 没有额外的“连接”按钮。**

Frontend 选择：

```text
/dev/ttyUSB0
```

后立即调用：

```http
POST /api/v1/transmitter/connect
```

```json
{
  "device": "/dev/ttyUSB0"
}
```

如果当前已经连接 `/dev/ttyUSB0`，用户改选 `/dev/ttyUSB1`：

```text
关闭旧连接
    ↓
连接 /dev/ttyUSB1
```

如果新连接失败：

```text
state = failed
device = /dev/ttyUSB1
```

不要自动恢复旧串口。

如果用户选择的就是当前已连接串口，可以直接返回当前状态，不重复打开。

---

## 2.3 USB 串口枚举

接口：

```http
GET /api/v1/transmitter/serial-ports
```

必须列出当前可选串口。

注意当前 C++：

```cpp
serial::list_ports()
```

用于枚举串口。

而：

```cpp
transmitter::discover()
```

会主动打开候选串口、执行 SDO 探测，只返回已经验证通过的 Transmitter。

因此：

```text
/transmitter/serial-ports
```

语义应该对应：

```text
serial::list_ports()
```

而不是 `transmitter::discover()`。

Mock 中也应模拟“普通串口列表”。

---

## 2.4 Receiver 通过 DDS 连接

Receiver 页面只有：

```text
Domain ID
[连接]
```

不使用 IP Address。

用户输入 Domain ID 后点击“连接”：

```http
POST /api/v1/receiver/connect
```

```json
{
  "domain_id": 0
}
```

Receiver 是整个工具中需要显式“连接”按钮的设备。

---

## 2.5 LoRa 和 GFSK 必须完全拆开

禁止定义：

```text
RadioParameters
RadioConfig
GenericParameters
radio_type discriminator
```

Transmitter：

```text
LoRaParameters
GfskParameters
```

Receiver：

```text
LoRaParameters
GfskParameters
```

Frontend 将来也是四个独立参数页面：

```text
Transmitter LoRa
Transmitter GFSK

Receiver LoRa
Receiver GFSK
```

REST API、Pydantic Model、Service 方法、Mock State、Operation Type 都必须按 LoRa/GFSK 分开。

---

# 3. 参数模型必须使用协议原始值

这是非常重要的要求。

Frontend 页面直接显示协议字段的原始数值。

禁止 Backend 做二次数值转换。

例如：

```text
rssi_threshold = 110
```

API 就返回：

```json
{
  "rssi_threshold": 110
}
```

不要转换成：

```text
-110 dBm
```

例如：

```text
bandwidth = 1
coding_rate = 4
header_type = 0
pulse_shaping = 9
param_flags = 0
```

API 全部直接返回原始数字。

不要转换成：

```text
250 kHz
LI4/5
explicit
BT 0.5
```

Frontend 将来页面展示的就是原始协议值。

---

# 4. 参数范围只根据字段数值类型

Backend/Pydantic 只限制：

```text
值能否由对应 C++ 类型表示
```

不要在 Backend 重复实现 C++ 中已经存在的协议业务逻辑。

统一定义：

```python
Uint8 = Annotated[
    int,
    Field(ge=0, le=255),
]

Uint16 = Annotated[
    int,
    Field(ge=0, le=65535),
]

Uint32 = Annotated[
    int,
    Field(ge=0, le=4294967295),
]

Int16 = Annotated[
    int,
    Field(ge=-32768, le=32767),
]
```

不要在 Pydantic 中额外实现：

```text
433 MHz → tx_power <= 10

SF5 / SF6 → preamble_len == 12

sync_word → 0xY4X4

2 * freq_deviation / bitrate >= 0.5
```

等协议业务约束。

这些约束由 C++ `transmitter::Client` 最终验证。

当前 `develop` 中 `Client` 已经增加：

```text
valid_lora_parameters
valid_gfsk_parameters
```

等 Native 校验逻辑。

Backend 不重复维护第二套协议规则。

---

# 5. LoRaParameters

根据当前：

```cpp
transmitter::LoRaParamFrame
```

Public Pydantic Model 应定义为：

```python
class LoRaParameters(ApiModel):
    param_flags: Uint16

    tx_power: Int16
    freq_offset: Uint16
    payload_len: Uint8
    rssi_threshold: Uint8

    heartbeat_interval: Uint16
    heartbeat_loss: Uint8

    bandwidth: Uint8
    spreading_factor: Uint8
    coding_rate: Uint8
    header_type: Uint8
    preamble_len: Uint8
    sync_word: Uint16
```

不要暴露：

```text
cmd
transaction_id
object_index
object_data
reserved
result_code
crc16
```

这些属于 C++ 协议控制字段。

---

# 6. GfskParameters

根据当前已经修复后的：

```cpp
transmitter::GfskParamFrame
```

定义：

```python
class GfskParameters(ApiModel):
    param_flags: Uint16

    tx_power: Int16
    freq_offset: Uint16
    payload_len: Uint8
    rssi_threshold: Uint8

    heartbeat_interval: Uint16
    heartbeat_loss: Uint8

    bandwidth: Uint8
    bitrate: Uint32
    freq_deviation: Uint32
    pulse_shaping: Uint8
    preamble_len: Uint8
    sync_word: Uint16
```

当前 `develop` 已经将：

```cpp
bitrate
freq_deviation
```

修复为：

```cpp
std::uint32_t
```

并按照：

```text
Byte23-26 bitrate
Byte27-30 freq_deviation
```

序列化。

不要重新按照旧实现设计。

---

# 7. Public Model 基线

所有 Public Model：

```python
class ApiModel(BaseModel):
    model_config = ConfigDict(
        extra="forbid",
        str_strip_whitespace=True,
    )
```

必须使用：

```text
extra="forbid"
```

避免错误字段静默通过。

至少需要实现：

```text
HealthResponse
SystemInfoResponse
SnapshotResponse

SerialPortInfo
SerialPortListResponse

ConnectionState
ConnectionError
TransmitterConnection
ReceiverConnection

ConnectTransmitterRequest
ConnectReceiverRequest
ReceiverSettings

TransmitterInfo
ReceiverInfo

PinResponse
WritePinRequest

LoRaParameters
GfskParameters

Operation
OperationCreatedResponse
ActiveOperationResponse

ProblemDetails
WebSocket Event Models
```

---

# 8. PIN

PIN：

```python
pin: str = Field(
    pattern=r"^[0-9]{6}$"
)
```

不要使用：

```text
\d
```

防止 Unicode 数字通过。

当前协议中：

```text
"000000"
```

用于读取 PIN，因此写入时不能允许 `"000000"`。

PIN 不允许：

```text
写日志
写 Audit 内容
持久化到 SQLite
发送 WebSocket
```

---

# 9. REST API

统一前缀：

```text
/api/v1
```

实现以下完整 API。

## System

```text
GET /api/v1/health

GET /api/v1/system/info

GET /api/v1/snapshot
```

---

## Transmitter

### Serial / Connection

```text
GET  /api/v1/transmitter/serial-ports

GET  /api/v1/transmitter/connection

POST /api/v1/transmitter/connect

POST /api/v1/transmitter/disconnect
```

Connect Request：

```json
{
  "device": "/dev/ttyUSB0"
}
```

---

### Information

```text
GET /api/v1/transmitter
```

当前至少返回：

```text
device_id
product_code
version_number
serial_number
```

如果 Device ID 暂时无法可靠获取：

```text
device_id = null
```

禁止用 SDO Serial Number 替代 Device ID。

---

### PIN

```text
GET /api/v1/transmitter/pin

PUT /api/v1/transmitter/pin
```

PUT 成功：

```text
204 No Content
```

---

### LoRa

```text
GET  /api/v1/transmitter/lora-parameters

PUT  /api/v1/transmitter/lora-parameters

POST /api/v1/transmitter/lora-parameters/restore-defaults
```

GET：

```text
200 LoRaParameters
```

PUT：

```text
Request = LoRaParameters
Success = 204 No Content
```

Restore：

```text
Success = 204 No Content
```

---

### GFSK

```text
GET  /api/v1/transmitter/gfsk-parameters

PUT  /api/v1/transmitter/gfsk-parameters

POST /api/v1/transmitter/gfsk-parameters/restore-defaults
```

PUT / Restore 成功：

```text
204 No Content
```

---

## Receiver

### Settings

```text
GET /api/v1/receiver/settings

PUT /api/v1/receiver/settings
```

模型：

```json
{
  "domain_id": 0
}
```

---

### Connection

```text
GET  /api/v1/receiver/connection

POST /api/v1/receiver/connect

POST /api/v1/receiver/disconnect
```

Connect：

```json
{
  "domain_id": 0
}
```

---

### Information

```text
GET /api/v1/receiver
```

至少返回：

```json
{
  "bound_device_id": null
}
```

---

### Receiver LoRa

```text
GET  /api/v1/receiver/lora-parameters

PUT  /api/v1/receiver/lora-parameters

POST /api/v1/receiver/lora-parameters/restore-defaults

POST /api/v1/receiver/lora-parameters/sync-from-transmitter
```

---

### Receiver GFSK

```text
GET  /api/v1/receiver/gfsk-parameters

PUT  /api/v1/receiver/gfsk-parameters

POST /api/v1/receiver/gfsk-parameters/restore-defaults

POST /api/v1/receiver/gfsk-parameters/sync-from-transmitter
```

---

### Factory Binding

```text
POST /api/v1/receiver/factory-bind
```

不要建立：

```text
/api/v1/binding
```

顶层资源。

---

## Operations

```text
GET /api/v1/operations/active

GET /api/v1/operations/{operation_id}
```

注意：

```text
/active
```

静态路由应避免被：

```text
/{operation_id}
```

误匹配。

---

# 10. 参数写入行为

不要在 Backend 自动：

```text
write
 ↓
readback
 ↓
compare
```

当前 C++：

```cpp
write_lora_parameters(...)
write_gfsk_parameters(...)
```

成功返回：

```text
Result<void, Error>
```

Backend 保持同样简单语义：

```text
PUT
 ↓
Gateway write
 ↓
成功
 ↓
204 No Content
```

Frontend 如果需要刷新参数，写入成功后重新 GET。

Restore Default 也一样：

```text
POST restore-defaults
 ↓
Gateway
 ↓
204
 ↓
Frontend GET refresh
```

不要人为增加额外串口/DDS操作。

---

# 11. Receiver 与 Transmitter LoRa/GFSK 对称

虽然当前 C++ Receiver 模块还没有完成 LoRa/GFSK Client，但 Backend Public Contract 现在必须直接冻结为：

```text
Transmitter
├── LoRaParameters
└── GfskParameters

Receiver
├── LoRaParameters
└── GfskParameters
```

Receiver Mock 必须完整实现：

```text
read_receiver_lora_parameters
write_receiver_lora_parameters
restore_receiver_lora_defaults

read_receiver_gfsk_parameters
write_receiver_gfsk_parameters
restore_receiver_gfsk_defaults
```

后续 C++ Receiver 模块完成后只替换 Gateway 实现。

Frontend API 不改。

---

# 12. Gateway

定义窄的 Backend Port，例如：

```python
class WrsGateway(Protocol):

    # Serial / Transmitter connection

    def list_serial_ports(
        self,
    ) -> list[SerialPortInfo]:
        ...

    def connect_transmitter(
        self,
        device: str,
    ) -> None:
        ...

    def disconnect_transmitter(
        self,
    ) -> None:
        ...

    def read_transmitter_info(
        self,
    ) -> TransmitterInfo:
        ...

    # PIN

    def read_transmitter_pin(
        self,
    ) -> str:
        ...

    def write_transmitter_pin(
        self,
        pin: str,
    ) -> None:
        ...

    # Transmitter LoRa

    def read_transmitter_lora_parameters(
        self,
    ) -> LoRaParameters:
        ...

    def write_transmitter_lora_parameters(
        self,
        parameters: LoRaParameters,
    ) -> None:
        ...

    def restore_transmitter_lora_defaults(
        self,
    ) -> None:
        ...

    # Transmitter GFSK

    def read_transmitter_gfsk_parameters(
        self,
    ) -> GfskParameters:
        ...

    def write_transmitter_gfsk_parameters(
        self,
        parameters: GfskParameters,
    ) -> None:
        ...

    def restore_transmitter_gfsk_defaults(
        self,
    ) -> None:
        ...

    # Receiver

    def connect_receiver(
        self,
        domain_id: int,
    ) -> None:
        ...

    def disconnect_receiver(
        self,
    ) -> None:
        ...

    def read_receiver_info(
        self,
    ) -> ReceiverInfo:
        ...

    # Receiver LoRa

    def read_receiver_lora_parameters(
        self,
    ) -> LoRaParameters:
        ...

    def write_receiver_lora_parameters(
        self,
        parameters: LoRaParameters,
    ) -> None:
        ...

    def restore_receiver_lora_defaults(
        self,
    ) -> None:
        ...

    # Receiver GFSK

    def read_receiver_gfsk_parameters(
        self,
    ) -> GfskParameters:
        ...

    def write_receiver_gfsk_parameters(
        self,
        parameters: GfskParameters,
    ) -> None:
        ...

    def restore_receiver_gfsk_defaults(
        self,
    ) -> None:
        ...

    # Factory Binding
    ...
```

不要为了减少方法数量重新创建：

```text
RadioType
GenericParameter
read_parameters(type)
write_parameters(type)
```

之类抽象。

LoRa/GFSK 在这里也直接分开。

---

# 13. MockWrsGateway

完整实现 Mock。

至少维护：

```text
available_serial_ports

transmitter_connection
transmitter_info
transmitter_pin

transmitter_lora_parameters
transmitter_gfsk_parameters

receiver_connection
receiver_settings
receiver_info

receiver_lora_parameters
receiver_gfsk_parameters

active_operation
```

提供合理默认数据。

例如 LoRa 默认：

```json
{
  "param_flags": 0,
  "tx_power": 10,
  "freq_offset": 250,
  "payload_len": 12,
  "rssi_threshold": 110,
  "heartbeat_interval": 200,
  "heartbeat_loss": 3,
  "bandwidth": 1,
  "spreading_factor": 6,
  "coding_rate": 4,
  "header_type": 0,
  "preamble_len": 12,
  "sync_word": 5156
}
```

GFSK：

```json
{
  "param_flags": 16384,
  "tx_power": 10,
  "freq_offset": 250,
  "payload_len": 12,
  "rssi_threshold": 110,
  "heartbeat_interval": 200,
  "heartbeat_loss": 3,
  "bandwidth": 1,
  "bitrate": 50000,
  "freq_deviation": 25000,
  "pulse_shaping": 9,
  "preamble_len": 16,
  "sync_word": 5156
}
```

Mock 应允许测试失败场景，但不要提供生产 REST Fault Injection API。

---

# 14. 参数同步

LoRa：

```http
POST /api/v1/receiver/lora-parameters/sync-from-transmitter
```

要求：

```text
Transmitter connected
Receiver connected
```

流程：

```text
read transmitter LoRa
        ↓
write receiver LoRa
        ↓
complete
```

GFSK：

```http
POST /api/v1/receiver/gfsk-parameters/sync-from-transmitter
```

流程：

```text
read transmitter GFSK
        ↓
write receiver GFSK
        ↓
complete
```

使用两个不同 Operation Type：

```text
sync_lora_parameters
sync_gfsk_parameters
```

不要使用：

```text
sync_radio_parameters
```

---

# 15. Factory Binding

接口：

```http
POST /api/v1/receiver/factory-bind
```

返回：

```http
202 Accepted
```

```json
{
  "operation_id": "..."
}
```

当前 C++ Transmitter 已有：

```text
DeviceKeyFrame
```

但 Client 绑定业务接口尚未最终完成。

因此当前 Factory Binding 由 Mock 模拟。

Frontend/REST 不暴露：

```text
Kbind
Transaction ID
0x01
0x02
0x03
```

Kbind 不允许：

```text
日志
错误信息
REST
WebSocket
SQLite
Audit
```

---

# 16. Operation

OperationType：

```text
sync_lora_parameters
sync_gfsk_parameters
factory_binding
```

OperationState：

```text
pending
running
succeeded
failed
interrupted
unknown
```

Operation 至少包含：

```text
operation_id
type
state
stage
progress
started_at
updated_at
error
```

`progress`：

```text
0.0 ~ 1.0
```

不要使用 0~100 整数。

接口：

```text
GET /operations/active

GET /operations/{operation_id}
```

---

# 17. WebSocket

接口：

```text
WS /api/v1/events
```

统一 Envelope：

```json
{
  "version": "1.0",
  "stream_id": "...",
  "sequence": 1,
  "event": "transmitter.connection.changed",
  "timestamp": "...",
  "data": {}
}
```

事件至少：

```text
system.phase.changed

transmitter.connection.changed
receiver.connection.changed

transmitter.lora_parameters.changed
transmitter.gfsk_parameters.changed

receiver.lora_parameters.changed
receiver.gfsk_parameters.changed

receiver.info.changed

operation.updated
```

REST 是当前状态事实来源。

WebSocket 只做状态变化通知。

WebSocket 断开重连后：

```text
GET /snapshot
```

恢复主要状态。

Hub 必须避免慢客户端阻塞其他客户端，推荐每客户端 bounded queue。

---

# 18. Snapshot

Snapshot 只返回轻量内存状态：

```text
backend phase
transmitter connection
receiver connection
active operation
```

不要因为：

```text
GET /snapshot
```

去执行：

```text
Serial read
DDS request
PIN read
LoRa read
GFSK read
```

Snapshot 不包含参数值。

---

# 19. Error Contract

Service 层不得抛：

```python
fastapi.HTTPException
```

Service 使用 Backend 自己的 typed application error。

API Layer 统一转换成：

```json
{
  "type": "urn:wrs-debugger:error:serial-port-not-found",
  "title": "Serial port not found",
  "status": 404,
  "detail": "The selected serial port is unavailable.",
  "code": "SERIAL_PORT_NOT_FOUND",
  "request_id": "...",
  "context": {}
}
```

至少需要：

```text
INVALID_ARGUMENT
VALIDATION_FAILED
RESOURCE_NOT_FOUND

BACKEND_NOT_READY
OPERATION_CONFLICT
OPERATION_TIMEOUT
OPERATION_FAILED

SERIAL_PORT_NOT_FOUND
SERIAL_PORT_PERMISSION_DENIED
SERIAL_PORT_BUSY
SERIAL_PORT_OPEN_FAILED

TRANSMITTER_ALREADY_CONNECTED
TRANSMITTER_NOT_CONNECTED
TRANSMITTER_CONNECTION_FAILED
TRANSMITTER_NOT_RECOGNIZED
TRANSMITTER_DISCONNECTED
TRANSMITTER_PROTOCOL_ERROR
TRANSMITTER_NO_RESPONSE

RECEIVER_ALREADY_CONNECTED
RECEIVER_NOT_CONNECTED
RECEIVER_CONNECTION_FAILED
RECEIVER_NOT_AVAILABLE
RECEIVER_NO_RESPONSE

PARAMETER_VALIDATION_FAILED
PARAMETER_NOT_SUPPORTED

BINDING_FAILED

SYSTEM_UNAVAILABLE
INTERNAL_ERROR
```

FastAPI/Pydantic 默认 Validation Error 也需要转换成统一 Error Contract。

---

# 20. i18n

Backend 完全语言无关。

Backend 不负责：

```text
中文 Label
英文 Label
当前语言
翻译
```

Frontend 使用：

```text
vue-i18n
```

管理：

```text
zh-CN
en-US
```

Backend 字段：

```text
heartbeat_interval
```

永远不变。

Frontend：

```text
zh-CN:
心跳包间隔

en-US:
Heartbeat Interval
```

Backend Error 主要依赖稳定：

```text
code
```

Frontend 根据 Error Code 本地化。

不要创建：

```text
/language
```

REST API。

不要根据 Accept-Language 修改业务 JSON。

---

# 21. OpenAPI

所有 Router 必须显式提供稳定：

```text
operation_id
```

例如：

```text
get_health
get_system_info
get_snapshot

list_transmitter_serial_ports
get_transmitter_connection
connect_transmitter
disconnect_transmitter

get_transmitter_info

read_transmitter_pin
write_transmitter_pin

read_transmitter_lora_parameters
write_transmitter_lora_parameters
restore_transmitter_lora_defaults

read_transmitter_gfsk_parameters
write_transmitter_gfsk_parameters
restore_transmitter_gfsk_defaults

get_receiver_settings
update_receiver_settings

get_receiver_connection
connect_receiver
disconnect_receiver

get_receiver_info

read_receiver_lora_parameters
write_receiver_lora_parameters
restore_receiver_lora_defaults
sync_receiver_lora_parameters

read_receiver_gfsk_parameters
write_receiver_gfsk_parameters
restore_receiver_gfsk_defaults
sync_receiver_gfsk_parameters

factory_bind_receiver

get_active_operation
get_operation
```

保证：

```text
/api/v1/openapi contract
```

可以稳定用于后续 TypeScript Client 生成。

---

# 22. Blocking I/O

C++：

```text
Serial
DDS
pybind11 synchronous call
```

可能阻塞。

不得直接阻塞 FastAPI Event Loop。

使用统一：

```python
anyio.to_thread.run_sync(...)
```

或单一 Native Executor。

不要让各个 Service 自己创建线程池。

生产运行采用：

```text
single process
single Uvicorn worker
```

因为 Backend 持有：

```text
Transmitter connection
Receiver DDS state
Operation state
Native objects
```

多个 worker 会导致设备状态分裂。

---

# 23. 并发

至少维护：

```text
transmitter_lock
receiver_lock
```

Transmitter Serial Request/Response 必须串行。

跨设备 Operation 固定锁顺序：

```text
Transmitter
    ↓
Receiver
```

避免死锁。

跨设备 Operation 执行时，冲突请求返回：

```text
409 OPERATION_CONFLICT
```

---

# 24. Persistence

Receiver Domain ID 可保存：

```text
$XDG_CONFIG_HOME/wrs-debugger/settings.json
```

Transmitter：

```text
/dev/ttyUSBx
```

不要持久化，因为 Linux 串口节点可能改变。

PIN 不持久化。

Kbind 不持久化。

不要为了当前 Mock 引入复杂数据库。

如果 Operation/Audit 当前实现确实需要 SQLite，再建立最小 SQLite；否则不要为了设计文档中的“可能需要”而提前增加数据库复杂度。

保持 YAGNI。

---

# 25. 推荐目录

重构为：

```text
backend/
├── pyproject.toml
├── README.md
│
├── wrs_debugger/
│   ├── main.py
│   ├── settings.py
│   │
│   ├── api/
│   │   ├── dependencies.py
│   │   ├── errors.py
│   │   └── routers/
│   │       ├── system.py
│   │       ├── transmitter.py
│   │       ├── receiver.py
│   │       └── operations.py
│   │
│   ├── models/
│   │   ├── base.py
│   │   ├── system.py
│   │   ├── serial.py
│   │   ├── connection.py
│   │   ├── transmitter.py
│   │   ├── receiver.py
│   │   ├── lora.py
│   │   ├── gfsk.py
│   │   ├── operation.py
│   │   ├── error.py
│   │   └── event.py
│   │
│   ├── services/
│   │   ├── transmitter.py
│   │   ├── receiver.py
│   │   ├── synchronization.py
│   │   └── binding.py
│   │
│   ├── gateway/
│   │   ├── base.py
│   │   ├── mock.py
│   │   └── pybind.py
│   │
│   ├── operations/
│   │   ├── manager.py
│   │   └── coordinator.py
│   │
│   └── websocket/
│       └── hub.py
│
└── tests/
    ├── unit/
    ├── api/
    ├── integration/
    └── conftest.py
```

不要创建：

```text
common/
utils/
helpers/
core/
```

等无明确领域职责的目录。

如果某个文件没有必要，不要为了机械匹配目录树创建空文件。

---

# 26. 当前旧代码迁移

删除旧模型/命名：

```text
RobotConnection
BoxConnection

ConnectRobotRequest
ConnectBoxRequest

BoxInfo

RadioConfig
LoRaPhyConfig
GfskPhyConfig

sync_receiver_config
factory_bind
```

替换为：

```text
ReceiverConnection
TransmitterConnection

ConnectReceiverRequest
ConnectTransmitterRequest

TransmitterInfo

LoRaParameters
GfskParameters

sync_lora_parameters
sync_gfsk_parameters
factory_binding
```

删除旧 Route：

```text
/api/v1/connection/robot
/api/v1/connection/box

/api/v1/box
/api/v1/box/config

/api/v1/receiver/config

/api/v1/receiver/sync-from-box
```

当前 API 尚未正式发布，不做 backward compatibility。

---

# 27. 测试要求

必须补充完整自动化测试。

至少覆盖：

## Connection

```text
无串口
多个串口

选择串口连接
重复选择当前串口
切换到另一个串口
连接不存在串口
连接失败

Transmitter disconnect

Receiver connect
Receiver disconnect

Transmitter/Receiver 独立连接
```

## Numeric validation

验证：

```text
Uint8
Uint16
Uint32
Int16
```

边界：

```text
min
max
min - 1
max + 1
```

不要在 Pydantic 测试协议业务约束。

---

## LoRa

```text
Transmitter GET
Transmitter PUT
Transmitter restore

Receiver GET
Receiver PUT
Receiver restore

Transmitter disconnected
Receiver disconnected
```

---

## GFSK

与 LoRa 同等覆盖。

特别验证：

```text
bitrate uint32
freq_deviation uint32
```

---

## PIN

```text
read
write

000000 reject
Unicode digits reject
length reject
```

---

## Sync

```text
LoRa sync

GFSK sync

Transmitter disconnected

Receiver disconnected

operation conflict
```

---

## Factory Binding

```text
success
failure
rollback/error path
```

---

## Error Contract

确认所有：

```text
RequestValidationError
ApplicationError
unexpected error
```

都输出统一 Problem Details。

---

## WebSocket

确认：

```text
connection event

LoRa changed event

GFSK changed event

operation event
```

结构稳定。

---

# 28. 代码质量要求

严格遵守现有项目：

```text
Python 3.12
ruff
mypy strict
pytest
```

要求：

```text
完整 type hints

无 Any 滥用

无大而全 service.py

无 HTTPException 泄漏到 Service

无重复业务逻辑

无 magic string 散落

清晰命名

不做 speculative abstraction

不做 generic repository framework

不做 dependency injection framework

不做 event bus framework

不做 microservice 化

不引入 Celery / Redis / MQ

不引入不必要数据库
```

保持轻量。

---

# 29. 实施阶段

整个任务控制在五个阶段以内。

## Phase 1 — Contract / Structure

完成：

```text
目录重构

Pydantic Models

LoRaParameters
GfskParameters

Error Contract

Router Contract

OpenAPI operation_id

MockWrsGateway 基础结构
```

## Phase 2 — Independent Connections

完成：

```text
Serial Port Enumeration

选择串口即连接 Transmitter

Transmitter switching/disconnect

Receiver Settings

Receiver connect/disconnect

Snapshot

Connection Events
```

## Phase 3 — Device APIs

完成：

```text
Transmitter Info

PIN

Transmitter LoRa
Transmitter GFSK

Receiver LoRa
Receiver GFSK

Restore Defaults

Mock full behavior
```

## Phase 4 — Cross-device Operations

完成：

```text
LoRa Sync
GFSK Sync

Factory Binding

Operation Manager

WebSocket Operation Events
```

## Phase 5 — Quality Convergence

完成：

```text
完整 tests

ruff

mypy strict

OpenAPI audit

README 更新

删除旧代码

检查死代码与重复代码
```

---

# 30. 验收标准

完成后必须确保：

```text
pytest
```

全部通过。

```text
ruff check
```

通过。

```text
mypy
```

strict 模式通过。

检查 OpenAPI：

```text
/api/v1/...
```

不存在旧：

```text
robot
box
config
radio-parameters
```

资源。

必须确认 API 中：

```text
transmitter/lora-parameters
transmitter/gfsk-parameters

receiver/lora-parameters
receiver/gfsk-parameters
```

全部独立存在。

必须确认：

```text
Transmitter:
选择串口 → POST connect

Receiver:
Domain ID + Connect Button → POST connect
```

两个连接模型完全独立。

必须确认 Public LoRa/GFSK JSON 字段直接使用协议原始数值，不存在：

```text
RSSI 负数转换
Bandwidth 物理量转换
Coding Rate 字符串转换
Header Type 枚举转换
Pulse Shaping 字符串转换
```

必须确认 `GfskParameters`：

```text
bitrate
freq_deviation
```

都是 `uint32` 范围。

---

# 31. 执行要求

不要只给分析或修改计划。

请直接：

1. 分析当前仓库；
2. 修改 Backend；
3. 补齐测试；
4. 运行测试、ruff、mypy；
5. 修复发现的问题；
6. 最后给出完整变更总结。

除非遇到无法从仓库、现有协议或上述要求中确定的阻塞性问题，否则不要停下来询问确认。

对于能够合理从现有设计确定的问题，请直接选择最简单、职责最清晰、最符合当前项目风格的实现。

如果当前 C++ Receiver 模块尚未提供某项实际能力：

```text
不要阻塞 Backend 实现；
使用 MockWrsGateway 完成 Backend；
保留清晰 Gateway seam；
不要伪造不存在的 pybind 实现。
```

最终输出需要包含：

```text
1. 修改的架构与目录
2. 实现的 API
3. Pydantic Model
4. Mock 行为
5. Operation / WebSocket
6. 删除或替换的旧接口
7. 测试结果
8. ruff / mypy 结果
9. 仍依赖未来 C++ Receiver / pybind 集成的边界
```

目标不是继续堆叠 Mock，而是把当前 `backend/` 收敛为可以正式支撑后续 Frontend 开发的 **Backend V1 基线**。
