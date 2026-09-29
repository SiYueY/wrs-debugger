#pragma once

#include <array>
#include <cstdint>

namespace receiver {

enum class RadioType : std::uint8_t { LoRa = 0, Gfsk = 1 };
enum class Band : std::uint8_t { MHz433 = 0, MHz915 = 1 };

struct ParamFlags final {
    Band band{Band::MHz433};
    bool crc_enabled{true};
    bool estop_enabled{true};
    bool heartbeat_enabled{true};
    bool one_to_one{true};
    bool channel_scan{false};
    RadioType radio_type{RadioType::LoRa};

    [[nodiscard]] std::uint16_t to_raw() const noexcept;
    [[nodiscard]] static bool from_raw(std::uint16_t raw, ParamFlags& flags) noexcept;
};

struct LoRaParameters final {
    std::uint16_t param_flags{};
    std::int16_t tx_power{};
    std::uint16_t freq_offset{};
    std::uint8_t payload_len{};
    std::uint8_t rssi_threshold{};
    std::uint16_t heartbeat_interval{};
    std::uint8_t heartbeat_loss{};
    std::uint8_t bandwidth{};
    std::uint8_t spreading_factor{};
    std::uint8_t coding_rate{};
    std::uint8_t header_type{};
    std::uint8_t preamble_len{};
    std::uint16_t sync_word{};
};

struct GfskParameters final {
    std::uint16_t param_flags{};
    std::int16_t tx_power{};
    std::uint16_t freq_offset{};
    std::uint8_t payload_len{};
    std::uint8_t rssi_threshold{};
    std::uint16_t heartbeat_interval{};
    std::uint8_t heartbeat_loss{};
    std::uint8_t bandwidth{};
    std::uint32_t bitrate{};
    std::uint32_t freq_deviation{};
    std::uint8_t pulse_shaping{};
    std::uint8_t preamble_len{};
    std::uint16_t sync_word{};
};

struct ReceiverInfo final {
    std::array<std::uint8_t, 3> bound_device_id{};
};

struct BindingState final {
    bool bound{};
    std::uint32_t device_id{};
};

struct ReceiverState final {
    std::uint32_t device_id{};
    std::uint16_t state{};
    std::int16_t rssi{};
    float snr{};
    std::int32_t error_code{};
    std::int32_t warning_code{};
    std::uint64_t tick{};
    std::uint32_t crc{};
};

struct SdoResponse final {
    std::uint16_t object_address{};
    std::uint32_t object_data{};
    std::uint8_t status{};
    std::uint8_t result_code{};
};

[[nodiscard]] bool valid(const LoRaParameters& parameters) noexcept;
[[nodiscard]] bool valid(const GfskParameters& parameters) noexcept;
[[nodiscard]] LoRaParameters default_lora_parameters() noexcept;
[[nodiscard]] GfskParameters default_gfsk_parameters() noexcept;
[[nodiscard]] bool same_parameters(
    const LoRaParameters& left, const LoRaParameters& right) noexcept;

}  // namespace receiver
