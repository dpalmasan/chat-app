#include "InMemoryMessageQueue.h"

#include <stdexcept>
#include <utility>

namespace message_queue {

void InMemoryMessageQueue::Publish(MessageEvent event) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (!running_) {
        throw std::runtime_error("message queue is shut down");
    }

    queue_.push_back(std::move(event));
    queue_cv_.notify_one();
}

bool InMemoryMessageQueue::Pop(MessageEvent* event) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this]() {
        return !running_ || !queue_.empty();
    });

    if (queue_.empty()) {
        return false;
    }

    *event = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

void InMemoryMessageQueue::Shutdown() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    running_ = false;
    queue_cv_.notify_all();
}

}  // namespace message_queue
