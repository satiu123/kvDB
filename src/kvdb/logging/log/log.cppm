export module kvdb.logging.log.log_impl;

import std;
import kvdb.logging.log.logger;
import kvdb.logging.log.log_record;
export namespace kvdb::logging {
// 使用CRTP的基类
template <typename Derived>
class LOG_BASE {
  public:
    LOG_BASE(std::source_location loc = std::source_location::current()) : loc_(loc) {}

    template <typename... Args>
    void operator()(const std::format_string<Args...>& fmt, Args&&... args) const {
        // 直接使用派生类中定义的LEVEL
        Logger::getInstance().log(Derived::LEVEL, loc_, fmt, std::forward<Args>(args)...);
    }

  protected:
    std::source_location loc_;
};

class LOG_DEBUG : public LOG_BASE<LOG_DEBUG> {
  public:
    static constexpr LogLevel LEVEL = LogLevel::DEBUG;
    LOG_DEBUG(std::source_location loc = std::source_location::current())
        : LOG_BASE<LOG_DEBUG>(loc) {}       // 使用基类构造函数
    using LOG_BASE<LOG_DEBUG>::operator();  // 继承基类的操作
};

class LOG_INFO : public LOG_BASE<LOG_INFO> {
  public:
    static constexpr LogLevel LEVEL = LogLevel::INFO;
    LOG_INFO(std::source_location loc = std::source_location::current())
        : LOG_BASE<LOG_INFO>(loc) {}       // 使用基类构造函数
    using LOG_BASE<LOG_INFO>::operator();  // 继承基类的操作
};

class LOG_WARNING : public LOG_BASE<LOG_WARNING> {
  public:
    static constexpr LogLevel LEVEL = LogLevel::WARNING;
    LOG_WARNING(std::source_location loc = std::source_location::current())
        : LOG_BASE<LOG_WARNING>(loc) {}       // 使用基类构造函数
    using LOG_BASE<LOG_WARNING>::operator();  // 继承基类的操作
};

class LOG_ERROR : public LOG_BASE<LOG_ERROR> {
  public:
    static constexpr LogLevel LEVEL = LogLevel::ERROR;
    LOG_ERROR(std::source_location loc = std::source_location::current())
        : LOG_BASE<LOG_ERROR>(loc) {}       // 使用基类构造函数
    using LOG_BASE<LOG_ERROR>::operator();  // 继承基类的操作
};

}  // namespace kvdb::logging