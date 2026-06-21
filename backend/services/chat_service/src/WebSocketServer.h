#pragma once

#include <cstdint>
#include <string>

namespace chat {

class ChatService;

class WebSocketServer {
public:
    explicit WebSocketServer(ChatService& chat_service);

    void Run(const std::string& host, std::uint16_t port);

private:
    ChatService& chat_service_;
};

}  // namespace chat
