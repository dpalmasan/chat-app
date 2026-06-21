#include <memory>
#include <cstdlib>
#include <cstdint>
#include <string>

#include "ChatService.h"
#include "Logger.h"
#include "RemoteMessageQueuePublisher.h"
#include "RemoteMessageStore.h"
#include "WebSocketServer.h"

int main(int argc, char* argv[]) {
    logging::Logger::Instance().SetMinLevel(logging::LogLevel::kDebug);

    std::uint16_t websocket_port = 8080;
    if (argc > 1) {
        websocket_port = static_cast<std::uint16_t>(std::stoul(argv[1]));
    }

    std::string queue_host = "127.0.0.1";
    if (const char* mq_host = std::getenv("MESSAGE_QUEUE_HOST")) {
        queue_host = mq_host;
    }

    std::uint16_t queue_port = 9090;
    if (const char* mq_port = std::getenv("MESSAGE_QUEUE_PORT")) {
        queue_port = static_cast<std::uint16_t>(std::stoul(mq_port));
    }

    std::string db_host = "127.0.0.1";
    if (const char* db_host_env = std::getenv("DB_SERVICE_HOST")) {
        db_host = db_host_env;
    }

    std::uint16_t db_port = 9091;
    if (const char* db_port_env = std::getenv("DB_SERVICE_PORT")) {
        db_port = static_cast<std::uint16_t>(std::stoul(db_port_env));
    }

    auto message_publisher = std::make_unique<chat::RemoteMessageQueuePublisher>(queue_host, queue_port);
    auto history_store = std::make_unique<chat::RemoteMessageStore>(db_host, db_port);
    chat::ChatService service(std::move(message_publisher), std::move(history_store));
    chat::WebSocketServer server(service);

    logging::Logger::Instance().Info("Main", "Chat service startup complete");
    logging::Logger::Instance().Info(
        "Main",
        "Remote message queue at " + queue_host + ":" + std::to_string(queue_port));
    logging::Logger::Instance().Info(
        "Main",
        "Remote db service at " + db_host + ":" + std::to_string(db_port));
    logging::Logger::Instance().Info(
        "Main",
        "Starting WebSocket server on 127.0.0.1:" + std::to_string(websocket_port));

    server.Run("127.0.0.1", websocket_port);

    return 0;
}
