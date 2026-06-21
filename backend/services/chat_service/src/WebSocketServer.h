#pragma once

#include <cstdint>
#include <string>

namespace chat {

class ChatService;

/**
 * @brief Minimal WebSocket server endpoint for chat clients.
 *
 * Delegates business operations to ChatService and owns socket accept loop.
 */
class WebSocketServer {
public:
    /** @param chat_service Service used for validation, publish, and history operations. */
    explicit WebSocketServer(ChatService& chat_service);

    /**
     * @brief Start the blocking server loop.
     * @param host Bind host (prototype supports 127.0.0.1 / 0.0.0.0).
     * @param port Bind port.
     */
    void Run(const std::string& host, std::uint16_t port);

private:
    ChatService& chat_service_;
};

}  // namespace chat
