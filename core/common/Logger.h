#pragma once
// core/common/Logger.h
// Minimal leveled logger for the Lodestar core. Writes to stderr and an
// optional log file. Thread-safe via a mutex.

#include <cstdio>
#include <mutex>
#include <string>

namespace lodestar::common {

enum class LogLevel { Trace = 0, Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void setLevel(LogLevel level);
    void setFile(const std::string& path);

    void log(LogLevel level, const std::string& message);
    void trace(const std::string& m) { log(LogLevel::Trace, m); }
    void debug(const std::string& m) { log(LogLevel::Debug, m); }
    void info(const std::string& m) { log(LogLevel::Info, m); }
    void warn(const std::string& m) { log(LogLevel::Warn, m); }
    void error(const std::string& m) { log(LogLevel::Error, m); }

    static const char* levelName(LogLevel level);

private:
    Logger() = default;
    ~Logger();

    LogLevel level_ = LogLevel::Info;
    std::mutex mutex_;
    std::FILE* file_ = nullptr;
};

}  // namespace lodestar::common
