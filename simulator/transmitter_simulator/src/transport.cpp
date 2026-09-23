#include <transmitter_simulator/transport.hpp>

#include <cerrno>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <utility>

#include <fcntl.h>
#include <pty.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace transmitter_simulator {
namespace {
std::atomic<std::uint64_t> temporary_counter{0};

Error io_error() noexcept {
    return errno == EACCES ? Error::Busy : (errno == ENOENT ? Error::InvalidArgument : Error::Io);
}
}  // namespace

PtyTransport::~PtyTransport() noexcept { close(); }

PtyTransport::PtyTransport(PtyTransport&& other) noexcept
: master_fd_(std::exchange(other.master_fd_, -1)),
  lock_fd_(std::exchange(other.lock_fd_, -1)),
  lock_path_(std::move(other.lock_path_)),
  slave_path_(std::move(other.slave_path_)),
  stable_path_(std::move(other.stable_path_)) {}

PtyTransport& PtyTransport::operator=(PtyTransport&& other) noexcept {
    if (this == &other) return *this;
    close();
    master_fd_ = std::exchange(other.master_fd_, -1);
    lock_fd_ = std::exchange(other.lock_fd_, -1);
    lock_path_ = std::move(other.lock_path_);
    slave_path_ = std::move(other.slave_path_);
    stable_path_ = std::move(other.stable_path_);
    return *this;
}

Result<void, Error> PtyTransport::open(const TransportOptions& options) {
    if (is_open() || options.stable_path.empty())
        return Result<void, Error>::failure(Error::InvalidState);
    std::error_code error;
    const auto stable = std::filesystem::path(options.stable_path);
    const auto parent = stable.parent_path();
    if (parent.empty()) return Result<void, Error>::failure(Error::InvalidArgument);
    if (options.stable_path == "/tmp/wrs-transmitter-simulator/tty") {
        std::filesystem::create_directories(parent, error);
        if (error) return Result<void, Error>::failure(Error::Io);
        (void)::chmod(parent.c_str(), S_IRWXU);
    }
    if (!std::filesystem::is_directory(parent, error) || error)
        return Result<void, Error>::failure(Error::InvalidArgument);
    struct stat parent_stat {};
    if (::lstat(parent.c_str(), &parent_stat) != 0 || !S_ISDIR(parent_stat.st_mode) ||
        parent_stat.st_uid != ::geteuid())
        return Result<void, Error>::failure(Error::Busy);
    if (lock_fd_ < 0) {
        lock_path_ = (parent / ("." + stable.filename().string() + ".lock")).string();
        lock_fd_ = ::open(
            lock_path_.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
        if (lock_fd_ < 0) return Result<void, Error>::failure(io_error());
        struct stat lock_stat {};
        if (::fstat(lock_fd_, &lock_stat) != 0 || !S_ISREG(lock_stat.st_mode) ||
            lock_stat.st_uid != ::geteuid() || ::flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
            (void)::close(lock_fd_);
            lock_fd_ = -1;
            return Result<void, Error>::failure(Error::Busy);
        }
        (void)::fchmod(lock_fd_, S_IRUSR | S_IWUSR);
    }
    if (std::filesystem::exists(stable, error) && !std::filesystem::is_symlink(stable, error)) {
        close();
        return Result<void, Error>::failure(Error::Busy);
    }

    int slave_fd = -1;
    char name[256]{};
    if (::openpty(&master_fd_, &slave_fd, name, nullptr, nullptr) != 0) {
        close();
        return Result<void, Error>::failure(io_error());
    }
    const auto flags = ::fcntl(master_fd_, F_GETFL);
    if (flags < 0 || ::fcntl(master_fd_, F_SETFL, flags | O_NONBLOCK) != 0) {
        ::close(slave_fd);
        close();
        return Result<void, Error>::failure(io_error());
    }
    ::close(slave_fd);
    slave_path_ = name;
    stable_path_ = options.stable_path;
    const auto nonce =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) ^
        temporary_counter.fetch_add(1, std::memory_order_relaxed);
    const auto temporary =
        stable_path_ + ".tmp." + std::to_string(::getpid()) + "." + std::to_string(nonce);
    if (::symlink(slave_path_.c_str(), temporary.c_str()) != 0 ||
        ::rename(temporary.c_str(), stable_path_.c_str()) != 0) {
        (void)::unlink(temporary.c_str());
        close();
        return Result<void, Error>::failure(Error::Io);
    }
    return Result<void, Error>::success();
}

Result<void, Error> PtyTransport::recreate() {
    if (!is_open() || stable_path_.empty() || lock_fd_ < 0)
        return Result<void, Error>::failure(Error::InvalidState);

    // Keep the old master open while allocating the replacement. This prevents devpts
    // from immediately reusing its number and makes stable-path publication atomic.
    int replacement_master = -1;
    int replacement_slave = -1;
    char replacement_name[256]{};
    if (::openpty(&replacement_master, &replacement_slave, replacement_name, nullptr, nullptr) != 0)
        return Result<void, Error>::failure(io_error());
    const auto flags = ::fcntl(replacement_master, F_GETFL);
    if (flags < 0 || ::fcntl(replacement_master, F_SETFL, flags | O_NONBLOCK) != 0) {
        (void)::close(replacement_slave);
        (void)::close(replacement_master);
        return Result<void, Error>::failure(io_error());
    }
    (void)::close(replacement_slave);

    const auto nonce =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) ^
        temporary_counter.fetch_add(1, std::memory_order_relaxed);
    const auto temporary =
        stable_path_ + ".tmp." + std::to_string(::getpid()) + "." + std::to_string(nonce);
    if (::symlink(replacement_name, temporary.c_str()) != 0 ||
        ::rename(temporary.c_str(), stable_path_.c_str()) != 0) {
        (void)::unlink(temporary.c_str());
        (void)::close(replacement_master);
        return Result<void, Error>::failure(Error::Io);
    }

    (void)::close(master_fd_);
    master_fd_ = replacement_master;
    slave_path_ = replacement_name;
    return Result<void, Error>::success();
}

void PtyTransport::disconnect() noexcept {
    if (!stable_path_.empty()) {
        char target[256]{};
        const auto size = ::readlink(stable_path_.c_str(), target, sizeof(target) - 1);
        if (size >= 0 && slave_path_ == std::string(target, static_cast<std::size_t>(size)))
            (void)::unlink(stable_path_.c_str());
    }
    if (master_fd_ >= 0) (void)::close(master_fd_);
    master_fd_ = -1;
    slave_path_.clear();
    stable_path_.clear();
}

void PtyTransport::close() noexcept {
    disconnect();
    if (lock_fd_ >= 0) {
        (void)::flock(lock_fd_, LOCK_UN);
        (void)::close(lock_fd_);
    }
    lock_fd_ = -1;
    lock_path_.clear();
}

bool PtyTransport::is_open() const noexcept { return master_fd_ >= 0; }
int PtyTransport::native_handle() const noexcept { return master_fd_; }

Result<std::size_t, Error> PtyTransport::read(std::uint8_t* data, std::size_t size) noexcept {
    const auto result = ::read(master_fd_, data, size);
    if (result >= 0) return Result<std::size_t, Error>::success(static_cast<std::size_t>(result));
    if (errno == EAGAIN || errno == EWOULDBLOCK) return Result<std::size_t, Error>::success(0);
    if (errno == EIO) return Result<std::size_t, Error>::failure(Error::Disconnected);
    return Result<std::size_t, Error>::failure(io_error());
}

Result<std::size_t, Error> PtyTransport::write(
    const std::uint8_t* data, std::size_t size) noexcept {
    const auto result = ::write(master_fd_, data, size);
    if (result >= 0) return Result<std::size_t, Error>::success(static_cast<std::size_t>(result));
    if (errno == EAGAIN || errno == EWOULDBLOCK) return Result<std::size_t, Error>::success(0);
    if (errno == EIO) return Result<std::size_t, Error>::failure(Error::Disconnected);
    return Result<std::size_t, Error>::failure(io_error());
}

const std::string& PtyTransport::slave_path() const noexcept { return slave_path_; }
const std::string& PtyTransport::stable_path() const noexcept { return stable_path_; }
}  // namespace transmitter_simulator
