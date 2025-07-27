module kvdb.logging.log;

import std;

Logger& Logger::getInstance() {
    static Logger instance;  // 使用局部静态变量确保单例
    return instance;
}
Logger::Logger() = default;
Logger::~Logger() = default;
void kvdb::Logger::addSink(std::shared_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));  // 使用 std::move 避免不必要的复制
}
void kvdb::Logger::removeAllSinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();  // 清空所有接收器
}
void kvdb::Logger::setLevel(LogLevel level) {
    level_ = level;  // 设置当前日志级别
}
bool kvdb::Logger::shouldLog(LogLevel level) const {
    return level >= level_;
}
