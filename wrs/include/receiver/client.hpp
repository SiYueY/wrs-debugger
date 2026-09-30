#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <wrs/result.hpp>
#include <receiver/error.hpp>
#include <receiver/protocol.hpp>

namespace receiver {

class DriverProxy;

/**
 * @brief 接收端客户端。
 *
 * 通过机器人驱动的 WirelessEStop DDS 接口读取状态和执行绑定操作。
 */
class Client final {
public:
    Client() noexcept;
    ~Client() noexcept;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    /**
     * @brief 连接指定 DDS 域中的接收端驱动。
     * @param domain_id DDS 域 ID，范围 0~232。
     * @return 连接成功时返回成功，否则返回错误码。
     * @note 连接需要 WRS_DDS_PROFILE 指向有效的 DDS 配置文件。
     */
    [[nodiscard]] wrs::Result<void, Error> connect(std::uint16_t domain_id) noexcept;

    /**
     * @brief 断开 DDS 连接。
     * @return 断开成功时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> disconnect() noexcept;

    /**
     * @brief 判断 DDS 连接是否已建立。
     * @return true=已连接，false=未连接。
     */
    [[nodiscard]] bool is_connected() const noexcept;

    /**
     * @brief 获取当前 DDS 域 ID。
     * @return 已连接的域 ID；断开后为 0。
     */
    [[nodiscard]] std::uint16_t domain_id() const noexcept;

    /**
     * @brief 读取已绑定设备信息。
     * @return 设备信息；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<ReceiverInfo, Error> read_info() noexcept;

    /**
     * @brief 读取最新的无线急停状态 Topic 样本。
     * @return 最新状态；未收到样本或读取失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<ReceiverState, Error> read_wireless_estop_state() noexcept;

    /**
     * @brief 查询当前绑定状态。
     * @return 绑定状态；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<BindingState, Error> binding_state() noexcept;

    /**
     * @brief 请求开始绑定无线急停盒。
     * @param device_id 24 位非零设备 ID。
     * @param kbind 16 字节绑定密钥。
     * @param transaction_id 非零绑定事务 ID。
     * @return 驱动接受请求时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> start_binding(
        std::uint32_t device_id, const std::array<std::uint8_t, 16>& kbind,
        std::uint32_t transaction_id) noexcept;

    /**
     * @brief 读取 SDO 对象。
     * @param address 1~0x0FFF 范围内的对象地址。
     * @return SDO 响应；当前驱动未提供该 DDS 服务时返回 Unsupported。
     */
    [[nodiscard]] wrs::Result<SdoResponse, Error> read_sdo(std::uint16_t address) noexcept;

    /**
     * @brief 写入 SDO 对象。
     * @param address 1~0x0FFF 范围内的对象地址。
     * @param data 待写入的数据。
     * @return SDO 响应；当前驱动未提供该 DDS 服务时返回 Unsupported。
     */
    [[nodiscard]] wrs::Result<SdoResponse, Error> write_sdo(
        std::uint16_t address, std::uint32_t data) noexcept;

    /**
     * @brief 读取 LoRa 通信参数。
     * @return 参数；失败时返回错误码。
     */
    [[nodiscard]] wrs::Result<LoRaParameters, Error> read_lora_parameters() noexcept;

    /**
     * @brief 校验并写入 LoRa 通信参数。
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
     * @brief 校验并写入 GFSK 通信参数。
     * @param parameters 待写入参数。
     * @return 当前驱动未提供该 DDS 服务时返回 Unsupported。
     */
    [[nodiscard]] wrs::Result<void, Error> write_gfsk_parameters(
        const GfskParameters& parameters) noexcept;

    /**
     * @brief 写入默认 LoRa 参数，并尝试读回校验。
     * @return 写入和读回均成功时返回成功，否则返回错误码。
     */
    [[nodiscard]] wrs::Result<void, Error> restore_lora() noexcept;

    /**
     * @brief 恢复默认 GFSK 参数。
     * @return 当前驱动未提供该 DDS 服务时返回 Unsupported。
     */
    [[nodiscard]] wrs::Result<void, Error> restore_gfsk() noexcept;

private:
    std::unique_ptr<DriverProxy> driver_;  // DDS 驱动代理，管理连接及请求
};

}  // namespace receiver
