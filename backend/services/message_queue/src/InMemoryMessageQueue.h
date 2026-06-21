#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

#include "MessageQueue.h"

namespace message_queue {

/**
 * @brief Thread-safe in-process queue implementation for worker runtime.
 */
class InMemoryMessageQueue final : public MessageQueue {
public:
    /** @inheritdoc MessageQueue::Publish */
    void Publish(MessageEvent event) override;
    /** @inheritdoc MessageQueue::Pop */
    bool Pop(MessageEvent* event) override;
    /** @inheritdoc MessageQueue::Shutdown */
    void Shutdown() override;

private:
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<MessageEvent> queue_;
    bool running_ = true;
};

}  // namespace message_queue
