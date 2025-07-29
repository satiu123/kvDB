#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
import kvdb.core;

class DeletionTest : public ::testing::Test {
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

    std::string base_path = "test_db_dir";
    std::unique_ptr<kvdb::core::Database> db;
};

TEST_F(DeletionTest, TombstoneWorksAcrossMemtablesAndSSTables) {
    // 1. Put a key and flush it to an SSTable.
    db->setMemtableFlushThreshold(2);
    db->put("key1", "value1");
    db->put("key2", "value2");  // Flush occurs here, sstable_0.db is created.

    ASSERT_TRUE(std::filesystem::exists(base_path + "/data/sstables/sstable_0.db"));
    ASSERT_EQ(*db->get("key1"), "value1");

    // 2. Remove the key. This will write a tombstone to the mutable memtable.
    ASSERT_TRUE(db->remove("key1"));

    // 3. The key should now appear as deleted.
    ASSERT_FALSE(db->get("key1").has_value());
    ASSERT_FALSE(db->exists("key1"));
    ASSERT_EQ(db->size(), 1);  // Only key2 should exist.

    // 4. Flush the tombstone to another SSTable.
    db->put("key3", "value3");
    db->put("key4", "value4");  // Flush occurs here, sstable_1.db is created.
    std::string sstable_1_path =
        std::filesystem::path(base_path) / "data" / "sstables" / "sstable_1.db";
    ASSERT_TRUE(std::filesystem::exists(sstable_1_path));
    ASSERT_FALSE(db->get("key1").has_value());

    // 5. Compact the SSTables.
    db->compact();

    // 6. After compaction, the key and its tombstone should be gone forever.
    ASSERT_FALSE(db->get("key1").has_value());
    ASSERT_EQ(db->size(), 3);  // key2, key3, key4
}
