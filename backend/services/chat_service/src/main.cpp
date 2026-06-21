#include <iostream>
#include <memory>

#include "ChatService.h"
#include "InMemoryMessageStore.h"
#include "Logger.h"
#include "WebSocketServer.h"

int main() {
    chat::Logger::Instance().SetMinLevel(chat::LogLevel::kDebug);

    auto message_store = std::make_unique<chat::InMemoryMessageStore>();
    chat::ChatService service(std::move(message_store));
    chat::WebSocketServer server(service);

    chat::Logger::Instance().Info("Main", "Chat service startup complete");
    std::cout << "Chat service boilerplate initialized." << std::endl;
    std::cout << "In-memory persistence configured. Starting WebSocket server on 127.0.0.1:8080." << std::endl;

    server.Run("127.0.0.1", 8080);

    return 0;
}
