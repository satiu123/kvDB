#include "kvdb/logging/log_types.h"
namespace kvdb {

kvdb::LogRecord::LogRecord(LogLevel lvl, std::chrono::system_clock::time_point ts, std::string msg,
                           std::string src, int line)
    : level_(lvl),
      timestamp_(convertTimeStamp(ts)),
      message_(std::move(msg)),
      sourceFile_(std::move(src)),
      lineNumber_(line) {}

std::string kvdb::LogRecord::convertTimeStamp(std::chrono::system_clock::time_point ts) const {
    auto timeTPoint = std::chrono::system_clock::to_time_t(ts);
    std::tm localTime = *std::localtime(&timeTPoint);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch() %
                                                                    std::chrono::seconds(1));
    std::array<char, TIMELENTH> buffer{};  // Buffer for formatted timestamp
    size_t result = std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S", &localTime);
    if (result == 0)
        return "error formatting time";
    else
        return std::format("{}.{:03d}", buffer.data(), ms.count());
}
std::string kvdb::LogRecord::toString() const {
    return std::format(FORMATSTR, timestamp_, logLevelToString(level_), sourceFile_, lineNumber_,
                       message_);
}

}  // namespace kvdb