#pragma once

#include <cstdint>

#include "MessageQueue.h"

namespace message_queue {

class WorkerServer {
public:
    explicit WorkerServer(MessageQueue& queue);
    void Run(std::uint16_t port);

private:
    MessageQueue& queue_;
};

}  // namespace message_queue
