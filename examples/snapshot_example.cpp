#include <chrono>
#include <iostream>
#include <thread>

#include "kvdb/core/database.h"
#include "kvdb/storage/snapshot_manager.h"

int main() {
    // 创建数据库实例
    kvdb::Database db("example.wal", "example.snapshot");

    std::cout << "=== KvDB 快照示例 ===" << std::endl;

    // 配置自动快照
    kvdb::SnapshotConfig config;
    config.auto_snapshot_enabled = true;
    config.operation_count_threshold = 5;            // 5个操作后自动创建快照
    config.time_interval = std::chrono::minutes(1);  // 1分钟间隔
    db.setSnapshotConfig(config);

    std::cout << "快照配置已设置：每5个操作或1分钟自动创建快照" << std::endl;

    // 检查是否有已存在的快照
    if (db.hasSnapshot()) {
        std::cout << "发现已存在的快照文件，数据已从快照恢复" << std::endl;
    } else {
        std::cout << "没有快照文件，从空数据库开始" << std::endl;
    }

    std::cout << "当前数据库大小: " << db.size() << std::endl;

    // 插入一些数据
    std::cout << "\n--- 插入测试数据 ---" << std::endl;
    for (int i = 1; i <= 10; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);

        if (db.put(key, value)) {
            std::cout << "插入: " << key << " = " << value << std::endl;
        }

        // 每次操作后短暂停顿，观察自动快照
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n插入完成，数据库大小: " << db.size() << std::endl;

    // 手动创建快照
    std::cout << "\n--- 手动创建快照 ---" << std::endl;
    if (db.createSnapshot()) {
        std::cout << "手动快照创建成功！" << std::endl;
    } else {
        std::cout << "手动快照创建失败！" << std::endl;
    }

    // 继续插入更多数据
    std::cout << "\n--- 插入更多数据 ---" << std::endl;
    for (int i = 11; i <= 15; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);

        if (db.put(key, value)) {
            std::cout << "插入: " << key << " = " << value << std::endl;
        }
    }

    // 删除一些数据
    std::cout << "\n--- 删除部分数据 ---" << std::endl;
    for (int i = 1; i <= 3; ++i) {
        std::string key = "key" + std::to_string(i);
        if (db.remove(key)) {
            std::cout << "删除: " << key << std::endl;
        }
    }

    std::cout << "\n最终数据库大小: " << db.size() << std::endl;

    // 显示一些剩余的数据
    std::cout << "\n--- 剩余数据示例 ---" << std::endl;
    for (int i = 4; i <= 8; ++i) {
        std::string key = "key" + std::to_string(i);
        auto value = db.get(key);
        if (value) {
            std::cout << key << " = " << *value << std::endl;
        }
    }

    std::cout << "\n=== 程序结束 ===" << std::endl;
    std::cout << "提示：重新运行程序查看从快照恢复的效果" << std::endl;

    return 0;
}
