#include <transmitter/client.hpp>
#include <transmitter/protocol.hpp>

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <pty.h>
#include <string>
#include <thread>
#include <unistd.h>

namespace {
using namespace transmitter;

void test_flags() {
    ParamFlags flags{};
    flags.band = ParamFlags::Band::MHz915;
    flags.physical_crc = ParamFlags::PhysicalCrcSwitch::Disabled;
    flags.wireless_estop = ParamFlags::WirelessEstopSwitch::Disabled;
    flags.heartbeat = ParamFlags::HeartbeatSwitch::Disabled;
    flags.group_mode = ParamFlags::GroupMode::OneToMany;
    flags.channel_scan_mode = ParamFlags::ChannelScanMode::Hopping;
    flags.modulation = ParamFlags::Modulation::Gfsk;
    assert(flags.to_flags() == 0x403f);
    const auto decoded = ParamFlags::from_flags(0x403f);
    assert(decoded.band == ParamFlags::Band::MHz915);
    assert(decoded.physical_crc == ParamFlags::PhysicalCrcSwitch::Disabled);
    assert(decoded.wireless_estop == ParamFlags::WirelessEstopSwitch::Disabled);
    assert(decoded.heartbeat == ParamFlags::HeartbeatSwitch::Disabled);
    assert(decoded.group_mode == ParamFlags::GroupMode::OneToMany);
    assert(decoded.channel_scan_mode == ParamFlags::ChannelScanMode::Hopping);
    assert(decoded.modulation == ParamFlags::Modulation::Gfsk);
    assert(ParamFlags::from_flags(0x3fc0).to_flags() == 0);
    const auto reserved_modulation = ParamFlags::from_flags(0x8000).modulation;
    assert(static_cast<std::uint16_t>(reserved_modulation) == 2);
}

void test_lora_frame() {
    LoRaParamFrame frame{};
    frame.cmd = static_cast<std::uint8_t>(SystemCommand::WriteParamReq);
    frame.transaction_id = 0x44332211;
    frame.object_index = 0x6123;
    frame.object_data = 0x88776655;
    frame.param_flags = 0x4001;
    frame.tx_power = -12;
    frame.freq_offset = 0x1234;
    frame.payload_len = 12;
    frame.rssi_threshold = 110;
    frame.heartbeat_interval = 200;
    frame.heartbeat_loss = 3;
    frame.bandwidth = 1;
    frame.spreading_factor = 6;
    frame.coding_rate = 4;
    frame.header_type = 1;
    frame.preamble_len = 12;
    frame.sync_word = 0x1424;
    frame.reserved[9] = 0xa5;
    frame.result_code = static_cast<std::uint8_t>(ResultCode::Failure);
    frame.crc16 = 0xbeef;
    const auto raw = frame.to_frame();
    assert(raw[0] == 0x05);
    assert(raw[1] == 0x11);
    assert(raw[4] == 0x44);
    assert(raw[5] == 0x23);
    assert(raw[6] == 0x61);
    assert(raw[7] == 0x55);
    assert(raw[10] == 0x88);
    assert(raw[13] == 0xf4);
    assert(raw[14] == 0xff);
    assert(raw[27] == 0x24);
    assert(raw[28] == 0x14);
    assert(raw[38] == 0);
    assert(raw[39] == 0xff);
    assert(has_valid_crc(raw));
    assert(read_le16(raw.bytes() + kFrameBodySize) != frame.crc16);
    const auto round_trip = LoRaParamFrame::from_frame(raw);
    assert(round_trip.transaction_id == frame.transaction_id && round_trip.tx_power == -12);
    assert(round_trip.reserved == (std::array<std::uint8_t, 10>{}) && has_valid_crc(raw));
    auto corrupt = raw;
    corrupt[24] ^= 0x01;
    assert(!has_valid_crc(corrupt));
}

void test_gfsk_frame() {
    GfskParamFrame frame{};
    frame.cmd = static_cast<std::uint8_t>(SystemCommand::WriteParamReq);
    frame.transaction_id = 7;
    frame.param_flags = 0x4000;
    frame.bandwidth = 2;
    frame.bitrate = 50000;
    frame.freq_deviation = 25000;
    frame.pulse_shaping = 0x09;
    frame.preamble_len = 16;
    frame.sync_word = 0x1424;
    const auto raw = frame.to_frame();
    assert(raw[22] == 2 && read_le32(raw.bytes() + 23) == 50000);
    assert(read_le32(raw.bytes() + 27) == 25000);
    assert(raw[31] == 0x09);
    assert(raw[32] == 16);
    assert(raw[33] == 0x24);
    assert(raw[34] == 0x14);
    assert(has_valid_crc(raw));
    const auto round_trip = GfskParamFrame::from_frame(raw);
    assert(round_trip.bitrate == 50000 && round_trip.freq_deviation == 25000);
}

void test_pin_device_key_and_sdo() {
    PinFrame pin{};
    pin.cmd = SystemCommand::PinConfigReq;
    pin.pin = {'1', '2', '3', '4', '5', '6'};
    pin.transaction_id = 0x01020304;
    const auto pin_bytes = pin.to_frame();
    assert(pin_bytes[1] == '1');
    assert(pin_bytes[6] == '6');
    assert(pin_bytes[12] == 0x04);
    assert(
        has_valid_crc(pin_bytes) &&
        PinFrame::from_frame(pin_bytes).transaction_id == pin.transaction_id);

    DeviceKeyFrame key{};
    key.cmd = SystemCommand::BindReq;
    key.device_id = {1, 2, 3};
    key.kbind[0] = 0x42;
    key.transaction_id = 9;
    const auto key_bytes = key.to_frame();
    assert(key_bytes[1] == 1);
    assert(key_bytes[3] == 3);
    assert(key_bytes[4] == 0x42);
    assert(key_bytes[20] == 9);
    assert(DeviceKeyFrame::from_frame(key_bytes).kbind[0] == 0x42);

    SdoFrame sdo{};
    sdo.cmd = SystemCommand::ReadParamRsp;
    sdo.transaction_id = 12;
    sdo.object_index = 0x4001;
    sdo.object_data = 0x44332211;
    const auto sdo_bytes = sdo.to_frame();
    assert(sdo_bytes[5] == 1);
    assert(sdo_bytes[6] == 0x40);
    assert(sdo_bytes[11] == 0);
    const auto decoded = SdoFrame::from_frame(sdo_bytes);
    assert(decoded.object_index == 0x4001 && decoded.object_data == 0x44332211);

    NormalFrame normal{};
    normal.cmd = SystemCommand::RunStatusRsp;
    normal.device_id = {0xa1, 0xb2, 0xc3};
    normal.wireless_counter = 0x81234567;
    normal.receiver_status = 0x0011;
    normal.rssi = 72;
    normal.snr = -24;
    const auto normal_bytes = normal.to_frame();
    assert(normal_bytes[0] == 0x86);
    assert(normal_bytes[1] == 0xa1);
    assert(normal_bytes[3] == 0xc3);
    assert(normal_bytes[4] == 0x67);
    assert(normal_bytes[7] == 0x81);
    assert(has_valid_crc(normal_bytes));
    const auto decoded_normal = NormalFrame::from_frame(normal_bytes);
    assert(decoded_normal.device_id == normal.device_id);
    assert(decoded_normal.wireless_counter == normal.wireless_counter);
    assert(decoded_normal.receiver_status == normal.receiver_status);
    assert(decoded_normal.snr == normal.snr);
}

bool read_all(int fd, std::uint8_t* bytes, std::size_t size) {
    std::size_t received = 0;
    while (received < size) {
        const auto result = ::read(fd, bytes + received, size - received);
        if (result > 0) {
            received += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

bool write_all(int fd, const std::uint8_t* bytes, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
        const auto result = ::write(fd, bytes + sent, size - sent);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

enum class BadResponse { Crc, Command, Transaction, Result };

Frame successful_sdo_response(const Frame& request, std::uint32_t object_data) {
    const auto request_frame = SdoFrame::from_frame(request);
    SdoFrame response{};
    response.cmd = SystemCommand::ReadParamRsp;
    response.transaction_id = request_frame.transaction_id;
    response.object_index =
        static_cast<std::uint16_t>((request_frame.object_index & 0x0fff) | 0x4000);
    response.object_data = object_data;
    return response.to_frame();
}

Frame bad_lora_response(const Frame& request, BadResponse kind) {
    LoRaParamFrame response{};
    response.cmd = static_cast<std::uint8_t>(SystemCommand::ReadParamRsp);
    response.transaction_id = read_le32(request.bytes() + 1);
    response.object_index = 0x4000;
    response.param_flags = ParamFlags{}.to_flags();
    auto bytes = response.to_frame();
    switch (kind) {
        case BadResponse::Crc:
            bytes[23] ^= 1;
            break;
        case BadResponse::Command:
            bytes[0] = static_cast<std::uint8_t>(SystemCommand::WriteParamRsp);
            fill_crc(bytes);
            break;
        case BadResponse::Transaction:
            write_le32(bytes.bytes() + 1, response.transaction_id + 1);
            fill_crc(bytes);
            break;
        case BadResponse::Result:
            bytes[39] = static_cast<std::uint8_t>(ResultCode::Failure);
            fill_crc(bytes);
            break;
    }
    return bytes;
}

void test_client_response_validation(BadResponse kind, Error expected) {
    int master = -1;
    int slave = -1;
    char path[128]{};
    assert(::openpty(&master, &slave, path, nullptr, nullptr) == 0);
    std::thread device([master, kind] {
        Frame request{};
        for (std::uint32_t value = 1; value <= 3; ++value) {
            assert(read_all(master, request.bytes(), request.size()));
            const auto response = successful_sdo_response(request, value);
            assert(write_all(master, response.bytes(), response.size()));
        }
        assert(read_all(master, request.bytes(), request.size()));
        const auto response = bad_lora_response(request, kind);
        assert(write_all(master, response.bytes(), response.size()));
    });
    Client client{};
    assert(client.open(path, std::chrono::milliseconds(100), std::chrono::milliseconds(0), 1));
    const auto result = client.read_lora_parameters();
    assert(!result && result.error() == expected);
    (void)client.close();
    device.join();
    assert(::close(master) == 0);
    assert(::close(slave) == 0);
}

void test_client_rejects_invalid_responses() {
    test_client_response_validation(BadResponse::Crc, Error::CrcMismatch);
    test_client_response_validation(BadResponse::Command, Error::UnexpectedResponse);
    test_client_response_validation(BadResponse::Transaction, Error::TimedOut);
    test_client_response_validation(BadResponse::Result, Error::DeviceRejected);
}

void test_client_reads_pin_with_ascii_zero_request() {
    int master = -1;
    int slave = -1;
    char path[128]{};
    assert(::openpty(&master, &slave, path, nullptr, nullptr) == 0);
    std::thread device([master] {
        Frame request{};
        for (std::uint32_t value = 1; value <= 3; ++value) {
            assert(read_all(master, request.bytes(), request.size()));
            const auto response = successful_sdo_response(request, value);
            assert(write_all(master, response.bytes(), response.size()));
        }
        assert(read_all(master, request.bytes(), request.size()));
        assert(request[0] == static_cast<std::uint8_t>(SystemCommand::PinConfigReq));
        for (std::size_t index = 1; index <= 6; ++index)
            assert(request[index] == static_cast<std::uint8_t>('0'));

        PinFrame response{};
        response.cmd = SystemCommand::PinConfigRsp;
        response.pin = {'1', '2', '3', '4', '5', '6'};
        response.transaction_id = read_le32(request.bytes() + 12);
        const auto response_bytes = response.to_frame();
        assert(write_all(master, response_bytes.bytes(), response_bytes.size()));
    });
    Client client{};
    assert(client.open(path, std::chrono::milliseconds(100), std::chrono::milliseconds(0), 1));
    const auto result = client.read_pin();
    assert(result);
    assert((result.value().pin == std::array<std::uint8_t, 6>{'1', '2', '3', '4', '5', '6'}));
    (void)client.close();
    device.join();
    assert(::close(master) == 0);
    assert(::close(slave) == 0);
}

void test_client_reads_device_id_with_normal_communication() {
    int master = -1;
    int slave = -1;
    char path[128]{};
    assert(::openpty(&master, &slave, path, nullptr, nullptr) == 0);
    std::thread device([master] {
        Frame request{};
        for (std::uint32_t value = 1; value <= 3; ++value) {
            assert(read_all(master, request.bytes(), request.size()));
            const auto response = successful_sdo_response(request, value);
            assert(write_all(master, response.bytes(), response.size()));
        }
        assert(read_all(master, request.bytes(), request.size()));
        assert(request[0] == static_cast<std::uint8_t>(SystemCommand::RunStatusReq));
        for (std::size_t index = 1; index < kFrameBodySize; ++index) assert(request[index] == 0);
        NormalFrame response{};
        response.cmd = SystemCommand::RunStatusRsp;
        response.device_id = {0xa1, 0xb2, 0xc3};
        const auto response_bytes = response.to_frame();
        assert(write_all(master, response_bytes.bytes(), response_bytes.size()));
    });
    Client client{};
    assert(client.open(path, std::chrono::milliseconds(100), std::chrono::milliseconds(0), 1));
    const auto result = client.read_device_id();
    assert(result);
    assert((result.value() == std::array<std::uint8_t, 3>{0xa1, 0xb2, 0xc3}));
    (void)client.close();
    device.join();
    assert(::close(master) == 0);
    assert(::close(slave) == 0);
}

void test_client_rejects_invalid_gfsk_parameters() {
    int master = -1;
    int slave = -1;
    char path[128]{};
    assert(::openpty(&master, &slave, path, nullptr, nullptr) == 0);
    std::thread device([master] {
        Frame request{};
        for (std::uint32_t value = 1; value <= 3; ++value) {
            assert(read_all(master, request.bytes(), request.size()));
            const auto response = successful_sdo_response(request, value);
            assert(write_all(master, response.bytes(), response.size()));
        }
    });
    Client client{};
    assert(client.open(path, std::chrono::milliseconds(100), std::chrono::milliseconds(0), 1));

    GfskParamFrame frame{};
    frame.param_flags = 0x4000;
    frame.tx_power = 10;
    frame.freq_offset = 250;
    frame.payload_len = 12;
    frame.rssi_threshold = 110;
    frame.heartbeat_interval = 200;
    frame.heartbeat_loss = 3;
    frame.bandwidth = 1;
    frame.bitrate = 599;
    frame.freq_deviation = 25000;
    frame.pulse_shaping = 0x09;
    frame.preamble_len = 16;
    frame.sync_word = 0x1424;
    const auto result = client.write_gfsk_parameters(frame);
    assert(!result && result.error() == Error::InvalidArgument);

    (void)client.close();
    device.join();
    assert(::close(master) == 0);
    assert(::close(slave) == 0);
}
enum class ReservedResponse {
    LoRaRead,
    LoRaWrite,
    GfskRead,
    GfskWrite,
    PinReadFirst,
    PinReadSecond,
    PinWrite,
    Bind,
    Find,
    Unbind,
    RunStatus,
};

Frame response_with_nonzero_reserved(const Frame& request, ReservedResponse kind) {
    Frame response{};
    std::size_t reserved_offset = 0;
    switch (kind) {
        case ReservedResponse::LoRaRead:
        case ReservedResponse::LoRaWrite: {
            LoRaParamFrame frame{};
            frame.cmd = static_cast<std::uint8_t>(
                kind == ReservedResponse::LoRaRead ? SystemCommand::ReadParamRsp
                                                   : SystemCommand::WriteParamRsp);
            frame.transaction_id = read_le32(request.bytes() + 1);
            frame.object_index = kind == ReservedResponse::LoRaRead ? 0x4000 : 0x6000;
            frame.param_flags = ParamFlags{}.to_flags();
            frame.tx_power = 10;
            frame.payload_len = 12;
            frame.rssi_threshold = 110;
            frame.heartbeat_interval = 200;
            frame.heartbeat_loss = 3;
            frame.bandwidth = 1;
            frame.spreading_factor = 6;
            frame.coding_rate = 4;
            frame.preamble_len = 12;
            frame.sync_word = 0x1424;
            response = frame.to_frame();
            reserved_offset = 29;
            break;
        }
        case ReservedResponse::GfskRead:
        case ReservedResponse::GfskWrite: {
            GfskParamFrame frame{};
            frame.cmd = static_cast<std::uint8_t>(
                kind == ReservedResponse::GfskRead ? SystemCommand::ReadParamRsp
                                                   : SystemCommand::WriteParamRsp);
            frame.transaction_id = read_le32(request.bytes() + 1);
            frame.object_index = kind == ReservedResponse::GfskRead ? 0x4000 : 0x6000;
            frame.param_flags = 0x4000;
            frame.tx_power = 10;
            frame.payload_len = 12;
            frame.rssi_threshold = 110;
            frame.heartbeat_interval = 200;
            frame.heartbeat_loss = 3;
            frame.bandwidth = 1;
            frame.bitrate = 50000;
            frame.freq_deviation = 25000;
            frame.pulse_shaping = 0x09;
            frame.preamble_len = 16;
            frame.sync_word = 0x1424;
            response = frame.to_frame();
            reserved_offset = 35;
            break;
        }
        case ReservedResponse::PinReadFirst:
        case ReservedResponse::PinReadSecond:
        case ReservedResponse::PinWrite: {
            PinFrame frame{};
            frame.cmd = SystemCommand::PinConfigRsp;
            frame.pin = kind == ReservedResponse::PinWrite
                            ? std::array<std::uint8_t, 6>{'1', '2', '3', '4', '5', '6'}
                            : std::array<std::uint8_t, 6>{'6', '5', '4', '3', '2', '1'};
            frame.transaction_id = read_le32(request.bytes() + 12);
            response = frame.to_frame();
            reserved_offset = kind == ReservedResponse::PinReadFirst ? 7 : 16;
            break;
        }
        case ReservedResponse::Bind:
        case ReservedResponse::Find:
        case ReservedResponse::Unbind: {
            DeviceKeyFrame frame{};
            frame.cmd = kind == ReservedResponse::Bind
                            ? SystemCommand::BindRsp
                            : (kind == ReservedResponse::Find ? SystemCommand::FindRsp
                                                              : SystemCommand::UnbindRsp);
            frame.device_id = {1, 2, 3};
            frame.transaction_id = read_le32(request.bytes() + 20);
            response = frame.to_frame();
            reserved_offset = 24;
            break;
        }
        case ReservedResponse::RunStatus: {
            NormalFrame frame{};
            frame.cmd = SystemCommand::RunStatusRsp;
            frame.device_id = {1, 2, 3};
            response = frame.to_frame();
            reserved_offset = 28;
            break;
        }
    }
    response[reserved_offset] = 1;
    fill_crc(response);
    assert(has_valid_crc(response));
    return response;
}

void test_client_rejects_nonzero_reserved(ReservedResponse kind) {
    int master = -1;
    int slave = -1;
    char path[128]{};
    assert(::openpty(&master, &slave, path, nullptr, nullptr) == 0);
    std::thread device([master, kind] {
        Frame request{};
        for (std::uint32_t value = 1; value <= 3; ++value) {
            assert(read_all(master, request.bytes(), request.size()));
            const auto response = successful_sdo_response(request, value);
            assert(write_all(master, response.bytes(), response.size()));
        }
        assert(read_all(master, request.bytes(), request.size()));
        const auto response = response_with_nonzero_reserved(request, kind);
        assert(write_all(master, response.bytes(), response.size()));
    });
    Client client{};
    assert(client.open(path, std::chrono::milliseconds(100), std::chrono::milliseconds(0), 1));
    bool rejected = false;
    switch (kind) {
        case ReservedResponse::LoRaRead: {
            const auto result = client.read_lora_parameters();
            rejected = !result && result.error() == Error::InvalidResponse;
            break;
        }
        case ReservedResponse::LoRaWrite: {
            LoRaParamFrame frame{};
            frame.param_flags = 0;
            frame.tx_power = 10;
            frame.payload_len = 12;
            frame.rssi_threshold = 110;
            frame.heartbeat_interval = 200;
            frame.heartbeat_loss = 3;
            frame.bandwidth = 1;
            frame.spreading_factor = 6;
            frame.coding_rate = 4;
            frame.preamble_len = 12;
            frame.sync_word = 0x1424;
            const auto result = client.write_lora_parameters(frame);
            rejected = !result && result.error() == Error::InvalidResponse;
            break;
        }
        case ReservedResponse::GfskRead: {
            const auto result = client.read_gfsk_parameters();
            rejected = !result && result.error() == Error::InvalidResponse;
            break;
        }
        case ReservedResponse::GfskWrite: {
            GfskParamFrame frame{};
            frame.param_flags = 0x4000;
            frame.tx_power = 10;
            frame.payload_len = 12;
            frame.rssi_threshold = 110;
            frame.heartbeat_interval = 200;
            frame.heartbeat_loss = 3;
            frame.bandwidth = 1;
            frame.bitrate = 50000;
            frame.freq_deviation = 25000;
            frame.pulse_shaping = 0x09;
            frame.preamble_len = 16;
            frame.sync_word = 0x1424;
            const auto result = client.write_gfsk_parameters(frame);
            rejected = !result && result.error() == Error::InvalidResponse;
            break;
        }
        case ReservedResponse::PinReadFirst:
        case ReservedResponse::PinReadSecond: {
            const auto result = client.read_pin();
            rejected = !result && result.error() == Error::InvalidResponse;
            break;
        }
        case ReservedResponse::PinWrite: {
            PinFrame frame{};
            frame.pin = {'1', '2', '3', '4', '5', '6'};
            const auto result = client.write_pin(frame);
            rejected = !result && result.error() == Error::InvalidResponse;
            break;
        }
        case ReservedResponse::Bind: {
            const auto result = client.prepare_binding();
            rejected = !result && result.error() == Error::InvalidResponse;
            break;
        }
        case ReservedResponse::Find:
        case ReservedResponse::Unbind: {
            DeviceKeyFrame frame{};
            frame.device_id = {1, 2, 3};
            frame.transaction_id = 7;
            const auto result = kind == ReservedResponse::Find ? client.find_binding(frame)
                                                               : client.cancel_binding(frame);
            rejected = !result && result.error() == Error::InvalidResponse;
            break;
        }
        case ReservedResponse::RunStatus: {
            const auto result = client.read_device_id();
            rejected = !result && result.error() == Error::InvalidResponse;
            break;
        }
    }
    assert(rejected);
    (void)client.close();
    device.join();
    assert(::close(master) == 0);
    assert(::close(slave) == 0);
}

void test_invalid_default_modulation() {
    Client client{};
    const auto result = client.restore_default_parameters(static_cast<ParamFlags::Modulation>(2));
    assert(!result && result.error() == Error::InvalidArgument);
}

}  // namespace

int main() {
    test_flags();
    test_lora_frame();
    test_gfsk_frame();
    test_pin_device_key_and_sdo();
    test_client_rejects_invalid_responses();
    test_client_reads_pin_with_ascii_zero_request();
    test_client_reads_device_id_with_normal_communication();
    test_client_rejects_invalid_gfsk_parameters();
    for (const auto kind :
         {ReservedResponse::LoRaRead, ReservedResponse::LoRaWrite, ReservedResponse::GfskRead,
          ReservedResponse::GfskWrite, ReservedResponse::PinReadFirst,
          ReservedResponse::PinReadSecond, ReservedResponse::PinWrite, ReservedResponse::Bind,
          ReservedResponse::Find, ReservedResponse::Unbind, ReservedResponse::RunStatus}) {
        test_client_rejects_nonzero_reserved(kind);
    }
    test_invalid_default_modulation();
}
