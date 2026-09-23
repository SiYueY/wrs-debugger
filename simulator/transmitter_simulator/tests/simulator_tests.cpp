#include <catch2/catch.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <transmitter_simulator/simulator.hpp>

namespace transmitter_simulator {
namespace {
int open_raw_slave(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) return -1;
    termios settings{};
    if (::tcgetattr(fd, &settings) != 0) {
        ::close(fd);
        return -1;
    }
    ::cfmakeraw(&settings);
    settings.c_cflag |= CLOCAL | CREAD;
    if (::tcsetattr(fd, TCSANOW, &settings) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

bool write_all(int fd, const Bytes& frame) {
    std::size_t offset = 0;
    while (offset < frame.size()) {
        const auto written = ::write(fd, frame.bytes() + offset, frame.size() - offset);
        if (written <= 0) return false;
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

bool read_frame(int fd, Bytes& frame) {
    std::size_t offset = 0;
    while (offset < frame.size()) {
        pollfd descriptor{fd, POLLIN, 0};
        if (::poll(&descriptor, 1, 1000) <= 0) return false;
        const auto received = ::read(fd, frame.bytes() + offset, frame.size() - offset);
        if (received <= 0) return false;
        offset += static_cast<std::size_t>(received);
    }
    return true;
}

bool readable_within(int fd, int timeout_ms) {
    pollfd descriptor{fd, POLLIN, 0};
    return ::poll(&descriptor, 1, timeout_ms) > 0 && (descriptor.revents & POLLIN) != 0;
}

std::string temporary_link() {
    return "/tmp/wrs-transmitter-simulator-test-" + std::to_string(::getpid()) + "/tty";
}
}  // namespace

TEST_CASE("runtime serves SDO requests across an actual PTY") {
    Simulator simulator;
    SimulatorOptions options{};
    options.transport.stable_path = temporary_link();
    std::filesystem::create_directories(
        std::filesystem::path(options.transport.stable_path).parent_path());
    REQUIRE(simulator.start(options));
    const int fd = open_raw_slave(simulator.snapshot().stable_path);
    REQUIRE(fd >= 0);

    SdoFrame request{};
    request.command = SystemCmd::ParamReadReq;
    request.transaction_id = 17;
    request.object_index = 0x001;
    REQUIRE(write_all(fd, request.to_bytes()));
    Bytes response{};
    REQUIRE(read_frame(fd, response));
    REQUIRE(response[0] == static_cast<std::uint8_t>(SystemCmd::ParamReadRsp));
    REQUIRE(response.read_le16(5) == 0x4001);
    REQUIRE(response.has_valid_crc());

    ::close(fd);
    simulator.stop();
    REQUIRE_FALSE(std::filesystem::exists(options.transport.stable_path));
}

TEST_CASE("runtime rejects invalid radio write without committing state") {
    Simulator simulator;
    SimulatorOptions options{};
    options.transport.stable_path = temporary_link();
    std::filesystem::create_directories(
        std::filesystem::path(options.transport.stable_path).parent_path());
    REQUIRE(simulator.start(options));
    const int fd = open_raw_slave(simulator.snapshot().stable_path);
    REQUIRE(fd >= 0);

    auto invalid = LoraConfig::defaults();
    invalid.tx_power = 11;
    LoRaParamFrame request{};
    request.command = SystemCmd::ParamWriteReq;
    request.transaction_id = 18;
    request.config = invalid;
    REQUIRE(write_all(fd, request.to_bytes()));
    Bytes response{};
    REQUIRE(read_frame(fd, response));
    REQUIRE(response[39] == static_cast<std::uint8_t>(ResultCode::Failure));
    REQUIRE(simulator.snapshot().device.lora.tx_power == 10);

    ::close(fd);
    simulator.stop();
}

TEST_CASE("public control calls complete before returning") {
    Simulator simulator;
    SimulatorOptions options{};
    options.transport.stable_path = temporary_link();
    std::filesystem::create_directories(
        std::filesystem::path(options.transport.stable_path).parent_path());
    REQUIRE(simulator.start(options));
    REQUIRE(simulator.set_battery(42));
    REQUIRE(simulator.snapshot().device.battery_percentage == 42);
    REQUIRE_FALSE(simulator.set_battery(101));
    SdoEditorState editor{};
    editor.identity.product_code = 0xaabbccdd;
    editor.battery_percentage = 42;
    editor.upgrade_request = 0x12345678;
    REQUIRE(simulator.set_sdo_editor_state(editor));
    const auto state = simulator.snapshot().device;
    REQUIRE(state.identity.product_code == 0xaabbccdd);
    REQUIRE(state.battery_percentage == 42);
    REQUIRE(state.upgrade_request == 0x12345678);
    simulator.stop();
}

TEST_CASE("stable path has exclusive ownership across disconnect and reconnect") {
    Simulator first;
    Simulator second;
    SimulatorOptions options{};
    options.transport.stable_path = temporary_link();
    std::filesystem::create_directories(
        std::filesystem::path(options.transport.stable_path).parent_path());
    REQUIRE(first.start(options));
    REQUIRE_FALSE(second.start(options));
    REQUIRE(first.disconnect());
    REQUIRE_FALSE(second.start(options));
    REQUIRE(first.reconnect());
    REQUIRE(std::filesystem::is_symlink(options.transport.stable_path));
    first.stop();
    REQUIRE(second.start(options));
    second.stop();
}

TEST_CASE("transport path switches atomically through the runtime control queue") {
    Simulator simulator;
    SimulatorOptions options{};
    options.transport.stable_path = temporary_link();
    const auto parent = std::filesystem::path(options.transport.stable_path).parent_path();
    const auto replacement = (parent / "tty-replacement").string();
    std::filesystem::create_directories(parent);
    REQUIRE(simulator.start(options));
    const auto old_slave = simulator.snapshot().slave_path;

    REQUIRE(simulator.set_transport_path(replacement));
    const auto changed = simulator.snapshot();
    REQUIRE(changed.lifecycle == LifecycleState::Running);
    REQUIRE(changed.stable_path == replacement);
    REQUIRE(changed.slave_path != old_slave);
    REQUIRE_FALSE(std::filesystem::exists(options.transport.stable_path));
    REQUIRE(std::filesystem::is_symlink(replacement));

    REQUIRE_FALSE(simulator.set_transport_path("relative-tty"));
    REQUIRE(simulator.snapshot().stable_path == replacement);
    REQUIRE(std::filesystem::is_symlink(replacement));
    simulator.stop();
    REQUIRE_FALSE(std::filesystem::exists(replacement));
}

TEST_CASE("recreating a PTY republishes the configured stable path") {
    Simulator simulator;
    SimulatorOptions options{};
    options.transport.stable_path = temporary_link();
    std::filesystem::create_directories(
        std::filesystem::path(options.transport.stable_path).parent_path());
    REQUIRE(simulator.start(options));
    const auto before = simulator.snapshot();

    REQUIRE(simulator.recreate_pty());
    const auto after = simulator.snapshot();
    REQUIRE(after.lifecycle == LifecycleState::Running);
    REQUIRE(after.stable_path == before.stable_path);
    REQUIRE_FALSE(after.slave_path.empty());
    REQUIRE(after.slave_path != before.slave_path);
    REQUIRE(std::filesystem::is_symlink(after.stable_path));

    const int fd = open_raw_slave(after.stable_path);
    REQUIRE(fd >= 0);
    ::close(fd);
    simulator.stop();
}

TEST_CASE("Host peer close does not simulate a device disconnect") {
    Simulator simulator;
    SimulatorOptions options{};
    options.transport.stable_path = temporary_link();
    std::filesystem::create_directories(
        std::filesystem::path(options.transport.stable_path).parent_path());
    REQUIRE(simulator.start(options));
    int fd = open_raw_slave(simulator.snapshot().stable_path);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    REQUIRE(simulator.snapshot().lifecycle == LifecycleState::Running);
    REQUIRE(std::filesystem::is_symlink(options.transport.stable_path));

    fd = open_raw_slave(simulator.snapshot().stable_path);
    REQUIRE(fd >= 0);
    SdoFrame request{};
    request.command = SystemCmd::ParamReadReq;
    request.transaction_id = 31;
    request.object_index = 0x102;
    REQUIRE(write_all(fd, request.to_bytes()));
    Bytes response{};
    REQUIRE(read_frame(fd, response));
    REQUIRE(response.read_le16(5) == 0x4102);
    ::close(fd);
    simulator.stop();
}

TEST_CASE("fault scheduler delays, corrupts, and drops real PTY responses") {
    Simulator simulator;
    SimulatorOptions options{};
    options.transport.stable_path = temporary_link();
    std::filesystem::create_directories(
        std::filesystem::path(options.transport.stable_path).parent_path());
    REQUIRE(simulator.start(options));
    const int fd = open_raw_slave(simulator.snapshot().stable_path);
    REQUIRE(fd >= 0);

    SdoFrame request{};
    request.command = SystemCmd::ParamReadReq;
    request.transaction_id = 41;
    request.object_index = 0x001;
    FaultConfig delay{};
    delay.response_delay = std::chrono::milliseconds(100);
    REQUIRE(simulator.set_fault_config(delay));
    REQUIRE(write_all(fd, request.to_bytes()));
    REQUIRE(write_all(
        fd,
        request.to_bytes()));  // Same verified frame while response is pending: retry coalescing.
    REQUIRE_FALSE(readable_within(fd, 30));
    Bytes delayed{};
    REQUIRE(read_frame(fd, delayed));
    REQUIRE(delayed.has_valid_crc());
    REQUIRE_FALSE(readable_within(fd, 100));

    FaultConfig corrupt{};
    corrupt.corrupt_next_crc = true;
    REQUIRE(simulator.set_fault_config(corrupt));
    REQUIRE(write_all(fd, request.to_bytes()));
    Bytes corrupted{};
    REQUIRE(read_frame(fd, corrupted));
    REQUIRE_FALSE(corrupted.has_valid_crc());

    FaultConfig drop{};
    drop.next_delivery_fault = DeliveryFault::Drop;
    REQUIRE(simulator.set_fault_config(drop));
    REQUIRE(write_all(fd, request.to_bytes()));
    REQUIRE_FALSE(readable_within(fd, 100));
    REQUIRE(write_all(fd, request.to_bytes()));
    Bytes retried{};
    REQUIRE(read_frame(fd, retried));
    REQUIRE(retried.has_valid_crc());
    ::close(fd);
    simulator.stop();
}

TEST_CASE("SDO and delivery faults use their documented wire effects") {
    Simulator simulator;
    SimulatorOptions options{};
    options.transport.stable_path = temporary_link();
    std::filesystem::create_directories(
        std::filesystem::path(options.transport.stable_path).parent_path());
    REQUIRE(simulator.start(options));
    const int fd = open_raw_slave(simulator.snapshot().stable_path);
    REQUIRE(fd >= 0);
    SdoFrame request{};
    request.command = SystemCmd::ParamReadReq;
    request.transaction_id = 51;
    request.object_index = 0x001;

    FaultConfig sdo{};
    sdo.next_sdo_fault = SdoFault::InProgress;
    REQUIRE(simulator.set_fault_config(sdo));
    REQUIRE(write_all(fd, request.to_bytes()));
    Bytes sdo_response{};
    REQUIRE(read_frame(fd, sdo_response));
    REQUIRE(sdo_response.read_le16(5) == 0xb001);
    REQUIRE(sdo_response[39] == 0);

    FaultConfig split{};
    split.next_delivery_fault = DeliveryFault::Split;
    split.split_after_bytes = 10;
    split.split_delay = std::chrono::milliseconds(20);
    REQUIRE(simulator.set_fault_config(split));
    REQUIRE(write_all(fd, request.to_bytes()));
    Bytes split_response{};
    REQUIRE(read_frame(fd, split_response));
    REQUIRE(split_response.has_valid_crc());

    FaultConfig truncate{};
    truncate.next_delivery_fault = DeliveryFault::Truncate;
    truncate.truncate_after_bytes = 7;
    REQUIRE(simulator.set_fault_config(truncate));
    REQUIRE(write_all(fd, request.to_bytes()));
    std::array<std::uint8_t, k_frame_size> truncated{};
    pollfd truncated_descriptor{fd, POLLIN, 0};
    REQUIRE(::poll(&truncated_descriptor, 1, 1000) > 0);
    const auto truncated_size = ::read(fd, truncated.data(), truncated.size());
    REQUIRE(truncated_size == 7);
    REQUIRE_FALSE(readable_within(fd, 100));

    FaultConfig disconnect{};
    disconnect.next_delivery_fault = DeliveryFault::Disconnect;
    REQUIRE(simulator.set_fault_config(disconnect));
    REQUIRE(write_all(fd, request.to_bytes()));
    REQUIRE(readable_within(fd, 250));  // PTY reports EOF/HUP after device-side close.
    REQUIRE(simulator.snapshot().lifecycle == LifecycleState::Disconnected);
    REQUIRE_FALSE(std::filesystem::exists(options.transport.stable_path));
    ::close(fd);
    simulator.stop();
}
}  // namespace transmitter_simulator
