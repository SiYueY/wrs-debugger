#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <transmitter_simulator/error.hpp>
#include <transmitter_simulator/result.hpp>

namespace transmitter_simulator {

struct TransportOptions final {
    std::string stable_path{"/tmp/wrs-transmitter-simulator/tty"};
};

class PtyTransport final {
public:
    PtyTransport() noexcept = default;
    ~PtyTransport() noexcept;
    PtyTransport(const PtyTransport&) = delete;
    PtyTransport& operator=(const PtyTransport&) = delete;
    PtyTransport(PtyTransport&& other) noexcept;
    PtyTransport& operator=(PtyTransport&& other) noexcept;

    /** Creates one Linux PTY and atomically publishes its slave via stable_path. */
    [[nodiscard]] Result<void, Error> open(const TransportOptions& options);
    /** Replaces the kernel PTY while retaining exclusive ownership of stable_path. */
    [[nodiscard]] Result<void, Error> recreate();
    void disconnect() noexcept;
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] Result<std::size_t, Error> read(std::uint8_t* data, std::size_t size) noexcept;
    [[nodiscard]] Result<std::size_t, Error> write(
        const std::uint8_t* data, std::size_t size) noexcept;
    [[nodiscard]] const std::string& slave_path() const noexcept;
    [[nodiscard]] const std::string& stable_path() const noexcept;

private:
    int master_fd_{-1};
    int lock_fd_{-1};
    std::string lock_path_{};
    std::string slave_path_{};
    std::string stable_path_{};
};

}  // namespace transmitter_simulator
