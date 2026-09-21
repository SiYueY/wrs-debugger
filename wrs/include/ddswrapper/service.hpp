#ifndef DDSWRAPPER_SERVICE_HPP_
#define DDSWRAPPER_SERVICE_HPP_

#include <algorithm>
#include <condition_variable>
#include <iterator>
#include <mutex>
#include <type_traits>
#include <vector>

namespace ddswrapper {

#define DDSWRAPPER_SERVICE(service_namespace, service_name)                                      \
    ::ddswrapper::ServiceTraits<                                                                 \
        service_namespace::service_name##_Request_, service_namespace::service_name##_Response_, \
        service_namespace::service_name##_Request_PubSubType,                                    \
        service_namespace::service_name##_Response_PubSubType>

template <
    typename RequestT, typename ResponseT, typename RequestPubSubTypeT,
    typename ResponsePubSubTypeT>
struct ServiceTraits {
    using Request = RequestT;
    using Response = ResponseT;
    using RequestPubSubType = RequestPubSubTypeT;
    using ResponsePubSubType = ResponsePubSubTypeT;
};

template <typename T>
struct is_service_traits : std::false_type {};

template <
    typename RequestT, typename ResponseT, typename RequestPubSubTypeT,
    typename ResponsePubSubTypeT>
struct is_service_traits<
    ServiceTraits<RequestT, ResponseT, RequestPubSubTypeT, ResponsePubSubTypeT>> : std::true_type {
};

template <typename T>
inline constexpr bool is_service_traits_v = is_service_traits<T>::value;

class ServiceCallback {
public:
    virtual ~ServiceCallback() = default;

    bool begin_callback() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!accept_callback_) {
            return false;
        }
        active_callbacks_.push_back(this);
        ++active_callback_count_;
        ++callback_count_;
        return true;
    }

    void end_callback() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --active_callback_count_;
            condition_.notify_all();
        }
        const auto callback = std::find(active_callbacks_.rbegin(), active_callbacks_.rend(), this);
        if (callback != active_callbacks_.rend()) {
            active_callbacks_.erase(std::next(callback).base());
        }
        --callback_count_;
    }

    void shutdown_and_wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        accept_callback_ = false;
        const auto callback_count = static_cast<unsigned int>(
            std::count(active_callbacks_.begin(), active_callbacks_.end(), this));
        condition_.wait(
            lock, [this, callback_count] { return active_callback_count_ <= callback_count; });
    }

    static bool is_callback_thread() { return callback_count_ != 0; }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool accept_callback_{true};
    unsigned int active_callback_count_{0};

    // Thread local
    inline static thread_local unsigned int callback_count_{0};
    using ServiceCallbackStack = std::vector<const ServiceCallback*>;
    inline static thread_local ServiceCallbackStack active_callbacks_;
};

}  // namespace ddswrapper

#endif  // DDSWRAPPER_SERVICE_HPP_
