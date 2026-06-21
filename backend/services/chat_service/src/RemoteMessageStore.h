#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "MessageStore.h"

namespace chat {

/**
 * @brief TCP client for the db_service history API.
 *
 * This implementation supports save, pending-load and delivered-mark operations
 * over a lightweight line-based protocol.
 */
class RemoteMessageStore final : public MessageStore {
public:
    /** @param host DB service host. @param port DB service port. */
    RemoteMessageStore(std::string host, std::uint16_t port);

    /** @inheritdoc MessageStore::SaveMessage */
    ChatMessage SaveMessage(ChatMessage message) override;
    /** @inheritdoc MessageStore::LoadPendingMessagesFor */
    std::vector<ChatMessage> LoadPendingMessagesFor(const UserId& recipient_id) override;
    /** @inheritdoc MessageStore::MarkMessageDelivered */
    void MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) override;

private:
    std::string SendRequest(const std::string& request);
    static std::string UrlEncode(const std::string& input);
    static std::string UrlDecode(const std::string& input);
    static std::vector<std::string> Split(const std::string& line, char delim);
    static ChatMessage ParseMessageLine(const std::string& line);

    std::string host_;
    std::uint16_t port_;
};

}  // namespace chat
