#include <transmitter_simulator/fault.hpp>

namespace transmitter_simulator {
bool valid_fault_config(const FaultConfig& config) noexcept {
    if (config.response_delay.count() < 0 || config.split_delay.count() < 0) return false;
    if (config.next_delivery_fault == DeliveryFault::Split)
        return config.split_after_bytes > 0 && config.split_after_bytes < 42;
    if (config.next_delivery_fault == DeliveryFault::Truncate)
        return config.truncate_after_bytes > 0 && config.truncate_after_bytes < 42;
    return true;
}
}  // namespace transmitter_simulator
