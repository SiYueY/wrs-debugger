#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

#include <unistd.h>

#include <transmitter/client.hpp>
#include <transmitter/protocol.hpp>
#include <transmitter_simulator/fault.hpp>
#include <transmitter_simulator/simulator.hpp>

namespace {
using namespace std::chrono_literals;

std::string stable_path() {
    const auto directory = "/tmp/wrs-transmitter-e2e-" + std::to_string(::getpid());
    std::filesystem::create_directories(directory);
    return directory + "/tty";
}

void test_normal_path() {
    transmitter_simulator::Simulator simulator;
    transmitter_simulator::SimulatorOptions options{};
    options.transport.stable_path = stable_path();
    assert(simulator.start(options));
    transmitter::Client client;
    assert(client.open(options.transport.stable_path, 300ms, 0ms, 1));
    assert(client.read_lora_parameters());
    assert(client.read_gfsk_parameters());
    assert(client.read_pin());
    assert(client.read_sdo(transmitter::SdoObject::Battery));
    transmitter::SdoFrame upgrade{};
    upgrade.object_index = static_cast<std::uint16_t>(transmitter::SdoObject::UpgradeRequest);
    upgrade.object_data = 0x454e;
    assert(client.write_sdo(upgrade));
    assert(client.close());
    simulator.stop();
}

void test_crc_fault() {
    transmitter_simulator::Simulator simulator;
    transmitter_simulator::SimulatorOptions options{};
    options.transport.stable_path = stable_path();
    assert(simulator.start(options));
    transmitter::Client client;
    assert(client.open(options.transport.stable_path, 300ms, 0ms, 1));
    transmitter_simulator::FaultConfig fault{};
    fault.corrupt_next_crc = true;
    assert(simulator.set_fault_config(fault));
    const auto read = client.read_lora_parameters();
    assert(!read && read.error() == transmitter::Error::CrcMismatch);
    simulator.stop();
}

void test_fault_error_mapping() {
    transmitter_simulator::Simulator simulator;
    transmitter_simulator::SimulatorOptions options{};
    options.transport.stable_path = stable_path();
    assert(simulator.start(options));
    transmitter::Client client;
    assert(client.open(options.transport.stable_path, 300ms, 0ms, 1));

    transmitter_simulator::FaultConfig wrong_command{};
    wrong_command.wrong_next_command = true;
    assert(simulator.set_fault_config(wrong_command));
    const auto command_read = client.read_lora_parameters();
    assert(!command_read && command_read.error() == transmitter::Error::UnexpectedResponse);

    transmitter_simulator::FaultConfig sdo_fault{};
    sdo_fault.next_sdo_fault = transmitter_simulator::SdoFault::Error;
    assert(simulator.set_fault_config(sdo_fault));
    const auto sdo_read = client.read_sdo(transmitter::SdoObject::Battery);
    assert(!sdo_read && sdo_read.error() == transmitter::Error::SdoError);
    assert(client.close());
    simulator.stop();
}

void test_timeout_rejection_and_disconnect_mapping() {
    transmitter_simulator::Simulator simulator;
    transmitter_simulator::SimulatorOptions options{};
    options.transport.stable_path = stable_path();
    assert(simulator.start(options));
    transmitter::Client client;
    assert(client.open(options.transport.stable_path, 100ms, 0ms, 1));

    transmitter_simulator::FaultConfig wrong_transaction{};
    wrong_transaction.wrong_next_transaction = true;
    assert(simulator.set_fault_config(wrong_transaction));
    const auto timeout = client.read_lora_parameters();
    assert(!timeout && timeout.error() == transmitter::Error::TimedOut);

    transmitter_simulator::FaultConfig rejected{};
    rejected.fail_next_business_response = true;
    assert(simulator.set_fault_config(rejected));
    const auto rejection = client.read_lora_parameters();
    assert(!rejection && rejection.error() == transmitter::Error::DeviceRejected);

    transmitter_simulator::FaultConfig disconnect{};
    disconnect.next_delivery_fault = transmitter_simulator::DeliveryFault::Disconnect;
    assert(simulator.set_fault_config(disconnect));
    const auto disconnected = client.read_lora_parameters();
    assert(!disconnected && disconnected.error() == transmitter::Error::Disconnected);
    simulator.stop();
}
}  // namespace

int main() {
    test_normal_path();
    test_crc_fault();
    test_fault_error_mapping();
    test_timeout_rejection_and_disconnect_mapping();
    return 0;
}
