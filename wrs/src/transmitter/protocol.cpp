#include <transmitter/protocol.hpp>

namespace transmitter {
namespace {

// 公共字段偏移量
constexpr std::size_t kCommandOffset = 0;
constexpr std::size_t kTransactionOffset = 1;
constexpr std::size_t kObjectOffset = 5;
constexpr std::size_t kObjectDataOffset = 7;
constexpr std::size_t kFlagsOffset = 11;
constexpr std::size_t kResultOffset = 39;

}  // namespace

std::uint16_t ParamFlags::to_flags() const noexcept {
    std::uint16_t flags = 0;

    // bit15..14：调制方式
    flags |= static_cast<std::uint16_t>(modulation) << 14;

    // bit13..6：保留，固定 0（flags 初始为 0）

    // bit5：信道扫描方式
    flags |= static_cast<std::uint16_t>(channel_scan_mode) << 5;

    // bit4：分组模式
    flags |= static_cast<std::uint16_t>(group_mode) << 4;

    // bit3：心跳包开关
    flags |= static_cast<std::uint16_t>(heartbeat) << 3;

    // bit2：无线急停开关
    flags |= static_cast<std::uint16_t>(wireless_estop) << 2;

    // bit1：物理层 CRC 开关
    flags |= static_cast<std::uint16_t>(physical_crc) << 1;

    // bit0：频段
    flags |= static_cast<std::uint16_t>(band) << 0;

    return flags;
}

ParamFlags ParamFlags::from_flags(std::uint16_t flags) noexcept {
    ParamFlags result{};

    // bit15..14：调制方式
    result.modulation = static_cast<Modulation>((flags >> 14) & 0x3);

    // bit13..6：保留，忽略

    // bit5：信道扫描方式
    result.channel_scan_mode = static_cast<ChannelScanMode>((flags >> 5) & 0x1);

    // bit4：分组模式
    result.group_mode = static_cast<GroupMode>((flags >> 4) & 0x1);

    // bit3：心跳包开关
    result.heartbeat = static_cast<HeartbeatSwitch>((flags >> 3) & 0x1);

    // bit2：无线急停开关
    result.wireless_estop = static_cast<WirelessEstopSwitch>((flags >> 2) & 0x1);

    // bit1：物理层 CRC 开关
    result.physical_crc = static_cast<PhysicalCrcSwitch>((flags >> 1) & 0x1);

    // bit0：频段
    result.band = static_cast<Band>(flags & 0x1);

    return result;
}

void write_le16(std::uint8_t* destination, std::uint16_t value) noexcept {
    for (unsigned index = 0; index < 2; ++index)
        destination[index] = static_cast<std::uint8_t>(value >> (index * 8));
}

void write_le32(std::uint8_t* destination, std::uint32_t value) noexcept {
    for (unsigned index = 0; index < 4; ++index)
        destination[index] = static_cast<std::uint8_t>(value >> (index * 8));
}

std::uint16_t read_le16(const std::uint8_t* source) noexcept {
    std::uint16_t value = 0;
    for (unsigned index = 0; index < 2; ++index)
        value |= static_cast<std::uint16_t>(source[index]) << (index * 8);
    return value;
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

std::uint16_t crc16_xmodem(const Frame& frame) noexcept {
    return crc16_xmodem(frame.bytes(), kFrameBodySize);
}

void fill_crc(Frame& frame) noexcept {
    write_le16(frame.bytes() + kFrameBodySize, crc16_xmodem(frame));
}

bool has_valid_crc(const Frame& frame) noexcept {
    return read_le16(frame.bytes() + kFrameBodySize) == crc16_xmodem(frame);
}

Frame LoRaParamFrame::to_frame() const noexcept {
    Frame frame{};
    frame[kCommandOffset] = cmd;
    write_le32(frame.bytes() + kTransactionOffset, transaction_id);
    write_le16(frame.bytes() + kObjectOffset, object_index);
    write_le32(frame.bytes() + kObjectDataOffset, object_data);
    write_le16(frame.bytes() + kFlagsOffset, param_flags);
    write_le16(frame.bytes() + 13, static_cast<std::uint16_t>(tx_power));
    write_le16(frame.bytes() + 15, freq_offset);
    frame[17] = kPayloadLen;  // 强制固定为 12
    frame[18] = rssi_threshold;
    write_le16(frame.bytes() + 19, heartbeat_interval);
    frame[21] = heartbeat_loss;
    frame[22] = bandwidth;
    frame[23] = spreading_factor;
    frame[24] = coding_rate;
    frame[25] = header_type;
    frame[26] = preamble_len;
    write_le16(frame.bytes() + 27, sync_word);
    // Byte29~38 预留，强制 0（Frame{} 已置 0）
    frame[kResultOffset] = result_code;
    fill_crc(frame);
    return frame;
}

LoRaParamFrame LoRaParamFrame::from_frame(const Frame& frame) noexcept {
    LoRaParamFrame result{};
    result.cmd = frame[kCommandOffset];
    result.transaction_id = read_le32(frame.bytes() + kTransactionOffset);
    result.object_index = read_le16(frame.bytes() + kObjectOffset);
    result.object_data = read_le32(frame.bytes() + kObjectDataOffset);
    result.param_flags = read_le16(frame.bytes() + kFlagsOffset);
    result.tx_power = static_cast<std::int16_t>(read_le16(frame.bytes() + 13));
    result.freq_offset = read_le16(frame.bytes() + 15);
    result.payload_len = frame[17];
    result.rssi_threshold = frame[18];
    result.heartbeat_interval = read_le16(frame.bytes() + 19);
    result.heartbeat_loss = frame[21];
    result.bandwidth = frame[22];
    result.spreading_factor = frame[23];
    result.coding_rate = frame[24];
    result.header_type = frame[25];
    result.preamble_len = frame[26];
    result.sync_word = read_le16(frame.bytes() + 27);
    // Byte29~38 预留，忽略
    result.result_code = frame[kResultOffset];
    result.crc16 = read_le16(frame.bytes() + kFrameBodySize);
    return result;
}

Frame GfskParamFrame::to_frame() const noexcept {
    Frame frame{};
    frame[kCommandOffset] = cmd;
    write_le32(frame.bytes() + kTransactionOffset, transaction_id);
    write_le16(frame.bytes() + kObjectOffset, object_index);
    write_le32(frame.bytes() + kObjectDataOffset, object_data);
    write_le16(frame.bytes() + kFlagsOffset, param_flags);
    write_le16(frame.bytes() + 13, static_cast<std::uint16_t>(tx_power));
    write_le16(frame.bytes() + 15, freq_offset);
    frame[17] = kPayloadLen;  // 强制固定为 12
    frame[18] = rssi_threshold;
    write_le16(frame.bytes() + 19, heartbeat_interval);
    frame[21] = heartbeat_loss;
    frame[22] = bandwidth;
    write_le32(frame.bytes() + 23, bitrate);
    write_le32(frame.bytes() + 27, freq_deviation);
    frame[31] = pulse_shaping;
    frame[32] = preamble_len;
    write_le16(frame.bytes() + 33, sync_word);
    // Byte35~38 预留，强制 0（Frame{} 已置 0）
    frame[kResultOffset] = result_code;
    fill_crc(frame);
    return frame;
}

GfskParamFrame GfskParamFrame::from_frame(const Frame& frame) noexcept {
    GfskParamFrame result{};
    result.cmd = frame[kCommandOffset];
    result.transaction_id = read_le32(frame.bytes() + kTransactionOffset);
    result.object_index = read_le16(frame.bytes() + kObjectOffset);
    result.object_data = read_le32(frame.bytes() + kObjectDataOffset);
    result.param_flags = read_le16(frame.bytes() + kFlagsOffset);
    result.tx_power = static_cast<std::int16_t>(read_le16(frame.bytes() + 13));
    result.freq_offset = read_le16(frame.bytes() + 15);
    result.payload_len = frame[17];
    result.rssi_threshold = frame[18];
    result.heartbeat_interval = read_le16(frame.bytes() + 19);
    result.heartbeat_loss = frame[21];
    result.bandwidth = frame[22];
    result.bitrate = read_le32(frame.bytes() + 23);
    result.freq_deviation = read_le32(frame.bytes() + 27);
    result.pulse_shaping = frame[31];
    result.preamble_len = frame[32];
    result.sync_word = read_le16(frame.bytes() + 33);
    // Byte35~38 预留，忽略
    result.result_code = frame[kResultOffset];
    result.crc16 = read_le16(frame.bytes() + kFrameBodySize);
    return result;
}

Frame PinFrame::to_frame() const noexcept {
    Frame frame{};
    frame[0] = static_cast<std::uint8_t>(cmd);
    for (std::size_t index = 0; index < pin.size(); ++index) frame[1 + index] = pin[index];
    // Byte7~11 预留，强制 0（Frame{} 已置 0）
    write_le32(frame.bytes() + 12, transaction_id);
    // Byte16~38 预留，强制 0（Frame{} 已置 0）
    frame[kResultOffset] = static_cast<std::uint8_t>(result_code);
    fill_crc(frame);
    return frame;
}

PinFrame PinFrame::from_frame(const Frame& frame) noexcept {
    PinFrame result{};
    result.cmd = static_cast<SystemCommand>(frame[0]);
    for (std::size_t index = 0; index < result.pin.size(); ++index)
        result.pin[index] = frame[1 + index];
    // Byte7~11 预留，忽略
    result.transaction_id = read_le32(frame.bytes() + 12);
    // Byte16~38 预留，忽略
    result.result_code = static_cast<ResultCode>(frame[kResultOffset]);
    result.crc16 = read_le16(frame.bytes() + kFrameBodySize);
    return result;
}

bool PinFrame::set_pin(std::string_view value) noexcept {
    if (value.size() != kPinLength) return false;
    for (std::size_t index = 0; index < kPinLength; ++index) {
        const char ch = value[index];
        if (ch < '0' || ch > '9') return false;
        pin[index] = static_cast<std::uint8_t>(ch);
    }
    return true;
}

std::string PinFrame::get_pin() const {
    return std::string(reinterpret_cast<const char*>(pin.data()), pin.size());
}

Frame DeviceKeyFrame::to_frame() const noexcept {
    Frame frame{};
    frame[0] = static_cast<std::uint8_t>(cmd);
    for (std::size_t index = 0; index < device_id.size(); ++index)
        frame[1 + index] = device_id[index];
    for (std::size_t index = 0; index < kbind.size(); ++index) frame[4 + index] = kbind[index];
    write_le32(frame.bytes() + 20, transaction_id);
    // Byte24~38 预留，强制 0（Frame{} 已置 0）
    frame[kResultOffset] = static_cast<std::uint8_t>(result_code);
    fill_crc(frame);
    return frame;
}

DeviceKeyFrame DeviceKeyFrame::from_frame(const Frame& frame) noexcept {
    DeviceKeyFrame result{};
    result.cmd = static_cast<SystemCommand>(frame[0]);
    for (std::size_t index = 0; index < result.device_id.size(); ++index)
        result.device_id[index] = frame[1 + index];
    for (std::size_t index = 0; index < result.kbind.size(); ++index)
        result.kbind[index] = frame[4 + index];
    result.transaction_id = read_le32(frame.bytes() + 20);
    // Byte24~38 预留，忽略
    result.result_code = static_cast<ResultCode>(frame[kResultOffset]);
    result.crc16 = read_le16(frame.bytes() + kFrameBodySize);
    return result;
}

void DeviceKeyFrame::wipe() noexcept {
    device_id.fill(0);
    kbind.fill(0);
    transaction_id = 0;
}

Frame NormalFrame::to_frame() const noexcept {
    Frame frame{};
    frame[0] = static_cast<std::uint8_t>(cmd);
    for (std::size_t index = 0; index < device_id.size(); ++index)
        frame[1 + index] = device_id[index];
    write_le32(frame.bytes() + 4, wireless_counter);
    write_le16(frame.bytes() + 8, receiver_status);
    frame[10] = rssi;
    write_le16(frame.bytes() + 11, static_cast<std::uint16_t>(snr));
    frame[13] = control;
    write_le16(frame.bytes() + 14, object_index);
    write_le32(frame.bytes() + 16, object_data);
    write_le32(frame.bytes() + 20, warning_code);
    write_le32(frame.bytes() + 24, error_code);
    // Byte28~38 预留，强制 0（Frame{} 已置 0）
    frame[kResultOffset] = static_cast<std::uint8_t>(result_code);
    fill_crc(frame);
    return frame;
}

NormalFrame NormalFrame::from_frame(const Frame& frame) noexcept {
    NormalFrame result{};
    result.cmd = static_cast<SystemCommand>(frame[0]);
    for (std::size_t index = 0; index < result.device_id.size(); ++index)
        result.device_id[index] = frame[1 + index];
    result.wireless_counter = read_le32(frame.bytes() + 4);
    result.receiver_status = read_le16(frame.bytes() + 8);
    result.rssi = frame[10];
    result.snr = static_cast<std::int16_t>(read_le16(frame.bytes() + 11));
    result.control = frame[13];
    result.object_index = read_le16(frame.bytes() + 14);
    result.object_data = read_le32(frame.bytes() + 16);
    result.warning_code = read_le32(frame.bytes() + 20);
    result.error_code = read_le32(frame.bytes() + 24);
    // Byte28~38 预留，忽略
    result.result_code = static_cast<ResultCode>(frame[kResultOffset]);
    result.crc16 = read_le16(frame.bytes() + kFrameBodySize);
    return result;
}

Frame SdoFrame::to_frame() const noexcept {
    Frame frame{};
    frame[0] = static_cast<std::uint8_t>(cmd);
    write_le32(frame.bytes() + 1, transaction_id);
    write_le16(frame.bytes() + 5, object_index);
    write_le32(frame.bytes() + 7, object_data);
    // Byte11~38 预留，强制 0（Frame{} 已置 0）
    frame[kResultOffset] = static_cast<std::uint8_t>(result_code);
    fill_crc(frame);
    return frame;
}

SdoFrame SdoFrame::from_frame(const Frame& frame) noexcept {
    SdoFrame result{};
    result.cmd = static_cast<SystemCommand>(frame[0]);
    result.transaction_id = read_le32(frame.bytes() + 1);
    result.object_index = read_le16(frame.bytes() + 5);
    result.object_data = read_le32(frame.bytes() + 7);
    // Byte11~38 预留，忽略
    result.result_code = static_cast<ResultCode>(frame[kResultOffset]);
    result.crc16 = read_le16(frame.bytes() + kFrameBodySize);
    return result;
}

}  // namespace transmitter