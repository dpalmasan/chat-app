#pragma once

#include "ChatMessage.h"

namespace chat {

class MessagePublisher {
public:
    virtual ~MessagePublisher() = default;
    virtual ChatMessage PublishMessage(const ChatMessage& message) = 0;
};

}  // namespace chat
