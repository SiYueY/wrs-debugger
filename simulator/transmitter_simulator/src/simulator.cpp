#include <transmitter_simulator/simulator.hpp>

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <deque>
#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace transmitter_simulator {
namespace {
struct ControlCommand final {
    std::function<Error()> action{};
    std::promise<Error> completion{};
};

struct PendingResponse final {
    Bytes request{};
    Bytes frame{};
    std::chrono::steady_clock::time_point due{};
    DeliveryFault delivery{DeliveryFault::None};
    std::size_t split_after_bytes{};
    std::chrono::milliseconds split_delay{};
    std::size_t truncate_after_bytes{};
    std::size_t offset{};
};

constexpr std::size_t k_command = 0;
constexpr std::size_t k_transaction = 1;
constexpr std::size_t k_object = 5;
constexpr std::size_t k_object_data = 7;
constexpr std::size_t k_flags = 11;
constexpr std::size_t k_result = 39;

std::uint32_t transaction_id(const Bytes& frame) noexcept {
    return frame.read_le32(
        frame[k_command] == static_cast<std::uint8_t>(SystemCmd::PinCfgReq) ? 12 : k_transaction);
}

bool radio_read_is_valid(const Bytes& frame, RadioType& type) noexcept {
    ParamFlags flags{};
    return frame.read_le16(k_object) == 0 && frame.read_le32(k_object_data) == 0 &&
           ParamFlags::from_raw(frame.read_le16(k_flags), flags) &&
           (frame.read_le16(k_flags) & 0x3fffU) == 0 && frame.is_zero(13, 26) &&
           frame[k_result] == 0 && ((type = flags.radio_type), true);
}

bool sdo_structure_is_valid(const Bytes& frame, bool write) noexcept {
    const auto object = frame.read_le16(k_object);
    const auto operation = static_cast<std::uint16_t>(object >> 12U);
    return (write ? operation == 1 : operation == 0) && (object & 0x0fffU) != 0 &&
           (!write ? frame.read_le32(k_object_data) == 0 : true) && frame.is_zero(11, 28) &&
           frame[k_result] == 0;
}

SdoStatus sdo_error_status(SdoAccessError error) noexcept {
    return error == SdoAccessError::Unknown ? SdoStatus::InvalidCommand : SdoStatus::Error;
}
}  // namespace

struct Simulator::Impl final {
    mutable std::mutex mutex{};
    TransportOptions transport_options{};
    PtyTransport transport{};
    Device device{};
    History history{};
    FaultConfig fault{};
    LifecycleState lifecycle{LifecycleState::Stopped};
    PeerState peer{PeerState::Detached};
    std::thread worker{};
    int wake_fd{-1};
    std::atomic<bool> stop_requested{false};
    std::deque<std::shared_ptr<ControlCommand>> controls{};
    std::optional<PendingResponse> pending_response{};
    std::uint64_t connection_generation{};
    std::uint64_t request_sequence{};
    std::vector<std::uint8_t> rx_buffer{};
    std::size_t rx_frames{};
    std::size_t tx_frames{};
};

#define mutex_ impl_->mutex
#define transport_options_ impl_->transport_options
#define transport_ impl_->transport
#define device_ impl_->device
#define history_ impl_->history
#define fault_ impl_->fault
#define lifecycle_ impl_->lifecycle
#define peer_ impl_->peer
#define worker_ impl_->worker
#define wake_fd_ impl_->wake_fd
#define stop_requested_ impl_->stop_requested
#define controls_ impl_->controls
#define pending_response_ impl_->pending_response
#define connection_generation_ impl_->connection_generation
#define request_sequence_ impl_->request_sequence
#define rx_buffer_ impl_->rx_buffer
#define rx_frames_ impl_->rx_frames
#define tx_frames_ impl_->tx_frames

Simulator::Simulator() : impl_(std::make_unique<Impl>()) {}
Simulator::~Simulator() noexcept { stop(); }

Result<void, Error> Simulator::start(const SimulatorOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lifecycle_ != LifecycleState::Stopped)
        return Result<void, Error>::failure(Error::AlreadyRunning);
    auto opened = transport_.open(options.transport);
    if (!opened) return opened;
    transport_options_ = options.transport;
    wake_fd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd_ < 0) {
        transport_.close();
        return Result<void, Error>::failure(Error::Io);
    }
    stop_requested_.store(false);
    lifecycle_ = LifecycleState::Running;
    ++connection_generation_;
    peer_ = PeerState::Detached;
    rx_buffer_.clear();
    worker_ = std::thread(&Simulator::worker, this);
    return Result<void, Error>::success();
}

void Simulator::stop() noexcept {
    stop_requested_.store(true);
    notify_worker();
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& control : controls_) control->completion.set_value(Error::InvalidState);
    controls_.clear();
    transport_.close();
    if (wake_fd_ >= 0) (void)::close(wake_fd_);
    wake_fd_ = -1;
    lifecycle_ = LifecycleState::Stopped;
    peer_ = PeerState::Detached;
    rx_buffer_.clear();
    pending_response_.reset();
}

SimulatorSnapshot Simulator::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SimulatorSnapshot result{};
    result.lifecycle = lifecycle_;
    result.peer = peer_;
    result.slave_path = transport_.slave_path();
    result.stable_path = transport_.stable_path();
    result.device = device_.state();
    result.fault = fault_;
    result.history.assign(history_.records().begin(), history_.records().end());
    result.rx_frames = rx_frames_;
    result.tx_frames = tx_frames_;
    return result;
}

Result<void, Error> Simulator::ensure_running() const noexcept {
    return lifecycle_ == LifecycleState::Running
               ? Result<void, Error>::success()
               : Result<void, Error>::failure(Error::InvalidState);
}

void Simulator::notify_worker() noexcept {
    if (wake_fd_ < 0) return;
    const std::uint64_t one = 1;
    (void)::write(wake_fd_, &one, sizeof(one));
}

Result<void, Error> Simulator::submit(std::function<Error()> action) {
    auto control = std::make_shared<ControlCommand>();
    control->action = std::move(action);
    auto completed = control->completion.get_future();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lifecycle_ == LifecycleState::Stopped)
            return Result<void, Error>::failure(Error::InvalidState);
        controls_.push_back(control);
    }
    notify_worker();
    const auto error = completed.get();
    return error == Error::None ? Result<void, Error>::success()
                                : Result<void, Error>::failure(error);
}

void Simulator::process_control_commands() noexcept {
    while (!controls_.empty()) {
        const auto control = controls_.front();
        controls_.pop_front();
        control->completion.set_value(control->action());
    }
}

Result<void, Error> Simulator::set_identity(const DeviceIdentity& identity) {
    return submit([this, identity] {
        const auto result = device_.set_identity(identity);
        return result ? Error::None : result.error();
    });
}
Result<void, Error> Simulator::set_pin(const Pin& pin) {
    return submit([this, pin] {
        const auto result = device_.set_pin(pin);
        return result ? Error::None : result.error();
    });
}
Result<void, Error> Simulator::set_battery(std::uint8_t percentage) {
    return submit([this, percentage] {
        const auto result = device_.set_battery(percentage);
        return result ? Error::None : result.error();
    });
}
Result<void, Error> Simulator::set_lora(const LoraConfig& config) {
    return submit([this, config] {
        const auto result = device_.set_lora(config);
        return result ? Error::None : result.error();
    });
}
Result<void, Error> Simulator::set_gfsk(const GfskConfig& config) {
    return submit([this, config] {
        const auto result = device_.set_gfsk(config);
        return result ? Error::None : result.error();
    });
}
Result<void, Error> Simulator::set_firmware_metadata(const FirmwareMetadata& metadata) {
    return submit([this, metadata] {
        const auto result = device_.set_firmware_metadata(metadata);
        return result ? Error::None : result.error();
    });
}
Result<void, Error> Simulator::set_sdo_editor_state(const SdoEditorState& state) {
    return submit([this, state] {
        const auto result = device_.set_sdo_editor_state(state);
        return result ? Error::None : result.error();
    });
}
Result<void, Error> Simulator::reset_device() {
    return submit([this] {
        device_.reset();
        return Error::None;
    });
}
Result<void, Error> Simulator::set_fault_config(const FaultConfig& config) {
    if (!valid_fault_config(config)) return Result<void, Error>::failure(Error::InvalidArgument);
    return submit([this, config] {
        fault_ = config;
        return Error::None;
    });
}
Result<void, Error> Simulator::set_transport_path(std::string stable_path) {
    if (stable_path.empty() || !std::filesystem::path(stable_path).is_absolute())
        return Result<void, Error>::failure(Error::InvalidArgument);
    bool reconnect_same_path = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lifecycle_ == LifecycleState::Stopped)
            return Result<void, Error>::failure(Error::InvalidState);
        if (stable_path == transport_options_.stable_path) {
            if (lifecycle_ == LifecycleState::Running) return Result<void, Error>::success();
            reconnect_same_path = true;
        }
    }
    // The existing lock belongs to this instance; reconnect it instead of self-conflicting.
    if (reconnect_same_path) return reconnect();
    return submit([this, stable_path = std::move(stable_path)] {
        PtyTransport replacement;
        const TransportOptions replacement_options{stable_path};
        const auto opened = replacement.open(replacement_options);
        if (!opened) return opened.error();

        // replacement has published its own stable link; close() only removes an old link it owns.
        transport_.close();
        transport_ = std::move(replacement);
        transport_options_ = replacement_options;
        lifecycle_ = LifecycleState::Running;
        peer_ = PeerState::Detached;
        ++connection_generation_;
        rx_buffer_.clear();
        pending_response_.reset();
        return Error::None;
    });
}
Result<void, Error> Simulator::disconnect() {
    return submit([this] {
        transport_.disconnect();
        lifecycle_ = LifecycleState::Disconnected;
        peer_ = PeerState::Detached;
        rx_buffer_.clear();
        pending_response_.reset();
        return Error::None;
    });
}
Result<void, Error> Simulator::reconnect() {
    return submit([this] {
        if (lifecycle_ != LifecycleState::Disconnected) return Error::InvalidState;
        const auto result = transport_.open(transport_options_);
        if (!result) return result.error();
        lifecycle_ = LifecycleState::Running;
        ++connection_generation_;
        peer_ = PeerState::Detached;
        rx_buffer_.clear();
        pending_response_.reset();
        return Error::None;
    });
}
Result<void, Error> Simulator::clear_history() {
    return submit([this] {
        history_.clear();
        return Error::None;
    });
}

Bytes Simulator::handle_frame(const Bytes& request, bool& should_respond) {
    should_respond = false;
    history_.record(FrameDirection::Received, request);
    if (!request.has_valid_crc()) return {};
    const auto command = static_cast<SystemCmd>(request[k_command]);
    if (command != SystemCmd::ParamReadReq && command != SystemCmd::ParamWriteReq &&
        command != SystemCmd::PinCfgReq)
        return {};
    const auto transaction = transaction_id(request);
    if (transaction == 0) return {};
    const auto object = request.read_le16(k_object);

    if (command == SystemCmd::PinCfgReq) {
        const auto pin_request = PinFrame::from_bytes(request);
        const bool valid = pin_request.is_ascii_pin() && request.is_zero(7, 5) &&
                           request.is_zero(16, 23) && request[k_result] == 0;
        const bool forced_failure = valid && fault_.fail_next_business_response;
        if (forced_failure) fault_.fail_next_business_response = false;
        PinFrame response{};
        response.command = SystemCmd::PinCfgRsp;
        response.transaction_id = transaction;
        response.pin = device_.state().pin.digits;
        if (!valid || forced_failure) {
            response.result = ResultCode::Failure;
        } else if (pin_request.is_read_request()) {
            response.result = ResultCode::Success;
        } else {
            const Pin proposed{pin_request.pin};
            const auto result = device_.write_pin(proposed);
            response.result = result ? ResultCode::Success : ResultCode::Failure;
        }
        should_respond = true;
        return response.to_bytes();
    }

    if (command == SystemCmd::ParamReadReq && object == 0) {
        RadioType type{};
        const bool valid = radio_read_is_valid(request, type);
        const bool forced_failure = valid && fault_.fail_next_business_response;
        if (forced_failure) fault_.fail_next_business_response = false;
        should_respond = true;
        if (type == RadioType::LoRa) {
            LoRaParamFrame response{};
            response.command = SystemCmd::ParamReadRsp;
            response.transaction_id = transaction;
            response.object_index = 0x4000;
            response.config = device_.state().lora;
            response.result = valid && !forced_failure ? ResultCode::Success : ResultCode::Failure;
            return response.to_bytes();
        }
        GfskParamFrame response{};
        response.command = SystemCmd::ParamReadRsp;
        response.transaction_id = transaction;
        response.object_index = 0x4000;
        response.config = device_.state().gfsk;
        response.result = valid && !forced_failure ? ResultCode::Success : ResultCode::Failure;
        return response.to_bytes();
    }
    if (command == SystemCmd::ParamWriteReq && object == 0) {
        ParamFlags flags{};
        const bool flags_valid = ParamFlags::from_raw(request.read_le16(k_flags), flags);
        const bool common = request.read_le32(k_object_data) == 0 && request[k_result] == 0;
        const bool forced_failure = flags_valid && fault_.fail_next_business_response;
        if (forced_failure) fault_.fail_next_business_response = false;
        if (flags_valid && flags.radio_type == RadioType::LoRa) {
            const auto parsed = LoRaParamFrame::from_bytes(request);
            const bool valid = common && request.is_zero(29, 10) && parsed.config.is_valid() &&
                               !forced_failure &&
                               static_cast<bool>(device_.set_lora(parsed.config));
            LoRaParamFrame response{};
            response.command = SystemCmd::ParamWriteRsp;
            response.transaction_id = transaction;
            response.object_index = 0x6000;
            response.config = device_.state().lora;
            response.result = valid ? ResultCode::Success : ResultCode::Failure;
            should_respond = true;
            return response.to_bytes();
        } else if (flags_valid && flags.radio_type == RadioType::Gfsk) {
            const auto parsed = GfskParamFrame::from_bytes(request);
            const bool valid = common && request.is_zero(35, 4) && parsed.config.is_valid() &&
                               !forced_failure &&
                               static_cast<bool>(device_.set_gfsk(parsed.config));
            GfskParamFrame response{};
            response.command = SystemCmd::ParamWriteRsp;
            response.transaction_id = transaction;
            response.object_index = 0x6000;
            response.config = device_.state().gfsk;
            response.result = valid ? ResultCode::Success : ResultCode::Failure;
            should_respond = true;
            return response.to_bytes();
        }
        LoRaParamFrame response{};
        response.command = SystemCmd::ParamWriteRsp;
        response.transaction_id = transaction;
        response.object_index = 0x6000;
        response.config = device_.state().lora;
        response.result = ResultCode::Failure;
        should_respond = true;
        return response.to_bytes();
    }

    const auto address = static_cast<std::uint16_t>(object & 0x0fffU);
    const bool write = command == SystemCmd::ParamWriteReq;
    SdoFrame response{};
    response.command = write ? SystemCmd::ParamWriteRsp : SystemCmd::ParamReadRsp;
    response.transaction_id = transaction;
    if (!sdo_structure_is_valid(request, write)) {
        response.object_index = static_cast<std::uint16_t>(0xe000U | address);
    } else if (!write) {
        const auto result = device_.read_sdo(address);
        response.object_index =
            result ? static_cast<std::uint16_t>(0x4000U | address)
                   : static_cast<std::uint16_t>(
                         static_cast<std::uint16_t>(sdo_error_status(result.error())) << 12U |
                         address);
        response.object_data = result ? result.value() : 0;
    } else {
        const auto result = device_.write_sdo(address, request.read_le32(k_object_data));
        response.object_index =
            result ? static_cast<std::uint16_t>(0x6000U | address)
                   : static_cast<std::uint16_t>(
                         static_cast<std::uint16_t>(sdo_error_status(result.error())) << 12U |
                         address);
    }
    should_respond = true;
    return response.to_bytes();
}

void Simulator::apply_response_faults(Bytes& response, const Bytes& request) noexcept {
    const auto request_command = static_cast<SystemCmd>(request[k_command]);
    const auto object = request.read_le16(k_object);
    const bool sdo = (request_command == SystemCmd::ParamReadReq ||
                      request_command == SystemCmd::ParamWriteReq) &&
                     object != 0;
    if (sdo && fault_.next_sdo_fault != SdoFault::None) {
        const auto address = static_cast<std::uint16_t>(response.read_le16(k_object) & 0x0fffU);
        const auto status = static_cast<std::uint16_t>(fault_.next_sdo_fault);
        response.write_le16(k_object, static_cast<std::uint16_t>((status << 12U) | address));
        response.write_le32(k_object_data, 0);
        response[k_result] = 0;
        fault_.next_sdo_fault = SdoFault::None;
    }
    if (fault_.wrong_next_command) {
        response[k_command] =
            response[k_command] == static_cast<std::uint8_t>(SystemCmd::ParamReadRsp)
                ? static_cast<std::uint8_t>(SystemCmd::ParamWriteRsp)
                : static_cast<std::uint8_t>(SystemCmd::ParamReadRsp);
        fault_.wrong_next_command = false;
    }
    if (fault_.wrong_next_transaction) {
        const auto offset = request_command == SystemCmd::PinCfgReq ? 12U : 1U;
        const auto transaction = response.read_le32(offset);
        response.write_le32(offset, transaction == 1 ? 2 : transaction - 1);
        fault_.wrong_next_transaction = false;
    }
    response.fill_crc();
    if (fault_.corrupt_next_crc) {
        response[40] ^= 0xffU;
        fault_.corrupt_next_crc = false;
    }
}

void Simulator::schedule_response(Bytes response, const Bytes& request) noexcept {
    apply_response_faults(response, request);
    PendingResponse pending{};
    pending.request = request;
    pending.frame = response;
    pending.due = std::chrono::steady_clock::now() + fault_.response_delay;
    pending.delivery = fault_.next_delivery_fault;
    pending.split_after_bytes = fault_.split_after_bytes;
    pending.split_delay = fault_.split_delay;
    pending.truncate_after_bytes = fault_.truncate_after_bytes;
    fault_.next_delivery_fault = DeliveryFault::None;
    pending_response_ = pending;
}

bool Simulator::write_bytes(const std::uint8_t* bytes, std::size_t size) noexcept {
    std::size_t offset = 0;
    while (offset < size && !stop_requested_.load()) {
        const auto written = transport_.write(bytes + offset, size - offset);
        if (!written) return false;
        if (written.value() == 0) return false;
        offset += written.value();
    }
    return offset == size;
}

void Simulator::deliver_pending_response() noexcept {
    if (!pending_response_ || pending_response_->due > std::chrono::steady_clock::now()) return;
    auto& pending = *pending_response_;
    if (pending.delivery == DeliveryFault::Drop) {
        history_.record(FrameDirection::Event, pending.frame, "DropResponse");
        pending_response_.reset();
        return;
    }
    if (pending.delivery == DeliveryFault::Disconnect) {
        transport_.disconnect();
        lifecycle_ = LifecycleState::Disconnected;
        peer_ = PeerState::Detached;
        history_.record(FrameDirection::Event, pending.frame, "DisconnectAfterRequest");
        pending_response_.reset();
        return;
    }
    if (pending.delivery == DeliveryFault::Truncate) {
        (void)write_bytes(pending.frame.bytes(), pending.truncate_after_bytes);
        history_.record(FrameDirection::Transmitted, pending.frame, "TruncatedResponse");
        pending_response_.reset();
        return;
    }
    if (pending.delivery == DeliveryFault::Split && pending.offset == 0) {
        if (!write_bytes(pending.frame.bytes(), pending.split_after_bytes)) {
            pending_response_.reset();
            return;
        }
        pending.offset = pending.split_after_bytes;
        pending.due = std::chrono::steady_clock::now() + pending.split_delay;
        pending.delivery = DeliveryFault::None;
        return;
    }
    const auto size = pending.frame.size() - pending.offset;
    if (write_bytes(pending.frame.bytes() + pending.offset, size)) {
        ++tx_frames_;
        history_.record(FrameDirection::Transmitted, pending.frame);
    }
    pending_response_.reset();
}

void Simulator::write_frame(const Bytes& frame) noexcept {
    std::size_t offset = 0;
    while (offset < frame.size() && !stop_requested_.load()) {
        auto written = transport_.write(frame.bytes() + offset, frame.size() - offset);
        if (!written) return;
        if (written.value() == 0) {
            pollfd descriptor{transport_.native_handle(), POLLOUT, 0};
            (void)::poll(&descriptor, 1, 20);
            continue;
        }
        offset += written.value();
    }
    if (offset == frame.size()) {
        ++tx_frames_;
        history_.record(FrameDirection::Transmitted, frame);
    }
}

void Simulator::worker() noexcept {
    std::array<std::uint8_t, 256> received{};
    while (!stop_requested_.load()) {
        int fd = -1;
        int wake_fd = -1;
        int timeout_ms = 100;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            deliver_pending_response();
            if (lifecycle_ == LifecycleState::Running) fd = transport_.native_handle();
            wake_fd = wake_fd_;
            if (pending_response_) {
                const auto remaining = pending_response_->due - std::chrono::steady_clock::now();
                timeout_ms =
                    remaining <= std::chrono::steady_clock::duration::zero()
                        ? 0
                        : static_cast<int>(std::min<std::int64_t>(
                              std::chrono::duration_cast<std::chrono::milliseconds>(remaining)
                                  .count(),
                              100));
            }
        }
        std::array<pollfd, 2> descriptors{{{wake_fd, POLLIN, 0}, {fd, POLLIN, 0}}};
        const auto count = fd >= 0 ? 2UL : 1UL;
        if (::poll(descriptors.data(), static_cast<nfds_t>(count), timeout_ms) <= 0) continue;
        std::lock_guard<std::mutex> lock(mutex_);
        if ((descriptors[0].revents & POLLIN) != 0) {
            std::uint64_t ignored{};
            while (::read(wake_fd_, &ignored, sizeof(ignored)) == sizeof(ignored)) {
            }
            process_control_commands();
        }
        if (stop_requested_.load() || lifecycle_ != LifecycleState::Running ||
            (count == 2UL && (descriptors[1].revents & POLLIN) == 0)) {
            if (count == 2UL && (descriptors[1].revents & (POLLHUP | POLLERR | POLLNVAL)) != 0)
                peer_ = PeerState::Detached;
            continue;
        }
        auto result = transport_.read(received.data(), received.size());
        if (!result) {
            peer_ = PeerState::Detached;
            continue;
        }
        if (result.value() == 0) continue;
        peer_ = PeerState::Active;
        rx_buffer_.insert(rx_buffer_.end(), received.begin(), received.begin() + result.value());
        while (rx_buffer_.size() >= kFrameSize) {
            Bytes request{};
            std::copy_n(rx_buffer_.begin(), kFrameSize, request.data.begin());
            rx_buffer_.erase(rx_buffer_.begin(), rx_buffer_.begin() + kFrameSize);
            ++rx_frames_;
            if (pending_response_) {
                if (std::equal(
                        request.data.begin(), request.data.begin() + kFrameBodySize,
                        pending_response_->request.data.begin())) {
                    history_.record(FrameDirection::Received, request, "Retry");
                } else {
                    history_.record(
                        FrameDirection::Received, request, "UnexpectedPipelinedRequest");
                }
                continue;
            }
            ++request_sequence_;
            bool should_respond = false;
            const auto response = handle_frame(request, should_respond);
            if (should_respond) schedule_response(response, request);
        }
    }
}
}  // namespace transmitter_simulator
