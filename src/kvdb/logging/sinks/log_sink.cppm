export module kvdb.logging.sinks.log_sink;

import kvdb.logging.log.log_record;

export namespace kvdb {

// 日志记录器接口
class LogSink {
  public:
    virtual ~LogSink() = default;
    virtual void log(const logging::LogRecord& record) = 0;  // 记录日志
    virtual void flush() = 0;                                // 刷新日志

  protected:
    LogSink() = default;
};

}  // namespace kvdb
