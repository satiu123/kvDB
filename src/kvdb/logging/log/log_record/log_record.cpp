module kvdb.logging.log.log_record;
import std;

namespace kvdb::logging {
LogRecord::LogRecord(LogLevel lvl, std::chrono::system_clock::time_point ts, std::string msg,
                     std::string src, int line)
    : level_(lvl),
      timestamp_(convertTimeStamp(ts)),
      message_(std::move(msg)),
      sourceFile_(std::move(src)),
      lineNumber_(line) {}


std::string LogRecord::convertTimeStamp(std::chrono::system_clock::time_point ts) {
    return std::format("{:%Y-%m-%d %H:%M:%S}", ts);
}
std::string LogRecord::toString() const {
    return std::format(FORMATSTR, timestamp_, logLevelToString(level_), sourceFile_, lineNumber_,
                       message_);
}
}  // namespace kvdb::logging
