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

/**
 * @brief Tracks an active WebSocket client attached to this chat_service instance.
 */
struct ClientSession {
    std::string client_id;
    std::thread::id worker_thread_id;
};

/**
 * @brief Core application service for connection tracking, validation, publish, and history polling.
 *
 * Responsibilities:
 * - Maintain connected clients for this process.
 * - Validate incoming chat payloads.
 * - Publish writes through MessagePublisher.
 * - Load and mark pending history through MessageStore.
 */
class ChatService {
public:
    /**
     * @brief Construct service dependencies.
     * @param message_publisher Write path abstraction (queue/event pipeline).
     * @param history_store Read/ack abstraction (history datastore).
     */
    ChatService(
        std::unique_ptr<MessagePublisher> message_publisher,
        std::unique_ptr<MessageStore> history_store);

    ChatService(const ChatService&) = delete;
    ChatService& operator=(const ChatService&) = delete;
    ChatService(ChatService&&) = delete;
    ChatService& operator=(ChatService&&) = delete;

    /** @brief Register that a client opened a WebSocket connection on this server. */
    void OnWebSocketOpen(const std::string& client_id);
    /** @brief Register that a client closed its WebSocket connection on this server. */
    void OnWebSocketClose(const std::string& client_id);
    /**
     * @brief Validate and publish a message.
     * @return Persisted canonical message (with server metadata).
     */
    ChatMessage OnWebSocketMessage(const std::string& client_id, const ChatMessage& message);
    /** @brief Load pending messages for a connected client from history storage. */
    std::vector<ChatMessage> LoadPendingMessagesForClient(const std::string& client_id);
    /** @brief Mark one message as delivered for a recipient. */
    void MarkMessageDelivered(const std::string& client_id, MessageId message_id);

    /** @brief Snapshot currently connected client IDs for this process. */
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
