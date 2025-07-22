#ifndef KVDB_LOG_SINK_H
#define KVDB_LOG_SINK_H

#include "kvdb/log_types.h" // 依赖核心类型

namespace kvdb {

// 日志记录器接口
class LogSink {
    public:
        virtual ~LogSink() = default;
        virtual void log(const LogRecord& record) = 0; // 记录日志
        virtual void flush() = 0; // 刷新日志
    
    protected:
        LogSink() = default;
};

} // namespace kvdb

#endif // KVDB_LOG_SINK_H