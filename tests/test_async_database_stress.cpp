#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

import kvdb.core; // exports async_database

using kvdb::core::AsyncDatabase;

TEST(AsyncDatabase, StressFlushAndReadBack) {
    const std::filesystem::path base{"./test_db_stress"};
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);

    AsyncDatabase db(base.string());
    db.set_flush_threshold(5);  // 小阈值以触发多次刷盘

    // init
    db.run(db.init());

    // 写入多条记录以触发多次刷盘
    const int N = 20;
    for (int i = 0; i < N; ++i) {
        std::string k = std::string("k") + std::to_string(i);
        std::string v = std::string("v") + std::to_string(i);
        ASSERT_TRUE(db.run(db.async_put(k, v)));
    }

    // 读取部分键验证
    for (int i = 0; i < N; i += 3) {
        std::string k = std::string("k") + std::to_string(i);
        auto val = db.run(db.async_get(k));
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(*val, std::string("v") + std::to_string(i));
    }

    // 检查 sstables 目录是否有文件生成（支持按 Level 子目录）
    auto sst_dir = base / "sstables";
    ASSERT_TRUE(std::filesystem::exists(sst_dir));
    size_t file_count = 0;
    if (std::filesystem::exists(sst_dir / "L0")) {
        for (const auto& e : std::filesystem::directory_iterator(sst_dir / "L0")) {
            if (e.is_regular_file())
                ++file_count;
        }
    }
    if (file_count == 0) {
        for (const auto& e : std::filesystem::recursive_directory_iterator(sst_dir)) {
            if (e.is_regular_file())
                ++file_count;
        }
    }
    EXPECT_GT(file_count, 0U);
}
