#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace binding {
void bind_receiver(py::module_& module);
void bind_transmitter(py::module_& module);
void bind_logging(py::module_& module);
}  // namespace binding

PYBIND11_MODULE(wrs_debugger_adapter, module) {
    binding::bind_logging(module);
    binding::bind_transmitter(module);
    binding::bind_receiver(module);
}
