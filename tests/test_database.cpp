#include <gtest/gtest.h>
#include "kvdb/database.h"

TEST(DatabaseTest, BasicOperations) {
    kvdb::Database db;
    
    // 测试 put 和 get
    EXPECT_TRUE(db.put("key1", "value1"));
    auto value = db.get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "value1");
    
    // 测试不存在的键
    EXPECT_FALSE(db.get("nonexistent").has_value());
    
    // 测试 exists
    EXPECT_TRUE(db.exists("key1"));
    EXPECT_FALSE(db.exists("nonexistent"));
    
    // 测试 remove
    EXPECT_TRUE(db.remove("key1"));
    EXPECT_FALSE(db.exists("key1"));
    
    // 测试 size 和 clear
    EXPECT_EQ(db.size(), 0);
    db.put("key1", "value1");
    db.put("key2", "value2");
    EXPECT_EQ(db.size(), 2);
    db.clear();
    EXPECT_EQ(db.size(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
