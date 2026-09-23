#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <serial/tool.hpp>
#include <transmitter/client.hpp>

namespace py = pybind11;
using namespace py::literals;

namespace {
[[noreturn]] void fail(transmitter::Error error) {
    throw std::runtime_error("TRANSMITTER_" + std::to_string(static_cast<unsigned int>(error)));
}
template <typename T>
T unwrap(hardware::Result<T, transmitter::Error>&& result) {
    if (!result) fail(result.error());
    return std::move(result.value());
}
void unwrap(hardware::Result<void, transmitter::Error>&& result) {
    if (!result) fail(result.error());
}
std::string device_id_out(const std::array<std::uint8_t, 3>& value) {
    char text[7]{};
    std::snprintf(
        text, sizeof(text), "%02X%02X%02X", static_cast<unsigned int>(value[0]),
        static_cast<unsigned int>(value[1]), static_cast<unsigned int>(value[2]));
    return text;
}
py::dict lora_out(const transmitter::LoRaParamFrame& value) {
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
transmitter::LoRaParamFrame lora_in(const py::dict& d) {
    transmitter::LoRaParamFrame v{};
    v.param_flags = d["param_flags"].cast<std::uint16_t>();
    v.tx_power = d["tx_power"].cast<std::int16_t>();
    v.freq_offset = d["freq_offset"].cast<std::uint16_t>();
    v.payload_len = d["payload_len"].cast<std::uint8_t>();
    v.rssi_threshold = d["rssi_threshold"].cast<std::uint8_t>();
    v.heartbeat_interval = d["heartbeat_interval"].cast<std::uint16_t>();
    v.heartbeat_loss = d["heartbeat_loss"].cast<std::uint8_t>();
    v.bandwidth = d["bandwidth"].cast<std::uint8_t>();
    v.spreading_factor = d["spreading_factor"].cast<std::uint8_t>();
    v.coding_rate = d["coding_rate"].cast<std::uint8_t>();
    v.header_type = d["header_type"].cast<std::uint8_t>();
    v.preamble_len = d["preamble_len"].cast<std::uint8_t>();
    v.sync_word = d["sync_word"].cast<std::uint16_t>();
    return v;
}
py::dict gfsk_out(const transmitter::GfskParamFrame& value) {
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
transmitter::GfskParamFrame gfsk_in(const py::dict& d) {
    transmitter::GfskParamFrame v{};
    v.param_flags = d["param_flags"].cast<std::uint16_t>();
    v.tx_power = d["tx_power"].cast<std::int16_t>();
    v.freq_offset = d["freq_offset"].cast<std::uint16_t>();
    v.payload_len = d["payload_len"].cast<std::uint8_t>();
    v.rssi_threshold = d["rssi_threshold"].cast<std::uint8_t>();
    v.heartbeat_interval = d["heartbeat_interval"].cast<std::uint16_t>();
    v.heartbeat_loss = d["heartbeat_loss"].cast<std::uint8_t>();
    v.bandwidth = d["bandwidth"].cast<std::uint8_t>();
    v.bitrate = d["bitrate"].cast<std::uint32_t>();
    v.freq_deviation = d["freq_deviation"].cast<std::uint32_t>();
    v.pulse_shaping = d["pulse_shaping"].cast<std::uint8_t>();
    v.preamble_len = d["preamble_len"].cast<std::uint8_t>();
    v.sync_word = d["sync_word"].cast<std::uint16_t>();
    return v;
}
}  // namespace

PYBIND11_MODULE(wrs_debugger_native, m) {
    m.def("list_serial_ports", [] {
        const auto result = serial::list_ports();
        if (!result) throw std::runtime_error("SERIAL_LIST_FAILED");
        py::list ports;
        for (const auto& p : result.value())
            ports.append(py::dict(
                "device"_a = p.path, "description"_a = p.description,
                "manufacturer"_a = (p.usb.available ? p.usb.manufacturer : ""),
                "product"_a = (p.usb.available ? p.usb.product : ""),
                "serial_number"_a = (p.usb.available ? p.usb.serial_number : ""),
                "vendor_id"_a = (p.usb.available ? py::cast(p.usb.vendor_id) : py::none()),
                "product_id"_a = (p.usb.available ? py::cast(p.usb.product_id) : py::none())));
        return ports;
    });
    py::class_<transmitter::Client>(m, "TransmitterClient")
        .def(py::init<>())
        .def(
            "open",
            [](transmitter::Client& c, const std::string& path) {
                unwrap(
                    c.open(path, std::chrono::milliseconds(300), std::chrono::milliseconds(0), 1));
            })
        .def("close", [](transmitter::Client& c) { unwrap(c.close()); })
        .def(
            "identity",
            [](const transmitter::Client& c) {
                const auto& i = c.identity();
                return py::dict(
                    "product_code"_a = i.product_code, "version_number"_a = i.version_number,
                    "serial_number"_a = i.serial_number);
            })
        .def(
            "read_device_id",
            [](transmitter::Client& c) { return device_id_out(unwrap(c.read_device_id())); })
        .def(
            "read_pin",
            [](transmitter::Client& c) {
                const auto p = unwrap(c.read_pin());
                return std::string(p.pin.begin(), p.pin.end());
            })
        .def(
            "read_sdo",
            [](transmitter::Client& c, std::uint16_t address) {
                const auto f = unwrap(c.read_sdo(address));
                return py::dict(
                    "object_address"_a = (f.object_index & 0x0fffU),
                    "object_data"_a = f.object_data, "status"_a = (f.object_index >> 12U),
                    "result_code"_a = f.result_code);
            })
        .def(
            "write_sdo",
            [](transmitter::Client& c, std::uint16_t address, std::uint32_t data) {
                transmitter::SdoFrame f{};
                f.object_index = address;
                f.object_data = data;
                unwrap(c.write_sdo(f));
            })
        .def(
            "write_pin",
            [](transmitter::Client& c, const std::string& pin) {
                if (pin.size() != 6) throw std::invalid_argument("PIN must contain six digits");
                transmitter::PinFrame p{};
                std::copy(pin.begin(), pin.end(), p.pin.begin());
                unwrap(c.write_pin(p));
            })
        .def(
            "read_lora",
            [](transmitter::Client& c) { return lora_out(unwrap(c.read_lora_parameters())); })
        .def(
            "write_lora", [](transmitter::Client& c,
                             const py::dict& d) { unwrap(c.write_lora_parameters(lora_in(d))); })
        .def(
            "restore_lora",
            [](transmitter::Client& c) {
                unwrap(c.restore_default_parameters(transmitter::RadioType::LoRa));
            })
        .def(
            "read_gfsk",
            [](transmitter::Client& c) { return gfsk_out(unwrap(c.read_gfsk_parameters())); })
        .def(
            "write_gfsk", [](transmitter::Client& c,
                             const py::dict& d) { unwrap(c.write_gfsk_parameters(gfsk_in(d))); })
        .def("restore_gfsk", [](transmitter::Client& c) {
            unwrap(c.restore_default_parameters(transmitter::RadioType::GFSK));
        });
}
