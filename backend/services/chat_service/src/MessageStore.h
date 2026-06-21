#pragma once

#include "ChatMessage.h"

namespace chat {

class MessageStore {
public:
    virtual ~MessageStore() = default;
    virtual void SaveMessage(const ChatMessage& message) = 0;
};

}  // namespace chat
