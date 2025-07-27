export module kvdb.logging.log;

import std;
export import kvdb.logging.log.logger;
export import kvdb.logging.log.log_record;
export namespace kvdb::logging {
// 使用CRTP的基类
class LOG_BASE {
  public:
    LOG_BASE(std::source_location loc = std::source_location::current()) : loc_(loc) {}
    template <typename... Args>
    void operator()(this auto&& self, const std::format_string<Args...>& fmt, Args&&... args) {
        // 直接使用派生类中定义的LEVEL
        Logger::getInstance().log(self.LEVEL, self.loc_, fmt, std::forward<Args>(args)...);
    }

  protected:
    std::source_location loc_;
};

class LOG_DEBUG : public LOG_BASE {
  public:
    static constexpr LogLevel LEVEL = LogLevel::DEBUG;
    LOG_DEBUG(std::source_location loc = std::source_location::current())
        : LOG_BASE(loc) {}  // 使用基类构造函数
};

class LOG_INFO : public LOG_BASE {
  public:
    static constexpr LogLevel LEVEL = LogLevel::INFO;
    LOG_INFO(std::source_location loc = std::source_location::current())
        : LOG_BASE(loc) {}  // 使用基类构造函数
};

class LOG_WARNING : public LOG_BASE {
  public:
    static constexpr LogLevel LEVEL = LogLevel::WARNING;
    LOG_WARNING(std::source_location loc = std::source_location::current())
        : LOG_BASE(loc) {}  // 使用基类构造函数
};

class LOG_ERROR : public LOG_BASE {
  public:
    static constexpr LogLevel LEVEL = LogLevel::ERROR;
    LOG_ERROR(std::source_location loc = std::source_location::current())
        : LOG_BASE(loc) {}  // 使用基类构造函数
};

}  // namespace kvdb::logging