export module kvdb.logging.sinks.file_sink;

import std;
import kvdb.logging.sinks.log_sink;
import kvdb.logging.log.log_record;
export namespace kvdb::logging {

// 文件日志记录器
class FileSink : public LogSink {
  private:
    std::ofstream fileStream_;
    std::string filePath_;
    std::mutex mutex_;  // 确保线程安全

  public:
    explicit FileSink(std::string_view filePath);
    ~FileSink() override;

    void log(const logging::LogRecord& record) override;  // 实现日志记录
    void flush() override;                                // 刷新文件流
};

}  // namespace kvdb::logging
