import std;

import kvdb.core;
import kvdb.core.coro.task;
// 协程任务运行器
void run_task(kvdb::core::coro::Task<void>&& task) {
    while (!task.done()) {
        task.resume();
    }
}

kvdb::core::coro::Task<void> test_async_db(const std::string& db_path) {
    auto db = std::make_unique<kvdb::core::AsyncDatabase>(db_path);
    co_await db->open();


    // 1. 测试 Put 和 Get
    std::cout << "测试 Put 和 Get..." << std::endl;
    co_await db->put("key1", "value1");
    auto value = co_await db->get("key1");
    if (value && *value == "value1") {
        std::cout << "Put/Get 测试成功。" << std::endl;
    } else {
        std::cout << "Put/Get 测试失败。" << std::endl;
    }

    // 2. 测试 Remove
    std::cout << "测试 Remove..." << std::endl;
    co_await db->remove("key1");
    value = co_await db->get("key1");
    if (!value) {
        std::cout << "Remove 测试成功。" << std::endl;
    } else {
        std::cout << "Remove 测试失败。" << std::endl;
    }

    std::cout << "测试完成。" << std::endl;
}

int main() {
    const std::string db_path = "./test_db_async";
    // 准备测试环境
    std::filesystem::create_directory(db_path);

    run_task(test_async_db(db_path));

    // 清理测试环境
    std::filesystem::remove_all(db_path);

    return 0;
}
