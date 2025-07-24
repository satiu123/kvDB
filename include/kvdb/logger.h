#ifndef KVDB_LOGGER_H
#define KVDB_LOGGER_H

#include <format>
#include <memory>
#include <mutex>
#include <source_location>
#include <vector>

#include "kvdb/log_sink.h"   // 依赖 Sink 接口
#include "kvdb/log_types.h"  // 依赖核心类型

namespace kvdb {

class Logger {
  public:
    static Logger& getInstance();

    void addSink(std::shared_ptr<LogSink> sink);
    void removeAllSinks();
    void setLevel(LogLevel level);
    [[nodiscard]] bool shouldLog(LogLevel level) const;

    template <typename... Args>
    void log(LogLevel level, const std::source_location& loc,
             const std::format_string<Args...>& fmt, Args&&... args);

  private:
    Logger();
    ~Logger();
    std::vector<std::shared_ptr<LogSink>> sinks_;
    LogLevel level_ = LogLevel::DEBUG;
    mutable std::mutex mutex_;  // 注意：shouldLog 是 const 函数，所以 mutex_ 需要是 mutable
};

// 模板函数的实现需要放在头文件中
template <typename... Args>
void Logger::log(LogLevel level, const std::source_location& loc,
                 const std::format_string<Args...>& fmt, Args&&... args) {
    // LogRecord 的创建可以放到锁外，格式化消息是耗时操作
    LogRecord record(level, std::chrono::system_clock::now(),
                     std::vformat(fmt.get(), std::make_format_args(args...)), loc.file_name(),
                     static_cast<int>(loc.line()));

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& sink : sinks_) {
        sink->log(record);
    }
}

}  // namespace kvdb

#endif  // KVDB_LOGGER_H