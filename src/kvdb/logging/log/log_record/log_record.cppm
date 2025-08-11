export module kvdb.logging.log.log_record;

import std;

export namespace kvdb::logging {

// 日志级别枚举
enum class LogLevel : std::uint8_t { DEBUG, INFO, WARNING, ERROR };

// 将日志级别转换为字符串
inline const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}
// 日志记录类
class LogRecord {
  public:
    LogRecord(LogLevel lvl, std::chrono::system_clock::time_point ts, std::string msg,
              std::string src, int line);

    static std::string convertTimeStamp(std::chrono::system_clock::time_point ts);
    [[nodiscard]] std::string toString() const;
    [[nodiscard]] const std::string& getMessage() const {
        return message_;
    }

  private:
    LogLevel level_;
    std::string timestamp_;
    std::string message_;
    std::string sourceFile_;
    int lineNumber_;
    static constexpr int TIMELENTH{20};
    static constexpr const char* FORMATSTR{"{} [{}] {}:{}: {}"};
};

}  // namespace kvdb::logging
