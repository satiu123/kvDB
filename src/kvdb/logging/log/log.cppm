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
    using LOG_BASE<LOG_DEBUG>::LOG_BASE;  // 继承构造函数
};

class LOG_INFO : public LOG_BASE<LOG_INFO> {
  public:
    static constexpr LogLevel LEVEL = LogLevel::INFO;
    using LOG_BASE<LOG_INFO>::LOG_BASE;
};

class LOG_WARNING : public LOG_BASE<LOG_WARNING> {
  public:
    static constexpr LogLevel LEVEL = LogLevel::WARNING;
    using LOG_BASE<LOG_WARNING>::LOG_BASE;
};

class LOG_ERROR : public LOG_BASE<LOG_ERROR> {
  public:
    static constexpr LogLevel LEVEL = LogLevel::ERROR;
    using LOG_BASE<LOG_ERROR>::LOG_BASE;
};

}  // namespace kvdb::logging