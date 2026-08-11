// core/common/Logger.cpp
#include "core/common/Logger.h"

#include <ctime>

namespace lodestar::common {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
    if (!path.empty()) {
        file_ = std::fopen(path.c_str(), "a");
    }
}

const char* Logger::levelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(level_)) {
        return;
    }
    std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char stamp[32] = {0};
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm);

    std::string line = std::string("[") + stamp + "] [" + levelName(level) + "] " + message + "\n";
    std::fputs(line.c_str(), stderr);
    if (file_ != nullptr) {
        std::fputs(line.c_str(), file_);
        std::fflush(file_);
    }
}

}  // namespace lodestar::common
