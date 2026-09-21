#ifndef DDSWRAPPER_SERVER_HPP_
#define DDSWRAPPER_SERVER_HPP_

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include "ddswrapper/context.hpp"
#include "ddswrapper/ddswrapper_api.hpp"
#include "ddswrapper/service.hpp"

namespace ddswrapper {

template <typename ServiceT>
class Server final : public Entity {
public:
    static_assert(
        is_service_traits_v<ServiceT>, "ServiceT must be ddswrapper::ServiceTraits instance");

    using UniquePtr = std::unique_ptr<Server<ServiceT>>;
    using SharedPtr = std::shared_ptr<Server<ServiceT>>;
    using WeakPtr = std::weak_ptr<Server<ServiceT>>;
    using Request = typename ServiceT::Request;
    using Response = typename ServiceT::Response;
    using RequestPubSubType = typename ServiceT::RequestPubSubType;
    using ResponsePubSubType = typename ServiceT::ResponsePubSubType;
    using Callback = std::function<void(const Request&, Response&)>;

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    Server(
        const std::shared_ptr<Context>& context, CFastDDSDomainParticipant* participant,
        const std::string& service_name, Callback callback) {
        init(context, participant, service_name, std::move(callback));
    }
    ~Server() { shutdown(); }

    std::string get_service_name() const { return service_name_; }

private:
    void init(
        const std::shared_ptr<Context>& context, CFastDDSDomainParticipant* participant,
        const std::string& service_name, Callback callback) {
        if (service_name.empty()) {
            throw std::invalid_argument("[ddswrapper::Server] invalid service name");
        }
        if (!callback) {
            throw std::invalid_argument("[ddswrapper::Server] invalid callback");
        }
        if (context == nullptr || !context->is_initialized()) {
            throw std::runtime_error("[ddswrapper::Server] invalid context");
        }
        if (participant == nullptr) {
            throw std::runtime_error("[ddswrapper::Server] invalid participant");
        }

        context_ = context;
        service_name_ = service_name;
        server_callback_ = std::make_shared<ServerCallback>(std::move(callback));
        auto request_type = std::make_unique<RequestPubSubType>();
        auto response_type = std::make_unique<ResponsePubSubType>();
        ServiceInfo service_info{};
        service_info.requestTopicType = request_type.get();
        service_info.responseTopicType = response_type.get();
        service_info.userData = server_callback_.get();
        const bool success = createService(
            service_name.c_str(), service_info, reinterpret_cast<void*>(&Server::server_callback),
            participant, nullptr);
        // dds_wrapper owns the PubSubType after createService succeeds.
        request_type.release();
        response_type.release();
        if (!success) {
            throw std::runtime_error("[ddswrapper::Server] failed to create server");
        }
        try {
            context->register_service_callback(server_callback_);
        } catch (...) {
            cleanup();
            throw;
        }

        initialized_.store(true);
    }

    bool is_initialized() const {
        const auto context = context_.lock();
        return initialized_.load() && context && context->is_initialized();
    }

    void shutdown() noexcept override {
        if (!initialized_.exchange(false)) {
            return;
        }
        cleanup();
    }

    void cleanup() noexcept {
        if (server_callback_) {
            server_callback_->shutdown_and_wait();
            server_callback_->clear_callback();
        }
        if (!service_name_.empty()) {
            removeService(service_name_.c_str(), nullptr);
        }
    }

private:
    friend class Node;

    class ServerCallback final : public ServiceCallback {
    public:
        explicit ServerCallback(Callback callback)
        : callback_(std::make_shared<Callback>(std::move(callback))) {}

        std::shared_ptr<const Callback> get_callback() const {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            return callback_;
        }

        void clear_callback() noexcept {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            callback_.reset();
        }

    private:
        mutable std::mutex callback_mutex_;
        std::shared_ptr<const Callback> callback_;
    };

    static void server_callback(void* request, void* response, void* data) {
        auto* server_callback = static_cast<ServerCallback*>(data);
        if (server_callback == nullptr || request == nullptr || response == nullptr) {
            return;
        }
        if (!server_callback->begin_callback()) {
            return;
        }
        try {
            const auto callback = server_callback->get_callback();
            if (callback) {
                (*callback)(
                    *static_cast<const Request*>(request), *static_cast<Response*>(response));
            }
        } catch (...) {
        }
        server_callback->end_callback();
    }

    std::weak_ptr<Context> context_;
    std::string service_name_;
    std::shared_ptr<ServerCallback> server_callback_;
    std::atomic_bool initialized_{false};
};

}  // namespace ddswrapper

#endif  // DDSWRAPPER_SERVER_HPP_
