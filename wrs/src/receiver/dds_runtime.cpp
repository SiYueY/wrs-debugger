#include <receiver/dds_runtime.hpp>

#include <filesystem>

#include <ddswrapper/context.hpp>

namespace receiver {

DdsRuntime::~DdsRuntime() noexcept { shutdown(); }

hardware::Result<void, Error> DdsRuntime::initialize(const std::string& qos_profile_path) noexcept {
    if (owns_runtime_ || ddswrapper::ok()) {
        return hardware::Result<void, Error>::failure(Error::AlreadyInitialized);
    }

    std::error_code error;
    if (qos_profile_path.empty() || !std::filesystem::is_regular_file(qos_profile_path, error) ||
        error) {
        return hardware::Result<void, Error>::failure(Error::InvalidArgument);
    }

    if (!ddswrapper::init(qos_profile_path)) {
        return hardware::Result<void, Error>::failure(Error::InitializationFailed);
    }

    owns_runtime_ = true;
    return hardware::Result<void, Error>::success();
}

void DdsRuntime::shutdown() noexcept {
    if (!owns_runtime_) {
        return;
    }

    ddswrapper::shutdown();
    owns_runtime_ = false;
}

bool DdsRuntime::is_initialized() const noexcept { return owns_runtime_ && ddswrapper::ok(); }

}  // namespace receiver
