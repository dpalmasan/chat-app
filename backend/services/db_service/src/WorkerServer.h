#pragma once

#include <cstdint>

#include "MessageStore.h"

namespace db_service {

class WorkerServer {
public:
    explicit WorkerServer(MessageStore& store);
    void Run(std::uint16_t port);

private:
    MessageStore& store_;
};

}  // namespace db_service
