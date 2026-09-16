# 无线急停调试工具需求与总体设计说明书

> 文档状态：设计基线草案  
> 适用产品：Wireless Remote Stop  
> 适用机器人：H20 DVT2、H23  
> 编写目的：统一无线急停系统、无线急停调试工具与机器人侧 Driver 的需求理解、系统边界、通信链路和总体设计，作为后续 ROS 2 接口设计、软件详细设计、UI 设计与实现的共同基线。

---

## 参考文档

| 编号 | 文档 |
|---|---|
| S1 | 《无线急停系统_产品需求说明书_(PRD) V1.0》 |
| S2 | 《急停配置工具迭代1需求 V1.7.0》 |
| S3 | 《APP-Sub1G无线急停通讯协议 V1.0》 |
| S4 | 《H20-DVT2、H23_CDU通信协议 V1.1》 |

本文不重新定义底层无线协议、CDU 内部协议或产品安全逻辑。涉及具体字节布局、CRC、无线认证、硬件电气特性、CDU 内部 RS485 通信等内容时，以对应源文档为准。

---

# 1. 无线急停系统概述

## 1.1 系统定位与目标

Wireless Remote Stop 是与机器人配套使用的无线急停系统，主要用于机器人集群作业、人机协同、研发调试及生产测试场景中的紧急停止与安全控制。

系统的核心目标包括：

- 用户通过无线急停盒向机器人发出急停、暂停等控制指令；
- 机器人持续监测无线链路状态；
- 无线链路异常或中断时，机器人进入规定的安全停止状态；
- 无线急停盒与机器人接收端通过绑定关系保证控制对象正确；
- 发射端与接收端使用一致的无线通信参数；
- 配置、绑定与诊断过程由独立调试工具完成，但调试工具不参与机器人运行时的无线急停安全链路。

根据产品需求，Wireless Remote Stop 由用户侧发射器和机器人侧接收器两部分组成。用户操作发射器产生控制信号，机器人内部接收器负责接收无线信号，并由机器人系统执行相应停止动作。

当前产品基线主要面向 433 MHz 无线通信；协议层同时具备 433 MHz / 915 MHz、LoRa / GFSK 等扩展能力。产品与协议能力必须区分：调试工具 V1 应首先满足当前产品需求，而不应仅因协议支持某能力就默认将其全部暴露给用户。

## 1.2 系统组成与角色职责

从完整系统实现角度，无线急停由以下逻辑角色组成：

| 组件 | 主要职责 |
|---|---|
| 无线急停盒 | 用户输入、周期心跳、急停、暂停、寻机、Sub-1G 数据发送、保存无线参数与绑定信息 |
| Sub_1G 接收板 | 接收无线数据、校验设备身份与认证信息、链路监测、保存无线配置和绑定信息、上报运行状态 |
| CDU | 机器人内部控制单元，负责 RCU 与下属从设备之间的数据转发和管理 |
| Robot Driver | Linux / ROS 2 侧无线急停设备访问入口，对上提供业务接口，对下封装 CDU/Sub_1G 协议 |
| Robot Control / Safety | 根据急停、暂停、无线链路中断等状态执行机器人控制与安全策略 |
| 无线急停调试工具 | 用于连接设备、配置参数、同步参数、出厂绑定和必要诊断 |

系统总体关系如下：

```text
                           配置 / 调试链路
                ┌────────────────────────────────┐
                │                                │
                │        无线急停调试工具          │
                │                                │
                └────────┬──────────────┬────────┘
                         │              │
                    USB Serial        ROS 2
                         │              │
                         ▼              ▼
                  ┌────────────┐  ┌──────────────┐
                  │ 无线急停盒  │  │ Robot Driver │
                  └─────┬──────┘  └──────┬───────┘
                        │                │
                        │                │ SPI
                        │                ▼
                        │             ┌─────┐
                        │             │ CDU │
                        │             └──┬──┘
                        │                │
                        │                │ 内部 RS485
                        │                ▼
                        │          ┌────────────┐
                        │          │ Sub_1G接收板│
                        │          └─────┬──────┘
                        │                ▲
                        │    Sub-1G      │
                        └════════════════╝
                          433 / 915 MHz
```

其中：

- 调试工具直接连接无线急停盒；
- 调试工具通过 ROS 2 与 Robot Driver 通信；
- Robot Driver 通过 SPI 与 CDU 通信；
- CDU 与 Sub_1G 接收板之间采用机器人内部 RS485 总线；
- 无线急停盒与 Sub_1G 接收板之间采用 Sub-1G 无线链路；
- 调试工具不直接访问 CDU、不直接访问内部 RS485，也不直接参与运行时无线控制。

### 1.2.1 无线急停盒

无线急停盒是用户侧无线设备，主要负责：

- 急停按键输入；
- 暂停按键输入；
- 通信确认/寻机输入；
- 周期心跳；
- 无线控制数据发送；
- 保存无线通信参数；
- 保存绑定 Device ID、Kbind 等绑定信息；
- 在配置阶段通过 USB/BLE 与外部配置软件通信。

产品需求中，急停按键按下后应发送停止信号并机械锁定；旋起或拉起后停止发送停止信号，但机器人动力电恢复仍需由机器人侧软件完成急停错误清除。

暂停按键用于进入或退出暂停状态；暂停状态下机器人保持静止并拒绝新的运动任务。

通信确认按键承担配对模式与寻机功能：长按进入配对模式，短按执行寻机。

### 1.2.2 Sub_1G 接收板

Sub_1G 接收板安装在机器人内部，主要负责：

- 接收无线急停盒发送的 Sub-1G 数据；
- 校验绑定 Device ID；
- 校验动态计数器；
- 使用 Kbind 校验无线数据认证码；
- 检测周期心跳和无线链路中断；
- 维护急停、暂停、寻机、绑定有效等状态；
- 保存接收端无线通信参数和绑定信息；
- 向机器人系统提供运行状态。

产品需求中，机器人端接收器采用 12 V 供电，并通过 RS485 与机器人内部系统通信。

### 1.2.3 CDU

CDU（Cerebellum Control Unit）是机器人内部控制单元。

RCU 与 CDU 使用 SPI 主从通信，主要参数为：

- 全双工；
- 20 Mbps；
- MODE3；
- MSB First；
- RCU 为主节点；
- 单次发送/接收帧长度均为 100 Byte。

Sub_1G 设备在 CDU 协议中的轮询 ID 为 `0x05`。

CDU 与下属从设备之间的 RS485 被定义为机器人内部总线。因此 Linux Driver 不应绕过 CDU 直接操作 Sub_1G 接收板 RS485。

### 1.2.4 Robot Driver

Robot Driver 是机器人 Linux / ROS 2 侧访问无线急停接收板的统一软件入口。

其职责为：

- 封装 CDU/Sub_1G 底层协议；
- 读取接收板配置和运行状态；
- 修改接收板无线通信参数；
- 执行绑定、取消绑定和主动解绑；
- 提供接收板对象字典访问能力；
- 向调试工具提供 ROS 2 业务接口；
- 隔离 SPI 帧格式、CDU 数据区和内部 RS485 实现。

调试工具不应感知 SPI 字节位置、CDU Poll ID、RS485 地址或 MODBUS 等内部细节。

### 1.2.5 无线急停调试工具

无线急停调试工具用于研发、生产和测试阶段配置无线急停系统。

当前工具需求包含三个一级功能：

1. 连接机器人；
2. 无线急停盒配置；
3. 机器人急停板配置。

在本文中，“机器人急停板”统一称为“Sub_1G 接收板”或“接收板”。

调试工具同时维护两条独立通信链路：

```text
Tool ── USB Serial ── Wireless E-Stop Box

Tool ── ROS 2 ── Robot Driver ── CDU ── Sub_1G Receiver
```

## 1.3 无线急停运行链路

机器人正常运行期间，实际承担安全控制的是无线急停盒与接收板之间的 Sub-1G 链路：

```text
无线急停盒
     │
     │ Sub-1G
     ▼
Sub_1G 接收板
     │
     ▼
CDU
     │
     ▼
Robot Driver / Robot Control / Safety
```

调试工具不位于该链路中。

因此，调试工具未启动、退出或异常，不应影响已经完成配置和绑定的无线急停系统继续工作。

无线协议支持 433 MHz 和 915 MHz，当前主要测试 433 MHz；支持 LoRa 和 GFSK，默认使用 LoRa。默认无线负载长度为 12 Byte，默认采用单信道，默认心跳周期为 200 ms。

当前产品和协议能力可归纳如下：

| 项目 | 当前产品基线 | 协议能力 |
|---|---|---|
| 频段 | 433 MHz | 433 / 915 MHz |
| 调制方式 | 当前主要采用 LoRa | LoRa / GFSK |
| 绑定模式 | 一对一 | 一对一 / 一对多 |
| 心跳 | 默认开启 | 可配置开启/关闭 |
| 无线急停 | 默认开启 | 可配置开启/关闭 |

产品需求中保留未来一个发射器绑定多个机器人的能力，但当前阶段按一对一模式使用。因此调试工具 V1 的出厂绑定应按照一对一模式实现。

## 1.4 无线数据认证与链路监测

无线急停盒发送的心跳和业务控制数据包含：

- 急停盒 Device ID；
- 动态计数器；
- 控制字；
- 基于 Kbind 计算的认证码。

接收板应拒绝：

- Device ID 不匹配的数据；
- 旧计数值；
- 重复计数值；
- 认证失败的数据。

业务控制字定义为：

| Bit | 含义 |
|---|---|
| bit0 | 急停 |
| bit1 | 暂停 |
| bit2 | 寻机 |
| bit3 | 配对验证 |

无线链路默认：

- Heartbeat Interval：200 ms；
- Heartbeat Loss Threshold：3 Packet。

接收板运行状态至少包含：

- 当前绑定 Device ID；
- 最近无线动态计数器；
- 急停状态；
- 暂停状态；
- 寻机状态；
- 无线链路中断状态；
- 绑定有效状态；
- RSSI；
- SNR；
- Warning Code；
- Error Code。

产品需求规定无线通信中断后机器人应执行 Stop2。Stop2 定义为主动控制机器人减速至停止，保持平衡并维持动力电。

本文不规定 `link_lost → Stop2` 最终由 Driver、上层 Robot Control 还是独立 Safety 模块执行，因为当前四份源文档没有明确冻结该软件责任边界。

## 1.5 绑定关系

无线急停系统不是任意发射端都可以控制任意机器人。

无线急停盒与接收板通过以下绑定信息建立关系：

```text
Device ID
+
Kbind
```

其中 Kbind 为 Sub-1G 无线链路使用的 16 Byte 对称密钥。

绑定过程不能简单理解为“向接收板写入 Device ID 和 Kbind”。完整绑定还必须通过无线 FIND 验证确认两端能够使用相同 Kbind 正常通信。

完整逻辑为：

```text
急停盒生成 Device ID / Kbind
        │
        ▼
调试工具
        │
        │ ROS 2
        ▼
Robot Driver
        │
        ▼
接收板暂存绑定信息
        │
        ▼
Tool 触发 FIND
        │
        ▼
急停盒 ═════ Sub-1G FIND ═════▶ 接收板
        │                         │
        │◀════════ ACK ══════════│
        │
        ▼
确认绑定成功并正式生效
```

因此：

> Kbind 写入接收板不等于绑定成功；只有 FIND 无线验证成功后，绑定事务才完成。

## 1.6 调试工具系统边界

调试工具属于配置、出厂和诊断系统，不属于机器人实时安全控制链路。

调试工具负责：

- 建立机器人 ROS 2 连接；
- 建立无线急停盒 USB 串口连接；
- 配置无线急停盒；
- 配置机器人接收板；
- 同步两端无线通信参数；
- 执行出厂绑定；
- 执行绑定事务取消或解绑；
- 展示需求范围内的设备与配置状态。

调试工具不负责：

- 实时发送机器人运行时急停控制；
- 实现无线心跳；
- 直接处理机器人 Stop0 / Stop1 / Stop2 控制；
- 直接操作 CDU 内部 RS485；
- 实现无线急停盒或接收板固件；
- 替代 APP 的最终用户日常操作。

---

# 2. 调试工具需求

## 2.1 功能范围

调试工具 V1 包含以下一级业务能力：

| 模块 | 目标 |
|---|---|
| 连接管理 | 建立机器人和无线急停盒通信 |
| 无线急停盒配置 | 查看和配置无线急停盒 |
| 机器人接收板配置 | 查看和配置机器人接收板 |
| 参数同步 | 将无线急停盒参数同步到接收板 |
| 出厂绑定 | 完成一对一绑定及无线验证 |

## 2.2 连接管理

### 2.2.1 机器人连接

调试工具应提供：

- Robot IP；
- Domain ID；
- 连接状态；
- Connect；
- Disconnect；
- Apply；
- Reload。

连接状态至少包括：

- 未连接；
- 连接中；
- 已连接；
- 连接失败。

连接过程中应禁止重复连接操作。

连接成功后：

- Robot IP 不可编辑；
- Domain ID 不可编辑；
- “连接”切换为“断开连接”。

连接失败后：

- 显示连接失败；
- 恢复“连接”操作。

“应用”用于保存 Robot IP 和 Domain ID 到配置文件。

“重新加载”仅允许在未连接状态下执行，用于从配置文件重新加载 Robot IP 和 Domain ID。

本文已确认：

> 调试工具与 Robot Driver 使用 ROS 2 通信。

但源文档尚未定义：

- ROS 2 Service / Topic 名称；
- `.srv/.msg` 数据结构；
- ROS 2 Discovery 具体方案；
- Robot IP 与 ROS 2 Discovery 的具体关系。

上述内容应在后续 ROS 2 接口规格中冻结。

### 2.2.2 无线急停盒连接

调试工具通过 USB 串口连接无线急停盒。

串口参数固定为：

| 参数 | 值 |
|---|---:|
| Baud Rate | 115200 bps |
| Data Bits | 8 |
| Stop Bits | 1 |
| Parity | None |
| Flow Control | None |

工具应提供可用串口设备选择与连接状态展示。

机器人连接与无线急停盒串口连接应分别维护状态，不应使用单一“全局连接状态”替代两个独立连接状态。

当前需求文档将“已连接机器人”和“USB 已连接急停盒”同时列为无线急停盒配置与机器人接收板配置的前置条件。该限制是否应该保持到所有单侧配置操作，需要进一步确认，本文不自行修改需求。

## 2.3 无线急停盒配置

无线急停盒配置包括：

- Device ID；
- PIN；
- 无线通信参数；
- 参数读取；
- 参数配置；
- 恢复默认。

### 2.3.1 Device ID

工具需求要求显示当前连接无线急停盒的 Device ID。

当前协议中，Device ID 最明确的返回路径出现在绑定响应中；是否存在独立 Device ID 查询机制需要进一步确认。

### 2.3.2 PIN

工具需求要求：

- 读取 PIN；
- 配置出厂 PIN。

协议已经定义 PIN 配置：

- Request：`0x07`；
- Response：`0x87`；
- PIN：6 Byte ASCII 数字；
- 合法字符：`0x30 ~ 0x39`；
- 使用新的非 0 Transaction ID；
- 同一请求重试保持 Transaction ID 不变；
- 使用 CRC16-XMODEM。

当前协议没有定义独立“读取 PIN”命令，因此“读取 PIN”属于需求已提出但协议尚未闭环的功能。

实现阶段不得自行扩展私有协议解决该缺口。

### 2.3.3 无线通信参数

工具需求列出以下通信参数：

- 参数标志；
- 发射功率；
- 信道中心频率偏移；
- 负载字节长度；
- 信号接收强度门槛；
- 心跳包间隔；
- 心跳丢失判断门槛；
- 接收带宽；
- 扩频因子；
- 编码率；
- 报头类型；
- 前导码长度；
- 同步字；
- 操作类型；
- 信道扫描方式；
- 分组模式；
- 心跳包开关；
- 无线急停开关；
- 物理层 CRC 开关；
- 频段。

无线参数应支持：

- Read；
- Write；
- Restore Default。

## 2.4 无线参数模型

调试工具和 Robot Driver 应使用统一的业务级 `RadioConfig` 模型。

不应将：

- Byte Offset；
- bit 位；
- CDU 帧布局

直接暴露给 UI 或业务层。

### 2.4.1 公共参数

| 参数 | 协议约束 |
|---|---|
| Modulation | LoRa / GFSK |
| Frequency Band | 433 MHz / 915 MHz |
| TX Power | 0～22 dBm；433 MHz ≤10 dBm；915 MHz ≤20 dBm |
| Channel Frequency Offset | Center Frequency = Base + Offset |
| Payload Length | 固定 12 Byte |
| RSSI Threshold | raw 10～148，对应 `-raw dBm` |
| Heartbeat Interval | 200～10000 ms |
| Heartbeat Loss Threshold | 1～255 Packet |
| Channel Scan | 单信道 / 跳信道 |
| Group Mode | 一对一 / 一对多 |
| Heartbeat Enable | Enable / Disable |
| Wireless E-Stop Enable | Enable / Disable |
| PHY CRC | Enable / Disable |

当前阶段 Group Mode 应按一对一使用。

关闭无线急停属于受控维护操作，UI 不应将其设计成容易误触的普通开关。

### 2.4.2 LoRa 参数

| 参数 | 约束 |
|---|---|
| Bandwidth | 125 / 250 / 500 kHz |
| Spreading Factor | SF5～SF12 |
| Coding Rate | 4/5、4/6、4/7、4/8、LI4/5、LI4/6、LI4/8 |
| Header Type | Explicit / Implicit |
| Preamble Length | 10～50 symbol；SF5、SF6 固定 12 |
| Sync Word | `0xY4X4`，默认私网 `0x1424` |

### 2.4.3 GFSK 参数

协议定义 GFSK 专属参数：

- Receive Bandwidth；
- Bit Rate；
- Frequency Deviation；
- Gaussian Pulse Shaping；
- Preamble Length；
- Sync Word。

主要约束：

- Bit Rate：600～150000 bps；
- Frequency Deviation：600～300000 Hz；
- `2 × Frequency Deviation / Bit Rate ≥ 0.5`。

当前调试工具需求虽然允许操作类型选择 LoRa / GFSK，但并未列出全部 GFSK 专属配置字段。

因此，GFSK 是否完整进入 V1 UI 必须进一步确认。

## 2.5 默认参数

### 2.5.1 LoRa 默认参数

| 参数 | 默认值 |
|---|---|
| Modulation | LoRa |
| Frequency Band | 433 MHz |
| PHY CRC | Enabled |
| Wireless E-Stop | Enabled |
| Heartbeat | Enabled |
| Group Mode | 一对一 |
| TX Power | 10 dBm |
| Center Frequency | 433.250 MHz |
| Payload Length | 12 Byte |
| RSSI Threshold | -110 dBm |
| Heartbeat Interval | 200 ms |
| Heartbeat Loss | 3 Packet |
| Bandwidth | 250 kHz |
| Spreading Factor | SF6 |
| Coding Rate | LI4/5 |
| Header | Explicit |
| Preamble | 12 symbol |
| Sync Word | `0x1424` |

### 2.5.2 GFSK 默认参数

| 参数 | 默认值 |
|---|---|
| Frequency Band | 433 MHz |
| TX Power | 10 dBm |
| Center Frequency | 433.250 MHz |
| Payload Length | 12 Byte |
| RSSI Threshold | -110 dBm |
| Heartbeat Interval | 200 ms |
| Heartbeat Loss | 3 Packet |
| Receive Bandwidth | 234.3 kHz |
| Bit Rate | 50000 bps |
| Frequency Deviation | 25000 Hz |
| Pulse Shaping | Gaussian BT 0.5 |
| Preamble | 16 bit |
| Sync Word | `0x1424` |

当前协议提供默认值，但没有定义独立“恢复出厂默认参数”命令。

因此：

> 不能默认将“Restore Default”实现成“Tool 直接将上述默认值通过 0x05 写回”。

该行为需产品/协议负责人确认。

## 2.6 机器人接收板配置

接收板配置应包括：

- 当前已绑定 Device ID；
- 无线参数读取；
- 无线参数配置；
- 恢复默认；
- 从无线急停盒同步参数；
- 出厂绑定；
- 取消绑定 / 主动解绑。

工具不直接向接收板发送内部 RS485 数据。

所有机器人侧配置均经：

```text
Tool
  ↓ ROS 2
Robot Driver
  ↓
CDU
  ↓
Sub_1G Receiver
```

完成。

---

# 3. 软件与通信设计

## 3.1 软件总体架构

调试工具建议划分为 UI、Application、Device Client 三层：

```text
┌──────────────────────────────────┐
│               UI                 │
│                                  │
│ Connection                       │
│ Wireless E-Stop Box              │
│ Robot Receiver                   │
│ Factory Binding                  │
└────────────────┬─────────────────┘
                 │
                 ▼
┌──────────────────────────────────┐
│        Application Layer         │
│                                  │
│ Connection Management            │
│ Box Configuration                │
│ Receiver Configuration           │
│ Parameter Synchronization        │
│ Binding Orchestration            │
└─────────────┬──────────────┬─────┘
              │              │
              ▼              ▼
      ┌──────────────┐ ┌───────────────┐
      │ Box Client   │ │ Robot Client  │
      │ USB Serial   │ │ ROS 2         │
      └──────┬───────┘ └───────┬───────┘
             ▼                 ▼
       无线急停盒          Robot Driver
```

Application Layer 负责业务流程编排，不负责协议字节封装。

## 3.2 Tool ↔ 无线急停盒

调试工具通过 USB Serial 与无线急停盒通信。

APP-Sub1G 协议规定该方向业务帧均为 42 Byte：

- 多字节整数采用 Little Endian；
- Byte Array 按原序发送；
- 保留字段发送时置 0；
- CRC 使用 CRC16-XMODEM。

主要业务命令：

| Request | Response | 功能 |
|---:|---:|---|
| `0x01` | `0x81` | Binding |
| `0x02` | `0x82` | Cancel Binding |
| `0x03` | `0x83` | Find |
| `0x04` | `0x84` | Read Config / SDO |
| `0x05` | `0x85` | Write Config / SDO |
| `0x07` | `0x87` | Set PIN |

参数读写协议：

```text
Object Index == 0
    → Radio Config

Object Index != 0
    → SDO
```

事务规则：

- 每次独立操作使用新的非 0 Transaction ID；
- 同一请求重试保持相同 Transaction ID。

协议层应负责：

- Frame Encode；
- Frame Decode；
- Transaction ID；
- CRC；
- Response Matching；
- Timeout；
- Retry；
- Protocol Error。

UI 不应直接发送命令字。

## 3.3 Tool ↔ Robot Driver

Tool 与 Robot Driver 通过 ROS 2 通信。

ROS 2 接口应面向业务，而不是面向 CDU 原始帧。

至少需要覆盖：

| 能力 | 说明 |
|---|---|
| Get Receiver Status | 获取接收板运行/绑定状态 |
| Get Radio Config | 读取接收板无线参数 |
| Set Radio Config | 配置接收板无线参数 |
| Get Bound Device ID | 查询当前绑定设备 |
| Bind | 下发 Device ID、Kbind、Transaction ID |
| Cancel Binding | 取消正在进行的绑定事务 |
| Unbind | 解除现有绑定 |
| Read Object | 读取接收板 SDO |
| Write Object | 写入接收板可写 SDO |

Restore Default 是否作为独立 ROS 2 API，应在底层实现语义明确后决定。

本文件只定义业务接口能力，不虚构最终 `.srv/.msg` 名称和字段。

## 3.4 Driver ↔ CDU ↔ 接收板

CDU-Sub_1G 业务命令：

| Request | Response | 功能 |
|---:|---:|---|
| `0x01` | `0x81` | Binding |
| `0x02` | `0x82` | Cancel Binding / Unbind |
| `0x03` | `0x83` | Find |
| `0x04` | `0x84` | Read Config / SDO |
| `0x05` | `0x85` | Write Config / SDO |
| `0x06` | `0x86` | Runtime Status |
| `0x07` | `0x87` | PIN，仅外部 USB 使用 |

Robot Driver 负责：

```text
ROS 2 Business Request
        ↓
Sub_1G Business Command
        ↓
CDU SPI Frame
        ↓
CDU
        ↓
Receiver
```

响应反向转换为业务对象返回 Tool。

Tool 不应知道：

- `poll_id = 0x05`；
- SPI Byte 52～93；
- CRC 字节位置；
- CDU 数据区布局；
- RS485 帧；
- MODBUS。

## 3.5 统一业务模型

### 3.5.1 RadioConfig

统一表示急停盒和接收板无线通信参数。

建议至少包含：

- modulation；
- frequency_band；
- tx_power；
- channel_frequency_offset；
- payload_length；
- rssi_threshold；
- heartbeat_interval；
- heartbeat_loss_threshold；
- channel_scan_mode；
- group_mode；
- heartbeat_enable；
- wireless_estop_enable；
- phy_crc_enable；
- LoRa-specific parameters；
- GFSK-specific parameters。

### 3.5.2 BindingContext

绑定事务至少包含：

```text
device_id
kbind
transaction_id
```

其中 Kbind 属于安全敏感数据：

- UI 不应显示完整 Kbind；
- 日志不应记录完整 Kbind；
- 错误信息不应回显完整 Kbind。

### 3.5.3 ReceiverStatus

建议 Driver 业务模型能够表达：

- bound_device_id；
- wireless_counter；
- emergency_stop；
- pause；
- find；
- link_lost；
- binding_valid；
- rssi；
- snr；
- warning_code；
- error_code。

当前 V1 UI 是否展示其中全部字段，应以工具需求为准。

### 3.5.4 DeviceInfo

设备对象字典能够提供：

- Product Code；
- Version Number；
- Serial Number；
- App Firmware Version；
- Bootloader Firmware Version；
- Branch；
- Git SHA；
- Upgrade Request Flag 等。

这些能力适合保留在底层 SDO API 中，但当前需求未要求全部展示在 V1 UI。

---

# 4. 核心业务流程

## 4.1 参数读取与配置

无线急停盒：

```text
Tool
  │ USB Serial
  │ 0x04 / 0x05
  ▼
Wireless E-Stop Box
```

接收板：

```text
Tool
  │ ROS 2
  ▼
Robot Driver
  │
  ▼
CDU
  │
  ▼
Sub_1G Receiver
```

两侧使用相同的 `RadioConfig` 业务语义，但 Transport 与底层协议不同。

参数配置应执行：

```text
Read
 ↓
Edit
 ↓
Validate
 ↓
Write
 ↓
Read Back / Verify
```

具体是否要求每次写入后自动 Read Back，由详细设计阶段冻结。

## 4.2 参数同步

“同步配置急停盒参数”的业务语义为：

```text
        USB Serial
Tool ───────────────▶ Wireless E-Stop Box
        Read Config
             │
             ▼
         RadioConfig
             │
             │ ROS 2
             ▼
        Robot Driver
             │
             ▼
      Sub_1G Receiver
        Write Config
```

因此参数同步由 Tool 编排。

急停盒和接收板之间不通过无线链路自动同步配置。

同步成功至少要求接收板配置操作成功；是否进一步执行 Read Back 比对，需要在详细设计阶段冻结。

## 4.3 出厂绑定

出厂绑定是跨 USB、ROS 2 和 Sub-1G 三条链路的事务。

### 4.3.1 阶段一：急停盒准备绑定

```text
Tool ── 0x01 ──▶ Box
```

急停盒：

- 生成新的 Kbind；
- 返回自身 Device ID；
- 返回 Kbind；
- 回显 Binding Transaction ID；
- 暂存本次绑定事务。

成功返回的数据构成：

```text
BindingContext {
    Device ID
    Kbind
    Transaction ID
}
```

### 4.3.2 阶段二：接收板绑定准备

Tool 通过 ROS 2 把 BindingContext 交给 Robot Driver：

```text
Tool
   │ ROS 2
   ▼
Robot Driver
   │
   ▼
CDU
   │
   ▼
Sub_1G Receiver
```

接收板绑定请求必须使用发射端返回的：

- Device ID；
- Kbind；
- 原 Binding Transaction ID。

接收板成功响应只代表接收端绑定信息已准备，不代表整个系统已经完成绑定。

### 4.3.3 阶段三：FIND 无线验证

Tool 触发无线急停盒执行 FIND：

```text
Tool
   │
   ▼
Wireless E-Stop Box
   │
   │ Sub-1G FIND
   ▼
Receiver
```

接收板使用已经下发的绑定信息校验 FIND。

验证结果经机器人侧返回 Tool，同时无线急停盒收到对应无线 ACK。

只有该阶段成功后，绑定才能正式完成。

### 4.3.4 绑定成功条件

绑定成功条件必须满足：

```text
Box Binding Prepared
        +
Receiver Binding Prepared
        +
Wireless FIND Verified
        =
Binding Success
```

不能仅以 Receiver 成功保存 Kbind 作为“绑定成功”依据。

## 4.4 取消绑定与主动解绑

协议区分：

### Cancel Binding Transaction

用于正在进行但尚未正式完成的绑定事务。

需要同时取消：

- 无线急停盒侧事务；
- 接收板侧事务。

双方恢复原有绑定状态。

### Unbind Existing Device

用于解除已经完成的绑定。

协议规定主动解绑只解除接收板当前绑定。

Tool 业务层必须区分这两个操作，不能使用一个语义模糊的“解绑”接口覆盖全部场景。

## 4.5 事务与异常处理

以下操作使用 Transaction ID：

- 参数读取；
- 参数配置；
- PIN 配置；
- Binding；
- Cancel Binding。

规则：

- 新业务操作生成新的非 0 Transaction ID；
- 同一请求重试保持原 Transaction ID。

Tool / Protocol Layer 至少应处理：

- USB Timeout；
- ROS 2 Timeout；
- CRC Error；
- Transaction ID Mismatch；
- Unexpected Command；
- Device Disconnect；
- Receiver No Response；
- Parameter Validation Failure；
- Binding Receiver Failure；
- FIND Verification Failure；
- Binding Rollback Failure。

绑定失败时不能只显示“失败”，必须根据当前阶段执行必要的 Cancel Binding / Rollback。

---

# 5. 接口约束、验收边界与未决事项

## 5.1 通信责任矩阵

| 边界 | 通信方式 | 上层业务关注 | 上层不应关注 |
|---|---|---|---|
| Tool ↔ Box | USB Serial | Device、PIN、RadioConfig、Binding | MCU 内部实现 |
| Tool ↔ Driver | ROS 2 | Receiver Config、Binding、Status | SPI/CDU/RS485 |
| Driver ↔ CDU | SPI | Sub_1G 业务请求 | UI |
| CDU ↔ Receiver | 内部 RS485 | CDU Firmware | Tool |
| Box ↔ Receiver | Sub-1G | Runtime Safety | Tool Transport |

## 5.2 V1 必须覆盖的功能

根据当前调试工具需求，V1 至少应覆盖：

- Robot IP；
- Domain ID；
- Robot Connect / Disconnect；
- Robot Connection State；
- Robot Config Apply / Reload；
- USB Serial Device Select；
- Box Connect / Disconnect；
- Box Device ID Display；
- Box PIN Configuration；
- Box Radio Config Read；
- Box Radio Config Write；
- Box Restore Default 入口；
- Receiver Bound Device ID Query；
- Receiver Radio Config Read；
- Receiver Radio Config Write；
- Receiver Restore Default 入口；
- Synchronize Box Config to Receiver；
- Factory One-to-One Binding。

协议已经提供但当前工具需求没有明确要求展示的能力，例如：

- RSSI；
- SNR；
- Runtime E-Stop；
- Runtime Pause；
- Link Lost；
- Firmware Version；
- Branch；
- Git SHA；
- Warning；
- Error；

不应未经需求确认直接扩大为 V1 UI 功能。

## 5.3 安全与工程约束

### Kbind

Kbind 不应：

- 在 UI 中明文展示；
- 写入普通日志；
- 放入非必要错误信息；
- 在业务层长期持久化，除非正式设计明确要求。

### 无线急停开关

协议允许关闭无线急停，但该行为属于受控维护操作。

工具若提供此配置，应具备明显的风险提示或受控操作流程。

### 参数一致性

急停盒和接收板必须使用匹配的无线参数才能正常建立无线链路。

Tool 应提供参数同步能力，并避免用户无意中使两端参数长期不一致。

### 调试工具独立性

工具异常退出不得影响已经完成配置和绑定的无线急停系统正常运行。

## 5.4 当前未闭环事项

以下内容必须在详细实现前冻结：

| 编号 | 问题 | 当前状态 | 影响 |
|---|---|---|---|
| TBD-01 | PIN 如何读取 | 需求要求；协议未定义独立读取命令 | Box Configuration |
| TBD-02 | Box Device ID 如何独立查询 | 需求要求展示；协议独立查询路径不明确 | Device Info |
| TBD-03 | Restore Default 如何实现 | 需求要求；协议只有默认参数，没有恢复命令 | Box / Receiver |
| TBD-04 | GFSK 是否完整进入 V1 UI | 协议支持，需求字段不完整 | RadioConfig / UI |
| TBD-05 | ROS 2 API | 已确认 ROS 2，`.srv/.msg` 尚未定义 | Tool ↔ Driver |
| TBD-06 | Robot IP 与 ROS 2 Discovery 的关系 | 需求要求 IP + Domain ID，机制未定义 | Connection |
| TBD-07 | 登录功能 | 用例将登录作为前置条件，但没有功能定义 | UI / Permission |
| TBD-08 | Sub_1G Error / Warning | 当前协议仍有待定项 | Diagnosis |
| TBD-09 | `link_lost → Stop2` 的责任模块 | PRD 有行为，软件边界未冻结 | Robot Safety |
| TBD-10 | 配置操作是否必须同时连接 Robot 和 Box | 当前需求列为共同前置条件 | UI State |
| TBD-11 | 写参数后是否强制 Read Back Verify | 当前文档未明确 | Reliability |
| TBD-12 | Binding Rollback 超时和失败处理 | 协议定义取消语义，工具策略未冻结 | Binding |

## 5.5 设计结论

无线急停调试工具的职责可以归纳为：

> 通过 USB Serial 管理无线急停盒，通过 ROS 2 管理机器人侧无线急停接收板，并基于统一无线参数模型完成设备配置、参数同步以及经过无线 FIND 验证的可靠出厂绑定。

系统存在三条必须严格区分的通信链路：

```text
配置无线急停盒：
Tool ── USB Serial ── Box

配置机器人接收板：
Tool ── ROS 2 ── Driver ── CDU ── Receiver

机器人运行时无线急停：
Box ═══════ Sub-1G ═══════ Receiver
```

其中第三条是实际无线急停安全链路，不依赖调试工具。

后续工作应按以下顺序推进：

1. 冻结影响实现的 TBD；
2. 定义 Tool ↔ Driver 的 ROS 2 接口；
3. 冻结 `RadioConfig`、`BindingContext`、`ReceiverStatus` 等统一业务模型；
4. 定义 Binding / Rollback 状态机；
5. 在上述接口和业务模型稳定后进行 UI 详细设计和代码实现。

---

# 附录 A：协议命令汇总

| Request | Response | Tool ↔ Box | Robot ↔ Receiver | 说明 |
|---:|---:|---:|---:|---|
| `0x01` | `0x81` | 是 | 是 | Binding |
| `0x02` | `0x82` | 是 | 是 | Cancel Binding / Unbind |
| `0x03` | `0x83` | 是 | 响应/结果 | Find |
| `0x04` | `0x84` | 是 | 是 | Read Radio Config / SDO |
| `0x05` | `0x85` | 是 | 是 | Write Radio Config / SDO |
| `0x06` | `0x86` | 否 | 是 | Runtime Status |
| `0x07` | `0x87` | USB | 否 | Set PIN |

---

# 附录 B：源文档与本文内容映射

| 本文内容 | 主要来源 |
|---|---|
| Wireless Remote Stop 定位、发射器/接收器、停止类别 | S1 |
| 工具功能、连接、急停盒配置、接收板配置 | S2 |
| USB/BLE、无线参数、Binding、PIN、SDO、默认参数、无线心跳 | S3 |
| RCU/CDU、Sub_1G 数据区、机器人侧参数配置、运行状态、对象字典 | S4 |

本文对源文档没有明确规定的内容统一标记为 TBD，不以推断替代正式接口或产品定义。
