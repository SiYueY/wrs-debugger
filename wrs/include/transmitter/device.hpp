#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <wrs/result.hpp>
#include <transmitter/error.hpp>

namespace transmitter {

/**
 * @brief 从发射端 SDO 字典读取的设备身份。
 */
struct DeviceIdentity final {
    std::uint32_t product_code{0};    // 产品编码
    std::uint32_t version_number{0};  // 版本号
    std::uint32_t serial_number{0};   // 序列号
};

/**
 * @brief 通过 Linux sysfs 获取的可选 USB 信息。
 */
struct UsbInfo final {
    bool available{false};        // true 表示 USB 信息可用
    std::uint16_t vendor_id{0};   // USB 厂商 ID
    std::uint16_t product_id{0};  // USB 产品 ID
    std::string serial_number;    // USB 序列号
    std::string manufacturer;     // 厂商名称
    std::string product;          // 产品名称
};

/**
 * @brief 已通过身份 SDO 验证的发射端及其串口信息。
 */
struct DeviceInfo final {
    std::string path;           // 串口设备路径
    std::string description;    // 串口描述
    UsbInfo usb{};              // 可选 USB 信息
    DeviceIdentity identity{};  // 发射端设备身份
};

/**
 * @brief 枚举串口并查找通过三个身份 SDO 验证的发射端。
 * @param response_timeout 单次请求的读、写超时时间，必须大于 0。
 * @param retry_interval 重试间隔，必须大于或等于 0。
 * @param max_attempts 单次请求的最大尝试次数，必须大于 0。
 * @return 已验证的设备列表；串口枚举或内存分配失败时返回错误码。
 * @note 单个候选串口探测失败不会中断其余串口的检查。
 */
[[nodiscard]] wrs::Result<std::vector<DeviceInfo>, Error> discover(
    std::chrono::milliseconds response_timeout, std::chrono::milliseconds retry_interval,
    std::uint8_t max_attempts) noexcept;

}  // namespace transmitter
