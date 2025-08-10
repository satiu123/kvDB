
import std;
import kvdb.core;
import kvdb.core.coro.task;

// 辅助函数：生成指定长度的随机字符串
std::string generate_random_string(std::size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string random_string;
    random_string.reserve(length);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, sizeof(alphanum) - 2);

    for (std::size_t i = 0; i < length; ++i) {
        random_string += alphanum[distrib(gen)];
    }
    return random_string;
}

// 异步压力测试任务
auto performance_test_main(kvdb::core::AsyncDatabase& db) -> kvdb::core::coro::Task<void> {
    const int num_operations = 20000;
    const std::size_t key_size = 32;
    const std::size_t value_size = 1024;  // 1KB

    std::vector<std::string> keys;
    std::vector<std::string> values;
    keys.reserve(num_operations);
    values.reserve(num_operations);

    for (int i = 0; i < num_operations; ++i) {
        keys.push_back(generate_random_string(key_size));
        values.push_back(generate_random_string(value_size));
    }

    co_await db.init();

    // 异步写入性能测试
    auto start_write = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        co_await db.async_put(keys[i], values[i]);
    }
    auto end_write = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> write_duration = end_write - start_write;
    double write_mbps = (static_cast<double>(num_operations) * (key_size + value_size)) /
                        (1024 * 1024) / write_duration.count();

    std::cout << "---------- 异步写入性能测试 ----------" << std::endl;
    std::cout << "总操作数: " << num_operations << std::endl;
    std::cout << "键大小: " << key_size << " B, 值大小: " << value_size << " B" << std::endl;
    std::cout << "总耗时: " << write_duration.count() << " 秒" << std::endl;
    std::cout << "写入吞吐量: " << write_mbps << " MB/s" << std::endl;
    std::cout << "------------------------------------" << std::endl;


    // 异步读取性能测试
    auto start_read = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        auto value = co_await db.async_get(keys[i]);
    }
    auto end_read = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> read_duration = end_read - start_read;
    double read_mbps = (static_cast<double>(num_operations) * (key_size + value_size)) /
                       (1024 * 1024) / read_duration.count();

    std::cout << "---------- 异步读取性能测试 ----------" << std::endl;
    std::cout << "总操作数: " << num_operations << std::endl;
    std::cout << "总耗时: " << read_duration.count() << " 秒" << std::endl;
    std::cout << "读取吞吐量: " << read_mbps << " MB/s" << std::endl;
    std::cout << "------------------------------------" << std::endl;
}

int main() {
    const std::string db_path = "./performance_test_db_async";

    // 在开始前清理数据库目录
    std::filesystem::remove_all(db_path);

    kvdb::core::AsyncDatabase db(db_path);
    // std::filesystem::create_directories(db_path);
    // 运行异步压力测试
    db.run(performance_test_main(db));

    return 0;
}
