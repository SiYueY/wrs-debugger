#include <wrs/logging.hpp>

#include <iomanip>
#include <iostream>
#include <mutex>
#include <utility>

namespace wrs::logging {
namespace {
std::mutex sink_mutex;
Sink sink;

const char* level_name(Level level) noexcept {
    switch (level) {
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO";
        case Level::Warning:
            return "WARNING";
        case Level::Error:
            return "ERROR";
    }
    return "UNKNOWN";
}

void fallback(const RecordView& record) noexcept {
    std::cerr << "[wrs] " << level_name(record.level) << ' ' << record.component << ' '
              << record.message << '\n';
}
}  // namespace

HexView hex(const std::uint8_t* data, std::size_t size) noexcept { return {data, size, false}; }

HexView redacted(std::size_t size) noexcept { return {nullptr, size, true}; }

std::ostream& operator<<(std::ostream& output, HexView value) {
    if (value.redacted || value.data == nullptr)
        return output << "<redacted " << value.size << " bytes>";
    const auto flags = output.flags();
    const auto fill = output.fill();
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < value.size; ++index)
        output << std::setw(2) << static_cast<unsigned int>(value.data[index]);
    output.flags(flags);
    output.fill(fill);
    return output;
}

void set_sink(Sink value) {
    std::lock_guard<std::mutex> lock(sink_mutex);
    sink = std::move(value);
}

void clear_sink() noexcept {
    try {
        std::lock_guard<std::mutex> lock(sink_mutex);
        sink = nullptr;
    } catch (...) {
    }
}

void write(Level level, std::string_view component, std::string_view message) noexcept {
    const RecordView record{level, component, message};
    try {
        Sink current;
        {
            std::lock_guard<std::mutex> lock(sink_mutex);
            current = sink;
        }
        if (current) {
            current(record);
            return;
        }
    } catch (...) {
    }
    fallback(record);
}

Line::Line(Level level, std::string_view component) noexcept
: level_(level), component_(component) {}

Line::~Line() noexcept {
    if (active_) write(level_, component_, message_.str());
}

Line::Line(Line&& other) noexcept
: level_(other.level_),
  component_(std::move(other.component_)),
  message_(std::move(other.message_)),
  active_(std::exchange(other.active_, false)) {}

Line& Line::operator=(Line&& other) noexcept {
    if (this == &other) return *this;
    if (active_) write(level_, component_, message_.str());
    level_ = other.level_;
    component_ = std::move(other.component_);
    message_ = std::move(other.message_);
    active_ = std::exchange(other.active_, false);
    return *this;
}

}  // namespace wrs::logging
