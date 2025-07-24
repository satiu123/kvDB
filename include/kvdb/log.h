#ifndef KVDB_LOG_H
#define KVDB_LOG_H

#include "kvdb/detail/log_macros.h"   // 包含内部实现宏
#include "kvdb/sinks/console_sink.h"  // 方便用户直接使用
#include "kvdb/sinks/file_sink.h"     // 方便用户直接使用

// --- 公共日志宏接口 ---

#define LOG_DEBUG(fmt, ...) KVDB_LOG_IMPL(kvdb::LogLevel::DEBUG, fmt __VA_OPT__(, ) __VA_ARGS__)
#define LOG_INFO(fmt, ...) KVDB_LOG_IMPL(kvdb::LogLevel::INFO, fmt __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARNING(fmt, ...) KVDB_LOG_IMPL(kvdb::LogLevel::WARNING, fmt __VA_OPT__(, ) __VA_ARGS__)
#define LOG_ERROR(fmt, ...) KVDB_LOG_IMPL(kvdb::LogLevel::ERROR, fmt __VA_OPT__(, ) __VA_ARGS__)

#endif  // KVDB_LOG_H