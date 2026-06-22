#include <memory>
#include <cstdlib>
#include <string>

#include "InMemoryMessageStore.h"
#include "Logger.h"
#include "MongoMessageStore.h"
#include "WorkerServer.h"

int main() {
    logging::Logger::Instance().SetMinLevel(logging::LogLevel::kDebug);

    const char* backend_env = std::getenv("DB_BACKEND");
    const std::string backend = backend_env == nullptr ? "mongo" : backend_env;

    std::unique_ptr<db_service::MessageStore> store;
    if (backend == "in_memory") {
        logging::Logger::Instance().Warn("Main", "Using in-memory datastore backend");
        store = std::make_unique<db_service::InMemoryMessageStore>();
    } else {
        std::string mongo_uri = "mongodb://127.0.0.1:27017";
        if (const char* mongo_uri_env = std::getenv("MONGODB_URI")) {
            mongo_uri = mongo_uri_env;
        }

        std::string database_name = "chat_app";
        if (const char* database_name_env = std::getenv("MONGODB_DB_NAME")) {
            database_name = database_name_env;
        }

        logging::Logger::Instance().Info(
            "Main",
            "Using MongoDB datastore backend at " + mongo_uri + " db=" + database_name);
        store = std::make_unique<db_service::MongoMessageStore>(mongo_uri, database_name);
    }

    db_service::WorkerServer server(*store);
    server.Run(9091);

    return 0;
}
