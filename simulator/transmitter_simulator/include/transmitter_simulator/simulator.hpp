#pragma once

#include <functional>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <transmitter_simulator/device.hpp>
#include <transmitter_simulator/error.hpp>
#include <transmitter_simulator/fault.hpp>
#include <transmitter_simulator/history.hpp>
#include <transmitter_simulator/result.hpp>
#include <transmitter_simulator/transport.hpp>

namespace transmitter_simulator {

enum class LifecycleState : std::uint8_t { Stopped, Running, Disconnected };
enum class PeerState : std::uint8_t { Detached, Active };

struct SimulatorOptions final {
    TransportOptions transport{};
};

struct SimulatorSnapshot final {
    LifecycleState lifecycle{LifecycleState::Stopped};
    PeerState peer{PeerState::Detached};
    std::string slave_path{};
    std::string stable_path{};
    DeviceState device{};
    FaultConfig fault{};
    std::vector<HistoryRecord> history{};
    std::size_t rx_frames{};
    std::size_t tx_frames{};
};

class Simulator final {
public:
    Simulator();
    ~Simulator() noexcept;
    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;

    [[nodiscard]] Result<void, Error> start(const SimulatorOptions& options);
    void stop() noexcept;
    [[nodiscard]] SimulatorSnapshot snapshot() const;
    [[nodiscard]] Result<void, Error> set_identity(const DeviceIdentity& identity);
    [[nodiscard]] Result<void, Error> set_pin(const Pin& pin);
    [[nodiscard]] Result<void, Error> set_battery(std::uint8_t percentage);
    [[nodiscard]] Result<void, Error> set_lora(const LoraConfig& config);
    [[nodiscard]] Result<void, Error> set_gfsk(const GfskConfig& config);
    [[nodiscard]] Result<void, Error> set_firmware_metadata(const FirmwareMetadata& metadata);
    /** Atomically updates GUI-editable SDO state through the runtime control queue. */
    [[nodiscard]] Result<void, Error> set_sdo_editor_state(const SdoEditorState& state);
    [[nodiscard]] Result<void, Error> reset_device();
    [[nodiscard]] Result<void, Error> set_fault_config(const FaultConfig& config);
    /** Atomically publishes a new PTY stable path and disconnects the old peer on success. */
    [[nodiscard]] Result<void, Error> set_transport_path(std::string stable_path);
    [[nodiscard]] Result<void, Error> disconnect();
    [[nodiscard]] Result<void, Error> reconnect();
    [[nodiscard]] Result<void, Error> clear_history();

private:
    struct Impl;
    [[nodiscard]] Result<void, Error> ensure_running() const noexcept;
    [[nodiscard]] Result<void, Error> submit(std::function<Error()> action);
    void process_control_commands() noexcept;
    void notify_worker() noexcept;
    [[nodiscard]] Bytes handle_frame(const Bytes& request, bool& should_respond);
    void apply_response_faults(Bytes& response, const Bytes& request) noexcept;
    void schedule_response(Bytes response, const Bytes& request) noexcept;
    void deliver_pending_response() noexcept;
    void worker() noexcept;
    void write_frame(const Bytes& frame) noexcept;
    [[nodiscard]] bool write_bytes(const std::uint8_t* bytes, std::size_t size) noexcept;

    std::unique_ptr<Impl> impl_;
};

}  // namespace transmitter_simulator
