#pragma once

#include <chrono>
#include <vector>

#include "ChatMessage.h"

namespace db_service {

/**
 * @brief Storage abstraction used by db_service API handlers.
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
    /** @brief Load recipient pending messages in delivery order. */
    virtual std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) = 0;
    /** @brief Load full message history where user is sender or recipient. */
    virtual std::vector<ChatMessage> LoadHistoryFor(const UserId& user_id) = 0;
    /** @brief Mark one message as delivered for recipient. */
    virtual void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) = 0;
    /** @brief Simulate auth login handoff and mark user online. */
    virtual UserPresence LoginUser(const UserId& user_id) = 0;
    /** @brief Check whether user exists in the datastore. */
    virtual bool UserExists(const UserId& user_id) = 0;
    /** @brief Update user online/offline presence and activity timestamp. */
    virtual void SetPresence(const UserId& user_id, bool is_online) = 0;
    /** @brief Return all users with current presence information. */
    virtual std::vector<UserPresence> ListUsers() = 0;
};

}  // namespace db_service
