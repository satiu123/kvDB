#include <iostream>

#include "kvdb.h"  // 使用新的简化包含文件

int main() {
    std::cout << "=== 测试新的文件结构 ===" << std::endl;

    // 创建数据库实例
    kvdb::Database db("structure_test.wal", "structure_test.snapshot");

    // 基本操作
    db.put("module", "core");
    db.put("storage", "snapshot+wal");
    db.put("logging", "flexible");

    std::cout << "当前数据库大小: " << db.size() << std::endl;

    // 读取数据
    auto modules = {"module", "storage", "logging"};
    for (const auto& key : modules) {
        auto value = db.get(key);
        if (value) {
            std::cout << key << " = " << *value << std::endl;
        }
    }

    std::cout << "新文件结构测试完成！" << std::endl;
    return 0;
}
