#pragma once

#include <array>
#include <cstdint>

namespace receiver {

/**
 * @brief 接收端 Sub-1G 参数标志。
 *
 * 对应协议 4.1.5 节 Byte11~12，与发送端使用相同的 16 位布局（小端字节序）：
 *   bit15..14  调制方式
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
     * @brief 信道扫描方式。
     */
    enum class ChannelScanMode : std::uint16_t {
        Single = 0,   // 单信道
        Hopping = 1,  // 跳信道
    };

    /**
     * @brief 调制方式。
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
     * @note 与发送端一致，保留位被忽略，调制方式的保留取值原样保留。
     */
    [[nodiscard]] static ParamFlags from_flags(std::uint16_t flags) noexcept;

    /**
     * @brief 判断原始参数标志是否可用于接收端参数操作。
     * @param flags 16 位参数标志值。
     * @return true=保留位为 0 且调制方式为 LoRa 或 GFSK，false=无效。
     */
    [[nodiscard]] static bool valid_flags(std::uint16_t flags) noexcept;

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
 * @brief 接收端 LoRa 通信参数。
 */
struct LoRaParameters final {
    std::uint16_t param_flags{};         // 参数标志原始位图
    std::int16_t tx_power{};             // 发射功率，单位 dBm
    std::uint16_t freq_offset{};         // 中心频率偏移，单位 kHz
    std::uint8_t payload_len{};          // 无线负载长度，固定为 12 字节
    std::uint8_t rssi_threshold{};       // RSSI 门槛协议值
    std::uint16_t heartbeat_interval{};  // 心跳间隔，单位 ms
    std::uint8_t heartbeat_loss{};       // 心跳丢失门槛
    std::uint8_t bandwidth{};            // 接收带宽协议值
    std::uint8_t spreading_factor{};     // 扩频因子，5~12
    std::uint8_t coding_rate{};          // 编码率协议值，0~6
    std::uint8_t header_type{};          // 报头类型，0=显式，1=隐式
    std::uint8_t preamble_len{};         // 前导码长度，单位 symbol
    std::uint16_t sync_word{};           // 同步字
};

/**
 * @brief 接收端 GFSK 通信参数。
 */
struct GfskParameters final {
    std::uint16_t param_flags{};         // 参数标志原始位图
    std::int16_t tx_power{};             // 发射功率，单位 dBm
    std::uint16_t freq_offset{};         // 中心频率偏移，单位 kHz
    std::uint8_t payload_len{};          // 无线负载长度，固定为 12 字节
    std::uint8_t rssi_threshold{};       // RSSI 门槛协议值
    std::uint16_t heartbeat_interval{};  // 心跳间隔，单位 ms
    std::uint8_t heartbeat_loss{};       // 心跳丢失门槛
    std::uint8_t bandwidth{};            // 接收带宽协议值
    std::uint32_t bitrate{};             // 码率，单位 bps
    std::uint32_t freq_deviation{};      // 频偏，单位 Hz
    std::uint8_t pulse_shaping{};        // 脉冲整形协议值
    std::uint8_t preamble_len{};         // 前导码长度，单位 bit
    std::uint16_t sync_word{};           // 同步字
};

/**
 * @brief 接收端当前绑定的设备信息。
 */
struct ReceiverInfo final {
    std::array<std::uint8_t, 3> bound_device_id{};  // 已绑定设备 ID；未绑定时全零
};

/**
 * @brief 接收端绑定状态。
 */
struct BindingState final {
    bool bound{};               // true=已绑定
    std::uint32_t device_id{};  // 24 位设备 ID
};

/**
 * @brief DDS 广播的无线急停运行状态。
 */
struct ReceiverState final {
    std::uint32_t device_id{};    // 24 位设备 ID
    std::uint16_t state{};        // 驱动报告的状态值
    std::int16_t rssi{};          // 驱动报告的 RSSI 值
    float snr{};                  // 驱动报告的 SNR 值
    std::int32_t error_code{};    // 错误码
    std::int32_t warning_code{};  // 警告码
    std::uint64_t tick{};         // 驱动报告的时间计数
    std::uint32_t crc{};          // 驱动报告的 CRC 值
};

/**
 * @brief SDO 操作响应。
 */
struct SdoResponse final {
    std::uint16_t object_address{};  // SDO 对象地址
    std::uint32_t object_data{};     // SDO 对象数据
    std::uint8_t status{};           // SDO 执行状态
    std::uint8_t result_code{};      // 设备处理结果码
};

/**
 * @brief 校验 LoRa 参数是否符合当前接收端约束。
 * @param parameters 待校验参数。
 * @return true=参数有效，false=参数无效。
 */
[[nodiscard]] bool valid(const LoRaParameters& parameters) noexcept;

/**
 * @brief 校验 GFSK 参数是否符合当前接收端约束。
 * @param parameters 待校验参数。
 * @return true=参数有效，false=参数无效。
 */
[[nodiscard]] bool valid(const GfskParameters& parameters) noexcept;

/**
 * @brief 获取默认 LoRa 通信参数。
 * @return 默认 LoRa 参数。
 */
[[nodiscard]] LoRaParameters default_lora_parameters() noexcept;

/**
 * @brief 获取默认 GFSK 通信参数。
 * @return 默认 GFSK 参数。
 */
[[nodiscard]] GfskParameters default_gfsk_parameters() noexcept;

/**
 * @brief 比较两组 LoRa 参数的全部字段。
 * @param left 第一组参数。
 * @param right 第二组参数。
 * @return true=全部字段相同，false=至少一个字段不同。
 */
[[nodiscard]] bool same_parameters(
    const LoRaParameters& left, const LoRaParameters& right) noexcept;

}  // namespace receiver
