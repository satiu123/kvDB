#ifndef KVDB_LOG_H
#define KVDB_LOG_H
#include <array>
#include <format>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <fstream>
#include <chrono>
#include <string_view>
#include <vector>

namespace kvdb {
    // 日志级别枚举
    enum class LogLevel: uint8_t {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };
    // 将日志级别转换为字符串
    inline const char* logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR:   return "ERROR";
            default:                return "UNKNOWN";
        }
    }
    // 日志记录类
    class LogRecord {
        public:
            LogRecord(
                LogLevel lvl,
                std::chrono::system_clock::time_point ts,
                std::string msg,
                std::string src,
                int line
            ) : level_(lvl),  message_(std::move(msg)), 
                sourceFile_(std::move(src)), lineNumber_(line) {
                    // Convert the timestamp to a formatted string (YYYY-MM-DD HH:MM:SS.mmm format)
                    auto timeTPoint = std::chrono::system_clock::to_time_t(ts);
                    std::tm localTime = *std::localtime(&timeTPoint);


                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        ts.time_since_epoch() % std::chrono::seconds(1));

                    std::array<char, TIMELENTH> buffer{}; // Buffer for formatted timestamp
                    size_t result = std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S", &localTime);
                    if (result == 0) {
                        timestamp_ = "error formatting time";
                    } else {
                        timestamp_ = std::format("{}.{:03d}", buffer.data(), ms.count());
                    }
                }
            [[nodiscard]] std::string toString() const;
        private:    
            LogLevel level_; // 日志级别
            std::string timestamp_; // 时间戳字符串
            std::string message_;    // 日志消息
            std::string sourceFile_; // 文件名
            int lineNumber_; // 行号
            static constexpr const int TIMELENTH {20}; // 时间戳格式长度
            static constexpr const char* FORMATSTR {"{} [{}] {}:{}: {}"};

        
    };
    // 日志记录器接口
    class LogSink {
        public:
            virtual ~LogSink() = default;
            LogSink(const LogSink&) = delete;
            LogSink& operator=(const LogSink&) = delete;
            LogSink(LogSink&&) = delete;
            LogSink& operator=(LogSink&&) = delete;
            
            virtual void log(const LogRecord& record) = 0; // 记录日志
            virtual void flush() = 0; // 刷新日志
        
        protected:
            LogSink() = default;
    };
    // 文件日志记录器
    class FileSink : public LogSink {
        private:
            std::ofstream fileStream_;
            std::string filePath_;
            std::mutex mutex_; // 确保线程安全

        public:
            explicit FileSink(std::string_view filePath);
            ~FileSink() override;
            FileSink(const FileSink&) = delete;
            FileSink& operator=(const FileSink&) = delete;
            FileSink(FileSink&&) = delete;
            FileSink& operator=(FileSink&&) = delete;
            void log(const LogRecord& record) override; // 实现日志记录
            void flush() override; // 刷新文件流
    };
    // 控制台日志记录器
    class ConsoleSink : public LogSink {
        public:
            void log(const LogRecord& record) override; // 实现日志记录到控制台
            void flush() override; // 控制台通常不需要刷新
        private:
            std::mutex mutex_; // 确保线程安全
    };

    class Logger{
        public:
        static Logger& getInstance() {
            static Logger instance; // 单例模式
            return instance;
        }
        void addSink(std::shared_ptr<LogSink>sink);
        void removeAllSinks();

        void setLevel(LogLevel level);
        [[nodiscard]] bool shouldLog(LogLevel level) const;

        template<typename... Args>
        void log(LogLevel level, 
            const std::source_location& loc,
            const std::format_string<Args...>& fmt,
            Args&&... args) {
                if (!shouldLog(level)) {
                    return; // 如果当前日志级别不允许记录，则直接返回
                }
                std::lock_guard<std::mutex> lock(mutex_);
                LogRecord record(
                    level,
                    std::chrono::system_clock::now(),
                    std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)),
                    loc.file_name(),
                    loc.line()
                );
                for (const auto& sink : sinks_) {
                    sink->log(record); // 将日志记录到所有接收器
                }
             }
        private:
            Logger() = default; // 私有构造函数
            std::vector<std::shared_ptr<LogSink>> sinks_; // 日志接收器列表
            LogLevel level_ = LogLevel::DEBUG; // 默认日志级别
            std::mutex mutex_; // 确保线程安全
            
    };
}  // namespace kvdb
// 辅助宏，用于检查日志级别，避免不必要的函数调用和参数求值
#define KVDB_LOG_SHOULD_LOG(level) (kvdb::Logger::getInstance().shouldLog(level))

// 宏定义，用于简化日志记录操作
// 这些宏会检查日志级别是否允许记录，如果允许，则调用 Logger::log
// 它们使用 std::source_location 来获取源文件和行号信息
#define KVDB_LOG_IMPL(level, fmt, ...) \
    do { \
        if (KVDB_LOG_SHOULD_LOG(level)) { \
            kvdb::Logger::getInstance().log(level, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__); \
        } \
    } while (false)

#define LOG_DEBUG(fmt, ...)   KVDB_LOG_IMPL(kvdb::LogLevel::DEBUG, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_INFO(fmt, ...)    KVDB_LOG_IMPL(kvdb::LogLevel::INFO, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_WARNING(fmt, ...) KVDB_LOG_IMPL(kvdb::LogLevel::WARNING, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_ERROR(fmt, ...)   KVDB_LOG_IMPL(kvdb::LogLevel::ERROR, fmt __VA_OPT__(,) __VA_ARGS__)
#endif // KVDB_LOG_H