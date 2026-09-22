你需要在当前仓库中实现完整的 **WRS Transmitter Simulator**。

目标不是实现一个简单 Fake Device，而是实现一个可以长期用于：

```text
开发
协议联调
故障注入
自动化测试
Host 协议交叉验证
```

的独立无线急停盒串口 Firmware 行为仿真器。

最终结果必须是正式工程代码，而不是 Demo。

---

# 1. 开始前分析仓库并确认协议基线

在修改代码前，先完整检查当前仓库，重点阅读：

```text
docs/wrs/serial_protocol.md

wrs/include/transmitter/
wrs/src/transmitter/
wrs/tests/transmitter_tests.cpp

wrs/include/serial/
wrs/src/serial/

CMakeLists.txt
AGENTS.md
```

首先理解：

* 当前 `transmitter::Client` 的 Public API；
* `Client::open()` 的设备探测过程；
* 当前 42 Byte Frame；
* Little Endian；
* CRC16-XMODEM；
* Transaction ID；
* LoRa 参数；
* GFSK 参数；
* PIN；
* SDO；
* Retry；
* Timeout；
* Partial Read；
* FrameSyncLost；
* Disconnect；
* 当前 CMake、C++、测试和命名风格。

在正式编码前，先输出一个简短实施计划，并列出你确认到的关键协议约束。

但必须严格区分：

```text
docs/wrs/serial_protocol.md
    = 正式协议权威

wrs/transmitter
    = 当前 Host 实现，仅用于理解 Host 行为
```

Simulator 不能为了兼容 Host 当前 Bug 而偏离协议。

如果发现：

```text
serial_protocol.md
```

与：

```text
wrs/transmitter
```

存在差异：

1. Simulator 按协议实现；
2. 不要复制 Host 的错误；
3. 最终报告中明确列出差异；
4. 如果 Host E2E 因此失败，也要明确指出根因。

特别检查：

```text
433 MHz TX power <= 10 dBm
915 MHz TX power <= 20 dBm
```

当前 Host 对 915 MHz 上限可能存在遗漏。

同时注意：

> Simulator V1 不再只是“当前 Host 使用功能的子集”，而是实现当前 `serial_protocol.md` 已定义的完整 USB Serial 设备端行为，特别是完整 SDO Object Dictionary。

---

# 2. 工程结构、命名、依赖和架构边界

创建：

```text
simulator/transmitter_simulator/
```

所有 Simulator 自身：

```text
源码
头文件
测试
CMake
third-party dependency declarations
README
```

都放在该目录中。

统一命名：

```text
工程目录
simulator/transmitter_simulator/

namespace
transmitter_simulator

include path
include/transmitter_simulator/

非 GUI Runtime Target
transmitter_runtime

最终 executable
transmitter_simulator

独立 test target
transmitter_simulator_tests
```

不要使用旧名称：

```text
simulator/transmitter/

transmitter_sim

transmitter_simulator_core

transmitter_simulator_runtime

wrs-transmitter-sim
```

推荐结构：

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

可以根据实际职责做小幅调整，但不要破坏：

```text
Protocol
Device
Transport
Fault
History
Simulator
UI
```

之间的边界。

不要创建无必要的：

```text
common/
utils/
base/
framework/
manager/
engine/
```

等泛化目录。

## 独立性要求

`simulator/transmitter_simulator` 必须完全独立于：

```text
wrs/
backend/
frontend/

ROS 2
FastDDS
DDS Wrapper
humanoid-driver
```

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

禁止添加：

```text
../../wrs/include
```

之类 include path。

必须独立支持：

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

## CMake Targets

建立：

```text
transmitter_runtime
transmitter_simulator
transmitter_simulator_tests
```

关系：

```text
                    transmitter_runtime
                     /               \
                    /                 \
                   ▼                   ▼
      transmitter_simulator   transmitter_simulator_tests
```

`transmitter_runtime` 包含：

```text
protocol
device
transport
fault
history
simulator
```

且不得依赖：

```text
Dear ImGui
GLFW
OpenGL
Catch2
wrs
```

`transmitter_simulator`：

```text
main
app
ui
+
transmitter_runtime
+
Dear ImGui
+
GLFW
+
OpenGL
```

`transmitter_simulator_tests`：

```text
transmitter_runtime
+
Catch2
```

仓库级 Host E2E 可以另外建立：

```text
transmitter_simulator_e2e_tests
```

同时链接：

```text
wrs_transmitter
transmitter_runtime
```

但依赖方向必须始终是：

```text
E2E Test
├── Host
└── Simulator Runtime
```

禁止：

```text
transmitter_runtime
→
wrs
```

## Third Party

只允许：

```text
Dear ImGui
GLFW
Catch2
```

Dear ImGui 使用官方核心源码及：

```text
imgui_impl_glfw
imgui_impl_opengl3
```

OpenGL 使用系统：

```cmake
find_package(OpenGL REQUIRED)
```

不要引入：

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

除非实际实现过程中出现无法规避的明确需求，并在最终报告解释原因。

PTY 直接使用 Linux：

```cpp
openpty()
poll()
eventfd()
read()
write()
close()
flock()
symlink()
rename()
```

Third-party 版本固定到明确 Tag 或 Commit，不跟随 `main/master/latest`。
使用 CMake `FetchContent` 在构建目录获取依赖；不得将 Dear ImGui、GLFW 或 Catch2 源码提交到仓库。

---

# 3. 独立实现完整 Protocol 和 Device Model

Simulator 必须自己实现协议。

禁止复用 Host：

```text
Frame
CRC
Endian
LoRaParamFrame
GfskParamFrame
PinFrame
SdoFrame
Validation
```

也不要复制 Host 实现后简单换 namespace。

## Frame

固定：

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
reinterpret_cast<WireStruct*>()
```

所有 Wire Field 都使用明确 Byte Offset。

独立实现：

```cpp
read_u16_le()
read_u32_le()

write_u16_le()
write_u32_le()
```

CRC：

```text
CRC16-XMODEM

Polynomial = 0x1021
Init       = 0x0000
RefIn      = false
RefOut     = false
XorOut     = 0x0000
```

覆盖：

```text
Byte0 ~ Byte39
```

存储：

```text
Byte40 = CRC low
Byte41 = CRC high
```

CRC Test 使用标准 Known Vector，例如：

```text
"123456789"
```

不要调用 Host CRC 生成 Expected Value。

## Stream Framing

PTY 是 Byte Stream。

必须支持：

```text
read 5
read 12
read 25
→ 1 Frame
```

也支持：

```text
read 84
→ 2 Frames
```

维护明确有界 RX Buffer。

协议没有：

```text
SOF
Length
Escape
```

不要自行实现 sliding-window CRC resynchronization。

如果 Byte Stream 因：

```text
lost byte
inserted byte
truncated frame
```

失去 42 Byte 边界，则当前正式恢复方式是：

```text
Disconnect
→
Reconnect
```

Reconnect 必须清空 RX Buffer。

## Device Model

自己定义明确类型，例如：

```cpp
struct DeviceIdentity final {
    std::uint32_t product_code{};
    std::uint32_t version_number{};
    std::uint32_t serial_number{};
};

struct Pin final {
    std::array<std::uint8_t, 6> digits{};
};

struct FirmwareMetadata final {
    std::array<std::uint8_t, 40> app_firmware_version{};
    std::array<std::uint8_t, 40> bootloader_firmware_version{};

    std::array<std::uint8_t, 40> app_branch_name{};
    std::array<std::uint8_t, 40> app_tag_sha1_id{};

    std::array<std::uint8_t, 40> boot_branch_name{};
    std::array<std::uint8_t, 40> boot_tag_sha1_id{};
};

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

不要复用任何：

```text
transmitter::*
```

类型。

## Factory Defaults

PIN：

```text
123456
```

实际 PIN 不允许是：

```text
000000
```

因为它已经作为 PIN Read sentinel。

LoRa 默认：

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

GFSK 默认：

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

Identity 和 Firmware Metadata 使用明确的 Simulator Test Data，不冒充真实生产设备。

## 完整 SDO Object Dictionary

实现完整协议字典，不再只实现 5 个对象：

```text
0x001 Product Code                 RO
0x002 Version Number               RO
0x003 Serial Number                RO

0x004~0x007 Reserved               RO / Read 0

0x008~0x011 App Firmware Version   RO
0x012~0x01B Bootloader Firmware    RO
0x01C~0x025 App Branch Name        RO
0x026~0x02F App Tag SHA1           RO
0x030~0x039 Boot Branch Name       RO
0x03A~0x043 Boot Tag SHA1          RO

0x044~0x0FF Reserved               RO / Read 0

0x102 Battery                      RO

0x202 Upgrade Request              RW
```

字符串对象：

```text
40 Byte
每 Object 返回 4 Byte
```

例如：

```text
offset = (index - base_index) * 4
```

不要使用 host-native `uint32_t` reinterpret cast。

Upgrade Request 有效写值：

```text
0x454E
```

其他值失败。

Unknown Object：

```text
SDO Status = 0xE
Result = 0
```

Reserved Read：

```text
Status = 0x4
Data   = 0
Result = 0
```

Write RO：

```text
Status = 0x8
Data   = 0
Result = 0
```

Invalid `0x202` value：

```text
Status = 0x8
Data   = 0
Result = 0
```

不要对这些 SDO Semantic Failure 返回：

```text
Result = 0xFF
```

否则 Host 会先进入普通 `DeviceRejected`，无法覆盖 `SdoError` / `SdoInvalidCommand`。

---

# 4. 请求校验、PTY Runtime 和生命周期必须按明确规则实现

不要只实现一个泛化的：

```text
Request Validation
```

必须落实完整 Matrix。

## 通用 Request

所有有效请求：

```text
CRC valid
Transaction ID != 0
Byte39 == 0
```

CRC Error：

```text
log
discard
no response
state unchanged
next-response fault not consumed
```

Unknown Command：

```text
log
no response
state unchanged
fault not consumed
```

Transaction ID == 0：

```text
log
no response
state unchanged
fault not consumed
```

不要生成 Transaction ID 为 0 的非法 Response。

## `0x04` Radio Read

有效条件：

```text
Command            = 0x04
Transaction        != 0
Object Index       = 0x0000
Object Data        = 0

Parameter Flags:
bit15..14          = 00 LoRa / 01 GFSK
bit13..0           = 0

Byte13~38          = 0
Byte39             = 0
CRC                valid
```

这里 Read Request 的 Parameter Flags 只负责选择：

```text
LoRa
GFSK
```

其余 Flag 必须为 0。

成功：

```text
0x84

Transaction ID = echo

Object Index = 0x4000

Object Data = 0

Result = 0

返回当前完整参数
```

如果 Command / CRC / Transaction 已合法，但 Radio Read 结构字段不合法：

```text
0x84
Result = 0xFF
state unchanged
```

Response 参数部分使用当前合法 Device State。

## `0x04` SDO Read

有效：

```text
Command            = 0x04
Transaction        != 0

Object Index
high nibble        = 0
low 12 bits        != 0

Object Data        = 0
Byte11~38          = 0
Byte39             = 0
CRC                valid
```

成功：

```text
0x84
Status = 0x4
Object address = echo
Object Data = value
Result = 0
```

SDO Invalid Operation / Structural Failure：

```text
Status = 0xE
Result = 0
state unchanged
```

## `0x05` Radio Write

有效：

```text
Command            = 0x05
Transaction        != 0
Object Index       = 0
Object Data        = 0
Flags              valid
Parameters         valid
Reserved           = 0
Byte39             = 0
CRC                valid
```

必须：

```text
Decode
↓
Temporary Config
↓
Validate Complete Config
↓
Atomic Commit
```

任何字段非法：

```text
0x85
Object Index = 0x6000
Result = 0xFF
state unchanged
```

成功：

```text
0x85
Object Index = 0x6000
Object Data = 0
Result = 0
```

## `0x05` SDO Write

有效：

```text
Command            = 0x05
Transaction        != 0

Object Index
high nibble        = 1
low 12 bits        != 0

Reserved           = 0
Byte39             = 0
CRC                valid
```

成功：

```text
0x85
Status = 0x6
Object address = echo
Object Data = 0
Result = 0
```

Unknown Object：

```text
Status = 0xE
Result = 0
```

RO Object：

```text
Status = 0x8
Result = 0
```

Invalid Upgrade Value：

```text
Status = 0x8
Result = 0
```

## `0x07` PIN

有效：

```text
Command          = 0x07

Byte1~6
six ASCII digits

Byte7~11         = 0

Transaction
Byte12~15        != 0

Byte16~38        = 0

Byte39           = 0

CRC              valid
```

如果：

```text
PIN == "000000"
```

执行 Read。

否则：

```text
PIN Write
```

成功：

```text
0x87
PIN = current / newly written PIN
Transaction = echo
Result = 0
```

结构非法：

```text
0x87
PIN = current stored PIN
Transaction = echo
Result = 0xFF
state unchanged
```

## Radio Validation

Flags：

```text
bit15..14
00 LoRa
01 GFSK
10/11 invalid

bit13..6 = 0

bit5
single / hopping

bit4
当前必须 0

bit3
heartbeat

bit2
wireless estop

bit1
PHY CRC

bit0
band
```

TX Power：

```text
433 MHz <= 10 dBm
915 MHz <= 20 dBm
```

必须测试：

```text
433 / 10 → valid
433 / 11 → invalid

915 / 20 → valid
915 / 21 → invalid
915 / 22 → invalid
```

Common：

```text
Payload = 12

RSSI Raw = 10~148

Heartbeat Interval = 200~10000

Heartbeat Loss = 1~255
```

LoRa：

```text
Bandwidth       0~2
SF              5~12
Coding Rate     0~6
Header          0~1
Preamble        10~50

SF5/SF6:
Preamble exactly 12

Sync:
each byte low nibble == 4
```

GFSK：

```text
Bandwidth       0~2
Bit Rate        600~150000
Deviation       600~300000

4 * deviation >= bitrate

Pulse Shaping:
0x00
0x08
0x09
0x0A
0x0B

Preamble:
16~255
```

使用足够宽整数验证：

```text
4 * deviation
```

避免 overflow。

## `0x01 / 0x02 / 0x03`

当前只：

```text
Recognize
Log
Mark Unsupported
No Response
No State Mutation
```

不要实现：

```text
Binding State Machine
Kbind
Find State
```

---

## PTY Runtime

直接使用：

```cpp
openpty()
```

Simulator 持有 Master。

获取 Slave Path 后，Simulator 自己关闭 `openpty()` 返回的 Slave FD。

Host 自己打开：

```text
/dev/pts/N
```

Stable Path 默认：

```text
/tmp/wrs-transmitter-simulator/tty
```

### Stable Directory

默认目录：

```text
/tmp/wrs-transmitter-simulator/
```

如果不存在：

```text
mkdir
mode 0700
```

如果存在：

* 必须是 Directory；
* 不能是 Symlink；
* Owner 必须是当前 UID；
* 权限不合适时，如果当前用户拥有目录，可修正为 `0700`；
* 否则拒绝启动。

禁止：

```text
rm -rf
```

式清理。

### Instance Lock

使用：

```text
/tmp/wrs-transmitter-simulator/.lock
```

通过：

```cpp
flock(LOCK_EX | LOCK_NB)
```

保证一个 Stable Path 同时只有一个 Simulator Owner。

第二个实例：

```text
start()
→ Busy
```

### Stable Link 原子更新

使用：

```text
temporary symlink
+
rename()
```

例如：

```text
.tty.<pid>.tmp
→
tty
```

existing path 如果是：

```text
regular file
directory
socket
fifo
```

则拒绝覆盖。

### Cleanup

Shutdown 时：

1. `lstat(stable_path)`；
2. 必须仍然是 Symlink；
3. `readlink()`；
4. Target 必须仍然等于本实例的 `slave_path`；
5. 才 `unlink()`。

不要无条件删除 Stable Path。

`--pty-link <path>` 同样必须执行：

```text
parent exists
parent writable
non-symlink existing object rejected
same-directory atomic rename
instance lock
owned-link-only cleanup
```

---

## Simulator Lifecycle 与 Peer State

分开建模：

```cpp
enum class LifecycleState {
    Stopped,
    Running,
    Disconnected,
};

enum class PeerState {
    Detached,
    Active,
};
```

`PeerState` 是：

> Observable peer state

而不是完美精确的 Slave-open 状态。

Host 关闭 Slave 时，PTY Master 可能得到：

```text
POLLHUP
EIO
```

这不等于：

```text
Simulator Disconnect
```

此时：

```text
PeerState = Detached

Lifecycle remains Running

keep master

keep stable path

do not recreate PTY
```

Host 再次打开同一个 Slave 后，新的有效 I/O 将：

```text
PeerState = Active
```

Detached 时必须避免：

```text
POLLHUP busy-loop
```

可使用受控 PTY probe interval，例如：

```text
100 ms
```

但仍然必须立即响应：

```text
eventfd
Stop
Disconnect
Reconnect
Control API
```

---

## Worker Thread

只使用两个主要线程：

```text
Main Thread
├── GLFW
├── OpenGL
└── Dear ImGui

Worker Thread
├── poll
├── eventfd
├── PTY RX
├── Protocol
├── Device
├── Fault
├── Pending Response Scheduler
└── PTY TX
```

使用：

```cpp
eventfd()
```

作为 worker wakeup。

Worker `poll()` 至少监听：

```text
master fd
wake fd
```

并根据：

```text
pending response due time
peer probe deadline
```

计算 Poll Timeout。

不要使用：

```cpp
sleep_for(response_delay)
```

阻塞 Worker。

---

## Stop / Reconnect

Stop：

```text
set stop requested
↓
wake worker
↓
cancel pending response
↓
cancel split segment
↓
clear RX
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

`stop()` 必须安全、幂等。

Reconnect：

```text
connection_generation++
↓
cancel pending response
↓
cancel split
↓
clear RX
↓
close old PTY
↓
cleanup old stable link
↓
create new PTY
↓
replace stable link
↓
PeerState = Detached
↓
Lifecycle = Running
```

Host 需要重新 `Client::open()`。

---

## Pending Response 与 Retry

Runtime 维护：

```cpp
std::uint64_t connection_generation;
std::uint64_t request_sequence;
```

Pending Response 至少记录：

```text
connection generation
request sequence
request fingerprint
due time
response frame
delivery fault
```

Reconnect 后：

```text
old generation response
→ discard
```

绝不能发到新 PTY。

V1 不支持 Host Request Pipelining。

同一连接最多：

```text
one pending normal response
```

如果 Pending Response 尚未发送时收到：

```text
same command
same transaction
same verified Byte0~39 body
```

则视为：

```text
Retry
```

处理：

```text
record Retry

do not execute Device Operation again

do not create second pending response

keep original pending response
```

这是：

```text
pending-response coalescing
```

不是长期 Transaction Dedup Cache。

如果 Response 已经：

```text
sent
dropped
truncated
disconnected
```

之后相同 Request 再到达，则正常重新执行。

因此：

```text
Drop
↓
Host Retry
↓
Device Operation executes again
```

如果 Pending Response 期间来了不同请求：

```text
UnexpectedPipelinedRequest
```

V1：

```text
log
no execution
no response
```

不要增加复杂 request queue。

---

# 5. Fault Injection、GUI 与 Headless

Fault Injection 是正式功能。

默认：

```text
all faults disabled
```

Fault 分为：

```text
Semantic
Correlation
Integrity
Timing
Delivery
```

## Semantic Fault

```text
Fail Next Business Response
```

只作用于：

```text
Radio
PIN
```

下一个可响应业务操作。

语义：

```text
simulate device rejection

do not commit Device State

Result = 0xFF
```

SDO 使用独立 SDO Fault。

## SDO Fault

```cpp
enum class SdoFault : std::uint8_t {
    None,
    NotReceived,
    InProgress,
    Error,
    InvalidCommand,
};
```

对应：

```text
0x0
0xB
0x8
0xE
```

Result：

```text
0
```

如果 Fault 已 armed，但下一个 Request 不是 SDO：

```text
keep fault armed
```

直到下一个 SDO Response。

## Correlation Fault

```text
Wrong Next Transaction
Wrong Next Command
```

## Integrity Fault

```text
Corrupt Next CRC
```

顺序：

```text
build normal response
↓
calculate CRC
↓
corrupt CRC
```

## Timing Fault

```text
Response Delay
```

持续配置，不是 one-shot。

通过 Pending Response Scheduler 实现，不要 `sleep_for()` 阻塞 Worker。

## Delivery Fault

必须使用互斥 enum：

```cpp
enum class DeliveryFault : std::uint8_t {
    None,
    Drop,
    Split,
    Truncate,
    Disconnect,
};
```

不要使用多个 bool 允许：

```text
drop + truncate
split + disconnect
```

等未定义组合。

### Drop

```text
Device Operation executed
↓
response dropped
```

Host Retry 后正常重新处理请求。

### Split

两段发送即可。

配置：

```text
split_after_bytes
split_delay
```

要求：

```text
1 <= split_after_bytes < 42
```

第一段发送后，第二段通过 Scheduler，而不是阻塞 Worker。

Stop / Disconnect / Reconnect 取消第二段。

### Truncate

```text
1 <= truncate_after_bytes < 42
```

只发送前 N Byte。

不自动关闭 PTY。

预期 Host：

```text
partial response
↓
timeout
↓
FrameSyncLost
↓
close
```

### Disconnect

`Disconnect After Next Request`：

```text
valid request
↓
normal device operation
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

这是：

> Terminal Delivery Action

不要执行最终 TX。

另外支持：

```text
Disconnect Now
```

作为立即 Control API：

```text
cancel pending
cancel split
connection_generation++
close PTY
cleanup stable link
Lifecycle = Disconnected
```

## Fault Consumption

Next-response Fault 只有当 Request 已通过：

```text
CRC
Command
Transaction
```

且 Runtime 已准备产生 Response 时才消费。

以下请求不消费：

```text
bad CRC
unknown command
transaction == 0
unsupported 0x01/2/3
```

## Fault Processing Order

固定：

```text
Receive
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
Delivery:
    Normal
    Drop
    Split
    Truncate
    Disconnect
```

推荐：

```cpp
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

非法 FaultConfig 必须返回：

```text
InvalidArgument
```

---

## Runtime Public Control API

不要强制把：

```text
std::variant<ControlCommand...>
```

暴露为 Public API。

更推荐明确方法：

```cpp
Simulator::set_identity()
Simulator::set_firmware_metadata()

Simulator::set_pin()
Simulator::set_battery()

Simulator::set_lora()
Simulator::set_gfsk()

Simulator::reset_device()

Simulator::set_fault_config()

Simulator::disconnect()
Simulator::reconnect()

Simulator::clear_history()

Simulator::snapshot()
```

内部 Worker Ownership 可以使用私有 Command Queue。

外部保持类型明确。

---

## GUI

最终程序：

```bash
./transmitter_simulator
```

五个主页面：

```text
Device
Radio
SDO
Protocol
Fault Injection
```

### Device

显示：

```text
Lifecycle State
Peer State

PTY Slave
Stable Path
Connection Generation

RX/TX Frames
RX/TX Bytes

Product Code
Version
Serial

PIN
Battery
Upgrade Request
```

操作：

```text
Apply
Reset
Disconnect
Reconnect
```

必须清晰区分：

```text
Peer Detached
```

和：

```text
Simulator Disconnected
```

### Radio

LoRa / GFSK 分开展示。

完整显示和编辑协议字段。

UI Apply 和 Serial Write 必须调用同一个：

```text
validate_lora()
validate_gfsk()
```

不要出现 UI 和 Protocol 两套业务校验。

### SDO

覆盖完整 Object Dictionary。

按组展示：

```text
Identity

Firmware Metadata

Runtime

Reserved
```

不要把几百个 Reserved Object 逐行显示。

可显示：

```text
0x044~0x0FF
RO
read = 0
```

UI Editability 与 Protocol Access 分开。

例如：

```text
Battery
Host → RO
Simulator UI → editable
```

### Protocol

至少显示：

```text
Time

Direction

Connection Generation
Request Sequence

Command
Transaction

Object
Result / SDO Status

CRC

Note
```

选中记录展示：

```text
Decoded fields
Applied Fault
Raw 42 Byte
```

History 固定容量：

```text
2000
```

记录：

```text
RX
TX

Invalid Frame
Invalid Transaction
Invalid Request

Unsupported Command

Retry
Unexpected Pipelined Request

Fault Applied

Peer Detached
Peer Active

Disconnect
Reconnect
```

支持：

```text
Auto Scroll
Pause View
Clear
```

Pause 只暂停 GUI 显示，不暂停 Runtime。

### Fault Injection

集中配置所有 Fault。

必须能看到当前 armed one-shot 状态以及被消费后的自动 Reset。

---

## Headless

支持：

```bash
./transmitter_simulator --headless
```

以及：

```bash
./transmitter_simulator \
    --headless \
    --pty-link /tmp/wrs-transmitter-simulator/tty
```

Headless 不初始化：

```text
GLFW
OpenGL
Dear ImGui
```

但完整运行：

```text
transmitter_runtime
```

CLI V1 只需要：

```text
--headless
--pty-link <path>
--help
--version
```

不要增加：

```text
--fault-*
--battery
--pin
--radio-*
```

测试 Fault Control 不通过 CLI。

---

# 6. 自动测试、E2E 和代码质量要求

测试必须与实现同步完成，不允许最后补。

## Protocol Tests

覆盖：

```text
CRC Known Vector

Endian

LoRa Read Decode / Response Encode
LoRa Write Decode / Response Encode

GFSK Read / Write

PIN Read / Write

SDO Read / Write

Firmware String Segmentation

Reserved SDO

Unknown SDO

RO SDO Write

Upgrade Request

Flags
Object Index
Result
Reserved Fields
```

Golden Frame 使用 literal byte array。

不得使用 Host Codec 生成 Expected Data。

## Request Validation Tests

按完整 Matrix 测：

```text
Transaction == 0

Byte39 != 0

Radio Read Object Data != 0

Radio Read parameter bytes != 0

Invalid Radio Type

Reserved flag != 0

Group Mode != 0

SDO Read wrong operation nibble

SDO Write wrong operation nibble

SDO Read Object Data != 0

SDO reserved byte != 0

PIN reserved byte != 0

PIN non-digit
```

每个测试必须同时验证：

```text
response or no response

Result / SDO Status

State changed or unchanged

Fault consumed or not consumed
```

## Device Tests

覆盖：

```text
Factory Defaults

Identity

Firmware Metadata

PIN

LoRa

GFSK

Battery

Upgrade

Reserved SDO

Unknown SDO

Reset
```

Radio Boundary：

```text
433 / 10 success
433 / 11 reject

915 / 20 success
915 / 21 reject

Payload != 12

RSSI < 10
RSSI > 148

Heartbeat < 200
Heartbeat > 10000

Heartbeat Loss = 0

SF5/SF6 wrong preamble

Invalid LoRa Sync

Bitrate < 600
Bitrate > 150000

Deviation < 600
Deviation > 300000

4 * deviation < bitrate

Invalid Pulse Shaping
```

## Transport Tests

使用真实 Linux PTY。

覆盖：

```text
openpty

slave path

stable directory

ownership

permission

lock acquisition

second instance rejection

atomic symlink replacement

existing regular file rejection

read/write

partial I/O

POLLHUP

EIO

Host close

Peer Detached

Host reopen

Peer Active

Disconnect

Reconnect

RX clear

stable link cleanup
```

必须证明：

```text
Host close != Simulator Disconnect
```

## Worker / Scheduler Tests

覆盖：

```text
Stop while poll blocked

Stop while response delayed

Stop while split segment pending

Disconnect while delayed

Reconnect while delayed

Reconnect cancels old generation

No stale response after reconnect

Repeated stop

Repeated disconnect/reconnect
```

## Retry Tests

必须覆盖：

### Retry while pending

```text
Request #1
↓
Response delayed
↓
same Request retry
↓
record Retry
↓
Device Operation not repeated
↓
still one pending response
```

### Drop then retry

```text
Request #1
↓
Device Operation
↓
Drop
↓
Host Retry
↓
Device Operation executes again
↓
normal response
```

### Unexpected Pipeline

```text
Request #1 pending
↓
different request arrives
↓
record UnexpectedPipelinedRequest
↓
no execution
↓
no response
```

## Fault Tests

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

验证 One-shot。

例如：

```text
first applicable request
→ fault applied

second
→ normal
```

SDO Fault 应验证：

```text
armed
↓
Radio request
↓
still armed
↓
SDO request
↓
consumed
```

## Simulator Integration Tests

不初始化 GUI。

直接通过 PTY 测：

```text
bytes
→
Runtime
→
bytes
```

覆盖：

```text
complete request

partial request

multiple frames in one read

Device State transition

History

Fault

Reconnect
```

---

## Repository-level Host E2E

不要通过：

```text
GUI
CLI fault flags
Unix socket
JSON scenario
```

控制 Fault。

E2E 直接链接：

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

client.open(
    simulator.snapshot().stable_path,
    ...);
```

正常路径至少覆盖：

```text
Client::open

LoRa Read
LoRa Write
LoRa Read Back
LoRa Restore

GFSK Read
GFSK Write
GFSK Read Back
GFSK Restore

PIN Read
PIN Write
PIN Read Back

Product Code
Version
Serial
Battery

Upgrade Request
```

异常路径：

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

完整 SDO 字典可以由 Simulator 自身 Unit / Integration Test 覆盖，即使当前 Host API 尚未暴露所有对象。

现有：

```text
wrs/tests/transmitter_tests.cpp
```

继续保留。

测试层次应形成：

```text
Host Component Test

Simulator Unit Test

Host ↔ Independent Simulator E2E

Real Hardware Acceptance
```

---

## C++ 代码质量

这是正式工程代码，不是测试脚本。

要求：

```text
C++17
```

必须遵守：

* RAII；
* 明确 ownership；
* Single Responsibility；
* 窄 Public API；
* const correctness；
* 重要返回值使用 `[[nodiscard]]`；
* 合理使用 `noexcept`；
* 不使用 owning raw pointer；
* 不使用 detached thread；
* 不使用 global mutable state；
* 不使用 packed wire struct；
* 普通协议错误不依赖 exception；
* Protocol Offset 集中定义；
* 不散布 magic number；
* 不复制明显重复实现；
* FD 生命周期明确；
* eventfd 生命周期明确；
* lock fd 生命周期明确；
* malformed frame 不得 crash；
* History 有固定容量；
* RX Buffer 有明确上限；
* Pending Response 生命周期明确；
* Reconnect 后不得有 stale response；
* Shutdown 不得泄漏 thread/fd/link；
* Stable Link Cleanup 不得删除其他进程资源。

## 不要过度抽象

不要为了未来可能复用而设计：

```text
ITransport
IProtocol

GenericFrame
GenericDevice

SimulatorFramework
DeviceFramework

PluginSystem
PropertySystem

ManagerFactory
```

当前没有真实第二个消费者。

同样不要大量使用：

```text
std::any
string-based dispatch
generic property map
```

能用明确类型时使用明确类型。

Public API 不必暴露 `std::variant<ControlCommand...>`。

## 命名

避免：

```text
data
info
ctx
mgr
helper
util
process()
do_work()
```

这种模糊命名。

优先：

```text
parse_radio_read_request

validate_lora
validate_gfsk

read_sdo_object
write_sdo_object

schedule_response

cancel_pending_response

replace_stable_link

mark_peer_detached

record_received_frame
```

## Header 注释

以下：

```text
include/transmitter_simulator/*.hpp
```

使用 Doxygen 风格。

重点说明：

```text
responsibility
ownership
thread safety
lifecycle
error semantics
protocol invariants
```

不要写重复函数名称的低价值注释。

## Error Handling

不要依赖：

```text
hardware::Result
```

Simulator 可以提供局部：

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

和轻量：

```text
Result<T>
```

但不要复制整个 `hardware::Result`。

也不要将其发展为仓库级公共库。

## CMake

使用 Target-based CMake。

禁止：

```cmake
include_directories(...)
```

全局污染。

禁止全局 Compiler Flags。

每个 Target 明确：

```text
include
compile feature
link dependency
```

要求：

```text
BUILD_TESTING controls Catch2

Runtime does not link GUI

Tests do not link GUI

Simulator does not link wrs

third-party versions fixed

build tree does not pollute source tree
```

---

# 7. 实施阶段、实际验证和最终报告

整个实现最多分四阶段。

不要一开始先做 GUI。

## Phase 1 — Protocol + Device

完成：

```text
simulator/transmitter_simulator/

CMake

third_party integration

Error / Result

Frame
Endian
CRC

Request Classification

Request Validation Matrix

LoRa
GFSK

PIN

Complete SDO Dictionary

Firmware Metadata Segmentation

DeviceState

Factory Defaults

SDO Failure Semantics
```

同步完成：

```text
Protocol Tests
Request Validation Tests
Device Tests
```

先保证独立协议实现可靠。

## Phase 2 — PTY + transmitter_runtime

完成：

```text
PtyTransport

openpty

stable directory

flock

stable symlink

atomic rename

eventfd

poll loop

Peer State

POLLHUP/EIO handling

RX Stream Buffer

Request Dispatcher

PendingResponse Scheduler

connection_generation

request_sequence

Retry coalescing

History

Stop

Disconnect

Reconnect

Headless Runtime
```

同步完成：

```text
Transport Tests
Lifecycle Tests
Worker Tests
Retry / Delay Tests
```

此阶段结束时，在没有 GUI 的情况下 Runtime 已经完整可用。

## Phase 3 — Fault + Host E2E

实现完整：

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

完成：

```text
Fault Tests

Simulator Integration Tests

Host ↔ Simulator E2E
```

仓库级 E2E 直接链接：

```text
wrs_transmitter
+
transmitter_runtime
```

不要建设测试 IPC。

## Phase 4 — Dear ImGui

最后实现：

```text
Device
Radio
SDO
Protocol
Fault Injection
```

同时完成：

```text
GUI Lifecycle

Headless CLI

README

Build Instructions

Developer Usage

Third-party license information

Final cleanup
```

---

完成后必须实际执行：

```bash
cmake \
  -S simulator/transmitter_simulator \
  -B build/transmitter-simulator \
  -DBUILD_TESTING=ON

cmake --build build/transmitter-simulator -j

ctest \
  --test-dir build/transmitter-simulator \
  --output-on-failure
```

检查生产依赖：

```bash
grep -R \
  -E '#include <(serial|transmitter|hardware)/' \
  simulator/transmitter_simulator
```

Simulator 自身结果必须没有 `wrs` include。

还需要实际运行 Headless：

```bash
./build/transmitter-simulator/transmitter_simulator \
    --headless
```

验证：

```text
PTY created

stable link created

worker runs

SIGINT / SIGTERM

worker wakes

pending response cancelled

PTY closes

owned stable link removed

lock released

worker joined
```

不得留下：

```text
detached thread
stale fd
stale owned symlink
```

如果图形环境可用，再实际运行 GUI。

---

最终报告必须包含：

### 实现摘要

说明实现了：

```text
Protocol
Device
PTY
Scheduler
Fault
History
GUI
Headless
Tests
E2E
```

中的哪些部分。

### 重要文件变化

列出关键新增/修改文件。

### 架构

解释：

```text
transmitter_runtime

transmitter_simulator

transmitter_simulator_tests

repository-level E2E
```

之间的关系。

### 验证结果

给出实际执行的：

```text
configure

build

ctest

headless smoke test

Host E2E

GUI smoke test
```

结果。

不要只说“应该能通过”。

### 协议差异

逐项列出：

```text
serial_protocol.md
vs
wrs/transmitter
```

发现的差异。

特别说明是否发现：

```text
915 MHz TX power
```

等 Host Protocol Validation 问题。

### 未完成内容

如果某部分无法完成：

* 明确指出；
* 说明原因；
* 不要使用临时 Hack 假装完成；
* 不要因为当前 Host 不支持，就删除 Simulator 协议完整性要求。

---

最终结果应让：

```text
simulator/transmitter_simulator/
```

成为一个：

> **完全独立于 `wrs`、可独立构建和测试、拥有完整协议设备模型、真实 Linux PTY 边界、确定性 Fault Injection、可靠 Runtime 生命周期、GUI 和 Headless 模式的无线急停盒 Firmware 行为 Simulator。**

最终优先级：

```text
correctness
protocol fidelity
determinism
testability
clarity
maintainability
development efficiency
```

避免：

```text
为了测试通过复制 Host Bug

过度抽象

无必要依赖

重复实现

忙等

不可唤醒线程

stale response

不安全 /tmp 操作

测试专用 IPC

临时 workaround
```

如果设计文档与实际仓库结构存在小幅工程差异，可以在不破坏上述架构和协议约束的前提下做合理调整；如果涉及协议语义变化，则不要自行猜测，应按 `serial_protocol.md` 和本提示中的确定性规则实现，并在最终报告明确说明。
