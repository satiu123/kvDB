#ifndef KVDB_DETAIL_LOG_MACROS_H
#define KVDB_DETAIL_LOG_MACROS_H

#include "kvdb/logger.h" // 内部宏也需要 logger.h
#include <source_location>

// 将实现细节放在 detail 命名空间中

// 辅助宏，用于检查日志级别
#define KVDB_LOG_SHOULD_LOG(level) (kvdb::Logger::getInstance().shouldLog(level))

// 核心宏实现
#define KVDB_LOG_IMPL(level, fmt, ...) \
    do { \
        if (KVDB_LOG_SHOULD_LOG(level)) { \
            kvdb::Logger::getInstance().log(level, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__); \
        } \
    } while (false)

#endif // KVDB_DETAIL_LOG_MACROS_H