module kvdb.logging.sinks.file_sink;
import kvdb.logging.log;

namespace kvdb::logging {

// 工厂函数实现
std::expected<std::shared_ptr<FileSink>, std::string> FileSink::create(
    std::string_view filePath) {
    // 使用 new FileSink(filePath) 因为构造函数是私有的
    auto sink = std::shared_ptr<FileSink>(new FileSink(filePath));
    if (sink->open()) {
        return sink;
    }
    return std::unexpected("无法打开日志文件: " + std::string(filePath));
}

// 私有构造函数
FileSink::FileSink(std::string_view filePath) : filePath_(filePath) {}

// 打开文件
bool FileSink::open() {
    fileStream_.open(filePath_, std::ios::app);
    return fileStream_.is_open();
}

FileSink::~FileSink() {
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
}
// 文件日志接收器实现
bool FileSink::log(const logging::LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_) {
        fileStream_ << record.toString() << '\n';
        return true;
    } else {
        LOG_ERROR()("日志文件流未打开: {}", filePath_);
        return false;
    }
}
// 刷新文件流
bool FileSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_) {
        fileStream_.flush();
        return true;
    } else {
        LOG_ERROR()("日志文件流未打开: {}", filePath_);
        return false;
    }
}
}  // namespace kvdb::logging

