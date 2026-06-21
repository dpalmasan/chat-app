#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace db_service {

using MessageId = std::uint64_t;
using UserId = std::string;

/**
 * @brief Canonical persisted chat message model in db_service.
 */
struct ChatMessage {
    MessageId message_id{};
    UserId message_from;
    UserId message_to;
    std::string content;
    std::chrono::system_clock::time_point created_at{};
};

}  // namespace db_service
