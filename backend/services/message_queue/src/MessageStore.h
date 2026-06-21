#pragma once

#include <vector>

#include "ChatMessage.h"

namespace message_queue {

/**
 * @brief Persistence contract used by the queue subscriber.
 */
class MessageStore {
public:
    virtual ~MessageStore() = default;

    /** @brief Persist and return canonical message values. */
    virtual ChatMessage SaveMessage(ChatMessage message) = 0;
    /** @brief Load recipient pending messages (used by compatibility clients). */
    virtual std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) = 0;
    /** @brief Mark recipient message as delivered. */
    virtual void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) = 0;
};

}  // namespace message_queue
