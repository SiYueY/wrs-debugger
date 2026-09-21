#include <transmitter/client.hpp>

#include <chrono>
#include <thread>
#include <utility>

namespace transmitter {
namespace {
constexpr std::uint32_t kUpgradeRequestValue = 0x454e;
template <typename T>
using Result = hardware::Result<T, Error>;

Error to_error(serial::Error error) noexcept {
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

bool valid_param_flags(std::uint16_t raw_flags, RadioType expected_type) noexcept {
    constexpr std::uint16_t kReservedBits = 0x3fc0;
    const auto radio_type = static_cast<std::uint8_t>((raw_flags >> 14) & 0x3);
    if ((raw_flags & kReservedBits) != 0 || radio_type > 1 || (raw_flags & 0x0010) != 0)
        return false;
    return radio_type == static_cast<std::uint8_t>(expected_type);
}

bool valid_lora_parameters(const LoRaParamFrame& frame) noexcept {
    if (frame.object_index != 0 || frame.object_data != 0 || frame.result_code != 0 ||
        !is_zero(frame.reserved.data(), frame.reserved.size()) ||
        !valid_param_flags(frame.param_flags, RadioType::LoRa))
        return false;

    const auto sync_low_nibbles = static_cast<std::uint16_t>(frame.sync_word & 0x0f0f);
    const auto band = ParamFlags::from_raw(frame.param_flags).band;
    return frame.tx_power >= 0 && frame.tx_power <= 22 &&
           (band != Band::MHz433 || frame.tx_power <= 10) && frame.payload_len == 12 &&
           frame.rssi_threshold >= 10 && frame.rssi_threshold <= 148 &&
           frame.heartbeat_interval >= 200 && frame.heartbeat_interval <= 10000 &&
           frame.heartbeat_loss >= 1 && frame.bandwidth <= 2 && frame.spreading_factor >= 5 &&
           frame.spreading_factor <= 12 && frame.coding_rate <= 6 && frame.header_type <= 1 &&
           frame.preamble_len >= 10 && frame.preamble_len <= 50 &&
           ((frame.spreading_factor != 5 && frame.spreading_factor != 6) ||
            frame.preamble_len == 12) &&
           sync_low_nibbles == 0x0404;
}

bool valid_gfsk_parameters(const GfskParamFrame& frame) noexcept {
    if (frame.object_index != 0 || frame.object_data != 0 || frame.result_code != 0 ||
        !is_zero(frame.reserved.data(), frame.reserved.size()) ||
        !valid_param_flags(frame.param_flags, RadioType::GFSK))
        return false;

    const auto band = ParamFlags::from_raw(frame.param_flags).band;
    return frame.tx_power >= 0 && frame.tx_power <= 22 &&
           (band != Band::MHz433 || frame.tx_power <= 10) && frame.payload_len == 12 &&
           frame.rssi_threshold >= 10 && frame.rssi_threshold <= 148 &&
           frame.heartbeat_interval >= 200 && frame.heartbeat_interval <= 10000 &&
           frame.heartbeat_loss >= 1 && frame.bandwidth <= 2 && frame.bitrate >= 600 &&
           frame.bitrate <= 150000 && frame.freq_deviation >= 600 &&
           frame.freq_deviation <= 300000 &&
           static_cast<std::uint64_t>(frame.freq_deviation) * 4 >= frame.bitrate &&
           (frame.pulse_shaping == 0 ||
            (frame.pulse_shaping >= 0x08 && frame.pulse_shaping <= 0x0b)) &&
           frame.preamble_len >= 16;
}

bool valid_lora_response(const LoRaParamFrame& frame) noexcept {
    const auto band = ParamFlags::from_raw(frame.param_flags).band;
    return frame.object_index == 0x4000 && frame.object_data == 0 &&
           valid_param_flags(frame.param_flags, RadioType::LoRa) && frame.tx_power >= 0 &&
           frame.tx_power <= 22 && (band != Band::MHz433 || frame.tx_power <= 10) &&
           frame.payload_len == 12 && frame.rssi_threshold >= 10 && frame.rssi_threshold <= 148 &&
           frame.heartbeat_interval >= 200 && frame.heartbeat_interval <= 10000 &&
           frame.heartbeat_loss >= 1 && frame.bandwidth <= 2 && frame.spreading_factor >= 5 &&
           frame.spreading_factor <= 12 && frame.coding_rate <= 6 && frame.header_type <= 1 &&
           frame.preamble_len >= 10 && frame.preamble_len <= 50 &&
           ((frame.spreading_factor != 5 && frame.spreading_factor != 6) ||
            frame.preamble_len == 12) &&
           static_cast<std::uint16_t>(frame.sync_word & 0x0f0f) == 0x0404 &&
           is_zero(frame.reserved.data(), frame.reserved.size());
}

bool valid_gfsk_response(const GfskParamFrame& frame) noexcept {
    const auto band = ParamFlags::from_raw(frame.param_flags).band;
    return frame.object_index == 0x4000 && frame.object_data == 0 &&
           valid_param_flags(frame.param_flags, RadioType::GFSK) && frame.tx_power >= 0 &&
           frame.tx_power <= 22 && (band != Band::MHz433 || frame.tx_power <= 10) &&
           frame.payload_len == 12 && frame.rssi_threshold >= 10 && frame.rssi_threshold <= 148 &&
           frame.heartbeat_interval >= 200 && frame.heartbeat_interval <= 10000 &&
           frame.heartbeat_loss >= 1 && frame.bandwidth <= 2 && frame.bitrate >= 600 &&
           frame.bitrate <= 150000 && frame.freq_deviation >= 600 &&
           frame.freq_deviation <= 300000 &&
           static_cast<std::uint64_t>(frame.freq_deviation) * 4 >= frame.bitrate &&
           (frame.pulse_shaping == 0 ||
            (frame.pulse_shaping >= 0x08 && frame.pulse_shaping <= 0x0b)) &&
           frame.preamble_len >= 16 && is_zero(frame.reserved.data(), frame.reserved.size());
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

std::uint32_t response_transaction_id(const Bytes& bytes, SystemCmd expected) noexcept {
    return expected == SystemCmd::PinCfgRsp ? read_le32(bytes.bytes() + 12)
                                            : read_le32(bytes.bytes() + 1);
}

LoRaParamFrame default_lora_frame() noexcept {
    LoRaParamFrame frame{};
    ParamFlags flags{};
    flags.radio_type = RadioType::LoRa;
    frame.param_flags = flags.to_raw();
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
    flags.radio_type = RadioType::GFSK;
    frame.param_flags = flags.to_raw();
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

hardware::Result<void, Error> Client::open(
    const std::string& path, std::chrono::milliseconds response_timeout,
    std::chrono::milliseconds retry_interval, std::uint8_t max_attempts) noexcept {
    if (is_open()) {
        return Result<void>::failure(Error::AlreadyOpen);
    }
    if (path.empty() || !valid_options(response_timeout, retry_interval, max_attempts)) {
        return Result<void>::failure(Error::InvalidArgument);
    }
    auto opened = port_.open(path, transmitter_config());
    if (!opened) {
        return Result<void>::failure(to_error(opened.error()));
    }
    response_timeout_ = response_timeout;
    retry_interval_ = retry_interval;
    max_attempts_ = max_attempts;
    identity_ = {};
    auto discarded = port_.discard_input();
    if (!discarded) {
        (void)close();
        return Result<void>::failure(to_error(discarded.error()));
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
        (void)close();
        return Result<void>::failure(error);
    }
    identity_ = {
        product.value().object_data, version.value().object_data, serial.value().object_data};
    return Result<void>::success();
}

hardware::Result<void, Error> Client::close() noexcept {
    identity_ = {};
    auto close_result = port_.close();
    return close_result ? Result<void>::success()
                        : Result<void>::failure(to_error(close_result.error()));
}

bool Client::is_open() const noexcept { return port_.is_open(); }

const DeviceIdentity& Client::identity() const noexcept { return identity_; }

std::uint32_t Client::next_transaction() noexcept {
    const auto value = next_transaction_id_++;
    if (next_transaction_id_ == 0) {
        next_transaction_id_ = 1;
    }
    return value == 0 ? next_transaction() : value;
}

hardware::Result<Bytes, Error> Client::exchange(
    const Bytes& request, std::uint32_t transaction_id, SystemCmd expected_command) noexcept {
    if (!is_open()) {
        return Result<Bytes>::failure(Error::NotOpen);
    }
    for (std::uint8_t attempt = 0; attempt < max_attempts_; ++attempt) {
        if (attempt != 0 && retry_interval_ > std::chrono::milliseconds::zero()) {
            std::this_thread::sleep_for(retry_interval_);
        }
        std::size_t sent = 0;
        const auto write_deadline = std::chrono::steady_clock::now() + response_timeout_;
        while (sent < request.size()) {
            const auto remaining = write_deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::steady_clock::duration::zero()) {
                break;
            }
            auto write = port_.write(
                reinterpret_cast<const std::byte*>(request.bytes()) + sent, request.size() - sent,
                remaining);
            if (!write) {
                if (sent != 0) {
                    (void)close();
                    return Result<Bytes>::failure(Error::FrameSyncLost);
                }
                if (write.error() != serial::Error::TimedOut &&
                    write.error() != serial::Error::WouldBlock) {
                    const auto error = to_error(write.error());
                    if (error == Error::Disconnected) (void)close();
                    return Result<Bytes>::failure(error);
                }
                break;
            }
            sent += write.value();
        }
        if (sent != request.size()) continue;

        Bytes response{};
        std::size_t received = 0;
        const auto read_deadline = std::chrono::steady_clock::now() + response_timeout_;
        while (received < response.size()) {
            const auto remaining = read_deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::steady_clock::duration::zero()) break;
            auto read = port_.read(
                reinterpret_cast<std::byte*>(response.bytes()) + received,
                response.size() - received, remaining);
            if (!read) {
                if (read.error() == serial::Error::TimedOut ||
                    read.error() == serial::Error::WouldBlock)
                    break;
                const auto error = to_error(read.error());
                if (error == Error::Disconnected) (void)close();
                return Result<Bytes>::failure(error);
            }
            received += read.value();
        }
        if (received != response.size()) {
            if (received != 0) {
                (void)close();
                return Result<Bytes>::failure(Error::FrameSyncLost);
            }
            continue;
        }
        if (!has_valid_crc(response)) {
            (void)close();
            return Result<Bytes>::failure(Error::CrcMismatch);
        }
        if (response[0] != static_cast<std::uint8_t>(expected_command))
            return Result<Bytes>::failure(Error::UnexpectedResponse);
        if (response_transaction_id(response, expected_command) != transaction_id) continue;
        if (response[39] != static_cast<std::uint8_t>(ResultCode::Success))
            return Result<Bytes>::failure(Error::DeviceRejected);
        return Result<Bytes>::success(std::move(response));
    }
    return Result<Bytes>::failure(Error::TimedOut);
}

hardware::Result<SdoFrame, Error> Client::read_sdo(SdoObject object) noexcept {
    if (!is_open()) return Result<SdoFrame>::failure(Error::NotOpen);
    SdoFrame request{};
    request.cmd = static_cast<std::uint8_t>(SystemCmd::ParamReadReq);
    request.transaction_id = next_transaction();
    request.object_index = static_cast<std::uint16_t>(object);
    auto response = exchange(request.to_bytes(), request.transaction_id, SystemCmd::ParamReadRsp);
    if (!response) return Result<SdoFrame>::failure(response.error());
    const auto frame = SdoFrame::from_bytes(response.value());
    if ((frame.object_index & 0x0fff) != request.object_index)
        return Result<SdoFrame>::failure(Error::InvalidResponse);
    const auto status = static_cast<std::uint16_t>(frame.object_index >> 12);
    if (status != static_cast<std::uint16_t>(SdoStatus::ReadSuccess))
        return Result<SdoFrame>::failure(sdo_status_error(status));
    return Result<SdoFrame>::success(frame);
}

hardware::Result<void, Error> Client::write_sdo(const SdoFrame& request_frame) noexcept {
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if ((request_frame.object_index & 0x0fff) !=
            static_cast<std::uint16_t>(SdoObject::UpgradeRequest) ||
        request_frame.object_data != kUpgradeRequestValue || request_frame.result_code != 0 ||
        !is_zero(request_frame.reserved.data(), request_frame.reserved.size()))
        return Result<void>::failure(Error::InvalidArgument);
    auto request = request_frame;
    request.cmd = static_cast<std::uint8_t>(SystemCmd::ParamWriteReq);
    request.transaction_id = next_transaction();
    request.object_index = static_cast<std::uint16_t>((request.object_index & 0x0fff) | 0x1000);
    auto response = exchange(request.to_bytes(), request.transaction_id, SystemCmd::ParamWriteRsp);
    if (!response) return Result<void>::failure(response.error());
    const auto frame = SdoFrame::from_bytes(response.value());
    const auto status = static_cast<std::uint16_t>(frame.object_index >> 12);
    if ((frame.object_index & 0x0fff) != static_cast<std::uint16_t>(SdoObject::UpgradeRequest))
        return Result<void>::failure(Error::InvalidResponse);
    return status == static_cast<std::uint16_t>(SdoStatus::WriteSuccess)
               ? Result<void>::success()
               : Result<void>::failure(sdo_status_error(status));
}

hardware::Result<LoRaParamFrame, Error> Client::read_lora_parameters() noexcept {
    if (!is_open()) return Result<LoRaParamFrame>::failure(Error::NotOpen);
    LoRaParamFrame request{};
    request.cmd = static_cast<std::uint8_t>(SystemCmd::ParamReadReq);
    request.transaction_id = next_transaction();
    request.param_flags = ParamFlags{}.to_raw();
    auto response = exchange(request.to_bytes(), request.transaction_id, SystemCmd::ParamReadRsp);
    if (!response) return Result<LoRaParamFrame>::failure(response.error());
    const auto frame = LoRaParamFrame::from_bytes(response.value());
    if (!valid_lora_response(frame)) return Result<LoRaParamFrame>::failure(Error::InvalidResponse);
    return Result<LoRaParamFrame>::success(frame);
}

hardware::Result<GfskParamFrame, Error> Client::read_gfsk_parameters() noexcept {
    if (!is_open()) return Result<GfskParamFrame>::failure(Error::NotOpen);
    GfskParamFrame request{};
    request.cmd = static_cast<std::uint8_t>(SystemCmd::ParamReadReq);
    request.transaction_id = next_transaction();
    ParamFlags flags{};
    flags.radio_type = RadioType::GFSK;
    request.param_flags = flags.to_raw();
    auto response = exchange(request.to_bytes(), request.transaction_id, SystemCmd::ParamReadRsp);
    if (!response) return Result<GfskParamFrame>::failure(response.error());
    const auto frame = GfskParamFrame::from_bytes(response.value());
    if (!valid_gfsk_response(frame)) return Result<GfskParamFrame>::failure(Error::InvalidResponse);
    return Result<GfskParamFrame>::success(frame);
}

hardware::Result<void, Error> Client::write_lora_parameters(
    const LoRaParamFrame& parameter_frame) noexcept {
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if (!valid_lora_parameters(parameter_frame))
        return Result<void>::failure(Error::InvalidArgument);
    auto request = parameter_frame;
    request.cmd = static_cast<std::uint8_t>(SystemCmd::ParamWriteReq);
    request.transaction_id = next_transaction();
    auto response = exchange(request.to_bytes(), request.transaction_id, SystemCmd::ParamWriteRsp);
    if (!response) return Result<void>::failure(response.error());
    const auto frame = LoRaParamFrame::from_bytes(response.value());
    return frame.object_index == 0x6000 && frame.object_data == 0 &&
                   valid_param_flags(frame.param_flags, RadioType::LoRa) &&
                   is_zero(frame.reserved.data(), frame.reserved.size())
               ? Result<void>::success()
               : Result<void>::failure(Error::InvalidResponse);
}

hardware::Result<void, Error> Client::write_gfsk_parameters(
    const GfskParamFrame& parameter_frame) noexcept {
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if (!valid_gfsk_parameters(parameter_frame))
        return Result<void>::failure(Error::InvalidArgument);
    auto request = parameter_frame;
    request.cmd = static_cast<std::uint8_t>(SystemCmd::ParamWriteReq);
    request.transaction_id = next_transaction();
    auto response = exchange(request.to_bytes(), request.transaction_id, SystemCmd::ParamWriteRsp);
    if (!response) return Result<void>::failure(response.error());
    const auto frame = GfskParamFrame::from_bytes(response.value());
    return frame.object_index == 0x6000 && frame.object_data == 0 &&
                   valid_param_flags(frame.param_flags, RadioType::GFSK) &&
                   is_zero(frame.reserved.data(), frame.reserved.size())
               ? Result<void>::success()
               : Result<void>::failure(Error::InvalidResponse);
}

hardware::Result<void, Error> Client::restore_default_parameters(RadioType type) noexcept {
    return type == RadioType::LoRa ? write_lora_parameters(default_lora_frame())
                                   : write_gfsk_parameters(default_gfsk_frame());
}

hardware::Result<PinFrame, Error> Client::read_pin() noexcept {
    if (!is_open()) return Result<PinFrame>::failure(Error::NotOpen);
    PinFrame request{};
    request.cmd = static_cast<std::uint8_t>(SystemCmd::PinCfgReq);
    request.pin.fill(static_cast<std::uint8_t>('0'));
    request.transaction_id = next_transaction();
    auto response = exchange(request.to_bytes(), request.transaction_id, SystemCmd::PinCfgRsp);
    if (!response) return Result<PinFrame>::failure(response.error());
    const auto frame = PinFrame::from_bytes(response.value());
    if (!is_zero(frame.reserved1.data(), frame.reserved1.size()) ||
        !is_zero(frame.reserved2.data(), frame.reserved2.size()) || !valid_pin(frame))
        return Result<PinFrame>::failure(Error::InvalidResponse);
    return Result<PinFrame>::success(frame);
}

hardware::Result<void, Error> Client::write_pin(const PinFrame& pin_frame) noexcept {
    if (!is_open()) return Result<void>::failure(Error::NotOpen);
    if (!valid_pin(pin_frame) || all_zero_pin(pin_frame) || pin_frame.result_code != 0 ||
        !is_zero(pin_frame.reserved1.data(), pin_frame.reserved1.size()) ||
        !is_zero(pin_frame.reserved2.data(), pin_frame.reserved2.size()))
        return Result<void>::failure(Error::InvalidArgument);
    auto request = pin_frame;
    request.cmd = static_cast<std::uint8_t>(SystemCmd::PinCfgReq);
    request.transaction_id = next_transaction();
    auto response = exchange(request.to_bytes(), request.transaction_id, SystemCmd::PinCfgRsp);
    if (!response) return Result<void>::failure(response.error());
    const auto frame = PinFrame::from_bytes(response.value());
    return frame.pin == request.pin ? Result<void>::success()
                                    : Result<void>::failure(Error::InvalidResponse);
}

}  // namespace transmitter
