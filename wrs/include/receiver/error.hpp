#pragma once

#include <cstdint>

namespace receiver {

/** @brief Stable errors reported by the Receiver DDS runtime. */
enum class Error : std::uint8_t {
    InvalidArgument,
    AlreadyInitialized,
    InitializationFailed,
};

}  // namespace receiver
