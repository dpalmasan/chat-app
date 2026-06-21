#pragma once

#include <cstdint>

#include "MessageStore.h"

namespace db_service {

/**
 * @brief TCP API server for persistence and history operations.
 *
 * Supports save, pending-load and mark-delivered commands over a line-based
 * protocol consumed by other services.
 */
class WorkerServer {
public:
    /** @param store Storage implementation used by request handlers. */
    explicit WorkerServer(MessageStore& store);
    /** @brief Start blocking accept loop on given port. */
    void Run(std::uint16_t port);

private:
    MessageStore& store_;
};

}  // namespace db_service
