#include "ChatService.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace chat {

ChatService::ChatService(std::unique_ptr<MessageStore> message_store)
    : message_store_(std::move(message_store)) {
    if (!message_store_) {
        throw std::invalid_argument("message_store cannot be null");
    }
}

void ChatService::OnWebSocketOpen(const std::string& client_id) {
    RegisterClient(client_id);
}

void ChatService::OnWebSocketClose(const std::string& client_id) {
    UnregisterClient(client_id);
}

void ChatService::OnWebSocketMessage(const std::string& client_id, const ChatMessage& message) {
    HandleIncomingMessage(client_id, message);
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

void ChatService::HandleIncomingMessage(const std::string& client_id, const ChatMessage& message) {
    if (message.message_from != client_id) {
        throw std::invalid_argument("message_from must match websocket client id");
    }
    if (message.message_to.empty()) {
        throw std::invalid_argument("message_to must be provided for 1:1 chat");
    }
    if (message.content.empty()) {
        throw std::invalid_argument("content must be non-empty");
    }

    PersistMessage(message);
}

void ChatService::PersistMessage(const ChatMessage& message) {
    message_store_->SaveMessage(message);
}

}  // namespace chat
