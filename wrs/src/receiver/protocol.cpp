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

bool valid(const LoRaParameters& p) noexcept {
    return valid_common(
               p.param_flags, RadioType::LoRa, p.tx_power, p.payload_len, p.rssi_threshold,
               p.heartbeat_interval, p.heartbeat_loss) &&
           p.bandwidth <= 2 && p.spreading_factor >= 5 && p.spreading_factor <= 12 &&
           p.coding_rate <= 6 && p.header_type <= 1 && p.preamble_len >= 10 &&
           p.preamble_len <= 50 &&
           ((p.spreading_factor != 5 && p.spreading_factor != 6) || p.preamble_len == 12) &&
           (p.sync_word & 0x0f0fU) == 0x0404U;
}

bool valid(const GfskParameters& p) noexcept {
    return valid_common(
               p.param_flags, RadioType::Gfsk, p.tx_power, p.payload_len, p.rssi_threshold,
               p.heartbeat_interval, p.heartbeat_loss) &&
           p.bandwidth <= 2 && p.bitrate >= 600 && p.bitrate <= 150000 && p.freq_deviation >= 600 &&
           p.freq_deviation <= 300000 &&
           static_cast<std::uint64_t>(p.freq_deviation) * 4U >= p.bitrate &&
           (p.pulse_shaping == 0 || (p.pulse_shaping >= 0x08 && p.pulse_shaping <= 0x0b)) &&
           p.preamble_len >= 16;
}

LoRaParameters default_lora_parameters() noexcept {
    return {0x0000, 10, 250, 12, 110, 200, 3, 1, 6, 4, 0, 12, 0x1424};
}

GfskParameters default_gfsk_parameters() noexcept {
    return {0x4000, 10, 250, 12, 110, 200, 3, 1, 50000, 25000, 9, 16, 0x1424};
}

}  // namespace receiver
