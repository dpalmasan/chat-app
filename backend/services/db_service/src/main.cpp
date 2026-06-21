#include <memory>

#include "InMemoryMessageStore.h"
#include "Logger.h"
#include "WorkerServer.h"

int main() {
    logging::Logger::Instance().SetMinLevel(logging::LogLevel::kDebug);

    auto store = std::make_unique<db_service::InMemoryMessageStore>();
    db_service::WorkerServer server(*store);
    server.Run(9091);

    return 0;
}
