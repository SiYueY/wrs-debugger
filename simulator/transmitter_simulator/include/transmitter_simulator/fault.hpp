#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace transmitter_simulator {

enum class SdoFault : std::uint8_t {
    None = 0xff,
    NotReceived = 0x0,
    InProgress = 0xb,
    Error = 0x8,
    InvalidCommand = 0xe,
};
enum class DeliveryFault : std::uint8_t { None, Drop, Split, Truncate, Disconnect };

struct FaultConfig final {
    std::chrono::milliseconds response_delay{0};
    bool fail_next_business_response{};
    SdoFault next_sdo_fault{SdoFault::None};
    bool wrong_next_transaction{};
    bool wrong_next_command{};
    bool corrupt_next_crc{};
    DeliveryFault next_delivery_fault{DeliveryFault::None};
    std::size_t split_after_bytes{};
    std::chrono::milliseconds split_delay{0};
    std::size_t truncate_after_bytes{};
};

[[nodiscard]] bool valid_fault_config(const FaultConfig& config) noexcept;

}  // namespace transmitter_simulator
