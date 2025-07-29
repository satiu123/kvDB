#include <gtest/gtest.h>

import std;

import kvdb;
using namespace kvdb::storage;
using namespace kvdb::core;
class SnapshotTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // 清理测试文件
        cleanup();

        // 创建数据库实例
        db = std::make_unique<kvdb::core::Database>(wal_path, snapshot_path);
    }

    void TearDown() override {
        db.reset();
        cleanup();
    }

    void cleanup() {
        std::filesystem::remove(wal_path);
        std::filesystem::remove(snapshot_path);
    }

    std::string wal_path = "test_snapshot.wal";
    std::string snapshot_path = "test_snapshot.snapshot";
    std::unique_ptr<Database> db;
};

// 测试基本快照创建和恢复
TEST_F(SnapshotTest, DISABLED_BasicSnapshotAndRestore) {
    // 插入一些数据
    ASSERT_TRUE(db->put("key1", "value1"));
    ASSERT_TRUE(db->put("key2", "value2"));
    ASSERT_TRUE(db->put("key3", "value3"));

    ASSERT_EQ(db->size(), 3);

    // 创建快照
    ASSERT_TRUE(db->createSnapshot());
    ASSERT_TRUE(db->hasSnapshot());

    // 插入更多数据
    ASSERT_TRUE(db->put("key4", "value4"));
    ASSERT_TRUE(db->put("key5", "value5"));

    ASSERT_EQ(db->size(), 5);

    // 重新创建数据库实例（模拟重启）
    db.reset();
    db = std::make_unique<Database>(wal_path, snapshot_path);

    // 验证数据恢复
    ASSERT_EQ(db->size(), 5);  // 应该恢复所有数据

    auto value1 = db->get("key1");
    ASSERT_TRUE(value1.has_value());
    ASSERT_EQ(*value1, "value1");

    auto value4 = db->get("key4");
    ASSERT_TRUE(value4.has_value());
    ASSERT_EQ(*value4, "value4");
}

// 测试快照配置
TEST_F(SnapshotTest, SnapshotConfiguration) {
    SnapshotConfig config;
    config.auto_snapshot_enabled = true;
    config.operation_count_threshold = 3;
    config.time_interval = std::chrono::minutes(1);

    db->setSnapshotConfig(std::move(config));

    const auto& retrieved_config = db->getSnapshotConfig();
    ASSERT_TRUE(retrieved_config.auto_snapshot_enabled);
    ASSERT_EQ(retrieved_config.operation_count_threshold, 3);
    ASSERT_EQ(retrieved_config.time_interval, std::chrono::minutes(1));
}

// 测试自动快照（基于操作数量）
TEST_F(SnapshotTest, AutoSnapshotByOperationCount) {
    // 配置自动快照
    SnapshotConfig config;
    config.auto_snapshot_enabled = true;
    config.operation_count_threshold = 3;
    db->setSnapshotConfig(std::move(config));

    // 执行3个操作，应该触发自动快照
    ASSERT_TRUE(db->put("key1", "value1"));
    ASSERT_TRUE(db->put("key2", "value2"));
    ASSERT_TRUE(db->put("key3", "value3"));

    // 给自动快照一些时间执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 应该已经创建了快照
    ASSERT_TRUE(db->hasSnapshot());
}

// 测试快照后的数据一致性
TEST_F(SnapshotTest, DISABLED_DataConsistencyAfterSnapshot) {
    // 插入初始数据
    for (int i = 1; i <= 10; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        ASSERT_TRUE(db->put(key, value));
    }

    // 创建快照
    ASSERT_TRUE(db->createSnapshot());

    // 修改数据
    ASSERT_TRUE(db->put("key1", "modified_value1"));
    ASSERT_TRUE(db->remove("key2"));
    ASSERT_TRUE(db->put("key11", "value11"));

    // 记录当前状态
    auto current_size = db->size();
    auto key1_value = db->get("key1");
    auto key2_value = db->get("key2");
    auto key11_value = db->get("key11");

    // 重启数据库
    db.reset();
    db = std::make_unique<Database>(wal_path, snapshot_path);

    // 验证数据一致性
    ASSERT_EQ(db->size(), current_size);

    auto restored_key1 = db->get("key1");
    ASSERT_TRUE(restored_key1.has_value());
    ASSERT_EQ(*restored_key1, "modified_value1");

    auto restored_key2 = db->get("key2");
    ASSERT_FALSE(restored_key2.has_value());  // 应该已被删除

    auto restored_key11 = db->get("key11");
    ASSERT_TRUE(restored_key11.has_value());
    ASSERT_EQ(*restored_key11, "value11");
}

// 测试空数据库的快照
TEST_F(SnapshotTest, EmptyDatabaseSnapshot) {
    // 创建空数据库的快照
    ASSERT_TRUE(db->createSnapshot());
    ASSERT_TRUE(db->hasSnapshot());

    // 重启数据库
    db.reset();
    db = std::make_unique<Database>(wal_path, snapshot_path);

    // 验证恢复的数据库仍然是空的
    ASSERT_EQ(db->size(), 0);
}

// 测试快照文件不存在时的恢复
TEST_F(SnapshotTest, RecoveryWithoutSnapshot) {
    // 只插入数据到WAL，不创建快照
    ASSERT_TRUE(db->put("key1", "value1"));
    ASSERT_TRUE(db->put("key2", "value2"));

    ASSERT_FALSE(db->hasSnapshot());

    // 重启数据库
    db.reset();
    db = std::make_unique<Database>(wal_path, snapshot_path);

    // 应该从WAL恢复数据
    ASSERT_EQ(db->size(), 2);

    auto value1 = db->get("key1");
    ASSERT_TRUE(value1.has_value());
    ASSERT_EQ(*value1, "value1");
}
