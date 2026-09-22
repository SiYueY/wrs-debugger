#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

#include <transmitter_simulator/protocol.hpp>

namespace transmitter_simulator {

enum class FrameDirection : std::uint8_t { Received, Transmitted, Event };

struct HistoryRecord final {
    std::chrono::steady_clock::time_point timestamp{};
    FrameDirection direction{};
    Bytes frame{};
    std::string note{};
};

class History final {
public:
    void record(FrameDirection direction, const Bytes& frame, std::string note = {});
    void clear() noexcept;
    [[nodiscard]] const std::deque<HistoryRecord>& records() const noexcept;

private:
    std::deque<HistoryRecord> records_{};
    static constexpr std::size_t k_capacity = 2000;
};

}  // namespace transmitter_simulator
