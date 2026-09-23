#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace transmitter {

/** 设备串口协议中每帧的固定长度（Byte0 至 Byte41）。 */
constexpr std::size_t kFrameSize = 42;
/** CRC 覆盖的载荷范围：Byte0 至 Byte39。 */
constexpr std::size_t kFrameBodySize = 40;
/** CRC16-XMODEM 占用 Byte40 至 Byte41，按小端序传输。 */
constexpr std::size_t kFrameCrcSize = 2;

/**
 * @brief 42 字节的原始线格式帧。
 *
 * 该类型不依赖 C++ 结构体内存布局。所有协议帧均显式转换为 Bytes，
 * 因而可安全地用于串口读写和逐字节协议检查。
 */
struct Bytes final {
    std::array<std::uint8_t, kFrameSize> data{};

    [[nodiscard]] std::uint8_t* bytes() noexcept { return data.data(); }
    [[nodiscard]] const std::uint8_t* bytes() const noexcept { return data.data(); }
    [[nodiscard]] std::uint8_t& operator[](std::size_t index) noexcept { return data[index]; }
    [[nodiscard]] const std::uint8_t& operator[](std::size_t index) const noexcept {
        return data[index];
    }
    [[nodiscard]] static constexpr std::size_t size() noexcept { return kFrameSize; }
};
static_assert(sizeof(Bytes) == kFrameSize, "Bytes must be exactly 42 bytes");

/** Byte0 中的系统命令；响应命令的最高位置位。 */
enum class SystemCmd : std::uint8_t {
    BindReq = 0x01,        // 绑定请求。
    UnbindReq = 0x02,      // 解绑请求。
    FindReq = 0x03,        // 寻机请求。
    ParamReadReq = 0x04,   // 读取通信参数或 SDO 对象请求。
    ParamWriteReq = 0x05,  // 写入通信参数或 SDO 对象请求。
    NormalReq = 0x06,      // 正常通信（运行状态）请求。
    PinCfgReq = 0x07,      // PIN 配置（ASCII "000000" 表示读取）请求。
    BindRsp = 0x81,        // 绑定响应。
    UnbindRsp = 0x82,      // 解绑响应。
    FindRsp = 0x83,        // 寻机响应。
    ParamReadRsp = 0x84,   // 通信参数或 SDO 对象读取响应。
    ParamWriteRsp = 0x85,  // 通信参数或 SDO 对象写入响应。
    NormalRsp = 0x86,      // 正常通信（运行状态）响应。
    PinCfgRsp = 0x87,      // PIN 配置响应。
};

/** Byte39 的设备处理结果。 */
enum class ResultCode : std::uint8_t {
    Success = 0x00,  // 成功
    Failure = 0xff   // 失败
};

/** object_index 高 4 位编码的 SDO 执行状态。 */
enum class SdoStatus : std::uint8_t {
    NotReceived = 0x0,   // 从机尚未接收命令。
    ReadSuccess = 0x4,   // 读命令执行成功。
    WriteSuccess = 0x6,  // 写命令执行成功。
    InProgress = 0xb,    // 命令正在执行。
    Error = 0x8,         // 从机执行失败。
    InvalidCmd = 0xe,    // 从机不支持该命令。
};

/** 参数标志 bit15..14 指定的物理层类型。 */
enum class RadioType : std::uint8_t {
    LoRa = 0,  // Lora
    GFSK = 1   // GFSK
};

/** 参数标志 bit0 指定的工作频段。 */
enum class Band : std::uint8_t {
    MHz433 = 0,  // 433 MHz
    MHz915 = 1   // 915 MHz
};

/**
 * @brief 参数帧 Byte11–12 的可读位域表示。
 *
 * 协议中的 crc、急停、心跳和组网模式位均为“0 表示启用”，因此布尔字段
 * 使用正向语义；调用 to_raw()/from_raw() 进行转换，不应直接依赖位值。
 */
struct ParamFlags final {
    Band band{Band::MHz433};                // bit0：0=433 MHz，1=915 MHz。
    bool crc_enabled{true};                 // bit1：0=启用 PHY CRC，1=关闭。
    bool estop_enabled{true};               // bit2：0=启用无线急停，1=关闭。
    bool heartbeat_enabled{true};           // bit3：0=启用心跳，1=关闭。
    bool one_to_one{true};                  // bit4：0=一对一，1=一对多。
    bool channel_scan{false};               // bit5：0=单信道，1=跳频。
    RadioType radio_type{RadioType::LoRa};  // bit15..14：00=LoRa，01=GFSK。

    /** 将语义化字段编码为协议原始位图。 */
    [[nodiscard]] std::uint16_t to_raw() const noexcept;
    /**
     * 从协议原始位图解码语义化字段；保留无线类型会映射为 LoRa。
     * 调用方处理线上输入前仍须验证 bit15..14 和 bit13..6。
     */
    [[nodiscard]] static ParamFlags from_raw(std::uint16_t raw_flags) noexcept;
};

/** 将 16 位无符号值以小端序写入目标地址。 */
void write_le16(std::uint8_t* destination, std::uint16_t value) noexcept;
/** 将 32 位无符号值以小端序写入目标地址。 */
void write_le32(std::uint8_t* destination, std::uint32_t value) noexcept;
/** 从小端字节序读取 16 位无符号值。 */
[[nodiscard]] std::uint16_t read_le16(const std::uint8_t* source) noexcept;
/** 从小端字节序读取 32 位无符号值。 */
[[nodiscard]] std::uint32_t read_le32(const std::uint8_t* source) noexcept;
/** 计算 CRC16-XMODEM（初值 0，poly 0x1021）。 */
[[nodiscard]] std::uint16_t crc16_xmodem(const std::uint8_t* source, std::size_t size) noexcept;
/** 计算 Bytes 的 Byte0–39 CRC，不读取其现有 CRC 字段。 */
[[nodiscard]] std::uint16_t crc16_xmodem(const Bytes& bytes) noexcept;
/** 将 Byte0–39 的 CRC 写入 Byte40–41。 */
void fill_crc(Bytes& bytes) noexcept;
/** 验证 Byte40–41 是否等于 Byte0–39 的 CRC16-XMODEM。 */
[[nodiscard]] bool has_valid_crc(const Bytes& bytes) noexcept;

/**
 * @brief LoRa 通信参数帧。
 *
 * 请求使用 ParamReadReq/ParamWriteReq，响应使用对应 Rsp 命令。读取请求
 * 的参数区应置零；写请求和响应使用完整的 Byte11–38 参数区。
 */
struct LoRaParamFrame final {
    std::uint8_t cmd{};              // Byte0：参数读/写命令。
    std::uint32_t transaction_id{};  // Byte1–4：非零事务 ID，响应回显。
    std::uint16_t object_index{};    // Byte5–6：低 12 位对象，高 4 位 SDO 状态。
    std::uint32_t object_data{};     // Byte7–10：SDO 数据；无线参数操作为 0。
    std::uint16_t param_flags{};     // Byte11–12：ParamFlags 原始位图，类型为 LoRa。
    std::int16_t tx_power{};         // Byte13–14：发射功率，单位 dBm。
    std::uint16_t freq_offset{};     // Byte15–16：中心频率偏移，单位 kHz。
    std::uint8_t payload_len{};      // Byte17：无线负载长度，固定为 12。
    std::uint8_t rssi_threshold{};   // Byte18：RSSI 接收门限的协议值。
    std::uint16_t heartbeat_interval{};       // Byte19–20：心跳周期，单位 ms。
    std::uint8_t heartbeat_loss{};            // Byte21：允许连续丢失的心跳数量。
    std::uint8_t bandwidth{};                 // Byte22：0=125k，1=250k，2=500k。
    std::uint8_t spreading_factor{};          // Byte23：SF5–SF12 分别编码为 5–12。
    std::uint8_t coding_rate{};               // Byte24：LoRa 编码率协议值 0–6。
    std::uint8_t header_type{};               // Byte25：0=显式报头，1=隐式报头。
    std::uint8_t preamble_len{};              // Byte26：前导码长度，单位 symbol。
    std::uint16_t sync_word{};                // Byte27–28：同步字；常用值 0x1424。
    std::array<std::uint8_t, 10> reserved{};  // Byte29–38：保留，应为 0。
    std::uint8_t result_code{};               // Byte39：ResultCode。
    std::uint16_t crc16{};                    // Byte40–41：小端 CRC16-XMODEM。
    /** 序列化并用 Byte0–39 自动计算并填写 CRC 字段。 */
    [[nodiscard]] Bytes to_bytes() const noexcept;
    /** 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。 */
    [[nodiscard]] static LoRaParamFrame from_bytes(const Bytes&) noexcept;
};

/**
 * @brief GFSK 通信参数帧。
 *
 * Byte0–21、Byte35–41 与 LoRaParamFrame 相同；Byte22–34 的含义按 GFSK
 * 解释，其中码率和频偏均以 32 位物理单位的小端序传输。
 */
struct GfskParamFrame final {
    std::uint8_t cmd{};              // Byte0：参数读/写命令。
    std::uint32_t transaction_id{};  // Byte1–4：非零事务 ID，响应回显。
    std::uint16_t object_index{};    // Byte5–6：低 12 位对象，高 4 位 SDO 状态。
    std::uint32_t object_data{};     // Byte7–10：SDO 数据；无线参数操作为 0。
    std::uint16_t param_flags{};     // Byte11–12：ParamFlags 原始位图，类型为 GFSK。
    std::int16_t tx_power{};         // Byte13–14：发射功率，单位 dBm。
    std::uint16_t freq_offset{};     // Byte15–16：中心频率偏移，单位 kHz。
    std::uint8_t payload_len{};      // Byte17：无线负载长度，固定为 12。
    std::uint8_t rssi_threshold{};   // Byte18：RSSI 接收门限的协议值。
    std::uint16_t heartbeat_interval{};      // Byte19–20：心跳周期，单位 ms。
    std::uint8_t heartbeat_loss{};           // Byte21：允许连续丢失的心跳数量。
    std::uint8_t bandwidth{};                // Byte22：GFSK 带宽协议值。
    std::uint32_t bitrate{};                 // Byte23–26：码率，单位 bps。
    std::uint32_t freq_deviation{};          // Byte27–30：频偏，单位 Hz。
    std::uint8_t pulse_shaping{};            // Byte31：脉冲整形；默认值常为 0x09。
    std::uint8_t preamble_len{};             // Byte32：前导码长度，单位 bit。
    std::uint16_t sync_word{};               // Byte33–34：同步字；常用值 0x1424。
    std::array<std::uint8_t, 4> reserved{};  // Byte35–38：保留，应为 0。
    std::uint8_t result_code{};              // Byte39：ResultCode。
    std::uint16_t crc16{};                   // Byte40–41：小端 CRC16-XMODEM。
    /** 序列化并用 Byte0–39 自动计算并填写 CRC 字段。 */
    [[nodiscard]] Bytes to_bytes() const noexcept;
    /** 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。 */
    [[nodiscard]] static GfskParamFrame from_bytes(const Bytes&) noexcept;
};

/**
 * @brief PIN 配置帧。
 *
 * Byte1–6 是六个 ASCII 数字。请求时 ASCII "000000" 表示读取当前 PIN；
 * 写入时必须使用其他六位数字，Client 会拒绝保留值。
 */
struct PinFrame final {
    std::uint8_t cmd{};                 // Byte0：PinCfgReq 或 PinCfgRsp。
    std::array<std::uint8_t, 6> pin{};  // Byte1–6：ASCII PIN；"000000" 用于读取请求。
    std::array<std::uint8_t, 5> reserved1{};   // Byte7–11：保留，应为 0。
    std::uint32_t transaction_id{};            // Byte12–15：事务 ID，响应回显。
    std::array<std::uint8_t, 23> reserved2{};  // Byte16–38：保留，应为 0。
    std::uint8_t result_code{};                // Byte39：ResultCode。
    std::uint16_t crc16{};                     // Byte40–41：小端 CRC16-XMODEM。
    /** 序列化并用 Byte0–39 自动计算并填写 CRC 字段。 */
    [[nodiscard]] Bytes to_bytes() const noexcept;
    /** 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。 */
    [[nodiscard]] static PinFrame from_bytes(const Bytes&) noexcept;
};

/**
 * @brief 绑定、解绑和寻机使用的设备密钥帧。
 *
 * 该类型只定义线格式；当前 Client 不暴露绑定、解绑或寻机业务操作。
 * 调用方不得将 kbind 写入日志或错误信息。
 */
struct DeviceKeyFrame final {
    std::uint8_t cmd{};                       // Byte0：绑定、解绑或寻机命令。
    std::array<std::uint8_t, 3> device_id{};  // Byte1–3：急停盒设备 ID。
    std::array<std::uint8_t, 16> kbind{};  // Byte4–19：16 字节绑定密钥（敏感数据）。
    std::uint32_t transaction_id{};        // Byte20–23：绑定事务 ID，响应回显。
    std::array<std::uint8_t, 15> reserved{};  // Byte24–38：保留，应为 0。
    std::uint8_t result_code{};               // Byte39：ResultCode。
    std::uint16_t crc16{};                    // Byte40–41：小端 CRC16-XMODEM。
    /** 序列化并用 Byte0–39 自动计算并填写 CRC 字段。 */
    [[nodiscard]] Bytes to_bytes() const noexcept;
    /** 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。 */
    [[nodiscard]] static DeviceKeyFrame from_bytes(const Bytes&) noexcept;
};

/**
 * @brief 正常通信运行状态帧。
 *
 * 查询请求除 cmd 外全部置零；响应的 Byte1–3 返回无线急停盒 Device ID。
 * 该帧没有事务 ID，不与参数/SDO 帧共用字段语义。
 */
struct NormalFrame final {
    std::uint8_t cmd{};                       // Byte0：NormalReq 或 NormalRsp。
    std::array<std::uint8_t, 3> device_id{};  // Byte1–3：无线急停盒 Device ID。
    std::uint32_t wireless_counter{};         // Byte4–7：最近无线动态计数器。
    std::uint16_t receiver_status{};          // Byte8–9：接收板状态字。
    std::uint8_t rssi{};                      // Byte10：RSSI 的负值。
    std::int16_t snr{};                       // Byte11–12：SNR，系数 0.25 dB。
    std::uint8_t control{};                   // Byte13：控制字。
    std::uint16_t object_index{};             // Byte14–15：SDO 对象及状态。
    std::uint32_t object_data{};              // Byte16–19：SDO 数据。
    std::uint32_t warning_code{};             // Byte20–23：警告码。
    std::uint32_t error_code{};               // Byte24–27：错误码。
    std::array<std::uint8_t, 11> reserved{};  // Byte28–38：保留。
    std::uint8_t result_code{};               // Byte39：ResultCode。
    std::uint16_t crc16{};                    // Byte40–41：小端 CRC16-XMODEM。
    [[nodiscard]] Bytes to_bytes() const noexcept;
    [[nodiscard]] static NormalFrame from_bytes(const Bytes&) noexcept;
};

/**
 * @brief 稀疏 SDO 帧。
 *
 * SDO 的对象索引占 Byte5–6：低 12 位为对象地址，高 4 位为 SdoStatus。
 * Byte11–38 在原协议中没有单独的 SDO 语义；Client 生成请求时置零，
 * 接收响应时原样保留而不据此拒绝有效 SDO 结果。
 */
struct SdoFrame final {
    std::uint8_t cmd{};              // Byte0：参数读/写命令。
    std::uint32_t transaction_id{};  // Byte1–4：非零事务 ID，响应回显。
    std::uint16_t object_index{};    // Byte5–6：低 12 位对象，高 4 位 SDO 状态。
    std::uint32_t object_data{};     // Byte7–10：对象读回值或待写值。
    std::array<std::uint8_t, 28> reserved{};  // Byte11–38：SDO 未解释载荷。
    std::uint8_t result_code{};               // Byte39：ResultCode。
    std::uint16_t crc16{};                    // Byte40–41：小端 CRC16-XMODEM。
    /** 序列化并用 Byte0–39 自动计算并填写 CRC 字段。 */
    [[nodiscard]] Bytes to_bytes() const noexcept;
    /** 原样反序列化；如需完整性校验，请先调用 has_valid_crc()。 */
    [[nodiscard]] static SdoFrame from_bytes(const Bytes&) noexcept;
};

/** 协议定义的稳定 SDO 对象索引，供底层实现和调用方复用。 */
namespace sdo {
constexpr std::uint16_t ProductCode = 0x001;     // 产品编码。
constexpr std::uint16_t VersionNumber = 0x002;   // 固件版本号。
constexpr std::uint16_t SerialNumber = 0x003;    // 设备序列号。
constexpr std::uint16_t BatteryLevel = 0x102;    // 电量，单位 1%。
constexpr std::uint16_t UpgradeRequest = 0x202;  // 固件升级请求标志。
}  // namespace sdo
/** Client 支持的 SDO 对象集合。 */
enum class SdoObject : std::uint16_t {
    ProductCode = sdo::ProductCode,
    VersionNumber = sdo::VersionNumber,
    SerialNumber = sdo::SerialNumber,
    Battery = sdo::BatteryLevel,
    UpgradeRequest = sdo::UpgradeRequest,
};

}  // namespace transmitter
