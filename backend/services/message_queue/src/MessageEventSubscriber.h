#pragma once

#include <thread>

#include "MessageQueue.h"
#include "MessageStore.h"

namespace message_queue {

class MessageEventSubscriber {
public:
    MessageEventSubscriber(MessageQueue& queue, MessageStore& store);
    ~MessageEventSubscriber();

    MessageEventSubscriber(const MessageEventSubscriber&) = delete;
    MessageEventSubscriber& operator=(const MessageEventSubscriber&) = delete;

    void Stop();

private:
    void Run();

    MessageQueue& queue_;
    MessageStore& store_;
    std::thread worker_thread_;
    bool started_ = false;
};

}  // namespace message_queue
