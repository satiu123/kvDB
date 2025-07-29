#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
import kvdb.core;

class MemtableTest : public ::testing::Test {
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
        std::filesystem::remove("sstable_0.db");
    }

    std::string wal_path = "test_memtable.wal";
    std::string snapshot_path = "test_memtable.snapshot";
    std::unique_ptr<kvdb::core::Database> db;
};

TEST_F(MemtableTest, FlushOnThreshold) {
    db->setMemtableFlushThreshold(3);

    db->put("key1", "value1");
    db->put("key2", "value2");
    ASSERT_EQ(db->size(), 2);

    // This should trigger the flush to SSTable
    db->put("key3", "value3");

    // After flush, the data is moved to an SSTable.
    // The total size of the database should still be 3.
    ASSERT_EQ(db->size(), 3);

    // Verify that the SSTable file was created
    ASSERT_TRUE(std::filesystem::exists("sstable_0.db"));

    // Check that we can still get the old keys from the new SSTable
    ASSERT_EQ(*db->get("key1"), "value1");
    ASSERT_EQ(*db->get("key2"), "value2");
    ASSERT_EQ(*db->get("key3"), "value3");
}
