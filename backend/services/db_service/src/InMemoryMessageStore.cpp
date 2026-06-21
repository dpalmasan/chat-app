#include "InMemoryMessageStore.h"

#include <chrono>

namespace db_service {

ChatMessage InMemoryMessageStore::SaveMessage(ChatMessage message) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    if (message.message_id == 0) {
        message.message_id = next_message_id_;
        ++next_message_id_;
    }
    if (message.created_at == std::chrono::system_clock::time_point{}) {
        message.created_at = std::chrono::system_clock::now();
    }

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

void InMemoryMessageStore::MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    delivered_message_ids_[recipient_id].insert(message_id);
}

}  // namespace db_service
