#include "kvdb/sinks/file_sink.h"
namespace kvdb {

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
// 文件日志记录器实现
void kvdb::FileSink::log(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_) {
        fileStream_ << record.toString() << '\n';
    } else {
        throw std::runtime_error("Log file stream is not open: " + filePath_);
    }
}
// 刷新文件流
void kvdb::FileSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_) {
        fileStream_.flush();
    } else {
        throw std::runtime_error("Log file stream is not open: " + filePath_);
    }
}
}