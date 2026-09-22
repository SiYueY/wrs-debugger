#pragma once

#include <array>
#include <cstdint>

#include <transmitter_simulator/error.hpp>
#include <transmitter_simulator/protocol.hpp>
#include <transmitter_simulator/result.hpp>

namespace transmitter_simulator {

struct DeviceIdentity final {
    std::uint32_t product_code{0x57525301};
    std::uint32_t version_number{0x00010000};
    std::uint32_t serial_number{0x53494d01};
};

struct Pin final {
    std::array<std::uint8_t, 6> digits{{'1', '2', '3', '4', '5', '6'}};
};

struct FirmwareMetadata final {
    std::array<std::uint8_t, 40> app_firmware_version{};
    std::array<std::uint8_t, 40> bootloader_firmware_version{};
    std::array<std::uint8_t, 40> app_branch_name{};
    std::array<std::uint8_t, 40> app_tag_sha1_id{};
    std::array<std::uint8_t, 40> boot_branch_name{};
    std::array<std::uint8_t, 40> boot_tag_sha1_id{};
};

struct DeviceState final {
    DeviceIdentity identity{};
    FirmwareMetadata firmware{};
    Pin pin{};
    LoraConfig lora{LoraConfig::defaults()};
    GfskConfig gfsk{GfskConfig::defaults()};
    std::uint8_t battery_percentage{100};
    std::uint32_t upgrade_request{};
};

/** GUI-only editable projection of the non-reserved SDO dictionary values.
 *
 * This deliberately does not change the access permissions enforced for serial SDO writes.
 */
struct SdoEditorState final {
    DeviceIdentity identity{};
    FirmwareMetadata firmware{};
    std::uint8_t battery_percentage{100};
    std::uint32_t upgrade_request{};
};

enum class SdoAccessError : std::uint8_t { None, Unknown, ReadOnly, InvalidValue };

class Device final {
public:
    Device();

    [[nodiscard]] const DeviceState& state() const noexcept;
    void reset() noexcept;

    [[nodiscard]] Result<void, Error> set_identity(const DeviceIdentity& identity) noexcept;
    [[nodiscard]] Result<void, Error> set_pin(const Pin& pin) noexcept;
    [[nodiscard]] Result<void, Error> set_battery(std::uint8_t percentage) noexcept;
    [[nodiscard]] Result<void, Error> set_lora(const LoraConfig& config) noexcept;
    [[nodiscard]] Result<void, Error> set_gfsk(const GfskConfig& config) noexcept;
    [[nodiscard]] Result<void, Error> set_firmware_metadata(
        const FirmwareMetadata& metadata) noexcept;
    [[nodiscard]] Result<void, Error> set_sdo_editor_state(const SdoEditorState& state) noexcept;

    [[nodiscard]] Result<std::uint32_t, SdoAccessError> read_sdo(
        std::uint16_t object) const noexcept;
    [[nodiscard]] Result<void, SdoAccessError> write_sdo(
        std::uint16_t object, std::uint32_t value) noexcept;
    [[nodiscard]] Result<void, Error> write_pin(const Pin& pin) noexcept;

private:
    DeviceState state_{};
};

}  // namespace transmitter_simulator
