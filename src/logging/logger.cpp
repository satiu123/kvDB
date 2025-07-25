#include "kvdb/logging/logger.h"

namespace kvdb {

Logger& Logger::getInstance() {
    static Logger instance;  // 使用局部静态变量确保单例
    return instance;
}
Logger::Logger() = default;
Logger::~Logger() = default;
}  // namespace kvdb