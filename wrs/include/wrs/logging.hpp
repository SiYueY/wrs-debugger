#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>

namespace wrs::logging {

enum class Level {
    Debug,
    Info,
    Warning,
    Error,
};

struct RecordView final {
    Level level;
    std::string_view component;
    std::string_view message;
};

using Sink = std::function<void(const RecordView&)>;

struct HexView final {
    const std::uint8_t* data;
    std::size_t size;
    bool redacted{false};
};

[[nodiscard]] HexView hex(const std::uint8_t* data, std::size_t size) noexcept;
[[nodiscard]] HexView redacted(std::size_t size) noexcept;
std::ostream& operator<<(std::ostream& output, HexView value);

void set_sink(Sink sink);
void clear_sink() noexcept;
void write(Level level, std::string_view component, std::string_view message) noexcept;

class Line final {
public:
    Line(Level level, std::string_view component) noexcept;
    ~Line() noexcept;
    Line(const Line&) = delete;
    Line& operator=(const Line&) = delete;
    Line(Line&& other) noexcept;
    Line& operator=(Line&& other) noexcept;

    template <typename T>
    Line& operator<<(const T& value) noexcept {
        try {
            message_ << value;
        } catch (...) {
        }
        return *this;
    }

private:
    Level level_;
    std::string component_;
    std::ostringstream message_;
    bool active_{true};
};

}  // namespace wrs::logging

#define WRS_LOG_DEBUG(component) ::wrs::logging::Line(::wrs::logging::Level::Debug, component)
#define WRS_LOG_INFO(component) ::wrs::logging::Line(::wrs::logging::Level::Info, component)
#define WRS_LOG_WARNING(component) ::wrs::logging::Line(::wrs::logging::Level::Warning, component)
#define WRS_LOG_ERROR(component) ::wrs::logging::Line(::wrs::logging::Level::Error, component)
