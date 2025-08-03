#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

import kvdb.core;

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

TEST_F(PerformanceTest, HeavyWriteAndRead) {
    const int num_operations = 20000;  // 减少操作数以避免测试时间过长
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

    // 写入性能测试
    auto start_write = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        db->put(keys[i], values[i]);
    }
    auto end_write = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> write_duration = end_write - start_write;
    double write_mbps = (static_cast<double>(num_operations) * (key_size + value_size)) /
                        (1024 * 1024) / write_duration.count();

    std::cout << "---------- 大数据写入性能测试 ----------" << std::endl;
    std::cout << "总操作数: " << num_operations << std::endl;
    std::cout << "键大小: " << key_size << " B, 值大小: " << value_size << " B" << std::endl;
    std::cout << "总耗时: " << write_duration.count() << " 秒" << std::endl;
    std::cout << "写入吞吐量: " << write_mbps << " MB/s" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    // 读取性能测试
    auto start_read = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        auto value = db->get(keys[i]);
    }
    auto end_read = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> read_duration = end_read - start_read;
    double read_mbps = (static_cast<double>(num_operations) * (key_size + value_size)) /
                       (1024 * 1024) / read_duration.count();

    std::cout << "---------- 大数据读取性能测试 ----------" << std::endl;
    std::cout << "总操作数: " << num_operations << std::endl;
    std::cout << "总耗时: " << read_duration.count() << " 秒" << std::endl;
    std::cout << "读取吞吐量: " << read_mbps << " MB/s" << std::endl;
    std::cout << "------------------------------------" << std::endl;
}

TEST_F(PerformanceTest, HeavyCachedRead) {
    const int num_initial_keys = 5000;
    const int num_hot_keys = 100;
    const int num_cached_reads = 100000;
    const std::size_t key_size = 32;
    const std::size_t value_size = 1024;

    // 1. 先写入一批数据
    std::vector<std::string> keys;
    keys.reserve(num_initial_keys);
    for (int i = 0; i < num_initial_keys; ++i) {
        keys.push_back(generate_random_string(key_size));
        db->put(keys.back(), generate_random_string(value_size));
    }

    // 2. 强制合并，生成SSTable
    db->compact();

    // 3. 选取一小部分作为热点数据
    std::vector<std::string> hot_keys;
    hot_keys.reserve(num_hot_keys);
    std::sample(keys.begin(), keys.end(), std::back_inserter(hot_keys), num_hot_keys,
                std::mt19937{std::random_device{}()});

    // 4. 第一次读取，将数据加载到缓存中
    for (const auto& key : hot_keys) {
        db->get(key);
    }

    // 5. 开始缓存性能测试
    auto start_cached_read = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_cached_reads; ++i) {
        db->get(hot_keys[i % num_hot_keys]);
    }
    auto end_cached_read = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> cached_read_duration = end_cached_read - start_cached_read;
    double cached_read_mbps = (static_cast<double>(num_cached_reads) * (key_size + value_size)) /
                              (1024 * 1024) / cached_read_duration.count();

    std::cout << "---------- 大数据缓存读取性能测试 ----------" << std::endl;
    std::cout << "热点键数量: " << num_hot_keys << std::endl;
    std::cout << "总读取操作数: " << num_cached_reads << std::endl;
    std::cout << "总耗时: " << cached_read_duration.count() << " 秒" << std::endl;
    std::cout << "缓存读取吞吐量: " << cached_read_mbps << " MB/s" << std::endl;
    std::cout << "------------------------------------" << std::endl;
}