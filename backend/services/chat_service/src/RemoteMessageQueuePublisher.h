#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "MessagePublisher.h"

namespace chat {

/**
 * @brief TCP client that publishes chat writes to the message_queue worker.
 */
class RemoteMessageQueuePublisher final : public MessagePublisher {
public:
    /** @param host Message queue worker host. @param port Message queue worker port. */
    RemoteMessageQueuePublisher(std::string host, std::uint16_t port);

    /** @inheritdoc MessagePublisher::PublishMessage */
    ChatMessage PublishMessage(const ChatMessage& message) override;

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
