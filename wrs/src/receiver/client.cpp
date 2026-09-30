#include <receiver/client.hpp>

#include <receiver/driver_proxy.hpp>

#include <wrs/logging.hpp>

namespace receiver {
namespace {
template <typename T>
using Result = wrs::Result<T, Error>;

}  // namespace

Client::Client() noexcept : driver_(new DriverProxy()) {}

Client::~Client() noexcept = default;

Client::Client(Client&&) noexcept = default;

Client& Client::operator=(Client&&) noexcept = default;

Result<void> Client::connect(std::uint16_t domain_id) noexcept {
    if (domain_id > 232) return Result<void>::failure(Error::InvalidArgument);
    return driver_->connect(domain_id);
}

Result<void> Client::disconnect() noexcept { return driver_->disconnect(); }

bool Client::is_connected() const noexcept { return driver_->is_connected(); }

std::uint16_t Client::domain_id() const noexcept { return driver_->domain_id(); }

Result<ReceiverInfo> Client::read_info() noexcept { return driver_->read_info(); }

Result<ReceiverState> Client::read_wireless_estop_state() noexcept {
    return driver_->read_wireless_estop_state();
}

Result<BindingState> Client::binding_state() noexcept { return driver_->binding_state(); }

Result<void> Client::start_binding(
    std::uint32_t device_id, const std::array<std::uint8_t, 16>& kbind,
    std::uint32_t transaction_id) noexcept {
    if (device_id == 0 || device_id > 0xffffffU || transaction_id == 0)
        return Result<void>::failure(Error::InvalidArgument);
    if (!is_connected()) return Result<void>::failure(Error::NotConnected);
    return driver_->start_binding(device_id, kbind, transaction_id);
}

Result<SdoResponse> Client::read_sdo(std::uint16_t address) noexcept {
    if (!is_connected()) return Result<SdoResponse>::failure(Error::NotConnected);
    if (address == 0 || address > 0x0fffU)
        return Result<SdoResponse>::failure(Error::InvalidArgument);
    return driver_->read_sdo(address);
}

Result<SdoResponse> Client::write_sdo(std::uint16_t address, std::uint32_t data) noexcept {
    if (!is_connected()) return Result<SdoResponse>::failure(Error::NotConnected);
    if (address == 0 || address > 0x0fffU)
        return Result<SdoResponse>::failure(Error::InvalidArgument);
    return driver_->write_sdo(address, data);
}

Result<LoRaParameters> Client::read_lora_parameters() noexcept {
    return driver_->read_lora_parameters();
}

Result<void> Client::write_lora_parameters(const LoRaParameters& parameters) noexcept {
    if (!is_connected()) return Result<void>::failure(Error::NotConnected);
    if (!valid(parameters)) return Result<void>::failure(Error::InvalidArgument);
    return driver_->write_lora_parameters(parameters);
}

Result<GfskParameters> Client::read_gfsk_parameters() noexcept {
    return driver_->read_gfsk_parameters();
}

Result<void> Client::write_gfsk_parameters(const GfskParameters& parameters) noexcept {
    if (!is_connected()) return Result<void>::failure(Error::NotConnected);
    if (!valid(parameters)) return Result<void>::failure(Error::InvalidArgument);
    return driver_->write_gfsk_parameters(parameters);
}

Result<void> Client::restore_lora() noexcept {
    WRS_LOG_INFO("receiver.client") << "Restore LoRa defaults request";
    const auto defaults = default_lora_parameters();
    auto written = write_lora_parameters(defaults);
    if (!written) {
        WRS_LOG_ERROR("receiver.client") << "Restore LoRa defaults failed while writing\n  error: "
                                         << static_cast<unsigned int>(written.error());
        return written;
    }
    auto actual = read_lora_parameters();
    if (!actual) {
        WRS_LOG_ERROR("receiver.client")
            << "Restore LoRa defaults failed while reading back\n  error: "
            << static_cast<unsigned int>(actual.error());
        return Result<void>::failure(actual.error());
    }
    if (same_parameters(actual.value(), defaults)) {
        WRS_LOG_INFO("receiver.client") << "Restore LoRa defaults succeeded";
        return Result<void>::success();
    }
    WRS_LOG_ERROR("receiver.client")
        << "Restore LoRa defaults expected"
        << "\n  param_flags: " << defaults.param_flags << "\n  tx_power: " << defaults.tx_power
        << "\n  freq_offset: " << defaults.freq_offset
        << "\n  payload_len: " << static_cast<unsigned int>(defaults.payload_len)
        << "\n  rssi_threshold: " << static_cast<unsigned int>(defaults.rssi_threshold)
        << "\n  heartbeat_interval: " << defaults.heartbeat_interval
        << "\n  heartbeat_loss: " << static_cast<unsigned int>(defaults.heartbeat_loss)
        << "\n  bandwidth: " << static_cast<unsigned int>(defaults.bandwidth)
        << "\n  spreading_factor: " << static_cast<unsigned int>(defaults.spreading_factor)
        << "\n  coding_rate: " << static_cast<unsigned int>(defaults.coding_rate)
        << "\n  header_type: " << static_cast<unsigned int>(defaults.header_type)
        << "\n  preamble_len: " << static_cast<unsigned int>(defaults.preamble_len)
        << "\n  sync_word: " << defaults.sync_word;
    WRS_LOG_ERROR("receiver.client")
        << "Restore LoRa defaults read back"
        << "\n  param_flags: " << actual.value().param_flags
        << "\n  tx_power: " << actual.value().tx_power
        << "\n  freq_offset: " << actual.value().freq_offset
        << "\n  payload_len: " << static_cast<unsigned int>(actual.value().payload_len)
        << "\n  rssi_threshold: " << static_cast<unsigned int>(actual.value().rssi_threshold)
        << "\n  heartbeat_interval: " << actual.value().heartbeat_interval
        << "\n  heartbeat_loss: " << static_cast<unsigned int>(actual.value().heartbeat_loss)
        << "\n  bandwidth: " << static_cast<unsigned int>(actual.value().bandwidth)
        << "\n  spreading_factor: " << static_cast<unsigned int>(actual.value().spreading_factor)
        << "\n  coding_rate: " << static_cast<unsigned int>(actual.value().coding_rate)
        << "\n  header_type: " << static_cast<unsigned int>(actual.value().header_type)
        << "\n  preamble_len: " << static_cast<unsigned int>(actual.value().preamble_len)
        << "\n  sync_word: " << actual.value().sync_word;
    return Result<void>::failure(Error::UnexpectedResponse);
}

Result<void> Client::restore_gfsk() noexcept {
    WRS_LOG_INFO("receiver.client") << "Restore GFSK defaults request";
    if (!is_connected()) {
        WRS_LOG_ERROR("receiver.client")
            << "Restore GFSK defaults failed: receiver is not connected";
        return Result<void>::failure(Error::NotConnected);
    }
    WRS_LOG_WARNING("receiver.client") << "Restore GFSK defaults is unsupported";
    return Result<void>::failure(Error::Unsupported);
}
}  // namespace receiver
