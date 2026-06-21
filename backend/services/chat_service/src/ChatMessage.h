#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace chat {

using MessageId = std::uint64_t;
using UserId = std::string;

struct ChatMessage {
    MessageId message_id{};
    UserId message_from;
    UserId message_to;
    std::string content;
    std::chrono::system_clock::time_point created_at{};
};

}  // namespace chat
