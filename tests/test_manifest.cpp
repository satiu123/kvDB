#include <gtest/gtest.h>

#include <sstream>

import kvdb.core.database.manifest;

using kvdb::core::database::Manifest;

TEST(Manifest, SerializeDeserialize) {
    Manifest m;
    m.last_wal_sequence_number = 42;
    m.sstables[0] = {"sstable-000001.sst", "sstable-000002.sst"};
    m.sstables[1] = {"sstable-000010.sst"};

    std::stringstream ss;
    auto sres = m.serialize(ss);
    ASSERT_TRUE(sres);

    Manifest m2;
    auto dres = m2.deserialize(ss);
    ASSERT_TRUE(dres);
    EXPECT_EQ(m2.last_wal_sequence_number, 42u);
    ASSERT_TRUE(m2.sstables.contains(0));
    ASSERT_TRUE(m2.sstables.contains(1));
    EXPECT_EQ(m2.sstables[0].size(), 2u);
    EXPECT_EQ(m2.sstables[1].size(), 1u);
}

TEST(Manifest, CRCMismatch) {
    Manifest m;
    m.last_wal_sequence_number = 1;
    std::stringstream ss;
    auto sres = m.serialize(ss);
    ASSERT_TRUE(sres);
    std::string content = ss.str();
    // Flip one byte in CRC area safely
    if (!content.empty()) {
        auto uc = static_cast<unsigned char>(content[0]);
        uc ^= static_cast<unsigned char>(0xFF);
        content[0] = static_cast<char>(uc);
    }
    std::stringstream ss2(content);
    Manifest m2;
    auto dres = m2.deserialize(ss2);
    EXPECT_FALSE(dres);
}
