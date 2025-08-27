#include <gtest/gtest.h>

#include <optional>

import kvdb.storage.bloom_filter;

using kvdb::storage::BloomFilter;

TEST(BloomFilter, Basic) {
    BloomFilter bf(1000, 0.01);
    bf.add("apple");
    bf.add("banana");
    EXPECT_TRUE(bf.contains("apple"));
    EXPECT_TRUE(bf.contains("banana"));
    // 可能为假阳性，不做严格为 false 的断言
}

TEST(BloomFilter, SerializeDeserialize) {
    BloomFilter bf(1000, 0.01);
    bf.add("k1");
    bf.add("k2");
    std::stringstream ss;
    bf.serialize(ss);
    auto restored = BloomFilter::deserialize(ss);
    ASSERT_TRUE(restored.has_value());
    EXPECT_TRUE(restored->contains("k1"));
    EXPECT_TRUE(restored->contains("k2"));
}
