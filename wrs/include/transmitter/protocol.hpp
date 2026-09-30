#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace transmitter {

// 数据帧固定长度: Byte0 ~ Byte41
constexpr std::size_t kFrameSize = 42;
// 数据帧的载荷范围: Byte0 ~ Byte39
constexpr std::size_t kFrameBodySize = 40;
// 数据帧的 CRC16-XMODEM: Byte40 ~ Byte41, 小端字节序
constexpr std::size_t kFrameCrcSize = 2;
// 无线负载长度固定值
constexpr std::uint8_t kPayloadLen = 12;
// PIN 码长度
constexpr std::size_t kPinLength = 6;

/**
 * @brief 原始数据帧[42字节]。
 */
struct Frame final {
    std::array<std::uint8_t, kFrameSize> data{};

    /**
     * @brief 获取可写字节指针。
     * @return 指向 42 字节缓冲区的可写指针。
     */
    [[nodiscard]] std::uint8_t* bytes() noexcept { return data.data(); }

    /**
     * @brief 获取只读字节指针。
     * @return 指向 42 字节缓冲区的只读指针。
     */
    [[nodiscard]] const std::uint8_t* bytes() const noexcept { return data.data(); }

    /**
     * @brief 按下标访问字节（可写）。
     * @param index 字节下标，范围 0~41。
     * @return 对应字节的可写引用。
     */
    std::uint8_t& operator[](std::size_t index) noexcept { return data[index]; }

    /**
     * @brief 按下标访问字节（只读）。
     * @param index 字节下标，范围 0~41。
     * @return 对应字节的只读引用。
     */
    const std::uint8_t& operator[](std::size_t index) const noexcept { return data[index]; }

    /**
     * @brief 帧固定长度。
     * @return 固定为 42。
     */
    [[nodiscard]] static constexpr std::size_t size() noexcept { return kFrameSize; }
};
static_assert(sizeof(Frame) == kFrameSize, "Frame must be exactly 42 bytes");

/**
 * @brief 系统指令。
 */
enum class SystemCommand : std::uint8_t {
    BindReq = 0x01,  // 绑定请求
    BindRsp = 0x81,  // 绑定响应

    UnbindReq = 0x02,  // 取消绑定/主动解绑请求
    UnbindRsp = 0x82,  // 取消绑定/主动解绑响应

    FindReq = 0x03,  // 寻机请求
    FindRsp = 0x83,  // 寻机响应

    ReadParamReq = 0x04,  // 读取通信参数请求
    ReadParamRsp = 0x84,  // 读取通信参数响应

    WriteParamReq = 0x05,  // 配置通信参数请求
    WriteParamRsp = 0x85,  // 配置通信参数响应

    RunStatusReq = 0x06,  // 运行状态通信请求
    RunStatusRsp = 0x86,  // 运行状态通信响应

    PinConfigReq = 0x07,  // 配置 PIN 码请求
    PinConfigRsp = 0x87,  // 配置 PIN 码响应
};

/**
 * @brief 判断是否为请求指令。
 * @param cmd 系统指令码。
 * @return true=请求，false=响应。
 */
[[nodiscard]] constexpr bool is_request(SystemCommand cmd) noexcept {
    return (static_cast<std::uint8_t>(cmd) & 0x80) == 0;
}

/**
 * @brief 判断是否为响应指令。
 * @param cmd 系统指令码。
 * @return true=响应，false=请求。
 */
[[nodiscard]] constexpr bool is_response(SystemCommand cmd) noexcept {
    return (static_cast<std::uint8_t>(cmd) & 0x80) != 0;
}

/**
 * @brief 请求指令转对应响应指令。
 * @param req 请求指令。
 * @return 对应的响应指令；若传入已是响应指令，则原样返回。
 */
[[nodiscard]] constexpr SystemCommand to_response(SystemCommand req) noexcept {
    return static_cast<SystemCommand>(static_cast<std::uint8_t>(req) | 0x80);
}

/**
 * @brief 响应指令转对应请求指令。
 * @param rsp 响应指令。
 * @return 对应的请求指令；若传入已是请求指令，则原样返回。
 */
[[nodiscard]] constexpr SystemCommand to_request(SystemCommand rsp) noexcept {
    return static_cast<SystemCommand>(static_cast<std::uint8_t>(rsp) & 0x7F);
}

/**
 * @brief 结果码。
 */
enum class ResultCode : std::uint8_t {
    Success = 0x00,  // 处理成功
    Failure = 0xFF   // 处理失败
};

/**
 * @brief SDO 执行状态。
 */
enum class SdoStatus : std::uint8_t {
    NotReceived = 0x0,   // 从机未收到
    ReadSuccess = 0x4,   // 读取成功
    WriteSuccess = 0x6,  // 写入成功
    InProgress = 0xB,    // 执行中
    Error = 0x8,         // 发生错误
    InvalidCmd = 0xE,    // 无效命令
};

/**
 * @brief Sub-1G 参数标志。
 *
 * 对应协议 4.1.5 节 Byte11~12，共 16 位，小端字节序。
 *
 * 位段分配：
 *   bit15..14  操作类型（协议表 4.1.5-2 称“操作类型”，3.2 节称“调制方式”）
 *   bit13..6   保留，固定 0
 *   bit5       信道扫描方式
 *   bit4       分组模式
 *   bit3       心跳包开关
 *   bit2       无线急停开关
 *   bit1       物理层 CRC 开关
 *   bit0       频段
 */
struct ParamFlags final {
    /**
     * @brief 频段。
     */
    enum class Band : std::uint16_t {
        MHz433 = 0,  // 433 MHz
        MHz915 = 1,  // 915 MHz
    };

    /**
     * @brief 物理层 CRC 开关。
     */
    enum class PhysicalCrcSwitch : std::uint16_t {
        Enabled = 0,   // 开启
        Disabled = 1,  // 关闭
    };

    /**
     * @brief 无线急停开关。
     */
    enum class WirelessEstopSwitch : std::uint16_t {
        Enabled = 0,   // 开启
        Disabled = 1,  // 关闭
    };

    /**
     * @brief 心跳包开关。
     */
    enum class HeartbeatSwitch : std::uint16_t {
        Enabled = 0,   // 开启
        Disabled = 1,  // 关闭
    };

    /**
     * @brief 分组模式。
     */
    enum class GroupMode : std::uint16_t {
        OneToOne = 0,   // 一对一
        OneToMany = 1,  // 一对多
    };

    /**
     * @brief 信道扫描方式（协议表 4.1.5-2 bit5）。
     */
    enum class ChannelScanMode : std::uint16_t {
        Single = 0,   // 单信道
        Hopping = 1,  // 跳信道
    };

    /**
     * @brief 调制方式（协议表 4.1.5-2 称“操作类型”）。
     */
    enum class Modulation : std::uint16_t {
        LoRa = 0,  // LoRa 调制
        Gfsk = 1,  // GFSK 调制
    };

    Band band{Band::MHz433};                                           // bit0: 频段
    PhysicalCrcSwitch physical_crc{PhysicalCrcSwitch::Enabled};        // bit1: 物理层 CRC 开关
    WirelessEstopSwitch wireless_estop{WirelessEstopSwitch::Enabled};  // bit2: 无线急停开关
    HeartbeatSwitch heartbeat{HeartbeatSwitch::Enabled};               // bit3: 心跳包开关
    GroupMode group_mode{GroupMode::OneToOne};                         // bit4: 分组模式
    ChannelScanMode channel_scan_mode{ChannelScanMode::Single};        // bit5: 信道扫描方式
    // bit6~13: 保留，固定 0
    Modulation modulation{Modulation::LoRa};  // bit15..14: 调制方式

    /**
     * @brief 序列化为 16 位参数标志。
     * @return 16 位参数标志值；写入 Byte11~12 时按小端序编码。
     * @note 保留位 bit6~13 固定为 0。
     */
    [[nodiscard]] std::uint16_t to_flags() const noexcept;

    /**
     * @brief 从 16 位参数标志反序列化。
     * @param flags 从 Byte11~12 按小端序读取后的 16 位参数标志值。
     * @return 解析后的 ParamFlags 对象。
     * @note 保留位 bit6~13 被忽略；bit15..14 的保留取值 2、3 原样保留，调用方须校验。
     */
    [[nodiscard]] static ParamFlags from_flags(std::uint16_t flags) noexcept;

    /**
     * @brief 比较两个参数标志的全部字段。
     * @param left 左侧参数标志。
     * @param right 右侧参数标志。
     * @return true=全部字段相同，false=至少一个字段不同。
     */
    [[nodiscard]] friend constexpr bool operator==(
        const ParamFlags& left, const ParamFlags& right) noexcept {
        if (left.band != right.band) return false;
        if (left.physical_crc != right.physical_crc) return false;
        if (left.wireless_estop != right.wireless_estop) return false;
        if (left.heartbeat != right.heartbeat) return false;
        if (left.group_mode != right.group_mode) return false;
        if (left.channel_scan_mode != right.channel_scan_mode) return false;
        if (left.modulation != right.modulation) return false;
        return true;
    }
};

/**
 * @brief 将 16 位无符号值以小端序写入目标地址。
 * @param destination 目标地址。
 * @param value 待写入的 16 位值。
 */
void write_le16(std::uint8_t* destination, std::uint16_t value) noexcept;

/**
 * @brief 将 32 位无符号值以小端序写入目标地址。
 * @param destination 目标地址。
 * @param value 待写入的 32 位值。
 */
void write_le32(std::uint8_t* destination, std::uint32_t value) noexcept;

/**
 * @brief 从小端字节序读取 16 位无符号值。
 * @param source 源地址。
 * @return 读取到的 16 位值。
 */
[[nodiscard]] std::uint16_t read_le16(const std::uint8_t* source) noexcept;

/**
 * @brief 从小端字节序读取 32 位无符号值。
 * @param source 源地址。
 * @return 读取到的 32 位值。
 */
[[nodiscard]] std::uint32_t read_le32(const std::uint8_t* source) noexcept;

/**
 * @brief 计算 CRC16-XMODEM（初值 0，poly 0x1021）。
 * @param source 数据指针。
 * @param size 数据长度。
 * @return CRC16-XMODEM 值。
 */
[[nodiscard]] std::uint16_t crc16_xmodem(const std::uint8_t* source, std::size_t size) noexcept;

/**
 * @brief 计算 Frame 的 Byte0–39 CRC，不读取其现有 CRC 字段。
 * @param frame 数据帧。
 * @return CRC16-XMODEM 值。
 */
[[nodiscard]] std::uint16_t crc16_xmodem(const Frame& frame) noexcept;

/**
 * @brief 将 Byte0–39 的 CRC 写入 Byte40–41。
 * @param frame 数据帧。
 */
void fill_crc(Frame& frame) noexcept;

/**
 * @brief 验证 Byte40–41 是否等于 Byte0–39 的 CRC16-XMODEM。
 * @param frame 数据帧。
 * @return true=CRC 正确，false=CRC 错误。
 */
[[nodiscard]] bool has_valid_crc(const Frame& frame) noexcept;

/**
 * @brief LoRa 通信参数帧。
 *
 * 请求使用 ReadParamReq/WriteParamReq，响应使用对应 Rsp 命令。
 * 读取请求的参数区应置零；写请求和响应使用完整的 Byte11–38 参数区。
 */
struct LoRaParamFrame final {
    std::uint8_t cmd{};                       // Byte0: SystemCommand 指令码
    std::uint32_t transaction_id{};           // Byte1–4: 非零事务 ID，响应回显
    std::uint16_t object_index{};             // Byte5–6: 低 12 位对象，高 4 位 SDO 状态
    std::uint32_t object_data{};              // Byte7–10: SDO 数据；无线参数操作为 0
    std::uint16_t param_flags{};              // Byte11–12: ParamFlags 原始位图
    std::int16_t tx_power{};                  // Byte13–14: 发射功率，单位 dBm
    std::uint16_t freq_offset{};              // Byte15–16: 信道中心频率偏移，单位 kHz
    std::uint8_t payload_len{kPayloadLen};    // Byte17: 负载字节长度，固定为 12
    std::uint8_t rssi_threshold{};            // Byte18: 信号接收强度门槛的协议 raw 值
    std::uint16_t heartbeat_interval{};       // Byte19–20: 心跳包间隔，单位 ms
    std::uint8_t heartbeat_loss{};            // Byte21: 心跳丢失判断门槛
    std::uint8_t bandwidth{};                 // Byte22: 接收带宽，0=125k，1=250k，2=500k
    std::uint8_t spreading_factor{};          // Byte23: 扩频因子，SF5–SF12 编码为 5–12
    std::uint8_t coding_rate{};               // Byte24: 编码率，0–6
    std::uint8_t header_type{};               // Byte25: 报头类型，0=显式，1=隐式
    std::uint8_t preamble_len{};              // Byte26: 前导码长度，单位 symbol
    std::uint16_t sync_word{};                // Byte27–28: 同步字；常用值 0x1424
    std::array<std::uint8_t, 10> reserved{};  // Byte29–38: 预留，固定 0
    std::uint8_t result_code{};               // Byte39: ResultCode 结果码
    std::uint16_t crc16{};                    // Byte40–41: 小端 CRC16-XMODEM

    /**
     * @brief 序列化并用 Byte0–39 自动计算并填写 CRC 字段。
     * @return 42 字节数据帧。
     */
    [[nodiscard]] Frame to_frame() const noexcept;

    /**
     * @brief 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。
     * @param frame 42 字节数据帧。
     * @return 解析后的 LoRaParamFrame 对象。
     */
    [[nodiscard]] static LoRaParamFrame from_frame(const Frame& frame) noexcept;
};

/**
 * @brief GFSK 通信参数帧。
 *
 * Byte0–21、Byte35–41 与 LoRaParamFrame 相同；Byte22–34 的含义按 GFSK
 * 解释，其中码率和频偏均以 32 位物理单位的小端序传输。
 */
struct GfskParamFrame final {
    std::uint8_t cmd{};                      // Byte0: SystemCommand 指令码
    std::uint32_t transaction_id{};          // Byte1–4: 非零事务 ID，响应回显
    std::uint16_t object_index{};            // Byte5–6: 低 12 位对象，高 4 位 SDO 状态
    std::uint32_t object_data{};             // Byte7–10: SDO 数据；无线参数操作为 0
    std::uint16_t param_flags{};             // Byte11–12: ParamFlags 原始位图
    std::int16_t tx_power{};                 // Byte13–14: 发射功率，单位 dBm
    std::uint16_t freq_offset{};             // Byte15–16: 信道中心频率偏移，单位 kHz
    std::uint8_t payload_len{kPayloadLen};   // Byte17: 负载字节长度，固定为 12
    std::uint8_t rssi_threshold{};           // Byte18: 信号接收强度门槛的协议 raw 值
    std::uint16_t heartbeat_interval{};      // Byte19–20: 心跳包间隔，单位 ms
    std::uint8_t heartbeat_loss{};           // Byte21: 心跳丢失判断门槛
    std::uint8_t bandwidth{};                // Byte22: GFSK 接收带宽协议值
    std::uint32_t bitrate{};                 // Byte23–26: 码率，单位 bps
    std::uint32_t freq_deviation{};          // Byte27–30: 频偏，单位 Hz
    std::uint8_t pulse_shaping{};            // Byte31: 脉冲整形；默认值常为 0x09
    std::uint8_t preamble_len{};             // Byte32: 前导码长度，单位 bit
    std::uint16_t sync_word{};               // Byte33–34: 同步字；常用值 0x1424
    std::array<std::uint8_t, 4> reserved{};  // Byte35–38: 预留，固定 0
    std::uint8_t result_code{};              // Byte39: ResultCode 结果码
    std::uint16_t crc16{};                   // Byte40–41: 小端 CRC16-XMODEM

    /**
     * @brief 序列化并用 Byte0–39 自动计算并填写 CRC 字段。
     * @return 42 字节数据帧。
     */
    [[nodiscard]] Frame to_frame() const noexcept;

    /**
     * @brief 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。
     * @param frame 42 字节数据帧。
     * @return 解析后的 GfskParamFrame 对象。
     */
    [[nodiscard]] static GfskParamFrame from_frame(const Frame& frame) noexcept;
};

/**
 * @brief PIN 配置帧。
 *
 * Byte1–6 是六个 ASCII 数字。请求时 ASCII "000000" 表示读取当前 PIN；
 * 写入时必须使用其他六位数字，Client 会拒绝保留值。
 */
struct PinFrame final {
    SystemCommand cmd{SystemCommand::PinConfigReq};  // Byte0: PinConfigReq 或 PinConfigRsp
    std::array<std::uint8_t, kPinLength> pin{};      // Byte1–6: ASCII PIN
    std::array<std::uint8_t, 5> reserved1{};         // Byte7–11: 预留，固定 0
    std::uint32_t transaction_id{};                  // Byte12–15: 事务 ID，响应回显
    std::array<std::uint8_t, 23> reserved2{};        // Byte16–38: 预留，固定 0
    ResultCode result_code{ResultCode::Success};     // Byte39: 结果码
    std::uint16_t crc16{};                           // Byte40–41: 小端 CRC16-XMODEM

    /**
     * @brief 序列化并用 Byte0–39 自动计算并填写 CRC 字段。
     * @return 42 字节数据帧。
     */
    [[nodiscard]] Frame to_frame() const noexcept;

    /**
     * @brief 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。
     * @param frame 42 字节数据帧。
     * @return 解析后的 PinFrame 对象。
     */
    [[nodiscard]] static PinFrame from_frame(const Frame& frame) noexcept;

    /**
     * @brief 从字符串设置 PIN。
     * @param value 恰好 6 位 ASCII 数字。
     * @return true=成功，false=长度或字符非法。
     */
    [[nodiscard]] bool set_pin(std::string_view value) noexcept;

    /**
     * @brief 获取 PIN 字符串。
     * @return 6 位 ASCII 数字字符串。
     */
    [[nodiscard]] std::string get_pin() const;
};

/**
 * @brief 绑定、解绑和寻机使用的设备密钥帧。
 *
 * 该类型只定义线格式；当前 Client 不暴露绑定、解绑或寻机业务操作。
 * 调用方不得将 kbind 写入日志或错误信息。
 */
struct DeviceKeyFrame final {
    SystemCommand cmd{SystemCommand::BindReq};  // Byte0: 绑定、解绑或寻机命令
    std::array<std::uint8_t, 3> device_id{};    // Byte1–3: 急停盒设备 ID
    std::array<std::uint8_t, 16> kbind{};     // Byte4–19: 16 字节绑定密钥（敏感数据）
    std::uint32_t transaction_id{};           // Byte20–23: 绑定事务 ID，响应回显
    std::array<std::uint8_t, 15> reserved{};  // Byte24–38: 预留，固定 0
    ResultCode result_code{ResultCode::Success};  // Byte39: 结果码
    std::uint16_t crc16{};                        // Byte40–41: 小端 CRC16-XMODEM

    /**
     * @brief 序列化并用 Byte0–39 自动计算并填写 CRC 字段。
     * @return 42 字节数据帧。
     */
    [[nodiscard]] Frame to_frame() const noexcept;

    /**
     * @brief 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。
     * @param frame 42 字节数据帧。
     * @return 解析后的 DeviceKeyFrame 对象。
     */
    [[nodiscard]] static DeviceKeyFrame from_frame(const Frame& frame) noexcept;

    /**
     * @brief 清零敏感数据（kbind、device_id、transaction_id）。
     */
    void wipe() noexcept;
};

/**
 * @brief 正常通信运行状态帧。
 *
 * 查询请求除 cmd 外全部置零；响应的 Byte1–3 返回无线急停盒 Device ID。
 * 该帧没有事务 ID，不与参数/SDO 帧共用字段语义。
 */
struct NormalFrame final {
    SystemCommand cmd{SystemCommand::RunStatusReq};  // Byte0: RunStatusReq 或 RunStatusRsp
    std::array<std::uint8_t, 3> device_id{};         // Byte1–3: 无线急停盒 Device ID
    std::uint32_t wireless_counter{};                // Byte4–7: 最近无线动态计数器
    std::uint16_t receiver_status{};                 // Byte8–9: 接收板状态字
    std::uint8_t rssi{};           // Byte10: RSSI 协议 raw 值，实际 RSSI = -raw dBm
    std::int16_t snr{};            // Byte11–12: SNR 协议 raw 值，实际 SNR = raw * 0.25 dB
    std::uint8_t control{};        // Byte13: 控制字
    std::uint16_t object_index{};  // Byte14–15: SDO 对象及状态
    std::uint32_t object_data{};   // Byte16–19: SDO 数据
    std::uint32_t warning_code{};  // Byte20–23: 警告码
    std::uint32_t error_code{};    // Byte24–27: 错误码
    std::array<std::uint8_t, 11> reserved{};      // Byte28–38: 预留，固定 0
    ResultCode result_code{ResultCode::Success};  // Byte39: 结果码
    std::uint16_t crc16{};                        // Byte40–41: 小端 CRC16-XMODEM

    /**
     * @brief 序列化并用 Byte0–39 自动计算并填写 CRC 字段。
     * @return 42 字节数据帧。
     */
    [[nodiscard]] Frame to_frame() const noexcept;

    /**
     * @brief 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。
     * @param frame 42 字节数据帧。
     * @return 解析后的 NormalFrame 对象。
     */
    [[nodiscard]] static NormalFrame from_frame(const Frame& frame) noexcept;
};

/**
 * @brief 稀疏 SDO 帧。
 *
 * SDO 的对象索引占 Byte5–6：低 12 位为对象地址，高 4 位为 SdoStatus。
 * Byte11–38 在原协议中没有单独的 SDO 语义；Client 生成请求时置零，
 * 接收响应时原样保留而不据此拒绝有效 SDO 结果。
 */
struct SdoFrame final {
    SystemCommand cmd{SystemCommand::ReadParamReq};  // Byte0: 参数读/写命令
    std::uint32_t transaction_id{};                  // Byte1–4: 非零事务 ID，响应回显
    std::uint16_t object_index{};             // Byte5–6: 低 12 位对象，高 4 位 SDO 状态
    std::uint32_t object_data{};              // Byte7–10: 对象读回值或待写值
    std::array<std::uint8_t, 28> reserved{};  // Byte11–38: SDO 未解释载荷
    ResultCode result_code{ResultCode::Success};  // Byte39: 结果码
    std::uint16_t crc16{};                        // Byte40–41: 小端 CRC16-XMODEM

    /**
     * @brief 序列化并用 Byte0–39 自动计算并填写 CRC 字段。
     * @return 42 字节数据帧。
     */
    [[nodiscard]] Frame to_frame() const noexcept;

    /**
     * @brief 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。
     * @param frame 42 字节数据帧。
     * @return 解析后的 SdoFrame 对象。
     */
    [[nodiscard]] static SdoFrame from_frame(const Frame& frame) noexcept;
};

// 协议定义的稳定 SDO 对象索引
namespace sdo {
constexpr std::uint16_t kAddressMask = 0x0FFF;
constexpr unsigned kStatusShift = 12;

constexpr std::uint16_t ProductCode = 0x001;     // 产品编码
constexpr std::uint16_t VersionNumber = 0x002;   // 固件版本号
constexpr std::uint16_t SerialNumber = 0x003;    // 设备序列号
constexpr std::uint16_t BatteryLevel = 0x102;    // 电量，单位 1%
constexpr std::uint16_t UpgradeRequest = 0x202;  // 固件升级请求标志

/**
 * @brief 提取 Object Index 中的 SDO 状态（高 4 位）。
 * @param object_index 16 位 Object Index。
 * @return SDO 状态。
 */
[[nodiscard]] constexpr SdoStatus status(std::uint16_t object_index) noexcept {
    return static_cast<SdoStatus>((object_index >> kStatusShift) & 0xF);
}

/**
 * @brief 提取 Object Index 中的对象地址（低 12 位）。
 * @param object_index 16 位 Object Index。
 * @return 12 位对象地址。
 */
[[nodiscard]] constexpr std::uint16_t object_address(std::uint16_t object_index) noexcept {
    return object_index & kAddressMask;
}

/**
 * @brief 组合 SDO 状态与对象地址为 Object Index。
 * @param status SDO 状态（高 4 位）。
 * @param object_address 对象地址（低 12 位）。
 * @return 16 位 Object Index。
 */
[[nodiscard]] constexpr std::uint16_t make_index(
    SdoStatus status, std::uint16_t object_address) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(status) << kStatusShift) | (object_address & kAddressMask));
}
}  // namespace sdo

/**
 * @brief Client 支持的 SDO 对象集合。
 */
enum class SdoObject : std::uint16_t {
    ProductCode = sdo::ProductCode,
    VersionNumber = sdo::VersionNumber,
    SerialNumber = sdo::SerialNumber,
    Battery = sdo::BatteryLevel,
    UpgradeRequest = sdo::UpgradeRequest,
};

}  // namespace transmitter