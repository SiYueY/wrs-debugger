#ifndef DDSWRAPPER_PUBLISHER_HPP_
#define DDSWRAPPER_PUBLISHER_HPP_

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

#include "ddswrapper/ddswrapper_api.hpp"
#include "ddswrapper/context.hpp"

namespace ddswrapper {

template <typename MessageT>
class Publisher final : public Entity {
public:
    using UniquePtr = std::unique_ptr<Publisher<MessageT>>;
    using SharedPtr = std::shared_ptr<Publisher<MessageT>>;
    using WeakPtr = std::weak_ptr<Publisher<MessageT>>;

    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;
    Publisher(Publisher&&) = delete;
    Publisher& operator=(Publisher&&) = delete;

    template <typename MessagePubSubTypeT>
    Publisher(
        const std::shared_ptr<Context>& context, CFastDDSDomainParticipant* participant,
        const std::string& topic_name, std::unique_ptr<MessagePubSubTypeT> topic_type) {
        init(context, participant, topic_name, std::move(topic_type));
    }

    ~Publisher() { shutdown(); }

    bool write(const MessageT& message, ReturnCode* return_code = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_initialized()) {
            return false;
        }
        return topic_->sendPublication(&message, return_code);
    }

    std::string get_topic_name() const { return topic_name_; }

    int get_subscriber_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_initialized()) {
            return 0;
        }
        return topic_->getSubAliveCount();
    }

private:
    friend class Node;

    template <typename MessagePubSubTypeT>
    void init(
        const std::shared_ptr<Context>& context, CFastDDSDomainParticipant* participant,
        const std::string& topic_name, std::unique_ptr<MessagePubSubTypeT> topic_type) {
        if (topic_name.empty()) {
            throw std::invalid_argument("[ddswrapper::Publisher] invalid topic name");
        }
        if (topic_type == nullptr) {
            throw std::invalid_argument("[ddswrapper::Publisher] invalid topic type");
        }
        if (context == nullptr || !context->is_initialized()) {
            throw std::runtime_error("[ddswrapper::Publisher] invalid context");
        }
        if (participant == nullptr) {
            throw std::runtime_error("[ddswrapper::Publisher] invalid participant");
        }

        context_ = context;
        topic_name_ = topic_name;
        CAbstractTopic* const topic =
            createTopic(topic_name.c_str(), TOPIC_PUBLISHER, topic_type.get(), participant);
        // dds_wrapper owns the PubSubType after createTopic succeeds.
        topic_type.release();
        if (topic == nullptr) {
            throw std::runtime_error("[ddswrapper::Publisher] failed to create publisher");
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
    std::atomic<bool> initialized_{false};
};

}  // namespace ddswrapper

#endif  // DDSWRAPPER_PUBLISHER_HPP_
