#include <stdexcept>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <serial/tool.hpp>

namespace py = pybind11;
using namespace py::literals;

namespace binding {
void bind_receiver(py::module_& module);
void bind_transmitter(py::module_& module);
}  // namespace binding

PYBIND11_MODULE(wrs_debugger_native, module) {
    module.def("list_serial_ports", [] {
        const auto result = serial::list_ports();
        if (!result) throw std::runtime_error("SERIAL_LIST_FAILED");
        py::list ports;
        for (const auto& port : result.value())
            ports.append(py::dict(
                "device"_a = port.path, "description"_a = port.description,
                "manufacturer"_a = (port.usb.available ? port.usb.manufacturer : ""),
                "product"_a = (port.usb.available ? port.usb.product : ""),
                "serial_number"_a = (port.usb.available ? port.usb.serial_number : ""),
                "vendor_id"_a =
                    (port.usb.available ? py::cast(port.usb.vendor_id) : py::none()),
                "product_id"_a =
                    (port.usb.available ? py::cast(port.usb.product_id) : py::none())));
        return ports;
    });

    binding::bind_transmitter(module);
    binding::bind_receiver(module);
}
