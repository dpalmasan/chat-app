#pragma once

#include "ChatMessage.h"

namespace chat {

/**
 * @brief Abstraction for publishing chat writes into the asynchronous message pipeline.
 */
class MessagePublisher {
public:
    virtual ~MessagePublisher() = default;

    /**
     * @brief Publish a chat message for persistence and fanout processing.
     * @param message User payload to publish.
     * @return Canonical message from backend (id/timestamp assigned).
     */
    virtual ChatMessage PublishMessage(const ChatMessage& message) = 0;
};

}  // namespace chat
