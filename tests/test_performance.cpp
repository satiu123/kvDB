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

TEST_F(PerformanceTest, CachedRead) {
    const int num_initial_keys = 10000;
    const int num_hot_keys = 100;
    const int num_cached_reads = 100000;

    // 1. 先写入一批数据
    std::vector<std::string> keys;
    keys.reserve(num_initial_keys);
    for (int i = 0; i < num_initial_keys; ++i) {
        keys.push_back("key" + std::to_string(i));
        db->put(keys.back(), "value" + std::to_string(i));
    }

    // 2. 强制合并，生成SSTable
    db->compact();

    // 3. 选取一小部分作为热点数据
    std::vector<std::string> hot_keys;
    hot_keys.reserve(num_hot_keys);
    for (int i = 0; i < num_hot_keys; ++i) {
        hot_keys.push_back(keys[i * 10]);  // 间隔选取，避免集中在同一个块
    }

    // 4. 第一次读取，将数据加载到缓存中
    for (const auto& key : hot_keys) {
        db->get(key);
    }

    // 5. 开始缓存性能测试
    auto start_cached_read = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_cached_reads; ++i) {
        // 随机读取热点数据
        db->get(hot_keys[i % num_hot_keys]);
    }
    auto end_cached_read = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> cached_read_duration = end_cached_read - start_cached_read;

    std::cout << "---------- 缓存读取性能测试 ----------" << std::endl;
    std::cout << "热点键数量: " << num_hot_keys << std::endl;
    std::cout << "总读取操作数: " << num_cached_reads << std::endl;
    std::cout << "总耗时: " << cached_read_duration.count() << " 秒" << std::endl;
    std::cout << "每秒操作数 (Cached QPS): "
              << static_cast<long long>(num_cached_reads / cached_read_duration.count())
              << std::endl;
    std::cout << "------------------------------------" << std::endl;
}
