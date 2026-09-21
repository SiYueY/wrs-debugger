#ifndef DDSWRAPPER_SUBSCRIBER_HPP_
#define DDSWRAPPER_SUBSCRIBER_HPP_

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

#include "ddswrapper/ddswrapper_api.hpp"
#include "ddswrapper/context.hpp"

namespace ddswrapper {

template <typename MessageT>
class Subscriber final : public Entity {
public:
    using UniquePtr = std::unique_ptr<Subscriber<MessageT>>;
    using SharedPtr = std::shared_ptr<Subscriber<MessageT>>;
    using WeakPtr = std::weak_ptr<Subscriber<MessageT>>;

    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;
    Subscriber(Subscriber&&) = delete;
    Subscriber& operator=(Subscriber&&) = delete;

    template <typename MessagePubSubTypeT>
    Subscriber(
        const std::shared_ptr<Context>& context, CFastDDSDomainParticipant* participant,
        const std::string& topic_name, std::unique_ptr<MessagePubSubTypeT> topic_type) {
        init(context, participant, topic_name, std::move(topic_type));
    }

    ~Subscriber() { shutdown(); }

    bool read(MessageT& message, ReturnCode* return_code = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_initialized()) {
            return false;
        }
        return topic_->recvSubscription(&message, return_code);
    }

    std::string get_topic_name() const { return topic_name_; }

    int get_publisher_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_initialized()) {
            return 0;
        }
        return topic_->getPubAliveCount();
    }

private:
    friend class Node;

    template <typename MessagePubSubTypeT>
    void init(
        const std::shared_ptr<Context>& context, CFastDDSDomainParticipant* participant,
        const std::string& topic_name, std::unique_ptr<MessagePubSubTypeT> topic_type) {
        if (topic_name.empty()) {
            throw std::invalid_argument("[ddswrapper::Subscriber] invalid topic name");
        }
        if (topic_type == nullptr) {
            throw std::invalid_argument("[ddswrapper::Subscriber] invalid topic type");
        }
        if (context == nullptr || !context->is_initialized()) {
            throw std::runtime_error("[ddswrapper::Subscriber] invalid context");
        }
        if (participant == nullptr) {
            throw std::runtime_error("[ddswrapper::Subscriber] invalid participant");
        }

        context_ = context;
        topic_name_ = topic_name;
        CAbstractTopic* const topic =
            createTopic(topic_name.c_str(), TOPIC_SUBSCRIBER, topic_type.get(), participant);
        // dds_wrapper owns the PubSubType after createTopic succeeds.
        topic_type.release();
        if (topic == nullptr) {
            throw std::runtime_error("[ddswrapper::Subscriber] failed to create subscriber");
        }

        topic_.reset(topic);

        initialized_.store(true);
    }

    void shutdown() noexcept override {
        if (!initialized_.exchange(false)) {
            return;
        }
        cleanup();
    }

    void cleanup() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        topic_.reset();
    }

    bool is_initialized() const {
        const auto context = context_.lock();
        return initialized_.load() && context && context->is_initialized();
    }

    std::weak_ptr<Context> context_;
    std::string topic_name_;
    std::unique_ptr<CAbstractTopic> topic_;
    mutable std::mutex mutex_;
    std::atomic_bool initialized_{false};
};

}  // namespace ddswrapper

#endif  // DDSWRAPPER_SUBSCRIBER_HPP_
