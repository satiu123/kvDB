#include <gtest/gtest.h>
import std;

import kvdb.core;

class CompactionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        cleanup();
        db = std::make_unique<kvdb::core::Database>(wal_path, snapshot_path);
    }

    void TearDown() override {
        db.reset();
        cleanup();
    }

    void cleanup() {
        std::filesystem::remove(wal_path);
        std::filesystem::remove(snapshot_path);
        for (const auto& entry : std::filesystem::directory_iterator(".")) {
            if (entry.path().extension() == ".db") {
                std::filesystem::remove(entry.path());
            }
        }
    }

    std::string wal_path = "test_compaction.wal";
    std::string snapshot_path = "test_compaction.snapshot";
    std::unique_ptr<kvdb::core::Database> db;
};

TEST_F(CompactionTest, ManualCompaction) {
    // Set a low threshold to create multiple SSTables
    db->setMemtableFlushThreshold(2);

    // Create first SSTable
    db->put("key1", "value1");
    db->put("key2", "value2_old");  // This will be overwritten

    // Create second SSTable
    db->put("key2", "value2_new");
    db->put("key3", "value3");

    // Check that we have two SSTables
    ASSERT_TRUE(std::filesystem::exists("sstable_0.db"));
    ASSERT_TRUE(std::filesystem::exists("sstable_1.db"));

    // Manually trigger compaction
    db->compact();

    // Check that old SSTables are gone and a new one is created
    ASSERT_FALSE(std::filesystem::exists("sstable_0.db"));
    ASSERT_FALSE(std::filesystem::exists("sstable_1.db"));
    ASSERT_TRUE(std::filesystem::exists("sstable_compacted_2.db"));

    // Verify data after compaction
    ASSERT_EQ(*db->get("key1"), "value1");
    ASSERT_EQ(*db->get("key2"), "value2_new");
    ASSERT_EQ(*db->get("key3"), "value3");
}
