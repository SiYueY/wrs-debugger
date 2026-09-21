#ifndef DDSWRAPPER_CLIENT_HPP_
#define DDSWRAPPER_CLIENT_HPP_

#include <atomic>
#include <chrono>
#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include "ddswrapper/context.hpp"
#include "ddswrapper/ddswrapper_api.hpp"
#include "ddswrapper/service.hpp"

namespace ddswrapper {

template <typename ServiceT>
class Client final : public Entity {
public:
    static_assert(
        is_service_traits_v<ServiceT>, "ServiceT must be a ddswrapper::ServiceTraits instance");

    using UniquePtr = std::unique_ptr<Client<ServiceT>>;
    using SharedPtr = std::shared_ptr<Client<ServiceT>>;
    using WeakPtr = std::weak_ptr<Client<ServiceT>>;
    using Request = typename ServiceT::Request;
    using Response = typename ServiceT::Response;
    using RequestPubSubType = typename ServiceT::RequestPubSubType;
    using ResponsePubSubType = typename ServiceT::ResponsePubSubType;
    using SharedRequest = std::shared_ptr<Request>;
    using SharedResponse = std::shared_ptr<Response>;
    using SharedFuture = std::shared_future<SharedResponse>;
    using ResponseCallback = std::function<void(SharedFuture)>;
    Client(
        const std::shared_ptr<Context>& context, CFastDDSDomainParticipant* participant,
        const std::string& service_name) {
        init(context, participant, service_name);
    }
    ~Client() { shutdown(); }

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(Client&&) = delete;

    std::string get_service_name() const { return service_name_; }

    SharedFuture async_send_request(const SharedRequest& request) {
        return async_send_request_impl(request, nullptr).future;
    }

    template <typename CallbackT>
    SharedFuture async_send_request(const SharedRequest& request, CallbackT&& callback) {
        ResponseCallback response_callback(std::forward<CallbackT>(callback));
        return async_send_request_impl(request, std::move(response_callback)).future;
    }

    SharedResponse send_request(const SharedRequest& request) {
        return send_request(request, std::chrono::seconds{3});
    }

    template <typename Rep, typename Period>
    SharedResponse send_request(
        const SharedRequest& request, std::chrono::duration<Rep, Period> timeout) {
        if (ServiceCallback::is_callback_thread()) {
            throw std::logic_error(
                "[ddswrapper::Client] synchronous requests cannot be sent from a service callback");
        }

        const auto result = async_send_request_impl(request, nullptr);
        if (result.future.wait_for(timeout) != std::future_status::ready) {
            if (client_callback_) {
                client_callback_->cancel_pending_request(result.pending, "request timed out");
            }
            throw std::runtime_error("[ddswrapper::Client] request timed out");
        }
        return result.future.get();
    }

    void remove_pending_request() {
        if (client_callback_) {
            client_callback_->cancel_pending_request(
                "request removed before a response was received");
        }
    }

    // Completes the current future with cancellation. It does not cancel the
    // request on DDS; a later response is discarded because it is no longer pending.
    void cancel_pending_request() {
        if (client_callback_) {
            client_callback_->cancel_pending_request("request cancelled");
        }
    }

private:
    void init(
        const std::shared_ptr<Context>& context, CFastDDSDomainParticipant* participant,
        const std::string& service_name) {
        if (service_name.empty()) {
            throw std::invalid_argument("[ddswrapper::Client] invalid service name");
        }
        if (context == nullptr || !context->is_initialized()) {
            throw std::runtime_error("[ddswrapper::Client] invalid context");
        }
        if (participant == nullptr) {
            throw std::runtime_error("[ddswrapper::Client] invalid participant");
        }

        service_name_ = service_name;
        context_ = context;
        client_callback_ = std::make_shared<ClientCallback>();
        auto request_type = std::make_unique<RequestPubSubType>();
        auto response_type = std::make_unique<ResponsePubSubType>();
        ClientInfo client_info{};
        client_info.requestTopicType = request_type.get();
        client_info.responseTopicType = response_type.get();
        client_info.fnCallback = &Client::client_callback;
        client_info.userData = client_callback_.get();
        client_ = createClient(service_name.c_str(), client_info, participant, nullptr);
        // dds_wrapper owns the PubSubType after createClient succeeds.
        request_type.release();
        response_type.release();
        if (client_ == INVALID_CLIENT) {
            throw std::runtime_error("[ddswrapper::Client] failed to create client");
        }
        try {
            context->register_service_callback(client_callback_);
        } catch (...) {
            cleanup();
            throw;
        }

        initialized_.store(true);
    }

    void shutdown() noexcept override {
        if (!initialized_.exchange(false)) {
            return;
        }
        cleanup();
    }

    void cleanup() noexcept {
        if (client_callback_) {
            client_callback_->shutdown_and_wait();
            client_callback_->cancel_pending_request("client closed");
        }

        std::lock_guard<std::mutex> lock(client_mutex_);
        if (client_ != INVALID_CLIENT) {
            removeClient(client_, nullptr);
            client_ = INVALID_CLIENT;
        }
    }

private:
    friend class Node;

    bool is_initialized() const {
        const auto context = context_.lock();
        return initialized_.load() && context && context->is_initialized();
    }

    class ClientCallback final : public ServiceCallback {
    public:
        struct PendingRequest {
            std::shared_ptr<std::promise<SharedResponse>> promise;
            SharedFuture future;
            ResponseCallback callback;
        };

        using PendingRequestPtr = std::shared_ptr<PendingRequest>;

        struct RequestResult {
            SharedFuture future;
            PendingRequestPtr pending;
        };

        RequestResult begin_request(ResponseCallback callback) {
            auto promise = std::make_shared<std::promise<SharedResponse>>();
            const auto future = promise->get_future().share();
            auto pending = std::make_shared<PendingRequest>(
                PendingRequest{promise, future, std::move(callback)});

            std::lock_guard<std::mutex> lock(pending_mutex);
            if (pending_request) {
                promise->set_exception(std::make_exception_ptr(
                    std::runtime_error("only one request may be in flight for a Client")));
                return {future, nullptr};
            }
            pending_request = std::move(pending);
            return {future, pending_request};
        }

        void complete_response(const Response& value) {
            std::shared_ptr<PendingRequest> pending;
            {
                std::lock_guard<std::mutex> lock(pending_mutex);
                pending = std::move(pending_request);
            }
            if (!pending) {
                return;
            }

            pending->promise->set_value(std::make_shared<Response>(value));
            try {
                if (pending->callback) {
                    pending->callback(pending->future);
                }
            } catch (...) {
            }
        }

        bool cancel_pending_request(const PendingRequestPtr& expected_pending, const char* reason) {
            std::shared_ptr<PendingRequest> pending;
            {
                std::lock_guard<std::mutex> lock(pending_mutex);
                if (!pending_request || (expected_pending && pending_request != expected_pending)) {
                    return false;
                }
                pending = std::move(pending_request);
            }
            pending->promise->set_exception(std::make_exception_ptr(std::runtime_error(reason)));
            return true;
        }

        bool cancel_pending_request(const char* reason) {
            return cancel_pending_request(nullptr, reason);
        }

    private:
        std::mutex pending_mutex;
        std::shared_ptr<PendingRequest> pending_request;
    };

    static void client_callback(void* response, void* data) {
        auto* callback_state = static_cast<ClientCallback*>(data);
        if (callback_state == nullptr || response == nullptr) {
            return;
        }
        if (!callback_state->begin_callback()) {
            return;
        }
        try {
            callback_state->complete_response(*static_cast<Response*>(response));
        } catch (...) {
        }
        callback_state->end_callback();
    }

    typename ClientCallback::RequestResult async_send_request_impl(
        const SharedRequest& request, ResponseCallback callback) {
        if (!request || !is_initialized()) {
            auto promise = std::make_shared<std::promise<SharedResponse>>();
            auto future = promise->get_future().share();
            promise->set_exception(
                std::make_exception_ptr(std::runtime_error("client is not initialized")));
            return {future, nullptr};
        }

        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!initialized_.load() || client_ == INVALID_CLIENT || !client_callback_) {
            auto promise = std::make_shared<std::promise<SharedResponse>>();
            auto future = promise->get_future().share();
            promise->set_exception(
                std::make_exception_ptr(std::runtime_error("client is not initialized")));
            return {future, nullptr};
        }

        const auto result = client_callback_->begin_request(std::move(callback));
        if (!result.pending) {
            return result;
        }
        if (!clientInvokeService(client_, request.get())) {
            client_callback_->cancel_pending_request(result.pending, "failed to send request");
        }
        return result;
    }

    std::weak_ptr<Context> context_;
    DdsClientHandle client_{INVALID_CLIENT};
    std::string service_name_;
    std::shared_ptr<ClientCallback> client_callback_;
    mutable std::mutex client_mutex_;
    std::atomic_bool initialized_{false};
};

}  // namespace ddswrapper

#endif  // DDSWRAPPER_CLIENT_HPP_
