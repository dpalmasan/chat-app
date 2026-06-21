#include <memory>
#include <cstdlib>
#include <cstdint>
#include <string>

#include "InMemoryMessageQueue.h"
#include "Logger.h"
#include "MessageEventSubscriber.h"
#include "RemoteDbMessageStore.h"
#include "WorkerServer.h"

int main() {
    logging::Logger::Instance().SetMinLevel(logging::LogLevel::kDebug);

    std::string db_host = "127.0.0.1";
    if (const char* db_host_env = std::getenv("DB_SERVICE_HOST")) {
        db_host = db_host_env;
    }

    std::uint16_t db_port = 9091;
    if (const char* db_port_env = std::getenv("DB_SERVICE_PORT")) {
        db_port = static_cast<std::uint16_t>(std::stoul(db_port_env));
    }

    logging::Logger::Instance().Info(
        "MessageQueueWorker",
        "Remote db service at " + db_host + ":" + std::to_string(db_port));

    auto queue = std::make_unique<message_queue::InMemoryMessageQueue>();
    auto store = std::make_unique<message_queue::RemoteDbMessageStore>(db_host, db_port);
    auto subscriber = std::make_unique<message_queue::MessageEventSubscriber>(*queue, *store);

    message_queue::WorkerServer server(*queue);
    server.Run(9090);

    subscriber->Stop();
    return 0;
}
