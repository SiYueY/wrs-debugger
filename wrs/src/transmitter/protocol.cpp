#include <transmitter/protocol.hpp>

namespace transmitter {
namespace {
constexpr std::size_t kCommandOffset = 0;
constexpr std::size_t kTransactionOffset = 1;
constexpr std::size_t kObjectOffset = 5;
constexpr std::size_t kObjectDataOffset = 7;
constexpr std::size_t kFlagsOffset = 11;
constexpr std::size_t kResultOffset = 39;

}  // namespace

std::uint16_t ParamFlags::to_raw() const noexcept {
    std::uint16_t raw_flags = 0;
    if (band == Band::MHz915) raw_flags |= 0x0001;
    if (!crc_enabled) raw_flags |= 0x0002;
    if (!estop_enabled) raw_flags |= 0x0004;
    if (!heartbeat_enabled) raw_flags |= 0x0008;
    if (!one_to_one) raw_flags |= 0x0010;
    if (channel_scan) raw_flags |= 0x0020;
    if (radio_type == RadioType::GFSK) raw_flags |= 0x4000;
    return raw_flags;
}

ParamFlags ParamFlags::from_raw(std::uint16_t raw_flags) noexcept {
    ParamFlags flags{};
    flags.band = (raw_flags & 0x0001) ? Band::MHz915 : Band::MHz433;
    flags.crc_enabled = !(raw_flags & 0x0002);
    flags.estop_enabled = !(raw_flags & 0x0004);
    flags.heartbeat_enabled = !(raw_flags & 0x0008);
    flags.one_to_one = !(raw_flags & 0x0010);
    flags.channel_scan = raw_flags & 0x0020;
    flags.radio_type = ((raw_flags >> 14) & 0x3) == 1 ? RadioType::GFSK : RadioType::LoRa;
    return flags;
}

void write_le16(std::uint8_t* destination, std::uint16_t value) noexcept {
    destination[0] = static_cast<std::uint8_t>(value);
    destination[1] = static_cast<std::uint8_t>(value >> 8);
}

void write_le32(std::uint8_t* destination, std::uint32_t value) noexcept {
    for (unsigned index = 0; index < 4; ++index)
        destination[index] = static_cast<std::uint8_t>(value >> (index * 8));
}

std::uint16_t read_le16(const std::uint8_t* source) noexcept {
    return static_cast<std::uint16_t>(source[0]) | (static_cast<std::uint16_t>(source[1]) << 8);
}

std::uint32_t read_le32(const std::uint8_t* source) noexcept {
    std::uint32_t value = 0;
    for (unsigned index = 0; index < 4; ++index)
        value |= static_cast<std::uint32_t>(source[index]) << (index * 8);
    return value;
}

std::uint16_t crc16_xmodem(const std::uint8_t* source, std::size_t size) noexcept {
    std::uint16_t crc = 0;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= static_cast<std::uint16_t>(source[index]) << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<std::uint16_t>(crc << 1);
    }
    return crc;
}

std::uint16_t crc16_xmodem(const Bytes& bytes) noexcept {
    return crc16_xmodem(bytes.bytes(), kFrameBodySize);
}

void fill_crc(Bytes& bytes) noexcept {
    write_le16(bytes.bytes() + kFrameBodySize, crc16_xmodem(bytes));
}

bool has_valid_crc(const Bytes& bytes) noexcept {
    return read_le16(bytes.bytes() + kFrameBodySize) == crc16_xmodem(bytes);
}

Bytes LoRaParamFrame::to_bytes() const noexcept {
    Bytes bytes{};
    bytes[kCommandOffset] = cmd;
    write_le32(bytes.bytes() + kTransactionOffset, transaction_id);
    write_le16(bytes.bytes() + kObjectOffset, object_index);
    write_le32(bytes.bytes() + kObjectDataOffset, object_data);
    write_le16(bytes.bytes() + kFlagsOffset, param_flags);
    write_le16(bytes.bytes() + 13, static_cast<std::uint16_t>(tx_power));
    write_le16(bytes.bytes() + 15, freq_offset);
    bytes[17] = payload_len;
    bytes[18] = rssi_threshold;
    write_le16(bytes.bytes() + 19, heartbeat_interval);
    bytes[21] = heartbeat_loss;
    bytes[22] = bandwidth;
    bytes[23] = spreading_factor;
    bytes[24] = coding_rate;
    bytes[25] = header_type;
    bytes[26] = preamble_len;
    write_le16(bytes.bytes() + 27, sync_word);
    for (std::size_t index = 0; index < reserved.size(); ++index)
        bytes[29 + index] = reserved[index];
    bytes[kResultOffset] = result_code;
    fill_crc(bytes);
    return bytes;
}

LoRaParamFrame LoRaParamFrame::from_bytes(const Bytes& bytes) noexcept {
    LoRaParamFrame frame{};
    frame.cmd = bytes[kCommandOffset];
    frame.transaction_id = read_le32(bytes.bytes() + kTransactionOffset);
    frame.object_index = read_le16(bytes.bytes() + kObjectOffset);
    frame.object_data = read_le32(bytes.bytes() + kObjectDataOffset);
    frame.param_flags = read_le16(bytes.bytes() + kFlagsOffset);
    frame.tx_power = static_cast<std::int16_t>(read_le16(bytes.bytes() + 13));
    frame.freq_offset = read_le16(bytes.bytes() + 15);
    frame.payload_len = bytes[17];
    frame.rssi_threshold = bytes[18];
    frame.heartbeat_interval = read_le16(bytes.bytes() + 19);
    frame.heartbeat_loss = bytes[21];
    frame.bandwidth = bytes[22];
    frame.spreading_factor = bytes[23];
    frame.coding_rate = bytes[24];
    frame.header_type = bytes[25];
    frame.preamble_len = bytes[26];
    frame.sync_word = read_le16(bytes.bytes() + 27);
    for (std::size_t index = 0; index < frame.reserved.size(); ++index)
        frame.reserved[index] = bytes[29 + index];
    frame.result_code = bytes[kResultOffset];
    frame.crc16 = read_le16(bytes.bytes() + kFrameBodySize);
    return frame;
}

Bytes GfskParamFrame::to_bytes() const noexcept {
    Bytes bytes{};
    bytes[kCommandOffset] = cmd;
    write_le32(bytes.bytes() + kTransactionOffset, transaction_id);
    write_le16(bytes.bytes() + kObjectOffset, object_index);
    write_le32(bytes.bytes() + kObjectDataOffset, object_data);
    write_le16(bytes.bytes() + kFlagsOffset, param_flags);
    write_le16(bytes.bytes() + 13, static_cast<std::uint16_t>(tx_power));
    write_le16(bytes.bytes() + 15, freq_offset);
    bytes[17] = payload_len;
    bytes[18] = rssi_threshold;
    write_le16(bytes.bytes() + 19, heartbeat_interval);
    bytes[21] = heartbeat_loss;
    bytes[22] = bandwidth;
    write_le32(bytes.bytes() + 23, bitrate);
    write_le32(bytes.bytes() + 27, freq_deviation);
    bytes[31] = pulse_shaping;
    bytes[32] = preamble_len;
    write_le16(bytes.bytes() + 33, sync_word);
    for (std::size_t index = 0; index < reserved.size(); ++index)
        bytes[35 + index] = reserved[index];
    bytes[kResultOffset] = result_code;
    fill_crc(bytes);
    return bytes;
}

GfskParamFrame GfskParamFrame::from_bytes(const Bytes& bytes) noexcept {
    GfskParamFrame frame{};
    frame.cmd = bytes[kCommandOffset];
    frame.transaction_id = read_le32(bytes.bytes() + kTransactionOffset);
    frame.object_index = read_le16(bytes.bytes() + kObjectOffset);
    frame.object_data = read_le32(bytes.bytes() + kObjectDataOffset);
    frame.param_flags = read_le16(bytes.bytes() + kFlagsOffset);
    frame.tx_power = static_cast<std::int16_t>(read_le16(bytes.bytes() + 13));
    frame.freq_offset = read_le16(bytes.bytes() + 15);
    frame.payload_len = bytes[17];
    frame.rssi_threshold = bytes[18];
    frame.heartbeat_interval = read_le16(bytes.bytes() + 19);
    frame.heartbeat_loss = bytes[21];
    frame.bandwidth = bytes[22];
    frame.bitrate = read_le32(bytes.bytes() + 23);
    frame.freq_deviation = read_le32(bytes.bytes() + 27);
    frame.pulse_shaping = bytes[31];
    frame.preamble_len = bytes[32];
    frame.sync_word = read_le16(bytes.bytes() + 33);
    for (std::size_t index = 0; index < frame.reserved.size(); ++index)
        frame.reserved[index] = bytes[35 + index];
    frame.result_code = bytes[kResultOffset];
    frame.crc16 = read_le16(bytes.bytes() + kFrameBodySize);
    return frame;
}

Bytes PinFrame::to_bytes() const noexcept {
    Bytes bytes{};
    bytes[0] = cmd;
    for (std::size_t index = 0; index < pin.size(); ++index) bytes[1 + index] = pin[index];
    for (std::size_t index = 0; index < reserved1.size(); ++index)
        bytes[7 + index] = reserved1[index];
    write_le32(bytes.bytes() + 12, transaction_id);
    for (std::size_t index = 0; index < reserved2.size(); ++index)
        bytes[16 + index] = reserved2[index];
    bytes[kResultOffset] = result_code;
    fill_crc(bytes);
    return bytes;
}

PinFrame PinFrame::from_bytes(const Bytes& bytes) noexcept {
    PinFrame frame{};
    frame.cmd = bytes[0];
    for (std::size_t index = 0; index < frame.pin.size(); ++index)
        frame.pin[index] = bytes[1 + index];
    for (std::size_t index = 0; index < frame.reserved1.size(); ++index)
        frame.reserved1[index] = bytes[7 + index];
    frame.transaction_id = read_le32(bytes.bytes() + 12);
    for (std::size_t index = 0; index < frame.reserved2.size(); ++index)
        frame.reserved2[index] = bytes[16 + index];
    frame.result_code = bytes[kResultOffset];
    frame.crc16 = read_le16(bytes.bytes() + kFrameBodySize);
    return frame;
}

Bytes DeviceKeyFrame::to_bytes() const noexcept {
    Bytes bytes{};
    bytes[0] = cmd;
    for (std::size_t index = 0; index < device_id.size(); ++index)
        bytes[1 + index] = device_id[index];
    for (std::size_t index = 0; index < kbind.size(); ++index) bytes[4 + index] = kbind[index];
    write_le32(bytes.bytes() + 20, transaction_id);
    for (std::size_t index = 0; index < reserved.size(); ++index)
        bytes[24 + index] = reserved[index];
    bytes[kResultOffset] = result_code;
    fill_crc(bytes);
    return bytes;
}

DeviceKeyFrame DeviceKeyFrame::from_bytes(const Bytes& bytes) noexcept {
    DeviceKeyFrame frame{};
    frame.cmd = bytes[0];
    for (std::size_t index = 0; index < frame.device_id.size(); ++index)
        frame.device_id[index] = bytes[1 + index];
    for (std::size_t index = 0; index < frame.kbind.size(); ++index)
        frame.kbind[index] = bytes[4 + index];
    frame.transaction_id = read_le32(bytes.bytes() + 20);
    for (std::size_t index = 0; index < frame.reserved.size(); ++index)
        frame.reserved[index] = bytes[24 + index];
    frame.result_code = bytes[kResultOffset];
    frame.crc16 = read_le16(bytes.bytes() + kFrameBodySize);
    return frame;
}

Bytes SdoFrame::to_bytes() const noexcept {
    Bytes bytes{};
    bytes[0] = cmd;
    write_le32(bytes.bytes() + 1, transaction_id);
    write_le16(bytes.bytes() + 5, object_index);
    write_le32(bytes.bytes() + 7, object_data);
    for (std::size_t index = 0; index < reserved.size(); ++index)
        bytes[11 + index] = reserved[index];
    bytes[kResultOffset] = result_code;
    fill_crc(bytes);
    return bytes;
}

SdoFrame SdoFrame::from_bytes(const Bytes& bytes) noexcept {
    SdoFrame frame{};
    frame.cmd = bytes[0];
    frame.transaction_id = read_le32(bytes.bytes() + 1);
    frame.object_index = read_le16(bytes.bytes() + 5);
    frame.object_data = read_le32(bytes.bytes() + 7);
    for (std::size_t index = 0; index < frame.reserved.size(); ++index)
        frame.reserved[index] = bytes[11 + index];
    frame.result_code = bytes[kResultOffset];
    frame.crc16 = read_le16(bytes.bytes() + kFrameBodySize);
    return frame;
}

}  // namespace transmitter
