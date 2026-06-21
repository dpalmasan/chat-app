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

}  // namespace chat
