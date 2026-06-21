#include <iostream>
#include <memory>

#include "ChatService.h"
#include "InMemoryMessageStore.h"
#include "Logger.h"

int main() {
    chat::Logger::Instance().SetMinLevel(chat::LogLevel::kDebug);

    auto message_store = std::make_unique<chat::InMemoryMessageStore>();
    chat::ChatService service(std::move(message_store));

    chat::Logger::Instance().Info("Main", "Chat service startup complete");
    std::cout << "Chat service boilerplate initialized." << std::endl;
    std::cout << "WebSocket wiring is pending; in-memory persistence is configured." << std::endl;

    return 0;
}
