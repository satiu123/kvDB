import std;

import kvdb.core;
import kvdb.core.coro.task;

// 定义一个异步任务，用于测试数据库功能
auto test_async_db(kvdb::core::AsyncDatabase& db) -> kvdb::core::coro::Task<void> {
    // 打开数据库（这会异步加载 manifest 等）
    co_await db.init();
    db.set_flush_threshold(2);

    // 1. 测试 Put 和 Get
    std::cout << "测试 Put 和 Get..." << std::endl;
    co_await db.async_put("key1", "value1");
    auto value = co_await db.async_get("key1");
    if (value && *value == "value1") {
        std::cout << "Put/Get 测试成功。" << std::endl;
    } else {
        std::cout << "Put/Get 测试失败。" << std::endl;
    }
    co_await db.async_put("key2", "value2");
    // 2. 测试 Remove
    std::cout << "测试 Remove..." << std::endl;
    auto remove_ok = co_await db.async_remove("key1");
    if (remove_ok) {
        std::cout << "Remove 测试成功。" << std::endl;
    } else {
        std::cout << "Remove 测试失败。" << std::endl;
    }
    co_await db.async_put("key3", "value3");
    co_await db.async_put("key4", "value4");
    co_await db.printWALRecords();
    co_await db.printSSTables();
    db.printManifest();
    std::cout << "测试完成。" << std::endl;
}

int main() {
    // 数据库文件存放的路径
    const std::string db_path = "./data";
    std::filesystem::remove_all(db_path);  // 清理旧数据
    // 创建 AsyncDatabase 实例
    // IOUring 和事件循环现在被封装在内部
    kvdb::core::AsyncDatabase db(db_path);

    // 使用 db.run 来执行整个异步任务
    // 用户不再需要关心协程调度
    db.run(test_async_db(db));

    return 0;
}
