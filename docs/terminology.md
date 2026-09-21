# Wireless Remote Stop / 急停配置工具术语表

**适用项目：** WRS Debugger  
**文档用途：** 统一产品、协议、硬件、软件和代码中的术语与命名  
**依据文档：**
- 《无线急停系统_产品需求说明书_(PRD) V1.0》
- 《APP-Sub1G无线急停通讯协议 V1.0》
- 《H20-DVT2、H23_CDU通信协议 V1.1》
- 《急停配置工具迭代1需求 V1.7.0》
- 《调试工具—无线急停盒 USB Serial 通信协议设计文档》

> 说明：
> 1. “英文全称”优先采用原始文档明确给出的全称；原文未展开的术语标记为“原文未展开”。
> 2. “项目统一术语”用于 WRS Debugger 的代码、接口和设计文档，目的是消除“盒 / 板 / 接收器 / Sub_1G”等不同文档视角造成的歧义。
> 3. 产品角色名称与机器人内部硬件名称分层使用，不将 `Receiver` 与 `Sub1g` 直接视为同义词。

---

## 1. 产品与设备角色

| 原文术语 | 英文/缩写 | 释义 | 项目统一术语 / 推荐代码名 |
|---|---|---|---|
| 无线急停系统 | Wireless Remote Stop | 由用户侧发射器和机器人侧接收器组成的无线急停系统 | `Wireless Remote Stop` / `WRS` |
| 急停配置工具 | Debugger / Configuration Tool | 用于配置无线急停盒和机器人急停板的 PC 工具 | `Debugger` |
| 调试工具 | Debugger | 协议文档中与无线急停盒通过串口通信的软件端 | `Debugger` |
| 发射器 | Transmitter | 用户侧操作设备，包含停止按钮、暂停按钮、通信确认按钮等 | `Transmitter` |
| 无线急停盒 | — | 配置工具文档和通信协议中对发射器的称呼 | **统一为 `Transmitter`** |
| 接收器 | Receiver | 机器人本体内部的无线通信接收设备 | `Receiver` |
| 接收板 | Receiver | 无线急停通信协议中对机器人侧接收设备的简称 | **统一为 `Receiver`** |
| 无线急停盒接收板 | Receiver | 协议文档中的机器人侧无线接收设备 | `Receiver` |
| 机器人急停板 | Receiver | 配置工具需求中对机器人侧无线急停接收设备的称呼 | **统一为 `Receiver`** |
| 机器人端接收器 | Receiver | PRD 中布置于机器人本体内部的专用无线通信模块 | `Receiver` |
| 机器人 | Robot | H20-DVT2 / H23 等机器人整机 | `Robot` |
| Robot Driver | — | 调试工具通过 ROS 2 访问机器人侧功能的软件边界 | `RobotClient` 对应的远端 |
| APP | Application | 手机应用程序 | `App`，Debugger 代码中通常不使用 |
| H20 DVT2 | — | 无线急停系统适配机器人型号之一 | `H20 DVT2` |
| H23 | — | 无线急停系统适配机器人型号之一 | `H23` |

---

## 2. 机器人内部控制单元与硬件

| 缩写/术语 | 英文全称 | 文档释义 | 推荐代码名 |
|---|---|---|---|
| RCU | Robot Control Unit | 机器人控制单元；RCU-CDU SPI 链路中的主节点 | `Rcu` |
| CDU | Cerebellum Control Unit | 小脑控制单元；RCU-CDU SPI 链路中的从节点，同时作为内部 485 总线主机 | `Cdu` |
| PCU | Power Control Unit | 电源控制单元 | `Pcu` |
| LRCU | LED Ring Control Unit | 灯环控制单元 | `Lrcu` |
| MCU | Microcontroller Unit | 微控制器 | `Mcu` |
| IMU | Inertial Measurement Unit | 惯性测量单元 | `Imu` |
| GPS | Global Positioning System | 全球定位系统 | `Gps` |
| RTK | Real-time Kinematic | 实时差分定位 | `Rtk` |
| BAT | Battery | 电池 | `Battery` / `Bat` |
| Sub_1G | — | CDU 下属的 Sub-1G 无线通信设备/节点 | `Sub1g` |
| PCBA | 原文未展开 | 机器人端接收器采用的板级形态 | `Pcba`（仅硬件文档） |
| FPC 天线 | 原文未展开 | 机器人端接收器连接的天线形式 | `FpcAntenna`（如代码确有需要） |
| 大脑 | — | PRD 中机器人高配版计算/控制系统称谓 | 不建议直接作为底层类名 |
| 小脑 | — | PRD 中基础版机器人控制系统称谓；CDU 为 Cerebellum Control Unit | 底层使用 `Cdu` |

### 2.1 产品角色与硬件实现的关系

```text
Wireless Remote Stop
├── Transmitter              # 无线急停盒 / 发射器
└── Receiver                 # 接收器 / 接收板 / 机器人急停板
      └── 当前机器人内部实现涉及 Sub1g 等硬件节点

Robot
└── Rcu
     └── SPI
          └── Cdu
               └── RS485 / internal bus
                    ├── Pcu
                    ├── Lrcu
                    └── Sub1g
```

`Receiver` 是产品角色，`Sub1g` 是机器人内部硬件/协议节点名称，两者不应在代码中直接互换。

---

## 3. 有线接口、总线与通信方式

| 术语 | 英文/全称 | 文档含义 | 推荐代码/文档写法 |
|---|---|---|---|
| USB | 原文未展开 | PC 与无线急停盒连接相关接口 | `USB` |
| USB Type-C | 原文未展开 | 发射器物理接口形式；PRD 将其描述为充电接口 | `USB Type-C` |
| USB Serial | — | 调试工具与无线急停盒的逻辑串口通信方式 | `USB Serial` |
| USB 转串口 | — | 协议中 PC/APP 与无线急停盒的串口承载方式 | `USB Serial` |
| 串口 | Serial | 115200 bps、8 数据位、1 停止位、无校验、无流控 | `Serial` |
| SPI | 原文未展开 | RCU-CDU 主从通信总线 | `SPI` |
| SPI 主机 | — | RCU | `Rcu` / `master` |
| SPI 从机 | — | CDU | `Cdu` / `slave` |
| 全双工 | Full Duplex（原文未展开英文） | RCU-CDU SPI 同时发送和接收 | `full_duplex` |
| MODE3 | — | SPI 模式 3 | `Mode3` |
| MSB First | — | SPI 数据高位优先 | `msb_first` |
| RS485 / 485 | 原文未展开 | CDU 与内部从机通信总线；机器人端接收器也使用 RS485 | `RS485` |
| MODBUS | 原文未展开 | CDU-从机 485 总线采用的通信协议 | `Modbus` |
| 波特率 | Baud Rate | 串行链路通信速率 | `baud_rate` |
| 数据位 | Data Bits | 串口帧参数 | `data_bits` |
| 停止位 | Stop Bits | 串口帧参数 | `stop_bits` |
| 校验位 | Parity | 串口帧参数 | `parity` |
| 流控 | Flow Control | 串口流控设置 | `flow_control` |
| 通信帧 | Frame | 协议传输的固定或结构化数据单元 | `Frame` |
| 帧头 | Frame Header | CDU SPI 协议帧起始字段 | `frame_header` |
| 帧间隔 | Frame Interval | 连续通信帧间的时间间隔 | `frame_interval` |
| 透传模式 | Pass-through Mode | CDU 将升级帧转发至 PCU/LRCU 等节点 | `passthrough` |

---

## 4. 无线通信与射频术语

| 缩写/术语 | 英文全称 | 文档释义 | 推荐代码名 |
|---|---|---|---|
| Sub-1G | — | 低于 1 GHz 的无线通信；本方案包括 433 MHz 和 915 MHz | `Sub1g` |
| 433 MHz | — | 当前主要测试/默认无线频段 | `433MHz` |
| 915 MHz | — | 支持的另一无线频段 | `915MHz` |
| LoRa | Long Range | 远距离低功耗无线调制技术 | `Lora` / 枚举值 `lora` |
| GFSK | Gaussian Frequency Shift Keying | 高斯频移键控 | `Gfsk` / 枚举值 `gfsk` |
| RSSI | Received Signal Strength Indicator | 接收信号强度指示 | `rssi` |
| SNR | Signal-to-Noise Ratio | 信噪比 | `snr` |
| LDRO | Low Data Rate Optimization | 低数据速率优化 | `ldro` |
| 中心频率 | Center Frequency | 基值加信道中心频率偏移得到的无线中心频率 | `center_frequency` |
| 信道中心频率偏移 | Frequency Offset | 相对于频段基值的中心频率偏移 | `frequency_offset` |
| 发射功率 | TX Power | 无线发射功率，单位 dBm | `tx_power_dbm` |
| 接收带宽 | Receive Bandwidth | LoRa/GFSK 接收带宽 | `receive_bandwidth` |
| 信道 | Channel | 无线频率/跳频使用的频道 | `channel` |
| 单信道 | Single Channel | 固定使用单一信道 | `single_channel` |
| 跳信道 | Channel Hopping | 在多个信道间切换 | `channel_hopping` |
| 公共信道 | Public Channel | 跳信道模式中的公共频道；文档暂定 CH1 433.25 MHz | `public_channel` |
| 扩频因子 | Spreading Factor | LoRa SF5～SF12 | `spreading_factor` |
| SF | Spreading Factor（由字段语义） | LoRa 扩频因子简称，如 SF6 | `sf` / `spreading_factor` |
| 编码率 | Coding Rate | LoRa 4/5、4/6、4/7、4/8、LI4/5 等编码率 | `coding_rate` |
| LI4/5 等 | — | LoRa 编码率枚举值 | 保持协议枚举名称 |
| 报头类型 | Header Type | LoRa 显式/隐式报头模式 | `header_type` |
| 显式报头 | Explicit Header | LoRa 报头类型 | `explicit` |
| 隐式报头 | Implicit Header | LoRa 报头类型；要求收发负载长度一致 | `implicit` |
| 前导码 | Preamble | LoRa/GFSK 帧同步前导字段 | `preamble_length` |
| 同步字 | Sync Word | 无线协议同步标识；默认私网 `0x1424` | `sync_word` |
| 私网同步字 | Private Sync Word | 默认 `0x1424` | `private_sync_word` |
| 公网同步字 | Public Sync Word | LoRa 文档值 `0x3444` | `public_sync_word` |
| 负载字节长度 | Payload Length | 无线帧业务载荷长度，当前固定 12 Byte | `payload_length` |
| 码率 | Bit Rate | GFSK 比特率 | `bit_rate_bps` |
| 频偏 | Frequency Deviation | GFSK 频率偏移量 | `frequency_deviation_hz` |
| 脉冲整形 | Pulse Shaping | GFSK Gaussian BT 参数 | `pulse_shaping` |
| BT | — | GFSK 高斯滤波相关参数，如 BT 0.5 | `bt` |
| 物理层 CRC | PHY CRC | 无线物理层 CRC 开关 | `phy_crc` |
| 心跳包 | Heartbeat | 周期通信监测包 | `heartbeat` |
| 心跳周期 / 心跳包间隔 | Heartbeat Interval | 心跳发送周期，默认 200 ms | `heartbeat_interval_ms` |
| 心跳丢失判断门槛 | Heartbeat Loss Threshold | 连续丢失多少个心跳后判定异常 | `heartbeat_loss_threshold` |
| 无线帧长度 | Wireless Frame Length | Sub-1G 无线帧长度，协议中为 12 Byte | `wireless_frame_length` |

---

## 5. 协议、数据模型与帧字段术语

| 缩写/术语 | 英文全称 | 释义 | 推荐代码名 |
|---|---|---|---|
| SDO | Service Data Object | 服务数据对象，用于非周期对象读写 | `Sdo` |
| PDO | Process Data Object | 过程数据对象 | `Pdo` |
| Object Index | — | 16 位对象索引，高 4 位为读写/响应状态，低 12 位为对象地址 | `object_index` |
| Object Data | — | SDO 读写的数据值 | `object_data` |
| 对象字典 | Object Dictionary | SDO 可访问对象及权限定义 | `ObjectDictionary` |
| RO | Read Only（原文以权限缩写出现） | 只读对象权限 | `read_only` |
| RW | Read/Write（原文以权限缩写出现） | 可读写对象权限 | `read_write` |
| 事务 ID | Transaction ID | 请求与响应关联用的非 0 `uint32` 标识；重试保持不变 | `transaction_id` |
| 系统指令 | Command | 业务帧 Byte0 指令码 | `command` |
| 请求 | Request | 调试工具或主机发出的命令帧 | `Request` |
| 响应 | Response | 设备返回的应答帧 | `Response` |
| ACK | 原文未展开 | 应答语义，在无线业务控制包中 bit7=1 表示 ACK | `ack` |
| 参数标志 | Parameter Flags | 通信参数帧中的 16 位配置标志 | `parameter_flags` |
| 操作类型 | Operation Type | Parameter Flags bit15..14；LoRa/GFSK 类型选择 | `operation_type` |
| 信道扫描方式 | Channel Scan Mode | 单信道/跳信道选择 | `channel_scan_mode` |
| 分组模式 | Group Mode | 一对一/一对多模式 | `group_mode` |
| 心跳包开关 | Heartbeat Enable | 0 开启、1 关闭 | `heartbeat_enabled` |
| 无线急停开关 | Wireless E-stop Enable | 0 开启、1 关闭 | `wireless_estop_enabled` |
| 物理层 CRC 开关 | PHY CRC Enable | 0 开启、1 关闭 | `phy_crc_enabled` |
| 频段 | Frequency Band | 433 MHz / 915 MHz | `frequency_band` |
| 结果码 | Result Code | 业务帧处理结果 | `result_code` |
| 错误码 | Error Code | CDU/Sub_1G 等协议的错误状态编码 | `error_code` |
| 警告码 | Warning Code | CDU/Sub_1G 等协议的警告状态编码 | `warning_code` |
| 标志位 | Flag | 状态或控制字段中的位标志 | `flags` |
| CRC | Cyclic Redundancy Check | 循环冗余校验 | `crc` |
| CRC16-XMODEM | — | USB Serial 业务帧 CRC 算法；覆盖 Byte0～39，低字节在前 | `crc16_xmodem` |
| CRC-16/CCITT-FALSE | — | GFSK 物理层 CRC 类型 | 保持标准名 |
| Little Endian | — | 多字节整数低字节优先编码 | `little_endian` |
| MSB First | — | SPI 比特发送顺序，高位优先 | `msb_first` |
| 保留字段 | Reserved Field | 预留的协议字节/位；发送时通常固定 0 | `reserved` |
| 轮询 | Polling | RCU/CDU 或 CDU/从机周期访问方式 | `polling` |
| 轮询 ID | Polling ID | CDU 协议中用于从机轮询/电气指令的节点选择标识 | `polling_id` |
| 主机 | Master | 主动发起通信的一端 | `master` |
| 从机 | Slave | 响应主机请求的一端 | `slave` |

---

## 6. 身份、绑定与配置术语

| 术语 | 英文/全称 | 释义 | 推荐代码名 |
|---|---|---|---|
| ID | Identifier | 标识符 | `Id` |
| Device ID / DeviceID | — | 无线急停盒设备 ID；协议其他帧定义为芯片 ID 后 3 字节 | `TransmitterDeviceId` |
| 急停盒设备 ID | — | 无线急停盒的业务设备标识 | `TransmitterDeviceId` |
| 已绑定急停盒 DeviceID | — | 机器人急停板保存的已绑定发射器设备 ID | `bound_transmitter_device_id` |
| Product Code | — | SDO `0x001`，设备产品编码 | `product_code` |
| Version Number | — | SDO `0x002`，设备版本号 | `version_number` |
| Serial Number | — | SDO `0x003`，设备序列号 | `serial_number` |
| SN | 原文未展开 | PRD 中出厂标签上用于标识配对机器人的序列信息 | `serial_number`（按上下文） |
| USB Serial Number | — | PC USB 枚举层序列号；当前硬件能力待确认 | `usb_serial_number` |
| PIN | 原文未展开 | 无线急停盒 6 位 ASCII PIN；`"000000"` 在当前确认规则中用于读取 PIN | `pin` |
| 出厂 PIN | Factory PIN | 出厂配置的无线急停盒 PIN | `factory_pin` |
| 绑定 | Binding | 建立发射器与机器人侧接收器的关联 | `bind` |
| 出厂绑定 | Factory Binding | 调试工具出厂阶段建立一对一绑定 | `factory_bind` |
| 解绑 | Unbinding | 删除已有绑定关系 | `unbind` |
| 取消绑定 | Cancel Binding | 取消正在进行的绑定事务 | `cancel_binding` |
| 配对 | Pairing | 产品/APP 层建立发射器与机器人关系的操作 | `pairing` |
| 配对模式 | Pairing Mode | 长按通信确认按钮进入的设备状态 | `pairing_mode` |
| 寻机 | Find / Locate | 发射器触发已绑定机器人产生识别灯效的功能 | `locate` / `find_robot` |
| Kbind | Key bind | Sub-1G 无线链路使用的 16 字节对称绑定密钥 | `Kbind` / `binding_key` |
| 动态计数器 | Dynamic Counter | Sub-1G 包中的防重放/顺序相关计数值 | `counter` |
| 认证码 | Authentication Code | 使用 Kbind 计算的短认证数据 | `auth_code` |

> `Device ID`、SDO `Serial Number`、USB `Serial Number` 是三个不同概念，不应在代码中合并为一个通用 `id`。

---

## 7. 安全与密码学术语

| 缩写/术语 | 英文全称 | 文档释义 | 推荐写法 |
|---|---|---|---|
| AEAD | Authenticated Encryption with Associated Data | 带关联数据的认证加密 | `AEAD` |
| AES-CCM | Advanced Encryption Standard - Counter with CBC-MAC | AES 计数器加密与 CBC-MAC 认证模式 | `AES-CCM` |
| CMAC | Cipher-based Message Authentication Code | 基于分组密码的消息认证码 | `CMAC` |
| AES-CMAC | Advanced Encryption Standard CMAC | 使用 AES 实现的 CMAC | `AES-CMAC` |
| AES-128-CMAC | — | 协议中使用 Kbind 计算认证码的算法 | `AES-128-CMAC` |
| MIC | Message Integrity Check | 消息完整性校验值 | `MIC` |
| 对称密钥 | Symmetric Key（原文未给英文） | Kbind 为 16 字节对称密钥 | `binding_key` |
| 认证 | Authentication | 验证消息/设备是否合法 | `authentication` |
| 完整性校验 | Integrity Check | 检查消息是否被篡改 | `integrity_check` |

---

## 8. BLE 与移动端相关术语

| 缩写/术语 | 英文全称 | 文档释义 | 推荐写法 |
|---|---|---|---|
| BLE | Bluetooth Low Energy | 低功耗蓝牙 | `BLE` |
| ATT_MTU | Attribute Protocol Maximum Transmission Unit | BLE 属性协议最大传输单元 | `ATT_MTU` |
| APP | Application | 手机应用程序 | `App` |
| 蓝牙 PIN | — | 原协议对无线急停盒 PIN 字段的称呼 | 项目统一写 `PIN` |
| 蓝牙通信 | BLE Communication | APP 与发射器配对时使用的通信方式 | `BLE` |

---

## 9. 固件、版本与升级术语

| 术语 | 英文/原文 | 释义 | 推荐代码名 |
|---|---|---|---|
| Firmware | Firmware | 设备固件 | `firmware` |
| App Firmware | `App_Firmware_Version` | 应用固件版本字符串对象 | `app_firmware_version` |
| Bootloader | `Bootloader_Firmware_Version` | Bootloader 固件版本字符串对象 | `bootloader_firmware_version` |
| AppBranchName | — | 应用固件分支名称 | `app_branch_name` |
| AppTagSha1Id | — | 应用固件 Tag/SHA1 标识 | `app_tag_sha1_id` |
| BootBranchName | — | Bootloader 分支名称 | `boot_branch_name` |
| BootTagSha1Id | — | Bootloader Tag/SHA1 标识 | `boot_tag_sha1_id` |
| Upgrade_Request_flag | — | SDO `0x202` 升级请求标志，`0x454E` 表示升级标志 | `upgrade_request_flag` |
| 固件升级 | Firmware Upgrade | 设备固件更新流程 | `firmware_upgrade` |
| 在线升级 | Online Upgrade | 通过通信链路执行的固件升级 | `online_upgrade` |
| 升级目标 | Upgrade Target | CDU/PCU/LRCU 等被升级节点 | `upgrade_target` |
| 运行状态 | Runtime Status | 固件升级/设备状态查询中的运行状态 | `runtime_status` |
| 透传 | Pass-through | CDU 将数据转发给下属节点 | `passthrough` |

---

## 10. 急停、安全控制与用户交互术语

| 术语 | 文档含义 | 推荐代码名 |
|---|---|---|
| 急停 / 紧急停止 | 发射器停止按钮触发的机器人紧急停止功能 | `estop` / `emergency_stop` |
| 无线急停 | 通过无线链路实现的急停功能 | `wireless_estop` |
| 急停按钮 / 停止信号发射按钮 | 发射器蘑菇头停止按钮 | `estop_button` |
| 暂停 | 机器人停止运动并保持静止，但不等同于急停 | `pause` |
| 暂停模式 | 锁定当前状态且不接受运动指令/任务 | `pause_mode` |
| 通信确认按钮 | 配对模式开关及寻机确认按钮 | `communication_button` |
| 通信中断监测 | 无线通信丢失检测机制 | `communication_monitor` |
| Stop0 | 严重错误；先切阻尼，延时后切断动力电 | `Stop0` |
| Stop1 | 主动控制减速到安全位置/速度后切断动力电 | `Stop1` |
| Stop2 | 主动控制减速至停止并保持平衡，保持动力电 | `Stop2` |
| 受控停止 | 机器人主动减速/保持平衡的停止过程 | `controlled_stop` |
| 阻尼 | Stop0 描述中的关节/驱动阻尼状态 | `damping` |
| 动力电 | 机器人执行器动力供电 | `power` / `drive_power` |
| 配对验证 | 无线控制字中的功能位 | `pairing_verification` |
| 通信监测 | 心跳/链路状态检测 | `communication_health` |
| 电量 | 发射器电池百分比；SDO `0x102` 范围 0～100 | `battery_percent` |

---

## 11. I/O、电气与板级术语

| 缩写/术语 | 文档含义 | 推荐写法 |
|---|---|---|
| IO / I/O | 输入/输出接口 | `io` |
| DI | 原文未展开；发射器具备 2 路 DI | `di` / `digital_input` |
| DO | 原文未展开；发射器具备 1 路 DO | `do` / `digital_output` |
| DC 5V | 发射器充电建议电压 | `5 V DC` |
| 12V | 机器人端接收器供电电压 | `12 V` |
| LED | 原文未展开 | 发射器电量灯和机器人灯环相关指示 | `led` |
| LED Ring | LRCU 对应灯环系统 | `led_ring` |
| 电量指示灯 | 发射器 4 灯电量显示 | `battery_indicator` |
| 快闪 / 慢闪 | LED 状态显示方式 | `fast_blink` / `slow_blink` |

---

## 12. 调试工具与软件配置术语

| 术语 | 释义 | 推荐代码/接口名 |
|---|---|---|
| 无线急停盒配置 | 对发射器 PIN、通信参数等进行配置 | `TransmitterConfig` |
| 机器人急停板配置 | 对机器人侧 Receiver 通信参数进行配置 | `ReceiverConfig` |
| 连接机器人 | 配置工具建立与 Robot Driver/机器人侧软件通信 | `RobotConnection` |
| 连接急停盒 | PC 通过 USB Serial 连接 Transmitter | `TransmitterConnection` |
| 串口下拉框 | UI 中选择可用无线急停盒/串口的控件 | UI 不直接暴露为业务模型 |
| 自动发现 | 自动枚举 USB Serial 并通过协议确认无线急停盒 | `TransmitterDiscovery` |
| 恢复默认参数 | 将完整出厂默认通信参数重新写入设备 | `restore_defaults` |
| 同步配置急停盒参数 | 读取 Transmitter 参数并配置到 Receiver | `sync_transmitter_config` |
| 应用 | 保存连接配置等设置 | `apply` |
| 重新加载 | 从配置文件重新读取设置 | `reload` |
| IP 地址 | 机器人网络连接地址 | `ip_address` |
| Domain ID | ROS 2 Domain ID（需求文档仅给名称） | `domain_id` |
| 连接状态 | 未连接/连接中/已连接/连接失败等 | `connection_state` |

---

## 13. 认证、法规与环境标准术语

以下术语均出现在产品 PRD 中。原文主要列出法规/认证名称，没有对缩写逐一展开，因此此表保持原文名称，不自行补充全称。

| 术语 | 文档中的用途 |
|---|---|
| CE | 欧盟目标认证 |
| UL | 美国相关认证/标准 |
| EMC | 欧盟 CE 相关指令 |
| RoHS | 欧盟及中国环保要求 |
| RED | 欧盟无线设备相关指令 |
| REACH | 欧盟环保法规 |
| PoPs | 欧盟环保法规 |
| FCC | 美国法规要求，Part 15B/C |
| TSCA | 美国环保法规 |
| CP65 | 美国环保法规 |
| SRRC | 中国无线电发射设备型号核准 |
| EN IEC62368-1 | 欧盟相关安全标准 |
| EN 61000-6-2 | 欧盟 EMC 相关标准 |
| EN 61000-6-4 | 欧盟 EMC 相关标准 |
| EN 300 328 | 欧盟无线相关标准 |
| EN 301 489-1 | 欧盟无线 EMC 相关标准 |
| EN 301 489-17 | 欧盟无线 EMC 相关标准 |
| EN 18031 | 欧盟相关标准 |
| EN 62133-2 | 电池相关标准 |
| UL 62368-1 | 美国安全标准 |
| UL94 | 非金属材料阻燃相关标准 |
| UL 62133-2 | 电池相关标准 |
| GB/T 2423.1-2008 | 环境试验相关国标 |
| GB/T 2423.2-2008 | 环境试验相关国标 |
| GB/T 2423.3-2016 | 环境试验相关国标 |

---

## 14. 单位与协议表示

| 表示 | 含义 |
|---|---|
| bps | 串行/无线数据速率单位 |
| kbps | 千比特每秒 |
| Mbps | 兆比特每秒 |
| Hz | 赫兹 |
| kHz | 千赫兹 |
| MHz | 兆赫兹 |
| dBm | 射频功率/信号强度相关单位 |
| ms | 毫秒 |
| μs / us | 微秒 |
| bit | 比特 |
| Byte | 字节 |
| symbol | LoRa 前导码符号数 |
| % | 电量百分比 |
| V | 电压 |
| mA | 电流 |
| mAh | 电池容量 |
| mm | 长度 |
| ℃ | 温度 |
| uint8 / uint8_t | 8 位无符号整数 |
| uint16 | 16 位无符号整数 |
| uint32 | 32 位无符号整数 |
| int16 | 16 位有符号整数 |
| string[40] | 40 字节/字符协议字符串对象 |

---

## 15. 项目代码命名基线

建议 WRS Debugger 代码统一采用以下术语，不直接照搬不同文档中的“盒/板”名称：

| 业务对象 | 统一 C++ 名称 | 建议文件/目录 |
|---|---|---|
| 无线急停盒 / 发射器 | `Transmitter` | `transmitter/` |
| 无线急停盒客户端 | `TransmitterClient` | `transmitter/client.*` |
| 无线急停盒发现 | `TransmitterDiscovery` | `transmitter/discovery.*` |
| 无线急停盒信息 | `TransmitterInfo` | `transmitter/types.*` |
| 无线急停盒配置 | `TransmitterConfig` | `transmitter/types.*` |
| 无线急停盒 Device ID | `TransmitterDeviceId` | `transmitter/types.*` |
| 机器人侧接收器/急停板 | `Receiver` | 业务模型名称 |
| 接收器配置 | `ReceiverConfig` | `robot/` 或共享业务类型 |
| 接收器信息 | `ReceiverInfo` | `robot/` 或共享业务类型 |
| 已绑定发射器 ID | `bound_transmitter_device_id` | `ReceiverInfo` |
| 机器人客户端 | `RobotClient` | `robot/client.*` |
| 小脑控制单元 | `Cdu` | 仅机器人底层驱动 |
| 机器人控制单元 | `Rcu` | 仅机器人底层驱动 |
| Sub-1G 节点 | `Sub1g` | 仅机器人底层驱动 |
| 电源控制单元 | `Pcu` | 仅机器人底层驱动 |
| 灯环控制单元 | `Lrcu` | 仅机器人底层驱动 |

### 15.1 禁止混用

以下名称不应作为同义词随意互换：

```text
TransmitterDeviceId != Serial Number != USB Serial Number

Receiver != Sub1g

Transmitter != Box（代码层）

Receiver != Board（代码层）

RobotClient != ReceiverClient
```

推荐的顶层业务表达：

```text
wrs::debugger
├── transmitter
│   ├── TransmitterClient
│   ├── TransmitterDiscovery
│   ├── TransmitterInfo
│   └── TransmitterConfig
│
├── robot
│   ├── RobotClient
│   ├── ReceiverInfo
│   └── ReceiverConfig
│
└── Application
```
