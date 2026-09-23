#pragma once

#include <cstdint>

#include <hardware/result.hpp>
#include <receiver/error.hpp>
#include <receiver/protocol.hpp>

namespace receiver {

class IReceiverTransport {
public:
    virtual ~IReceiverTransport() noexcept = default;
    [[nodiscard]] virtual hardware::Result<void, Error> connect(
        std::uint16_t domain_id) noexcept = 0;
    [[nodiscard]] virtual hardware::Result<void, Error> disconnect() noexcept = 0;
    [[nodiscard]] virtual hardware::Result<ReceiverInfo, Error> read_info() noexcept = 0;
    [[nodiscard]] virtual hardware::Result<SdoResponse, Error> read_sdo(
        const SdoRequest& request) noexcept = 0;
    [[nodiscard]] virtual hardware::Result<SdoResponse, Error> write_sdo(
        const SdoRequest& request) noexcept = 0;
    [[nodiscard]] virtual hardware::Result<ParameterResponse<LoRaParameters>, Error> read_lora(
        const ParameterRequest<LoRaParameters>& request) noexcept = 0;
    [[nodiscard]] virtual hardware::Result<ParameterResponse<LoRaParameters>, Error> write_lora(
        const ParameterRequest<LoRaParameters>& request) noexcept = 0;
    [[nodiscard]] virtual hardware::Result<ParameterResponse<GfskParameters>, Error> read_gfsk(
        const ParameterRequest<GfskParameters>& request) noexcept = 0;
    [[nodiscard]] virtual hardware::Result<ParameterResponse<GfskParameters>, Error> write_gfsk(
        const ParameterRequest<GfskParameters>& request) noexcept = 0;
};

}  // namespace receiver
