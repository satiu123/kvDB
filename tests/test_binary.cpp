#include <gtest/gtest.h>

#include <string>
#include <vector>

import kvdb.core.binary;
import kvdb.core.types;

using kvdb::core::binary::BytesBuffer;
using kvdb::core::binary::BytesBufferView;
using kvdb::core::binary::calculate_crc32;
using kvdb::core::types::ByteSpan;
using kvdb::core::types::ConstByteSpan;

TEST(Binary, BufferRoundtrip) {
    BytesBuffer buf;
    std::string s = "hello";
    std::uint32_t u32 = 0x12345678;
    std::uint64_t u64 = 0x1122334455667788ULL;
    buf.push(std::bit_cast<const std::byte*>(&u32), sizeof(u32));
    buf.push(std::bit_cast<const std::byte*>(&u64), sizeof(u64));
    buf.push_string(s);

    auto span = buf.get_span();
    BytesBufferView view(span);
    auto ru32 = view.read_uint32();
    ASSERT_TRUE(ru32);
    EXPECT_EQ(*ru32, u32);
    auto ru64 = view.read_uint64();
    ASSERT_TRUE(ru64);
    EXPECT_EQ(*ru64, u64);
    auto rs = view.read_string_view();
    ASSERT_TRUE(rs);
    EXPECT_EQ(*rs, s);
}

TEST(Binary, CRC32) {
    std::vector<std::byte> data(4);
    std::string s = "abcd";
    std::memcpy(data.data(), s.data(), s.size());
    ConstByteSpan span{data.data(), data.size()};
    auto crc = calculate_crc32(span);
    EXPECT_NE(crc, 0u);
}

TEST(Binary, ReadOutOfBounds) {
    // Buffer with only 1 byte, attempt to read uint32 should fail
    std::vector<std::byte> data(1);
    BytesBufferView view(ConstByteSpan{data.data(), data.size()});
    auto r = view.read_uint32();
    EXPECT_FALSE(r);
}

TEST(Binary, ReadStringOutOfBounds) {
    // Craft buffer: length=8 (uint32), but only 2 bytes payload -> should fail
    std::vector<std::byte> data(6);
    std::uint32_t len = 8;
    std::memcpy(data.data(), &len, sizeof(len));
    data[4] = std::byte{0x41};
    data[5] = std::byte{0x42};
    BytesBufferView view(ConstByteSpan{data.data(), data.size()});
    auto r = view.read_string_view();
    EXPECT_FALSE(r);
}

TEST(Binary, WriteTooSmall) {
    std::vector<std::byte> buf(2);
    kvdb::core::types::ByteSpan w{buf.data(), buf.size()};
    BytesBufferView view(w);
    // Write uint32 requires 4 bytes -> should fail
    bool ok = view.write_uint32(123U);
    EXPECT_FALSE(ok);
}
