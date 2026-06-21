#pragma once

#include <cstdint>

#include "MessageQueue.h"

namespace message_queue {

/**
 * @brief TCP ingress server for message publish requests.
 *
 * This component accepts `PUBLISH` commands and enqueues events for subscriber
 * processing. It does not own persistence logic directly.
 */
class WorkerServer {
public:
    /** @param queue Shared queue where incoming publish requests are pushed. */
    explicit WorkerServer(MessageQueue& queue);
    /** @brief Start blocking accept loop on the given port. */
    void Run(std::uint16_t port);

private:
    MessageQueue& queue_;
};

}  // namespace message_queue
