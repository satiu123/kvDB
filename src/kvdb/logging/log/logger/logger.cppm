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

    void worker_loop();

    std::atomic<bool> done_{false};
    std::vector<std::shared_ptr<LogSink>> sinks_;
    LogLevel level_ = LogLevel::INFO;

    std::queue<LogRecord> queue_;
    mutable std::mutex sinks_mutex_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::jthread worker_thread_;
};

// 模板函数的实现需要放在模块接口文件中
template <typename... Args>
void Logger::log(LogLevel level, const std::source_location& loc,
                 const std::format_string<Args...>& fmt, Args&&... args) {
    // LogRecord 的创建和消息格式化是主要耗时操作，在锁外执行
    LogRecord record(level, std::chrono::system_clock::now(),
                     std::vformat(fmt.get(), std::make_format_args(args...)), loc.file_name(),
                     static_cast<int>(loc.line()));

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(std::move(record));
    }
    cv_.notify_one();
}

}  // namespace kvdb::logging