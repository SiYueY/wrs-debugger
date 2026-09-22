#pragma once

#include <cassert>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace transmitter_simulator {

/** Inline, allocation-free success-or-error value; mirrors hardware::Result semantics. */
template <typename T, typename E>
class [[nodiscard]] Result final {
    static_assert(!std::is_void_v<T> && std::is_object_v<T> && std::is_object_v<E>);
    static_assert(!std::is_array_v<T> && !std::is_array_v<E>);
    static_assert(
        std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>);
    static_assert(std::is_nothrow_destructible_v<T> && std::is_nothrow_destructible_v<E>);

public:
    using value_type = T;
    using error_type = E;
    template <typename U = T, std::enable_if_t<std::is_nothrow_copy_constructible_v<U>, int> = 0>
    [[nodiscard]] static Result success(const T& value) noexcept {
        return Result(ValueTag{}, value);
    }
    [[nodiscard]] static Result success(T&& value) noexcept {
        return Result(ValueTag{}, std::move(value));
    }
    template <typename U = E, std::enable_if_t<std::is_nothrow_copy_constructible_v<U>, int> = 0>
    [[nodiscard]] static Result failure(const E& error) noexcept {
        return Result(ErrorTag{}, error);
    }
    [[nodiscard]] static Result failure(E&& error) noexcept {
        return Result(ErrorTag{}, std::move(error));
    }
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result& operator=(Result&&) = delete;
    Result(Result&& other) noexcept : tag_(other.tag_) {
        if (has_value())
            ::new (&storage_.value) T(std::move(other.storage_.value));
        else
            ::new (&storage_.error) E(std::move(other.storage_.error));
    }
    ~Result() noexcept {
        if (has_value())
            storage_.value.~T();
        else
            storage_.error.~E();
    }
    [[nodiscard]] constexpr bool has_value() const noexcept { return tag_ == Tag::Value; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value(); }
    [[nodiscard]] T& value() & noexcept {
        assert(has_value());
        return storage_.value;
    }
    [[nodiscard]] const T& value() const& noexcept {
        assert(has_value());
        return storage_.value;
    }
    [[nodiscard]] T&& value() && noexcept {
        assert(has_value());
        return std::move(storage_.value);
    }
    [[nodiscard]] E& error() & noexcept {
        assert(!has_value());
        return storage_.error;
    }
    [[nodiscard]] const E& error() const& noexcept {
        assert(!has_value());
        return storage_.error;
    }
    [[nodiscard]] E&& error() && noexcept {
        assert(!has_value());
        return std::move(storage_.error);
    }

private:
    enum class Tag : std::uint8_t { Value, Error };
    struct ValueTag {};
    struct ErrorTag {};
    union Storage {
        T value;
        E error;
        Storage() {}
        ~Storage() {}
    } storage_;
    Result(ValueTag, const T& value) noexcept : tag_(Tag::Value) {
        ::new (&storage_.value) T(value);
    }
    Result(ValueTag, T&& value) noexcept : tag_(Tag::Value) {
        ::new (&storage_.value) T(std::move(value));
    }
    Result(ErrorTag, const E& error) noexcept : tag_(Tag::Error) {
        ::new (&storage_.error) E(error);
    }
    Result(ErrorTag, E&& error) noexcept : tag_(Tag::Error) {
        ::new (&storage_.error) E(std::move(error));
    }
    Tag tag_;
};

template <typename E>
class [[nodiscard]] Result<void, E> final {
    static_assert(
        std::is_object_v<E> && !std::is_array_v<E> && std::is_nothrow_move_constructible_v<E> &&
        std::is_nothrow_destructible_v<E>);

public:
    using value_type = void;
    using error_type = E;
    [[nodiscard]] static Result success() noexcept { return Result(true); }
    template <typename U = E, std::enable_if_t<std::is_nothrow_copy_constructible_v<U>, int> = 0>
    [[nodiscard]] static Result failure(const E& error) noexcept {
        return Result(error);
    }
    [[nodiscard]] static Result failure(E&& error) noexcept { return Result(std::move(error)); }
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result& operator=(Result&&) = delete;
    Result(Result&& other) noexcept : success_(other.success_) {
        if (!success_) ::new (&error_) E(std::move(other.error_));
    }
    ~Result() noexcept {
        if (!success_) error_.~E();
    }
    [[nodiscard]] constexpr bool has_value() const noexcept { return success_; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return success_; }
    [[nodiscard]] E& error() & noexcept {
        assert(!success_);
        return error_;
    }
    [[nodiscard]] const E& error() const& noexcept {
        assert(!success_);
        return error_;
    }

private:
    explicit Result(bool success) noexcept : success_(success) {}
    explicit Result(const E& error) noexcept : success_(false) { ::new (&error_) E(error); }
    explicit Result(E&& error) noexcept : success_(false) { ::new (&error_) E(std::move(error)); }
    union {
        unsigned char dummy_;
        E error_;
    };
    bool success_;
};
}  // namespace transmitter_simulator
