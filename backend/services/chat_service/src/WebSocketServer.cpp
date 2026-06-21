#include "WebSocketServer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ChatMessage.h"
#include "ChatService.h"
#include "Logger.h"

namespace chat {
namespace {

constexpr int kListenBacklog = 32;
constexpr std::size_t kMaxFrameSize = 1 << 20;

std::string Base64Encode(const unsigned char* data, std::size_t len) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (std::size_t i = 0; i < len; i += 3) {
        const std::uint32_t octet_a = data[i];
        const std::uint32_t octet_b = (i + 1 < len) ? data[i + 1] : 0;
        const std::uint32_t octet_c = (i + 2 < len) ? data[i + 2] : 0;
        const std::uint32_t triple = (octet_a << 16U) | (octet_b << 8U) | octet_c;

        out.push_back(kAlphabet[(triple >> 18U) & 0x3FU]);
        out.push_back(kAlphabet[(triple >> 12U) & 0x3FU]);
        out.push_back(i + 1 < len ? kAlphabet[(triple >> 6U) & 0x3FU] : '=');
        out.push_back(i + 2 < len ? kAlphabet[triple & 0x3FU] : '=');
    }

    return out;
}

std::string ComputeWebSocketAccept(const std::string& key) {
    static const std::string kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const std::string input = key + kGuid;

    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
    return Base64Encode(digest, SHA_DIGEST_LENGTH);
}

bool SendAll(int fd, const unsigned char* data, std::size_t size) {
    std::size_t total_sent = 0;
    while (total_sent < size) {
        const ssize_t sent =
            send(fd, data + total_sent, size - total_sent, MSG_NOSIGNAL);
        if (sent <= 0) {
            return false;
        }
        total_sent += static_cast<std::size_t>(sent);
    }
    return true;
}

bool RecvAll(int fd, unsigned char* data, std::size_t size) {
    std::size_t total_read = 0;
    while (total_read < size) {
        const ssize_t count = recv(fd, data + total_read, size - total_read, 0);
        if (count <= 0) {
            return false;
        }
        total_read += static_cast<std::size_t>(count);
    }
    return true;
}

std::optional<std::string> ParseHeaderValue(const std::string& request, const std::string& name) {
    const std::string token = name + ":";
    const std::size_t header_pos = request.find(token);
    if (header_pos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t value_start = header_pos + token.size();
    while (value_start < request.size() && request[value_start] == ' ') {
        ++value_start;
    }
    const std::size_t value_end = request.find("\r\n", value_start);
    if (value_end == std::string::npos) {
        return std::nullopt;
    }

    return request.substr(value_start, value_end - value_start);
}

std::optional<std::string> ParseClientIdFromRequestLine(const std::string& request) {
    const std::size_t line_end = request.find("\r\n");
    if (line_end == std::string::npos) {
        return std::nullopt;
    }

    const std::string request_line = request.substr(0, line_end);
    const std::size_t first_space = request_line.find(' ');
    if (first_space == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t second_space = request_line.find(' ', first_space + 1);
    if (second_space == std::string::npos) {
        return std::nullopt;
    }

    const std::string target = request_line.substr(first_space + 1, second_space - first_space - 1);
    const std::string marker = "client_id=";
    const std::size_t marker_pos = target.find(marker);
    if (marker_pos == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t value_start = marker_pos + marker.size();
    std::size_t value_end = target.find('&', value_start);
    if (value_end == std::string::npos) {
        value_end = target.size();
    }
    if (value_start >= value_end) {
        return std::nullopt;
    }

    return target.substr(value_start, value_end - value_start);
}

bool ReadHttpRequest(int fd, std::string* out_request) {
    out_request->clear();
    char buffer[2048];

    while (out_request->find("\r\n\r\n") == std::string::npos) {
        const ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
        if (count <= 0) {
            return false;
        }
        out_request->append(buffer, static_cast<std::size_t>(count));
        if (out_request->size() > 32 * 1024) {
            return false;
        }
    }

    return true;
}

bool PerformHandshake(int fd, std::string* client_id) {
    std::string request;
    if (!ReadHttpRequest(fd, &request)) {
        return false;
    }

    const auto sec_key = ParseHeaderValue(request, "Sec-WebSocket-Key");
    const auto parsed_client_id = ParseClientIdFromRequestLine(request);
    if (!sec_key.has_value() || !parsed_client_id.has_value()) {
        return false;
    }

    const std::string accept = ComputeWebSocketAccept(sec_key.value());
    const std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " +
        accept + "\r\n\r\n";

    if (!SendAll(fd, reinterpret_cast<const unsigned char*>(response.data()), response.size())) {
        return false;
    }

    *client_id = parsed_client_id.value();
    return true;
}

std::optional<std::string> ReadTextFrame(int fd) {
    unsigned char header[2];
    if (!RecvAll(fd, header, sizeof(header))) {
        return std::nullopt;
    }

    const unsigned char opcode = header[0] & 0x0F;
    if (opcode == 0x8) {
        return std::nullopt;
    }
    if (opcode != 0x1) {
        return std::nullopt;
    }

    const bool masked = (header[1] & 0x80U) != 0;
    if (!masked) {
        return std::nullopt;
    }

    std::uint64_t payload_len = header[1] & 0x7FU;
    if (payload_len == 126) {
        unsigned char ext[2];
        if (!RecvAll(fd, ext, sizeof(ext))) {
            return std::nullopt;
        }
        payload_len = (static_cast<std::uint64_t>(ext[0]) << 8U) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8];
        if (!RecvAll(fd, ext, sizeof(ext))) {
            return std::nullopt;
        }
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8U) | ext[i];
        }
    }

    if (payload_len > kMaxFrameSize) {
        return std::nullopt;
    }

    unsigned char mask[4];
    if (!RecvAll(fd, mask, sizeof(mask))) {
        return std::nullopt;
    }

    std::string payload(payload_len, '\0');
    if (!RecvAll(fd, reinterpret_cast<unsigned char*>(&payload[0]), payload_len)) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
    }
    return payload;
}

bool SendTextFrame(int fd, const std::string& payload) {
    std::vector<unsigned char> frame;
    frame.reserve(payload.size() + 10);
    frame.push_back(0x81U);

    if (payload.size() < 126) {
        frame.push_back(static_cast<unsigned char>(payload.size()));
    } else if (payload.size() <= 0xFFFF) {
        frame.push_back(126U);
        frame.push_back(static_cast<unsigned char>((payload.size() >> 8U) & 0xFFU));
        frame.push_back(static_cast<unsigned char>(payload.size() & 0xFFU));
    } else {
        frame.push_back(127U);
        const std::uint64_t size = payload.size();
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<unsigned char>((size >> shift) & 0xFFU));
        }
    }

    frame.insert(frame.end(), payload.begin(), payload.end());
    return SendAll(fd, frame.data(), frame.size());
}

std::optional<std::string> ExtractJsonStringValue(const std::string& json, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const std::size_t key_pos = json.find(marker);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t colon_pos = json.find(':', key_pos + marker.size());
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t quote_start = json.find('"', colon_pos + 1);
    if (quote_start == std::string::npos) {
        return std::nullopt;
    }
    ++quote_start;

    std::string value;
    bool escaped = false;
    for (std::size_t i = quote_start; i < json.size(); ++i) {
        const char c = json[i];
        if (escaped) {
            value.push_back(c);
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            return value;
        }
        value.push_back(c);
    }

    return std::nullopt;
}

std::string JsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

std::string ToIso8601(const std::chrono::system_clock::time_point& ts) {
    const auto raw_time = std::chrono::system_clock::to_time_t(ts);
    std::tm tm{};
    gmtime_r(&raw_time, &tm);

    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::string BuildChatMessageJson(const ChatMessage& message) {
    return "{"
           "\"type\":\"chat_message\"," 
           "\"message_id\":" + std::to_string(message.message_id) + ","
           "\"message_from\":\"" + JsonEscape(message.message_from) + "\","
           "\"message_to\":\"" + JsonEscape(message.message_to) + "\","
           "\"content\":\"" + JsonEscape(message.content) + "\","
           "\"created_at\":\"" + ToIso8601(message.created_at) + "\""
           "}";
}

std::string BuildErrorJson(const std::string& message) {
    return "{\"type\":\"error\",\"message\":\"" + JsonEscape(message) + "\"}";
}

class SessionRegistry {
public:
    void Add(const std::string& client_id, int fd) {
        std::lock_guard<std::mutex> lock(mutex_);
        sockets_[client_id] = fd;
    }

    void Remove(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        sockets_.erase(client_id);
    }

    std::optional<int> Get(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = sockets_.find(client_id);
        if (it == sockets_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, int> sockets_;
};

SessionRegistry g_session_registry;

void HandleClientSocket(int client_fd, ChatService& chat_service) {
    std::string client_id;
    if (!PerformHandshake(client_fd, &client_id)) {
        Logger::Instance().Warn("WebSocketServer", "Handshake failed; closing client socket");
        close(client_fd);
        return;
    }

    Logger::Instance().Info("WebSocketServer", "Handshake complete for client_id=" + client_id);
    g_session_registry.Add(client_id, client_fd);
    chat_service.OnWebSocketOpen(client_id);

    const std::vector<ChatMessage> pending_messages = chat_service.LoadPendingMessagesForClient(client_id);
    for (const ChatMessage& pending_message : pending_messages) {
        const std::string pending_json = BuildChatMessageJson(pending_message);
        if (!SendTextFrame(client_fd, pending_json)) {
            Logger::Instance().Warn(
                "WebSocketServer",
                "Failed to replay pending message_id=" + std::to_string(pending_message.message_id) +
                    " client_id=" + client_id);
            break;
        }
        chat_service.MarkMessageDelivered(client_id, pending_message.message_id);
    }

    while (true) {
        const auto payload = ReadTextFrame(client_fd);
        if (!payload.has_value()) {
            break;
        }

        const auto message_from = ExtractJsonStringValue(payload.value(), "message_from");
        const auto message_to = ExtractJsonStringValue(payload.value(), "message_to");
        const auto content = ExtractJsonStringValue(payload.value(), "content");
        if (!message_from.has_value() || !message_to.has_value() || !content.has_value()) {
            SendTextFrame(client_fd, BuildErrorJson("Missing required fields"));
            continue;
        }

        ChatMessage message;
        message.message_from = message_from.value();
        message.message_to = message_to.value();
        message.content = content.value();

        try {
            const ChatMessage persisted = chat_service.OnWebSocketMessage(client_id, message);
            const std::string msg_json = BuildChatMessageJson(persisted);

            SendTextFrame(client_fd, msg_json);
            const auto recipient_fd = g_session_registry.Get(persisted.message_to);
            if (recipient_fd.has_value()) {
                if (SendTextFrame(recipient_fd.value(), msg_json)) {
                    chat_service.MarkMessageDelivered(persisted.message_to, persisted.message_id);
                }
            } else {
                Logger::Instance().Warn(
                    "WebSocketServer",
                    "Recipient not connected for message_id=" + std::to_string(persisted.message_id) +
                        " recipient=" + persisted.message_to);
            }
        } catch (const std::exception& ex) {
            Logger::Instance().Warn("WebSocketServer", std::string("Rejected message: ") + ex.what());
            SendTextFrame(client_fd, BuildErrorJson(ex.what()));
        }
    }

    g_session_registry.Remove(client_id);
    chat_service.OnWebSocketClose(client_id);
    close(client_fd);
    Logger::Instance().Info("WebSocketServer", "Client disconnected client_id=" + client_id);
}

}  // namespace

WebSocketServer::WebSocketServer(ChatService& chat_service) : chat_service_(chat_service) {}

void WebSocketServer::Run(const std::string& host, std::uint16_t port) {
    if (host != "0.0.0.0" && host != "127.0.0.1") {
        throw std::invalid_argument("Only 0.0.0.0 and 127.0.0.1 are supported in this prototype");
    }

    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (host == "0.0.0.0") {
        address.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) <= 0) {
            close(listen_fd);
            throw std::invalid_argument("Invalid bind host");
        }
    }

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(listen_fd);
        throw std::runtime_error("Bind failed");
    }

    if (listen(listen_fd, kListenBacklog) < 0) {
        close(listen_fd);
        throw std::runtime_error("Listen failed");
    }

    Logger::Instance().Info(
        "WebSocketServer",
        "Listening on ws://" + host + ":" + std::to_string(port) + "/ws?client_id=<id>");

    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);
        const int client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&client_address), &client_len);
        if (client_fd < 0) {
            Logger::Instance().Warn("WebSocketServer", "accept failed; continuing");
            continue;
        }

        std::thread([client_fd, this]() {
            HandleClientSocket(client_fd, chat_service_);
        }).detach();
    }
}

}  // namespace chat
