module kvdb.logging.log.logger;

import std;

namespace kvdb::logging {
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : worker_thread_(&Logger::worker_loop, this) {}

Logger::~Logger() {
    done_ = true;
    cv_.notify_one();
    // worker_thread_ 的析构函数会自动调用 request_stop() 和 join()
}

void Logger::addSink(std::shared_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::removeAllSinks() {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    sinks_.clear();
}

void Logger::setLevel(LogLevel level) {
    level_ = level;
}

bool Logger::shouldLog(LogLevel level) const {
    return level >= level_;
}

bool Logger::isEnabled() const {
    // 检查sinks是否为空也需要保护
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    return !sinks_.empty();
}

void Logger::worker_loop() {
    std::queue<LogRecord> local_queue;
    while (!done_) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || done_; });
            if (done_ && queue_.empty()) {
                return;
            }
            local_queue.swap(queue_);
        }

        while (!local_queue.empty()) {
            const auto& record = local_queue.front();
            std::lock_guard<std::mutex> lock(sinks_mutex_);
            for (const auto& sink : sinks_) {
                sink->log(record);
            }
            local_queue.pop();
            // 标记处理完成一条记录
            auto done = processed_.fetch_add(1, std::memory_order_relaxed) + 1;
            if (done == queued_.load(std::memory_order_relaxed)) {
                // 当处理数追平入队数，刷新 sinks 并唤醒等待者
                for (const auto& sink : sinks_) {
                    sink->flush();
                }
                cv_.notify_all();
            }
        }
    }
}

void Logger::flush() {
    // 等待直到处理计数追平入队计数，且队列为空
    std::unique_lock<std::mutex> lock(queue_mutex_);
    cv_.wait(lock, [this] {
        return queue_.empty() && processed_.load(std::memory_order_relaxed) ==
                                     queued_.load(std::memory_order_relaxed);
    });
}

}  // namespace kvdb::logging