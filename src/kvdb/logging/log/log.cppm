export module kvdb.logging.log;

import std;
export import kvdb.logging.log.logger;
export import kvdb.logging.log.log_record;

namespace kvdb::logging {

class LOG_BASE {
  public:
    explicit LOG_BASE(std::source_location loc = std::source_location::current()) : loc_(loc) {}
    template <typename... Args>
    void operator()(this auto&& self, const std::format_string<Args...>& fmt, Args&&... args) {
        if (!self.logger_.isEnabled() or !self.logger_.shouldLog(self.LEVEL)) {
            return;  // 如果当前日志级别不允许，则直接返回
        }
        self.logger_.log(self.LEVEL, self.loc_, fmt, std::forward<Args>(args)...);
    }

  private:
    Logger& logger_ = Logger::getInstance();
    std::source_location loc_;
};

class LogDebugImpl : public LOG_BASE {
  public:
    static constexpr LogLevel LEVEL = LogLevel::DEBUG;
    explicit LogDebugImpl(std::source_location loc = std::source_location::current())
        : LOG_BASE(loc) {}
};

class LogInfoImpl : public LOG_BASE {
  public:
    static constexpr LogLevel LEVEL = LogLevel::INFO;
    explicit LogInfoImpl(std::source_location loc = std::source_location::current())
        : LOG_BASE(loc) {}
};

class LogWarningImpl : public LOG_BASE {
  public:
    static constexpr LogLevel LEVEL = LogLevel::WARNING;
    explicit LogWarningImpl(std::source_location loc = std::source_location::current())
        : LOG_BASE(loc) {}
};

class LogErrorImpl : public LOG_BASE {
  public:
    static constexpr LogLevel LEVEL = LogLevel::ERROR;
    explicit LogErrorImpl(std::source_location loc = std::source_location::current())
        : LOG_BASE(loc) {}
};

}  // namespace kvdb::logging

// 导出日志记录器函数
// 这些函数返回一个可调用对象，可以直接使用格式化字符串记录日志
export namespace kvdb::logging {

[[nodiscard]] auto LOG_DEBUG(std::source_location loc = std::source_location::current()) {
    return LogDebugImpl(loc);
}

[[nodiscard]] auto LOG_INFO(std::source_location loc = std::source_location::current()) {
    return LogInfoImpl(loc);
}

[[nodiscard]] auto LOG_WARNING(std::source_location loc = std::source_location::current()) {
    return LogWarningImpl(loc);
}

[[nodiscard]] auto LOG_ERROR(std::source_location loc = std::source_location::current()) {
    return LogErrorImpl(loc);
}

}  // namespace kvdb::logging