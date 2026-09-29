#include <receiver/client.hpp>

#include <receiver/driver_proxy.hpp>

namespace receiver {
namespace {
template <typename T>
using Result = wrs::Result<T, Error>;
}

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
Result<ReceiverState> Client::read_state() noexcept { return driver_->read_state(); }
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
    const auto defaults = default_lora_parameters();
    auto written = write_lora_parameters(defaults);
    if (!written) return written;
    auto actual = read_lora_parameters();
    if (!actual) return Result<void>::failure(actual.error());
    return same_parameters(actual.value(), defaults)
               ? Result<void>::success()
               : Result<void>::failure(Error::UnexpectedResponse);
}
Result<void> Client::restore_gfsk() noexcept {
    if (!is_connected()) return Result<void>::failure(Error::NotConnected);
    return Result<void>::failure(Error::Unsupported);
}
}  // namespace receiver
