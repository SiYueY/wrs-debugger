#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <wrs/result.hpp>
#include <receiver/error.hpp>
#include <receiver/protocol.hpp>

namespace receiver {

/**
 * @brief 通过 WirelessEStop DDS 接口与机器人驱动通信。
 */
class DriverProxy final {
public:
    DriverProxy() noexcept;
    ~DriverProxy() noexcept;
    DriverProxy(const DriverProxy&) = delete;
    DriverProxy& operator=(const DriverProxy&) = delete;
    DriverProxy(DriverProxy&&) noexcept;
    DriverProxy& operator=(DriverProxy&&) noexcept;

    /**
     * @brief 初始化 DDS 运行时并连接指定域。
     * @param domain_id DDS 域 ID。
     * @return 连接成功时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> connect(std::uint16_t domain_id) noexcept;

    /**
     * @brief 断开 DDS 连接并释放本实例拥有的运行时。
     * @return 断开结果。
     */
    [[nodiscard]] wrs::Result<void, Error> disconnect() noexcept;

    /**
     * @brief 判断 DDS 节点是否已建立。
     * @return true=已连接，false=未连接。
     */
    [[nodiscard]] bool is_connected() const noexcept;

    /**
     * @brief 获取当前 DDS 域 ID。
     * @return 已连接的域 ID；断开后为 0。
     */
    [[nodiscard]] std::uint16_t domain_id() const noexcept;

    /**
     * @brief 查询已绑定设备信息。
     * @return 设备信息；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<ReceiverInfo, Error> read_info() noexcept;

    /**
     * @brief 读取最新的无线急停状态 Topic 样本。
     * @return 最新状态；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<ReceiverState, Error> read_wireless_estop_state() noexcept;

    /**
     * @brief 查询绑定状态。
     * @return 绑定状态；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<BindingState, Error> binding_state() noexcept;

    /**
     * @brief 向驱动发送绑定请求。
     * @param device_id 24 位设备 ID。
     * @param kbind 16 字节绑定密钥。
     * @param transaction_id 绑定事务 ID。
     * @return 驱动接受请求时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> start_binding(
        std::uint32_t device_id, const std::array<std::uint8_t, 16>& kbind,
        std::uint32_t transaction_id) noexcept;

    /**
     * @brief 读取 SDO 对象。
     * @param address SDO 对象地址。
     * @return 当前驱动未提供该 DDS 服务时返回 Unsupported。
     */
    [[nodiscard]] wrs::Result<SdoResponse, Error> read_sdo(std::uint16_t address) noexcept;

    /**
     * @brief 写入 SDO 对象。
     * @param address SDO 对象地址。
     * @param data 待写入数据。
     * @return 当前驱动未提供该 DDS 服务时返回 Unsupported。
     */
    [[nodiscard]] wrs::Result<SdoResponse, Error> write_sdo(
        std::uint16_t address, std::uint32_t data) noexcept;

    /**
     * @brief 读取 LoRa 通信参数。
     * @return 参数；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<LoRaParameters, Error> read_lora_parameters() noexcept;

    /**
     * @brief 写入 LoRa 通信参数。
     * @param parameters 待写入参数。
     * @return 驱动接受请求时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> write_lora_parameters(
        const LoRaParameters& parameters) noexcept;

    /**
     * @brief 读取 GFSK 通信参数。
     * @return 当前驱动未提供该 DDS 服务时返回 Unsupported。
     */
    [[nodiscard]] wrs::Result<GfskParameters, Error> read_gfsk_parameters() noexcept;

    /**
     * @brief 写入 GFSK 通信参数。
     * @param parameters 待写入参数。
     * @return 当前驱动未提供该 DDS 服务时返回 Unsupported。
     */
    [[nodiscard]] wrs::Result<void, Error> write_gfsk_parameters(
        const GfskParameters& parameters) noexcept;

private:
    class Impl;                   // DDS 节点、服务客户端和状态订阅者的实现
    std::unique_ptr<Impl> impl_;  // 本实例拥有的 DDS 实现
    std::uint16_t domain_id_{};   // 当前域 ID；断开后为 0
    std::uint32_t next_transaction_id_{1};  // 下一个非零配置事务 ID

    /**
     * @brief 生成非零配置事务 ID，并推进计数器。
     * @return 本次请求使用的事务 ID；计数器溢出后从 1 继续。
     */
    [[nodiscard]] std::uint32_t next_transaction() noexcept;
};

}  // namespace receiver
