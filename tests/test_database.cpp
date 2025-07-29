#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
import kvdb.core;

class DatabaseTest : public ::testing::Test {
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

TEST_F(DatabaseTest, BasicOperations) {
    EXPECT_TRUE(db->put("key1", "value1"));
    auto value = db->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "value1");

    EXPECT_FALSE(db->get("nonexistent").has_value());

    EXPECT_TRUE(db->exists("key1"));
    EXPECT_FALSE(db->exists("nonexistent"));

    EXPECT_TRUE(db->remove("key1"));
    EXPECT_FALSE(db->exists("key1"));

    EXPECT_EQ(db->size(), 0);
    db->put("key1", "value1");
    db->put("key2", "value2");
    EXPECT_EQ(db->size(), 2);
    db->clear();
    EXPECT_EQ(db->size(), 0);
}

TEST_F(DatabaseTest, Persistence) {
    db->put("key1", "value1");
    db->put("key2", "value2");
    db->remove("key1");

    // Re-open the database
    db.reset();
    db = std::make_unique<kvdb::core::Database>(base_path);

    EXPECT_FALSE(db->exists("key1"));
    EXPECT_TRUE(db->exists("key2"));
    EXPECT_EQ(*db->get("key2"), "value2");
    EXPECT_EQ(db->size(), 1);
}
