#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

import kvdb.core;

class PerformanceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        cleanup();
        db = std::make_unique<kvdb::core::Database>(base_path);
    }

    void TearDown() override {
        db.reset();
        cleanup();
    }

    void cleanup() {
        std::filesystem::remove_all(base_path);
    }

    std::string base_path = "perf_test_db_dir";
    std::unique_ptr<kvdb::core::Database> db;
};

TEST_F(PerformanceTest, WriteAndRead) {
    const int num_operations = 100000;
    std::vector<std::string> keys;
    keys.reserve(num_operations);
    for (int i = 0; i < num_operations; ++i) {
        keys.push_back("key" + std::to_string(i));
    }

    // 写入性能测试
    auto start_write = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        db->put(keys[i], "value" + std::to_string(i));
    }
    auto end_write = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> write_duration = end_write - start_write;
    std::cout << "---------- 写入性能测试 ----------" << std::endl;
    std::cout << "总操作数: " << num_operations << std::endl;
    std::cout << "总耗时: " << write_duration.count() << " 秒" << std::endl;
    std::cout << "每秒操作数 (TPS): "
              << static_cast<long long>(num_operations / write_duration.count()) << std::endl;
    std::cout << "------------------------------------" << std::endl;

    // 读取性能测试
    auto start_read = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        auto value = db->get(keys[i]);
    }
    auto end_read = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> read_duration = end_read - start_read;
    std::cout << "---------- 读取性能测试 ----------" << std::endl;
    std::cout << "总操作数: " << num_operations << std::endl;
    std::cout << "总耗时: " << read_duration.count() << " 秒" << std::endl;
    std::cout << "每秒操作数 (QPS): "
              << static_cast<long long>(num_operations / read_duration.count()) << std::endl;
    std::cout << "------------------------------------" << std::endl;
}
