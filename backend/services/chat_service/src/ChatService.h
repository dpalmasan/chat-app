#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ChatMessage.h"
#include "MessagePublisher.h"
#include "MessageStore.h"

namespace chat {

struct ClientSession {
    std::string client_id;
    std::thread::id worker_thread_id;
};

class ChatService {
public:
    ChatService(
        std::unique_ptr<MessagePublisher> message_publisher,
        std::unique_ptr<MessageStore> history_store);

    ChatService(const ChatService&) = delete;
    ChatService& operator=(const ChatService&) = delete;
    ChatService(ChatService&&) = delete;
    ChatService& operator=(ChatService&&) = delete;

    void OnWebSocketOpen(const std::string& client_id);
    void OnWebSocketClose(const std::string& client_id);
    ChatMessage OnWebSocketMessage(const std::string& client_id, const ChatMessage& message);
    std::vector<ChatMessage> LoadPendingMessagesForClient(const std::string& client_id);
    void MarkMessageDelivered(const std::string& client_id, MessageId message_id);

    std::vector<std::string> ConnectedClients() const;

private:
    void RegisterClient(const std::string& client_id);
    void UnregisterClient(const std::string& client_id);
    ChatMessage HandleIncomingMessage(const std::string& client_id, const ChatMessage& message);

    // Persists and returns canonical message fields (ids/timestamps).
    ChatMessage PersistMessage(const ChatMessage& message);

    std::unique_ptr<MessagePublisher> message_publisher_;
    std::unique_ptr<MessageStore> history_store_;
    mutable std::mutex clients_mutex_;
    std::unordered_map<std::string, ClientSession> connected_clients_;
};

}  // namespace chat
