#pragma once

#include <string>

#include <hardware/result.hpp>
#include <receiver/error.hpp>

namespace receiver {

/** @brief Sole owner of the process-wide DDS runtime used for Receiver communication. */
class DdsRuntime final {
public:
    DdsRuntime() noexcept = default;
    ~DdsRuntime() noexcept;

    DdsRuntime(const DdsRuntime&) = delete;
    DdsRuntime& operator=(const DdsRuntime&) = delete;
    DdsRuntime(DdsRuntime&&) = delete;
    DdsRuntime& operator=(DdsRuntime&&) = delete;

    /** @brief Initializes DDS with a caller-supplied Fast DDS QoS XML profile. */
    [[nodiscard]] hardware::Result<void, Error> initialize(
        const std::string& qos_profile_path) noexcept;

    /** @brief Shuts down DDS when this instance successfully initialized it. */
    void shutdown() noexcept;

    /** @brief Reports whether this instance still owns an initialized DDS runtime. */
    [[nodiscard]] bool is_initialized() const noexcept;

private:
    bool owns_runtime_{false};
};

}  // namespace receiver
