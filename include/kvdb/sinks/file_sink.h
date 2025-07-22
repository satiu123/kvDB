#ifndef KVDB_SINKS_FILE_SINK_H
#define KVDB_SINKS_FILE_SINK_H

#include "kvdb/log_sink.h" // 依赖 Sink 接口
#include <fstream>
#include <mutex>
#include <string_view>

namespace kvdb {

// 文件日志记录器
class FileSink : public LogSink {
    private:
        std::ofstream fileStream_;
        std::string filePath_;
        std::mutex mutex_; // 确保线程安全

    public:
        explicit FileSink(std::string_view filePath);
        ~FileSink() override;
       
        void log(const LogRecord& record) override; // 实现日志记录
        void flush() override; // 刷新文件流
};

} // namespace kvdb

#endif // KVDB_SINKS_FILE_SINK_H