#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

#include <wrs/result.hpp>
#include <serial/port.hpp>
#include <transmitter/device.hpp>
#include <transmitter/error.hpp>
#include <transmitter/protocol.hpp>

namespace transmitter {

/**
 * @brief 发射端串口客户端。
 *
 * 每个实例独占一个串口连接；调用 open() 成功后才能执行协议操作。
 */
class Client final {
public:
    Client() noexcept = default;
    ~Client() noexcept;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) = delete;

    /**
     * @brief 打开串口并读取发射端设备身份。
     * @param path 串口设备路径。
     * @param response_timeout 单次请求的读、写超时时间，必须大于 0。
     * @param retry_interval 重试间隔，必须大于或等于 0。
     * @param max_attempts 单次请求的最大尝试次数，必须大于 0。
     * @return 成功时建立连接并保存产品编码、版本号和序列号；失败时返回错误码。
     * @note 串口使用 115200、8N1、无流控；打开后依次读取三个身份 SDO 对象。
     */
    [[nodiscard]] wrs::Result<void, Error> open(
        const std::string& path, std::chrono::milliseconds response_timeout,
        std::chrono::milliseconds retry_interval, std::uint8_t max_attempts) noexcept;

    /**
     * @brief 关闭串口并清空已读取的设备身份。
     * @return 成功或串口原本未打开时返回成功，否则返回串口错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> close() noexcept;

    /**
     * @brief 判断串口是否已打开。
     * @return true=已打开，false=未打开。
     */
    [[nodiscard]] bool is_open() const noexcept;

    /**
     * @brief 获取打开连接时读取的设备身份。
     * @return 设备身份的只读引用；连接关闭后各字段为 0。
     */
    [[nodiscard]] const DeviceIdentity& identity() const noexcept;

    /**
     * @brief 通过运行状态响应读取无线急停盒的设备 ID。
     * @return 三字节设备 ID；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<std::array<std::uint8_t, 3>, Error> read_device_id() noexcept;

    /**
     * @brief 发起绑定请求。
     * @return 响应中的设备 ID、Kbind 和事务 ID；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<DeviceKeyFrame, Error> prepare_binding() noexcept;

    /**
     * @brief 使用绑定信息发送寻机请求。
     * @param binding 绑定响应中的设备 ID、Kbind 和事务 ID。
     * @return 寻机响应的设备 ID 匹配时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> find_binding(const DeviceKeyFrame& binding) noexcept;

    /**
     * @brief 使用绑定信息发送取消绑定请求。
     * @param binding 绑定响应中的设备 ID 和事务 ID。
     * @return 响应有效时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> cancel_binding(const DeviceKeyFrame& binding) noexcept;

    /**
     * @brief 读取并校验 LoRa 通信参数。
     * @return 当前 LoRa 参数帧；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<LoRaParamFrame, Error> read_lora_parameters() noexcept;

    /**
     * @brief 读取并校验 GFSK 通信参数。
     * @return 当前 GFSK 参数帧；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<GfskParamFrame, Error> read_gfsk_parameters() noexcept;

    /**
     * @brief 校验并写入 LoRa 通信参数。
     * @param parameter_frame 待写入的完整 LoRa 参数帧。
     * @return 写入响应有效时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> write_lora_parameters(
        const LoRaParamFrame& parameter_frame) noexcept;

    /**
     * @brief 校验并写入 GFSK 通信参数。
     * @param parameter_frame 待写入的完整 GFSK 参数帧。
     * @return 写入响应有效时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> write_gfsk_parameters(
        const GfskParamFrame& parameter_frame) noexcept;

    /**
     * @brief 写入指定调制方式的默认通信参数。
     * @param modulation LoRa 或 GFSK；其他取值返回 InvalidArgument。
     * @return 写入成功时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> restore_default_parameters(
        ParamFlags::Modulation modulation) noexcept;

    /**
     * @brief 使用 ASCII "000000" 请求读取当前 PIN 码。
     * @return 包含六位 ASCII PIN 的响应帧；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<PinFrame, Error> read_pin() noexcept;

    /**
     * @brief 写入六位 ASCII PIN 码。
     * @param pin_frame 待写入的 PIN 帧；"000000" 为读取请求的保留值。
     * @return 响应 PIN 匹配时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> write_pin(const PinFrame& pin_frame) noexcept;

    /**
     * @brief 读取指定地址的 SDO 对象。
     * @param object_address 低 12 位 SDO 对象地址，高 4 位必须为 0。
     * @return SDO 响应帧；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<SdoFrame, Error> read_sdo(std::uint16_t object_address) noexcept;

    /**
     * @brief 使用预定义的 SDO 对象枚举读取对象。
     * @param object SDO 对象。
     * @return SDO 响应帧；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<SdoFrame, Error> read_sdo(SdoObject object) noexcept;

    /**
     * @brief 写入固件升级请求 SDO 对象。
     * @param request_frame 对象地址为 UpgradeRequest、数据为协议升级标记的请求帧。
     * @return 写入状态成功时返回成功，否则返回错误码。
     * @note V1 协议仅支持写入此 SDO 对象。
     */
    [[nodiscard]] wrs::Result<void, Error> write_sdo(const SdoFrame& request_frame) noexcept;

private:
    serial::Port port_;                             // 当前客户端独占的串口
    std::chrono::milliseconds response_timeout_{};  // 单次请求的写入和读取超时时间
    std::chrono::milliseconds retry_interval_{};    // 两次请求尝试之间的等待时间
    std::uint8_t max_attempts_{0};                  // 单次请求的最大尝试次数
    DeviceIdentity identity_{};                     // 打开连接时读取的设备身份
    std::uint32_t next_transaction_id_{1};          // 下一个事务 ID，跳过 0

    /**
     * @brief 生成非零事务 ID，并推进计数器。
     * @return 本次请求使用的事务 ID；计数器溢出后从 1 继续。
     */
    [[nodiscard]] std::uint32_t next_transaction() noexcept;

    /**
     * @brief 发送请求并接收对应响应。
     * @param request 待发送的 42 字节请求帧。
     * @param transaction_id 预期响应的事务 ID；运行状态通信使用 0。
     * @param expected_command 预期响应命令。
     * @return 通过 CRC、命令、事务 ID 和结果码校验的响应帧；失败时返回错误码。
     * @note 完整请求超时后按配置重试；收发部分帧时关闭连接。
     */
    [[nodiscard]] wrs::Result<Frame, Error> exchange(
        const Frame& request, std::uint32_t transaction_id,
        SystemCommand expected_command) noexcept;
};

}  // namespace transmitter
