#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <wrs/result.hpp>
#include <receiver/error.hpp>
#include <receiver/protocol.hpp>

namespace receiver {

class DriverProxy;

/** Public receiver operations used by the debugger and Python binding. */
class Client final {
public:
    Client() noexcept;
    ~Client() noexcept;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    [[nodiscard]] wrs::Result<void, Error> connect(std::uint16_t domain_id) noexcept;
    [[nodiscard]] wrs::Result<void, Error> disconnect() noexcept;
    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] std::uint16_t domain_id() const noexcept;
    [[nodiscard]] wrs::Result<ReceiverInfo, Error> read_info() noexcept;
    [[nodiscard]] wrs::Result<ReceiverState, Error> read_state() noexcept;
    [[nodiscard]] wrs::Result<BindingState, Error> binding_state() noexcept;
    [[nodiscard]] wrs::Result<void, Error> start_binding(
        std::uint32_t device_id, const std::array<std::uint8_t, 16>& kbind,
        std::uint32_t transaction_id) noexcept;
    [[nodiscard]] wrs::Result<SdoResponse, Error> read_sdo(std::uint16_t address) noexcept;
    [[nodiscard]] wrs::Result<SdoResponse, Error> write_sdo(
        std::uint16_t address, std::uint32_t data) noexcept;
    [[nodiscard]] wrs::Result<LoRaParameters, Error> read_lora_parameters() noexcept;
    [[nodiscard]] wrs::Result<void, Error> write_lora_parameters(
        const LoRaParameters& parameters) noexcept;
    [[nodiscard]] wrs::Result<GfskParameters, Error> read_gfsk_parameters() noexcept;
    [[nodiscard]] wrs::Result<void, Error> write_gfsk_parameters(
        const GfskParameters& parameters) noexcept;
    [[nodiscard]] wrs::Result<void, Error> restore_lora() noexcept;
    [[nodiscard]] wrs::Result<void, Error> restore_gfsk() noexcept;

private:
    std::unique_ptr<DriverProxy> driver_;
};

}  // namespace receiver
