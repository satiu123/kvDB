import std;
import kvdb.core;
import kvdb.core.coro.task;
import kvdb.logging;

// 全局随机数生成器
static std::mt19937 g_rng;
const int num_operations = 20000;
const std::size_t key_size = 32;
const std::size_t value_size = 1024;  // 1KB
// 辅助函数：生成指定长度的随机字符串
std::string generate_random_string(std::size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string random_string;
    random_string.reserve(length);

    std::uniform_int_distribution<> distrib(0, sizeof(alphanum) - 2);

    for (std::size_t i = 0; i < length; ++i) {
        random_string += alphanum[distrib(g_rng)];
    }
    return random_string;
}
auto test_put(kvdb::core::AsyncDatabase* db, std::span<const std::string> keys,
              std::span<const std::string> values) -> kvdb::core::coro::Task<bool> {
    auto start_write = std::chrono::high_resolution_clock::now();
    // 批量 WAL 追加 + 内存表更新，减少系统调用
    constexpr std::size_t window = 256;  // 可根据磁盘/CPU调整：64~1024
    for (std::size_t i = 0; i < keys.size(); i += window) {
        std::size_t n = std::min<std::size_t>(window, keys.size() - i);
        auto kspan = keys.subspan(i, n);
        auto vspan = values.subspan(i, n);
        auto ok = co_await db->async_put_batch(kspan, vspan);
        if (!ok) {
            std::cerr << "批量写失败在批次起始: " << i << std::endl;
            break;
        }
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
    co_return true;
}

// 异步读取性能测试
auto test_get(kvdb::core::AsyncDatabase& db, std::span<const std::string> keys)
    -> kvdb::core::coro::Task<void> {
    auto start_read = std::chrono::high_resolution_clock::now();

    // 将读取操作分批处理，以控制并发量
    constexpr std::size_t batch_size = 512;
    for (std::size_t i = 0; i < num_operations; i += batch_size) {
        std::size_t current_batch_size = std::min<std::size_t>(batch_size, num_operations - i);

        std::vector<kvdb::core::coro::Task<std::optional<std::string>>> tasks;
        tasks.reserve(current_batch_size);

        // 启动当前批次的所有异步读取操作
        for (std::size_t j = 0; j < current_batch_size; ++j) {
            tasks.emplace_back(db.async_get(keys[i + j]));
        }

        // 等待当前批次的所有操作完成
        for (auto& task : tasks) {
            auto value = co_await std::move(task);
        }
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
    co_return;
}
// 异步压力测试任务
auto performance_test_main(kvdb::core::AsyncDatabase* db) -> kvdb::core::coro::Task<void> {
    std::vector<std::string> keys;
    std::vector<std::string> values;
    keys.reserve(num_operations);
    values.reserve(num_operations);

    for (int i = 0; i < num_operations; ++i) {
        keys.push_back(generate_random_string(key_size));
        values.push_back(generate_random_string(value_size));
    }

    // // 异步写入性能测试
    co_await test_put(db, std::span<const std::string>{keys}, std::span<const std::string>{values});

    // 异步读取性能测试
    co_await test_get(*db, std::span<const std::string>{keys});

    co_return;
}

int main() {
    // 设置固定种子以确保可重复的测试结果
    // 在性能测试中使用固定种子是有意的，可以确保每次运行的数据相同
    // 如果需要随机数据，可以使用 std::random_device{}() 代替固定值
    const std::uint32_t fixed_seed = 12345;
    g_rng.seed(fixed_seed);

    std::cout << "使用固定种子: " << fixed_seed << " (确保可重复的测试结果)" << std::endl;

    const std::string db_path = "./performance_test_db_async";

    // 在开始前清理数据库目录
    std::filesystem::remove_all(db_path);

    kvdb::core::AsyncDatabase db(db_path);
    // 确保日志与数据库目录存在
    std::filesystem::create_directories(db_path);
    // auto& logger = kvdb::logging::Logger::getInstance();
    // if (auto sinkExp = kvdb::logging::FileSink::create(db_path); sinkExp) {
    //     logger.addSink(*sinkExp);
    // } else {
    //     std::cerr << "Failed to create FileSink: " << sinkExp.error() << std::endl;
    // }
    // 运行异步压力测试
    // db.set_flush_threshold(50000);
    db.run(performance_test_main(&db));

    return 0;
}