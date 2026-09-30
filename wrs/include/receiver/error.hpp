#pragma once

#include <cstdint>

namespace receiver {

/**
 * @brief 接收端 DDS 模块的错误码。
 * @note 枚举值对应 Python 绑定中的 RECEIVER_<编号>，已有编号不可调整。
 */
enum class Error : std::uint8_t {
    InvalidArgument,       // 参数无效
    AlreadyInitialized,    // DDS 运行时已初始化
    InitializationFailed,  // DDS 运行时初始化失败
    NotConnected,          // 尚未连接 DDS 驱动
    AlreadyConnected,      // 已连接 DDS 驱动
    DdsUnavailable,        // DDS 服务不可用
    TimedOut,              // DDS 请求超时
    Disconnected,          // 连接已断开
    UnexpectedResponse,    // 响应内容不符合预期
    TransactionMismatch,   // 事务 ID 不匹配
    DeviceRejected,        // 设备拒绝请求
    NotReceived,           // 尚未收到状态数据
    InProgress,            // 操作仍在进行
    ExecutionError,        // 操作执行失败
    InvalidCommand,        // 命令无效
    Unsupported,           // 当前驱动不支持该操作
};

}  // namespace receiver
