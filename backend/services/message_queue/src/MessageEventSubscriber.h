#pragma once

#include <thread>

#include "MessageQueue.h"
#include "MessageStore.h"

namespace message_queue {

/**
 * @brief Background worker that consumes queue events and persists them through MessageStore.
 */
class MessageEventSubscriber {
public:
    /** @brief Starts worker thread immediately. */
    MessageEventSubscriber(MessageQueue& queue, MessageStore& store);
    /** @brief Stops worker thread if still running. */
    ~MessageEventSubscriber();

    MessageEventSubscriber(const MessageEventSubscriber&) = delete;
    MessageEventSubscriber& operator=(const MessageEventSubscriber&) = delete;

    /** @brief Explicitly stop and join worker thread. */
    void Stop();

private:
    void Run();

    MessageQueue& queue_;
    MessageStore& store_;
    std::thread worker_thread_;
    bool started_ = false;
};

}  // namespace message_queue
