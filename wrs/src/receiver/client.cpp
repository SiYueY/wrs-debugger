#include <receiver/client.hpp>

#include <utility>

namespace receiver {
namespace {
template <typename T>
using Result = hardware::Result<T, Error>;

class UnavailableTransport final : public IReceiverTransport {
public:
    Result<void> connect(std::uint16_t) noexcept override {
        return Result<void>::failure(Error::DdsUnavailable);
    }
    Result<void> disconnect() noexcept override { return Result<void>::success(); }
    Result<ReceiverInfo> read_info() noexcept override {
        return Result<ReceiverInfo>::failure(Error::DdsUnavailable);
    }
    Result<SdoResponse> read_sdo(const SdoRequest&) noexcept override {
        return Result<SdoResponse>::failure(Error::DdsUnavailable);
    }
    Result<SdoResponse> write_sdo(const SdoRequest&) noexcept override {
        return Result<SdoResponse>::failure(Error::DdsUnavailable);
    }
    Result<ParameterResponse<LoRaParameters>> read_lora(
        const ParameterRequest<LoRaParameters>&) noexcept override {
        return Result<ParameterResponse<LoRaParameters>>::failure(Error::DdsUnavailable);
    }
    Result<ParameterResponse<LoRaParameters>> write_lora(
        const ParameterRequest<LoRaParameters>&) noexcept override {
        return Result<ParameterResponse<LoRaParameters>>::failure(Error::DdsUnavailable);
    }
    Result<ParameterResponse<GfskParameters>> read_gfsk(
        const ParameterRequest<GfskParameters>&) noexcept override {
        return Result<ParameterResponse<GfskParameters>>::failure(Error::DdsUnavailable);
    }
    Result<ParameterResponse<GfskParameters>> write_gfsk(
        const ParameterRequest<GfskParameters>&) noexcept override {
        return Result<ParameterResponse<GfskParameters>>::failure(Error::DdsUnavailable);
    }
};

Error status_error(std::uint16_t object_index) noexcept {
    switch (static_cast<ParameterStatus>(object_index >> 12U)) {
        case ParameterStatus::NotReceived:
            return Error::NotReceived;
        case ParameterStatus::InProgress:
            return Error::InProgress;
        case ParameterStatus::Error:
            return Error::ExecutionError;
        case ParameterStatus::InvalidCommand:
            return Error::InvalidCommand;
        default:
            return Error::UnexpectedResponse;
    }
}

template <typename Parameters>
Result<Parameters> validate_read_response(
    const ParameterResponse<Parameters>& response, std::uint32_t transaction) noexcept {
    if (response.command != Command::ParamReadResponse)
        return Result<Parameters>::failure(Error::UnexpectedResponse);
    if (response.transaction_id != transaction)
        return Result<Parameters>::failure(Error::TransactionMismatch);
    if (response.result_code != 0) return Result<Parameters>::failure(Error::DeviceRejected);
    if (response.object_index != 0x4000U)
        return Result<Parameters>::failure(status_error(response.object_index));
    if (response.object_data != 0 || !valid(response.parameters))
        return Result<Parameters>::failure(Error::UnexpectedResponse);
    return Result<Parameters>::success(response.parameters);
}

template <typename Parameters>
Result<void> validate_write_response(
    const ParameterResponse<Parameters>& response, std::uint32_t transaction) noexcept {
    if (response.command != Command::ParamWriteResponse)
        return Result<void>::failure(Error::UnexpectedResponse);
    if (response.transaction_id != transaction)
        return Result<void>::failure(Error::TransactionMismatch);
    if (response.result_code != 0) return Result<void>::failure(Error::DeviceRejected);
    if (response.object_index != 0x6000U)
        return Result<void>::failure(status_error(response.object_index));
    return response.object_data == 0 && valid(response.parameters)
               ? Result<void>::success()
               : Result<void>::failure(Error::UnexpectedResponse);
}

bool retryable(Error error) noexcept {
    return error == Error::TimedOut || error == Error::NotReceived || error == Error::InProgress;
}

Result<SdoResponse> validate_sdo_response(
    const SdoResponse& response, const SdoRequest& request, ParameterStatus success) noexcept {
    const auto expected_command = request.command == Command::ParamReadRequest
                                      ? Command::ParamReadResponse
                                      : Command::ParamWriteResponse;
    if (response.command != expected_command)
        return Result<SdoResponse>::failure(Error::UnexpectedResponse);
    if (response.transaction_id != request.transaction_id)
        return Result<SdoResponse>::failure(Error::TransactionMismatch);
    if (response.result_code != 0) return Result<SdoResponse>::failure(Error::DeviceRejected);
    const auto address = static_cast<std::uint16_t>(response.object_index & 0x0fffU);
    if (address != (request.object_index & 0x0fffU))
        return Result<SdoResponse>::failure(Error::UnexpectedResponse);
    if (static_cast<ParameterStatus>(response.object_index >> 12U) != success)
        return Result<SdoResponse>::failure(status_error(response.object_index));
    if (success == ParameterStatus::WriteSuccess && response.object_data != 0)
        return Result<SdoResponse>::failure(Error::UnexpectedResponse);
    return Result<SdoResponse>::success(response);
}
}  // namespace

Client::Client() : Client(std::make_unique<UnavailableTransport>()) {}
Client::Client(std::unique_ptr<IReceiverTransport> transport) noexcept
: transport_(std::move(transport)) {}
Client::~Client() noexcept {
    if (connected_) (void)disconnect();
}
Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

Result<void> Client::connect(std::uint16_t domain_id) noexcept {
    if (!transport_ || domain_id > 232) return Result<void>::failure(Error::InvalidArgument);
    if (connected_) return Result<void>::failure(Error::AlreadyConnected);
    auto result = transport_->connect(domain_id);
    if (!result) return Result<void>::failure(result.error());
    connected_ = true;
    domain_id_ = domain_id;
    return Result<void>::success();
}

Result<void> Client::disconnect() noexcept {
    if (!connected_) return Result<void>::success();
    auto result = transport_->disconnect();
    connected_ = false;
    domain_id_ = 0;
    return result ? Result<void>::success() : Result<void>::failure(result.error());
}

bool Client::is_connected() const noexcept { return connected_; }
std::uint16_t Client::domain_id() const noexcept { return domain_id_; }

std::uint32_t Client::next_transaction() noexcept {
    const auto value = next_transaction_id_++;
    if (next_transaction_id_ == 0) next_transaction_id_ = 1;
    return value == 0 ? next_transaction() : value;
}

Result<ReceiverInfo> Client::read_info() noexcept {
    if (!connected_) return Result<ReceiverInfo>::failure(Error::NotConnected);
    return transport_->read_info();
}

Result<SdoResponse> Client::read_sdo(std::uint16_t address) noexcept {
    if (!connected_) return Result<SdoResponse>::failure(Error::NotConnected);
    if (address == 0 || address > 0x0fffU)
        return Result<SdoResponse>::failure(Error::InvalidArgument);
    SdoRequest request{};
    request.transaction_id = next_transaction();
    request.object_index = address;
    for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
        auto response = transport_->read_sdo(request);
        if (!response) {
            if (attempt < 2 && retryable(response.error())) continue;
            return Result<SdoResponse>::failure(response.error());
        }
        auto validated =
            validate_sdo_response(response.value(), request, ParameterStatus::ReadSuccess);
        if (!validated && attempt < 2 && retryable(validated.error())) continue;
        return validated;
    }
    return Result<SdoResponse>::failure(Error::TimedOut);
}

Result<SdoResponse> Client::write_sdo(std::uint16_t address, std::uint32_t data) noexcept {
    if (!connected_) return Result<SdoResponse>::failure(Error::NotConnected);
    if (address == 0 || address > 0x0fffU)
        return Result<SdoResponse>::failure(Error::InvalidArgument);
    SdoRequest request{};
    request.command = Command::ParamWriteRequest;
    request.transaction_id = next_transaction();
    request.object_index = static_cast<std::uint16_t>(0x1000U | address);
    request.object_data = data;
    for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
        auto response = transport_->write_sdo(request);
        if (!response) {
            if (attempt < 2 && retryable(response.error())) continue;
            return Result<SdoResponse>::failure(response.error());
        }
        auto validated =
            validate_sdo_response(response.value(), request, ParameterStatus::WriteSuccess);
        if (!validated && attempt < 2 && retryable(validated.error())) continue;
        return validated;
    }
    return Result<SdoResponse>::failure(Error::TimedOut);
}

Result<LoRaParameters> Client::read_lora_parameters() noexcept {
    if (!connected_) return Result<LoRaParameters>::failure(Error::NotConnected);
    ParameterRequest<LoRaParameters> request{};
    request.transaction_id = next_transaction();
    request.parameters.param_flags = 0x0000;
    for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
        auto response = transport_->read_lora(request);
        if (!response) {
            if (attempt < 2 && retryable(response.error())) continue;
            return Result<LoRaParameters>::failure(response.error());
        }
        auto validated = validate_read_response(response.value(), request.transaction_id);
        if (!validated && attempt < 2 && retryable(validated.error())) continue;
        return validated;
    }
    return Result<LoRaParameters>::failure(Error::TimedOut);
}

Result<void> Client::write_lora_parameters(const LoRaParameters& parameters) noexcept {
    if (!connected_) return Result<void>::failure(Error::NotConnected);
    if (!valid(parameters)) return Result<void>::failure(Error::InvalidArgument);
    ParameterRequest<LoRaParameters> request{};
    request.command = Command::ParamWriteRequest;
    request.transaction_id = next_transaction();
    request.object_index = 0x1000;
    request.parameters = parameters;
    for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
        auto response = transport_->write_lora(request);
        if (!response) {
            if (attempt < 2 && retryable(response.error())) continue;
            return Result<void>::failure(response.error());
        }
        auto validated = validate_write_response(response.value(), request.transaction_id);
        if (!validated && attempt < 2 && retryable(validated.error())) continue;
        return validated;
    }
    return Result<void>::failure(Error::TimedOut);
}

Result<GfskParameters> Client::read_gfsk_parameters() noexcept {
    if (!connected_) return Result<GfskParameters>::failure(Error::NotConnected);
    ParameterRequest<GfskParameters> request{};
    request.transaction_id = next_transaction();
    request.parameters.param_flags = 0x4000;
    for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
        auto response = transport_->read_gfsk(request);
        if (!response) {
            if (attempt < 2 && retryable(response.error())) continue;
            return Result<GfskParameters>::failure(response.error());
        }
        auto validated = validate_read_response(response.value(), request.transaction_id);
        if (!validated && attempt < 2 && retryable(validated.error())) continue;
        return validated;
    }
    return Result<GfskParameters>::failure(Error::TimedOut);
}

Result<void> Client::write_gfsk_parameters(const GfskParameters& parameters) noexcept {
    if (!connected_) return Result<void>::failure(Error::NotConnected);
    if (!valid(parameters)) return Result<void>::failure(Error::InvalidArgument);
    ParameterRequest<GfskParameters> request{};
    request.command = Command::ParamWriteRequest;
    request.transaction_id = next_transaction();
    request.object_index = 0x1000;
    request.parameters = parameters;
    for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
        auto response = transport_->write_gfsk(request);
        if (!response) {
            if (attempt < 2 && retryable(response.error())) continue;
            return Result<void>::failure(response.error());
        }
        auto validated = validate_write_response(response.value(), request.transaction_id);
        if (!validated && attempt < 2 && retryable(validated.error())) continue;
        return validated;
    }
    return Result<void>::failure(Error::TimedOut);
}

Result<void> Client::restore_lora() noexcept {
    return write_lora_parameters(default_lora_parameters());
}
Result<void> Client::restore_gfsk() noexcept {
    return write_gfsk_parameters(default_gfsk_parameters());
}

}  // namespace receiver
