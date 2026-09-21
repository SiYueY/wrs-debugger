# WRS Debugger Backend 详细设计

**文档状态：** V1 设计基线
**适用目录：** `backend/`
**Python：** 3.12
**Framework：** FastAPI + Pydantic v2 + Uvicorn
**Native Integration：** Mock → pybind11 → C++ WRS Application
**API Prefix：** `/api/v1`

---

# 1. 总体架构与业务边界

## 1.1 Backend 定位

WRS Debugger Backend 位于 Frontend 与 C++ WRS Application 之间：

```text
┌────────────────────────────────────────────┐
│                  Frontend                  │
│            Vue 3 + TypeScript              │
│                                            │
│ 页面 / 表单 / i18n / 用户交互             │
└────────────────────┬───────────────────────┘
                     │
               REST / WebSocket
                     │
                     ▼
┌────────────────────────────────────────────┐
│                  Backend                   │
│                                            │
│ FastAPI                                    │
│ Pydantic                                   │
│ Services                                   │
│ Operation Manager                          │
│ WebSocket Hub                              │
│ Persistence                                │
│ Error Mapping                              │
└────────────────────┬───────────────────────┘
                     │
                  WrsGateway
                     │
           ┌─────────┴─────────┐
           │                   │
           ▼                   ▼
    MockWrsGateway      PybindWrsGateway
        当前                  后续
                                │
                                ▼
                      C++ WRS Application
                         │             │
                    USB Serial        DDS
                         │             │
                         ▼             ▼
                   Transmitter      Receiver
```

当前首先冻结：

```text
Frontend ↔ Backend
```

包括：

* REST API；
* OpenAPI；
* Pydantic Public Model；
* WebSocket Event；
* Error Contract。

Backend 与 C++ WRS Application 之间暂时通过：

```text
WrsGateway
```

隔离。

当前：

```text
MockWrsGateway
```

用于 Backend 和 Frontend 开发。

C++ 接口稳定后实现：

```text
PybindWrsGateway
```

C++ API 如何变化，不应反向影响已经冻结的 Frontend API。

---

## 1.2 产品对象

Backend V1 只围绕：

```text
System
Transmitter
Receiver
Operation
```

组织。

不建立：

```text
Robot REST Resource
WRS REST Resource
RadioParameters
Generic Device
Generic Parameters
Top-level Binding Resource
```

其中：

### Transmitter

无线急停盒，通过：

```text
USB Serial
```

通信。

### Receiver

机器人侧无线急停接收板，通过：

```text
DDS
```

通信。

### Operation

用于描述：

```text
LoRa 参数同步
GFSK 参数同步
Factory Binding
```

等多阶段操作。

---

## 1.3 C++ 与 Backend 边界

当前 Transmitter C++ 已经明确提供：

```cpp
transmitter::LoRaParamFrame
transmitter::GfskParamFrame

Client::read_lora_parameters()
Client::write_lora_parameters()

Client::read_gfsk_parameters()
Client::write_gfsk_parameters()
```

因此 Backend 也直接使用：

```text
LoRaParameters
GfskParameters
```

分别建模。

不再建立：

```text
RadioParameters
```

中间抽象。

Receiver 后续也按同样模式：

```text
Receiver LoRa Parameters
Receiver GFSK Parameters
```

独立设计。

---

# 2. Transmitter 与 Receiver 独立连接

## 2.1 Transmitter 连接流程

Transmitter 页面没有单独的“连接”按钮。

页面首先读取：

```http
GET /api/v1/transmitter/serial-ports
```

Frontend 使用下拉框展示：

```text
/dev/ttyUSB0
/dev/ttyUSB1
/dev/ttyACM0
...
```

用户在下拉框中选择一个串口：

```text
/dev/ttyUSB0
```

**选择动作本身就表示请求连接。**

Frontend 立即调用：

```http
POST /api/v1/transmitter/connect
```

Request：

```json
{
  "device": "/dev/ttyUSB0"
}
```

因此用户交互是：

```text
刷新串口
   ↓
下拉框展示串口
   ↓
选择 /dev/ttyUSB0
   ↓
立即发起连接
   ↓
connecting
   ↓
connected / failed
```

不存在：

```text
选择串口
   ↓
再点击“连接”
```

这一额外步骤。

---

## 2.2 切换 Transmitter 串口

如果当前已经连接：

```text
/dev/ttyUSB0
```

用户改选：

```text
/dev/ttyUSB1
```

该操作同样表示：

> 切换连接到 `/dev/ttyUSB1`。

Backend 处理：

```text
当前 /dev/ttyUSB0
      ↓
关闭旧连接
      ↓
打开 /dev/ttyUSB1
      ↓
执行 Transmitter 协议验证
      ↓
更新 connection state
```

如果新串口连接失败：

```text
state = failed
device = /dev/ttyUSB1
```

旧连接不自动恢复。

用户重新选择原串口即可再次连接。

如果选择的串口就是当前已连接串口，可以直接返回当前连接状态，不重复打开设备。

---

## 2.3 USB 串口枚举

Public Model：

```python
class SerialPortInfo(ApiModel):
    device: str

    description: str | None = None

    manufacturer: str | None = None
    product: str | None = None
    serial_number: str | None = None

    vendor_id: int | None = None
    product_id: int | None = None


class SerialPortListResponse(ApiModel):
    ports: list[SerialPortInfo]
```

Example：

```json
{
  "ports": [
    {
      "device": "/dev/ttyUSB0",
      "description": "USB Serial",
      "manufacturer": "FTDI",
      "product": "USB Serial Converter",
      "serial_number": "FT123456",
      "vendor_id": 1027,
      "product_id": 24577
    }
  ]
}
```

当前 C++ 中：

```cpp
serial::list_ports()
```

适合作为这个接口的数据源。

而：

```cpp
transmitter::discover()
```

会进一步打开串口并进行 SDO 协议探测，只返回已经确认的 Transmitter，因此不适合作为下拉框中的“全部可选串口”。

---

## 2.4 Transmitter Connection

```python
class ConnectionState(StrEnum):
    DISCONNECTED = "disconnected"
    CONNECTING = "connecting"
    CONNECTED = "connected"
    FAILED = "failed"


class ConnectionError(ApiModel):
    code: str
    detail: str


class TransmitterConnection(ApiModel):
    state: ConnectionState
    device: str | None = None
    connected_at: AwareDatetime | None = None
    error: ConnectionError | None = None
```

连接 Request：

```python
class ConnectTransmitterRequest(ApiModel):
    device: str
```

接口：

```text
GET  /api/v1/transmitter/serial-ports
GET  /api/v1/transmitter/connection
POST /api/v1/transmitter/connect
POST /api/v1/transmitter/disconnect
```

其中 `/connect` 由下拉框 selection change 直接触发。

---

## 2.5 Receiver 连接

Receiver 与 Transmitter 完全独立。

Receiver 页面有明确：

```text
Domain ID
[连接]
```

用户输入 Domain ID 后点击连接按钮：

```http
POST /api/v1/receiver/connect
```

Request：

```json
{
  "domain_id": 0
}
```

Receiver 不使用 IP Address。

Public Model：

```python
class ReceiverSettings(ApiModel):
    domain_id: int


class ConnectReceiverRequest(ApiModel):
    domain_id: int


class ReceiverConnection(ApiModel):
    state: ConnectionState
    domain_id: int | None = None
    connected_at: AwareDatetime | None = None
    error: ConnectionError | None = None
```

接口：

```text
GET  /api/v1/receiver/settings
PUT  /api/v1/receiver/settings

GET  /api/v1/receiver/connection
POST /api/v1/receiver/connect
POST /api/v1/receiver/disconnect
```

`Apply`：

```text
PUT /receiver/settings
```

`Reload`：

```text
GET /receiver/settings
```

连接时使用页面当前 Domain ID，不要求先 Apply。

---

## 2.6 两个连接不存在依赖关系

以下状态全部合法：

| Transmitter  | Receiver     |
| ------------ | ------------ |
| disconnected | disconnected |
| connected    | disconnected |
| disconnected | connected    |
| connected    | connected    |

因此：

```text
Transmitter Connection
```

不能隐式改变 Receiver；

```text
Receiver Connection
```

也不能隐式改变 Transmitter。

只有跨设备操作：

```text
参数同步
Factory Binding
```

才同时要求两个连接存在。

---

# 3. LoRa 与 GFSK 参数模型

## 3.1 参数模型设计原则

Frontend 参数页面直接显示协议参数的**原始数值**。

因此不进行如下转换：

```text
rssi_threshold:
110
→
-110 dBm
```

不转换。

仍然显示：

```text
110
```

也不进行：

```text
bandwidth:
1
→
250 kHz
```

转换。

页面直接显示：

```text
1
```

同样：

```text
coding_rate = 4
header_type = 0
pulse_shaping = 9
param_flags = 0
```

均使用原始协议值。

这样：

```text
C++ Frame
     ↓
pybind11
     ↓
Pydantic
     ↓
Frontend
```

之间不需要额外的值转换逻辑。

---

## 3.2 输入范围原则

Frontend 输入范围只根据字段数值类型决定。

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
    Field(
        ge=-32768,
        le=32767,
    ),
]
```

也就是说：

| C++ 类型     |    Frontend 范围 |
| ---------- | -------------: |
| `uint8_t`  |        0 ~ 255 |
| `uint16_t` |      0 ~ 65535 |
| `uint32_t` | 0 ~ 4294967295 |
| `int16_t`  | -32768 ~ 32767 |

Backend/Pydantic 不再额外添加：

```text
433 MHz → TX Power <= 10
SF5/SF6 → Preamble == 12
sync_word 必须满足 0xY4X4
bitrate 与 freq_deviation 比例
```

等跨字段业务逻辑。

这些规则如果属于底层协议合法性要求，由 C++ WRS Application 最终检查。

当前 `transmitter::Client` 已经具有相应 Native 参数校验。

Backend 的职责只是：

```text
JSON 类型正确
+
值能够被目标 C++ 类型表示
```

---

## 3.3 LoRaParameters

LoRa 页面只处理：

```text
LoRa Parameters
```

对应当前：

```cpp
transmitter::LoRaParamFrame
```

但以下协议控制字段不进入页面：

```text
cmd
transaction_id
object_index
object_data

reserved

result_code
crc16
```

它们由 C++ 管理。

需要进入 Frontend 的参数字段保持 C++ 原始命名和原始值语义：

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

对应 C++：

```cpp
std::uint16_t param_flags;

std::int16_t tx_power;
std::uint16_t freq_offset;
std::uint8_t payload_len;
std::uint8_t rssi_threshold;

std::uint16_t heartbeat_interval;
std::uint8_t heartbeat_loss;

std::uint8_t bandwidth;
std::uint8_t spreading_factor;
std::uint8_t coding_rate;
std::uint8_t header_type;
std::uint8_t preamble_len;
std::uint16_t sync_word;
```

Backend 不做：

```text
param_flags → 一组 bool
bandwidth → 125/250/500
coding_rate → LI4/5
header_type → explicit
rssi_threshold → negative dBm
```

转换。

---

## 3.4 GfskParameters

GFSK 单独定义：

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

当前修复后的 C++：

```cpp
std::uint16_t param_flags;

std::int16_t tx_power;
std::uint16_t freq_offset;
std::uint8_t payload_len;
std::uint8_t rssi_threshold;

std::uint16_t heartbeat_interval;
std::uint8_t heartbeat_loss;

std::uint8_t bandwidth;
std::uint32_t bitrate;
std::uint32_t freq_deviation;
std::uint8_t pulse_shaping;
std::uint8_t preamble_len;
std::uint16_t sync_word;
```

已经与 Backend 模型可以直接一一对应。

此前：

```text
bitrate: uint8
freq_deviation: uint8
```

的问题已经在当前 `develop` 中修复。

---

## 3.5 为什么仍然需要 Pydantic Model

虽然 pybind 可以直接暴露：

```text
LoRaParamFrame
GfskParamFrame
```

但 Frontend REST Contract 不应直接绑定 pybind 对象。

原因不是需要做数值转换，而是为了隔离：

```text
C++ Object
```

和：

```text
JSON API
```

Backend 仍然定义：

```text
LoRaParameters
GfskParameters
```

但字段和值与 C++ 参数字段直接对应。

转换基本只是：

```text
frame.tx_power
→
model.tx_power

frame.bitrate
→
model.bitrate
```

而不是语义转换。

---

## 3.6 Transmitter 与 Receiver 都分别拥有 LoRa/GFSK

必须明确：

```text
Transmitter
├── LoRa Parameters
└── GFSK Parameters

Receiver
├── LoRa Parameters
└── GFSK Parameters
```

不能设计成：

```text
Transmitter
├── LoRa
└── GFSK

Receiver
└── generic parameters
```

两端完全对称。

当前 C++ 状态只是：

```text
Transmitter LoRa/GFSK
    → 已有初版实现

Receiver LoRa/GFSK
    → C++ 尚未完成
```

这不影响 Backend Public API 现在按照相同资源结构冻结。

---

# 4. REST API 与页面映射

## 4.1 完整资源结构

```text
/api/v1
│
├── health
│
├── system
│   └── info
├── snapshot
│
├── transmitter
│   ├── serial-ports
│   ├── connection
│   ├── connect
│   ├── disconnect
│   ├── pin
│   │
│   ├── lora-parameters
│   │   └── restore-defaults
│   │
│   └── gfsk-parameters
│       └── restore-defaults
│
├── receiver
│   ├── settings
│   ├── connection
│   ├── connect
│   ├── disconnect
│   │
│   ├── lora-parameters
│   │   ├── restore-defaults
│   │   └── sync-from-transmitter
│   │
│   ├── gfsk-parameters
│   │   ├── restore-defaults
│   │   └── sync-from-transmitter
│   │
│   └── factory-bind
│
└── operations
    ├── active
    └── {operation_id}
```

---

## 4.2 Transmitter 页面

### 连接区域

Frontend：

```text
串口
[/dev/ttyUSB0 ▼]
```

接口：

```text
GET /transmitter/serial-ports
```

当用户选择：

```text
/dev/ttyUSB0
```

立即：

```text
POST /transmitter/connect
```

不存在 Transmitter “连接”按钮。

---

### PIN

```text
GET /transmitter/pin
PUT /transmitter/pin
```

PUT 成功：

```text
204 No Content
```

---

### LoRa 参数页面

```text
GET  /transmitter/lora-parameters
PUT  /transmitter/lora-parameters
POST /transmitter/lora-parameters/restore-defaults
```

GET 返回：

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

这些就是原始协议参数值。

---

### GFSK 参数页面

```text
GET  /transmitter/gfsk-parameters
PUT  /transmitter/gfsk-parameters
POST /transmitter/gfsk-parameters/restore-defaults
```

Example：

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

---

## 4.3 参数写入语义

由于当前 C++：

```cpp
write_lora_parameters(...)
write_gfsk_parameters(...)
```

返回：

```text
Result<void, Error>
```

Backend PUT 也采用简单语义：

```http
PUT /transmitter/lora-parameters
```

Body：

```text
LoRaParameters
```

成功：

```text
204 No Content
```

GFSK 同样。

不强制 Backend 自动：

```text
write
  ↓
readback
  ↓
compare
```

如果 Frontend 需要刷新当前值，写入成功后重新：

```text
GET /.../lora-parameters
```

即可。

这样 Backend 与当前 C++ 能力保持一致，也不人为增加额外协议操作。

Restore Default 同样：

```text
POST .../restore-defaults
→ 204 No Content
```

Frontend 随后重新 GET 当前参数。

---

## 4.4 Receiver 页面

Receiver：

```text
Domain ID [ 0 ]
[连接]
```

这是唯一显式 Connect Button。

API：

```text
POST /receiver/connect
```

---

### Receiver LoRa 页面

```text
GET  /receiver/lora-parameters
PUT  /receiver/lora-parameters
POST /receiver/lora-parameters/restore-defaults

POST /receiver/lora-parameters/sync-from-transmitter
```

---

### Receiver GFSK 页面

```text
GET  /receiver/gfsk-parameters
PUT  /receiver/gfsk-parameters
POST /receiver/gfsk-parameters/restore-defaults

POST /receiver/gfsk-parameters/sync-from-transmitter
```

Receiver 不使用通用：

```text
/receiver/parameters
```

或者：

```text
/receiver/radio-parameters
```

接口。

---

## 4.5 最终 Route Table

| Method | Path                                              | Request                     | Response                   |
| ------ | ------------------------------------------------- | --------------------------- | -------------------------- |
| GET    | `/health`                                         | -                           | `HealthResponse`           |
| GET    | `/system/info`                                    | -                           | `SystemInfoResponse`       |
| GET    | `/snapshot`                                       | -                           | `SnapshotResponse`         |
| GET    | `/transmitter/serial-ports`                       | -                           | `SerialPortListResponse`   |
| GET    | `/transmitter/connection`                         | -                           | `TransmitterConnection`    |
| POST   | `/transmitter/connect`                            | `ConnectTransmitterRequest` | `TransmitterConnection`    |
| POST   | `/transmitter/disconnect`                         | -                           | `TransmitterConnection`    |
| GET    | `/transmitter`                                    | -                           | `TransmitterInfo`          |
| GET    | `/transmitter/pin`                                | -                           | `PinResponse`              |
| PUT    | `/transmitter/pin`                                | `WritePinRequest`           | 204                        |
| GET    | `/transmitter/lora-parameters`                    | -                           | `LoRaParameters`           |
| PUT    | `/transmitter/lora-parameters`                    | `LoRaParameters`            | 204                        |
| POST   | `/transmitter/lora-parameters/restore-defaults`   | -                           | 204                        |
| GET    | `/transmitter/gfsk-parameters`                    | -                           | `GfskParameters`           |
| PUT    | `/transmitter/gfsk-parameters`                    | `GfskParameters`            | 204                        |
| POST   | `/transmitter/gfsk-parameters/restore-defaults`   | -                           | 204                        |
| GET    | `/receiver/settings`                              | -                           | `ReceiverSettings`         |
| PUT    | `/receiver/settings`                              | `ReceiverSettings`          | `ReceiverSettings`         |
| GET    | `/receiver/connection`                            | -                           | `ReceiverConnection`       |
| POST   | `/receiver/connect`                               | `ConnectReceiverRequest`    | `ReceiverConnection`       |
| POST   | `/receiver/disconnect`                            | -                           | `ReceiverConnection`       |
| GET    | `/receiver`                                       | -                           | `ReceiverInfo`             |
| GET    | `/receiver/lora-parameters`                       | -                           | `LoRaParameters`           |
| PUT    | `/receiver/lora-parameters`                       | `LoRaParameters`            | 204                        |
| POST   | `/receiver/lora-parameters/restore-defaults`      | -                           | 204                        |
| POST   | `/receiver/lora-parameters/sync-from-transmitter` | -                           | `OperationCreatedResponse` |
| GET    | `/receiver/gfsk-parameters`                       | -                           | `GfskParameters`           |
| PUT    | `/receiver/gfsk-parameters`                       | `GfskParameters`            | 204                        |
| POST   | `/receiver/gfsk-parameters/restore-defaults`      | -                           | 204                        |
| POST   | `/receiver/gfsk-parameters/sync-from-transmitter` | -                           | `OperationCreatedResponse` |
| POST   | `/receiver/factory-bind`                          | -                           | `OperationCreatedResponse` |
| GET    | `/operations/active`                              | -                           | `ActiveOperationResponse`  |
| GET    | `/operations/{operation_id}`                      | -                           | `Operation`                |

实际统一位于：

```text
/api/v1
```

下。

---

# 5. Backend Service、Gateway 与 Operation

## 5.1 Service 结构

建议：

```text
TransmitterService
ReceiverService
ParameterSyncService
FactoryBindingService
OperationManager
```

### TransmitterService

负责：

```text
list_serial_ports

connect_transmitter
disconnect_transmitter

read_info

read_pin
write_pin

read_lora_parameters
write_lora_parameters
restore_lora_defaults

read_gfsk_parameters
write_gfsk_parameters
restore_gfsk_defaults
```

### ReceiverService

负责：

```text
settings

connect_receiver
disconnect_receiver

read_info

read_lora_parameters
write_lora_parameters
restore_lora_defaults

read_gfsk_parameters
write_gfsk_parameters
restore_gfsk_defaults
```

### ParameterSyncService

直接分开：

```text
sync_lora_parameters()
sync_gfsk_parameters()
```

不设计：

```text
sync_radio_parameters(type)
```

---

## 5.2 WrsGateway

Backend Gateway 同样拆开：

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

    # Receiver connection

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

    # Binding

    def prepare_transmitter_binding(
        self,
    ) -> BindingHandle:
        ...

    def prepare_receiver_binding(
        self,
        handle: BindingHandle,
    ) -> None:
        ...

    def verify_binding(
        self,
        handle: BindingHandle,
    ) -> None:
        ...

    def rollback_binding(
        self,
        handle: BindingHandle,
    ) -> None:
        ...
```

这只是 Python Backend 所需要的能力接口，不约束最终 C++ Public API 的具体形态。

---

## 5.3 PybindWrsGateway

对于 Transmitter，转换几乎是一一映射：

```text
C++ LoRaParamFrame
       ↓
pybind11
       ↓
LoRaParameters
```

例如：

```text
param_flags        → param_flags
tx_power           → tx_power
freq_offset        → freq_offset
payload_len        → payload_len
rssi_threshold     → rssi_threshold
heartbeat_interval → heartbeat_interval
...
```

不存在二次数值转换。

GFSK 同理：

```text
bitrate        → bitrate
freq_deviation → freq_deviation
```

---

## 5.4 Receiver 当前状态

当前 `wrs/include/receiver` 已经存在：

```text
dds_runtime.hpp
error.hpp
```

但尚未出现类似：

```text
ReceiverClient
LoRa Parameter Client
GFSK Parameter Client
```

因此当前阶段：

```text
Receiver Connection
Receiver LoRa
Receiver GFSK
```

都先通过 `MockWrsGateway` 开发。

后续 C++ Receiver API 完成后由：

```text
PybindWrsGateway
```

适配。

Frontend API 不因此改变。

---

## 5.5 Mock State

Mock 至少维护：

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

这样即使没有：

```text
USB Hardware
DDS Receiver
```

也能完整开发 Frontend。

---

## 5.6 参数同步

LoRa：

```text
POST /receiver/lora-parameters/sync-from-transmitter
```

流程：

```text
Read Transmitter LoRa
        ↓
Write Receiver LoRa
        ↓
Operation Complete
```

GFSK：

```text
POST /receiver/gfsk-parameters/sync-from-transmitter
```

流程：

```text
Read Transmitter GFSK
        ↓
Write Receiver GFSK
        ↓
Operation Complete
```

Operation Type：

```text
sync_lora_parameters
sync_gfsk_parameters
factory_binding
```

---

## 5.7 WebSocket

```text
WS /api/v1/events
```

事件：

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

WebSocket 只负责变化通知。

---

# 6. OpenAPI、i18n、错误、运行与工程结构

## 6.1 OpenAPI 与参数范围

Pydantic 中的：

```text
Uint8
Uint16
Uint32
Int16
```

约束会直接进入 OpenAPI。

例如：

```python
tx_power: Int16
```

OpenAPI：

```json
{
  "type": "integer",
  "minimum": -32768,
  "maximum": 32767
}
```

Frontend Number Input 可以直接读取：

```text
minimum
maximum
```

作为输入范围。

不需要在 Frontend 再维护一套 C++ 类型范围。

---

## 6.2 多语言

Backend 不负责：

```text
中文 Label
英文 Label
语言切换
```

Frontend 使用：

```text
vue-i18n
```

维护：

```text
zh-CN
en-US
```

例如字段：

```text
heartbeat_interval
```

中文：

```text
心跳包间隔
```

英文：

```text
Heartbeat Interval
```

API 字段名始终是：

```text
heartbeat_interval
```

不会随语言变化。

技术值也不翻译：

```text
0
1
4
50000
0x1424
```

---

## 6.3 Error Contract

统一：

```json
{
  "type": "urn:wrs-debugger:error:transmitter-no-response",
  "title": "Transmitter no response",
  "status": 504,
  "detail": "The transmitter did not respond.",
  "code": "TRANSMITTER_NO_RESPONSE",
  "request_id": "..."
}
```

Frontend 主要根据：

```text
code
```

映射中英文错误信息。

Backend 不根据 Locale 返回不同错误文本。

---

## 6.4 线程与并发

Serial、DDS 和 pybind C++ 调用可能阻塞。

不能直接阻塞 FastAPI Event Loop。

统一通过：

```text
anyio.to_thread.run_sync()
```

或统一 Native Executor。

Backend 生产模式只允许：

```text
single process
single Uvicorn worker
```

因为：

```text
Transmitter Client
Receiver DDS Runtime
Operation State
```

均属于 Backend 进程唯一状态。

---

## 6.5 工程目录

建议：

```text
backend/
├── pyproject.toml
├── uv.lock
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
│   ├── persistence/
│   │   ├── settings_repository.py
│   │   ├── operation_repository.py
│   │   └── audit_repository.py
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

不创建职责模糊的：

```text
common
utils
helpers
core
```

---

## 6.6 开发阶段

### Phase 1 — Public Contract

完成：

```text
Pydantic Models
OpenAPI Routes

Transmitter / Receiver Connection

LoRaParameters
GfskParameters

Problem Details
WebSocket Contract

MockWrsGateway
```

删除旧：

```text
Robot
Box
RadioConfig
RadioParameters
```

---

### Phase 2 — Independent Connection

完成：

```text
USB Serial Enumeration

选择串口即连接 Transmitter

Transmitter Switch / Disconnect

Receiver Domain ID

Receiver Connect Button

Receiver Disconnect
```

---

### Phase 3 — LoRa / GFSK Pages

完成：

```text
Transmitter LoRa
Transmitter GFSK

Receiver LoRa
Receiver GFSK

PIN

Restore Defaults

OpenAPI numeric ranges
Frontend i18n contract
```

---

### Phase 4 — Cross-device Operations

完成：

```text
LoRa Sync
GFSK Sync

Factory Binding

Operation Manager
WebSocket Events
```

---

### Phase 5 — Native Integration

完成：

```text
PybindWrsGateway

LoRaParamFrame binding
GfskParamFrame binding

Receiver C++ binding

Native Error Mapping

Integration Tests
Hardware Tests
```

---

# 最终设计基线

Backend V1 最终应形成：

```text
                  WRS Debugger
                       │
        ┌──────────────┴──────────────┐
        │                             │
        ▼                             ▼
   Transmitter                    Receiver
        │                             │
   USB Serial                        DDS
        │                             │
选择下拉框即连接                  点击按钮连接
        │                             │
        ├── PIN                       │
        │                             │
        ├── LoRa Parameters ──────────┤── LoRa Parameters
        │                             │
        └── GFSK Parameters ──────────┤── GFSK Parameters
                                      │
                                      └── Factory Binding
```

其中：

1. **Transmitter 和 Receiver 连接完全独立；**
2. **Transmitter 不存在连接按钮，选择指定串口本身就是连接请求；**
3. **Receiver 使用 Domain ID，由页面“连接”按钮发起连接；**
4. **Transmitter 和 Receiver 都分别拥有独立的 LoRa 和 GFSK 参数资源；**
5. **不存在 `RadioParameters`；**
6. **LoRa/GFSK REST API 分开；**
7. **LoRa/GFSK Frontend 页面分开；**
8. **页面直接显示协议原始数值，不做 RSSI、Bandwidth、Coding Rate 等二次转换；**
9. **Pydantic 输入范围只根据 `uint8/uint16/uint32/int16` 的可表示数值范围限制；**
10. **额外协议合法性由 C++ WRS Application 负责；**
11. **当前 GFSK `bitrate/freq_deviation` 的 C++ 类型和 42 Byte 布局已经在 `develop` 修复；**
12. **Receiver C++ 参数模块当前尚未完成，但 Backend API 先按 LoRa/GFSK 两套资源冻结；**
13. **pybind11 后续只做 C++ 字段与 Pydantic 字段的一一传递，不做不必要的物理值转换；**
14. **中文/英文只由 Frontend `vue-i18n` 处理，Backend Contract 始终语言无关。**
