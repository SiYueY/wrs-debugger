#include <transmitter_simulator/device.hpp>

#include <algorithm>
#include <cstring>

namespace transmitter_simulator {
namespace {
constexpr std::uint32_t k_upgrade_request_value = 0x454e;

std::array<std::uint8_t, 40> metadata_value(const char* value) {
    std::array<std::uint8_t, 40> result{};
    const auto length = std::min(std::strlen(value), result.size());
    std::copy_n(reinterpret_cast<const std::uint8_t*>(value), length, result.begin());
    return result;
}

FirmwareMetadata default_firmware_metadata() {
    FirmwareMetadata metadata{};
    metadata.app_firmware_version = metadata_value("sim-app-1.0.0");
    metadata.bootloader_firmware_version = metadata_value("sim-boot-1.0.0");
    metadata.app_branch_name = metadata_value("simulator");
    metadata.app_tag_sha1_id = metadata_value("0000000000000000000000000000000000000000");
    metadata.boot_branch_name = metadata_value("simulator");
    metadata.boot_tag_sha1_id = metadata_value("0000000000000000000000000000000000000000");
    return metadata;
}

std::uint32_t segment(
    const std::array<std::uint8_t, 40>& source, std::uint16_t object, std::uint16_t base) noexcept {
    const auto offset = static_cast<std::size_t>(object - base) * 4;
    return static_cast<std::uint32_t>(source[offset]) |
           (static_cast<std::uint32_t>(source[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(source[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(source[offset + 3]) << 24U);
}

bool reserved_object(std::uint16_t object) noexcept {
    return (object >= 0x004 && object <= 0x007) || (object >= 0x044 && object <= 0x0ff);
}
}  // namespace

Device::Device() { reset(); }

const DeviceState& Device::state() const noexcept { return state_; }

void Device::reset() noexcept {
    state_ = {};
    state_.firmware = default_firmware_metadata();
    state_.lora = LoraConfig::defaults();
    state_.gfsk = GfskConfig::defaults();
}

Result<void, Error> Device::set_identity(const DeviceIdentity& identity) noexcept {
    state_.identity = identity;
    return Result<void, Error>::success();
}

Result<void, Error> Device::set_pin(const Pin& pin) noexcept {
    PinFrame pin_frame{};
    pin_frame.pin = pin.digits;
    if (!pin_frame.is_ascii_pin() || pin_frame.is_read_request())
        return Result<void, Error>::failure(Error::InvalidArgument);
    state_.pin = pin;
    return Result<void, Error>::success();
}

Result<void, Error> Device::set_battery(std::uint8_t percentage) noexcept {
    if (percentage > 100) return Result<void, Error>::failure(Error::InvalidArgument);
    state_.battery_percentage = percentage;
    return Result<void, Error>::success();
}

Result<void, Error> Device::set_lora(const LoraConfig& config) noexcept {
    if (!config.is_valid()) return Result<void, Error>::failure(Error::InvalidArgument);
    state_.lora = config;
    return Result<void, Error>::success();
}

Result<void, Error> Device::set_gfsk(const GfskConfig& config) noexcept {
    if (!config.is_valid()) return Result<void, Error>::failure(Error::InvalidArgument);
    state_.gfsk = config;
    return Result<void, Error>::success();
}

Result<void, Error> Device::set_firmware_metadata(const FirmwareMetadata& metadata) noexcept {
    state_.firmware = metadata;
    return Result<void, Error>::success();
}

Result<void, Error> Device::set_sdo_editor_state(const SdoEditorState& state) noexcept {
    DeviceState next = state_;
    next.identity = state.identity;
    next.firmware = state.firmware;
    next.battery_percentage = state.battery_percentage;
    next.upgrade_request = state.upgrade_request;
    state_ = next;
    return Result<void, Error>::success();
}

Result<std::uint32_t, SdoAccessError> Device::read_sdo(std::uint16_t object) const noexcept {
    switch (object) {
        case sdo::ProductCode:
            return Result<std::uint32_t, SdoAccessError>::success(state_.identity.product_code);
        case sdo::VersionNumber:
            return Result<std::uint32_t, SdoAccessError>::success(state_.identity.version_number);
        case sdo::SerialNumber:
            return Result<std::uint32_t, SdoAccessError>::success(state_.identity.serial_number);
        case sdo::BatteryLevel:
            return Result<std::uint32_t, SdoAccessError>::success(state_.battery_percentage);
        case sdo::UpgradeRequest:
            return Result<std::uint32_t, SdoAccessError>::success(state_.upgrade_request);
        default:
            break;
    }
    if (reserved_object(object)) return Result<std::uint32_t, SdoAccessError>::success(0);
    if (object >= 0x008 && object <= 0x011)
        return Result<std::uint32_t, SdoAccessError>::success(
            segment(state_.firmware.app_firmware_version, object, 0x008));
    if (object >= 0x012 && object <= 0x01b)
        return Result<std::uint32_t, SdoAccessError>::success(
            segment(state_.firmware.bootloader_firmware_version, object, 0x012));
    if (object >= 0x01c && object <= 0x025)
        return Result<std::uint32_t, SdoAccessError>::success(
            segment(state_.firmware.app_branch_name, object, 0x01c));
    if (object >= 0x026 && object <= 0x02f)
        return Result<std::uint32_t, SdoAccessError>::success(
            segment(state_.firmware.app_tag_sha1_id, object, 0x026));
    if (object >= 0x030 && object <= 0x039)
        return Result<std::uint32_t, SdoAccessError>::success(
            segment(state_.firmware.boot_branch_name, object, 0x030));
    if (object >= 0x03a && object <= 0x043)
        return Result<std::uint32_t, SdoAccessError>::success(
            segment(state_.firmware.boot_tag_sha1_id, object, 0x03a));
    return Result<std::uint32_t, SdoAccessError>::failure(SdoAccessError::Unknown);
}

Result<void, SdoAccessError> Device::write_sdo(std::uint16_t object, std::uint32_t value) noexcept {
    if (object == sdo::UpgradeRequest) {
        if (value != k_upgrade_request_value)
            return Result<void, SdoAccessError>::failure(SdoAccessError::InvalidValue);
        state_.upgrade_request = value;
        return Result<void, SdoAccessError>::success();
    }
    if (read_sdo(object)) return Result<void, SdoAccessError>::failure(SdoAccessError::ReadOnly);
    return Result<void, SdoAccessError>::failure(SdoAccessError::Unknown);
}

Result<void, Error> Device::write_pin(const Pin& pin) noexcept { return set_pin(pin); }

}  // namespace transmitter_simulator
