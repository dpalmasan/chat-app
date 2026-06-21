#pragma once

#include <string>
#include <string_view>

namespace chat {

enum class LogLevel {
    kDebug = 0,
    kInfo = 1,
    kWarn = 2,
    kError = 3,
};

class Logger {
public:
    static Logger& Instance();

    void SetMinLevel(LogLevel min_level);

    void Debug(std::string_view component, const std::string& message);
    void Info(std::string_view component, const std::string& message);
    void Warn(std::string_view component, const std::string& message);
    void Error(std::string_view component, const std::string& message);

private:
    Logger() = default;

    void Log(LogLevel level, std::string_view component, const std::string& message);

    LogLevel min_level_ = LogLevel::kDebug;
};

}  // namespace chat
