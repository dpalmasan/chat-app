#pragma once

#include <vector>

#include "ChatMessage.h"

namespace message_queue {

class MessageStore {
public:
    virtual ~MessageStore() = default;
    virtual ChatMessage SaveMessage(ChatMessage message) = 0;
    virtual std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) = 0;
    virtual void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) = 0;
};

}  // namespace message_queue
