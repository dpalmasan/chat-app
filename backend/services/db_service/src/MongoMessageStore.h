#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include "MessageStore.h"

namespace db_service {

/**
 * @brief MongoDB-backed datastore for db_service.
 */
class MongoMessageStore final : public MessageStore {
public:
    MongoMessageStore(std::string mongo_uri, std::string database_name);
    ~MongoMessageStore() override;

    MongoMessageStore(const MongoMessageStore&) = delete;
    MongoMessageStore& operator=(const MongoMessageStore&) = delete;
    MongoMessageStore(MongoMessageStore&&) = delete;
    MongoMessageStore& operator=(MongoMessageStore&&) = delete;

    ChatMessage SaveMessage(ChatMessage message) override;
    std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) override;
    std::vector<ChatMessage> LoadHistoryFor(const UserId& user_id) override;
    void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) override;
    UserPresence LoginUser(const UserId& user_id) override;
    bool UserExists(const UserId& user_id) override;
    void SetPresence(const UserId& user_id, bool is_online) override;
    std::vector<UserPresence> ListUsers() override;

private:
    using StoreLock = std::lock_guard<std::recursive_mutex>;

    void SeedUsersIfNeeded();
    MessageId NextMessageId();
    UserPresence LoadUserOrThrow(const UserId& user_id);
    bool UserExistsUnlocked(const UserId& user_id);

    std::string mongo_uri_;
    std::string database_name_;
    std::recursive_mutex store_mutex_;
    mongoc_client_t* client_ = nullptr;
    mongoc_database_t* database_ = nullptr;
    mongoc_collection_t* messages_collection_ = nullptr;
    mongoc_collection_t* users_collection_ = nullptr;
};

}  // namespace db_service
