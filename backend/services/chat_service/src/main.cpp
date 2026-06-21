#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include "ChatService.h"

namespace chat {

class InMemoryMessageStore final : public MessageStore {
public:
    void SaveMessage(const ChatMessage& message) override {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        messages_.push_back(message);
    }

private:
    std::mutex messages_mutex_;
    std::vector<ChatMessage> messages_;
};

}  // namespace chat

int main() {
    auto message_store = std::make_unique<chat::InMemoryMessageStore>();
    chat::ChatService service(std::move(message_store));

    std::cout << "Chat service boilerplate initialized." << std::endl;
    std::cout << "WebSocket wiring is pending; in-memory persistence is configured." << std::endl;

    return 0;
}
