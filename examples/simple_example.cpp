import std;
import std.compat;

import kvdb.core.database;
import kvdb.logging.log.log_impl;
import kvdb.logging.log.logger;
import kvdb.logging.log.log_record;


import kvdb.logging.sinks.file_sink;
import kvdb.logging.sinks.console_sink;
using kvdb::logging::LOG_INFO, kvdb::logging::LOG_DEBUG, kvdb::logging::LOG_ERROR;
int main() {
    kvdb::Database db("kvdb.wal");
    kvdb::logging::Logger::getInstance().addSink(std::make_shared<kvdb::ConsoleSink>());
    // 存储一些键值对
    db.put("name", "kvDB");
    db.put("version", "0.1.0");
    db.put("language", "C++");

    // 检索值
    auto name = db.get("name");
    if (name) {
        // std::cout << "Name: " << *name << std::endl;
        LOG_INFO()("Retrieved name: {}", *name);
    }

    // 检查键是否存在
    if (db.exists("version")) {
        // std::cout << "Version: " << *db.get("version") << std::endl;
        LOG_INFO()("Version exists: {}", *db.get("version"));
    }

    // 删除一个键
    db.remove("language");

    // 显示数据库大小
    // std::cout << "Database size: " << db.size() << std::endl;
    LOG_INFO()("Database size: {}", db.size());

    return 0;
}
