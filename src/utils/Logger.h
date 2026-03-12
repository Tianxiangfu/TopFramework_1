#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace TopOpt {

enum class LogLevel { Info, Warn, Error };

struct LogEntry {
    LogLevel    level;
    std::string message;
    std::string timestamp;
};

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void info(const std::string& msg)  { log(LogLevel::Info, msg); }
    void warn(const std::string& msg)  { log(LogLevel::Warn, msg); }
    void error(const std::string& msg) { log(LogLevel::Error, msg); }

    void log(LogLevel level, const std::string& msg) {
        LogEntry entry;
        entry.level     = level;
        entry.message   = msg;
        entry.timestamp = currentTime();
        entries_.push_back(entry);
        if (entries_.size() > maxEntries_) {
            entries_.erase(entries_.begin());
        }
    }

    const std::vector<LogEntry>& entries() const { return entries_; }

    void clear() { entries_.clear(); }

    bool autoScroll = true;

private:
    Logger() = default;
    std::vector<LogEntry> entries_;
    size_t maxEntries_ = 500;

    static std::string currentTime() {
        auto t  = std::time(nullptr);
        auto tm = std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(tm, "%H:%M:%S");
        return oss.str();
    }
};

} // namespace TopOpt
