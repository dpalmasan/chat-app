#pragma once

#include "ChatMessage.h"

namespace chat {

class MessageStore {
public:
    virtual ~MessageStore() = default;
    virtual ChatMessage SaveMessage(ChatMessage message) = 0;
};

}  // namespace chat
