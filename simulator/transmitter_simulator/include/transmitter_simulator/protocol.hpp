#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace transmitter_simulator {

/** Fixed USB serial frame size: Byte0 through Byte41. */
constexpr std::size_t kFrameSize = 42;
/** CRC16-XMODEM input range: Byte0 through Byte39. */
constexpr std::size_t kFrameBodySize = 40;
constexpr std::size_t kFrameCrcSize = 2;
// Internal compatibility names. New code must use the PascalCase constants above.
constexpr std::size_t k_frame_size = kFrameSize;
constexpr std::size_t k_frame_body_size = kFrameBodySize;

/** Layout-independent raw wire bytes for public protocol APIs. */
struct Bytes final {
    std::array<std::uint8_t, kFrameSize> data{};

    [[nodiscard]] std::uint8_t* bytes() noexcept { return data.data(); }
    [[nodiscard]] const std::uint8_t* bytes() const noexcept { return data.data(); }
    [[nodiscard]] std::uint8_t& operator[](std::size_t index) noexcept {
        assert(index < kFrameSize);
        return data[index];
    }
    [[nodiscard]] const std::uint8_t& operator[](std::size_t index) const noexcept {
        assert(index < kFrameSize);
        return data[index];
    }
    [[nodiscard]] static constexpr std::size_t size() noexcept { return kFrameSize; }
    [[nodiscard]] std::uint16_t read_le16(std::size_t offset) const noexcept;
    [[nodiscard]] std::uint32_t read_le32(std::size_t offset) const noexcept;
    void write_le16(std::size_t offset, std::uint16_t value) noexcept;
    void write_le32(std::size_t offset, std::uint32_t value) noexcept;
    /** Computes CRC16-XMODEM over the first byte_count bytes. */
    [[nodiscard]] std::uint16_t crc16_xmodem(
        std::size_t byte_count = kFrameBodySize) const noexcept;
    void fill_crc() noexcept;
    [[nodiscard]] bool has_valid_crc() const noexcept;
    [[nodiscard]] bool is_zero(std::size_t offset, std::size_t count) const noexcept;
};
static_assert(sizeof(Bytes) == kFrameSize, "Bytes must be exactly one wire frame");

enum class SystemCmd : std::uint8_t {
    ParamReadReq = 0x04,
    ParamWriteReq = 0x05,
    NormalReq = 0x06,
    PinCfgReq = 0x07,
    ParamReadRsp = 0x84,
    ParamWriteRsp = 0x85,
    NormalRsp = 0x86,
    PinCfgRsp = 0x87,
};

enum class ResultCode : std::uint8_t { Success = 0x00, Failure = 0xff };

enum class RadioType : std::uint8_t { LoRa = 0, Gfsk = 1 };
enum class Band : std::uint8_t { MHz433 = 0, MHz915 = 1 };
enum class SdoStatus : std::uint8_t {
    NotReceived = 0x0,
    ReadSuccess = 0x4,
    WriteSuccess = 0x6,
    Error = 0x8,
    InProgress = 0xb,
    InvalidCommand = 0xe,
};

struct ParamFlags final {
    RadioType radio_type{RadioType::LoRa};
    Band band{Band::MHz433};
    bool crc_enabled{true};
    bool estop_enabled{true};
    bool heartbeat_enabled{true};
    bool one_to_one{true};
    bool channel_scan{};
    [[nodiscard]] std::uint16_t to_raw() const noexcept;
    [[nodiscard]] static bool from_raw(std::uint16_t raw, ParamFlags& flags) noexcept;
};

struct LoraConfig final {
    ParamFlags flags{};
    std::int16_t tx_power{};
    std::uint16_t frequency_offset{};
    std::uint8_t payload_length{};
    std::uint8_t rssi_threshold{};
    std::uint16_t heartbeat_interval{};
    std::uint8_t heartbeat_loss{};
    std::uint8_t bandwidth{};
    std::uint8_t spreading_factor{};
    std::uint8_t coding_rate{};
    std::uint8_t header_type{};
    std::uint8_t preamble_length{};
    std::uint16_t sync_word{};
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] static LoraConfig defaults() noexcept;
};

struct GfskConfig final {
    ParamFlags flags{RadioType::Gfsk};
    std::int16_t tx_power{};
    std::uint16_t frequency_offset{};
    std::uint8_t payload_length{};
    std::uint8_t rssi_threshold{};
    std::uint16_t heartbeat_interval{};
    std::uint8_t heartbeat_loss{};
    std::uint8_t bandwidth{};
    std::uint32_t bitrate{};
    std::uint32_t frequency_deviation{};
    std::uint8_t pulse_shaping{};
    std::uint8_t preamble_length{};
    std::uint16_t sync_word{};
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] static GfskConfig defaults() noexcept;
};

/** Complete LoRa parameter frame with explicit wire serialization. */
struct LoRaParamFrame final {
    SystemCmd command{SystemCmd::ParamReadReq};
    std::uint32_t transaction_id{};
    std::uint16_t object_index{};
    std::uint32_t object_data{};
    LoraConfig config{};
    std::array<std::uint8_t, 10> reserved{};
    ResultCode result{ResultCode::Success};

    [[nodiscard]] Bytes to_bytes() const noexcept;
    [[nodiscard]] static LoRaParamFrame from_bytes(const Bytes& bytes) noexcept;
};

/** Complete GFSK parameter frame with explicit wire serialization. */
struct GfskParamFrame final {
    SystemCmd command{SystemCmd::ParamReadReq};
    std::uint32_t transaction_id{};
    std::uint16_t object_index{};
    std::uint32_t object_data{};
    GfskConfig config{};
    std::array<std::uint8_t, 4> reserved{};
    ResultCode result{ResultCode::Success};

    [[nodiscard]] Bytes to_bytes() const noexcept;
    [[nodiscard]] static GfskParamFrame from_bytes(const Bytes& bytes) noexcept;
};

struct PinFrame final {
    SystemCmd command{SystemCmd::PinCfgReq};
    std::array<std::uint8_t, 6> pin{};
    std::uint32_t transaction_id{};
    ResultCode result{ResultCode::Success};

    [[nodiscard]] Bytes to_bytes() const noexcept;
    [[nodiscard]] static PinFrame from_bytes(const Bytes& bytes) noexcept;
    /** True only for six ASCII decimal digits. */
    [[nodiscard]] bool is_ascii_pin() const noexcept;
    /** The all-zero PIN is the protocol's read request sentinel. */
    [[nodiscard]] bool is_read_request() const noexcept;
};

struct SdoFrame final {
    SystemCmd command{SystemCmd::ParamReadReq};
    std::uint32_t transaction_id{};
    std::uint16_t object_index{};
    std::uint32_t object_data{};
    std::array<std::uint8_t, 28> reserved{};
    ResultCode result{ResultCode::Success};

    [[nodiscard]] Bytes to_bytes() const noexcept;
    [[nodiscard]] static SdoFrame from_bytes(const Bytes& bytes) noexcept;
};

namespace sdo {
constexpr std::uint16_t ProductCode = 0x001;
constexpr std::uint16_t VersionNumber = 0x002;
constexpr std::uint16_t SerialNumber = 0x003;
constexpr std::uint16_t BatteryLevel = 0x102;
constexpr std::uint16_t UpgradeRequest = 0x202;
}  // namespace sdo

}  // namespace transmitter_simulator
