#include <receiver/protocol.hpp>

namespace receiver {
namespace {
bool valid_common(
    std::uint16_t raw_flags, RadioType expected, std::int16_t tx_power, std::uint8_t payload_len,
    std::uint8_t rssi_threshold, std::uint16_t heartbeat_interval,
    std::uint8_t heartbeat_loss) noexcept {
    ParamFlags flags{};
    if (!ParamFlags::from_raw(raw_flags, flags) || flags.radio_type != expected ||
        !flags.one_to_one)
        return false;
    const auto max_power = flags.band == Band::MHz433 ? 10 : 20;
    return tx_power >= 0 && tx_power <= max_power && payload_len == 12 && rssi_threshold >= 10 &&
           rssi_threshold <= 148 && heartbeat_interval >= 200 && heartbeat_interval <= 10000 &&
           heartbeat_loss >= 1;
}
}  // namespace

std::uint16_t ParamFlags::to_raw() const noexcept {
    std::uint16_t raw = radio_type == RadioType::Gfsk ? 0x4000U : 0U;
    if (channel_scan) raw |= 0x20U;
    if (!one_to_one) raw |= 0x10U;
    if (!heartbeat_enabled) raw |= 0x08U;
    if (!estop_enabled) raw |= 0x04U;
    if (!crc_enabled) raw |= 0x02U;
    if (band == Band::MHz915) raw |= 0x01U;
    return raw;
}

bool ParamFlags::from_raw(std::uint16_t raw, ParamFlags& flags) noexcept {
    if ((raw & 0x3fc0U) != 0 || ((raw >> 14U) & 0x3U) > 1) return false;
    flags.radio_type = ((raw >> 14U) & 1U) != 0 ? RadioType::Gfsk : RadioType::LoRa;
    flags.channel_scan = (raw & 0x20U) != 0;
    flags.one_to_one = (raw & 0x10U) == 0;
    flags.heartbeat_enabled = (raw & 0x08U) == 0;
    flags.estop_enabled = (raw & 0x04U) == 0;
    flags.crc_enabled = (raw & 0x02U) == 0;
    flags.band = (raw & 0x01U) != 0 ? Band::MHz915 : Band::MHz433;
    return true;
}

bool valid(const LoRaParameters& parameters) noexcept {
    return valid_common(
               parameters.param_flags, RadioType::LoRa, parameters.tx_power, parameters.payload_len,
               parameters.rssi_threshold, parameters.heartbeat_interval,
               parameters.heartbeat_loss) &&
           parameters.bandwidth <= 2 && parameters.spreading_factor >= 5 &&
           parameters.spreading_factor <= 12 && parameters.coding_rate <= 6 &&
           parameters.header_type <= 1 && parameters.preamble_len >= 10 &&
           parameters.preamble_len <= 50 &&
           ((parameters.spreading_factor != 5 && parameters.spreading_factor != 6) ||
            parameters.preamble_len == 12) &&
           (parameters.sync_word & 0x0f0fU) == 0x0404U;
}

bool valid(const GfskParameters& parameters) noexcept {
    return valid_common(
               parameters.param_flags, RadioType::Gfsk, parameters.tx_power, parameters.payload_len,
               parameters.rssi_threshold, parameters.heartbeat_interval,
               parameters.heartbeat_loss) &&
           parameters.bandwidth <= 2 && parameters.bitrate >= 600 && parameters.bitrate <= 150000 &&
           parameters.freq_deviation >= 600 && parameters.freq_deviation <= 300000 &&
           static_cast<std::uint64_t>(parameters.freq_deviation) * 4U >= parameters.bitrate &&
           (parameters.pulse_shaping == 0 ||
            (parameters.pulse_shaping >= 0x08 && parameters.pulse_shaping <= 0x0b)) &&
           parameters.preamble_len >= 16;
}

LoRaParameters default_lora_parameters() noexcept {
    return {0x0000, 10, 250, 12, 110, 200, 3, 1, 6, 4, 0, 12, 0x1424};
}

GfskParameters default_gfsk_parameters() noexcept {
    return {0x4000, 10, 250, 12, 110, 200, 3, 1, 50000, 25000, 9, 16, 0x1424};
}

bool same_parameters(const LoRaParameters& left, const LoRaParameters& right) noexcept {
    return left.param_flags == right.param_flags && left.tx_power == right.tx_power &&
           left.freq_offset == right.freq_offset && left.payload_len == right.payload_len &&
           left.rssi_threshold == right.rssi_threshold &&
           left.heartbeat_interval == right.heartbeat_interval &&
           left.heartbeat_loss == right.heartbeat_loss && left.bandwidth == right.bandwidth &&
           left.spreading_factor == right.spreading_factor &&
           left.coding_rate == right.coding_rate && left.header_type == right.header_type &&
           left.preamble_len == right.preamble_len && left.sync_word == right.sync_word;
}
}  // namespace receiver
