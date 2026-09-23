#include <transmitter_simulator/protocol.hpp>

#include <algorithm>

namespace transmitter_simulator {
namespace {
constexpr std::size_t kCommand = 0, kTransaction = 1, kObject = 5, kObjectData = 7;
constexpr std::size_t kFlags = 11, kResult = 39;

bool valid_common(
    const ParamFlags& flags, std::int16_t power, std::uint8_t payload, std::uint8_t rssi,
    std::uint16_t heartbeat, std::uint8_t loss) noexcept {
    return power >= 0 && power <= 22 && (flags.band != Band::MHz433 || power <= 10) &&
           (flags.band != Band::MHz915 || power <= 20) && payload == 12 && rssi >= 10 &&
           rssi <= 148 && heartbeat >= 200 && heartbeat <= 10000 && loss != 0;
}
}  // namespace

std::uint16_t Bytes::read_le16(std::size_t offset) const noexcept {
    assert(offset <= kFrameSize - 2);
    return static_cast<std::uint16_t>(data[offset]) |
           (static_cast<std::uint16_t>(data[offset + 1]) << 8U);
}
std::uint32_t Bytes::read_le32(std::size_t offset) const noexcept {
    assert(offset <= kFrameSize - 4);
    std::uint32_t value{};
    for (unsigned i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(data[offset + i]) << (i * 8U);
    return value;
}
void Bytes::write_le16(std::size_t offset, std::uint16_t value) noexcept {
    assert(offset <= kFrameSize - 2);
    data[offset] = static_cast<std::uint8_t>(value);
    data[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}
void Bytes::write_le32(std::size_t offset, std::uint32_t value) noexcept {
    assert(offset <= kFrameSize - 4);
    for (unsigned i = 0; i < 4; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (i * 8U));
}
std::uint16_t Bytes::crc16_xmodem(std::size_t count) const noexcept {
    assert(count <= kFrameSize);
    std::uint16_t crc{};
    for (std::size_t i = 0; i < count; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8U;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000U) ? static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U)
                                  : static_cast<std::uint16_t>(crc << 1U);
    }
    return crc;
}
void Bytes::fill_crc() noexcept { write_le16(kFrameBodySize, crc16_xmodem()); }
bool Bytes::has_valid_crc() const noexcept { return read_le16(kFrameBodySize) == crc16_xmodem(); }
bool Bytes::is_zero(std::size_t offset, std::size_t count) const noexcept {
    assert(offset <= kFrameSize && count <= kFrameSize - offset);
    return std::all_of(
        data.begin() + static_cast<std::ptrdiff_t>(offset),
        data.begin() + static_cast<std::ptrdiff_t>(offset + count),
        [](std::uint8_t value) { return value == 0; });
}

std::uint16_t ParamFlags::to_raw() const noexcept {
    std::uint16_t raw = radio_type == RadioType::Gfsk ? 0x4000U : 0U;
    raw |= band == Band::MHz915 ? 1U : 0U;
    raw |= crc_enabled ? 0U : 2U;
    raw |= estop_enabled ? 0U : 4U;
    raw |= heartbeat_enabled ? 0U : 8U;
    raw |= one_to_one ? 0U : 16U;
    raw |= channel_scan ? 32U : 0U;
    return raw;
}
bool ParamFlags::from_raw(std::uint16_t raw, ParamFlags& flags) noexcept {
    const auto radio = static_cast<std::uint8_t>((raw >> 14U) & 3U);
    if ((raw & 0x3fc0U) != 0U || radio > 1U) return false;
    flags.radio_type = radio == 0 ? RadioType::LoRa : RadioType::Gfsk;
    flags.band = (raw & 1U) ? Band::MHz915 : Band::MHz433;
    flags.crc_enabled = !(raw & 2U);
    flags.estop_enabled = !(raw & 4U);
    flags.heartbeat_enabled = !(raw & 8U);
    flags.one_to_one = !(raw & 16U);
    flags.channel_scan = raw & 32U;
    return true;
}

LoraConfig LoraConfig::defaults() noexcept {
    LoraConfig v{};
    v.tx_power = 10;
    v.frequency_offset = 250;
    v.payload_length = 12;
    v.rssi_threshold = 110;
    v.heartbeat_interval = 200;
    v.heartbeat_loss = 3;
    v.bandwidth = 1;
    v.spreading_factor = 6;
    v.coding_rate = 4;
    v.preamble_length = 12;
    v.sync_word = 0x1424;
    return v;
}
bool LoraConfig::is_valid() const noexcept {
    return flags.radio_type == RadioType::LoRa &&
           valid_common(
               flags, tx_power, payload_length, rssi_threshold, heartbeat_interval,
               heartbeat_loss) &&
           bandwidth <= 2 && spreading_factor >= 5 && spreading_factor <= 12 && coding_rate <= 6 &&
           header_type <= 1 && preamble_length >= 10 && preamble_length <= 50 &&
           ((spreading_factor != 5 && spreading_factor != 6) || preamble_length == 12) &&
           (sync_word & 0x0f0fU) == 0x0404U;
}
GfskConfig GfskConfig::defaults() noexcept {
    GfskConfig v{};
    v.flags.radio_type = RadioType::Gfsk;
    v.tx_power = 10;
    v.frequency_offset = 250;
    v.payload_length = 12;
    v.rssi_threshold = 110;
    v.heartbeat_interval = 200;
    v.heartbeat_loss = 3;
    v.bandwidth = 1;
    v.bitrate = 50000;
    v.frequency_deviation = 25000;
    v.pulse_shaping = 0x09;
    v.preamble_length = 16;
    v.sync_word = 0x1424;
    return v;
}
bool GfskConfig::is_valid() const noexcept {
    return flags.radio_type == RadioType::Gfsk &&
           valid_common(
               flags, tx_power, payload_length, rssi_threshold, heartbeat_interval,
               heartbeat_loss) &&
           bandwidth <= 2 && bitrate >= 600 && bitrate <= 150000 && frequency_deviation >= 600 &&
           frequency_deviation <= 300000 &&
           static_cast<std::uint64_t>(frequency_deviation) * 4U >= bitrate &&
           (pulse_shaping == 0 || (pulse_shaping >= 0x08 && pulse_shaping <= 0x0b)) &&
           preamble_length >= 16;
}

Bytes LoRaParamFrame::to_bytes() const noexcept {
    Bytes b{};
    b[kCommand] = static_cast<std::uint8_t>(command);
    b.write_le32(kTransaction, transaction_id);
    b.write_le16(kObject, object_index);
    b.write_le32(kObjectData, object_data);
    b.write_le16(kFlags, config.flags.to_raw());
    b.write_le16(13, static_cast<std::uint16_t>(config.tx_power));
    b.write_le16(15, config.frequency_offset);
    b[17] = config.payload_length;
    b[18] = config.rssi_threshold;
    b.write_le16(19, config.heartbeat_interval);
    b[21] = config.heartbeat_loss;
    b[22] = config.bandwidth;
    b[23] = config.spreading_factor;
    b[24] = config.coding_rate;
    b[25] = config.header_type;
    b[26] = config.preamble_length;
    b.write_le16(27, config.sync_word);
    std::copy(reserved.begin(), reserved.end(), b.data.begin() + 29);
    b[kResult] = static_cast<std::uint8_t>(result);
    b.fill_crc();
    return b;
}
LoRaParamFrame LoRaParamFrame::from_bytes(const Bytes& b) noexcept {
    LoRaParamFrame v{};
    v.command = static_cast<SystemCmd>(b[kCommand]);
    v.transaction_id = b.read_le32(kTransaction);
    v.object_index = b.read_le16(kObject);
    v.object_data = b.read_le32(kObjectData);
    (void)ParamFlags::from_raw(b.read_le16(kFlags), v.config.flags);
    v.config.tx_power = static_cast<std::int16_t>(b.read_le16(13));
    v.config.frequency_offset = b.read_le16(15);
    v.config.payload_length = b[17];
    v.config.rssi_threshold = b[18];
    v.config.heartbeat_interval = b.read_le16(19);
    v.config.heartbeat_loss = b[21];
    v.config.bandwidth = b[22];
    v.config.spreading_factor = b[23];
    v.config.coding_rate = b[24];
    v.config.header_type = b[25];
    v.config.preamble_length = b[26];
    v.config.sync_word = b.read_le16(27);
    std::copy_n(b.data.begin() + 29, v.reserved.size(), v.reserved.begin());
    v.result = static_cast<ResultCode>(b[kResult]);
    return v;
}
Bytes GfskParamFrame::to_bytes() const noexcept {
    Bytes b{};
    b[kCommand] = static_cast<std::uint8_t>(command);
    b.write_le32(kTransaction, transaction_id);
    b.write_le16(kObject, object_index);
    b.write_le32(kObjectData, object_data);
    b.write_le16(kFlags, config.flags.to_raw());
    b.write_le16(13, static_cast<std::uint16_t>(config.tx_power));
    b.write_le16(15, config.frequency_offset);
    b[17] = config.payload_length;
    b[18] = config.rssi_threshold;
    b.write_le16(19, config.heartbeat_interval);
    b[21] = config.heartbeat_loss;
    b[22] = config.bandwidth;
    b.write_le32(23, config.bitrate);
    b.write_le32(27, config.frequency_deviation);
    b[31] = config.pulse_shaping;
    b[32] = config.preamble_length;
    b.write_le16(33, config.sync_word);
    std::copy(reserved.begin(), reserved.end(), b.data.begin() + 35);
    b[kResult] = static_cast<std::uint8_t>(result);
    b.fill_crc();
    return b;
}
GfskParamFrame GfskParamFrame::from_bytes(const Bytes& b) noexcept {
    GfskParamFrame v{};
    v.command = static_cast<SystemCmd>(b[kCommand]);
    v.transaction_id = b.read_le32(kTransaction);
    v.object_index = b.read_le16(kObject);
    v.object_data = b.read_le32(kObjectData);
    (void)ParamFlags::from_raw(b.read_le16(kFlags), v.config.flags);
    v.config.tx_power = static_cast<std::int16_t>(b.read_le16(13));
    v.config.frequency_offset = b.read_le16(15);
    v.config.payload_length = b[17];
    v.config.rssi_threshold = b[18];
    v.config.heartbeat_interval = b.read_le16(19);
    v.config.heartbeat_loss = b[21];
    v.config.bandwidth = b[22];
    v.config.bitrate = b.read_le32(23);
    v.config.frequency_deviation = b.read_le32(27);
    v.config.pulse_shaping = b[31];
    v.config.preamble_length = b[32];
    v.config.sync_word = b.read_le16(33);
    std::copy_n(b.data.begin() + 35, v.reserved.size(), v.reserved.begin());
    v.result = static_cast<ResultCode>(b[kResult]);
    return v;
}
Bytes PinFrame::to_bytes() const noexcept {
    Bytes b{};
    b[kCommand] = static_cast<std::uint8_t>(command);
    std::copy(pin.begin(), pin.end(), b.data.begin() + 1);
    b.write_le32(12, transaction_id);
    b[kResult] = static_cast<std::uint8_t>(result);
    b.fill_crc();
    return b;
}
PinFrame PinFrame::from_bytes(const Bytes& b) noexcept {
    PinFrame v{};
    v.command = static_cast<SystemCmd>(b[kCommand]);
    std::copy_n(b.data.begin() + 1, v.pin.size(), v.pin.begin());
    v.transaction_id = b.read_le32(12);
    v.result = static_cast<ResultCode>(b[kResult]);
    return v;
}
bool PinFrame::is_ascii_pin() const noexcept {
    return std::all_of(pin.begin(), pin.end(), [](std::uint8_t v) {
        return v >= static_cast<std::uint8_t>('0') && v <= static_cast<std::uint8_t>('9');
    });
}
bool PinFrame::is_read_request() const noexcept {
    return std::all_of(
        pin.begin(), pin.end(), [](std::uint8_t v) { return v == static_cast<std::uint8_t>('0'); });
}
Bytes SdoFrame::to_bytes() const noexcept {
    Bytes b{};
    b[kCommand] = static_cast<std::uint8_t>(command);
    b.write_le32(kTransaction, transaction_id);
    b.write_le16(kObject, object_index);
    b.write_le32(kObjectData, object_data);
    std::copy(reserved.begin(), reserved.end(), b.data.begin() + 11);
    b[kResult] = static_cast<std::uint8_t>(result);
    b.fill_crc();
    return b;
}
SdoFrame SdoFrame::from_bytes(const Bytes& b) noexcept {
    SdoFrame v{};
    v.command = static_cast<SystemCmd>(b[kCommand]);
    v.transaction_id = b.read_le32(kTransaction);
    v.object_index = b.read_le16(kObject);
    v.object_data = b.read_le32(kObjectData);
    std::copy_n(b.data.begin() + 11, v.reserved.size(), v.reserved.begin());
    v.result = static_cast<ResultCode>(b[kResult]);
    return v;
}
}  // namespace transmitter_simulator
