#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace logging {
namespace {

std::mutex g_log_mutex;

const char* ToString(LogLevel level) {
    switch (level) {
        case LogLevel::kDebug:
            return "DEBUG";
        case LogLevel::kInfo:
            return "INFO";
        case LogLevel::kWarn:
            return "WARN";
        case LogLevel::kError:
            return "ERROR";
    }
    return "UNKNOWN";
}

std::string CurrentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
    localtime_r(&time, &tm);

    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

}  // namespace

Logger& Logger::Instance() {
    static Logger logger;
    return logger;
}

void Logger::SetMinLevel(LogLevel min_level) {
    min_level_ = min_level;
}

void Logger::Debug(std::string_view component, const std::string& message) {
    Log(LogLevel::kDebug, component, message);
}

void Logger::Info(std::string_view component, const std::string& message) {
    Log(LogLevel::kInfo, component, message);
}

void Logger::Warn(std::string_view component, const std::string& message) {
    Log(LogLevel::kWarn, component, message);
}

void Logger::Error(std::string_view component, const std::string& message) {
    Log(LogLevel::kError, component, message);
}

void Logger::Log(LogLevel level, std::string_view component, const std::string& message) {
    if (level < min_level_) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::clog << "[" << CurrentTimestamp() << "]"
              << " [" << ToString(level) << "]"
              << " [" << component << "]"
              << " [thread " << std::this_thread::get_id() << "] "
              << message << std::endl;
}

}  // namespace logging
