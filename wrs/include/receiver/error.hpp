#pragma once

#include <cstdint>

namespace receiver {

/** @brief Stable errors reported by the Receiver DDS runtime. */
enum class Error : std::uint8_t {
    InvalidArgument,
    AlreadyInitialized,
    InitializationFailed,
    NotConnected,
    AlreadyConnected,
    DdsUnavailable,
    TimedOut,
    Disconnected,
    UnexpectedResponse,
    TransactionMismatch,
    DeviceRejected,
    NotReceived,
    InProgress,
    ExecutionError,
    InvalidCommand,
};

}  // namespace receiver
