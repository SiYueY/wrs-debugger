#include <transmitter_simulator/history.hpp>

#include <utility>

namespace transmitter_simulator {
void History::record(FrameDirection direction, const Bytes& frame, std::string note) {
    if (records_.size() == k_capacity) records_.pop_front();
    records_.push_back({std::chrono::steady_clock::now(), direction, frame, std::move(note)});
}

void History::clear() noexcept { records_.clear(); }

const std::deque<HistoryRecord>& History::records() const noexcept { return records_; }
}  // namespace transmitter_simulator
