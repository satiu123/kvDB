import std;
import kvdb.logging;
using kvdb::logging::LOG_INFO, kvdb::logging::LOG_DEBUG;
int main() {
    // 获取 Logger 实例
    auto& logger = kvdb::logging::Logger::getInstance();

    // 设置日志级别
    logger.setLevel(kvdb::logging::LogLevel::DEBUG);

    // 添加一个简单的日志接收器（sink）
    logger.addSink(std::make_shared<kvdb::logging::ConsoleSink>());

    // 记录一些日志
    LOG_INFO()("这是一条信息日志。");
    LOG_DEBUG()("这是一条带数字的调试日志: {}", 42);
    return 0;
}
