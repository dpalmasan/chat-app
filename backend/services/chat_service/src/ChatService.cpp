#include "ChatService.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "Logger.h"

namespace chat {

ChatService::ChatService(
    std::unique_ptr<MessagePublisher> message_publisher,
    std::unique_ptr<MessageStore> history_store)
    : message_publisher_(std::move(message_publisher)),
      history_store_(std::move(history_store)) {
    if (!message_publisher_) {
        throw std::invalid_argument("message_publisher cannot be null");
    }
    if (!history_store_) {
        throw std::invalid_argument("history_store cannot be null");
    }
}

void ChatService::OnWebSocketOpen(const std::string& client_id) {
    logging::Logger::Instance().Info("ChatService", "WebSocket open for client_id=" + client_id);
    RegisterClient(client_id);
}

void ChatService::OnWebSocketClose(const std::string& client_id) {
    logging::Logger::Instance().Info("ChatService", "WebSocket close for client_id=" + client_id);
    UnregisterClient(client_id);
}

ChatMessage ChatService::OnWebSocketMessage(const std::string& client_id, const ChatMessage& message) {
    return HandleIncomingMessage(client_id, message);
}

std::vector<ChatMessage> ChatService::LoadPendingMessagesForClient(const std::string& client_id) {
    const std::vector<ChatMessage> pending_messages = history_store_->LoadPendingMessagesFor(client_id);
    logging::Logger::Instance().Info(
        "ChatService",
        "Loaded pending messages for client_id=" + client_id +
            " count=" + std::to_string(pending_messages.size()));
    return pending_messages;
}

void ChatService::MarkMessageDelivered(const std::string& client_id, MessageId message_id) {
    history_store_->MarkMessageDelivered(client_id, message_id);
    logging::Logger::Instance().Debug(
        "ChatService",
        "Marked delivered message_id=" + std::to_string(message_id) +
            " client_id=" + client_id);
}

std::vector<std::string> ChatService::ConnectedClients() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    std::vector<std::string> client_ids;
    client_ids.reserve(connected_clients_.size());

    for (const auto& [client_id, _] : connected_clients_) {
        client_ids.push_back(client_id);
    }

    std::sort(client_ids.begin(), client_ids.end());
    return client_ids;
}

void ChatService::RegisterClient(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    connected_clients_[client_id] = ClientSession{client_id, std::this_thread::get_id()};
}

void ChatService::UnregisterClient(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    connected_clients_.erase(client_id);
}

ChatMessage ChatService::HandleIncomingMessage(const std::string& client_id, const ChatMessage& message) {
    logging::Logger::Instance().Debug(
        "ChatService",
        "Incoming message from=" + message.message_from +
            " to=" + message.message_to +
            " content=\"" + message.content + "\"");

    if (message.message_from != client_id) {
        logging::Logger::Instance().Warn(
            "ChatService",
            "Rejected message: message_from does not match client_id. client_id=" + client_id +
                " message_from=" + message.message_from);
        throw std::invalid_argument("message_from must match websocket client id");
    }
    if (message.message_to.empty()) {
        logging::Logger::Instance().Warn("ChatService", "Rejected message: message_to is empty");
        throw std::invalid_argument("message_to must be provided for 1:1 chat");
    }
    if (message.content.empty()) {
        logging::Logger::Instance().Warn("ChatService", "Rejected message: content is empty");
        throw std::invalid_argument("content must be non-empty");
    }

    return PersistMessage(message);
}

ChatMessage ChatService::PersistMessage(const ChatMessage& message) {
    const ChatMessage persisted_message = message_publisher_->PublishMessage(message);

    logging::Logger::Instance().Info(
        "ChatService",
        "Persisted message_id=" + std::to_string(persisted_message.message_id) +
            " from=" + persisted_message.message_from +
            " to=" + persisted_message.message_to +
            " content=\"" + persisted_message.content + "\"");
    return persisted_message;
}

}  // namespace chat
