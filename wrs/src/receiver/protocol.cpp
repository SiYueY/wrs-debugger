#include <receiver/protocol.hpp>

namespace receiver {
namespace {
bool valid_common_parameters(
    std::uint16_t raw_flags, ParamFlags::Modulation expected, std::int16_t tx_power,
    std::uint8_t payload_len, std::uint8_t rssi_threshold, std::uint16_t heartbeat_interval,
    std::uint8_t heartbeat_loss) noexcept {
    if (!ParamFlags::valid_flags(raw_flags)) return false;
    const auto flags = ParamFlags::from_flags(raw_flags);
    if (flags.modulation != expected || flags.group_mode != ParamFlags::GroupMode::OneToOne) {
        return false;
    }

    const auto max_power = flags.band == ParamFlags::Band::MHz433 ? 10 : 20;
    bool result = tx_power >= 0 && tx_power <= max_power;
    result = result && (payload_len == 12);
    result = result && (rssi_threshold >= 10 && rssi_threshold <= 148);
    result = result && (heartbeat_interval >= 200 && heartbeat_interval <= 10000);
    result = result && (heartbeat_loss >= 1);
    return result;
}
}  // namespace

std::uint16_t ParamFlags::to_flags() const noexcept {
    std::uint16_t flags = 0;
    flags |= static_cast<std::uint16_t>(modulation) << 14;
    flags |= static_cast<std::uint16_t>(channel_scan_mode) << 5;
    flags |= static_cast<std::uint16_t>(group_mode) << 4;
    flags |= static_cast<std::uint16_t>(heartbeat) << 3;
    flags |= static_cast<std::uint16_t>(wireless_estop) << 2;
    flags |= static_cast<std::uint16_t>(physical_crc) << 1;
    flags |= static_cast<std::uint16_t>(band);
    return flags;
}

ParamFlags ParamFlags::from_flags(std::uint16_t flags) noexcept {
    ParamFlags result{};
    result.modulation = static_cast<Modulation>((flags >> 14) & 0x3);
    result.channel_scan_mode = static_cast<ChannelScanMode>((flags >> 5) & 0x1);
    result.group_mode = static_cast<GroupMode>((flags >> 4) & 0x1);
    result.heartbeat = static_cast<HeartbeatSwitch>((flags >> 3) & 0x1);
    result.wireless_estop = static_cast<WirelessEstopSwitch>((flags >> 2) & 0x1);
    result.physical_crc = static_cast<PhysicalCrcSwitch>((flags >> 1) & 0x1);
    result.band = static_cast<Band>(flags & 0x1);
    return result;
}

bool ParamFlags::valid_flags(std::uint16_t flags) noexcept {
    constexpr std::uint16_t kReservedBits = 0x3fc0U;
    return (flags & kReservedBits) == 0 && ((flags >> 14U) & 0x3U) <= 1;
}

bool valid(const LoRaParameters& parameters) noexcept {
    bool result = valid_common_parameters(
        parameters.param_flags, ParamFlags::Modulation::LoRa, parameters.tx_power,
        parameters.payload_len, parameters.rssi_threshold, parameters.heartbeat_interval,
        parameters.heartbeat_loss);
    result = result && (parameters.bandwidth <= 2);
    result = result && (parameters.spreading_factor >= 5 && parameters.spreading_factor <= 12);
    result = result && (parameters.coding_rate <= 6 && parameters.header_type <= 1);
    result = result && (parameters.preamble_len >= 10 && parameters.preamble_len <= 50);
    result = result && ((parameters.spreading_factor != 5 && parameters.spreading_factor != 6) ||
                        parameters.preamble_len == 12);
    result = result && ((parameters.sync_word & 0x0f0fU) == 0x0404U);
    return result;
}

bool valid(const GfskParameters& parameters) noexcept {
    bool result = valid_common_parameters(
        parameters.param_flags, ParamFlags::Modulation::Gfsk, parameters.tx_power,
        parameters.payload_len, parameters.rssi_threshold, parameters.heartbeat_interval,
        parameters.heartbeat_loss);
    result = result && (parameters.bandwidth <= 2);
    result = result && (parameters.bitrate >= 600 && parameters.bitrate <= 150000);
    result = result && (parameters.freq_deviation >= 600 && parameters.freq_deviation <= 300000);
    result = result &&
             (static_cast<std::uint64_t>(parameters.freq_deviation) * 4U >= parameters.bitrate);
    result = result && (parameters.pulse_shaping == 0 ||
                        (parameters.pulse_shaping >= 0x08 && parameters.pulse_shaping <= 0x0b));
    result = result && (parameters.preamble_len >= 16);
    return result;
}

LoRaParameters default_lora_parameters() noexcept {
    return {0x0000, 10, 250, 12, 110, 200, 3, 1, 6, 4, 0, 12, 0x1424};
}

GfskParameters default_gfsk_parameters() noexcept {
    return {0x4000, 10, 250, 12, 110, 200, 3, 1, 50000, 25000, 9, 16, 0x1424};
}

bool same_parameters(const LoRaParameters& left, const LoRaParameters& right) noexcept {
    bool result = left.param_flags == right.param_flags && left.tx_power == right.tx_power;
    result = result && (left.freq_offset == right.freq_offset);
    result = result && (left.payload_len == right.payload_len);
    result = result && (left.rssi_threshold == right.rssi_threshold);
    result = result && (left.heartbeat_interval == right.heartbeat_interval);
    result = result && (left.heartbeat_loss == right.heartbeat_loss);
    result = result && (left.bandwidth == right.bandwidth);
    result = result && (left.spreading_factor == right.spreading_factor);
    result = result && (left.coding_rate == right.coding_rate);
    result = result && (left.header_type == right.header_type);
    result = result && (left.preamble_len == right.preamble_len);
    result = result && (left.sync_word == right.sync_word);
    return result;
}
}  // namespace receiver
