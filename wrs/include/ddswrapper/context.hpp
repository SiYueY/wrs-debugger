#ifndef DDSWRAPPER_CONTEXT_HPP_
#define DDSWRAPPER_CONTEXT_HPP_

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ddswrapper/ddswrapper_api.hpp"
#include "ddswrapper/entity.hpp"
#include "ddswrapper/service.hpp"

namespace ddswrapper {

bool init(const std::string& qos_profile);
bool ok();
void shutdown();

class Node;
template <typename MessageT>
class Publisher;
template <typename MessageT>
class Subscriber;
template <typename ServiceT>
class Client;
template <typename ServiceT>
class Server;

class Context final {
private:
    explicit Context(std::string profile_path) : profile_path_(std::move(profile_path)) {}

    void init() {
        if (!ddsWrapperInit(profile_path_.c_str())) {
            throw std::runtime_error("[ddswrapper::Context] failed to initialize DDS runtime");
        }
        initialized_.store(true);
    }

    void shutdown() {
        initialized_.store(false);
        std::vector<Entity*> nodes;
        {
            std::lock_guard<std::recursive_mutex> lock(node_mutex_);
            nodes.swap(nodes_);
            for (auto* node : nodes) {
                node->shutdown();
            }
        }
        ddsWrapperUninit();
        unregister_service_callbacks();
    }

    bool is_initialized() const { return initialized_.load(); }

    const std::string& profile_path() const { return profile_path_; }

    void register_service_callback(const std::shared_ptr<ServiceCallback>& callback) {
        std::lock_guard<std::mutex> lock(service_callback_mutex_);
        service_callbacks_.push_back(callback);
    }

    template <typename CallbackT>
    void register_node(Entity* node, CallbackT&& on_registered) {
        std::lock_guard<std::recursive_mutex> lock(node_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("[ddswrapper::Context] cannot register node after shutdown");
        }
        nodes_.push_back(node);
        try {
            std::forward<CallbackT>(on_registered)();
        } catch (...) {
            nodes_.pop_back();
            throw;
        }
    }

    void unregister_service_callbacks() {
        std::lock_guard<std::mutex> lock(service_callback_mutex_);
        service_callbacks_.clear();
    }

    void unregister_node(const Entity* node) {
        std::lock_guard<std::recursive_mutex> lock(node_mutex_);
        nodes_.erase(std::remove(nodes_.begin(), nodes_.end(), node), nodes_.end());
    }

    // 全局上下文
    friend bool init(const std::string& qos_profile);
    friend bool ok();
    friend void shutdown();

    friend class Node;
    template <typename MessageT>
    friend class Publisher;
    template <typename MessageT>
    friend class Subscriber;
    template <typename ServiceT>
    friend class Client;
    template <typename ServiceT>
    friend class Server;

    // 初始化标志
    std::atomic<bool> initialized_{false};
    // Qos 配置文件
    const std::string profile_path_;
    // Service Callback
    std::mutex service_callback_mutex_;
    std::vector<std::shared_ptr<ServiceCallback>> service_callbacks_;
    // Node
    std::recursive_mutex node_mutex_;
    std::vector<Entity*> nodes_;
};

inline std::mutex context_mutex;
inline std::shared_ptr<Context> global_context;

inline std::shared_ptr<Context> get_global_default_context() {
    std::lock_guard<std::mutex> lock(context_mutex);
    return global_context;
}

inline bool init(const std::string& qos_profile) {
    if (qos_profile.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(context_mutex);
    if (global_context) {
        return false;
    }

    // The legacy C API needs its default participant to run the shared executor.
    // ddswrapper::Node never uses that participant: every Node creates and owns one
    // explicit participant, and all entities created by that Node use it.
    try {
        auto context = std::shared_ptr<Context>(new Context(qos_profile));
        context->init();
        global_context = std::move(context);
    } catch (...) {
        return false;
    }
    return true;
}

inline bool ok() {
    const auto context = get_global_default_context();
    return context && context->is_initialized();
}

inline void shutdown() {
    // ddsWrapperUninit() joins executor threads. Calling it from an executor
    // callback would therefore join the current thread. Defer teardown until
    // this callback has returned.
    if (ServiceCallback::is_callback_thread()) {
        std::thread([] { shutdown(); }).detach();
        return;
    }

    std::lock_guard<std::mutex> lock(context_mutex);
    if (!global_context) {
        return;
    }

    const auto context = global_context;
    context->shutdown();
    global_context.reset();
}

}  // namespace ddswrapper

#endif  // DDSWRAPPER_CONTEXT_HPP_
