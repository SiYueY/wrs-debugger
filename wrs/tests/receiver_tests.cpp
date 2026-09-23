#include <receiver/client.hpp>

#include <cassert>
#include <memory>

namespace {
using namespace receiver;
template <typename T>
using Result = hardware::Result<T, Error>;

class FakeTransport final : public IReceiverTransport {
public:
    bool connected{};
    std::uint16_t domain{};
    Error next_error{Error::InvalidArgument};
    bool fail_next{};
    bool wrong_transaction{};
    bool wrong_address{};
    bool wrong_command{};
    bool invalid_status{};
    std::uint8_t result_code{};
    std::uint8_t failures_remaining{};
    std::uint32_t previous_transaction{};
    bool transaction_changed_during_retry{};
    ReceiverInfo info{{0xa1, 0xb2, 0xc3}};
    LoRaParameters lora{default_lora_parameters()};
    GfskParameters gfsk{default_gfsk_parameters()};
    ParameterRequest<LoRaParameters> last_lora{};
    ParameterRequest<GfskParameters> last_gfsk{};
    SdoRequest last_sdo{};
    std::uint32_t sdo_value{0x12345678};

    Result<void> connect(std::uint16_t value) noexcept override {
        if (take_failure()) return Result<void>::failure(next_error);
        connected = true;
        domain = value;
        return Result<void>::success();
    }
    Result<void> disconnect() noexcept override {
        connected = false;
        return Result<void>::success();
    }
    Result<ReceiverInfo> read_info() noexcept override {
        if (take_failure()) return Result<ReceiverInfo>::failure(next_error);
        return Result<ReceiverInfo>::success(info);
    }
    Result<SdoResponse> read_sdo(const SdoRequest& request) noexcept override {
        observe_sdo_transaction(request);
        last_sdo = request;
        if (take_failure()) return Result<SdoResponse>::failure(next_error);
        return Result<SdoResponse>::success(sdo_response(request, false));
    }
    Result<SdoResponse> write_sdo(const SdoRequest& request) noexcept override {
        observe_sdo_transaction(request);
        last_sdo = request;
        if (take_failure()) return Result<SdoResponse>::failure(next_error);
        sdo_value = request.object_data;
        return Result<SdoResponse>::success(sdo_response(request, true));
    }
    Result<ParameterResponse<LoRaParameters>> read_lora(
        const ParameterRequest<LoRaParameters>& request) noexcept override {
        last_lora = request;
        if (take_failure()) return Result<ParameterResponse<LoRaParameters>>::failure(next_error);
        return Result<ParameterResponse<LoRaParameters>>::success(read_response(request, lora));
    }
    Result<ParameterResponse<LoRaParameters>> write_lora(
        const ParameterRequest<LoRaParameters>& request) noexcept override {
        last_lora = request;
        if (take_failure()) return Result<ParameterResponse<LoRaParameters>>::failure(next_error);
        lora = request.parameters;
        return Result<ParameterResponse<LoRaParameters>>::success(write_response(request, lora));
    }
    Result<ParameterResponse<GfskParameters>> read_gfsk(
        const ParameterRequest<GfskParameters>& request) noexcept override {
        observe_transaction(request);
        last_gfsk = request;
        if (take_failure()) return Result<ParameterResponse<GfskParameters>>::failure(next_error);
        return Result<ParameterResponse<GfskParameters>>::success(read_response(request, gfsk));
    }
    Result<ParameterResponse<GfskParameters>> write_gfsk(
        const ParameterRequest<GfskParameters>& request) noexcept override {
        last_gfsk = request;
        if (take_failure()) return Result<ParameterResponse<GfskParameters>>::failure(next_error);
        gfsk = request.parameters;
        return Result<ParameterResponse<GfskParameters>>::success(write_response(request, gfsk));
    }

private:
    bool take_failure() noexcept {
        const bool value = fail_next || failures_remaining != 0;
        fail_next = false;
        if (failures_remaining != 0) --failures_remaining;
        return value;
    }
    template <typename T>
    void observe_transaction(const ParameterRequest<T>& request) noexcept {
        if (previous_transaction != 0 && previous_transaction != request.transaction_id)
            transaction_changed_during_retry = true;
        previous_transaction = request.transaction_id;
    }
    void observe_sdo_transaction(const SdoRequest& request) noexcept {
        if (previous_transaction != 0 && previous_transaction != request.transaction_id)
            transaction_changed_during_retry = true;
        previous_transaction = request.transaction_id;
    }
    SdoResponse sdo_response(const SdoRequest& request, bool write) const noexcept {
        SdoResponse response{};
        response.command = wrong_command
                               ? (write ? Command::ParamReadResponse : Command::ParamWriteResponse)
                               : (write ? Command::ParamWriteResponse : Command::ParamReadResponse);
        response.transaction_id =
            wrong_transaction ? request.transaction_id + 1 : request.transaction_id;
        const auto address = static_cast<std::uint16_t>(request.object_index & 0x0fffU);
        response.object_index = static_cast<std::uint16_t>(
            (invalid_status ? (write ? 0xe000U : 0xb000U) : (write ? 0x6000U : 0x4000U)) |
            (wrong_address ? ((address + 1U) & 0x0fffU) : address));
        response.object_data = write ? 0 : sdo_value;
        response.result_code = result_code;
        return response;
    }
    template <typename T>
    ParameterResponse<T> read_response(
        const ParameterRequest<T>& request, const T& value) noexcept {
        ParameterResponse<T> response{};
        response.transaction_id =
            wrong_transaction ? request.transaction_id + 1 : request.transaction_id;
        response.object_index = invalid_status ? 0xb000 : 0x4000;
        response.parameters = value;
        return response;
    }
    template <typename T>
    ParameterResponse<T> write_response(
        const ParameterRequest<T>& request, const T& value) noexcept {
        auto response = read_response(request, value);
        response.command = Command::ParamWriteResponse;
        response.object_index = invalid_status ? 0xe000 : 0x6000;
        return response;
    }
};

void test_validation() {
    auto lora = default_lora_parameters();
    assert(valid(lora));
    lora.param_flags = 0x0010;
    assert(!valid(lora));
    lora = default_lora_parameters();
    lora.tx_power = 11;
    assert(!valid(lora));
    lora.param_flags = 1;
    lora.tx_power = 20;
    assert(valid(lora));
    lora.tx_power = 21;
    assert(!valid(lora));
    lora = default_lora_parameters();
    lora.sync_word = 0x1425;
    assert(!valid(lora));

    auto gfsk = default_gfsk_parameters();
    assert(valid(gfsk));
    gfsk.bitrate = 100000;
    gfsk.freq_deviation = 24999;
    assert(!valid(gfsk));
    gfsk.freq_deviation = 25000;
    assert(valid(gfsk));
}

void test_client_contract() {
    auto fake = std::make_unique<FakeTransport>();
    auto* transport = fake.get();
    Client client(std::move(fake));
    assert(
        !client.read_lora_parameters() &&
        client.read_lora_parameters().error() == Error::NotConnected);
    assert(client.connect(7));
    assert(client.is_connected() && client.domain_id() == 7 && transport->domain == 7);
    const auto info = client.read_info();
    assert(info && info.value().bound_device_id[0] == 0xa1);

    const auto lora = client.read_lora_parameters();
    assert(lora && valid(lora.value()));
    assert(transport->last_lora.command == Command::ParamReadRequest);
    assert(transport->last_lora.transaction_id != 0 && transport->last_lora.object_index == 0);
    const auto first_transaction = transport->last_lora.transaction_id;

    auto updated = lora.value();
    updated.heartbeat_interval = 500;
    assert(client.write_lora_parameters(updated));
    assert(transport->last_lora.command == Command::ParamWriteRequest);
    assert(transport->last_lora.object_index == 0x1000);
    assert(transport->last_lora.transaction_id != first_transaction);
    assert(transport->lora.heartbeat_interval == 500);

    const auto sdo = client.read_sdo(0x201);
    assert(sdo && sdo.value().object_data == 0x12345678);
    assert(transport->last_sdo.command == Command::ParamReadRequest);
    assert(transport->last_sdo.object_index == 0x0201 && transport->last_sdo.object_data == 0);
    const auto sdo_transaction = transport->last_sdo.transaction_id;
    const auto write_sdo = client.write_sdo(0x202, 0x454e);
    assert(write_sdo && write_sdo.value().object_index == 0x6202);
    assert(transport->last_sdo.command == Command::ParamWriteRequest);
    assert(transport->last_sdo.object_index == 0x1202);
    assert(transport->last_sdo.object_data == 0x454e);
    assert(transport->last_sdo.transaction_id != sdo_transaction);
    assert(transport->sdo_value == 0x454e);

    assert(client.restore_lora());
    assert(transport->lora.heartbeat_interval == 200);
    assert(client.restore_gfsk());
    assert(transport->gfsk.param_flags == 0x4000);
    assert(client.disconnect());
    assert(!client.is_connected() && !transport->connected);
}

void test_response_and_transport_errors() {
    auto fake = std::make_unique<FakeTransport>();
    auto* transport = fake.get();
    Client client(std::move(fake));
    assert(client.connect(0));

    transport->wrong_transaction = true;
    const auto wrong_transaction = client.read_lora_parameters();
    assert(!wrong_transaction && wrong_transaction.error() == Error::TransactionMismatch);
    transport->wrong_transaction = false;

    transport->wrong_address = true;
    const auto wrong_address = client.read_sdo(0x001);
    assert(!wrong_address && wrong_address.error() == Error::UnexpectedResponse);
    transport->wrong_address = false;

    transport->wrong_command = true;
    const auto wrong_command = client.read_sdo(0x001);
    assert(!wrong_command && wrong_command.error() == Error::UnexpectedResponse);
    transport->wrong_command = false;

    transport->result_code = 1;
    const auto rejected = client.read_sdo(0x001);
    assert(!rejected && rejected.error() == Error::DeviceRejected);
    transport->result_code = 0;

    transport->invalid_status = true;
    const auto in_progress = client.read_lora_parameters();
    assert(!in_progress && in_progress.error() == Error::InProgress);
    const auto invalid_command = client.write_lora_parameters(default_lora_parameters());
    assert(!invalid_command && invalid_command.error() == Error::InvalidCommand);
    transport->invalid_status = false;

    transport->invalid_status = true;
    const auto sdo_in_progress = client.read_sdo(0x201);
    assert(!sdo_in_progress && sdo_in_progress.error() == Error::InProgress);
    const auto sdo_invalid = client.write_sdo(0x202, 0x454e);
    assert(!sdo_invalid && sdo_invalid.error() == Error::InvalidCommand);
    transport->invalid_status = false;

    assert(!client.read_sdo(0) && client.read_sdo(0).error() == Error::InvalidArgument);
    assert(
        !client.write_sdo(0x1000, 0) &&
        client.write_sdo(0x1000, 0).error() == Error::InvalidArgument);

    transport->next_error = Error::TimedOut;
    transport->failures_remaining = 2;
    transport->previous_transaction = 0;
    transport->transaction_changed_during_retry = false;
    const auto recovered = client.read_gfsk_parameters();
    assert(recovered && !transport->transaction_changed_during_retry);

    transport->failures_remaining = 3;
    transport->previous_transaction = 0;
    transport->transaction_changed_during_retry = false;
    const auto timeout = client.read_gfsk_parameters();
    assert(!timeout && timeout.error() == Error::TimedOut);
    assert(!transport->transaction_changed_during_retry);

    transport->failures_remaining = 2;
    transport->previous_transaction = 0;
    transport->transaction_changed_during_retry = false;
    const auto recovered_sdo = client.read_sdo(0x001);
    assert(recovered_sdo && !transport->transaction_changed_during_retry);

    assert(client.disconnect());
    const auto disconnected_sdo = client.read_sdo(0x001);
    assert(!disconnected_sdo && disconnected_sdo.error() == Error::NotConnected);
}
}  // namespace

int main() {
    test_validation();
    test_client_contract();
    test_response_and_transport_errors();
}
