export module kvdb.logging.log.logger;

import std;
import kvdb.logging.log.log_record;
import kvdb.logging.sinks.log_sink;

export namespace kvdb::logging {

class Logger {
  public:
    static Logger& getInstance();

    void addSink(std::shared_ptr<LogSink> sink);
    void removeAllSinks();
    void setLevel(LogLevel level);
    [[nodiscard]] bool shouldLog(LogLevel level) const;
    [[nodiscard]] bool isEnabled() const;

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

}  // namespace kvdb::logging
