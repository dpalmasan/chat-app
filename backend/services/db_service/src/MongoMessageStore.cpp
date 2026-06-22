#include "MongoMessageStore.h"

#include <chrono>
#include <memory>
#include <stdexcept>

namespace db_service {
namespace {

std::chrono::system_clock::time_point EpochMsToTimePoint(std::int64_t epoch_ms) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(epoch_ms));
}

std::int64_t ToEpochMs(const std::chrono::system_clock::time_point& time_point) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count();
}

std::string GetUtf8OrThrow(const bson_t* doc, const char* key) {
    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, doc, key) || !BSON_ITER_HOLDS_UTF8(&iter)) {
        throw std::runtime_error(std::string("missing or invalid utf8 field: ") + key);
    }
    return bson_iter_utf8(&iter, nullptr);
}

bool GetBoolOrDefault(const bson_t* doc, const char* key, bool default_value) {
    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, doc, key) || !BSON_ITER_HOLDS_BOOL(&iter)) {
        return default_value;
    }
    return bson_iter_bool(&iter);
}

std::int64_t GetInt64OrThrow(const bson_t* doc, const char* key) {
    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, doc, key)) {
        throw std::runtime_error(std::string("missing int field: ") + key);
    }
    if (BSON_ITER_HOLDS_INT64(&iter)) {
        return bson_iter_int64(&iter);
    }
    if (BSON_ITER_HOLDS_INT32(&iter)) {
        return bson_iter_int32(&iter);
    }
    throw std::runtime_error(std::string("invalid int field: ") + key);
}

std::int64_t GetInt64OrDefault(const bson_t* doc, const char* key, std::int64_t default_value) {
    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, doc, key)) {
        return default_value;
    }
    if (BSON_ITER_HOLDS_INT64(&iter)) {
        return bson_iter_int64(&iter);
    }
    if (BSON_ITER_HOLDS_INT32(&iter)) {
        return bson_iter_int32(&iter);
    }
    return default_value;
}

ChatMessage ParseMessage(const bson_t* doc) {
    ChatMessage message;
    message.message_id = static_cast<MessageId>(GetInt64OrThrow(doc, "message_id"));
    message.message_from = GetUtf8OrThrow(doc, "message_from");
    message.message_to = GetUtf8OrThrow(doc, "message_to");
    message.content = GetUtf8OrThrow(doc, "content");
    message.created_at = EpochMsToTimePoint(GetInt64OrThrow(doc, "created_at_ms"));
    return message;
}

MessageStore::UserPresence ParseUserPresence(const bson_t* doc) {
    MessageStore::UserPresence user;
    user.user_id = GetUtf8OrThrow(doc, "user_id");
    user.is_online = GetBoolOrDefault(doc, "is_online", false);
    user.last_active_at = EpochMsToTimePoint(GetInt64OrDefault(doc, "last_active_at_ms", 0));
    return user;
}

void ThrowOnMongoError(bool ok, const bson_error_t& error, const char* operation) {
    if (!ok) {
        throw std::runtime_error(std::string(operation) + " failed: " + error.message);
    }
}

}  // namespace

MongoMessageStore::MongoMessageStore(std::string mongo_uri, std::string database_name)
    : mongo_uri_(std::move(mongo_uri)),
      database_name_(std::move(database_name)) {
    mongoc_init();

    client_.reset(mongoc_client_new(mongo_uri_.c_str()));
    if (client_ == nullptr) {
        mongoc_cleanup();
        throw std::runtime_error("failed to create MongoDB client");
    }

    database_.reset(mongoc_client_get_database(client_.get(), database_name_.c_str()));
    messages_collection_.reset(
        mongoc_client_get_collection(client_.get(), database_name_.c_str(), "messages"));
    users_collection_.reset(
        mongoc_client_get_collection(client_.get(), database_name_.c_str(), "users"));

    if (database_ == nullptr || messages_collection_ == nullptr || users_collection_ == nullptr) {
        throw std::runtime_error("failed to open MongoDB database or collections");
    }

    SeedUsersIfNeeded();
}

MongoMessageStore::~MongoMessageStore() {
    users_collection_.reset();
    messages_collection_.reset();
    database_.reset();
    client_.reset();
    mongoc_cleanup();
}

ChatMessage MongoMessageStore::SaveMessage(ChatMessage message) {
    StoreLock lock(store_mutex_);

    if (!UserExistsUnlocked(message.message_from)) {
        throw std::invalid_argument("sender does not exist");
    }
    if (!UserExistsUnlocked(message.message_to)) {
        throw std::invalid_argument("recipient does not exist");
    }

    if (message.message_id == 0) {
        message.message_id = NextMessageId();
    }
    if (message.created_at == std::chrono::system_clock::time_point{}) {
        message.created_at = std::chrono::system_clock::now();
    }

    const std::int64_t created_at_ms = ToEpochMs(message.created_at);
    bson_t* message_doc = BCON_NEW(
        "message_id", BCON_INT64(static_cast<std::int64_t>(message.message_id)),
        "message_from", BCON_UTF8(message.message_from.c_str()),
        "message_to", BCON_UTF8(message.message_to.c_str()),
        "content", BCON_UTF8(message.content.c_str()),
        "created_at_ms", BCON_INT64(created_at_ms),
        "delivered_to", "[", "]");

    bson_error_t error;
    const bool inserted =
        mongoc_collection_insert_one(messages_collection_.get(), message_doc, nullptr, nullptr, &error);
    bson_destroy(message_doc);
    ThrowOnMongoError(inserted, error, "insert message");

    bson_t* user_filter = BCON_NEW("user_id", BCON_UTF8(message.message_from.c_str()));
    bson_t* user_update = BCON_NEW(
        "$set", "{",
            "last_active_at_ms", BCON_INT64(created_at_ms),
        "}");
    const bool updated =
        mongoc_collection_update_one(users_collection_.get(), user_filter, user_update, nullptr, nullptr, &error);
    bson_destroy(user_filter);
    bson_destroy(user_update);
    ThrowOnMongoError(updated, error, "update sender last_active_at");

    return message;
}

std::vector<ChatMessage> MongoMessageStore::LoadPendingMessagesFor(const UserId& recipient_id) {
    StoreLock lock(store_mutex_);

    bson_t* filter = BCON_NEW(
        "message_to", BCON_UTF8(recipient_id.c_str()),
        "delivered_to", "{", "$ne", BCON_UTF8(recipient_id.c_str()), "}");
    bson_t* opts = BCON_NEW("sort", "{", "message_id", BCON_INT32(1), "}");

    mongoc_cursor_t* cursor =
        mongoc_collection_find_with_opts(messages_collection_.get(), filter, opts, nullptr);
    bson_destroy(filter);
    bson_destroy(opts);

    std::vector<ChatMessage> pending;
    const bson_t* doc = nullptr;
    while (mongoc_cursor_next(cursor, &doc)) {
        pending.push_back(ParseMessage(doc));
    }

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        mongoc_cursor_destroy(cursor);
        throw std::runtime_error(std::string("load pending failed: ") + error.message);
    }

    mongoc_cursor_destroy(cursor);
    return pending;
}

std::vector<ChatMessage> MongoMessageStore::LoadHistoryFor(const UserId& user_id) {
    StoreLock lock(store_mutex_);

    bson_t* filter = BCON_NEW(
        "$or", "[",
            "{", "message_from", BCON_UTF8(user_id.c_str()), "}",
            "{", "message_to", BCON_UTF8(user_id.c_str()), "}",
        "]");
    bson_t* opts = BCON_NEW("sort", "{", "message_id", BCON_INT32(1), "}");

    mongoc_cursor_t* cursor =
        mongoc_collection_find_with_opts(messages_collection_.get(), filter, opts, nullptr);
    bson_destroy(filter);
    bson_destroy(opts);

    std::vector<ChatMessage> history;
    const bson_t* doc = nullptr;
    while (mongoc_cursor_next(cursor, &doc)) {
        history.push_back(ParseMessage(doc));
    }

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        mongoc_cursor_destroy(cursor);
        throw std::runtime_error(std::string("load history failed: ") + error.message);
    }

    mongoc_cursor_destroy(cursor);
    return history;
}

void MongoMessageStore::MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) {
    StoreLock lock(store_mutex_);

    bson_t* filter = BCON_NEW(
        "message_id", BCON_INT64(static_cast<std::int64_t>(message_id)),
        "message_to", BCON_UTF8(recipient_id.c_str()));
    bson_t* update = BCON_NEW(
        "$addToSet", "{",
            "delivered_to", BCON_UTF8(recipient_id.c_str()),
        "}");

    bson_error_t error;
    const bool updated =
        mongoc_collection_update_one(messages_collection_.get(), filter, update, nullptr, nullptr, &error);
    bson_destroy(filter);
    bson_destroy(update);
    ThrowOnMongoError(updated, error, "mark delivered");
}

MessageStore::UserPresence MongoMessageStore::LoginUser(const UserId& user_id) {
    StoreLock lock(store_mutex_);

    if (!UserExistsUnlocked(user_id)) {
        throw std::invalid_argument("user does not exist");
    }

    bson_t* filter = BCON_NEW("user_id", BCON_UTF8(user_id.c_str()));
    bson_t* update = BCON_NEW(
        "$set", "{",
            "last_active_at_ms", BCON_INT64(ToEpochMs(std::chrono::system_clock::now())),
        "}");

    bson_error_t error;
    const bool updated =
        mongoc_collection_update_one(users_collection_.get(), filter, update, nullptr, nullptr, &error);
    bson_destroy(filter);
    bson_destroy(update);
    ThrowOnMongoError(updated, error, "login update user last_active_at");

    return LoadUserOrThrow(user_id);
}

bool MongoMessageStore::UserExists(const UserId& user_id) {
    StoreLock lock(store_mutex_);
    return UserExistsUnlocked(user_id);
}

bool MongoMessageStore::UserExistsUnlocked(const UserId& user_id) {
    bson_t* filter = BCON_NEW("user_id", BCON_UTF8(user_id.c_str()));
    bson_error_t error;
    const std::int64_t count = mongoc_collection_count_documents(
        users_collection_.get(), filter, nullptr, nullptr, nullptr, &error);
    bson_destroy(filter);

    if (count < 0) {
        throw std::runtime_error(std::string("user lookup failed: ") + error.message);
    }
    return count > 0;
}

void MongoMessageStore::SetPresence(const UserId& user_id, bool is_online) {
    StoreLock lock(store_mutex_);

    const UserPresence current_user = LoadUserOrThrow(user_id);

    std::int64_t active_connections = 0;
    {
        bson_t* count_filter = BCON_NEW("user_id", BCON_UTF8(user_id.c_str()));
        mongoc_cursor_t* count_cursor =
            mongoc_collection_find_with_opts(users_collection_.get(), count_filter, nullptr, nullptr);
        bson_destroy(count_filter);

        const bson_t* count_doc = nullptr;
        if (mongoc_cursor_next(count_cursor, &count_doc)) {
            active_connections = GetInt64OrDefault(count_doc, "active_connections", 0);
        }

        bson_error_t cursor_error;
        if (mongoc_cursor_error(count_cursor, &cursor_error)) {
            mongoc_cursor_destroy(count_cursor);
            throw std::runtime_error(std::string("set presence load user failed: ") + cursor_error.message);
        }
        mongoc_cursor_destroy(count_cursor);
    }

    if (is_online) {
        ++active_connections;
    } else if (active_connections > 0) {
        --active_connections;
    }

    bson_t* filter = BCON_NEW("user_id", BCON_UTF8(current_user.user_id.c_str()));
    bson_t* update = BCON_NEW(
        "$set", "{",
            "is_online", BCON_BOOL(active_connections > 0),
            "active_connections", BCON_INT64(active_connections),
            "last_active_at_ms", BCON_INT64(ToEpochMs(std::chrono::system_clock::now())),
        "}");

    bson_error_t error;
    const bool updated =
        mongoc_collection_update_one(users_collection_.get(), filter, update, nullptr, nullptr, &error);
    bson_destroy(filter);
    bson_destroy(update);
    ThrowOnMongoError(updated, error, "set presence");
}

std::vector<MessageStore::UserPresence> MongoMessageStore::ListUsers() {
    StoreLock lock(store_mutex_);

    bson_t* filter = bson_new();
    bson_t* opts = BCON_NEW("sort", "{", "user_id", BCON_INT32(1), "}");

    mongoc_cursor_t* cursor =
        mongoc_collection_find_with_opts(users_collection_.get(), filter, opts, nullptr);
    bson_destroy(filter);
    bson_destroy(opts);

    std::vector<UserPresence> users;
    const bson_t* doc = nullptr;
    while (mongoc_cursor_next(cursor, &doc)) {
        users.push_back(ParseUserPresence(doc));
    }

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        mongoc_cursor_destroy(cursor);
        throw std::runtime_error(std::string("list users failed: ") + error.message);
    }

    mongoc_cursor_destroy(cursor);
    return users;
}

void MongoMessageStore::SeedUsersIfNeeded() {
    StoreLock lock(store_mutex_);

    bson_t* filter = bson_new();
    bson_error_t error;
    const std::int64_t count = mongoc_collection_count_documents(
        users_collection_.get(), filter, nullptr, nullptr, nullptr, &error);
    bson_destroy(filter);

    if (count < 0) {
        throw std::runtime_error(std::string("count users failed: ") + error.message);
    }
    if (count > 0) {
        return;
    }

    const std::int64_t now_ms = ToEpochMs(std::chrono::system_clock::now());
    for (const char* user_id : {"user_a", "user_b", "user_c", "user_d", "user_e"}) {
        bson_t* doc = BCON_NEW(
            "user_id", BCON_UTF8(user_id),
            "is_online", BCON_BOOL(false),
            "active_connections", BCON_INT64(0),
            "last_active_at_ms", BCON_INT64(now_ms));
        const bool inserted =
            mongoc_collection_insert_one(users_collection_.get(), doc, nullptr, nullptr, &error);
        bson_destroy(doc);
        ThrowOnMongoError(inserted, error, "seed user");
    }
}

MessageId MongoMessageStore::NextMessageId() {
    StoreLock lock(store_mutex_);

    bson_t* filter = bson_new();
    bson_t* opts = BCON_NEW(
        "sort", "{", "message_id", BCON_INT32(-1), "}",
        "limit", BCON_INT32(1));

    mongoc_cursor_t* cursor =
        mongoc_collection_find_with_opts(messages_collection_.get(), filter, opts, nullptr);
    bson_destroy(filter);
    bson_destroy(opts);

    MessageId next = 1;
    const bson_t* doc = nullptr;
    if (mongoc_cursor_next(cursor, &doc)) {
        const auto last_message_id = static_cast<MessageId>(GetInt64OrThrow(doc, "message_id"));
        next = last_message_id + 1;
    }

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        mongoc_cursor_destroy(cursor);
        throw std::runtime_error(std::string("next message id query failed: ") + error.message);
    }

    mongoc_cursor_destroy(cursor);
    return next;
}

MessageStore::UserPresence MongoMessageStore::LoadUserOrThrow(const UserId& user_id) {
    StoreLock lock(store_mutex_);

    bson_t* filter = BCON_NEW("user_id", BCON_UTF8(user_id.c_str()));
    mongoc_cursor_t* cursor =
        mongoc_collection_find_with_opts(users_collection_.get(), filter, nullptr, nullptr);
    bson_destroy(filter);

    const bson_t* doc = nullptr;
    if (!mongoc_cursor_next(cursor, &doc)) {
        bson_error_t error;
        if (mongoc_cursor_error(cursor, &error)) {
            mongoc_cursor_destroy(cursor);
            throw std::runtime_error(std::string("load user failed: ") + error.message);
        }
        mongoc_cursor_destroy(cursor);
        throw std::invalid_argument("user does not exist");
    }

    const UserPresence user = ParseUserPresence(doc);
    mongoc_cursor_destroy(cursor);
    return user;
}

}  // namespace db_service
