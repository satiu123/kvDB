export module kvdb.logging.sinks.file_sink;

import std;
import kvdb.logging.sinks.log_sink;
import kvdb.logging.log.log_record;
import kvdb.core.types;
export namespace kvdb::logging {
using kvdb::core::types::Result;

// 文件日志接收器
class FileSink : public LogSink {
  public:
    ~FileSink() override;

    // 工厂函数，用于安全地创建FileSink实例
    static Result<std::shared_ptr<FileSink>> create(std::string_view filePath);

    bool log(const logging::LogRecord& record) override;  // 实现日志记录
    bool flush() override;                                // 刷新文件流

  private:
    explicit FileSink(std::string_view filePath);
    bool open();

    std::ofstream fileStream_;
    std::string filePath_;
    std::mutex mutex_;  // 确保线程安全
};

}  // namespace kvdb::logging
