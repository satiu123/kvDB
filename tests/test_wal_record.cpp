#include <gtest/gtest.h>

#include <string>
#include <vector>

import kvdb.storage.wal.wal_record;
import kvdb.core.types;

using kvdb::core::types::ByteSpan;
using kvdb::storage::WalOpType;
using kvdb::storage::WalRecord;

TEST(WalRecord, SerializeDeserialize) {
    WalRecord r(WalOpType::PUT, "key", "value", 7);
    std::vector<std::byte> buf(r.size());
    auto sres = r.serialize_to(ByteSpan{buf.data(), buf.size()});
    ASSERT_TRUE(sres) << sres.error();

    auto dres = WalRecord::deserialize(ByteSpan{buf.data(), buf.size()});
    ASSERT_TRUE(dres) << dres.error();
    auto rr = *dres;
    EXPECT_EQ(rr.getOpType(), WalOpType::PUT);
    EXPECT_EQ(rr.getKey(), "key");
    EXPECT_EQ(rr.getValue(), "value");
    EXPECT_EQ(rr.getSequenceNumber(), 7u);
    EXPECT_FALSE(rr.toString().empty());
}

TEST(WalRecord, DeserializeTruncated) {
    // Buffer too small should fail
    std::vector<std::byte> buf(2);
    auto dres = WalRecord::deserialize(ByteSpan{buf.data(), buf.size()});
    EXPECT_FALSE(dres);
}

TEST(WalRecord, DeserializeCrcMismatch) {
    WalRecord r(WalOpType::PUT, "k", "v", 1);
    std::vector<std::byte> buf(r.size());
    auto sres = r.serialize_to(ByteSpan{buf.data(), buf.size()});
    ASSERT_TRUE(sres);
    // flip one byte in payload
    if (buf.size() > 8) {
        buf[8] = std::byte{static_cast<unsigned char>(~static_cast<unsigned char>(buf[8]))};
    }
    auto dres = WalRecord::deserialize(ByteSpan{buf.data(), buf.size()});
    EXPECT_FALSE(dres);
}
