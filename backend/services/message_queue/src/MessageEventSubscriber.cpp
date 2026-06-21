#include "MessageEventSubscriber.h"

#include "Logger.h"

namespace message_queue {

MessageEventSubscriber::MessageEventSubscriber(MessageQueue& queue, MessageStore& store)
    : queue_(queue), store_(store), worker_thread_(&MessageEventSubscriber::Run, this), started_(true) {}

MessageEventSubscriber::~MessageEventSubscriber() {
    Stop();
}

void MessageEventSubscriber::Stop() {
    if (!started_) {
        return;
    }

    queue_.Shutdown();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    started_ = false;
}

void MessageEventSubscriber::Run() {
    logging::Logger::Instance().Info("MessageQueueWorker", "subscriber started");
    MessageEvent event;
    while (queue_.Pop(&event)) {
        try {
            ChatMessage persisted = store_.SaveMessage(event.message);
            logging::Logger::Instance().Debug(
                "MessageQueueWorker",
                "subscriber stored message_id=" + std::to_string(persisted.message_id));
            if (event.completion) {
                event.completion->set_value(persisted);
            }
        } catch (...) {
            if (event.completion) {
                event.completion->set_exception(std::current_exception());
            }
        }
    }
    logging::Logger::Instance().Info("MessageQueueWorker", "subscriber stopped");
}

}  // namespace message_queue
