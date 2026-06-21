#include "InMemoryMessageStore.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace db_service {

InMemoryMessageStore::InMemoryMessageStore() {
    const auto now = std::chrono::system_clock::now();
    for (const char* user_id : {"user_a", "user_b", "user_c", "user_d", "user_e"}) {
        UserPresence presence;
        presence.user_id = user_id;
        presence.is_online = false;
        presence.last_active_at = now;
        users_.emplace(user_id, presence);
    }
}

ChatMessage InMemoryMessageStore::SaveMessage(ChatMessage message) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    if (users_.find(message.message_from) == users_.end()) {
        throw std::invalid_argument("sender does not exist");
    }
    if (users_.find(message.message_to) == users_.end()) {
        throw std::invalid_argument("recipient does not exist");
    }

    if (message.message_id == 0) {
        message.message_id = next_message_id_;
        ++next_message_id_;
    }
    if (message.created_at == std::chrono::system_clock::time_point{}) {
        message.created_at = std::chrono::system_clock::now();
    }

    users_[message.message_from].last_active_at = message.created_at;

    messages_.push_back(message);
    return message;
}

std::vector<ChatMessage> InMemoryMessageStore::LoadPendingMessagesFor(const UserId& recipient_id) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    std::vector<ChatMessage> pending_messages;
    const auto delivered_it = delivered_message_ids_.find(recipient_id);

    for (const ChatMessage& message : messages_) {
        if (message.message_to != recipient_id) {
            continue;
        }
        if (delivered_it != delivered_message_ids_.end() &&
            delivered_it->second.count(message.message_id) > 0) {
            continue;
        }
        pending_messages.push_back(message);
    }

    return pending_messages;
}

std::vector<ChatMessage> InMemoryMessageStore::LoadHistoryFor(const UserId& user_id) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    std::vector<ChatMessage> history;
    for (const ChatMessage& message : messages_) {
        if (message.message_from == user_id || message.message_to == user_id) {
            history.push_back(message);
        }
    }
    return history;
}

void InMemoryMessageStore::MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    delivered_message_ids_[recipient_id].insert(message_id);
}

MessageStore::UserPresence InMemoryMessageStore::LoginUser(const UserId& user_id) {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    auto user_it = RequireUser(user_id);
    user_it->second.is_online = true;
    user_it->second.last_active_at = std::chrono::system_clock::now();
    return user_it->second;
}

bool InMemoryMessageStore::UserExists(const UserId& user_id) {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    return users_.find(user_id) != users_.end();
}

void InMemoryMessageStore::SetPresence(const UserId& user_id, bool is_online) {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    auto user_it = RequireUser(user_id);
    user_it->second.is_online = is_online;
    user_it->second.last_active_at = std::chrono::system_clock::now();
}

std::vector<MessageStore::UserPresence> InMemoryMessageStore::ListUsers() {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    std::vector<UserPresence> users;
    users.reserve(users_.size());
    for (const auto& [_, user] : users_) {
        users.push_back(user);
    }
    std::sort(
        users.begin(),
        users.end(),
        [](const UserPresence& left, const UserPresence& right) { return left.user_id < right.user_id; });
    return users;
}

InMemoryMessageStore::PresenceByUser::iterator InMemoryMessageStore::RequireUser(const UserId& user_id) {
    auto user_it = users_.find(user_id);
    if (user_it == users_.end()) {
        throw std::invalid_argument("user does not exist");
    }
    return user_it;
}

}  // namespace db_service
