#ifndef KVDB_SINKS_CONSOLE_SINK_H
#define KVDB_SINKS_CONSOLE_SINK_H

#include "kvdb/log_sink.h" // 依赖 Sink 接口
#include <mutex>

namespace kvdb {
 // 控制台日志记录器
class ConsoleSink : public LogSink {
    public:
        void log(const LogRecord& record) override; // 实现日志记录到控制台
        void flush() override; // 控制台通常不需要刷新
    private:
        std::mutex mutex_; // 确保线程安全
};
} // namespace kvdb

#endif // KVDB_SINKS_CONSOLE_SINK_H