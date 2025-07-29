#include <gtest/gtest.h>

import std;
import kvdb;

using namespace kvdb::logging;
using namespace kvdb::storage;
namespace kvdb {
namespace {

// WAL测试类
class WalTest : public ::testing::Test {
  protected:
    const std::string test_file = "test_wal.log";

    // 在每个测试前执行
    void SetUp() override {
        // 确保测试开始时文件不存在
        std::filesystem::remove(test_file);

        // 初始化日志系统
        auto& logger = Logger::getInstance();
        logger.addSink(std::make_shared<ConsoleSink>());
        logger.setLevel(LogLevel::DEBUG);
    }

    // 在每个测试后执行
    void TearDown() override {
        // 删除测试文件
        std::filesystem::remove(test_file);

        // 清理日志系统
        Logger::getInstance().removeAllSinks();
    }
};

// 测试WAL基本功能：创建、打开和关闭
TEST_F(WalTest, CreateOpenClose) {
    Wal wal(test_file);

    // 检查文件是否被创建
    ASSERT_TRUE(std::filesystem::exists(test_file));

    // 关闭WAL
    wal.close();
}

// 测试WAL的PUT操作
TEST_F(WalTest, AppendPut) {
    Wal wal(test_file);

    // 添加一条PUT记录
    ASSERT_TRUE(wal.appendPut("key1", "value1"));

    // 添加另一条PUT记录
    ASSERT_TRUE(wal.appendPut("key2", "value2"));

    // 使用lambda表达式重放记录并检查
    std::vector<std::pair<std::string, std::string>> records;

    ASSERT_TRUE(wal.replay([&records](const WalRecord& record) {
        if (record.getOpType() == WalOpType::PUT) {
            records.emplace_back(std::string(record.getKey()), std::string(record.getValue()));
        }
        return true;
    }));

    // 验证记录数量
    ASSERT_EQ(records.size(), 2);

    // 验证记录内容
    ASSERT_EQ(records[0].first, "key1");
    ASSERT_EQ(records[0].second, "value1");
    ASSERT_EQ(records[1].first, "key2");
    ASSERT_EQ(records[1].second, "value2");
}

// 测试WAL的REMOVE操作
TEST_F(WalTest, AppendRemove) {
    Wal wal(test_file);

    // 添加PUT和REMOVE记录
    ASSERT_TRUE(wal.appendPut("key1", "value1"));
    ASSERT_TRUE(wal.appendRemove("key1"));

    // 使用重放模拟数据库操作
    std::unordered_map<std::string, std::string> data;

    ASSERT_TRUE(wal.replay([&data](const WalRecord& record) {
        switch (record.getOpType()) {
            case WalOpType::PUT:
                data[std::string(record.getKey())] = std::string(record.getValue());
                break;
            case WalOpType::REMOVE:
                data.erase(std::string(record.getKey()));
                break;
            default:
                break;
        }
        return true;
    }));

    // 验证key1已被删除
    ASSERT_EQ(data.count("key1"), 0);
}

// 测试WAL的CLEAR操作
TEST_F(WalTest, AppendClear) {
    Wal wal(test_file);

    // 添加多条记录后清空
    ASSERT_TRUE(wal.appendPut("key1", "value1"));
    ASSERT_TRUE(wal.appendPut("key2", "value2"));
    ASSERT_TRUE(wal.appendClear());

    // 使用重放模拟数据库操作
    std::unordered_map<std::string, std::string> data;

    ASSERT_TRUE(wal.replay([&data](const WalRecord& record) {
        switch (record.getOpType()) {
            case WalOpType::PUT:
                data[std::string(record.getKey())] = std::string(record.getValue());
                break;
            case WalOpType::REMOVE:
                data.erase(std::string(record.getKey()));
                break;
            case WalOpType::CLEAR:
                data.clear();
                break;
        }
        return true;
    }));

    // 验证数据已清空
    ASSERT_TRUE(data.empty());
}

// 测试WAL的截断功能
TEST_F(WalTest, TruncateWal) {
    {
        // 创建WAL并添加记录
        Wal wal(test_file);
        ASSERT_TRUE(wal.appendPut("key1", "value1"));
        ASSERT_FALSE(wal.isEmpty());
    }

    {
        // 重新打开WAL并截断
        Wal wal(test_file);
        ASSERT_TRUE(wal.truncate());
        ASSERT_TRUE(wal.isEmpty());
    }

    {
        // 再次打开，验证文件为空
        Wal wal(test_file);
        ASSERT_TRUE(wal.isEmpty());
    }
}

// 测试大量数据
TEST_F(WalTest, LargeData) {
    constexpr int RECORD_COUNT = 1000;

    {
        // 创建WAL并添加大量记录
        Wal wal(test_file);

        for (int i = 0; i < RECORD_COUNT; ++i) {
            std::string key = "key" + std::to_string(i);
            std::string value = "value" + std::to_string(i);
            ASSERT_TRUE(wal.appendPut(key, value));
        }
    }

    {
        // 重新打开并重放记录
        Wal wal(test_file);

        int count = 0;
        ASSERT_TRUE(wal.replay([&count](const WalRecord& /* record */) {
            ++count;
            return true;
        }));

        // 验证记录数量
        ASSERT_EQ(count, RECORD_COUNT);
    }
}

// 测试WAL的持久化恢复
TEST_F(WalTest, Persistence) {
    std::vector<std::pair<std::string, std::string>> expected_data;

    {
        // 首先创建WAL并添加记录
        Wal wal(test_file);

        // 添加一些记录
        const std::vector<std::pair<std::string, std::string>> records = {
            {"key1", "value1"},
            {"key2", "value2"},
            {"key3", "value3"}
        };

        for (const auto& [key, value] : records) {
            ASSERT_TRUE(wal.appendPut(key, value));
            expected_data.push_back({key, value});
        }

        // 删除一条记录
        ASSERT_TRUE(wal.appendRemove("key2"));
        expected_data.erase(std::remove_if(expected_data.begin(), expected_data.end(),
                                           [](const auto& pair) { return pair.first == "key2"; }),
                            expected_data.end());

        // WAL会在析构时关闭
    }

    {
        // 重新打开WAL并重放记录
        Wal wal(test_file);

        std::unordered_map<std::string, std::string> recovered_data;

        ASSERT_TRUE(wal.replay([&recovered_data](const WalRecord& record) {
            switch (record.getOpType()) {
                case WalOpType::PUT:
                    recovered_data[std::string(record.getKey())] = std::string(record.getValue());
                    break;
                case WalOpType::REMOVE:
                    recovered_data.erase(std::string(record.getKey()));
                    break;
                case WalOpType::CLEAR:
                    recovered_data.clear();
                    break;
            }
            return true;
        }));

        // 验证恢复的数据
        ASSERT_EQ(recovered_data.size(), expected_data.size());

        for (const auto& [key, value] : expected_data) {
            ASSERT_TRUE(recovered_data.contains(key));
            ASSERT_EQ(recovered_data[key], value);
        }

        // 确认key2已被删除
        ASSERT_FALSE(recovered_data.contains("key2"));
    }
}

}  // namespace
}  // namespace kvdb
