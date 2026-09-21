#ifndef DDSWRAPPER_NODE_HPP_
#define DDSWRAPPER_NODE_HPP_

#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ddswrapper/context.hpp"
#include "ddswrapper/ddswrapper_api.hpp"
#include "ddswrapper/publisher.hpp"
#include "ddswrapper/subscriber.hpp"
#include "ddswrapper/server.hpp"
#include "ddswrapper/client.hpp"

namespace ddswrapper {

class Node final : public Entity {
public:
    using UniquePtr = std::unique_ptr<Node>;
    using SharedPtr = std::shared_ptr<Node>;
    using WeakPtr = std::weak_ptr<Node>;

    // A Node owns exactly one participant. Its publishers, subscribers, clients,
    // and services are always created with that participant, never the legacy
    // default participant maintained by ddsWrapperInit().
    Node() : context_(get_global_default_context()) { init(); }

    ~Node() { shutdown(); }

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    template <typename MessageT, typename MessagePubSubTypeT>
    typename Publisher<MessageT>::SharedPtr create_publisher(const std::string& topic_name) {
        return create_entity<Publisher<MessageT>>([&topic_name, this] {
            return std::shared_ptr<Publisher<MessageT>>(new Publisher<MessageT>(
                context_, participant_, topic_name, std::make_unique<MessagePubSubTypeT>()));
        });
    }

    template <typename MessageT, typename MessagePubSubTypeT>
    typename Subscriber<MessageT>::SharedPtr create_subscriber(const std::string& topic_name) {
        return create_entity<Subscriber<MessageT>>([&topic_name, this] {
            return std::shared_ptr<Subscriber<MessageT>>(new Subscriber<MessageT>(
                context_, participant_, topic_name, std::make_unique<MessagePubSubTypeT>()));
        });
    }

    template <typename ServiceT>
    typename Client<ServiceT>::SharedPtr create_client(const std::string& service_name) {
        static_assert(
            is_service_traits_v<ServiceT>, "ServiceT must be ddswrapper::ServiceTraits instance");
        using ClientT = Client<ServiceT>;
        return create_entity<ClientT>([&service_name, this] {
            return std::shared_ptr<ClientT>(new ClientT(context_, participant_, service_name));
        });
    }

    template <typename ServiceT, typename CallbackT>
    typename Server<ServiceT>::SharedPtr create_service(
        const std::string& service_name, CallbackT&& service_callback) {
        static_assert(
            is_service_traits_v<ServiceT>, "ServiceT must be ddswrapper::ServiceTraits instance");
        using ServerT = Server<ServiceT>;
        typename ServerT::Callback callback(std::forward<CallbackT>(service_callback));
        return create_entity<ServerT>(
            [&service_name, callback = std::move(callback), this]() mutable {
                return std::shared_ptr<ServerT>(
                    new ServerT(context_, participant_, service_name, std::move(callback)));
            });
    }

private:
    void init() {
        if (!context_ || !context_->is_initialized()) {
            throw std::runtime_error("[ddswrapper::Node] invalid context");
        }

        participant_ = createDomainParticipant(context_->profile_path().c_str());
        if (!participant_) {
            throw std::runtime_error("[ddswrapper::Node] failed to create participant");
        }

        try {
            context_->register_node(this, [this] { initialized_.store(true); });
        } catch (...) {
            cleanup();
            throw;
        }
    }

    bool is_initialized() const {
        return initialized_.load() && context_ && context_->is_initialized();
    }

    void shutdown() noexcept override {
        if (!initialized_.exchange(false)) {
            return;
        }
        cleanup();
    }

    void cleanup() noexcept {
        if (context_) {
            context_->unregister_node(this);
        }
        destroy_entities();
        if (participant_) {
            // TODO: destroy_participant();
            participant_ = nullptr;
        }
    }

    template <typename EntityT, typename FactoryT>
    std::shared_ptr<EntityT> create_entity(FactoryT&& factory) {
        std::lock_guard<std::mutex> lock(entities_mutex_);
        if (!is_initialized()) {
            return nullptr;
        }

        std::shared_ptr<EntityT> entity;
        try {
            entity = factory();
            if (!entity) {
                return nullptr;
            }
            entities_.push_back(entity);
        } catch (...) {
            if (entity) {
                destroy_entity(std::weak_ptr<Entity>(entity));
            }
            return nullptr;
        }
        return entity;
    }

    void destroy_entity(const std::weak_ptr<Entity>& weak_entity) {
        if (const auto entity = weak_entity.lock()) {
            entity->shutdown();
        }
    }

    void destroy_entities() noexcept {
        std::vector<std::weak_ptr<Entity>> entities;
        {
            std::lock_guard<std::mutex> lock(entities_mutex_);
            entities.swap(entities_);
        }
        for (const auto& weak_entity : entities) {
            destroy_entity(weak_entity);
        }
    }

    std::shared_ptr<Context> context_;
    CFastDDSDomainParticipant* participant_{nullptr};
    std::vector<std::weak_ptr<Entity>> entities_;
    mutable std::mutex entities_mutex_;
    std::atomic_bool initialized_{false};
};

}  // namespace ddswrapper

#endif  // DDSWRAPPER_NODE_HPP_
