#include <csignal>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <unistd.h>

#include <transmitter_simulator/simulator.hpp>
#ifdef TRANSMITTER_SIMULATOR_WITH_GUI
#include "app.hpp"
#endif

namespace {
volatile std::sig_atomic_t g_stop_requested = 0;

void request_stop(int) noexcept { g_stop_requested = 1; }

void print_usage() { std::cout << "Usage: transmitter_simulator --headless [--pty-link <path>]\n"; }
}  // namespace

int main(int argc, char* argv[]) {
    bool headless = false;
    transmitter_simulator::SimulatorOptions options{};
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--help") {
            print_usage();
            return 0;
        }
        if (argument == "--version") {
            std::cout << "transmitter_simulator 1.0\n";
            return 0;
        }
        if (argument == "--headless") {
            headless = true;
            continue;
        }
        if (argument == "--pty-link" && index + 1 < argc) {
            options.transport.stable_path = argv[++index];
            continue;
        }
        std::cerr << "transmitter_simulator: invalid argument: " << argument << '\n';
        return 2;
    }
    if (!headless) {
#ifdef TRANSMITTER_SIMULATOR_WITH_GUI
        transmitter_simulator::Simulator simulator;
        const auto started = simulator.start(options);
        if (!started) {
            std::cerr << "transmitter_simulator: failed to start\n";
            return 1;
        }
        const auto result = transmitter_simulator::run_gui(simulator);
        simulator.stop();
        return result;
#else
        std::cerr << "transmitter_simulator: this build supports --headless only\n";
        return 2;
#endif
    }
    transmitter_simulator::Simulator simulator;
    const auto started = simulator.start(options);
    if (!started) {
        std::cerr << "transmitter_simulator: failed to start\n";
        return 1;
    }
    const auto snapshot = simulator.snapshot();
    std::cout << "WRS_TRANSMITTER_SIMULATOR_READY stable_path=" << snapshot.stable_path
              << " pid=" << static_cast<long>(::getpid()) << std::endl;
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);
    while (g_stop_requested == 0) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    simulator.stop();
    return 0;
}
