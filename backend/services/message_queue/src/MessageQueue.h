#pragma once

#include <future>
#include <memory>

#include "ChatMessage.h"

namespace message_queue {

/**
 * @brief Unit of work published to the internal queue.
 *
 * `completion` is fulfilled by the subscriber after persistence completes.
 */
struct MessageEvent {
    ChatMessage message;
    std::shared_ptr<std::promise<ChatMessage>> completion;
};

/**
 * @brief Abstract synchronous queue used by worker server and subscriber.
 */
class MessageQueue {
public:
    virtual ~MessageQueue() = default;

    /** @brief Publish one event into the queue. */
    virtual void Publish(MessageEvent event) = 0;
    /** @brief Pop one event, blocking while queue is empty. Returns false on shutdown. */
    virtual bool Pop(MessageEvent* event) = 0;
    /** @brief Stop queue processing and wake blocked consumers. */
    virtual void Shutdown() = 0;
};

}  // namespace message_queue
