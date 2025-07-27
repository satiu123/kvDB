export module kvdb.logging.sinks.console_sink;


import std;
import kvdb.logging.sinks.log_sink;
import kvdb.logging.log.log_record;


export namespace kvdb {
// 控制台日志记录器
class ConsoleSink : public LogSink {
  public:
    // 是否只打印message
    // 构造函数，控制是否仅显示消息内容
    explicit ConsoleSink(bool message_only = false);

    // 是否只打印消息内容，不包含其他元数据
    bool message_only_ = false;
    void log(const logging::LogRecord& record) override;  // 实现日志记录到控制台
    void flush() override;                                // 控制台通常不需要刷新
  private:
    std::mutex mutex_;  // 确保线程安全
};
}  // namespace kvdb
