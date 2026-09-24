#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <transmitter/client.hpp>

namespace py = pybind11;
using namespace py::literals;

namespace binding {
namespace {
[[noreturn]] void fail(transmitter::Error error) {
    throw std::runtime_error("TRANSMITTER_" +
                             std::to_string(static_cast<unsigned int>(error)));
}

template <typename T>
T unwrap(hardware::Result<T, transmitter::Error>&& result) {
    if (!result) fail(result.error());
    return std::move(result.value());
}

void unwrap(hardware::Result<void, transmitter::Error>&& result) {
    if (!result) fail(result.error());
}

std::string device_id_to_string(const std::array<std::uint8_t, 3>& value) {
    char text[7]{};
    std::snprintf(
        text, sizeof(text), "%02X%02X%02X", static_cast<unsigned int>(value[0]),
        static_cast<unsigned int>(value[1]), static_cast<unsigned int>(value[2]));
    return text;
}

py::dict to_transmitter_lora_param(const transmitter::LoRaParamFrame& value) {
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

transmitter::LoRaParamFrame from_transmitter_lora_param(const py::dict& dict) {
    transmitter::LoRaParamFrame value{};
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

py::dict to_transmitter_gfsk_param(const transmitter::GfskParamFrame& value) {
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

transmitter::GfskParamFrame from_transmitter_gfsk_param(const py::dict& dict) {
    transmitter::GfskParamFrame value{};
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

void bind_transmitter_lora(py::class_<transmitter::Client>& client) {
    client
        .def(
            "read_lora",
            [](transmitter::Client& value) {
                return to_transmitter_lora_param(unwrap(value.read_lora_parameters()));
            })
        .def(
            "write_lora",
            [](transmitter::Client& value, const py::dict& dict) {
                unwrap(value.write_lora_parameters(from_transmitter_lora_param(dict)));
            })
        .def(
            "restore_lora",
            [](transmitter::Client& value) {
                unwrap(value.restore_default_parameters(transmitter::RadioType::LoRa));
            });
}

void bind_transmitter_gfsk(py::class_<transmitter::Client>& client) {
    client
        .def(
            "read_gfsk",
            [](transmitter::Client& value) {
                return to_transmitter_gfsk_param(unwrap(value.read_gfsk_parameters()));
            })
        .def(
            "write_gfsk",
            [](transmitter::Client& value, const py::dict& dict) {
                unwrap(value.write_gfsk_parameters(from_transmitter_gfsk_param(dict)));
            })
        .def(
            "restore_gfsk",
            [](transmitter::Client& value) {
                unwrap(value.restore_default_parameters(transmitter::RadioType::GFSK));
            });
}

void bind_transmitter(py::module_& module) {
    auto client = py::class_<transmitter::Client>(module, "TransmitterClient")
                      .def(py::init<>())
                      .def(
                          "open",
                          [](transmitter::Client& value, const std::string& path) {
                              unwrap(value.open(
                                  path, std::chrono::milliseconds(300),
                                  std::chrono::milliseconds(0), 1));
                          })
                      .def("close", [](transmitter::Client& value) { unwrap(value.close()); })
                      .def(
                          "identity",
                          [](const transmitter::Client& value) {
                              const auto& identity = value.identity();
                              return py::dict(
                                  "product_code"_a = identity.product_code,
                                  "version_number"_a = identity.version_number,
                                  "serial_number"_a = identity.serial_number);
                          })
                      .def(
                          "read_device_id",
                          [](transmitter::Client& value) -> py::object {
                              const auto device_id = unwrap(value.read_device_id());
                              if (device_id == std::array<std::uint8_t, 3>{}) return py::none();
                              return py::cast(device_id_to_string(device_id));
                          })
                      .def(
                          "read_pin",
                          [](transmitter::Client& value) {
                              const auto pin = unwrap(value.read_pin());
                              return std::string(pin.pin.begin(), pin.pin.end());
                          })
                      .def(
                          "read_sdo",
                          [](transmitter::Client& value, std::uint16_t address) {
                              const auto frame = unwrap(value.read_sdo(address));
                              return py::dict(
                                  "object_address"_a = (frame.object_index & 0x0fffU),
                                  "object_data"_a = frame.object_data,
                                  "status"_a = (frame.object_index >> 12U),
                                  "result_code"_a = frame.result_code);
                          })
                      .def(
                          "write_sdo",
                          [](transmitter::Client& value, std::uint16_t address,
                             std::uint32_t data) {
                              transmitter::SdoFrame frame{};
                              frame.object_index = address;
                              frame.object_data = data;
                              unwrap(value.write_sdo(frame));
                          })
                      .def(
                          "write_pin",
                          [](transmitter::Client& value, const std::string& pin) {
                              if (pin.size() != 6)
                                  throw std::invalid_argument("PIN must contain six digits");
                              transmitter::PinFrame frame{};
                              std::copy(pin.begin(), pin.end(), frame.pin.begin());
                              unwrap(value.write_pin(frame));
                          });

    bind_transmitter_lora(client);
    bind_transmitter_gfsk(client);
}
}  // namespace binding
