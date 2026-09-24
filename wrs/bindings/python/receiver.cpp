#include <array>
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <receiver/client.hpp>

namespace py = pybind11;
using namespace py::literals;

namespace binding {
namespace {
[[noreturn]] void fail(receiver::Error error) {
    throw std::runtime_error("RECEIVER_" +
                             std::to_string(static_cast<unsigned int>(error)));
}

template <typename T>
T unwrap(hardware::Result<T, receiver::Error>&& result) {
    if (!result) fail(result.error());
    return std::move(result.value());
}

void unwrap(hardware::Result<void, receiver::Error>&& result) {
    if (!result) fail(result.error());
}

std::string device_id_to_string(const std::array<std::uint8_t, 3>& value) {
    char text[7]{};
    std::snprintf(
        text, sizeof(text), "%02X%02X%02X", static_cast<unsigned int>(value[0]),
        static_cast<unsigned int>(value[1]), static_cast<unsigned int>(value[2]));
    return text;
}

py::dict to_receiver_lora_param(const receiver::LoRaParameters& value) {
    return py::dict(
        "param_flags"_a = value.param_flags, "tx_power"_a = value.tx_power,
        "freq_offset"_a = value.freq_offset, "payload_len"_a = value.payload_len,
        "rssi_threshold"_a = value.rssi_threshold,
        "heartbeat_interval"_a = value.heartbeat_interval,
        "heartbeat_loss"_a = value.heartbeat_loss, "bandwidth"_a = value.bandwidth,
        "spreading_factor"_a = value.spreading_factor, "coding_rate"_a = value.coding_rate,
        "header_type"_a = value.header_type, "preamble_len"_a = value.preamble_len,
        "sync_word"_a = value.sync_word);
}

receiver::LoRaParameters from_receiver_lora_param(const py::dict& dict) {
    receiver::LoRaParameters value{};
    value.param_flags = dict["param_flags"].cast<std::uint16_t>();
    value.tx_power = dict["tx_power"].cast<std::int16_t>();
    value.freq_offset = dict["freq_offset"].cast<std::uint16_t>();
    value.payload_len = dict["payload_len"].cast<std::uint8_t>();
    value.rssi_threshold = dict["rssi_threshold"].cast<std::uint8_t>();
    value.heartbeat_interval = dict["heartbeat_interval"].cast<std::uint16_t>();
    value.heartbeat_loss = dict["heartbeat_loss"].cast<std::uint8_t>();
    value.bandwidth = dict["bandwidth"].cast<std::uint8_t>();
    value.spreading_factor = dict["spreading_factor"].cast<std::uint8_t>();
    value.coding_rate = dict["coding_rate"].cast<std::uint8_t>();
    value.header_type = dict["header_type"].cast<std::uint8_t>();
    value.preamble_len = dict["preamble_len"].cast<std::uint8_t>();
    value.sync_word = dict["sync_word"].cast<std::uint16_t>();
    return value;
}

py::dict to_receiver_gfsk_param(const receiver::GfskParameters& value) {
    return py::dict(
        "param_flags"_a = value.param_flags, "tx_power"_a = value.tx_power,
        "freq_offset"_a = value.freq_offset, "payload_len"_a = value.payload_len,
        "rssi_threshold"_a = value.rssi_threshold,
        "heartbeat_interval"_a = value.heartbeat_interval,
        "heartbeat_loss"_a = value.heartbeat_loss, "bandwidth"_a = value.bandwidth,
        "bitrate"_a = value.bitrate, "freq_deviation"_a = value.freq_deviation,
        "pulse_shaping"_a = value.pulse_shaping, "preamble_len"_a = value.preamble_len,
        "sync_word"_a = value.sync_word);
}

receiver::GfskParameters from_receiver_gfsk_param(const py::dict& dict) {
    receiver::GfskParameters value{};
    value.param_flags = dict["param_flags"].cast<std::uint16_t>();
    value.tx_power = dict["tx_power"].cast<std::int16_t>();
    value.freq_offset = dict["freq_offset"].cast<std::uint16_t>();
    value.payload_len = dict["payload_len"].cast<std::uint8_t>();
    value.rssi_threshold = dict["rssi_threshold"].cast<std::uint8_t>();
    value.heartbeat_interval = dict["heartbeat_interval"].cast<std::uint16_t>();
    value.heartbeat_loss = dict["heartbeat_loss"].cast<std::uint8_t>();
    value.bandwidth = dict["bandwidth"].cast<std::uint8_t>();
    value.bitrate = dict["bitrate"].cast<std::uint32_t>();
    value.freq_deviation = dict["freq_deviation"].cast<std::uint32_t>();
    value.pulse_shaping = dict["pulse_shaping"].cast<std::uint8_t>();
    value.preamble_len = dict["preamble_len"].cast<std::uint8_t>();
    value.sync_word = dict["sync_word"].cast<std::uint16_t>();
    return value;
}
}  // namespace

void bind_receiver_lora(py::class_<receiver::Client>& client) {
    client
        .def(
            "read_lora",
            [](receiver::Client& value) {
                return to_receiver_lora_param(unwrap(value.read_lora_parameters()));
            })
        .def(
            "write_lora",
            [](receiver::Client& value, const py::dict& dict) {
                unwrap(value.write_lora_parameters(from_receiver_lora_param(dict)));
            })
        .def(
            "restore_lora",
            [](receiver::Client& value) { unwrap(value.restore_lora()); });
}

void bind_receiver_gfsk(py::class_<receiver::Client>& client) {
    client
        .def(
            "read_gfsk",
            [](receiver::Client& value) {
                return to_receiver_gfsk_param(unwrap(value.read_gfsk_parameters()));
            })
        .def(
            "write_gfsk",
            [](receiver::Client& value, const py::dict& dict) {
                unwrap(value.write_gfsk_parameters(from_receiver_gfsk_param(dict)));
            })
        .def(
            "restore_gfsk",
            [](receiver::Client& value) { unwrap(value.restore_gfsk()); });
}

void bind_receiver(py::module_& module) {
    auto client = py::class_<receiver::Client>(module, "ReceiverClient")
                      .def(py::init<>())
                      .def(
                          "connect",
                          [](receiver::Client& value, std::uint16_t domain_id) {
                              unwrap(value.connect(domain_id));
                          })
                      .def(
                          "disconnect",
                          [](receiver::Client& value) { unwrap(value.disconnect()); })
                      .def(
                          "info",
                          [](receiver::Client& value) {
                              const auto info = unwrap(value.read_info());
                              const bool bound = info.bound_device_id[0] != 0 ||
                                                  info.bound_device_id[1] != 0 ||
                                                  info.bound_device_id[2] != 0;
                              return py::dict(
                                  "bound_device_id"_a =
                                      bound ? py::cast(device_id_to_string(info.bound_device_id))
                                            : py::none());
                          })
                      .def(
                          "read_sdo",
                          [](receiver::Client& value, std::uint16_t address) {
                              const auto response = unwrap(value.read_sdo(address));
                              return py::dict(
                                  "object_address"_a = (response.object_index & 0x0fffU),
                                  "object_data"_a = response.object_data,
                                  "status"_a = (response.object_index >> 12U),
                                  "result_code"_a = response.result_code);
                          })
                      .def(
                          "write_sdo",
                          [](receiver::Client& value, std::uint16_t address,
                             std::uint32_t data) {
                              const auto response = unwrap(value.write_sdo(address, data));
                              return py::dict(
                                  "object_address"_a = (response.object_index & 0x0fffU),
                                  "object_data"_a = response.object_data,
                                  "status"_a = (response.object_index >> 12U),
                                  "result_code"_a = response.result_code);
                          });

    bind_receiver_lora(client);
    bind_receiver_gfsk(client);
}
}  // namespace binding
