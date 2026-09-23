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
    flags.band = Band::MHz915;
    flags.crc_enabled = false;
    flags.estop_enabled = false;
    flags.heartbeat_enabled = false;
    flags.one_to_one = false;
    flags.channel_scan = true;
    flags.radio_type = RadioType::GFSK;
    assert(flags.to_raw() == 0x403f);
    const auto decoded = ParamFlags::from_raw(0x403f);
    assert(decoded.band == Band::MHz915 && !decoded.crc_enabled && !decoded.estop_enabled);
    assert(!decoded.heartbeat_enabled && !decoded.one_to_one && decoded.channel_scan);
    assert(decoded.radio_type == RadioType::GFSK);
}

void test_lora_frame() {
    LoRaParamFrame frame{};
    frame.cmd = static_cast<std::uint8_t>(SystemCmd::ParamWriteReq);
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
    const auto raw = frame.to_bytes();
    assert(raw[0] == 0x05 && raw[1] == 0x11 && raw[4] == 0x44);
    assert(raw[5] == 0x23 && raw[6] == 0x61 && raw[7] == 0x55 && raw[10] == 0x88);
    assert(raw[13] == 0xf4 && raw[14] == 0xff && raw[27] == 0x24 && raw[28] == 0x14);
    assert(raw[38] == 0xa5 && raw[39] == 0xff && has_valid_crc(raw));
    assert(read_le16(raw.bytes() + kFrameBodySize) != frame.crc16);
    const auto round_trip = LoRaParamFrame::from_bytes(raw);
    assert(round_trip.transaction_id == frame.transaction_id && round_trip.tx_power == -12);
    assert(round_trip.reserved == frame.reserved && has_valid_crc(raw));
    auto corrupt = raw;
    corrupt[24] ^= 0x01;
    assert(!has_valid_crc(corrupt));
}

void test_gfsk_frame() {
    GfskParamFrame frame{};
    frame.cmd = static_cast<std::uint8_t>(SystemCmd::ParamWriteReq);
    frame.transaction_id = 7;
    frame.param_flags = 0x4000;
    frame.bandwidth = 2;
    frame.bitrate = 50000;
    frame.freq_deviation = 25000;
    frame.pulse_shaping = 0x09;
    frame.preamble_len = 16;
    frame.sync_word = 0x1424;
    const auto raw = frame.to_bytes();
    assert(raw[22] == 2 && read_le32(raw.bytes() + 23) == 50000);
    assert(read_le32(raw.bytes() + 27) == 25000 && raw[31] == 0x09 && raw[32] == 16);
    assert(raw[33] == 0x24 && raw[34] == 0x14 && has_valid_crc(raw));
    const auto round_trip = GfskParamFrame::from_bytes(raw);
    assert(round_trip.bitrate == 50000 && round_trip.freq_deviation == 25000);
}

void test_pin_device_key_and_sdo() {
    PinFrame pin{};
    pin.cmd = static_cast<std::uint8_t>(SystemCmd::PinCfgReq);
    pin.pin = {'1', '2', '3', '4', '5', '6'};
    pin.transaction_id = 0x01020304;
    const auto pin_bytes = pin.to_bytes();
    assert(pin_bytes[1] == '1' && pin_bytes[6] == '6' && pin_bytes[12] == 0x04);
    assert(
        has_valid_crc(pin_bytes) &&
        PinFrame::from_bytes(pin_bytes).transaction_id == pin.transaction_id);

    DeviceKeyFrame key{};
    key.cmd = static_cast<std::uint8_t>(SystemCmd::BindReq);
    key.device_id = {1, 2, 3};
    key.kbind[0] = 0x42;
    key.transaction_id = 9;
    const auto key_bytes = key.to_bytes();
    assert(key_bytes[1] == 1 && key_bytes[3] == 3 && key_bytes[4] == 0x42 && key_bytes[20] == 9);
    assert(DeviceKeyFrame::from_bytes(key_bytes).kbind[0] == 0x42);

    SdoFrame sdo{};
    sdo.cmd = static_cast<std::uint8_t>(SystemCmd::ParamReadRsp);
    sdo.transaction_id = 12;
    sdo.object_index = 0x4001;
    sdo.object_data = 0x44332211;
    const auto sdo_bytes = sdo.to_bytes();
    assert(sdo_bytes[5] == 1 && sdo_bytes[6] == 0x40 && sdo_bytes[11] == 0);
    const auto decoded = SdoFrame::from_bytes(sdo_bytes);
    assert(decoded.object_index == 0x4001 && decoded.object_data == 0x44332211);

    NormalFrame normal{};
    normal.cmd = static_cast<std::uint8_t>(SystemCmd::NormalRsp);
    normal.device_id = {0xa1, 0xb2, 0xc3};
    normal.wireless_counter = 0x81234567;
    normal.receiver_status = 0x0011;
    normal.rssi = 72;
    normal.snr = -24;
    const auto normal_bytes = normal.to_bytes();
    assert(normal_bytes[0] == 0x86 && normal_bytes[1] == 0xa1 && normal_bytes[3] == 0xc3);
    assert(normal_bytes[4] == 0x67 && normal_bytes[7] == 0x81 && has_valid_crc(normal_bytes));
    const auto decoded_normal = NormalFrame::from_bytes(normal_bytes);
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

Bytes successful_sdo_response(const Bytes& request, std::uint32_t object_data) {
    const auto request_frame = SdoFrame::from_bytes(request);
    SdoFrame response{};
    response.cmd = static_cast<std::uint8_t>(SystemCmd::ParamReadRsp);
    response.transaction_id = request_frame.transaction_id;
    response.object_index =
        static_cast<std::uint16_t>((request_frame.object_index & 0x0fff) | 0x4000);
    response.object_data = object_data;
    return response.to_bytes();
}
Bytes bad_lora_response(const Bytes& request, BadResponse kind) {
    LoRaParamFrame response{};
    response.cmd = static_cast<std::uint8_t>(SystemCmd::ParamReadRsp);
    response.transaction_id = read_le32(request.bytes() + 1);
    response.object_index = 0x4000;
    response.param_flags = ParamFlags{}.to_raw();
    auto bytes = response.to_bytes();
    switch (kind) {
        case BadResponse::Crc:
            bytes[23] ^= 1;
            break;
        case BadResponse::Command:
            bytes[0] = static_cast<std::uint8_t>(SystemCmd::ParamWriteRsp);
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
        Bytes request{};
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
        Bytes request{};
        for (std::uint32_t value = 1; value <= 3; ++value) {
            assert(read_all(master, request.bytes(), request.size()));
            const auto response = successful_sdo_response(request, value);
            assert(write_all(master, response.bytes(), response.size()));
        }
        assert(read_all(master, request.bytes(), request.size()));
        assert(request[0] == static_cast<std::uint8_t>(SystemCmd::PinCfgReq));
        for (std::size_t index = 1; index <= 6; ++index)
            assert(request[index] == static_cast<std::uint8_t>('0'));

        PinFrame response{};
        response.cmd = static_cast<std::uint8_t>(SystemCmd::PinCfgRsp);
        response.pin = {'1', '2', '3', '4', '5', '6'};
        response.transaction_id = read_le32(request.bytes() + 12);
        const auto response_bytes = response.to_bytes();
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
        Bytes request{};
        for (std::uint32_t value = 1; value <= 3; ++value) {
            assert(read_all(master, request.bytes(), request.size()));
            const auto response = successful_sdo_response(request, value);
            assert(write_all(master, response.bytes(), response.size()));
        }
        assert(read_all(master, request.bytes(), request.size()));
        assert(request[0] == static_cast<std::uint8_t>(SystemCmd::NormalReq));
        for (std::size_t index = 1; index < kFrameBodySize; ++index) assert(request[index] == 0);
        NormalFrame response{};
        response.cmd = static_cast<std::uint8_t>(SystemCmd::NormalRsp);
        response.device_id = {0xa1, 0xb2, 0xc3};
        const auto response_bytes = response.to_bytes();
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
        Bytes request{};
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
}
