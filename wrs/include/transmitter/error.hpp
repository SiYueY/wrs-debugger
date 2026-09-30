#pragma once

#include <cstdint>

namespace transmitter {
/**
 * @brief 发射端协议模块的错误码。
 * @note 枚举值对应 Python 绑定中的 TRANSMITTER_<编号>，已有编号不可调整。
 */
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
