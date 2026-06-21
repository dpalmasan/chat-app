#pragma once

#include <chrono>
#include <vector>

#include "ChatMessage.h"

namespace chat {

/**
 * @brief Abstraction for message persistence and recipient history state.
 */
class MessageStore {
public:
    struct UserPresence {
        UserId user_id;
        bool is_online = false;
        std::chrono::system_clock::time_point last_active_at{};
    };

    virtual ~MessageStore() = default;

    /** @brief Persist and return canonical message values. */
    virtual ChatMessage SaveMessage(ChatMessage message) = 0;
    /** @brief Return undelivered messages for a recipient in delivery order. */
    virtual std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) = 0;
    /** @brief Mark one message as delivered for a recipient. */
    virtual void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) = 0;
    /** @brief Check whether user exists in backend datastore. */
    virtual bool UserExists(const UserId& user_id) = 0;
    /** @brief Update user online/offline presence in backend datastore. */
    virtual void SetPresence(const UserId& user_id, bool is_online) = 0;
    /** @brief Return all users with current presence state. */
    virtual std::vector<UserPresence> ListUsers() = 0;
};

}  // namespace chat
