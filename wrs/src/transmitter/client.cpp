#include <transmitter/client.hpp>

#include <wrs/logging.hpp>

#include <chrono>
#include <thread>
#include <utility>

namespace transmitter {
namespace {
constexpr std::uint32_t kUpgradeRequestValue = 0x454e;
template <typename T>
using Result = wrs::Result<T, Error>;

Error map_serial_error(serial::Error error) noexcept {
    switch (error) {
        case serial::Error::InvalidArgument:
            return Error::InvalidArgument;
        case serial::Error::InvalidState:
            return Error::InvalidState;
        case serial::Error::AlreadyOpen:
            return Error::AlreadyOpen;
        case serial::Error::NotOpen:
            return Error::NotOpen;
        case serial::Error::Unsupported:
            return Error::Unsupported;
        case serial::Error::DeviceNotFound:
            return Error::DeviceNotFound;
        case serial::Error::PermissionDenied:
            return Error::PermissionDenied;
        case serial::Error::Busy:
            return Error::Busy;
        case serial::Error::Disconnected:
            return Error::Disconnected;
        case serial::Error::TimedOut:
            return Error::TimedOut;
        default:
            return Error::Io;
    }
}

serial::Config transmitter_config() noexcept {
    serial::Config config{};
    config.baud_rate = 115200;
    config.data_bits = serial::DataBits::Eight;
    config.parity = serial::Parity::None;
    config.stop_bits = serial::StopBits::One;
    config.flow_control = serial::FlowControl::None;
    return config;
}

bool valid_options(
    std::chrono::milliseconds response_timeout, std::chrono::milliseconds retry_interval,
    std::uint8_t max_attempts) noexcept {
    return response_timeout.count() > 0 && retry_interval.count() >= 0 && max_attempts > 0;
}

bool probe_protocol_error(Error error) noexcept {
    switch (error) {
        case Error::InvalidResponse:
        case Error::UnexpectedResponse:
        case Error::CrcMismatch:
        case Error::DeviceRejected:
        case Error::SdoNotReceived:
        case Error::SdoInProgress:
        case Error::SdoError:
        case Error::SdoInvalidCommand:
            return true;
        default:
            return false;
    }
}

bool all_zero_pin(const PinFrame& frame) noexcept {
    for (const auto digit : frame.pin) {
        if (digit != 0 && digit != static_cast<std::uint8_t>('0')) {
            return false;
        }
    }
    return true;
}

bool valid_pin(const PinFrame& frame) noexcept {
    for (const auto digit : frame.pin) {
        if (digit < static_cast<std::uint8_t>('0') || digit > static_cast<std::uint8_t>('9')) {
            return false;
        }
    }
    return true;
}

bool is_zero(const std::uint8_t* bytes, std::size_t size) noexcept {
    for (std::size_t index = 0; index < size; ++index) {
        if (bytes[index] != 0) {
            return false;
        }
    }
    return true;
}

bool valid_parameter_flags(std::uint16_t raw_flags, ParamFlags::Modulation expected_type) noexcept {
    constexpr std::uint16_t kReservedBits = 0x3fc0;
    const auto modulation = static_cast<std::uint8_t>((raw_flags >> 14) & 0x3);
    if ((raw_flags & kReservedBits) != 0 || modulation > 1) return false;
    return modulation == static_cast<std::uint8_t>(expected_type);
}

template <typename Frame>
bool valid_common_parameters(const Frame& frame) noexcept {
    const auto band = ParamFlags::from_flags(frame.param_flags).band;
    bool result = frame.tx_power >= 0 && frame.tx_power <= 22;
    result = result && (band != ParamFlags::Band::MHz433 || frame.tx_power <= 10);
    result = result && (frame.payload_len == 12);
    result = result && (frame.rssi_threshold >= 10 && frame.rssi_threshold <= 148);
    result = result && (frame.heartbeat_interval >= 200 && frame.heartbeat_interval <= 10000);
    result = result && (frame.heartbeat_loss >= 1 && frame.bandwidth <= 2);
    return result;
}

bool valid_lora_fields(const LoRaParamFrame& frame) noexcept {
    bool result = valid_common_parameters(frame);
    result = result && (frame.spreading_factor >= 5 && frame.spreading_factor <= 12);
    result = result && (frame.coding_rate <= 6 && frame.header_type <= 1);
    result = result && (frame.preamble_len >= 10 && frame.preamble_len <= 50);
    result = result && ((frame.spreading_factor != 5 && frame.spreading_factor != 6) ||
                        frame.preamble_len == 12);
    result = result && ((frame.sync_word & 0x0f0f) == 0x0404);
    return result;
}

bool valid_gfsk_fields(const GfskParamFrame& frame) noexcept {
    bool result = valid_common_parameters(frame);
    result = result && (frame.bitrate >= 600 && frame.bitrate <= 150000);
    result = result && (frame.freq_deviation >= 600 && frame.freq_deviation <= 300000);
    result = result && (static_cast<std::uint64_t>(frame.freq_deviation) * 4 >= frame.bitrate);
    result = result && (frame.pulse_shaping == 0 ||
                        (frame.pulse_shaping >= 0x08 && frame.pulse_shaping <= 0x0b));
    result = result && (frame.preamble_len >= 16);
    return result;
}

bool valid_lora_parameters(const LoRaParamFrame& frame) noexcept {
    bool result = frame.object_index == 0 && frame.object_data == 0 && frame.result_code == 0;
    result = result && (is_zero(frame.reserved.data(), frame.reserved.size()));
    result = result && (valid_parameter_flags(frame.param_flags, ParamFlags::Modulation::LoRa));
    result = result && (valid_lora_fields(frame));
    return result;
}

bool valid_gfsk_parameters(const GfskParamFrame& frame) noexcept {
    bool result = frame.object_index == 0 && frame.object_data == 0 && frame.result_code == 0;
    result = result && (is_zero(frame.reserved.data(), frame.reserved.size()));
    result = result && (valid_parameter_flags(frame.param_flags, ParamFlags::Modulation::Gfsk));
    result = result && (valid_gfsk_fields(frame));
    return result;
}

bool valid_lora_response(const LoRaParamFrame& frame) noexcept {
    bool result = frame.object_index == 0x4000 && frame.object_data == 0;
    result = result && (valid_parameter_flags(frame.param_flags, ParamFlags::Modulation::LoRa));
    result = result && (valid_lora_fields(frame));
    return result;
}

bool valid_gfsk_response(const GfskParamFrame& frame) noexcept {
    bool result = frame.object_index == 0x4000 && frame.object_data == 0;
    result = result && (valid_parameter_flags(frame.param_flags, ParamFlags::Modulation::Gfsk));
    result = result && (valid_gfsk_fields(frame));
    return result;
}

Error sdo_status_error(std::uint16_t status) noexcept {
    switch (status) {
        case static_cast<std::uint16_t>(SdoStatus::NotReceived):
            return Error::SdoNotReceived;
        case static_cast<std::uint16_t>(SdoStatus::Error):
            return Error::SdoError;
        case static_cast<std::uint16_t>(SdoStatus::InProgress):
            return Error::SdoInProgress;
        case static_cast<std::uint16_t>(SdoStatus::InvalidCmd):
            return Error::SdoInvalidCommand;
        default:
            return Error::InvalidResponse;
    }
}

// 解码器不保存保留区，响应必须直接检查原始帧。
bool reserved_bytes_are_zero(const Frame& frame, std::size_t offset, std::size_t size) noexcept {
    return is_zero(frame.bytes() + offset, size);
}

bool valid_pin_reserved_bytes(const Frame& frame) noexcept {
    return reserved_bytes_are_zero(frame, 7, 5) && reserved_bytes_are_zero(frame, 16, 23);
}

std::uint32_t response_transaction_id(const Frame& frame, SystemCommand expected) noexcept {
    if (expected == SystemCommand::RunStatusRsp) return 0;
    if (expected == SystemCommand::PinConfigRsp) return read_le32(frame.bytes() + 12);
    if (expected == SystemCommand::BindRsp || expected == SystemCommand::FindRsp ||
        expected == SystemCommand::UnbindRsp)
        return read_le32(frame.bytes() + 20);
    return read_le32(frame.bytes() + 1);
}

Frame parameter_read_request(
    std::uint32_t transaction_id, ParamFlags::Modulation modulation) noexcept {
    Frame request{};
    request[0] = static_cast<std::uint8_t>(SystemCommand::ReadParamReq);
    write_le32(request.bytes() + 1, transaction_id);
    ParamFlags flags{};
    flags.modulation = modulation;
    write_le16(request.bytes() + 11, flags.to_flags());
    fill_crc(request);
    return request;
}

LoRaParamFrame default_lora_frame() noexcept {
    LoRaParamFrame frame{};
    ParamFlags flags{};
    flags.modulation = ParamFlags::Modulation::LoRa;
    frame.param_flags = flags.to_flags();
    frame.tx_power = 10;
    frame.freq_offset = 250;
    frame.payload_len = 12;
    frame.rssi_threshold = 110;
    frame.heartbeat_interval = 200;
    frame.heartbeat_loss = 3;
    frame.bandwidth = 1;
    frame.spreading_factor = 6;
    frame.coding_rate = 4;
    frame.preamble_len = 12;
    frame.sync_word = 0x1424;
    return frame;
}

GfskParamFrame default_gfsk_frame() noexcept {
    GfskParamFrame frame{};
    ParamFlags flags{};
    flags.modulation = ParamFlags::Modulation::Gfsk;
    frame.param_flags = flags.to_flags();
    frame.tx_power = 10;
    frame.freq_offset = 250;
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
    return frame;
}
}  // namespace

Client::~Client() noexcept { (void)close(); }

Client::Client(Client&& other) noexcept
: port_(std::move(other.port_)),
  response_timeout_(other.response_timeout_),
  retry_interval_(other.retry_interval_),
  max_attempts_(other.max_attempts_),
  identity_(other.identity_),
  next_transaction_id_(other.next_transaction_id_) {
    other.response_timeout_ = {};
    other.retry_interval_ = {};
    other.max_attempts_ = 0;
    other.identity_ = {};
    other.next_transaction_id_ = 1;
}

wrs::Result<void, Error> Client::open(
    const std::string& path, std::chrono::milliseconds response_timeout,
    std::chrono::milliseconds retry_interval, std::uint8_t max_attempts) noexcept {
    if (is_open()) {
        WRS_LOG_ERROR("transmitter.client") << "Open rejected: client is already open";
        return Result<void>::failure(Error::AlreadyOpen);
    }
    if (path.empty() || !valid_options(response_timeout, retry_interval, max_attempts)) {
        WRS_LOG_ERROR("transmitter.client") << "Open rejected: invalid options";
        return Result<void>::failure(Error::InvalidArgument);
    }
    WRS_LOG_INFO("transmitter.client") << "Open request\n  path: " << path;
    auto opened = port_.open(path, transmitter_config());
    if (!opened) {
        WRS_LOG_ERROR("transmitter.client")
            << "Open failed\n  path: " << path
            << "\n  error: " << static_cast<unsigned int>(opened.error());
        return Result<void>::failure(map_serial_error(opened.error()));
    }
    response_timeout_ = response_timeout;
    retry_interval_ = retry_interval;
    max_attempts_ = max_attempts;
    identity_ = {};
    auto discarded = port_.discard_input();
    if (!discarded) {
        const auto error = map_serial_error(discarded.error());
        WRS_LOG_ERROR("transmitter.client")
            << "Open failed while discarding input\n  error: " << static_cast<unsigned int>(error);
        (void)close();
        return Result<void>::failure(error);
    }
    auto product = read_sdo(SdoObject::ProductCode);
    auto version =
        product ? read_sdo(SdoObject::VersionNumber) : Result<SdoFrame>::failure(product.error());
    auto serial =
        version ? read_sdo(SdoObject::SerialNumber) : Result<SdoFrame>::failure(version.error());
    if (!product || !version || !serial) {
        Error error = !product ? product.error() : (!version ? version.error() : serial.error());
        if (probe_protocol_error(error)) {
            error = Error::ProtocolMismatch;
        }
        WRS_LOG_ERROR("transmitter.client") << "Open failed during identity verification\n  error: "
                                            << static_cast<unsigned int>(error);
        (void)close();
        return Result<void>::failure(error);
    }
    identity_ = {
        product.value().object_data, version.value().object_data, serial.value().object_data};
    WRS_LOG_INFO("transmitter.client")
        << "Open succeeded\n  path: " << path << "\n  product_code: " << identity_.product_code
        << "\n  version: " << identity_.version_number
        << "\n  serial_number: " << identity_.serial_number;
    return Result<void>::success();
}

wrs::Result<void, Error> Client::close() noexcept {
    WRS_LOG_INFO("transmitter.client") << "Close request";
    identity_ = {};
    auto close_result = port_.close();
    if (close_result) {
        WRS_LOG_INFO("transmitter.client") << "Close succeeded";
        return Result<void>::success();
    }
    const auto error = map_serial_error(close_result.error());
    WRS_LOG_ERROR("transmitter.client")
        << "Close failed\n  error: " << static_cast<unsigned int>(error);
    return Result<void>::failure(error);
}

bool Client::is_open() const noexcept { return port_.is_open(); }

const DeviceIdentity& Client::identity() const noexcept { return identity_; }

wrs::Result<std::array<std::uint8_t, 3>, Error> Client::read_device_id() noexcept {
    WRS_LOG_INFO("transmitter.client") << "Read device ID request";
    if (!is_open()) {
        WRS_LOG_ERROR("transmitter.client") << "Read device ID failed: client is not open";
        return Result<std::array<std::uint8_t, 3>>::failure(Error::NotOpen);
    }
    NormalFrame request{};
    request.cmd = SystemCommand::RunStatusReq;
    auto response = exchange(request.to_frame(), 0, SystemCommand::RunStatusRsp);
    if (!response) {
        WRS_LOG_ERROR("transmitter.client")
            << "Read device ID failed\n  error: " << static_cast<unsigned int>(response.error());
        return Result<std::array<std::uint8_t, 3>>::failure(response.error());
    }
    if (!reserved_bytes_are_zero(response.value(), 28, 11)) {
        WRS_LOG_ERROR("transmitter.client") << "Read device ID failed: reserved bytes are nonzero";
        return Result<std::array<std::uint8_t, 3>>::failure(Error::InvalidResponse);
    }
    const auto frame = NormalFrame::from_frame(response.value());
    WRS_LOG_INFO("transmitter.client")
        << "Read device ID response\n  device_id: " << static_cast<unsigned int>(frame.device_id[0])
        << "." << static_cast<unsigned int>(frame.device_id[1]) << "."
        << static_cast<unsigned int>(frame.device_id[2]);
    return Result<std::array<std::uint8_t, 3>>::success(frame.device_id);
}

wrs::Result<DeviceKeyFrame, Error> Client::prepare_binding() noexcept {
    WRS_LOG_INFO("transmitter.client") << "Prepare binding request\n  kbind: <redacted>";
    if (!is_open()) {
        WRS_LOG_ERROR("transmitter.client") << "Prepare binding failed: client is not open";
        return Result<DeviceKeyFrame>::failure(Error::NotOpen);
    }
    DeviceKeyFrame request{};
    request.cmd = SystemCommand::BindReq;
    request.transaction_id = next_transaction();
    auto response = exchange(request.to_frame(), request.transaction_id, SystemCommand::BindRsp);
    if (!response) {
        WRS_LOG_ERROR("transmitter.client")
            << "Prepare binding failed\n  transaction_id: " << request.transaction_id
            << "\n  error: " << static_cast<unsigned int>(response.error());
        return Result<DeviceKeyFrame>::failure(response.error());
    }
    if (!reserved_bytes_are_zero(response.value(), 24, 15)) {
        WRS_LOG_ERROR("transmitter.client") << "Prepare binding failed: reserved bytes are nonzero";
        return Result<DeviceKeyFrame>::failure(Error::InvalidResponse);
    }
    auto frame = DeviceKeyFrame::from_frame(response.value());
    if (frame.device_id == std::array<std::uint8_t, 3>{}) {
        WRS_LOG_ERROR("transmitter.client") << "Prepare binding failed: response device ID is zero";
        return Result<DeviceKeyFrame>::failure(Error::InvalidResponse);
    }
    WRS_LOG_INFO("transmitter.client")
        << "Prepare binding response\n  transaction_id: " << frame.transaction_id
        << "\n  device_id: " << static_cast<unsigned int>(frame.device_id[0]) << "."
        << static_cast<unsigned int>(frame.device_id[1]) << "."
        << static_cast<unsigned int>(frame.device_id[2]) << "\n  kbind: <redacted>";
    return Result<DeviceKeyFrame>::success(std::move(frame));
}

wrs::Result<void, Error> Client::find_binding(const DeviceKeyFrame& binding) noexcept {
    WRS_LOG_INFO("transmitter.client")
        << "Find binding request\n  transaction_id: " << binding.transaction_id
        << "\n  device_id: " << static_cast<unsigned int>(binding.device_id[0]) << "."
        << static_cast<unsigned int>(binding.device_id[1]) << "."
        << static_cast<unsigned int>(binding.device_id[2]) << "\n  kbind: <redacted>";
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if (binding.transaction_id == 0 || binding.device_id == std::array<std::uint8_t, 3>{}) {
        WRS_LOG_ERROR("transmitter.client") << "Find binding rejected: invalid binding";
        return Result<void>::failure(Error::InvalidArgument);
    }
    DeviceKeyFrame request{};
    request.cmd = SystemCommand::FindReq;
    request.device_id = binding.device_id;
    request.kbind = binding.kbind;
    request.transaction_id = binding.transaction_id;
    auto response = exchange(request.to_frame(), request.transaction_id, SystemCommand::FindRsp);
    if (!response) return Result<void>::failure(response.error());
    if (!reserved_bytes_are_zero(response.value(), 24, 15)) {
        WRS_LOG_ERROR("transmitter.client") << "Find binding failed: reserved bytes are nonzero";
        return Result<void>::failure(Error::InvalidResponse);
    }
    const auto frame = DeviceKeyFrame::from_frame(response.value());
    if (frame.device_id != binding.device_id) {
        WRS_LOG_ERROR("transmitter.client") << "Find binding failed: device ID mismatch";
        return Result<void>::failure(Error::InvalidResponse);
    }
    WRS_LOG_INFO("transmitter.client")
        << "Find binding succeeded\n  transaction_id: " << binding.transaction_id;
    return Result<void>::success();
}

wrs::Result<void, Error> Client::cancel_binding(const DeviceKeyFrame& binding) noexcept {
    WRS_LOG_INFO("transmitter.client")
        << "Cancel binding request\n  transaction_id: " << binding.transaction_id
        << "\n  device_id: " << static_cast<unsigned int>(binding.device_id[0]) << "."
        << static_cast<unsigned int>(binding.device_id[1]) << "."
        << static_cast<unsigned int>(binding.device_id[2]);
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if (binding.transaction_id == 0 || binding.device_id == std::array<std::uint8_t, 3>{}) {
        WRS_LOG_ERROR("transmitter.client") << "Cancel binding rejected: invalid binding";
        return Result<void>::failure(Error::InvalidArgument);
    }
    DeviceKeyFrame request{};
    request.cmd = SystemCommand::UnbindReq;
    request.device_id = binding.device_id;
    request.transaction_id = binding.transaction_id;
    auto response = exchange(request.to_frame(), request.transaction_id, SystemCommand::UnbindRsp);
    if (!response) return Result<void>::failure(response.error());
    if (!reserved_bytes_are_zero(response.value(), 24, 15)) {
        WRS_LOG_ERROR("transmitter.client") << "Cancel binding failed: reserved bytes are nonzero";
        return Result<void>::failure(Error::InvalidResponse);
    }
    WRS_LOG_INFO("transmitter.client")
        << "Cancel binding succeeded\n  transaction_id: " << binding.transaction_id;
    return Result<void>::success();
}

std::uint32_t Client::next_transaction() noexcept {
    const auto value = next_transaction_id_++;
    if (next_transaction_id_ == 0) {
        next_transaction_id_ = 1;
    }
    return value == 0 ? next_transaction() : value;
}

wrs::Result<Frame, Error> Client::exchange(
    const Frame& request, std::uint32_t transaction_id, SystemCommand expected_command) noexcept {
    if (!is_open()) {
        WRS_LOG_ERROR("transmitter.exchange") << "Exchange rejected: client is not open";
        return Result<Frame>::failure(Error::NotOpen);
    }
    const bool sensitive_request =
        request[0] == static_cast<std::uint8_t>(SystemCommand::BindReq) ||
        request[0] == static_cast<std::uint8_t>(SystemCommand::FindReq) ||
        request[0] == static_cast<std::uint8_t>(SystemCommand::PinConfigReq);
    WRS_LOG_INFO("transmitter.exchange")
        << "Exchange request\n  command: " << static_cast<unsigned int>(request[0])
        << "\n  expected_command: " << static_cast<unsigned int>(expected_command)
        << "\n  transaction_id: " << transaction_id;
    WRS_LOG_DEBUG("transmitter.exchange")
        << "Request frame\n  bytes: "
        << (sensitive_request ? wrs::logging::redacted(request.size())
                              : wrs::logging::hex(request.bytes(), request.size()));
    for (std::uint8_t attempt = 0; attempt < max_attempts_; ++attempt) {
        WRS_LOG_INFO("transmitter.exchange")
            << "Exchange attempt\n  attempt: " << static_cast<unsigned int>(attempt + 1);
        if (attempt != 0 && retry_interval_ > std::chrono::milliseconds::zero()) {
            std::this_thread::sleep_for(retry_interval_);
        }
        std::size_t sent = 0;
        const auto write_deadline = std::chrono::steady_clock::now() + response_timeout_;
        while (sent < request.size()) {
            const auto remaining = write_deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::steady_clock::duration::zero()) {
                WRS_LOG_WARNING("transmitter.exchange")
                    << "Write deadline reached\n  transaction_id: " << transaction_id
                    << "\n  sent_bytes: " << sent;
                break;
            }
            auto write = port_.write(
                reinterpret_cast<const std::byte*>(request.bytes()) + sent, request.size() - sent,
                remaining);
            if (!write) {
                if (sent != 0) {
                    WRS_LOG_ERROR("transmitter.exchange")
                        << "Partial request frame\n  transaction_id: " << transaction_id
                        << "\n  sent_bytes: " << sent
                        << "\n  error: " << static_cast<unsigned int>(write.error());
                    (void)close();
                    return Result<Frame>::failure(Error::FrameSyncLost);
                }
                if (write.error() != serial::Error::TimedOut &&
                    write.error() != serial::Error::WouldBlock) {
                    const auto error = map_serial_error(write.error());
                    WRS_LOG_ERROR("transmitter.exchange")
                        << "Request write failed\n  transaction_id: " << transaction_id
                        << "\n  error: " << static_cast<unsigned int>(error);
                    if (error == Error::Disconnected) (void)close();
                    return Result<Frame>::failure(error);
                }
                break;
            }
            sent += write.value();
        }
        if (sent != request.size()) {
            WRS_LOG_WARNING("transmitter.exchange")
                << "Request was not fully written; retrying\n  transaction_id: " << transaction_id
                << "\n  sent_bytes: " << sent << "\n  expected_bytes: " << request.size();
            continue;
        }

        Frame response{};
        std::size_t received = 0;
        const auto read_deadline = std::chrono::steady_clock::now() + response_timeout_;
        while (received < response.size()) {
            const auto remaining = read_deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::steady_clock::duration::zero()) {
                WRS_LOG_WARNING("transmitter.exchange")
                    << "Response deadline reached\n  transaction_id: " << transaction_id
                    << "\n  received_bytes: " << received;
                break;
            }
            auto read = port_.read(
                reinterpret_cast<std::byte*>(response.bytes()) + received,
                response.size() - received, remaining);
            if (!read) {
                if (read.error() == serial::Error::TimedOut ||
                    read.error() == serial::Error::WouldBlock)
                    break;
                const auto error = map_serial_error(read.error());
                WRS_LOG_ERROR("transmitter.exchange")
                    << "Response read failed\n  transaction_id: " << transaction_id
                    << "\n  received_bytes: " << received
                    << "\n  error: " << static_cast<unsigned int>(error);
                if (error == Error::Disconnected) (void)close();
                return Result<Frame>::failure(error);
            }
            received += read.value();
        }
        if (received != response.size()) {
            if (received != 0) {
                WRS_LOG_ERROR("transmitter.exchange")
                    << "Partial response frame\n  transaction_id: " << transaction_id
                    << "\n  received_bytes: " << received
                    << "\n  expected_bytes: " << response.size();
                (void)close();
                return Result<Frame>::failure(Error::FrameSyncLost);
            }
            WRS_LOG_WARNING("transmitter.exchange")
                << "No response received; retrying\n  transaction_id: " << transaction_id;
            continue;
        }
        WRS_LOG_DEBUG("transmitter.exchange")
            << "Response frame\n  bytes: "
            << (sensitive_request ? wrs::logging::redacted(response.size())
                                  : wrs::logging::hex(response.bytes(), response.size()));
        if (!has_valid_crc(response)) {
            WRS_LOG_ERROR("transmitter.exchange") << "Response CRC validation failed";
            (void)close();
            return Result<Frame>::failure(Error::CrcMismatch);
        }
        if (response[0] != static_cast<std::uint8_t>(expected_command)) {
            WRS_LOG_ERROR("transmitter.exchange")
                << "Unexpected response command"
                << "\n  expected: " << static_cast<unsigned int>(expected_command)
                << "\n  actual: " << static_cast<unsigned int>(response[0]);
            return Result<Frame>::failure(Error::UnexpectedResponse);
        }
        const auto actual_transaction_id = response_transaction_id(response, expected_command);
        if (actual_transaction_id != transaction_id) {
            WRS_LOG_WARNING("transmitter.exchange")
                << "Response transaction ID mismatch; retrying\n  expected_transaction_id: "
                << transaction_id << "\n  actual_transaction_id: " << actual_transaction_id;
            continue;
        }
        if (response[39] != static_cast<std::uint8_t>(ResultCode::Success)) {
            WRS_LOG_ERROR("transmitter.exchange")
                << "Device rejected request\n  transaction_id: " << transaction_id
                << "\n  result_code: " << static_cast<unsigned int>(response[39]);
            return Result<Frame>::failure(Error::DeviceRejected);
        }
        WRS_LOG_DEBUG("transmitter.exchange")
            << "Exchange response succeeded\n  attempt: " << static_cast<unsigned int>(attempt + 1);
        return Result<Frame>::success(std::move(response));
    }
    WRS_LOG_ERROR("transmitter.exchange")
        << "Exchange timed out\n  transaction_id: " << transaction_id;
    return Result<Frame>::failure(Error::TimedOut);
}

wrs::Result<SdoFrame, Error> Client::read_sdo(std::uint16_t object_address) noexcept {
    WRS_LOG_INFO("transmitter.client") << "Read SDO request\n  object_address: " << object_address;
    if (!is_open()) return Result<SdoFrame>::failure(Error::NotOpen);
    if ((object_address & 0xf000U) != 0U) return Result<SdoFrame>::failure(Error::InvalidArgument);
    SdoFrame request{};
    request.cmd = SystemCommand::ReadParamReq;
    request.transaction_id = next_transaction();
    request.object_index = object_address;
    auto response =
        exchange(request.to_frame(), request.transaction_id, SystemCommand::ReadParamRsp);
    if (!response) return Result<SdoFrame>::failure(response.error());
    const auto frame = SdoFrame::from_frame(response.value());
    if ((frame.object_index & 0x0fff) != request.object_index)
        return Result<SdoFrame>::failure(Error::InvalidResponse);
    const auto status = static_cast<std::uint16_t>(frame.object_index >> 12);
    if (status != static_cast<std::uint16_t>(SdoStatus::ReadSuccess)) {
        WRS_LOG_ERROR("transmitter.client") << "Read SDO failed\n  status: " << status;
        return Result<SdoFrame>::failure(sdo_status_error(status));
    }
    WRS_LOG_INFO("transmitter.client")
        << "Read SDO response\n  object_address: " << (frame.object_index & 0x0fffU)
        << "\n  data: <redacted 4 bytes>";
    return Result<SdoFrame>::success(frame);
}

wrs::Result<SdoFrame, Error> Client::read_sdo(SdoObject object) noexcept {
    return read_sdo(static_cast<std::uint16_t>(object));
}

wrs::Result<void, Error> Client::write_sdo(const SdoFrame& request_frame) noexcept {
    WRS_LOG_INFO("transmitter.client")
        << "Write SDO request\n  object_address: " << (request_frame.object_index & 0x0fffU)
        << "\n  data: <redacted 4 bytes>";
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if ((request_frame.object_index & 0x0fff) !=
            static_cast<std::uint16_t>(SdoObject::UpgradeRequest) ||
        request_frame.object_data != kUpgradeRequestValue ||
        request_frame.result_code != ResultCode::Success ||
        !is_zero(request_frame.reserved.data(), request_frame.reserved.size()))
        return Result<void>::failure(Error::InvalidArgument);
    auto request = request_frame;
    request.cmd = SystemCommand::WriteParamReq;
    request.transaction_id = next_transaction();
    request.object_index = static_cast<std::uint16_t>((request.object_index & 0x0fff) | 0x1000);
    auto response =
        exchange(request.to_frame(), request.transaction_id, SystemCommand::WriteParamRsp);
    if (!response) return Result<void>::failure(response.error());
    const auto frame = SdoFrame::from_frame(response.value());
    const auto status = static_cast<std::uint16_t>(frame.object_index >> 12);
    if ((frame.object_index & 0x0fff) != static_cast<std::uint16_t>(SdoObject::UpgradeRequest)) {
        WRS_LOG_ERROR("transmitter.client") << "Write SDO failed: object address mismatch";
        return Result<void>::failure(Error::InvalidResponse);
    }
    if (status != static_cast<std::uint16_t>(SdoStatus::WriteSuccess)) {
        const auto error = sdo_status_error(status);
        WRS_LOG_ERROR("transmitter.client") << "Write SDO failed\n  status: " << status
                                            << "\n  error: " << static_cast<unsigned int>(error);
        return Result<void>::failure(error);
    }
    WRS_LOG_INFO("transmitter.client")
        << "Write SDO succeeded\n  transaction_id: " << frame.transaction_id;
    return Result<void>::success();
}

wrs::Result<LoRaParamFrame, Error> Client::read_lora_parameters() noexcept {
    WRS_LOG_INFO("transmitter.client") << "Read LoRa parameters request";
    if (!is_open()) return Result<LoRaParamFrame>::failure(Error::NotOpen);
    const auto transaction_id = next_transaction();
    const auto request = parameter_read_request(transaction_id, ParamFlags::Modulation::LoRa);
    auto response = exchange(request, transaction_id, SystemCommand::ReadParamRsp);
    if (!response) return Result<LoRaParamFrame>::failure(response.error());
    if (!reserved_bytes_are_zero(response.value(), 29, 10))
        return Result<LoRaParamFrame>::failure(Error::InvalidResponse);
    const auto frame = LoRaParamFrame::from_frame(response.value());
    if (!valid_lora_response(frame)) return Result<LoRaParamFrame>::failure(Error::InvalidResponse);
    WRS_LOG_INFO("transmitter.client")
        << "Read LoRa parameters response\n  transaction_id: " << frame.transaction_id
        << "\n  param_flags: " << frame.param_flags << "\n  tx_power: " << frame.tx_power
        << "\n  freq_offset: " << frame.freq_offset
        << "\n  payload_len: " << static_cast<unsigned int>(frame.payload_len)
        << "\n  rssi_threshold: " << static_cast<unsigned int>(frame.rssi_threshold)
        << "\n  heartbeat_interval: " << frame.heartbeat_interval
        << "\n  heartbeat_loss: " << static_cast<unsigned int>(frame.heartbeat_loss)
        << "\n  bandwidth: " << static_cast<unsigned int>(frame.bandwidth)
        << "\n  spreading_factor: " << static_cast<unsigned int>(frame.spreading_factor)
        << "\n  coding_rate: " << static_cast<unsigned int>(frame.coding_rate)
        << "\n  header_type: " << static_cast<unsigned int>(frame.header_type)
        << "\n  preamble_len: " << static_cast<unsigned int>(frame.preamble_len)
        << "\n  sync_word: " << frame.sync_word;
    return Result<LoRaParamFrame>::success(frame);
}

wrs::Result<GfskParamFrame, Error> Client::read_gfsk_parameters() noexcept {
    WRS_LOG_INFO("transmitter.client") << "Read GFSK parameters request";
    if (!is_open()) return Result<GfskParamFrame>::failure(Error::NotOpen);
    const auto transaction_id = next_transaction();
    const auto request = parameter_read_request(transaction_id, ParamFlags::Modulation::Gfsk);
    auto response = exchange(request, transaction_id, SystemCommand::ReadParamRsp);
    if (!response) return Result<GfskParamFrame>::failure(response.error());
    if (!reserved_bytes_are_zero(response.value(), 35, 4))
        return Result<GfskParamFrame>::failure(Error::InvalidResponse);
    const auto frame = GfskParamFrame::from_frame(response.value());
    if (!valid_gfsk_response(frame)) return Result<GfskParamFrame>::failure(Error::InvalidResponse);
    WRS_LOG_INFO("transmitter.client")
        << "Read GFSK parameters response\n  transaction_id: " << frame.transaction_id
        << "\n  param_flags: " << frame.param_flags << "\n  tx_power: " << frame.tx_power
        << "\n  freq_offset: " << frame.freq_offset
        << "\n  payload_len: " << static_cast<unsigned int>(frame.payload_len)
        << "\n  rssi_threshold: " << static_cast<unsigned int>(frame.rssi_threshold)
        << "\n  heartbeat_interval: " << frame.heartbeat_interval
        << "\n  heartbeat_loss: " << static_cast<unsigned int>(frame.heartbeat_loss)
        << "\n  bandwidth: " << static_cast<unsigned int>(frame.bandwidth)
        << "\n  bitrate: " << frame.bitrate << "\n  freq_deviation: " << frame.freq_deviation
        << "\n  pulse_shaping: " << static_cast<unsigned int>(frame.pulse_shaping)
        << "\n  preamble_len: " << frame.preamble_len << "\n  sync_word: " << frame.sync_word;
    return Result<GfskParamFrame>::success(frame);
}

wrs::Result<void, Error> Client::write_lora_parameters(
    const LoRaParamFrame& parameter_frame) noexcept {
    WRS_LOG_INFO("transmitter.client")
        << "Write LoRa parameters request\n  param_flags: " << parameter_frame.param_flags
        << "\n  tx_power: " << parameter_frame.tx_power
        << "\n  freq_offset: " << parameter_frame.freq_offset
        << "\n  payload_len: " << static_cast<unsigned int>(parameter_frame.payload_len)
        << "\n  rssi_threshold: " << static_cast<unsigned int>(parameter_frame.rssi_threshold)
        << "\n  heartbeat_interval: " << parameter_frame.heartbeat_interval
        << "\n  heartbeat_loss: " << static_cast<unsigned int>(parameter_frame.heartbeat_loss)
        << "\n  bandwidth: " << static_cast<unsigned int>(parameter_frame.bandwidth)
        << "\n  spreading_factor: " << static_cast<unsigned int>(parameter_frame.spreading_factor)
        << "\n  coding_rate: " << static_cast<unsigned int>(parameter_frame.coding_rate)
        << "\n  header_type: " << static_cast<unsigned int>(parameter_frame.header_type)
        << "\n  preamble_len: " << static_cast<unsigned int>(parameter_frame.preamble_len)
        << "\n  sync_word: " << parameter_frame.sync_word;
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if (!valid_lora_parameters(parameter_frame))
        return Result<void>::failure(Error::InvalidArgument);
    auto request = parameter_frame;
    request.cmd = static_cast<std::uint8_t>(SystemCommand::WriteParamReq);
    request.transaction_id = next_transaction();
    auto response =
        exchange(request.to_frame(), request.transaction_id, SystemCommand::WriteParamRsp);
    if (!response) return Result<void>::failure(response.error());
    if (!reserved_bytes_are_zero(response.value(), 29, 10))
        return Result<void>::failure(Error::InvalidResponse);
    const auto frame = LoRaParamFrame::from_frame(response.value());
    if (frame.object_index != 0x6000 || frame.object_data != 0 ||
        !valid_parameter_flags(frame.param_flags, ParamFlags::Modulation::LoRa)) {
        WRS_LOG_ERROR("transmitter.client") << "Write LoRa parameters failed: invalid response";
        return Result<void>::failure(Error::InvalidResponse);
    }
    WRS_LOG_INFO("transmitter.client")
        << "Write LoRa parameters succeeded\n  transaction_id: " << frame.transaction_id;
    return Result<void>::success();
}

wrs::Result<void, Error> Client::write_gfsk_parameters(
    const GfskParamFrame& parameter_frame) noexcept {
    WRS_LOG_INFO("transmitter.client")
        << "Write GFSK parameters request\n  param_flags: " << parameter_frame.param_flags
        << "\n  tx_power: " << parameter_frame.tx_power
        << "\n  freq_offset: " << parameter_frame.freq_offset
        << "\n  payload_len: " << static_cast<unsigned int>(parameter_frame.payload_len)
        << "\n  rssi_threshold: " << static_cast<unsigned int>(parameter_frame.rssi_threshold)
        << "\n  heartbeat_interval: " << parameter_frame.heartbeat_interval
        << "\n  heartbeat_loss: " << static_cast<unsigned int>(parameter_frame.heartbeat_loss)
        << "\n  bandwidth: " << static_cast<unsigned int>(parameter_frame.bandwidth)
        << "\n  bitrate: " << parameter_frame.bitrate
        << "\n  freq_deviation: " << parameter_frame.freq_deviation
        << "\n  pulse_shaping: " << static_cast<unsigned int>(parameter_frame.pulse_shaping)
        << "\n  preamble_len: " << parameter_frame.preamble_len
        << "\n  sync_word: " << parameter_frame.sync_word;
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if (!valid_gfsk_parameters(parameter_frame))
        return Result<void>::failure(Error::InvalidArgument);
    auto request = parameter_frame;
    request.cmd = static_cast<std::uint8_t>(SystemCommand::WriteParamReq);
    request.transaction_id = next_transaction();
    auto response =
        exchange(request.to_frame(), request.transaction_id, SystemCommand::WriteParamRsp);
    if (!response) return Result<void>::failure(response.error());
    if (!reserved_bytes_are_zero(response.value(), 35, 4))
        return Result<void>::failure(Error::InvalidResponse);
    const auto frame = GfskParamFrame::from_frame(response.value());
    if (frame.object_index != 0x6000 || frame.object_data != 0 ||
        !valid_parameter_flags(frame.param_flags, ParamFlags::Modulation::Gfsk)) {
        WRS_LOG_ERROR("transmitter.client") << "Write GFSK parameters failed: invalid response";
        return Result<void>::failure(Error::InvalidResponse);
    }
    WRS_LOG_INFO("transmitter.client")
        << "Write GFSK parameters succeeded\n  transaction_id: " << frame.transaction_id;
    return Result<void>::success();
}

wrs::Result<void, Error> Client::restore_default_parameters(ParamFlags::Modulation type) noexcept {
    WRS_LOG_INFO("transmitter.client")
        << "Restore default parameters request\n  modulation: " << static_cast<unsigned int>(type);
    if (type == ParamFlags::Modulation::LoRa) return write_lora_parameters(default_lora_frame());
    if (type == ParamFlags::Modulation::Gfsk) return write_gfsk_parameters(default_gfsk_frame());
    WRS_LOG_ERROR("transmitter.client")
        << "Restore default parameters rejected: invalid modulation";
    return Result<void>::failure(Error::InvalidArgument);
}

wrs::Result<PinFrame, Error> Client::read_pin() noexcept {
    WRS_LOG_INFO("transmitter.client") << "Read PIN request\n  pin: <redacted>";
    if (!is_open()) return Result<PinFrame>::failure(Error::NotOpen);
    PinFrame request{};
    request.cmd = SystemCommand::PinConfigReq;
    request.pin.fill(static_cast<std::uint8_t>('0'));
    request.transaction_id = next_transaction();
    auto response =
        exchange(request.to_frame(), request.transaction_id, SystemCommand::PinConfigRsp);
    if (!response) return Result<PinFrame>::failure(response.error());
    if (!valid_pin_reserved_bytes(response.value()))
        return Result<PinFrame>::failure(Error::InvalidResponse);
    const auto frame = PinFrame::from_frame(response.value());
    if (!valid_pin(frame)) return Result<PinFrame>::failure(Error::InvalidResponse);
    WRS_LOG_INFO("transmitter.client") << "Read PIN response\n  pin: <redacted>";
    return Result<PinFrame>::success(frame);
}

wrs::Result<void, Error> Client::write_pin(const PinFrame& pin_frame) noexcept {
    WRS_LOG_INFO("transmitter.client") << "Write PIN request\n  pin: <redacted>";
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if (!valid_pin(pin_frame) || all_zero_pin(pin_frame) ||
        pin_frame.result_code != ResultCode::Success ||
        !is_zero(pin_frame.reserved1.data(), pin_frame.reserved1.size()) ||
        !is_zero(pin_frame.reserved2.data(), pin_frame.reserved2.size()))
        return Result<void>::failure(Error::InvalidArgument);
    auto request = pin_frame;
    request.cmd = SystemCommand::PinConfigReq;
    request.transaction_id = next_transaction();
    auto response =
        exchange(request.to_frame(), request.transaction_id, SystemCommand::PinConfigRsp);
    if (!response) return Result<void>::failure(response.error());
    if (!valid_pin_reserved_bytes(response.value()))
        return Result<void>::failure(Error::InvalidResponse);
    const auto frame = PinFrame::from_frame(response.value());
    if (frame.pin != request.pin) {
        WRS_LOG_ERROR("transmitter.client") << "Write PIN failed: response PIN does not match";
        return Result<void>::failure(Error::InvalidResponse);
    }
    WRS_LOG_INFO("transmitter.client")
        << "Write PIN succeeded\n  transaction_id: " << frame.transaction_id
        << "\n  pin: <redacted>";
    return Result<void>::success();
}

}  // namespace transmitter
