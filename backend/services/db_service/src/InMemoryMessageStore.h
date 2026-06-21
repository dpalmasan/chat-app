#pragma once

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "MessageStore.h"

namespace db_service {

class InMemoryMessageStore final : public MessageStore {
public:
    ChatMessage SaveMessage(ChatMessage message) override;
    std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) override;
    void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) override;

private:
    std::mutex messages_mutex_;
    MessageId next_message_id_ = 1;
    std::vector<ChatMessage> messages_;
    std::unordered_map<UserId, std::unordered_set<MessageId>> delivered_message_ids_;
};

}  // namespace db_service
