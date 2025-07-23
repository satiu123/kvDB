#include <iostream>
#include "kvdb/database.h"

int main() {
    kvdb::Database db("kvdb.wal");
    
    // 存储一些键值对
    // db.put("name", "kvDB");
    // db.put("version", "0.1.0");
    // db.put("language", "C++");
    
    // 检索值
    auto name = db.get("name");
    if (name) {
        std::cout << "Name: " << *name << std::endl;
    }
    
    // 检查键是否存在
    if (db.exists("version")) {
        std::cout << "Version: " << *db.get("version") << std::endl;
    }
    
    // 删除一个键
    db.remove("language");
    
    // 显示数据库大小
    std::cout << "Database size: " << db.size() << std::endl;
    
    return 0;
}
