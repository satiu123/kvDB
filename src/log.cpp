#include "kvdb/log.h"
#include <format>
#include <iostream>
std::string kvdb::LogRecord::toString() const {
    return std::format(FORMATSTR,
        timestamp_,
        logLevelToString(level_),
        sourceFile_,
        lineNumber_,
        message_
        );
}


kvdb::FileSink::FileSink(std::string_view filePath) : filePath_(filePath) {
    fileStream_.open(filePath_, std::ios::app);
    if (!fileStream_) {
        throw std::runtime_error("Failed to open log file: " + filePath_);
    }
}
kvdb::FileSink::~FileSink() {
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
}
void kvdb::FileSink::log(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_) {
        fileStream_ << record.toString() << '\n';
    } else {
        throw std::runtime_error("Log file stream is not open: " + filePath_);
    }
}
void kvdb::FileSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_) {
        fileStream_.flush();
    } else {
        throw std::runtime_error("Log file stream is not open: " + filePath_);
    }
}


void kvdb::ConsoleSink::log(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << record.toString() << '\n';
}

void kvdb::ConsoleSink::flush() {
    // 控制台通常不需要刷新，但可以实现为无操作
}

void kvdb::Logger::addSink(std::shared_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink)); // 使用 std::move 避免不必要的复制
}
void kvdb::Logger::removeAllSinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear(); // 清空所有接收器
}
void kvdb::Logger::setLevel(LogLevel level) {
    level_ = level; // 设置当前日志级别
}
bool kvdb::Logger::shouldLog(LogLevel level) const {
    return level >= level_; 
}
// 模板函数实现已移至头文件