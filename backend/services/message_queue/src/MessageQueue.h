#pragma once

#include <future>
#include <memory>

#include "ChatMessage.h"

namespace message_queue {

struct MessageEvent {
    ChatMessage message;
    std::shared_ptr<std::promise<ChatMessage>> completion;
};

class MessageQueue {
public:
    virtual ~MessageQueue() = default;

    virtual void Publish(MessageEvent event) = 0;
    virtual bool Pop(MessageEvent* event) = 0;
    virtual void Shutdown() = 0;
};

}  // namespace message_queue
