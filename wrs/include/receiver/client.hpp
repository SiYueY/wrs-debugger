#pragma once

#include <cstdint>
#include <memory>

#include <hardware/result.hpp>
#include <receiver/error.hpp>
#include <receiver/protocol.hpp>
#include <receiver/transport.hpp>

namespace receiver {

class Client final {
public:
    Client();
    explicit Client(std::unique_ptr<IReceiverTransport> transport) noexcept;
    ~Client() noexcept;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    [[nodiscard]] hardware::Result<void, Error> connect(std::uint16_t domain_id) noexcept;
    [[nodiscard]] hardware::Result<void, Error> disconnect() noexcept;
    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] std::uint16_t domain_id() const noexcept;
    [[nodiscard]] hardware::Result<ReceiverInfo, Error> read_info() noexcept;
    [[nodiscard]] hardware::Result<SdoResponse, Error> read_sdo(std::uint16_t address) noexcept;
    [[nodiscard]] hardware::Result<SdoResponse, Error> write_sdo(
        std::uint16_t address, std::uint32_t data) noexcept;
    [[nodiscard]] hardware::Result<LoRaParameters, Error> read_lora_parameters() noexcept;
    [[nodiscard]] hardware::Result<void, Error> write_lora_parameters(
        const LoRaParameters&) noexcept;
    [[nodiscard]] hardware::Result<GfskParameters, Error> read_gfsk_parameters() noexcept;
    [[nodiscard]] hardware::Result<void, Error> write_gfsk_parameters(
        const GfskParameters&) noexcept;
    [[nodiscard]] hardware::Result<void, Error> restore_lora() noexcept;
    [[nodiscard]] hardware::Result<void, Error> restore_gfsk() noexcept;

private:
    std::unique_ptr<IReceiverTransport> transport_;
    bool connected_{false};
    std::uint16_t domain_id_{};
    std::uint32_t next_transaction_id_{1};
    [[nodiscard]] std::uint32_t next_transaction() noexcept;
};

}  // namespace receiver
