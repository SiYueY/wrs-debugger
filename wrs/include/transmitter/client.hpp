#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

#include <wrs/result.hpp>
#include <serial/port.hpp>
#include <transmitter/device.hpp>
#include <transmitter/error.hpp>
#include <transmitter/protocol.hpp>

namespace transmitter {
/** @brief Synchronous, single-owner client for one verified transmitter. */
class Client final {
public:
    Client() noexcept = default;
    ~Client() noexcept;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) = delete;
    /** @brief Opens a fixed-format serial port and verifies three identity SDOs. */
    [[nodiscard]] wrs::Result<void, Error> open(
        const std::string& path, std::chrono::milliseconds response_timeout,
        std::chrono::milliseconds retry_interval, std::uint8_t max_attempts) noexcept;
    /** @brief Closes the port; closing an already closed client succeeds. */
    [[nodiscard]] wrs::Result<void, Error> close() noexcept;
    /** @brief Reports whether the verified serial connection is ready. */
    [[nodiscard]] bool is_open() const noexcept;
    /** @brief Returns the identity captured by the successful open probe; zeroed when closed. */
    [[nodiscard]] const DeviceIdentity& identity() const noexcept;
    /** @brief Uses normal communication to read the wireless E-stop box Device ID. */
    [[nodiscard]] wrs::Result<std::array<std::uint8_t, 3>, Error> read_device_id() noexcept;
    /** @brief Starts a factory binding transaction; response carries generated Device ID and Kbind.
     */
    [[nodiscard]] wrs::Result<DeviceKeyFrame, Error> prepare_binding() noexcept;
    /** @brief Verifies the wireless binding through FIND with the same transaction. */
    [[nodiscard]] wrs::Result<void, Error> find_binding(const DeviceKeyFrame& binding) noexcept;
    /** @brief Cancels a pending transmitter binding transaction. */
    [[nodiscard]] wrs::Result<void, Error> cancel_binding(const DeviceKeyFrame& binding) noexcept;
    /** @brief Reads and validates the complete current LoRa parameters. */
    [[nodiscard]] wrs::Result<LoRaParamFrame, Error> read_lora_parameters() noexcept;
    /** @brief Reads and validates the complete current GFSK parameters. */
    [[nodiscard]] wrs::Result<GfskParamFrame, Error> read_gfsk_parameters() noexcept;
    /** @brief Validates and writes a complete LoRa parameter set. */
    [[nodiscard]] wrs::Result<void, Error> write_lora_parameters(
        const LoRaParamFrame& parameter_frame) noexcept;
    /** @brief Validates and writes a complete GFSK parameter set. */
    [[nodiscard]] wrs::Result<void, Error> write_gfsk_parameters(
        const GfskParamFrame& parameter_frame) noexcept;
    /** @brief Writes the complete protocol-defined factory parameter set. */
    [[nodiscard]] wrs::Result<void, Error> restore_default_parameters(RadioType) noexcept;
    /** @brief Reads the current PIN using the reserved all-zero request value. */
    [[nodiscard]] wrs::Result<PinFrame, Error> read_pin() noexcept;
    /** @brief Writes a six-digit PIN other than the reserved all-zero value. */
    [[nodiscard]] wrs::Result<void, Error> write_pin(const PinFrame& pin_frame) noexcept;
    /** @brief Reads any protocol-defined 12-bit SDO object value. */
    [[nodiscard]] wrs::Result<SdoFrame, Error> read_sdo(std::uint16_t object_address) noexcept;
    /** @brief Compatibility overload for the stable SDO object enum. */
    [[nodiscard]] wrs::Result<SdoFrame, Error> read_sdo(SdoObject object) noexcept;
    /** @brief Writes the only V1 writable SDO object, UpgradeRequest. */
    [[nodiscard]] wrs::Result<void, Error> write_sdo(const SdoFrame& request_frame) noexcept;

private:
    serial::Port port_;
    std::chrono::milliseconds response_timeout_{};
    std::chrono::milliseconds retry_interval_{};
    std::uint8_t max_attempts_{0};
    DeviceIdentity identity_{};
    std::uint32_t next_transaction_id_{1};
    [[nodiscard]] std::uint32_t next_transaction() noexcept;
    [[nodiscard]] wrs::Result<Bytes, Error> exchange(
        const Bytes& request, std::uint32_t transaction_id, SystemCmd expected_command) noexcept;
};
}  // namespace transmitter
