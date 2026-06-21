#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

#include "MessageQueue.h"

namespace message_queue {

class InMemoryMessageQueue final : public MessageQueue {
public:
    void Publish(MessageEvent event) override;
    bool Pop(MessageEvent* event) override;
    void Shutdown() override;

private:
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<MessageEvent> queue_;
    bool running_ = true;
};

}  // namespace message_queue
