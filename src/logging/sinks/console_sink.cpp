#include "kvdb/logging/sinks/console_sink.h"

#include <iostream>
namespace kvdb {

// 控制台日志记录器实现
void kvdb::ConsoleSink::log(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << record.toString() << '\n';
}

void kvdb::ConsoleSink::flush() {
    // 控制台通常不需要刷新，但可以实现为无操作
}

}  // namespace kvdb
