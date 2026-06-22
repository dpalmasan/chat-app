#include "RemoteMessageStore.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace chat {
namespace {

std::string ReadLine(int fd) {
    std::string line;
    char c = '\0';
    while (true) {
        const ssize_t bytes = recv(fd, &c, 1, 0);
        if (bytes <= 0) {
            throw std::runtime_error("connection closed while reading response");
        }
        if (c == '\n') {
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
            throw std::runtime_error("failed to send request");
        }
        sent_total += static_cast<std::size_t>(sent);
    }
}

std::chrono::system_clock::time_point EpochMsToTimePoint(std::uint64_t epoch_ms) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(epoch_ms));
}

std::uint64_t ParseUint64(const std::string& text) {
    return static_cast<std::uint64_t>(std::stoull(text));
}

}  // namespace

RemoteMessageStore::RemoteMessageStore(std::string host, std::uint16_t port)
    : host_(std::move(host)), port_(port) {}

ChatMessage RemoteMessageStore::SaveMessage(ChatMessage message) {
    const std::string request =
        "SAVE|" + UrlEncode(message.message_from) +
        "|" + UrlEncode(message.message_to) +
        "|" + UrlEncode(message.content) + "\n";

    return ParseMessageLine(SendRequest(request));
}

std::vector<ChatMessage> RemoteMessageStore::LoadPendingMessagesFor(const UserId& recipient_id) {
    const std::string request = "LOAD_PENDING|" + UrlEncode(recipient_id) + "\n";
    const std::string response = SendRequest(request);

    const std::vector<std::string> entries = Split(response, ';');
    std::vector<ChatMessage> messages;
    messages.reserve(entries.size());
    for (const std::string& entry : entries) {
        if (entry.empty()) {
            continue;
        }
        messages.push_back(ParseMessageLine(entry));
    }
    return messages;
}

void RemoteMessageStore::MarkMessageDelivered(const UserId& recipient_id, MessageId message_id) {
    const std::string request =
        "MARK_DELIVERED|" + UrlEncode(recipient_id) +
        "|" + std::to_string(message_id) + "\n";
    static_cast<void>(SendRequest(request));
}

bool RemoteMessageStore::UserExists(const UserId& user_id) {
    const std::string request = "USER_EXISTS|" + UrlEncode(user_id) + "\n";
    return SendRequest(request) == "1";
}

void RemoteMessageStore::SetPresence(const UserId& user_id, bool is_online) {
    const std::string request =
        "SET_PRESENCE|" + UrlEncode(user_id) +
        "|" + (is_online ? "online" : "offline") + "\n";
    static_cast<void>(SendRequest(request));
}

std::vector<MessageStore::UserPresence> RemoteMessageStore::ListUsers() {
    const std::string request = "LIST_USERS\n";
    const std::string response = SendRequest(request);

    const std::vector<std::string> entries = Split(response, ';');
    std::vector<UserPresence> users;
    users.reserve(entries.size());
    for (const std::string& entry : entries) {
        if (entry.empty()) {
            continue;
        }
        users.push_back(ParseUserLine(entry));
    }
    return users;
}

std::string RemoteMessageStore::SendRequest(const std::string& request) {
    const std::string port_text = std::to_string(port_);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(host_.c_str(), port_text.c_str(), &hints, &result) != 0 || result == nullptr) {
        throw std::runtime_error("failed to resolve remote db service host");
    }

    int sock = -1;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) {
            continue;
        }
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(result);

    if (sock < 0) {
        throw std::runtime_error("failed to connect to remote db service");
    }

    SendAll(sock, request);
    const std::string line = ReadLine(sock);
    close(sock);

    if (line.rfind("OK|", 0) == 0) {
        return line.substr(3);
    }
    if (line.rfind("ERR|", 0) == 0) {
        throw std::runtime_error("remote store error: " + line.substr(4));
    }
    throw std::runtime_error("invalid remote store response");
}

std::string RemoteMessageStore::UrlEncode(const std::string& input) {
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

std::string RemoteMessageStore::UrlDecode(const std::string& input) {
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

std::vector<std::string> RemoteMessageStore::Split(const std::string& line, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream stream(line);
    while (std::getline(stream, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

ChatMessage RemoteMessageStore::ParseMessageLine(const std::string& line) {
    const std::vector<std::string> tokens = Split(line, '|');
    if (tokens.size() != 5) {
        throw std::runtime_error("invalid message payload");
    }

    ChatMessage message;
    message.message_id = ParseUint64(tokens[0]);
    message.message_from = UrlDecode(tokens[1]);
    message.message_to = UrlDecode(tokens[2]);
    message.content = UrlDecode(tokens[3]);
    message.created_at = EpochMsToTimePoint(ParseUint64(tokens[4]));
    return message;
}

MessageStore::UserPresence RemoteMessageStore::ParseUserLine(const std::string& line) {
    const std::vector<std::string> tokens = Split(line, '|');
    if (tokens.size() != 3) {
        throw std::runtime_error("invalid user payload");
    }

    UserPresence user;
    user.user_id = UrlDecode(tokens[0]);
    if (tokens[1] == "online") {
        user.is_online = true;
    } else if (tokens[1] == "offline") {
        user.is_online = false;
    } else {
        throw std::runtime_error("invalid user status payload");
    }
    user.last_active_at = EpochMsToTimePoint(ParseUint64(tokens[2]));
    return user;
}

}  // namespace chat
