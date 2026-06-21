#include "InMemoryMessageStore.h"

#include <chrono>

#include "Logger.h"

namespace chat {

ChatMessage InMemoryMessageStore::SaveMessage(ChatMessage message) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    const bool had_message_id = message.message_id != 0;
    if (message.message_id == 0) {
        message.message_id = next_message_id_;
        ++next_message_id_;
    }
    if (message.created_at == std::chrono::system_clock::time_point{}) {
        message.created_at = std::chrono::system_clock::now();
    }

    messages_.push_back(message);
    Logger::Instance().Debug(
        "InMemoryMessageStore",
        "Stored message_id=" + std::to_string(message.message_id) +
            (had_message_id ? " (provided)" : " (assigned)") +
            " total_messages=" + std::to_string(messages_.size()));
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

    Logger::Instance().Debug(
        "InMemoryMessageStore",
        "Loaded pending messages recipient=" + recipient_id +
            " count=" + std::to_string(pending_messages.size()));
    return pending_messages;
}

void InMemoryMessageStore::MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    delivered_message_ids_[recipient_id].insert(message_id);
    Logger::Instance().Debug(
        "InMemoryMessageStore",
        "Marked delivered recipient=" + recipient_id +
            " message_id=" + std::to_string(message_id));
}

}  // namespace chat
