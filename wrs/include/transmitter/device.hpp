#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <hardware/result.hpp>
#include <transmitter/error.hpp>

namespace transmitter {
/** @brief Identity values read from the transmitter SDO dictionary. */
struct DeviceIdentity final {
    std::uint32_t product_code{0};
    std::uint32_t version_number{0};
    std::uint32_t serial_number{0};
};
/** @brief Optional USB metadata discovered through Linux sysfs. */
struct UsbInfo final {
    bool available{false};
    std::uint16_t vendor_id{0};
    std::uint16_t product_id{0};
    std::string serial_number;
    std::string manufacturer;
    std::string product;
};
/** @brief A verified transmitter and its transport metadata. */
struct DeviceInfo final {
    std::string path;
    std::string description;
    UsbInfo usb{};
    DeviceIdentity identity{};
};
/** @brief Finds all candidates that pass the three read-only identity probes. */
[[nodiscard]] hardware::Result<std::vector<DeviceInfo>, Error> discover(
    std::chrono::milliseconds response_timeout, std::chrono::milliseconds retry_interval,
    std::uint8_t max_attempts) noexcept;
}  // namespace transmitter
