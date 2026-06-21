#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "MessagePublisher.h"

namespace chat {

class RemoteMessageQueuePublisher final : public MessagePublisher {
public:
    RemoteMessageQueuePublisher(std::string host, std::uint16_t port);

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
