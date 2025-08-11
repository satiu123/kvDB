module kvdb.logging.sinks.console_sink;

import std;
namespace kvdb::logging {

// 控制台日志接收器构造函数
ConsoleSink::ConsoleSink(bool message_only) : message_only_(message_only) {}
// 控制台日志接收器实现
bool ConsoleSink::log(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (message_only_) {
        std::cout << record.getMessage() << std::endl;
    } else {
        std::cout << record.toString() << std::endl;
    }
    return true;
}

bool ConsoleSink::flush() {
    // 控制台通常不需要刷新，但可以实现为无操作
    return true;
}

}  // namespace kvdb::logging