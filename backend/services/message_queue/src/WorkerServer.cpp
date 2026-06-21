#include "WorkerServer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Logger.h"

namespace message_queue {
namespace {

std::string ReadLine(int fd) {
    std::string line;
    char c = '\0';
    while (true) {
        const ssize_t bytes = recv(fd, &c, 1, 0);
        if (bytes <= 0) {
            return "";
        }
        if (c == '\n') {
            // Line-delimited protocol: one request per connection.
            break;
        }
        line.push_back(c);
    }
    return line;
}

void SendAll(int fd, const std::string& data) {
    std::size_t sent_total = 0;
    while (sent_total < data.size()) {
        const ssize_t sent = send(fd, data.data() + sent_total, data.size() - sent_total, MSG_NOSIGNAL);
        if (sent <= 0) {
            throw std::runtime_error("failed to send response");
        }
        sent_total += static_cast<std::size_t>(sent);
    }
}

std::vector<std::string> Split(const std::string& line, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream stream(line);
    while (std::getline(stream, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string UrlEncode(const std::string& input) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << static_cast<char>(c);
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return encoded.str();
}

std::string UrlDecode(const std::string& input) {
    std::string decoded;
    decoded.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            const std::string hex = input.substr(i + 1, 2);
            const char c = static_cast<char>(std::stoi(hex, nullptr, 16));
            decoded.push_back(c);
            i += 2;
        } else {
            decoded.push_back(input[i]);
        }
    }
    return decoded;
}

std::uint64_t ToEpochMs(const std::chrono::system_clock::time_point& tp) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count());
}

std::string SerializeMessage(const ChatMessage& message) {
    return std::to_string(message.message_id) +
           "|" + UrlEncode(message.message_from) +
           "|" + UrlEncode(message.message_to) +
           "|" + UrlEncode(message.content) +
           "|" + std::to_string(ToEpochMs(message.created_at));
}

void HandleClient(int client_fd, MessageQueue& queue) {
    try {
        const std::string request = ReadLine(client_fd);
        if (request.empty()) {
            close(client_fd);
            return;
        }

        const std::vector<std::string> tokens = Split(request, '|');
        if (tokens.empty()) {
            SendAll(client_fd, "ERR|empty request\n");
            close(client_fd);
            return;
        }

        if (tokens[0] == "PUBLISH") {
            if (tokens.size() != 4) {
                SendAll(client_fd, "ERR|invalid publish request\n");
                close(client_fd);
                return;
            }

            ChatMessage message;
            message.message_from = UrlDecode(tokens[1]);
            message.message_to = UrlDecode(tokens[2]);
            message.content = UrlDecode(tokens[3]);

            logging::Logger::Instance().Info(
                "MessageQueueWorker",
                "PUBLISH from=" + message.message_from + " to=" + message.message_to);

            auto completion = std::make_shared<std::promise<ChatMessage>>();
            std::future<ChatMessage> persisted_future = completion->get_future();
            // Queue write is async internally, but API remains sync by awaiting completion.
            queue.Publish(MessageEvent{message, completion});
            const ChatMessage persisted = persisted_future.get();
            logging::Logger::Instance().Info(
                "MessageQueueWorker",
                "persisted message_id=" + std::to_string(persisted.message_id));

            SendAll(client_fd, "OK|" + SerializeMessage(persisted) + "\n");
            close(client_fd);
            return;
        }

        SendAll(client_fd, "ERR|unknown command\n");
        close(client_fd);
    } catch (const std::exception& ex) {
        try {
            SendAll(client_fd, std::string("ERR|") + ex.what() + "\n");
        } catch (...) {
        }
        close(client_fd);
    }
}

}  // namespace

WorkerServer::WorkerServer(MessageQueue& queue) : queue_(queue) {}

void WorkerServer::Run(std::uint16_t port) {
    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::runtime_error("failed to create worker socket");
    }

    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(listen_fd);
        throw std::runtime_error("failed to bind worker socket");
    }

    if (listen(listen_fd, 64) < 0) {
        close(listen_fd);
        throw std::runtime_error("failed to listen on worker socket");
    }

    logging::Logger::Instance().Info(
        "MessageQueueWorker",
        "listening on 0.0.0.0:" + std::to_string(port));

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            continue;
        }

        std::thread([client_fd, this]() {
            // Keep accept loop responsive; each client request is handled on its own thread.
            HandleClient(client_fd, queue_);
        }).detach();
    }
}

}  // namespace message_queue