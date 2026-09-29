#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <hardware/result.hpp>
#include <receiver/error.hpp>
#include <receiver/protocol.hpp>

namespace receiver {

/** Communicates with the robot driver through its WirelessEStop DDS API. */
class DriverProxy final {
public:
    DriverProxy() noexcept;
    ~DriverProxy() noexcept;
    DriverProxy(const DriverProxy&) = delete;
    DriverProxy& operator=(const DriverProxy&) = delete;
    DriverProxy(DriverProxy&&) noexcept;
    DriverProxy& operator=(DriverProxy&&) noexcept;

    [[nodiscard]] hardware::Result<void, Error> connect(std::uint16_t domain_id) noexcept;
    [[nodiscard]] hardware::Result<void, Error> disconnect() noexcept;
    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] std::uint16_t domain_id() const noexcept;

    [[nodiscard]] hardware::Result<ReceiverInfo, Error> read_info() noexcept;
    [[nodiscard]] hardware::Result<ReceiverState, Error> read_state() noexcept;
    [[nodiscard]] hardware::Result<BindingState, Error> binding_state() noexcept;
    [[nodiscard]] hardware::Result<void, Error> start_binding(
        std::uint32_t device_id, const std::array<std::uint8_t, 16>& kbind,
        std::uint32_t transaction_id) noexcept;

    // Retained as tool capabilities; current driver does not expose these DDS services.
    [[nodiscard]] hardware::Result<SdoResponse, Error> read_sdo(std::uint16_t address) noexcept;
    [[nodiscard]] hardware::Result<SdoResponse, Error> write_sdo(
        std::uint16_t address, std::uint32_t data) noexcept;
    [[nodiscard]] hardware::Result<LoRaParameters, Error> read_lora_parameters() noexcept;
    [[nodiscard]] hardware::Result<void, Error> write_lora_parameters(
        const LoRaParameters& parameters) noexcept;
    [[nodiscard]] hardware::Result<GfskParameters, Error> read_gfsk_parameters() noexcept;
    [[nodiscard]] hardware::Result<void, Error> write_gfsk_parameters(
        const GfskParameters& parameters) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::uint16_t domain_id_{};
    std::uint32_t next_transaction_id_{1};
    [[nodiscard]] std::uint32_t next_transaction() noexcept;
};

}  // namespace receiver
