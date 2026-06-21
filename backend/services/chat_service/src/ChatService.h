#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ChatMessage.h"
#include "MessageStore.h"

namespace chat {

struct ClientSession {
    std::string client_id;
    std::thread::id worker_thread_id;
};

class ChatService {
public:
    explicit ChatService(std::unique_ptr<MessageStore> message_store);

    ChatService(const ChatService&) = delete;
    ChatService& operator=(const ChatService&) = delete;
    ChatService(ChatService&&) = delete;
    ChatService& operator=(ChatService&&) = delete;

    void OnWebSocketOpen(const std::string& client_id);
    void OnWebSocketClose(const std::string& client_id);
    void OnWebSocketMessage(const std::string& client_id, const ChatMessage& message);

    std::vector<std::string> ConnectedClients() const;

private:
    void RegisterClient(const std::string& client_id);
    void UnregisterClient(const std::string& client_id);
    void HandleIncomingMessage(const std::string& client_id, const ChatMessage& message);

    // Placeholder for future persistence implementation.
    void PersistMessage(const ChatMessage& message);

    std::unique_ptr<MessageStore> message_store_;
    mutable std::mutex clients_mutex_;
    std::unordered_map<std::string, ClientSession> connected_clients_;
};

}  // namespace chat
