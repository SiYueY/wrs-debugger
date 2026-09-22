# WRS Transmitter Simulator 设计文档

**文档版本：** V1.1
**适用项目：** WRS Debugger
**工程目录：** `simulator/transmitter_simulator/`
**可执行程序：** `transmitter_simulator`
**内部 Runtime Target：** `transmitter_runtime`
**测试 Target：** `transmitter_simulator_tests`
**目标平台：** Linux / Ubuntu 22.04
**实现语言：** C++17
**UI：** Dear ImGui + GLFW + OpenGL3
**虚拟串口：** Linux PTY
**测试框架：** Catch2
**协议基线：** `docs/wrs/serial_protocol.md`

---

# 1. 项目定位、边界与设计原则

## 1.1 背景

WRS Debugger 当前已经具备无线急停盒 Host 侧通信能力，其基本调用链为：

```text
WRS Debugger
    │
    ▼
transmitter::Client
    │
    ▼
serial::Port
    │
    ▼
USB Serial
    │
    ▼
无线急停盒
```

但当前真实无线急停盒硬件尚不可用，因此以下能力无法稳定依赖真实设备进行持续开发和验证：

* 无线急停盒连接；
* Product Code、Version Number、Serial Number 读取；
* LoRa 参数读取和配置；
* GFSK 参数读取和配置；
* 恢复默认参数；
* PIN 读取和配置；
* SDO 对象读取和写入；
* Battery 状态模拟；
* Upgrade Request 行为模拟；
* Timeout / Retry；
* CRC 错误；
* 错误 Transaction ID；
* 错误 Response Command；
* Partial Response；
* Truncated Response；
* Device Disconnect；
* Host 关闭和重新打开串口；
* Backend / Frontend 联调；
* 自动化回归测试。

现有：

```text
wrs/tests/transmitter_tests.cpp
```

已经通过 PTY 和 Fake Device 验证部分 Host 行为，但该 Fake Device 的定位仍然是：

```text
Host Component Test Fixture
```

它不应继续演变为正式 Simulator，尤其不能通过共享 Host Protocol 类型和 CRC 实现来构建设备端协议，否则 Host 和 Simulator 可能同时携带相同错误，使 E2E 测试失去协议交叉验证价值。

因此需要建立独立的：

> **WRS Transmitter Simulator**

用于模拟无线急停盒 Firmware 在 USB Serial 边界上的外部可观察行为。

---

## 1.2 Simulator 定位

Simulator V1 同时承担三类职责：

```text
开发设备
+
协议调试工具
+
自动测试基础设施
```

它不是简单的串口回包程序。

Simulator 必须具有：

```text
Independent Protocol Implementation
+
Stateful Device Model
+
Real Linux PTY Boundary
+
Fault Injection
+
Headless Runtime
+
Dear ImGui Control / Observation UI
```

最终运行关系：

```text
                         WRS Debugger
                              │
                              ▼
                    transmitter::Client
                              │
                              ▼
                         serial::Port
                              │
                              ▼
                         PTY Slave
                              │
════════════════════════ Linux TTY Boundary ════════════════════════
                              │
                              ▼
                         PTY Master
                              │
                              ▼
                    transmitter_runtime
                              │
              ┌───────────────┼────────────────┐
              ▼               ▼                ▼
          Protocol          Device            Fault
              │               │                │
              ├───────────────┼────────────────┤
              │               ▼                │
              │            History             │
              └───────────────┼────────────────┘
                              │
             ┌────────────────┴─────────────────┐
             ▼                                  ▼
   transmitter_simulator                  E2E / Tests
        Dear ImGui                         Headless
```

对于 Host 来说：

```text
真实无线急停盒
```

和：

```text
transmitter_simulator
```

均通过正常 Linux TTY 接口访问。

Host 不需要引入 Simulator 专用 Transport。

---

## 1.3 Simulator 模拟边界

Simulator 模拟的是：

> 无线急停盒 Firmware 在 USB Serial 边界上的行为。

V1 不模拟：

```text
MCU
CPU
RTOS

GPIO
ADC
SPI
RS485

真实 USB Controller
USB CDC Firmware

LoRa PHY
GFSK PHY
射频传播

Receiver

Robot
ROS 2

真实 Firmware Upgrade
Bootloader Execution
Flash Wear
```

因此 Simulator 不需要模拟：

```text
中断
DMA
MCU 时钟
射频收发时序
Flash Sector
硬件寄存器
```

Host 能够通过串口观察到的行为才属于 Simulator 的职责范围。

---

## 1.4 工程独立性

所有 Simulator 自身代码必须位于：

```text
simulator/transmitter_simulator/
```

该目录必须是完整、独立的 CMake 工程。

必须支持：

```bash
cmake \
  -S simulator/transmitter_simulator \
  -B build/transmitter-simulator \
  -DBUILD_TESTING=ON

cmake --build build/transmitter-simulator

ctest \
  --test-dir build/transmitter-simulator \
  --output-on-failure
```

整个构建过程不得要求构建：

```text
wrs/
backend/
frontend/
```

不得要求：

```text
ROS 2
FastDDS
DDS Wrapper
HUMANOID_DRIVER_ROOT
GENERIC_BIN_DIR
```

存在。

Simulator 的独立 Unit / Integration Test 也不得依赖这些模块。

---

## 1.5 禁止依赖 `wrs`

Simulator 不得直接或者间接依赖 `wrs` 的源码或 CMake Target。

禁止：

```cpp
#include <hardware/result.hpp>

#include <serial/port.hpp>
#include <serial/tool.hpp>

#include <transmitter/client.hpp>
#include <transmitter/device.hpp>
#include <transmitter/error.hpp>
#include <transmitter/protocol.hpp>
```

禁止：

```cmake
target_link_libraries(... wrs_transmitter)
```

禁止：

```cmake
target_include_directories(... ../../wrs/include)
```

正确关系：

```text
simulator/transmitter_simulator
             │
             X
             │
            wrs
```

Simulator 和 Host 的共同契约只有：

```text
docs/wrs/serial_protocol.md
```

以及：

```text
Linux TTY / PTY boundary
```

---

## 1.6 协议实现必须独立

正确关系：

```text
                  serial_protocol.md
                    /           \
                   /             \
                  ▼               ▼
        Host Implementation   Simulator
        transmitter::*        transmitter_simulator::*
```

Simulator 必须独立实现：

* 42 Byte Frame；
* Byte Offset；
* Little Endian；
* CRC16-XMODEM；
* LoRa Codec；
* GFSK Codec；
* PIN Codec；
* SDO Codec；
* Parameter Flags；
* Request Validation；
* Response Building；
* SDO Object Dictionary；
* Result Code；
* SDO Status。

不得调用 Host：

```text
Frame Codec
CRC
Endian Helper
Validation Helper
LoRaParamFrame
GfskParamFrame
PinFrame
SdoFrame
```

也不应直接复制 Host 源码后改 namespace。

Simulator 和 Host 应当是两个真正独立的协议实现。

这样 E2E 才能发现：

```text
Byte Offset 错误
Endian 错误
CRC 错误
Object Index 错误
Reserved Field 错误
Request Validation 错误
参数范围错误
Response Format 错误
```

---

## 1.7 协议权威与 Simulator 确定性行为

协议正式权威来源是：

```text
docs/wrs/serial_protocol.md
```

Simulator 不得把当前：

```text
transmitter::Client
```

实现反过来当作协议规范。

如果：

```text
serial_protocol.md
```

和当前 Host 实现存在冲突：

```text
Simulator → 按协议实现
Host      → 记录为待修复差异
```

不得为了使 Host E2E 通过而静默复制 Host Bug。

但当前协议中仍有少量行为尚未明确，例如：

* 某些非法 SDO 请求应使用哪个失败 Status；
* 非法请求是否应静默丢弃；
* 固定 42 Byte Stream 的重新同步规则；
* 详细 Result Code 尚未定义。

对于这些没有正式协议定义的场景，本设计会定义：

> **Simulator Deterministic Policy**

其目的不是修改协议，而是保证：

```text
Simulator 行为确定
测试可重复
开发者实现一致
```

这些规则应在真实 Firmware 行为确认后重新与协议对齐。

---

## 1.8 开发效率优先

本项目优先目标之一是：

> 在真实硬件不可用期间尽快建立稳定、高效的开发环境。

因此采用：

```text
成熟库优先
+
简单 Linux API 优先
+
不重复造轮子
+
不提前建设通用框架
```

V1 技术选择：

```text
GUI              Dear ImGui
Window/Input     GLFW
Renderer         OpenGL3
Testing          Catch2

PTY              openpty()
Event Multiplex  poll()
Worker Wakeup    eventfd()
I/O              read()/write()
Stable Link      symlink()/rename()
Instance Lock    flock()
```

协议和 Device Model 属于 Simulator 的核心业务，必须独立实现。

---

## 1.9 Host Compatible 与 Protocol Complete

V1 不再定义为：

```text
只实现当前 Host 使用的 5 个 SDO
```

而采用：

> **当前 USB Serial 协议范围内的完整设备端实现。**

即：

* 当前 Host 已使用的对象必须工作；
* `serial_protocol.md` 已正式定义的 SDO Object Dictionary 必须实现；
* Reserved Read Object 按协议返回 0；
* Firmware String Object 按协议分段返回；
* 未定义 Object 使用明确的 Simulator 失败语义。

这样 Simulator 不会随着 Host 新增一个已存在于协议中的 SDO 就立即落后。

---

# 2. 工程组织、依赖与公共模型

## 2.1 工程目录

最终目录：

```text
simulator/
└── transmitter_simulator/
    ├── CMakeLists.txt
    ├── README.md
    │
    ├── include/
    │   └── transmitter_simulator/
    │       ├── device.hpp
    │       ├── error.hpp
    │       ├── fault.hpp
    │       ├── history.hpp
    │       ├── protocol.hpp
    │       ├── result.hpp
    │       ├── simulator.hpp
    │       └── transport.hpp
    │
    ├── src/
    │   ├── device.cpp
    │   ├── fault.cpp
    │   ├── history.cpp
    │   ├── protocol.cpp
    │   ├── simulator.cpp
    │   ├── transport.cpp
    │   │
    │   ├── app.hpp
    │   ├── app.cpp
    │   ├── ui.hpp
    │   ├── ui.cpp
    │   └── main.cpp
    │
    └── tests/
        ├── protocol_tests.cpp
        ├── device_tests.cpp
        ├── transport_tests.cpp
        ├── fault_tests.cpp
        └── simulator_tests.cpp
```

所有 Simulator 自身代码均保存于：

```text
simulator/transmitter_simulator/
```

仓库级：

```text
Host ↔ Simulator E2E
```

由于同时依赖 `wrs` 和 Simulator，不属于 Simulator 自身独立测试，可以放在仓库现有 Host 测试体系中，例如：

```text
wrs/tests/transmitter_simulator_e2e_tests.cpp
```

其存在不改变 Simulator 的独立性。

---

## 2.2 命名

统一使用：

```text
工程目录
simulator/transmitter_simulator/

namespace
transmitter_simulator

include path
include/transmitter_simulator/

内部 runtime target
transmitter_runtime

最终 executable
transmitter_simulator

独立 test target
transmitter_simulator_tests
```

不得再出现：

```text
simulator/transmitter/

transmitter_sim

transmitter_simulator_core

transmitter_simulator_runtime

wrs-transmitter-sim
```

---

## 2.3 CMake Target

核心 Target：

```text
transmitter_runtime
transmitter_simulator
transmitter_simulator_tests
```

依赖：

```text
                    transmitter_runtime
                     /               \
                    /                 \
                   ▼                   ▼
      transmitter_simulator   transmitter_simulator_tests
```

`transmitter_runtime`：

```text
transmitter_runtime
├── protocol
├── device
├── transport
├── fault
├── history
└── simulator
```

不得依赖：

```text
Dear ImGui
GLFW
OpenGL
Catch2
wrs
```

`transmitter_simulator`：

```text
transmitter_simulator
├── main
├── app
├── ui
├── transmitter_runtime
├── Dear ImGui
├── GLFW
└── OpenGL
```

为保证最小 CI 环境也能构建并运行 Headless，增加 CMake 选项：

```text
BUILD_GUI=ON   # 默认；构建 Dear ImGui / GLFW / OpenGL GUI
BUILD_GUI=OFF  # 不查找、不编译、不链接任何 GUI 依赖
```

`BUILD_GUI=OFF` 时仍构建同名正式 executable：

```text
transmitter_simulator
```

它只支持 Headless 行为；不带 `--headless` 启动时必须给出明确诊断并以非零状态退出。无论
`BUILD_GUI` 取值如何，`transmitter_runtime` 与 `transmitter_simulator_tests` 的行为和覆盖范围
必须一致。

`transmitter_simulator_tests`：

```text
transmitter_simulator_tests
├── transmitter_runtime
└── Catch2
```

仓库级 E2E：

```text
transmitter_simulator_e2e_tests
├── wrs_transmitter
└── transmitter_runtime
```

注意依赖方向仍然是：

```text
E2E Test
├── Host
└── Simulator Runtime
```

不是：

```text
Simulator Runtime
↓
Host
```

---

## 2.4 Third Party

V1 使用 CMake `FetchContent` 在构建目录下载：

```text
imgui
glfw
Catch2
```

下载的依赖不进入工作树或版本库，且必须固定到：

```text
明确 release tag
或
明确 commit
```

不得长期直接跟随：

```text
master
main
latest
```

同时保留对应：

```text
LICENSE
upstream information
version / commit
```

方便后续升级和审计。

---

## 2.5 Dear ImGui

使用 Dear ImGui：

```text
imgui.cpp
imgui_draw.cpp
imgui_tables.cpp
imgui_widgets.cpp
```

以及：

```text
backends/imgui_impl_glfw.cpp
backends/imgui_impl_opengl3.cpp
```

如确有 `std::string` 输入需求，可加入：

```text
misc/cpp/imgui_stdlib.cpp
```

V1 不引入：

```text
ImPlot
ImNodes
ImGuizmo
```

当前需求只需要：

```text
Table
Input
Combo
Checkbox
Button
Tree
Child
Popup
Hex View
```

Dear ImGui 原生组件足够。

---

## 2.6 GLFW 与 OpenGL

GLFW 负责：

```text
Window
Keyboard
Mouse
Event Loop
OpenGL Context
```

不自行实现：

```text
X11 Wrapper
Wayland Wrapper
GLX
EGL
```

OpenGL 使用系统：

```cmake
find_package(OpenGL REQUIRED)
```

Renderer 使用：

```text
Dear ImGui OpenGL3 Backend
```

V1 不增加：

```text
GLAD
GLEW
```

除非实际选定的 Dear ImGui Backend 版本明确需要。

---

## 2.7 Catch2

Catch2 只参与：

```cmake
BUILD_TESTING=ON
```

用途：

```text
Protocol Tests
Device Tests
Transport Tests
Fault Tests
Simulator Integration Tests
```

正式：

```text
transmitter_simulator
```

不链接 Catch2。

---

## 2.8 PTY 不引入普通串口库

V1 不引入：

```text
libserialport
wjwwood/serial
c-periphery
```

这些库主要解决：

```text
打开已有串口
配置真实串口
枚举串口
串口 Client I/O
```

Simulator 需要的是：

```text
创建 PTY master/slave
Simulator 持有 master
Host 打开 slave
```

因此直接使用：

```cpp
openpty()
poll()
read()
write()
close()
```

开发量和维护成本最低。

PTY 是终端设备，默认 line discipline 不等同于 USB Serial 的二进制字节流。责任明确如下：

* production Host 在打开 Slave 后，负责按协议配置 `115200 / 8N1 / no flow control / raw`；
* Simulator 不依赖未配置 Slave 的默认 termios 行为；
* 所有直接打开 Slave 的 Transport / Integration Test 也必须显式设置 raw mode；
* `openpty()` 返回的临时 Slave FD 只用于取得路径，Simulator 不持有它来伪造 Host 已连接。

这样 PTY 仍然忠实模拟“Host 配置串口、Device 持有对端”的边界，同时避免 canonical mode、echo
或字符转换污染二进制 42 Byte Frame。

---

## 2.9 Linux 系统依赖

`transmitter_runtime` 使用：

```text
Linux libc
Threads
libutil
```

Linux-specific API：

```text
openpty()
poll()
eventfd()
flock()
symlink()
rename()
readlink()
lstat()
```

这些 API 符合 Simulator 当前：

```text
Linux-only
```

目标，不需要为了跨平台引入额外抽象。

---

## 2.10 暂不引入的依赖

V1 不增加：

```text
libserialport
wjwwood/serial
c-periphery

fmt
spdlog

nlohmann/json
yaml-cpp

CLI11
Boost.Program_options

asio
libuv

Qt
SDL

ImPlot
ImNodes

GLAD
GLEW
```

出现明确实际需求后再评估。

---

## 2.11 Error 与 Result

Simulator 不依赖：

```text
hardware::Result
```

提供局部、轻量错误模型。

例如：

```cpp
enum class Error : std::uint8_t {
    InvalidArgument,
    InvalidState,
    AlreadyRunning,
    NotRunning,

    Io,
    Busy,
    PermissionDenied,

    Disconnected,
    ProtocolError,
};
```

以及局部：

```cpp
template <typename T>
class Result;
```

要求：

```text
普通 recoverable error 显式返回
异常不用于普通协议错误
```

但不复制整个 `hardware::Result`，也不将该 Result 上升为仓库级公共组件。

如果实际调用面证明：

```text
Result<void>
```

带来明显无收益复杂度，可以采用更简单的局部设计，但必须保持错误语义明确。

---

## 2.12 Public Header 与代码注释

以下头文件：

```text
include/transmitter_simulator/*.hpp
```

使用 Doxygen 风格注释。

重点说明：

* 类型职责；
* Ownership；
* Thread Safety；
* Lifecycle；
* Error Semantics；
* Protocol Invariants；
* 是否允许跨线程调用；
* Shutdown / Reconnect 行为。

避免：

```text
“返回是否打开”
“设置值”
```

这类重复函数名称的无意义注释。

---

# 3. Runtime 架构、设备模型与完整 SDO 字典

## 3.1 Runtime 总体结构

```text
┌────────────────────────────────────────────────────────────┐
│                  transmitter_simulator                     │
│                                                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                 Dear ImGui UI                       │  │
│  │                                                      │  │
│  │ Device │ Radio │ SDO │ Protocol │ Fault Injection │  │
│  └──────────────────────┬───────────────────────────────┘  │
│                         │                                  │
│                 Typed Control API                          │
│                         │                                  │
│                         ▼                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │               transmitter_runtime                  │  │
│  │                                                      │  │
│  │ Simulator                                            │  │
│  │ ├── Device                                           │  │
│  │ ├── Protocol                                         │  │
│  │ ├── Fault                                            │  │
│  │ ├── History                                          │  │
│  │ ├── Pending Response Scheduler                       │  │
│  │ └── PTY Transport                                    │  │
│  └──────────────────────┬───────────────────────────────┘  │
└─────────────────────────┼──────────────────────────────────┘
                          │
                          ▼
                      PTY Master
                          │
══════════════════════════┼══════════════════════════════════
                          │
                          ▼
                      PTY Slave
                          │
                          ▼
                 transmitter::Client
```

---

## 3.2 Transport

`PtyTransport` 只负责 PTY 和稳定路径资源。

职责：

```text
create PTY

close PTY

read
write
poll-related fd access

slave path

stable link

peer observable state

disconnect
reconnect
```

Transport 不理解：

```text
Command
CRC
Transaction ID
LoRa
GFSK
PIN
SDO
Fault semantics
```

推荐职责接口：

```cpp
class PtyTransport final {
public:
    PtyTransport() noexcept = default;
    ~PtyTransport() noexcept;

    PtyTransport(const PtyTransport&) = delete;
    PtyTransport& operator=(const PtyTransport&) = delete;

    PtyTransport(PtyTransport&&) = delete;
    PtyTransport& operator=(PtyTransport&&) = delete;

    [[nodiscard]]
    Result<void> open(const TransportOptions& options);

    void close() noexcept;

    [[nodiscard]]
    bool is_open() const noexcept;

    [[nodiscard]]
    int native_handle() const noexcept;

    [[nodiscard]]
    Result<std::size_t> read(
        std::uint8_t* data,
        std::size_t size);

    [[nodiscard]]
    Result<std::size_t> write(
        const std::uint8_t* data,
        std::size_t size);

    [[nodiscard]]
    const std::string& slave_path() const noexcept;

    [[nodiscard]]
    const std::string& stable_path() const noexcept;
};
```

`native_handle()` 只用于 Runtime 的 Linux `poll()`。

不向 UI 暴露 fd。

---

## 3.3 Protocol

Protocol 层负责：

```text
42-byte frame

Endian

CRC16-XMODEM

Command Recognition

Request Classification

Request Structural Validation

LoRa Decode / Encode

GFSK Decode / Encode

PIN Decode / Encode

SDO Decode / Encode

Response Build
```

Protocol 不负责：

```text
openpty()
poll()
DeviceState
sleep/delay
Fault scheduling
UI
```

---

## 3.4 Device

Device 层负责：

```text
Device Identity

Firmware Metadata

PIN

LoRa State

GFSK State

Battery

Upgrade Request

Factory Defaults

Business Validation

SDO Object Read / Write

Radio Read / Write

PIN Read / Write
```

Device 不理解：

```text
PTY
poll
Raw Stream
CRC corruption
Wrong Transaction Fault
UI
```

---

## 3.5 Simulator

`Simulator` 是 Runtime orchestration layer。

外部使用窄类型 API。

例如：

```cpp
class Simulator final {
public:
    [[nodiscard]]
    Result<void> start(const SimulatorOptions& options);

    void stop() noexcept;

    [[nodiscard]]
    SimulatorSnapshot snapshot() const;

    [[nodiscard]]
    Result<void> set_identity(const DeviceIdentity& identity);

    [[nodiscard]]
    Result<void> set_pin(const Pin& pin);

    [[nodiscard]]
    Result<void> set_battery(std::uint8_t percentage);

    [[nodiscard]]
    Result<void> set_lora(const LoraConfig& config);

    [[nodiscard]]
    Result<void> set_gfsk(const GfskConfig& config);

    [[nodiscard]]
    Result<void> set_firmware_metadata(
        const FirmwareMetadata& metadata);

    [[nodiscard]]
    Result<void> reset_device();

    [[nodiscard]]
    Result<void> set_fault_config(const FaultConfig& config);

    [[nodiscard]]
    Result<void> disconnect();

    [[nodiscard]]
    Result<void> reconnect();

    [[nodiscard]]
    Result<void> clear_history();
};
```

不要求向公共 API 暴露：

```text
std::variant<ControlCommand...>
```

Runtime 内部可以使用私有命令队列实现 Worker ownership，但对 UI 和 E2E Test 暴露的是明确类型方法。

### Public API completion contract

除 `stop()` 外，所有返回 `Result<void>` 的控制方法都是**同步完成调用**：成功返回表示该命令已经
由 Worker 执行完成，相关 State、Fault 或 Lifecycle 已对后续串口 I/O 和 `snapshot()` 可见；失败返回
表示没有发生目标状态修改。实现可以使用内部 command completion / condition variable，但不得只表示
“已入队”。

这条规则保证仓库级 E2E 可以安全执行：

```cpp
REQUIRE(simulator.start(options));
REQUIRE(simulator.set_fault_config(fault));
// 此处开始发送的 Host Request 必然看到 fault。
```

`set_fault_config()` 必须在提交前验证配置；例如非法 Split / Truncate 参数返回
`Error::InvalidArgument`，不改变当前 FaultConfig。`disconnect()` 与 `reconnect()` 成功返回时分别
保证已经进入 `Disconnected` 或已经创建新的可连接 PTY。

`start()` 成功返回的线性化点为：实例 lock 已取得、PTY 已创建、stable link 已原子发布、Worker 已开始
监听 wake / PTY fd，且 `snapshot().stable_path` 可立即供 `Client::open()` 使用。任一步失败必须清理已创建
资源并返回错误；不得留下 lock、link、FD 或后台线程。

---

## 3.6 Device Identity

```cpp
struct DeviceIdentity final {
    std::uint32_t product_code{};
    std::uint32_t version_number{};
    std::uint32_t serial_number{};
};
```

当前 Host：

```text
Client::open()
```

会立即读取：

```text
0x001 Product Code
0x002 Version Number
0x003 Serial Number
```

因此这些对象必须从 Simulator 启动开始立即可用。

默认值使用：

```text
Simulator Test Data
```

不得声称是正式生产设备编号。

---

## 3.7 PIN

PIN 类型：

```cpp
struct Pin final {
    std::array<std::uint8_t, 6> digits{};
};
```

正常值：

```text
6 ASCII digits
```

默认建议：

```text
123456
```

协议中的：

```text
000000
```

被用于：

```text
PIN Read Request
```

因此不允许作为设备实际可写 PIN。

---

## 3.8 LoRa Factory State

按照协议：

```text
Radio Type            LoRa

Band                  433 MHz
TX Power              10 dBm

Frequency Offset      250 kHz
Payload Length        12
RSSI Threshold        110

Heartbeat Interval    200 ms
Heartbeat Loss        3

Bandwidth             250 kHz
Spreading Factor      SF6
Coding Rate           LI4/5
Header Type           Explicit
Preamble              12
Sync Word             0x1424

PHY CRC               Enabled
Wireless E-Stop       Enabled
Heartbeat             Enabled

Group Mode            One-to-One
Channel Scan          Single
```

---

## 3.9 GFSK Factory State

```text
Radio Type            GFSK

Band                  433 MHz
TX Power              10 dBm

Frequency Offset      250 kHz
Payload Length        12
RSSI Threshold        110

Heartbeat Interval    200 ms
Heartbeat Loss        3

Bandwidth             234.3 kHz
Bit Rate              50000
Frequency Deviation   25000

Pulse Shaping         BT 0.5
Preamble              16
Sync Word             0x1424

PHY CRC               Enabled
Wireless E-Stop       Enabled
Heartbeat             Enabled

Group Mode            One-to-One
Channel Scan          Single
```

---

## 3.10 Firmware Metadata

为了完整实现协议中的 Firmware String SDO，Device State 增加：

```cpp
struct FirmwareMetadata final {
    std::array<std::uint8_t, 40> app_firmware_version{};
    std::array<std::uint8_t, 40> bootloader_firmware_version{};

    std::array<std::uint8_t, 40> app_branch_name{};
    std::array<std::uint8_t, 40> app_tag_sha1_id{};

    std::array<std::uint8_t, 40> boot_branch_name{};
    std::array<std::uint8_t, 40> boot_tag_sha1_id{};
};
```

采用固定 40 Byte storage，而不是在 Protocol Layer 中动态拼字符串。

规则：

```text
不足 40 字节
→ 第一个未使用 Byte 写 0
→ 后续补 0
```

默认具体文本属于：

```text
Simulator Test Data
```

而不是协议规定的出厂默认值。

实现必须固定一套确定测试值，以保证 Unit / E2E 可重复。

---

## 3.11 DeviceState

推荐：

```cpp
struct DeviceState final {
    DeviceIdentity identity{};

    FirmwareMetadata firmware{};

    Pin pin{};

    LoraConfig lora{};
    GfskConfig gfsk{};

    std::uint8_t battery_percentage{100};

    std::uint32_t upgrade_request{0};
};
```

`battery_percentage` 范围：

```text
0 ~ 100
```

---

## 3.12 完整 SDO Object Dictionary

V1 实现完整协议对象字典：

|  Object Index | Object                      | Access | Simulator 行为                 |
| ------------: | --------------------------- | ------ | ---------------------------- |
|       `0x001` | Product Code                | RO     | 返回 `identity.product_code`   |
|       `0x002` | Version Number              | RO     | 返回 `identity.version_number` |
|       `0x003` | Serial Number               | RO     | 返回 `identity.serial_number`  |
|       `0x004` | Reserved                    | RO     | 返回 `0`                       |
|       `0x005` | Reserved                    | RO     | 返回 `0`                       |
|       `0x006` | Reserved                    | RO     | 返回 `0`                       |
|       `0x007` | Reserved                    | RO     | 返回 `0`                       |
| `0x008~0x011` | App Firmware Version        | RO     | 每 Object 返回连续 4 Byte         |
| `0x012~0x01B` | Bootloader Firmware Version | RO     | 每 Object 返回连续 4 Byte         |
| `0x01C~0x025` | App Branch Name             | RO     | 每 Object 返回连续 4 Byte         |
| `0x026~0x02F` | App Tag SHA1                | RO     | 每 Object 返回连续 4 Byte         |
| `0x030~0x039` | Boot Branch Name            | RO     | 每 Object 返回连续 4 Byte         |
| `0x03A~0x043` | Boot Tag SHA1               | RO     | 每 Object 返回连续 4 Byte         |
| `0x044~0x0FF` | Reserved                    | RO     | 返回 `0`                       |
|       `0x102` | Battery                     | RO     | 返回 `0~100`                   |
|       `0x202` | Upgrade Request             | RW     | Read 当前值；Write 仅接受 `0x454E`  |

其他未定义地址：

```text
Unknown Object
```

按本设计的 SDO Failure Policy 返回：

```text
SDO Status = 0xE InvalidCommand
Result = 0
```

---

## 3.13 Firmware String 分段

例如：

```text
App Firmware Version
0x008 ~ 0x011
```

共有 10 个 Object。

第 `N` 个 Object：

```text
offset = (index - base) * 4
```

返回：

```text
metadata[offset + 0]
metadata[offset + 1]
metadata[offset + 2]
metadata[offset + 3]
```

对应 Response：

```text
Byte7
Byte8
Byte9
Byte10
```

依次是字符串中的 4 个原始 ASCII Byte。

不通过主机本地 `uint32_t` 内存布局进行 reinterpret cast。

---

## 3.14 Reset

`Reset Device` 恢复：

```text
Simulator Identity Defaults

Simulator Firmware Metadata Defaults

PIN Default

LoRa Factory Defaults

GFSK Factory Defaults

Battery = 100

Upgrade Request = 0
```

不自动清理：

```text
Protocol History
```

History 使用独立：

```text
Clear History
```

---

## 3.15 Persistence

V1：

```text
No persistent state
```

每次新启动进入确定的 Simulator Factory State。

V1 不引入：

```text
JSON
YAML
SQLite
```

保存 Device State。

后续确实需要模拟 NVM 后，再单独设计：

```text
--state-file
```

不在 V1 提前建设。

---

# 4. Protocol、请求校验矩阵与失败语义

## 4.1 Frame

所有协议 Frame 固定：

```text
42 Byte
```

定义：

```cpp
constexpr std::size_t kFrameSize = 42;
constexpr std::size_t kFrameBodySize = 40;

struct Frame final {
    std::array<std::uint8_t, kFrameSize> bytes{};
};
```

禁止：

```cpp
#pragma pack
```

禁止：

```cpp
reinterpret_cast<WireStruct*>(...)
```

Wire Field 全部通过显式 Offset 读取和写入。

---

## 4.2 Endian

所有多字节整数按照协议使用 Little Endian。

独立实现：

```cpp
read_u16_le()
read_u32_le()

write_u16_le()
write_u32_le()
```

所有 Offset 使用：

```cpp
constexpr std::size_t
```

集中定义。

避免：

```text
protocol.cpp 中大量散落的数字 Offset
```

---

## 4.3 CRC16-XMODEM

参数：

```text
Polynomial = 0x1021
Init       = 0x0000
RefIn      = false
RefOut     = false
XorOut     = 0x0000
```

CRC 范围：

```text
Byte0 ~ Byte39
```

Wire：

```text
Byte40 = CRC low byte
Byte41 = CRC high byte
```

Protocol Test 使用独立 Known Vector：

```text
"123456789"
```

不得通过 Host CRC 实现生成 Expected Value。

---

## 4.4 Stream Framing

PTY 是 Byte Stream。

Runtime 维护有界 RX Buffer：

```text
read()
  ↓
append
  ↓
buffer >= 42?
  ↓
extract first 42
  ↓
process
  ↓
continue while buffer >= 42
```

例如：

```text
5 Byte
+
12 Byte
+
25 Byte
=
42 Byte Frame
```

一次读取：

```text
84 Byte
```

则处理：

```text
Frame #1
Frame #2
```

RX Buffer 必须有明确容量，不允许恶意输入导致无限增长。

实现可使用固定 storage，例如：

```text
若干个 Frame 大小的内部 buffer
```

每形成完整 Frame 就立即消费。

---

## 4.5 Stream Resynchronization

当前协议没有：

```text
SOF
Length
Escape
```

因此 Simulator 不自行创造：

```text
shift one byte
try CRC
repeat
```

这样的 sliding-window resynchronization。

如果：

```text
byte loss
insertion
truncated frame
```

导致 Stream Boundary 丢失，当前正式恢复方式是：

```text
Disconnect
→
Reconnect
```

Reconnect 必须：

```text
clear RX buffer
```

未来协议正式增加 Resynchronization Rule 后再更新。

---

## 4.6 请求处理顺序

对完整 42 Byte Frame，校验顺序固定为：

```text
1. CRC
   ↓
2. Command
   ↓
3. Transaction ID
   ↓
4. Operation Classification
   ↓
5. Command-specific Structural Validation
   ↓
6. Business / Parameter Validation
   ↓
7. Device Operation
   ↓
8. Build Normal Response
   ↓
9. Fault Injection
   ↓
10. Response Scheduling / TX
```

校验顺序决定：

```text
是否响应
响应类型
Fault 是否消费
Device State 是否变化
```

不得在实现中随意改变。

---

## 4.7 通用请求约束

所有有效请求：

```text
CRC valid
Transaction ID != 0
Byte39 == 0
```

Command 必须为：

```text
0x04
0x05
0x07
```

### CRC Invalid

行为：

```text
History:
    InvalidFrame / CrcError

DeviceState:
    unchanged

Response:
    none

Fault:
    next-response faults NOT consumed
```

### Unknown Command

行为：

```text
History:
    UnsupportedCommand

DeviceState:
    unchanged

Response:
    none

Fault:
    next-response faults NOT consumed
```

不发明：

```text
unknown-command response opcode
```

### Transaction ID == 0

协议规定 Transaction ID 必须非 0。

Simulator Policy：

```text
History:
    InvalidTransaction

DeviceState:
    unchanged

Response:
    none

Fault:
    next-response faults NOT consumed
```

原因是：

> Simulator 不生成一个同样违反“Transaction ID 必须非 0”约束的 Response。

---

## 4.8 请求分类规则

对于：

```text
Command = 0x04
```

如果：

```text
Object Index == 0x0000
```

则：

```text
Radio Read
```

否则：

```text
SDO Read
```

对于：

```text
Command = 0x05
```

如果：

```text
Object Index == 0x0000
```

则：

```text
Radio Write
```

否则：

```text
SDO Write
```

`0x07` 始终属于：

```text
PIN Read / Write
```

由 PIN 字段决定具体操作。

---

## 4.9 `0x04` Radio Read 请求校验

有效请求：

```text
Command
    0x04

Transaction ID
    non-zero

Object Index
    exactly 0x0000

Object Data
    0

Parameter Flags bit15..14
    00 LoRa
    01 GFSK

Parameter Flags bit13..0
    0

Radio parameter data bytes
    Byte13~38 = 0

Byte39
    0

CRC
    valid
```

这里：

```text
bit13..0 = 0
```

是 Simulator V1 的严格 Read Selector Policy。

读取请求只负责选择：

```text
LoRa
或
GFSK
```

不使用其他 Flag 指定当前状态。

该规则与当前 Host 请求形式一致。

如果未来协议明确允许 Read Request 在其他 Flag 中携带选择条件，再更新该策略。

### Valid Response

LoRa / GFSK 都返回：

```text
Command         = 0x84
Transaction ID  = echo request
Object Index    = 0x4000
Object Data     = 0
Result          = 0
```

参数字段填写当前 Device State。

---

## 4.10 `0x04` Radio Read 非法请求

如果：

```text
Command / Transaction / CRC
```

已经合法，但 Radio Read 的：

```text
Object Data != 0
Parameter Flags invalid
non-selector Flag != 0
parameter payload != 0
Byte39 != 0
```

则：

```text
Response Command = 0x84
Transaction ID   = echo
Object Index     = 0x4000
Result           = 0xFF
DeviceState      = unchanged
```

Response 参数字段使用当前合法 Device State，避免构造另一个无效 Response。

---

## 4.11 `0x04` SDO Read 请求校验

有效 SDO Read：

```text
Command
    0x04

Transaction ID
    non-zero

Object Index high nibble
    0x0

Object Index low 12 bits
    non-zero

Object Data
    0

Byte11~38
    0

Byte39
    0

CRC
    valid
```

注意：

```text
Object Index == 0
```

已经被分类为 Radio Read，因此 SDO Object Address 必须非 0。

---

## 4.12 `0x05` Radio Write 请求校验

有效请求：

```text
Command
    0x05

Transaction ID
    non-zero

Object Index
    exactly 0x0000

Object Data
    0

Parameter Flags
    valid

完整 LoRa / GFSK 参数
    valid

Command-specific Reserved Bytes
    0

Byte39
    0

CRC
    valid
```

完整参数必须先：

```text
Decode
↓
Temporary Config
↓
Validate Complete Config
```

只有全部合法后：

```text
Atomic Commit
```

禁止：

```text
先修改 TX Power
↓
后发现 Sync Word 非法
↓
留下 Partial State
```

---

## 4.13 `0x05` Radio Write Response

成功：

```text
Command         = 0x85
Transaction ID  = echo
Object Index    = 0x6000
Object Data     = 0
Result          = 0
```

状态：

```text
DeviceState committed
```

失败：

```text
Command         = 0x85
Transaction ID  = echo
Object Index    = 0x6000
Result          = 0xFF
```

状态：

```text
DeviceState unchanged
```

---

## 4.14 `0x05` SDO Write 请求校验

有效 SDO Write：

```text
Command
    0x05

Transaction ID
    non-zero

Object Index high nibble
    0x1

Object Index low 12 bits
    non-zero

Byte11~38
    0

Byte39
    0

CRC
    valid
```

Object Data 是否合法由对应 SDO Object 决定。

---

## 4.15 `0x07` PIN 请求校验

有效 Frame：

```text
Command
    0x07

Byte1~6
    six ASCII digits

Byte7~11
    0

Transaction ID Byte12~15
    non-zero

Byte16~38
    0

Byte39
    0

CRC
    valid
```

如果：

```text
Byte1~6 == ASCII "000000"
```

则：

```text
PIN Read
```

否则：

```text
PIN Write
```

---

## 4.16 PIN Read

Request：

```text
PIN = "000000"
```

Response：

```text
Command         = 0x87
PIN             = current stored PIN
Transaction ID  = echo
Result          = 0
```

---

## 4.17 PIN Write

合法：

```text
six ASCII digits
!= "000000"
```

先验证完整 PIN。

成功后：

```text
DeviceState.pin = requested PIN
```

Response：

```text
Command         = 0x87
PIN             = new PIN
Transaction ID  = echo
Result          = 0
```

---

## 4.18 PIN Invalid Request

如果：

```text
non-digit
reserved != 0
Byte39 != 0
其他 PIN request constraint violation
```

但：

```text
CRC valid
Command valid
Transaction valid
```

则：

```text
Command         = 0x87
PIN             = current stored PIN
Transaction ID  = echo
Result          = 0xFF
```

Device State 不改变。

这是 Simulator Deterministic Policy。

协议未来若定义更具体错误码，则替换 `0xFF`。

---

## 4.19 Parameter Flags Validation

Radio Type：

```text
bit15..14

00 → LoRa
01 → GFSK

10 → invalid
11 → invalid
```

Reserved：

```text
bit13..6 = 0
```

Channel Scan：

```text
bit5

0 → Single
1 → Hopping
```

Group Mode：

```text
bit4

当前阶段固定 0
```

Heartbeat：

```text
bit3

0 → enabled
1 → disabled
```

Wireless E-Stop：

```text
bit2

0 → enabled
1 → disabled
```

PHY CRC：

```text
bit1

0 → enabled
1 → disabled
```

Band：

```text
bit0

0 → 433 MHz
1 → 915 MHz
```

Simulator V1 明确模拟为**双频虚拟硬件**：433 MHz 与 915 MHz 都是可支持的硬件能力。因此 Radio Write
允许在两种 Band 间切换，并分别执行对应功率上限。真实单频硬件“Band 必须匹配硬件识别结果”的约束仍由
协议保留；未来若需要模拟单频 SKU，必须新增显式 `supported_bands` 设备能力并将不支持的频段确定性拒绝，
不能隐式改变当前 V1 双频行为。

Radio Read 的严格 Selector Request 只允许：

```text
bit15..14
```

其余 Flag 为 0。

Radio Write 则按照协议接受完整 Flags。

---

## 4.20 Common Radio Validation

TX Power：

```text
General:
0 ~ 22 dBm

433 MHz:
<= 10 dBm

915 MHz:
<= 20 dBm
```

必须特别保证：

```text
433 / 10 → valid
433 / 11 → invalid

915 / 20 → valid
915 / 21 → invalid
915 / 22 → invalid
```

其他公共参数：

```text
Payload Length
    exactly 12

RSSI Raw
    10 ~ 148

Heartbeat Interval
    200 ~ 10000 ms

Heartbeat Loss
    1 ~ 255
```

---

## 4.21 LoRa Validation

```text
Bandwidth
    0 / 1 / 2

Spreading Factor
    5 ~ 12

Coding Rate
    0 ~ 6

Header
    0 / 1

Preamble
    10 ~ 50
```

额外关系：

```text
SF5 / SF6
→
Preamble exactly 12
```

Sync Word：

```text
0xY4X4
```

即：

```text
each byte low nibble == 0x4
```

例如：

```text
0x1424
0x3444
```

合法。

---

## 4.22 GFSK Validation

```text
Bandwidth
    0 / 1 / 2

Bit Rate
    600 ~ 150000

Frequency Deviation
    600 ~ 300000
```

关系：

```text
4 * FrequencyDeviation >= BitRate
```

等价于协议：

```text
2 * deviation / bitrate >= 0.5
```

计算必须使用足够宽整数，避免乘法溢出。

Pulse Shaping：

```text
0x00
0x08
0x09
0x0A
0x0B
```

Preamble：

```text
16 ~ 255
```

---

## 4.23 SDO Success Semantics

SDO Read 成功：

```text
Response Command
    0x84

Object Index high nibble
    0x4

Object Index low 12 bits
    same object

Object Data
    object value

Result
    0
```

SDO Write 成功：

```text
Response Command
    0x85

Object Index high nibble
    0x6

Object Index low 12 bits
    same object

Object Data
    0

Result
    0
```

---

## 4.24 SDO Failure Semantics

为了避免：

```text
Result = 0xFF
```

提前被 Host 映射为：

```text
DeviceRejected
```

从而无法测试：

```text
SdoError
SdoInvalidCommand
```

正常 SDO Semantic Failure 使用：

```text
SDO Status
```

表达，且：

```text
Result = 0
```

冻结如下。

| 场景                                    | Response SDO Status | Object Data | Result | State     |
| ------------------------------------- | ------------------: | ----------: | -----: | --------- |
| Read Success                          |               `0x4` |       value |    `0` | unchanged |
| Write Success                         |               `0x6` |         `0` |    `0` | committed |
| Unknown Object                        |               `0xE` |         `0` |    `0` | unchanged |
| Invalid Read/Write Operation Encoding |               `0xE` |         `0` |    `0` | unchanged |
| SDO Read Object Data 非 0              |               `0xE` |         `0` |    `0` | unchanged |
| SDO Reserved Field 非 0                |               `0xE` |         `0` |    `0` | unchanged |
| SDO Request Byte39 非 0                |               `0xE` |         `0` |    `0` | unchanged |
| Write RO Object                       |               `0x8` |         `0` |    `0` | unchanged |
| Write Reserved RO Object              |               `0x8` |         `0` |    `0` | unchanged |
| Invalid `0x202` Write Value           |               `0x8` |         `0` |    `0` | unchanged |

这里：

```text
0x8 = Error
0xE = InvalidCommand
```

是 Simulator Deterministic Policy。

未来真实 Firmware 行为确认后应更新协议文档并同步 Simulator。

---

## 4.25 SDO `0x202`

Read：

```text
0x04
Object = 0x202
```

返回当前：

```text
upgrade_request
```

Write：

```text
0x05
Object = 0x202
Data   = 0x454E
```

成功：

```text
Object Index = 0x6202
Object Data  = 0
Result       = 0
```

其他写值：

```text
SDO Status = 0x8
Result     = 0
State      = unchanged
```

---

## 4.26 Reserved SDO

以下：

```text
0x004 ~ 0x007
0x044 ~ 0x0FF
```

属于合法只读 Reserved Object。

Read：

```text
Status      = 0x4
Object Data = 0
Result      = 0
```

Write：

```text
Status      = 0x8
Object Data = 0
Result      = 0
```

不得将合法 Reserved Read 当作 Unknown Object。

---

## 4.27 `0x01 / 0x02 / 0x03`

协议/Host Wire Model 已存在：

```text
0x01 Bind
0x02 Unbind
0x03 Find
```

以及 DeviceKey Frame。

但是当前调试工具 USB Serial 正式业务范围没有对应 Host API。

因此 V1：

```text
Recognize command value
↓
Record UnsupportedOperation
↓
No Device State Change
↓
No Response
```

不实现：

```text
Binding State Machine
Kbind Management
Find State
```

不得猜测未冻结业务。

---

## 4.28 Request Validation Matrix

最终实现必须至少满足：

| Operation   | 关键有效条件                | 无效行为                  | State     |
| ----------- | --------------------- | --------------------- | --------- |
| Common      | CRC valid             | CRC 错误静默丢弃            | unchanged |
| Common      | Command known         | Unknown Command 静默丢弃  | unchanged |
| Common      | TxID != 0             | 静默丢弃                  | unchanged |
| Radio Read  | Object=`0`            | `0x84 / Result=FF`    | unchanged |
| Radio Read  | Object Data=`0`       | `0x84 / Result=FF`    | unchanged |
| Radio Read  | 仅 Radio Type selector | `0x84 / Result=FF`    | unchanged |
| Radio Read  | Parameter data=`0`    | `0x84 / Result=FF`    | unchanged |
| Radio Read  | Byte39=`0`            | `0x84 / Result=FF`    | unchanged |
| Radio Write | Object=`0`            | `0x85 / Result=FF`    | unchanged |
| Radio Write | Object Data=`0`       | `0x85 / Result=FF`    | unchanged |
| Radio Write | Flags/Params valid    | `0x85 / Result=FF`    | unchanged |
| Radio Write | Reserved=`0`          | `0x85 / Result=FF`    | unchanged |
| SDO Read    | op nibble=`0`         | Status `E`            | unchanged |
| SDO Read    | Object != 0           | Status `E`            | unchanged |
| SDO Read    | Object Data=`0`       | Status `E`            | unchanged |
| SDO Read    | Reserved=`0`          | Status `E`            | unchanged |
| SDO Write   | op nibble=`1`         | Status `E`            | unchanged |
| SDO Write   | Object != 0           | Status `E`            | unchanged |
| SDO Write   | Object writable       | Status `8` if RO      | unchanged |
| SDO Write   | Value valid           | Status `8` if invalid | unchanged |
| PIN         | six ASCII digits      | `0x87 / Result=FF`    | unchanged |
| PIN         | Reserved=`0`          | `0x87 / Result=FF`    | unchanged |
| PIN         | Byte39=`0`            | `0x87 / Result=FF`    | unchanged |

实现中不得只保留一句：

```text
Request Validation
```

而不实现上述具体规则。

---

# 5. PTY、Worker Event Loop、生命周期与稳定路径

## 5.1 PTY 创建

使用：

```cpp
openpty()
```

创建：

```text
PTY Master
PTY Slave
```

Simulator 保存：

```text
Master FD
Slave Path
```

`openpty()` 返回的 Simulator-side Slave FD 在获得 Slave Path 后关闭。

Host 通过：

```text
/dev/pts/N
```

自行打开 Slave。

链路：

```text
transmitter::Client
       │
       ▼
serial::Port
       │
       ▼
PTY Slave
       │
══════════════════
       │
       ▼
PTY Master
       │
       ▼
transmitter_runtime
```

---

## 5.2 Stable Path

默认 stable path：

```text
/tmp/wrs-transmitter-simulator/tty
```

例如：

```text
/tmp/wrs-transmitter-simulator/tty
    -> /dev/pts/12
```

Debugger 开发环境可以固定连接：

```text
/tmp/wrs-transmitter-simulator/tty
```

而不关心实际 `/dev/pts/N`。

---

## 5.3 Stable Path Directory Ownership

默认目录：

```text
/tmp/wrs-transmitter-simulator/
```

必须安全创建。

如果目录不存在：

```text
mkdir
mode = 0700
owner = current uid
```

如果已经存在：

1. 使用 `lstat()`；
2. 必须是 Directory，而不是 Symbolic Link；
3. Owner 必须是当前 UID；
4. 如果权限过宽且当前用户拥有目录，可以收紧为 `0700`；
5. 如果 Owner 不匹配，则拒绝启动。

不得盲目：

```text
rm -rf /tmp/wrs-transmitter-simulator
```

---

## 5.4 单实例所有权

Stable Path 邻近维护 Lock File，例如：

```text
/tmp/wrs-transmitter-simulator/.lock
```

打开：

```text
O_CREAT
0600
```

使用：

```cpp
flock(fd, LOCK_EX | LOCK_NB)
```

Lock File 必须以 `O_CREAT | O_NOFOLLOW | O_CLOEXEC` 打开，并在打开后通过 `fstat()` 验证其为当前 UID
拥有的普通文件、权限不宽于 `0600`。必要时由当前用户收紧权限；任何不满足条件的既有 lock path 都必须
拒绝启动。不得跟随 Symbolic Link。

持有整个 Simulator 生命周期。

如果锁已被其他实例持有：

```text
start()
→ Busy
```

避免两个 Simulator 同时修改：

```text
tty
```

---

## 5.5 Stable Link 原子更新

Reconnect 不直接：

```text
unlink("tty")
symlink(...)
```

产生明显空窗。

正确方式：

```text
create temporary symlink in same directory

例如（`random` 必须不可预测，并通过排他创建避免碰撞）:
.tty.<pid>.<random>.tmp
        ↓
rename(temp, tty)
```

同一目录中的 `rename()` 用于原子替换。

更新前必须检查：

```text
existing tty path
```

如果它是：

```text
regular file
directory
FIFO
socket
```

则拒绝覆盖。

如果是已有 symlink，则只有当前实例已获得对应 lock 后才允许替换。

---

## 5.6 Custom `--pty-link`

如果用户指定：

```bash
--pty-link <path>
```

必须：

* Parent Directory 已存在；
* Parent Directory 可写；
* Parent Directory 通过 `lstat()` 确认是 Directory 而不是 Symbolic Link；
* Existing Target 如果是普通文件则拒绝；
* 不递归创建任意用户目录；
* 使用同目录临时 symlink + rename；
* 使用邻近 lock file；
* 不允许静默删除非 symlink；
* Shutdown 只删除当前实例拥有的 link。

对于 custom path，邻近 lock file 同样必须使用 `O_NOFOLLOW | O_CLOEXEC` 打开，并经过 `fstat()` 的普通文件
与 owner 校验；临时 symlink 必须创建成功后才允许 `rename()`，`EEXIST` 或类型校验失败必须报错而非覆盖。

---

## 5.7 Stable Link Cleanup

Shutdown 时不得无条件：

```text
unlink(stable_path)
```

应：

1. `lstat()` 当前 Stable Path；
2. 必须仍然是 symlink；
3. `readlink()`；
4. 当前 target 必须等于本实例创建的 `slave_path`；
5. 才执行 `unlink()`。

如果已经被其他内容替换：

```text
do not delete
```

这样避免错误删除其他进程资源。

---

## 5.8 Simulator Lifecycle 与 Peer State 分离

Simulator Lifecycle：

```cpp
enum class LifecycleState {
    Stopped,
    Running,
    Disconnected,
};
```

Peer observable state：

```cpp
enum class PeerState {
    Detached,
    Active,
};
```

二者不是同一个概念。

例如：

```text
Simulator Running
+
Peer Detached
```

表示：

> PTY 存在，但当前 Host 没有产生可观察通信。

而：

```text
Simulator Disconnected
```

表示：

> Simulator 主动关闭 PTY，用于模拟设备断线。

---

## 5.9 PTY Peer Close 行为

当 Host：

```text
close(slave fd)
```

后，Linux PTY Master 可能出现：

```text
POLLHUP
read() -> EIO
```

这些行为不应自动解释为：

```text
Simulator Fault Disconnect
```

也不得：

```text
自动销毁 PTY
自动创建新的 PTY
```

正确行为：

```text
mark PeerState = Detached
keep PTY master
keep stable link
wait for Host reopen
```

Host 重新打开同一个 Slave 后，后续 I/O 应恢复。

---

## 5.10 Peer Detached 防 Busy Loop

某些情况下：

```text
poll(master)
```

可能持续立即报告 HUP。

Worker 不得：

```text
while (true) {
    poll(...);
    // immediate HUP
}
```

造成 CPU busy-loop。

当观察到：

```text
POLLHUP
或
EIO
```

时：

```text
PeerState = Detached
```

并采用受控重新探测间隔，例如：

```text
100 ms
```

在 Detached 状态下，Worker 仍必须立即响应：

```text
eventfd
Stop
Disconnect
Reconnect
Control Command
```

但 PTY peer probe 可以使用有限间隔。

一旦重新收到有效 I/O：

```text
PeerState = Active
```

由于 PTY 无法可靠区分：

```text
Slave 已 open 但完全 idle
```

和：

```text
Slave 未 open
```

因此 `PeerState` 定义为：

> **Observable peer state**

而不是精确 USB Connection State。

---

## 5.11 Worker Wakeup

Worker 不能依赖：

```text
poll(..., infinite)
```

同时又要求其他线程无法唤醒。

Runtime 创建：

```cpp
eventfd()
```

作为：

```text
wake_fd
```

Worker `poll()` 至少监听：

```text
PTY master fd
wake_fd
```

另外 Poll Timeout 根据：

```text
pending response due time
peer detached probe deadline
```

动态计算。

---

## 5.12 UI / Test Command Wakeup

调用：

```text
set_lora()
set_fault_config()
disconnect()
reconnect()
reset_device()
...
```

时：

```text
enqueue internal command
↓
write(wake_fd)
```

Worker：

```text
poll wakes
↓
drain eventfd
↓
process command queue
```

无需忙等。

公开控制方法在 enqueue 后必须等待其 command completion，直到 Worker 已执行或 Runtime 停止；因此 GUI 与
E2E 不会出现“Fault 尚未生效就已发送请求”的竞态。Worker Stop 时必须以 `Error::InvalidState` 完成所有
尚未执行的等待命令，避免调用线程无限等待。

---

## 5.13 Stop

`Simulator::stop()`：

```text
set stop_requested
↓
write(wake_fd)
↓
Worker wakes immediately
↓
cancel pending response
↓
cancel pending split TX
↓
clear RX state
↓
discard remaining internal commands
↓
close PTY
↓
cleanup stable link
↓
release lock
↓
worker exits
↓
join
```

禁止：

```cpp
thread.detach();
```

Stop 必须：

```text
idempotent
```

重复调用不得崩溃。

---

## 5.14 SIGINT / SIGTERM

Runtime 本身不在 Signal Handler 中执行复杂资源清理。

Application Layer 的 Signal Handler 只做：

```text
set signal-safe stop flag
+
signal-safe wake notification
```

可以使用：

```text
self-pipe
```

或其他安全机制。

正常线程收到通知后调用：

```cpp
Simulator::stop()
```

GUI 模式：

```text
Window Close
SIGINT
SIGTERM
```

最终都进入同一正常 Stop Path。

---

## 5.15 Reconnect

Reconnect 是 Simulator 主动模拟：

```text
设备重新枚举
```

流程：

```text
connection_generation++
↓
cancel pending responses
↓
cancel pending split TX
↓
clear RX buffer
↓
close old PTY
↓
remove old stable link if owned
↓
create new PTY
↓
create / replace stable link atomically
↓
PeerState = Detached
↓
LifecycleState = Running
```

Host 必须重新：

```text
Client::open()
```

旧 fd 不会自动恢复。

---

## 5.16 Connection Generation

Runtime 维护：

```cpp
std::uint64_t connection_generation;
```

每次：

```text
initial PTY creation
reconnect
```

产生新的 generation。

所有 Pending Response 都记录：

```text
generation
```

任何：

```text
pending.generation != current_generation
```

的 Response 永远不得发送。

这样保证：

```text
old delayed response
```

不会在 Reconnect 后污染新连接。

---

## 5.17 Response Sequence

Runtime 每个 Connection Generation 内维护：

```cpp
std::uint64_t request_sequence;
```

每个正常处理 Request 分配：

```text
sequence++
```

Pending Response 记录：

```text
generation
sequence
request fingerprint
due time
response frame
fault state
```

用于：

```text
History
Ordering
Retry Coalescing
Delayed Response Management
```

---

## 5.18 Response Delay 不使用阻塞 Sleep

禁止：

```cpp
std::this_thread::sleep_for(response_delay);
```

直接阻塞 Worker。

正确方式：

```text
build response
↓
pending_response.due_time = now + response_delay
↓
return to event loop
```

Worker 继续可以处理：

```text
Stop
Disconnect
Reconnect
Control Commands
Retry Request
```

Poll Timeout 使用：

```text
next due_time - now
```

计算。

---

## 5.19 同一 Connection 内 Response Ordering

V1 不支持 Host Request Pipelining。

Host 当前是同步 Request/Response Client，因此 V1 Runtime 只允许：

```text
最多一个 pending normal response
```

如果当前存在 Pending Response：

### Exact Duplicate Request

满足：

```text
same command
same transaction
same verified frame body
```

则视为：

```text
Host Retry
```

行为：

```text
record Retry
do not execute Device Operation again
do not create second pending response
keep original pending response
```

这是：

> Pending-response coalescing

不是长期 Transaction Dedup Cache。

当 Pending Response 已经：

```text
sent
dropped
truncated
disconnected
```

后，同一个请求再次到达，则按新的正常 Request 处理。

因此：

```text
Drop Response
→
Host Retry
→
Device Operation executes again
```

仍然成立。

---

## 5.20 Unexpected Pipelined Request

如果 Pending Response 存在，同时收到：

```text
different command
different transaction
或
different frame body
```

则属于当前 Host Contract 外的 Pipelined Request。

V1：

```text
History:
    UnexpectedPipelinedRequest

Device:
    unchanged

Response:
    none
```

不增加复杂 Request Queue。

---

## 5.21 Retry Fingerprint

Retry 判断使用：

```text
Command
+
Transaction ID
+
verified Byte0~39 body
```

CRC 已经验证，因此不需要依赖 CRC 字节作为业务 fingerprint。

History 可显示：

```text
Retry #2
Retry #3
```

---

## 5.22 Serial Discovery 边界

V1 不：

```text
伪造 VID
伪造 PID
伪造 USB Serial Number
伪造 /sys/class/tty
修改 serial::list_ports()
修改 transmitter::discover()
```

Host 联调直接：

```cpp
Client::open(
    "/tmp/wrs-transmitter-simulator/tty",
    ...);
```

这样完整验证：

```text
serial::Port
Protocol Probe
Radio
PIN
SDO
Fault
Reconnect
```

但：

```text
USB Enumeration
```

不属于 Simulator V1。

---

# 6. Fault Injection、GUI 与测试体系

## 6.1 Fault Injection 定位

Fault Injection 是正式 V1 能力。

目的：

```text
验证 Host Error Mapping
验证 Timeout
验证 Retry
验证 Stream Handling
验证 Connection Recovery
```

默认：

```text
all faults disabled
```

Fault 必须：

```text
deterministic
repeatable
observable
```

---

## 6.2 Fault 分类

Fault 分成五层：

```text
Semantic Fault
Correlation Fault
Integrity Fault
Timing Fault
Delivery Fault
```

应用顺序固定。

---

## 6.3 Semantic Fault

普通业务失败：

```text
Fail Next Business Response
```

仅消费下一个：

```text
Radio
或
PIN
```

正常可响应请求。

效果：

```text
Result = 0xFF
```

不改变已经按照 Fault Pipeline 定义执行的 Device Operation 时机。

具体是否先执行 State Commit，见各 Fault 的语义。

对于：

```text
fail next business response
```

V1 定义为：

```text
模拟设备业务拒绝
→
Device Operation 不提交
→
Result = 0xFF
```

这与：

```text
Drop Response
```

不同。

---

## 6.4 SDO Fault

SDO 专用 one-shot：

```cpp
enum class SdoFault : std::uint8_t {
    None,
    NotReceived,
    InProgress,
    Error,
    InvalidCommand,
};
```

作用于：

```text
next SDO response
```

如果下一个 Request 不是 SDO：

```text
fault remains armed
```

分别产生：

```text
0x0 NotReceived
0xB InProgress
0x8 Error
0xE InvalidCommand
```

Result：

```text
0
```

---

## 6.5 Correlation Fault

支持：

```text
Wrong Next Transaction
Wrong Next Command
```

两者可以与其他非互斥 Fault 组合。

例如：

```text
response_delay
+
wrong transaction
+
bad CRC
```

是允许的。

---

## 6.6 Integrity Fault

```text
Corrupt Next CRC
```

顺序：

```text
build response
↓
calculate valid CRC
↓
corrupt CRC bytes
↓
delivery
```

用于验证：

```text
CrcMismatch
```

---

## 6.7 Timing Fault

```text
Response Delay
```

是持续配置，而不是 one-shot。

默认：

```text
0 ms
```

可设置：

```text
50
100
200
500
1000 ms
```

其含义仅用于模拟和测试，不声称代表真实 Firmware Timing。

Delay 使用 Pending Response Scheduler，不阻塞 Worker。

---

## 6.8 Delivery Fault

Transport-level next-response fault 使用互斥 enum：

```cpp
enum class DeliveryFault : std::uint8_t {
    None,
    Drop,
    Split,
    Truncate,
    Disconnect,
};
```

一次只能选择一个。

禁止同时：

```text
Drop + Split
Split + Truncate
Truncate + Disconnect
Drop + Disconnect
```

避免未定义组合语义。

---

## 6.9 Drop

流程：

```text
receive
↓
validate
↓
execute Device Operation
↓
build normal response
↓
consume Drop
↓
do not send
```

状态操作已经执行。

因此：

```text
Write committed
+
Response lost
```

可用于验证：

```text
retry
idempotency
```

Drop 后：

```text
no pending response remains
```

Host Retry 到达后再次执行 Operation。

---

## 6.10 Split

V1 Split Response 使用两段。

配置：

```text
split_after_bytes
split_delay
```

约束：

```text
1 <= split_after_bytes < 42
```

例如：

```text
first 10 bytes
↓
20 ms
↓
remaining 32 bytes
```

完整 Response 内容不变化。

Split 第二段使用 Pending TX Scheduler，不阻塞 Worker。

Reconnect / Stop / Disconnect 必须取消剩余 segment。

---

## 6.11 Truncate

配置：

```text
truncate_after_bytes
```

约束：

```text
1 <= truncate_after_bytes < 42
```

行为：

```text
send first N bytes
↓
stop sending remainder
```

不自动关闭 PTY。

预期 Host：

```text
partial frame
↓
timeout
↓
FrameSyncLost
↓
Host closes connection
```

由于协议无 Resync，后续正常恢复依赖 Reconnect。

---

## 6.12 Disconnect After Next Request

`DeliveryFault::Disconnect` 定义为：

```text
receive valid request
↓
validate
↓
execute normal Device Operation
↓
build response
↓
consume Disconnect
↓
cancel response
↓
close PTY
↓
remove stable link
↓
Lifecycle = Disconnected
↓
no TX
```

因此：

> Disconnect 是 Terminal Delivery Action。

它不会进入最终：

```text
TX
```

阶段。

---

## 6.13 Disconnect Now

这是：

```text
Control Action
```

而不是 Pending Response Fault。

UI / Test 调用：

```cpp
simulator.disconnect();
```

立即：

```text
cancel pending response
cancel pending split TX
connection_generation++
close PTY
cleanup stable link
Lifecycle = Disconnected
```

---

## 6.14 Fault Consumption

只有在：

```text
一个请求已经通过基本 Frame / Command / Transaction Validation
并且 Runtime 已经准备构造 Response
```

时，next-response Fault 才消费。

以下不消费 Fault：

```text
bad CRC
unknown command
transaction == 0
unsupported 0x01/0x02/0x03
```

这样 malformed traffic 不会意外吃掉测试配置。

---

## 6.15 Fault Processing Order

固定：

```text
Receive Request
      ↓
CRC / Command / Transaction Validation
      ↓
Command-specific Validation
      ↓
Device Operation or Deterministic Failure
      ↓
Build Normal Response
      ↓
Business Result / SDO Status Fault
      ↓
Wrong Command / Wrong Transaction
      ↓
Calculate CRC
      ↓
Corrupt CRC
      ↓
Schedule Response Delay
      ↓
Delivery Fault
    ├── Normal TX
    ├── Drop
    ├── Split
    ├── Truncate
    └── Disconnect
```

Disconnect 不再被理解为：

```text
执行完后继续 TX
```

---

## 6.16 FaultConfig

推荐：

```cpp
enum class SdoFault : std::uint8_t {
    None,
    NotReceived,
    InProgress,
    Error,
    InvalidCommand,
};

enum class DeliveryFault : std::uint8_t {
    None,
    Drop,
    Split,
    Truncate,
    Disconnect,
};

struct FaultConfig final {
    std::chrono::milliseconds response_delay{0};

    bool fail_next_business_response{false};

    SdoFault next_sdo_fault{SdoFault::None};

    bool wrong_next_transaction{false};
    bool wrong_next_command{false};

    bool corrupt_next_crc{false};

    DeliveryFault next_delivery_fault{
        DeliveryFault::None};

    std::size_t split_after_bytes{0};
    std::chrono::milliseconds split_delay{0};

    std::size_t truncate_after_bytes{0};
};
```

约束：

```text
DeliveryFault::Split
→ split_after_bytes valid

DeliveryFault::Truncate
→ truncate_after_bytes valid
```

配置非法则：

```text
set_fault_config()
→ InvalidArgument
```

---

## 6.17 PendingResponse

内部建议：

```cpp
struct PendingResponse final {
    std::uint64_t connection_generation{};
    std::uint64_t request_sequence{};

    RequestFingerprint request{};

    Frame frame{};

    std::chrono::steady_clock::time_point due_time{};

    DeliveryFault delivery{DeliveryFault::None};

    std::size_t split_after_bytes{};
    std::chrono::milliseconds split_delay{};

    std::size_t truncate_after_bytes{};
};
```

如果 Split 第一段已经发送，还需要内部：

```text
PendingSegment
```

状态。

Reconnect / Stop 统一取消。

---

## 6.18 Protocol History

History 固定容量：

```text
2000 records
```

记录类型至少包括：

```text
RxFrame
TxFrame

InvalidFrame
UnsupportedCommand
InvalidTransaction
InvalidRequest

Retry
UnexpectedPipelinedRequest

FaultApplied

PeerDetached
PeerActive

Disconnect
Reconnect
```

每条 Frame History：

```text
Timestamp
Direction
Command
Transaction ID
Object
Result
SDO Status
CRC
Connection Generation
Request Sequence
Note
Raw Frame
```

---

## 6.19 Dear ImGui 页面

主界面保留五个主要页面：

```text
Device
Radio
SDO
Protocol
Fault Injection
```

---

## 6.20 Device 页面

显示：

```text
Lifecycle State
Peer State

PTY Slave
Stable Path
Connection Generation

RX Frames
TX Frames
RX Bytes
TX Bytes

Product Code
Version Number
Serial Number

PIN
Battery
Upgrade Request
```

操作：

```text
Apply Internal State

Reset Device

Disconnect
Reconnect
```

明确区分：

```text
Peer Detached
```

和：

```text
Simulator Disconnected
```

---

## 6.21 Radio 页面

分：

```text
LoRa
GFSK
```

LoRa：

```text
Band
TX Power
Frequency Offset
Payload Length
RSSI

Heartbeat Interval
Heartbeat Loss

PHY CRC
Wireless E-Stop
Heartbeat
Group Mode
Channel Scan

Bandwidth
Spreading Factor
Coding Rate
Header Type
Preamble
Sync Word
```

GFSK：

```text
Band
TX Power
Frequency Offset
Payload Length
RSSI

Heartbeat Interval
Heartbeat Loss

PHY CRC
Wireless E-Stop
Heartbeat
Group Mode
Channel Scan

Bandwidth
Bit Rate
Frequency Deviation
Pulse Shaping
Preamble
Sync Word
```

UI Apply 和 Serial Write 必须共用：

```text
validate_lora()
validate_gfsk()
```

同一套 Device Validation。

---

## 6.22 SDO 页面

SDO 页面不再只展示 5 个 Object。

应覆盖完整 Object Dictionary。

推荐按逻辑分组：

```text
Identity
├── 0x001 Product Code
├── 0x002 Version Number
└── 0x003 Serial Number

Firmware
├── App Firmware Version
├── Bootloader Firmware Version
├── App Branch
├── App Tag SHA1
├── Boot Branch
└── Boot Tag SHA1

Runtime
├── 0x102 Battery
└── 0x202 Upgrade Request

Reserved
├── 0x004~0x007
└── 0x044~0x0FF
```

Reserved Range 不需要逐行显示几百个对象，可以显示：

```text
Range
Access
Read Value
```

Simulator UI Editability 与 Protocol Access 必须区分。

例如：

```text
Battery:
Host Protocol → RO
Simulator UI  → editable
```

因为 UI 编辑代表设备内部状态变化。

---

## 6.23 Protocol 页面

Table：

| Time | Dir | Gen | Seq | Cmd | TxID | Object | Result/Status | CRC | Note |
| ---- | --- | --: | --: | --: | ---: | -----: | ------------- | --- | ---- |

选中记录后显示：

```text
Timestamp

Connection Generation
Request Sequence

Direction

Command
Transaction ID

Object Index
Object Data

Param Flags

Result
SDO Status

CRC

Decoded Operation

Fault Applied

Raw 42 Bytes
```

支持：

```text
Auto Scroll
Pause View
Clear
```

`Pause View`：

```text
只暂停 UI 跟随
```

不能暂停：

```text
PTY RX/TX
Runtime
Fault
```

---

## 6.24 Fault Injection 页面

显示：

```text
Response Delay

Fail Next Business Response

Next SDO Status Fault

Wrong Next Transaction
Wrong Next Command

Corrupt Next CRC

Next Delivery Fault
    None
    Drop
    Split
    Truncate
    Disconnect

Split Offset
Split Delay

Truncate Offset
```

另外提供：

```text
Disconnect Now
```

作为立即控制操作。

UI 必须明确展示：

```text
armed one-shot fault
```

以及 Fault 被消费后的自动复位结果。

---

## 6.25 Headless Mode

正式 executable：

```bash
./transmitter_simulator --headless
```

支持：

```bash
./transmitter_simulator \
    --headless \
    --pty-link /tmp/wrs-transmitter-simulator/tty
```

Headless 模式：

```text
不初始化 GLFW
不初始化 OpenGL
不初始化 Dear ImGui
```

但完整运行：

```text
PTY
Worker
Protocol
Device
Fault
History
```

成功进入 Running 后，Headless 必须向 stdout 输出并立即 flush 单行 machine-readable ready record：

```text
WRS_TRANSMITTER_SIMULATOR_READY stable_path=<path> pid=<pid>
```

此记录只表示 PTY、stable link 与 Worker 都已就绪，不包含 PIN、Kbind 或其他敏感内容。参数、创建 PTY、
取得 lock 或启动 Worker 任一失败时，必须向 stderr 给出简短原因并以非零状态退出；不得输出 READY。

---

## 6.26 Headless CLI 范围

V1 CLI 仍然只提供：

```text
--headless
--pty-link <path>
--help
--version
```

不增加：

```text
--fault-drop
--fault-crc
--battery
--pin
--tx-power
...
```

原因：

> Headless CLI 不是 CI Fault Control Plane。

Fault E2E 使用：

```text
direct transmitter_runtime API
```

解决。

这样避免：

```text
测试专用 CLI 膨胀
JSON/YAML Scenario
Unix Socket IPC
额外控制协议
```

---

## 6.27 Repository-level E2E 控制方式

仓库级 E2E Test 直接链接：

```text
wrs_transmitter
+
transmitter_runtime
```

例如：

```cpp
transmitter_simulator::Simulator simulator;

simulator.start(...);

simulator.set_fault_config(...);

transmitter::Client client;

client.open(simulator.snapshot().stable_path, ...);
```

因此 Test 可以直接：

```text
设置 Fault
观察 Snapshot
观察 History
触发 Reconnect
```

不需要：

```text
GUI
CLI Fault Parameter
Unix Socket
IPC
```

---

## 6.28 E2E 依赖方向

允许：

```text
Repository E2E Test
├── wrs_transmitter
└── transmitter_runtime
```

禁止：

```text
transmitter_runtime
└── wrs_transmitter
```

因此：

```text
simulator/transmitter_simulator
```

仍然可以独立构建。

---

## 6.29 Protocol Tests

使用 Catch2。

覆盖：

```text
CRC Known Vector

Endian

LoRa Read Request Decode
LoRa Read Response Encode
LoRa Write Decode
LoRa Write Response

GFSK Read
GFSK Write

PIN Read
PIN Write

SDO Read
SDO Write

Firmware String segmentation

Reserved SDO read

Unknown SDO

RO SDO write

Upgrade Request

Param Flags

Result

Reserved Bytes
```

Golden Frame 使用：

```text
literal byte arrays
```

不得调用 Host Protocol Helper 生成 Expected Data。

---

## 6.30 Request Validation Tests

单独按照矩阵测试：

```text
Transaction == 0

Request Result != 0

Radio Read Object Data != 0

Radio Read Parameter Payload != 0

Invalid Radio Type

Reserved Flag != 0

Group Mode != 0

SDO Read op nibble != 0

SDO Write op nibble != 1

SDO Read Object Data != 0

SDO reserved bytes != 0

PIN reserved bytes != 0

PIN invalid ASCII
```

每个测试同时验证：

```text
是否响应
响应 Result / Status
State 是否变化
Fault 是否消费
```

---

## 6.31 Device Tests

覆盖：

```text
Factory State

Identity

Firmware Metadata

PIN

LoRa

GFSK

Battery

Upgrade Request

Reserved SDO

Unknown SDO

Reset
```

Radio 边界必须包括：

```text
433 / 10 → success
433 / 11 → failure

915 / 20 → success
915 / 21 → failure
```

以及：

```text
Payload != 12

RSSI < 10
RSSI > 148

Heartbeat < 200
Heartbeat > 10000

Heartbeat Loss = 0

SF5 wrong preamble
SF6 wrong preamble

Invalid LoRa Sync

Bitrate < 600
Bitrate > 150000

Deviation < 600
Deviation > 300000

4 * deviation < bitrate

Invalid Pulse Shaping
```

---

## 6.32 Transport Tests

直接使用 Linux PTY。

覆盖：

```text
openpty

slave path

stable path

directory ownership

lock acquisition

second-instance rejection

atomic symlink replace

existing regular file rejection

read

write

partial IO

Host close

POLLHUP / EIO

Peer Detached

Host reopen

Peer Active

simulated Disconnect

Reconnect

RX clear

stable link cleanup
```

特别验证：

> Host Slave Close 不等同于 Simulator Disconnect。

---

## 6.33 Worker / Lifecycle Tests

覆盖：

```text
Stop while poll blocked

Stop while response delayed

Stop while split response pending

Disconnect while response delayed

Reconnect while response delayed

Reconnect cancels old generation response

No detached thread

Repeated stop()

Repeated disconnect/reconnect
```

确保 Worker 可以通过：

```text
eventfd
```

及时唤醒并 Join。

---

## 6.34 Delay / Retry Tests

至少覆盖：

### Retry While Response Pending

```text
Request #1
↓
Response delayed
↓
Host retries same frame
↓
Simulator identifies duplicate
↓
no second Device Operation
↓
one response only
```

### Drop Then Retry

```text
Request #1
↓
Device operation
↓
Response dropped
↓
Host retry
↓
Device operation executes again
↓
normal response
```

### Reconnect Cancels Delayed Response

```text
Request
↓
delayed pending response
↓
Reconnect
↓
generation++
↓
old response discarded
↓
never appears on new PTY
```

---

## 6.35 Fault Tests

覆盖：

```text
Business Failure

SDO NotReceived

SDO InProgress

SDO Error

SDO InvalidCommand

Wrong Transaction

Wrong Command

Bad CRC

Delay

Drop

Split

Truncate

Disconnect
```

验证 One-shot：

```text
first applicable response
→ fault applied

next applicable response
→ normal
```

验证 SDO Fault：

```text
armed
↓
non-SDO request
↓
fault remains armed
↓
SDO request
↓
fault consumed
```

---

## 6.36 Simulator Integration Tests

不初始化 GUI。

直接：

```text
request bytes
↓
PTY
↓
Simulator Runtime
↓
response bytes
```

验证：

```text
full request → response

partial request input

multiple frames in one read

Device State transition

History record

Fault application

Reconnect
```

---

## 6.37 Host ↔ Simulator E2E 正常路径

至少覆盖：

### Connection

```text
Client::open
↓
0x001
↓
0x002
↓
0x003
↓
Ready
```

### LoRa

```text
Read
Write
Read Back
Restore Default
Read Back
```

### GFSK

```text
Read
Write
Read Back
Restore Default
Read Back
```

### PIN

```text
Read
Write
Read Back
```

### SDO

当前 Host API 能覆盖：

```text
Product Code
Version Number
Serial Number
Battery
Upgrade Request
```

Simulator 自身测试另外覆盖完整 SDO Object Dictionary。

---

## 6.38 Host ↔ Simulator E2E Fault 路径

覆盖：

```text
TimedOut

CrcMismatch

UnexpectedResponse

DeviceRejected

FrameSyncLost

Disconnected

SdoNotReceived

SdoInProgress

SdoError

SdoInvalidCommand

Retry same transaction

Split response

Reconnect
```

Fault 由 E2E 直接调用：

```text
transmitter_runtime API
```

配置。

不依赖 GUI。

---

## 6.39 Existing Host Component Tests

现有：

```text
wrs/tests/transmitter_tests.cpp
```

继续保留。

测试层级：

```text
Level 1
Host Component Tests
    wrs/tests/transmitter_tests.cpp

Level 2
Simulator Unit Tests
    simulator/transmitter_simulator/tests/

Level 3
Host ↔ Independent Simulator E2E

Level 4
Host ↔ Real Hardware Acceptance
```

Level 1 不需要因为 Simulator 出现而全部删除。

它仍然适合：

```text
快速错误路径测试
局部 Client 行为验证
```

---

## 6.40 代码质量要求

全部使用：

```text
C++17
```

要求：

* RAII；
* 明确 Ownership；
* 单一职责；
* 窄接口；
* Const Correctness；
* 需要检查的结果使用 `[[nodiscard]]`；
* 明确不会抛出的 API 合理使用 `noexcept`；
* 不使用 owning raw pointer；
* 不使用 detached thread；
* 不使用 global mutable state；
* 不使用 packed wire struct；
* 不用 exception 表达普通协议失败；
* 不散布 magic offset；
* 不复制明显重复逻辑；
* FD 生命周期明确；
* eventfd 生命周期明确；
* Lock FD 生命周期明确；
* malformed frame 不导致 crash；
* History 固定容量；
* RX storage 有明确边界；
* Pending Response 有明确生命周期；
* Shutdown 无资源泄漏；
* Reconnect 无旧 Response 泄漏；
* Stable Link Cleanup 不删除非本进程资源。

---

## 6.41 避免过度抽象

V1 不创建：

```text
ITransport
IProtocol

GenericDevice
GenericFrame

SimulatorFramework
DeviceFramework

PluginSystem
PropertySystem

ManagerFactory
```

只有出现真正第二个复用对象后才评估。

不要为了内部 Command Queue 在 Public API 暴露复杂：

```text
std::variant
std::any
string-based property
```

外部控制接口保持类型明确。

---

## 6.42 命名

避免：

```text
data
info
ctx
mgr
helper
util
do_work()
process()
```

这类无法表达真实职责的泛化命名。

优先：

```text
parse_radio_read_request

build_sdo_response

validate_lora

validate_gfsk

read_sdo_object

write_sdo_object

create_pty

replace_stable_link

mark_peer_detached

schedule_response

cancel_pending_response

record_received_frame
```

---

## 6.43 Logging

V1 不引入：

```text
spdlog
```

使用：

```text
Protocol History
+
必要 stdout/stderr diagnostics
```

即可。

Headless READY record 是上述 stdout 的唯一稳定机器接口；其他 stdout/stderr diagnostics 面向人类，测试
不得依赖其具体文本。

敏感字段未来如：

```text
Kbind
```

不得直接输出。

---

# 7. 实施阶段与最终验收标准

## 7.1 Phase 1 — Independent Protocol + Device

首先建立：

```text
simulator/transmitter_simulator/
```

以及：

```text
CMakeLists.txt
README.md
include/
src/
tests/
```

Third Party 仅由 CMake 在构建目录获取，不属于工程目录内容。

完成：

```text
Error / Result

Frame
Endian
CRC

Request Classification

Request Validation Matrix

LoRa Codec
GFSK Codec

PIN Codec

Complete SDO Object Dictionary

Firmware Metadata Segmentation

DeviceState

Factory Defaults

Radio Validation

SDO Failure Semantics
```

同步完成：

```text
Protocol Tests
Request Validation Tests
Device Tests
```

阶段目标：

> 建立完整、独立、协议驱动的设备端模型。

---

## 7.2 Phase 2 — PTY + Runtime

完成：

```text
PtyTransport

openpty

stable directory ownership

lock file

atomic symlink update

read/write

peer observable state

POLLHUP / EIO handling

eventfd worker wakeup

RX stream buffer

Request Dispatcher

PendingResponse Scheduler

connection_generation

request_sequence

retry coalescing

History

Stop

Disconnect

Reconnect

Headless Runtime
```

同步完成：

```text
Transport Tests
Worker Tests
Lifecycle Tests
Delay / Retry Tests
```

阶段目标：

> Runtime 在没有 GUI 的情况下已经完整可运行。

---

## 7.3 Phase 3 — Fault + E2E

完成：

```text
Business Failure

SDO Fault

Wrong Transaction

Wrong Command

Bad CRC

Response Delay

Drop

Split

Truncate

Disconnect
```

以及：

```text
Fault Tests

Simulator Integration Tests

Repository-level
Host ↔ Simulator E2E
```

E2E 直接链接：

```text
wrs_transmitter
+
transmitter_runtime
```

进行 Fault 控制。

不增加测试 IPC。

阶段目标：

> Simulator 已成为可靠的自动化测试基础设施。

---

## 7.4 Phase 4 — Dear ImGui 与产品化

最后实现：

```text
Device

Radio

SDO

Protocol

Fault Injection
```

五个页面。

同时补齐：

```text
GUI Lifecycle

Headless CLI

README

Build Instructions

Developer Usage

Third Party License Information

Final Code Cleanup
```

不要先完成漂亮 UI 再补 Protocol 和测试。

---

## 7.5 独立构建验收

必须实际执行：

```bash
cmake \
  -S simulator/transmitter_simulator \
  -B build/transmitter-simulator \
  -DBUILD_TESTING=ON \
  -DBUILD_GUI=OFF

cmake --build build/transmitter-simulator -j

ctest \
  --test-dir build/transmitter-simulator \
  --output-on-failure
```

另在具备 GUI 系统依赖的开发机验证默认 `BUILD_GUI=ON`，确保 GUI target 仍可构建。

不得依赖：

```text
wrs
ROS 2
FastDDS
DDS Wrapper
humanoid-driver
backend
frontend
```

---

## 7.6 Source Dependency 验收

执行类似：

```bash
grep -R \
  -E '#include <(serial|transmitter|hardware)/' \
  simulator/transmitter_simulator
```

不得发现 production `wrs` include。

CMake 中不得存在：

```text
wrs_transmitter

wrs_receiver_dds

../../wrs/include
```

---

## 7.7 Third Party 验收

Simulator 独立工程不记录 Third Party 源码；CMake 仅在构建目录获取固定版本的 Dear ImGui、GLFW 与 Catch2。

Catch2 仅：

```text
BUILD_TESTING
```

参与。

---

## 7.8 Runtime 验收

必须生成：

```text
transmitter_runtime
```

其职责包含：

```text
Protocol

Device

Transport

Fault

History

Pending Response Scheduler

Simulator
```

不得依赖 GUI。

---

## 7.9 Executable 验收

正式用户程序：

```text
transmitter_simulator
```

GUI：

```bash
./transmitter_simulator
```

Headless：

```bash
./transmitter_simulator --headless
```

指定 Stable Link：

```bash
./transmitter_simulator \
    --headless \
    --pty-link /tmp/wrs-transmitter-simulator/tty
```

---

## 7.10 Headless Smoke Test

实际验证：

```text
start
↓
PTY created
↓
stable link created
↓
worker running
↓
stdout READY record flushed
↓
SIGINT / SIGTERM
↓
worker wakes
↓
pending TX cancelled
↓
PTY closed
↓
stable link safely removed
↓
lock released
↓
thread joined
```

不得留下：

```text
orphan thread
stale fd
incorrect symlink cleanup
```

---

## 7.11 Protocol 验收

必须支持：

```text
42 Byte Frame

Little Endian

CRC16-XMODEM

Transaction ID non-zero

Request Result Byte = 0

Reserved Fields

0x04 / 0x84

0x05 / 0x85

0x07 / 0x87

Radio Read Object 0x4000

Radio Write Object 0x6000

完整 SDO Object Dictionary

SDO Success / Error Status
```

---

## 7.12 SDO 验收

必须验证：

```text
0x001 Product Code

0x002 Version Number

0x003 Serial Number

0x004~0x007 Reserved Read 0

0x008~0x011 App Firmware

0x012~0x01B Bootloader Firmware

0x01C~0x025 App Branch

0x026~0x02F App Tag SHA1

0x030~0x039 Boot Branch

0x03A~0x043 Boot Tag SHA1

0x044~0x0FF Reserved Read 0

0x102 Battery

0x202 Upgrade Request
```

同时验证：

```text
Unknown Object → Status E

Write RO → Status 8

Invalid Upgrade Value → Status 8
```

---

## 7.13 Request Validation 验收

每种 Operation 必须实际测试：

```text
Transaction ID

Object Index

Object Data

Reserved Bytes

Parameter Flags

Request Result Byte

Business Parameters
```

以及：

```text
Response or No Response

Result / SDO Status

State Mutation
```

不允许存在“实现自行决定”的未冻结分支。

---

## 7.14 Radio 验收

LoRa：

```text
Read
Write
Read Back
Restore Default
Read Back
```

GFSK：

```text
Read
Write
Read Back
Restore Default
Read Back
```

边界：

```text
433 MHz / 10 dBm valid
433 MHz / 11 dBm reject

915 MHz / 20 dBm valid
915 MHz / 21 dBm reject
```

---

## 7.15 PIN 验收

通过：

```text
Read

Write

Read Back

Reject non-digit

Reject reserved violation

Reject actual "000000" write ambiguity
```

---

## 7.16 PTY Peer 验收

验证：

```text
Simulator Running
+
Host not open
```

不会：

```text
busy-loop
auto reconnect
destroy PTY
```

Host Open / I/O 后：

```text
PeerState = Active
```

Host Close：

```text
PeerState = Detached
Lifecycle remains Running
PTY remains
stable link remains
```

Host Reopen：

```text
communication resumes
```

---

## 7.17 Simulated Disconnect 验收

```text
Simulator disconnect
```

必须：

```text
close master

cancel pending response

cancel split

remove stable link if owned

Lifecycle = Disconnected
```

与普通：

```text
Host close slave
```

行为明确不同。

---

## 7.18 Delayed Response 验收

必须验证：

```text
Delay does not block Stop

Delay does not block Reconnect

Retry while pending is coalesced

No duplicate pending response

Reconnect cancels old generation

No old response appears after reconnect
```

---

## 7.19 Stable Link 安全验收

必须测试：

```text
directory mode

directory ownership

second instance lock failure

atomic replace

existing regular file refusal

cleanup only own link

custom path parent validation
```

禁止：

```text
blind unlink
blind overwrite
```

---

## 7.20 Fault 验收

Simulator 必须稳定产生：

```text
Business Failure

SDO NotReceived

SDO InProgress

SDO Error

SDO InvalidCommand

Wrong Transaction

Wrong Command

Bad CRC

Timeout

Drop

Split

Truncate

Disconnect
```

Delivery Fault 一次只能有一个。

---

## 7.21 GUI 验收

必须提供：

```text
Device

Radio

SDO

Protocol

Fault Injection
```

GUI 能够：

```text
观察 Lifecycle

观察 Peer State

观察 PTY

观察 Device State

修改 Simulator Internal State

查看完整 SDO 数据

查看 RX/TX

查看 Raw Frame

查看 Retry

查看 Fault Applied

执行 Fault Injection

Reset

Disconnect

Reconnect
```

---

## 7.22 Repository E2E 验收

E2E 必须可以直接：

```text
Simulator Runtime API
```

配置 Fault。

不依赖：

```text
GUI
Manual Operation
Unix Socket
Fault CLI
```

验证：

```text
Client::open

Radio

PIN

SDO

Timeout

Retry

CRC

Wrong Transaction

Wrong Command

DeviceRejected

Sdo*

FrameSyncLost

Disconnect

Reconnect
```

---

## 7.23 最终系统边界

完成后：

```text
                   docs/wrs/serial_protocol.md
                         /             \
                        /               \
                       ▼                 ▼
               WRS Host              Simulator
                  │                     │
         transmitter::Client      Independent Codec
                  │                     │
            serial::Port             Device
                  │                     │
                  │                    Fault
                  │                     │
                  ▼                     ▼
              PTY Slave ◄══════════► PTY Master
                                        │
                                        ▼
                              transmitter_runtime
                              /        |         \
                             /         |          \
                            ▼          ▼           ▼
                 transmitter_simulator Unit Test  Host E2E
                    /          \
                   ▼            ▼
                  GUI        Headless
```

Simulator 自身不存在：

```text
Simulator
↓
wrs
```

依赖。

---

## 7.24 最终冻结原则

WRS Transmitter Simulator V1.1 冻结以下原则：

1. 所有 Simulator 自身代码位于 `simulator/transmitter_simulator/`。
2. `simulator/transmitter_simulator/` 是完整独立 CMake 工程。
3. Simulator 不依赖 `wrs` 的源码、公共类型或 CMake Target。
4. namespace 统一为 `transmitter_simulator`。
5. 非 GUI Runtime Target 统一命名为 `transmitter_runtime`。
6. 最终可执行程序统一命名为 `transmitter_simulator`。
7. 独立测试 Target 统一命名为 `transmitter_simulator_tests`。
8. `serial_protocol.md` 是 Host 与 Simulator 的正式协议权威来源。
9. Host 与 Simulator 必须独立实现 Protocol Codec。
10. Simulator V1 实现当前协议定义的完整 SDO Object Dictionary，而不是只实现当前 Host 使用的 5 个对象。
11. Reserved SDO Read 按协议返回 `0`。
12. Firmware String SDO 按 4 Byte Segment 实现。
13. Unknown SDO、RO Write、Invalid Upgrade Value 使用明确的 Simulator Deterministic Failure Policy。
14. SDO Semantic Failure 使用 SDO Status，`Result` 保持 `0`，避免被普通 `DeviceRejected` 路径吞掉。
15. 所有 Request 必须按照完整 Validation Matrix 验证。
16. Transaction ID 为 0 的请求静默丢弃，不生成非法 Response。
17. CRC 错误和 Unknown Command 静默丢弃。
18. Radio / PIN 参数非法使用 `Result=0xFF` 且 State 不变。
19. Radio Write 必须完整验证后 Atomic Commit。
20. 433 MHz TX Power 不高于 10 dBm。
21. 915 MHz TX Power 不高于 20 dBm。
22. 开发效率优先，GUI 使用 Dear ImGui + GLFW。
23. 测试使用 Catch2。
24. PTY 直接使用 Linux `openpty()`。
25. Worker 使用 `poll()` + `eventfd()`，不能依赖不可唤醒的无限阻塞。
26. Host Peer Close 与 Simulator Disconnect 是两个不同状态。
27. `POLLHUP/EIO` 不得导致 Worker busy-loop。
28. Reconnect 必须清空 RX，并增加 `connection_generation`。
29. Delayed Response 必须使用 Scheduler，不得阻塞 `sleep_for()` Worker。
30. Reconnect / Stop 必须取消所有旧 Pending Response 和 Split Segment。
31. Exact Retry 在 Response Pending 期间进行 coalescing，不产生重复 Pending Response。
32. 不建立长期 Transaction Dedup Cache。
33. 不支持 Request Pipelining；Pending Response 期间的不同请求记录为异常并拒绝执行。
34. Fault Injection 是正式 V1 能力。
35. Delivery Fault 使用互斥 enum，Drop / Split / Truncate / Disconnect 不允许同时生效。
36. Disconnect After Request 是 Terminal Delivery Action，不再进入 TX。
37. Fault 只有在可产生 Response 的 Request 上消费。
38. Headless CLI 不承担 CI Fault Control Plane。
39. Repository-level Host E2E 直接链接 `transmitter_runtime` 配置 Fault。
40. Simulator Runtime 仍然不依赖 Host。
41. Stable PTY Path 默认使用 `/tmp/wrs-transmitter-simulator/tty`。
42. Stable Path Directory 必须验证 owner、type 和 permission。
43. Stable Link 使用同目录 temporary symlink + atomic `rename()`。
44. 使用 `flock()` 防止多个实例争用同一 Stable Path。
45. Shutdown 只删除仍然属于当前实例的 symlink。
46. `--pty-link` 不得静默覆盖普通文件或目录。
47. Dear ImGui 只负责控制和观察，不承载 Protocol / Device 业务逻辑。
48. `transmitter_runtime` 必须完全独立于 GUI。
49. `transmitter_simulator` 必须支持 Headless。
50. `BUILD_GUI=OFF` 时不得要求 OpenGL、GLFW 或 Dear ImGui，且仍构建 Headless executable 和全部 Runtime Tests。
51. Public Runtime 控制 API 成功返回时，命令已由 Worker 完成并对后续 Host I/O 可见；不得只表示已入队。
52. `start()` 成功返回时，PTY、stable link、lock 与 Worker 已就绪；失败必须回滚全部资源。
53. PTY Slave 的 raw / 115200 / 8N1 配置由 Host（及直接 Slave 测试）显式完成，不得依赖默认 terminal mode。
54. Simulator V1 是双频虚拟硬件，433 MHz 与 915 MHz 都受支持并分别执行功率限制。
55. Default 与 custom stable path 的 lock file 均使用 no-follow、owner/type 校验；临时 symlink 名必须不可预测且排他创建。
56. Headless 成功启动只输出一条稳定、flush 的 READY record；失败不输出 READY 且以非零退出。
57. V1 不提前引入 JSON、YAML、spdlog、CLI Framework、Plotting Framework 或普通 Serial Client Library。
58. 不创建 Generic Simulator Framework、Plugin System 或无真实复用对象的公共抽象。
59. Simulator 最重要的价值不是简单替代硬件，而是通过一个**独立、确定、可故障注入的设备端实现**支持无硬件开发并发现 Host 协议实现中的真实问题。
