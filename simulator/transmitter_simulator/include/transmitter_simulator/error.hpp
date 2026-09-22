#pragma once

#include <cstdint>

namespace transmitter_simulator {

/** Runtime-control failures. Wire-level failures are represented in protocol response fields. */
enum class Error : std::uint8_t {
    None,  // Internal success sentinel used by worker control commands only.
    InvalidArgument,
    InvalidState,
    AlreadyRunning,
    Busy,
    Io,
    Disconnected,
    ProtocolError,
};

}  // namespace transmitter_simulator
