#include <gtest/gtest.h>

#include <filesystem>

import kvdb.core; // aggregator exports async_database and dependencies

using kvdb::core::AsyncDatabase;

TEST(AsyncDatabase, BasicPutGetRemove) {
    const std::filesystem::path base{"./test_db_async"};
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);

    AsyncDatabase db(base.string());

    // init
    db.run(db.init());

    // basic put/get
    ASSERT_TRUE(db.run(db.async_put("k1", "v1")));
    ASSERT_TRUE(db.run(db.async_put("k2", "v2")));

    auto v1 = db.run(db.async_get("k1"));
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, "v1");

    // remove and verify
    ASSERT_TRUE(db.run(db.async_remove("k1")));
    auto v1_removed = db.run(db.async_get("k1"));
    EXPECT_FALSE(v1_removed.has_value());
}
