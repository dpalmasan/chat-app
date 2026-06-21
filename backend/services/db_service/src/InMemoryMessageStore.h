#pragma once

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "MessageStore.h"

namespace db_service {

/**
 * @brief In-process datastore implementation for db_service.
 *
 * Designed as a prototype backing store that can later be replaced by a
 * production adapter (for example MongoDB) behind the same MessageStore API.
 */
class InMemoryMessageStore final : public MessageStore {
public:
    InMemoryMessageStore();

    /** @inheritdoc MessageStore::SaveMessage */
    ChatMessage SaveMessage(ChatMessage message) override;
    /** @inheritdoc MessageStore::LoadPendingMessagesFor */
    std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) override;
    /** @inheritdoc MessageStore::LoadHistoryFor */
    std::vector<ChatMessage> LoadHistoryFor(const UserId& user_id) override;
    /** @inheritdoc MessageStore::MarkMessageDelivered */
    void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) override;
    /** @inheritdoc MessageStore::LoginUser */
    UserPresence LoginUser(const UserId& user_id) override;
    /** @inheritdoc MessageStore::UserExists */
    bool UserExists(const UserId& user_id) override;
    /** @inheritdoc MessageStore::SetPresence */
    void SetPresence(const UserId& user_id, bool is_online) override;
    /** @inheritdoc MessageStore::ListUsers */
    std::vector<UserPresence> ListUsers() override;

private:
    using PresenceByUser = std::unordered_map<UserId, UserPresence>;

    PresenceByUser::iterator RequireUser(const UserId& user_id);

    std::mutex messages_mutex_;
    MessageId next_message_id_ = 1;
    std::vector<ChatMessage> messages_;
    std::unordered_map<UserId, std::unordered_set<MessageId>> delivered_message_ids_;
    PresenceByUser users_;
};

}  // namespace db_service
