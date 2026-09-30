#include <string>
#include <utility>

#include <pybind11/pybind11.h>

#include <wrs/logging.hpp>

namespace py = pybind11;

namespace binding {
void bind_logging(py::module_& module) {
    module.def("configure_wrs_logging", [](py::function handler) {
        wrs::logging::set_sink(
            [handler = std::move(handler)](const wrs::logging::RecordView& record) {
                try {
                    py::gil_scoped_acquire gil;
                    handler(
                        static_cast<int>(record.level), std::string(record.component),
                        std::string(record.message));
                } catch (...) {
                }
            });
    });
    module.def("clear_wrs_logging", [] { wrs::logging::clear_sink(); });
}
}  // namespace binding
