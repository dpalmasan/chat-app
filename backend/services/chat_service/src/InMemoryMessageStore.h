#pragma once

#include <mutex>
#include <vector>

#include "MessageStore.h"

namespace chat {

class InMemoryMessageStore final : public MessageStore {
public:
    ChatMessage SaveMessage(ChatMessage message) override;

private:
    std::mutex messages_mutex_;
    MessageId next_message_id_ = 1;
    std::vector<ChatMessage> messages_;
};

}  // namespace chat
