#include <receiver/driver_proxy.hpp>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <string_view>

#include <wrs/logging.hpp>

#include <ddswrapper/context.hpp>
#include <ddswrapper/node.hpp>

#include "GetEStopBindingState_.h"
#include "GetEStopBindingState_PubSubTypes.h"
#include "GetEStopConfig_.h"
#include "GetEStopConfig_PubSubTypes.h"
#include "SetEStopConfig_.h"
#include "SetEStopConfig_PubSubTypes.h"
#include "StartEStopBinding_.h"
#include "StartEStopBinding_PubSubTypes.h"
#include "WirelessEStopState_.h"
#include "WirelessEStopState_PubSubTypes.h"

namespace receiver {
template <typename T>
using Result = wrs::Result<T, Error>;
namespace dds = wireless_estop::srv::dds_;
namespace msg = wireless_estop::msg::dds_;
using BindingService = DDSWRAPPER_SERVICE(dds, GetEStopBindingState);
using StartBindingService = DDSWRAPPER_SERVICE(dds, StartEStopBinding);
using GetConfigService = DDSWRAPPER_SERVICE(dds, GetEStopConfig);
using SetConfigService = DDSWRAPPER_SERVICE(dds, SetEStopConfig);

Error map_dds_request_error(const std::exception& error) noexcept {
    const std::string_view message(error.what());
    return message.find("timed out") != std::string_view::npos ? Error::TimedOut
                                                               : Error::DdsUnavailable;
}

class DriverProxy::Impl final {
public:
    ~Impl() noexcept { (void)disconnect(); }

    Result<void> connect(std::uint16_t domain_id) noexcept {
        WRS_LOG_INFO("receiver.dds") << "Connect request\n  domain_id: " << domain_id;
        if (node_) return Result<void>::failure(Error::AlreadyConnected);
        const char* profile = std::getenv("WRS_DDS_PROFILE");
        if (!profile || !*profile) return Result<void>::failure(Error::InvalidArgument);
        std::error_code file_error;
        if (!std::filesystem::is_regular_file(profile, file_error) || file_error)
            return Result<void>::failure(Error::InvalidArgument);
        if (ddswrapper::ok()) return Result<void>::failure(Error::AlreadyInitialized);
        if (!ddswrapper::init(profile)) return Result<void>::failure(Error::InitializationFailed);
        owns_runtime_ = true;
        try {
            node_ = std::make_unique<ddswrapper::Node>(domain_id);
            binding_ = node_->create_client<BindingService>("GetEStopBindingState");
            start_binding_ = node_->create_client<StartBindingService>("StartEStopBinding");
            get_config_ = node_->create_client<GetConfigService>("GetEStopConfig");
            set_config_ = node_->create_client<SetConfigService>("SetEStopConfig");
            state_ = node_->create_subscriber<
                msg::WirelessEStopState_, msg::WirelessEStopState_PubSubType>(
                "rt/wireless_estop_state");
            if (!binding_ || !start_binding_ || !get_config_ || !set_config_ || !state_) {
                (void)disconnect();
                return Result<void>::failure(Error::DdsUnavailable);
            }
            WRS_LOG_INFO("receiver.dds") << "Connect succeeded\n  domain_id: " << domain_id;
            return Result<void>::success();
        } catch (...) {
            (void)disconnect();
            return Result<void>::failure(Error::DdsUnavailable);
        }
    }

    bool is_connected() const noexcept { return node_ != nullptr; }

    Result<void> disconnect() noexcept {
        WRS_LOG_INFO("receiver.dds") << "Disconnect request";
        state_.reset();
        set_config_.reset();
        get_config_.reset();
        start_binding_.reset();
        binding_.reset();
        node_.reset();
        if (owns_runtime_) {
            ddswrapper::shutdown();
            owns_runtime_ = false;
        }
        return Result<void>::success();
    }

    Result<ReceiverState> read_wireless_estop_state() noexcept {
        if (!state_) return Result<ReceiverState>::failure(Error::NotConnected);
        msg::WirelessEStopState_ sample{};
        if (!state_->read(sample)) {
            WRS_LOG_WARNING("receiver.dds") << "WirelessEStopState not received";
            return Result<ReceiverState>::failure(Error::NotReceived);
        }
        if (!sample.is_data_valid_() || sample.device_id_() > 0xffffffU) {
            WRS_LOG_ERROR("receiver.dds") << "WirelessEStopState sample is invalid"
                                          << "\n  data_valid: " << sample.is_data_valid_()
                                          << "\n  device_id: " << sample.device_id_();
            return Result<ReceiverState>::failure(Error::UnexpectedResponse);
        }
        WRS_LOG_DEBUG("receiver.dds")
            << "WirelessEStopState sample"
            << "\n  device_id: " << sample.device_id_() << "\n  state: " << sample.state_()
            << "\n  rssi: " << sample.rssi_() << "\n  snr: " << sample.snr_()
            << "\n  error_code: " << sample.error_code_()
            << "\n  warning_code: " << sample.warning_code_() << "\n  tick: " << sample.tick_()
            << "\n  crc: " << sample.crc_();
        return Result<ReceiverState>::success(
            {sample.device_id_(), sample.state_(), sample.rssi_(), sample.snr_(),
             sample.error_code_(), sample.warning_code_(), sample.tick_(), sample.crc_()});
    }

    Result<BindingState> binding_state() noexcept {
        if (!binding_) {
            WRS_LOG_ERROR("receiver.dds") << "GetEStopBindingState client is not connected.";
            return Result<BindingState>::failure(Error::NotConnected);
        }
        try {
            WRS_LOG_INFO("receiver.dds") << "GetEStopBindingState request";
            auto response = binding_->send_request(
                std::make_shared<BindingService::Request>(), std::chrono::seconds(5));
            if (!response) {
                WRS_LOG_ERROR("receiver.dds") << "GetEStopBindingState returned no response.";
                return Result<BindingState>::failure(Error::DeviceRejected);
            }
            WRS_LOG_INFO("receiver.dds")
                << "GetEStopBindingState response\n"
                << "  result: " << response->result_() << '\n'
                << "  binding_state: " << response->binding_state_() << '\n'
                << "  device_id: " << response->device_id_() << '\n'
                << "  binding_transaction_id: " << response->binding_transaction_id_() << '\n';
            if (response->result_() != 0)
                return Result<BindingState>::failure(Error::DeviceRejected);
            const auto device_id = response->device_id_();
            if (device_id > 0xffffffU ||
                (response->binding_state_() != 0 && response->binding_state_() != 1)) {
                WRS_LOG_ERROR("receiver.dds") << "GetEStopBindingState returned invalid fields.";
                return Result<BindingState>::failure(Error::UnexpectedResponse);
            }
            // 当前驱动未填写 binding_transaction_id_，绑定状态只使用设备 ID。
            return Result<BindingState>::success({response->binding_state_() == 1, device_id});
        } catch (const std::exception& error) {
            WRS_LOG_ERROR("receiver.dds")
                << "GetEStopBindingState request failed: " << error.what() << '\n';
            return Result<BindingState>::failure(map_dds_request_error(error));
        } catch (...) {
            WRS_LOG_ERROR("receiver.dds")
                << "GetEStopBindingState request failed with an unknown exception.";
            return Result<BindingState>::failure(Error::DdsUnavailable);
        }
    }

    Result<ReceiverInfo> read_info() noexcept {
        auto state = binding_state();
        if (!state) return Result<ReceiverInfo>::failure(state.error());
        const auto id = state.value().bound ? state.value().device_id : 0U;
        return Result<ReceiverInfo>::success(
            {{static_cast<std::uint8_t>(id >> 16), static_cast<std::uint8_t>(id >> 8),
              static_cast<std::uint8_t>(id)}});
    }

    Result<void> start_binding(
        std::uint32_t device_id, const std::array<std::uint8_t, 16>& kbind,
        std::uint32_t transaction_id) noexcept {
        if (!start_binding_) {
            WRS_LOG_ERROR("receiver.dds") << "StartEStopBinding client is not connected.";
            return Result<void>::failure(Error::NotConnected);
        }
        try {
            WRS_LOG_INFO("receiver.dds") << "StartEStopBinding request\n"
                                         << "  device_id: " << device_id << '\n'
                                         << "  transaction_id: " << transaction_id << '\n';
            auto request = std::make_shared<StartBindingService::Request>();
            request->device_id_(device_id);
            request->kbind_(kbind);
            request->transaction_id_(transaction_id);
            auto response = start_binding_->send_request(request, std::chrono::seconds(5));
            if (!response) {
                WRS_LOG_ERROR("receiver.dds") << "StartEStopBinding returned no response.";
                return Result<void>::failure(Error::DeviceRejected);
            }
            WRS_LOG_INFO("receiver.dds") << "StartEStopBinding response"
                                         << "\n  result: " << response->result_()
                                         << "\n  transaction_id: " << transaction_id;
            return response->result_() == 0 ? Result<void>::success()
                                            : Result<void>::failure(Error::DeviceRejected);
        } catch (const std::exception& error) {
            WRS_LOG_ERROR("receiver.dds") << "StartEStopBinding request failed: " << error.what()
                                          << "\n  device_id: " << device_id << '\n'
                                          << "  transaction_id: " << transaction_id << '\n';
            return Result<void>::failure(map_dds_request_error(error));
        } catch (...) {
            WRS_LOG_ERROR("receiver.dds")
                << "StartEStopBinding request failed with an unknown exception\n"
                << "  device_id: " << device_id << '\n'
                << "  transaction_id: " << transaction_id << '\n';
            return Result<void>::failure(Error::DdsUnavailable);
        }
    }

    Result<LoRaParameters> read_lora_parameters() noexcept {
        if (!get_config_) {
            WRS_LOG_ERROR("receiver.dds") << "GetEStopConfig client is not connected.";
            return Result<LoRaParameters>::failure(Error::NotConnected);
        }
        try {
            WRS_LOG_INFO("receiver.dds") << "GetEStopConfig request";
            auto response = get_config_->send_request(
                std::make_shared<GetConfigService::Request>(), std::chrono::seconds(5));
            if (!response) {
                WRS_LOG_ERROR("receiver.dds") << "GetEStopConfig returned no response.";
                return Result<LoRaParameters>::failure(Error::DeviceRejected);
            }
            if (response->result_() != 0) {
                WRS_LOG_ERROR("receiver.dds")
                    << "GetEStopConfig was rejected\n"
                    << "  result: " << response->result_() << '\n'
                    << "  operation_type: " << response->operation_type_() << '\n';
                return Result<LoRaParameters>::failure(Error::DeviceRejected);
            }
            if (response->operation_type_() != 0) {
                WRS_LOG_ERROR("receiver.dds")
                    << "GetEStopConfig returned an unsupported operation type\n"
                    << "  operation_type: " << response->operation_type_() << '\n';
                return Result<LoRaParameters>::failure(Error::Unsupported);
            }
            ParamFlags flags{};
            flags.wireless_estop = response->estop_enabled_()
                                       ? ParamFlags::WirelessEstopSwitch::Enabled
                                       : ParamFlags::WirelessEstopSwitch::Disabled;
            flags.heartbeat = response->heartbeat_enable_() ? ParamFlags::HeartbeatSwitch::Enabled
                                                            : ParamFlags::HeartbeatSwitch::Disabled;
            flags.group_mode = response->group_mode_enable_() ? ParamFlags::GroupMode::OneToMany
                                                              : ParamFlags::GroupMode::OneToOne;
            flags.channel_scan_mode = response->channel_hopping_enabled_()
                                          ? ParamFlags::ChannelScanMode::Hopping
                                          : ParamFlags::ChannelScanMode::Single;
            flags.physical_crc = response->crc_enable_() ? ParamFlags::PhysicalCrcSwitch::Enabled
                                                         : ParamFlags::PhysicalCrcSwitch::Disabled;
            flags.band =
                response->frequency_band_() ? ParamFlags::Band::MHz915 : ParamFlags::Band::MHz433;
            LoRaParameters parameters{};
            parameters.param_flags = flags.to_flags();
            parameters.tx_power = response->transmit_power_();
            parameters.freq_offset = response->frequency_offset_();
            parameters.payload_len = response->byte_length_();
            parameters.rssi_threshold = response->intensity_threshold_();
            parameters.heartbeat_interval = response->heartbeat_interval_();
            parameters.heartbeat_loss = response->heartbeat_loss_threshold_();
            parameters.bandwidth = response->receive_bandwidth_();
            parameters.spreading_factor = response->spreading_factor_();
            parameters.coding_rate = response->encoding_rate_();
            parameters.header_type = response->header_type_();
            parameters.preamble_len = response->preamble_size_();
            parameters.sync_word = response->synchronous_word_();
            if (!valid(parameters)) {
                WRS_LOG_ERROR("receiver.dds")
                    << "GetEStopConfig returned invalid LoRa parameters"
                    << "\n  param_flags: " << parameters.param_flags
                    << "\n  tx_power: " << parameters.tx_power
                    << "\n  freq_offset: " << parameters.freq_offset
                    << "\n  payload_len: " << static_cast<unsigned int>(parameters.payload_len)
                    << "\n  rssi_threshold: "
                    << static_cast<unsigned int>(parameters.rssi_threshold)
                    << "\n  heartbeat_interval: " << parameters.heartbeat_interval
                    << "\n  heartbeat_loss: "
                    << static_cast<unsigned int>(parameters.heartbeat_loss)
                    << "\n  bandwidth: " << static_cast<unsigned int>(parameters.bandwidth)
                    << "\n  spreading_factor: "
                    << static_cast<unsigned int>(parameters.spreading_factor)
                    << "\n  coding_rate: " << static_cast<unsigned int>(parameters.coding_rate)
                    << "\n  header_type: " << static_cast<unsigned int>(parameters.header_type)
                    << "\n  preamble_len: " << static_cast<unsigned int>(parameters.preamble_len)
                    << "\n  sync_word: " << parameters.sync_word;
                return Result<LoRaParameters>::failure(Error::UnexpectedResponse);
            }
            WRS_LOG_INFO("receiver.dds")
                << "GetEStopConfig response"
                << "\n  param_flags: " << parameters.param_flags
                << "\n  tx_power: " << parameters.tx_power
                << "\n  freq_offset: " << parameters.freq_offset
                << "\n  payload_len: " << static_cast<unsigned int>(parameters.payload_len)
                << "\n  rssi_threshold: " << static_cast<unsigned int>(parameters.rssi_threshold)
                << "\n  heartbeat_interval: " << parameters.heartbeat_interval
                << "\n  heartbeat_loss: " << static_cast<unsigned int>(parameters.heartbeat_loss)
                << "\n  bandwidth: " << static_cast<unsigned int>(parameters.bandwidth)
                << "\n  spreading_factor: "
                << static_cast<unsigned int>(parameters.spreading_factor)
                << "\n  coding_rate: " << static_cast<unsigned int>(parameters.coding_rate)
                << "\n  header_type: " << static_cast<unsigned int>(parameters.header_type)
                << "\n  preamble_len: " << static_cast<unsigned int>(parameters.preamble_len)
                << "\n  sync_word: " << parameters.sync_word;
            return Result<LoRaParameters>::success(parameters);
        } catch (const std::exception& error) {
            WRS_LOG_ERROR("receiver.dds")
                << "GetEStopConfig request failed: " << error.what() << '\n';
            return Result<LoRaParameters>::failure(map_dds_request_error(error));
        } catch (...) {
            WRS_LOG_ERROR("receiver.dds")
                << "GetEStopConfig request failed with an unknown exception.";
            return Result<LoRaParameters>::failure(Error::DdsUnavailable);
        }
    }

    Result<void> write_lora_parameters(
        const LoRaParameters& parameters, std::uint32_t transaction_id) noexcept {
        if (!set_config_) {
            WRS_LOG_ERROR("receiver.dds") << "SetEStopConfig client is not connected.";
            return Result<void>::failure(Error::NotConnected);
        }
        if (!ParamFlags::valid_flags(parameters.param_flags))
            return Result<void>::failure(Error::InvalidArgument);
        const auto flags = ParamFlags::from_flags(parameters.param_flags);
        if (flags.modulation != ParamFlags::Modulation::LoRa)
            return Result<void>::failure(Error::InvalidArgument);
        try {
            WRS_LOG_INFO("receiver.dds")
                << "SetEStopConfig request"
                << "\n  param_flags: " << parameters.param_flags
                << "\n  tx_power: " << parameters.tx_power
                << "\n  freq_offset: " << parameters.freq_offset
                << "\n  payload_len: " << static_cast<unsigned int>(parameters.payload_len)
                << "\n  rssi_threshold: " << static_cast<unsigned int>(parameters.rssi_threshold)
                << "\n  heartbeat_interval: " << parameters.heartbeat_interval
                << "\n  heartbeat_loss: " << static_cast<unsigned int>(parameters.heartbeat_loss)
                << "\n  bandwidth: " << static_cast<unsigned int>(parameters.bandwidth)
                << "\n  spreading_factor: "
                << static_cast<unsigned int>(parameters.spreading_factor)
                << "\n  coding_rate: " << static_cast<unsigned int>(parameters.coding_rate)
                << "\n  header_type: " << static_cast<unsigned int>(parameters.header_type)
                << "\n  preamble_len: " << static_cast<unsigned int>(parameters.preamble_len)
                << "\n  sync_word: " << parameters.sync_word
                << "\n  transaction_id: " << transaction_id;
            auto value = std::make_shared<SetConfigService::Request>();

            value->estop_enabled_(flags.wireless_estop == ParamFlags::WirelessEstopSwitch::Enabled);
            value->heartbeat_enable_(flags.heartbeat == ParamFlags::HeartbeatSwitch::Enabled);
            value->group_mode_enable_(flags.group_mode == ParamFlags::GroupMode::OneToMany);
            value->channel_hopping_enabled_(
                flags.channel_scan_mode == ParamFlags::ChannelScanMode::Hopping);
            value->crc_enable_(flags.physical_crc == ParamFlags::PhysicalCrcSwitch::Enabled);
            value->frequency_band_(flags.band == ParamFlags::Band::MHz915);
            value->intensity_threshold_(parameters.rssi_threshold);
            value->byte_length_(parameters.payload_len);
            value->heartbeat_loss_threshold_(parameters.heartbeat_loss);
            value->receive_bandwidth_(parameters.bandwidth);
            value->spreading_factor_(parameters.spreading_factor);
            value->encoding_rate_(parameters.coding_rate);
            value->header_type_(parameters.header_type);
            value->preamble_size_(parameters.preamble_len);
            value->operation_type_(0);
            value->transmit_power_(parameters.tx_power);
            value->frequency_offset_(parameters.freq_offset);
            value->synchronous_word_(parameters.sync_word);
            value->heartbeat_interval_(parameters.heartbeat_interval);
            value->transaction_id_(transaction_id);
            auto response = set_config_->send_request(value, std::chrono::seconds(7));
            if (!response) {
                WRS_LOG_ERROR("receiver.dds") << "SetEStopConfig returned no response.";
                return Result<void>::failure(Error::DeviceRejected);
            }
            WRS_LOG_INFO("receiver.dds") << "SetEStopConfig response"
                                         << "\n  result: " << response->result_()
                                         << "\n  transaction_id: " << transaction_id;
            if (response->result_() != 0) {
                WRS_LOG_ERROR("receiver.dds") << "SetEStopConfig was rejected\n"
                                              << "  result: " << response->result_() << '\n'
                                              << "  transaction_id: " << transaction_id << '\n';
                return Result<void>::failure(Error::DeviceRejected);
            }
            return Result<void>::success();
        } catch (const std::exception& error) {
            WRS_LOG_ERROR("receiver.dds") << "SetEStopConfig request failed: " << error.what()
                                          << ", transaction_id=" << transaction_id << '\n';
            return Result<void>::failure(map_dds_request_error(error));
        } catch (...) {
            WRS_LOG_ERROR("receiver.dds")
                << "SetEStopConfig request failed with an unknown exception, "
                   "transaction_id="
                << transaction_id << '\n';
            return Result<void>::failure(Error::DdsUnavailable);
        }
    }

private:
    bool owns_runtime_{false};                // 仅关闭本实例初始化的 DDS 运行时
    std::unique_ptr<ddswrapper::Node> node_;  // 当前 DDS 域中的节点
    std::shared_ptr<ddswrapper::Client<BindingService>> binding_;              // 绑定状态服务
    std::shared_ptr<ddswrapper::Client<StartBindingService>> start_binding_;   // 绑定请求服务
    std::shared_ptr<ddswrapper::Client<GetConfigService>> get_config_;         // 参数读取服务
    std::shared_ptr<ddswrapper::Client<SetConfigService>> set_config_;         // 参数写入服务
    std::shared_ptr<ddswrapper::Subscriber<msg::WirelessEStopState_>> state_;  // 状态订阅者
};

DriverProxy::DriverProxy() noexcept : impl_(new Impl()) {}

DriverProxy::~DriverProxy() noexcept = default;

DriverProxy::DriverProxy(DriverProxy&&) noexcept = default;

DriverProxy& DriverProxy::operator=(DriverProxy&&) noexcept = default;

Result<void> DriverProxy::connect(std::uint16_t domain_id) noexcept {
    auto result = impl_->connect(domain_id);
    if (result) domain_id_ = domain_id;
    return result;
}

Result<void> DriverProxy::disconnect() noexcept {
    auto result = impl_->disconnect();
    domain_id_ = 0;
    return result;
}

bool DriverProxy::is_connected() const noexcept { return impl_->is_connected(); }

std::uint16_t DriverProxy::domain_id() const noexcept { return domain_id_; }

Result<ReceiverInfo> DriverProxy::read_info() noexcept { return impl_->read_info(); }

Result<ReceiverState> DriverProxy::read_wireless_estop_state() noexcept {
    return impl_->read_wireless_estop_state();
}

Result<BindingState> DriverProxy::binding_state() noexcept { return impl_->binding_state(); }

Result<void> DriverProxy::start_binding(
    std::uint32_t device_id, const std::array<std::uint8_t, 16>& kbind,
    std::uint32_t transaction_id) noexcept {
    return impl_->start_binding(device_id, kbind, transaction_id);
}

Result<SdoResponse> DriverProxy::read_sdo(std::uint16_t) noexcept {
    return Result<SdoResponse>::failure(is_connected() ? Error::Unsupported : Error::NotConnected);
}

Result<SdoResponse> DriverProxy::write_sdo(std::uint16_t, std::uint32_t) noexcept {
    return Result<SdoResponse>::failure(is_connected() ? Error::Unsupported : Error::NotConnected);
}

Result<LoRaParameters> DriverProxy::read_lora_parameters() noexcept {
    return impl_->read_lora_parameters();
}

Result<void> DriverProxy::write_lora_parameters(const LoRaParameters& parameters) noexcept {
    return impl_->write_lora_parameters(parameters, next_transaction());
}

Result<GfskParameters> DriverProxy::read_gfsk_parameters() noexcept {
    return Result<GfskParameters>::failure(
        is_connected() ? Error::Unsupported : Error::NotConnected);
}

Result<void> DriverProxy::write_gfsk_parameters(const GfskParameters&) noexcept {
    return Result<void>::failure(is_connected() ? Error::Unsupported : Error::NotConnected);
}

std::uint32_t DriverProxy::next_transaction() noexcept {
    const auto transaction_id = next_transaction_id_++;
    if (next_transaction_id_ == 0) next_transaction_id_ = 1;
    return transaction_id == 0 ? next_transaction() : transaction_id;
}

}  // namespace receiver
