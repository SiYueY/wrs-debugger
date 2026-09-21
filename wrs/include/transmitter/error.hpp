#pragma once

#include <cstdint>

namespace transmitter {
/** @brief Stable errors reported by the transmitter protocol module. */
enum class Error : std::uint8_t {
    InvalidArgument,
    InvalidState,
    AlreadyOpen,
    NotOpen,
    Unsupported,
    DeviceNotFound,
    PermissionDenied,
    Busy,
    Disconnected,
    TimedOut,
    Io,
    ProtocolMismatch,
    InvalidResponse,
    UnexpectedResponse,
    CrcMismatch,
    FrameSyncLost,
    DeviceRejected,
    SdoNotReceived,
    SdoInProgress,
    SdoError,
    SdoInvalidCommand,
};

}  // namespace transmitter
