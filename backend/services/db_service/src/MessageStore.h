#pragma once

#include <vector>

#include "ChatMessage.h"

namespace db_service {

/**
 * @brief Storage abstraction used by db_service API handlers.
 */
class MessageStore {
public:
    virtual ~MessageStore() = default;
    /** @brief Persist and return canonical message values. */
    virtual ChatMessage SaveMessage(ChatMessage message) = 0;
    /** @brief Load recipient pending messages in delivery order. */
    virtual std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) = 0;
    /** @brief Mark one message as delivered for recipient. */
    virtual void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) = 0;
};

}  // namespace db_service
