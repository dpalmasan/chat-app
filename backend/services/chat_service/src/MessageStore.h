#pragma once

#include <vector>

#include "ChatMessage.h"

namespace chat {

/**
 * @brief Abstraction for message persistence and recipient history state.
 */
class MessageStore {
public:
    virtual ~MessageStore() = default;

    /** @brief Persist and return canonical message values. */
    virtual ChatMessage SaveMessage(ChatMessage message) = 0;
    /** @brief Return undelivered messages for a recipient in delivery order. */
    virtual std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) = 0;
    /** @brief Mark one message as delivered for a recipient. */
    virtual void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) = 0;
};

}  // namespace chat
