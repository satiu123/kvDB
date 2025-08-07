module kvdb.storage.wal.wal_record;

import std;
import kvdb.logging.log;
import kvdb.core.binary;

using kvdb::core::binary::BytesBuffer;

namespace kvdb::storage {

// 构造函数
WalRecord::WalRecord(WalOpType op_type, std::string_view key, std::string_view value,
                     std::uint64_t sequence_number)
    : op_type_(op_type), key_(key), value_(value), sequence_number_(sequence_number) {
    // 在构造时计算并存储校验和
    checksum_ = calculateChecksum();
}

// 序列化记录为 std::vector<std::byte>
auto WalRecord::serialize() const -> std::vector<std::byte> {
    BytesBuffer payload_buf;
    // 1. 写入核心数据到 payload buffer
    kvdb::core::binary::write_uint8(payload_buf, static_cast<std::uint8_t>(op_type_));
    kvdb::core::binary::write_string(payload_buf, key_);
    kvdb::core::binary::write_string(payload_buf, value_);
    kvdb::core::binary::write_uint64(payload_buf, sequence_number_);

    // 2. 计算 payload 的校验和
    std::uint32_t crc = kvdb::core::binary::calculate_crc32(payload_buf.get_span());

    // 3. 构建最终的完整记录 buffer
    BytesBuffer final_buf;
    auto total_size =
        static_cast<std::uint32_t>(sizeof(std::uint32_t) * 2 + payload_buf.get_data().size());
    kvdb::core::binary::write_uint32(final_buf, total_size);
    kvdb::core::binary::write_uint32(final_buf, crc);
    final_buf.push(payload_buf.get_data().data(), payload_buf.get_data().size());

    return final_buf.get_data();
}

// 从 std::span<const std::byte> 反序列化记录
auto WalRecord::deserialize(std::span<const std::byte> data)
    -> std::expected<std::unique_ptr<WalRecord>, std::string> {
    if (data.empty()) {
        return std::unexpected("Cannot deserialize from empty data");
    }

    BytesBuffer buf(std::vector<std::byte>(data.begin(), data.end()));

    // 1. 读取总长度
    auto total_size_res = kvdb::core::binary::read_uint32(buf);
    if (!total_size_res)
        return std::unexpected("Failed to read total size: " + total_size_res.error());
    if (*total_size_res != data.size())
        return std::unexpected("Record size mismatch");

    // 2. 读取并校验CRC
    auto stored_crc_res = kvdb::core::binary::read_uint32(buf);
    if (!stored_crc_res)
        return std::unexpected("Failed to read checksum: " + stored_crc_res.error());

    // 3. 校验 payload
    // 获取从当前偏移量到结尾的 payload span
    auto payload_span = buf.get_span().subspan(buf.get_offset());
    if (kvdb::core::binary::calculate_crc32(payload_span) != *stored_crc_res) {
        return std::unexpected("Checksum mismatch, record may be corrupted");
    }

    // 4. 从 payload 中解析字段
    auto op_type_res = kvdb::core::binary::read_uint8(buf);
    if (!op_type_res)
        return std::unexpected(op_type_res.error());

    auto key_res = kvdb::core::binary::read_string(buf);
    if (!key_res)
        return std::unexpected(key_res.error());

    auto value_res = kvdb::core::binary::read_string(buf);
    if (!value_res)
        return std::unexpected(value_res.error());

    auto seq_num_res = kvdb::core::binary::read_uint64(buf);
    if (!seq_num_res)
        return std::unexpected(seq_num_res.error());

    return std::make_unique<WalRecord>(static_cast<WalOpType>(*op_type_res), *key_res, *value_res,
                                       *seq_num_res);
}

// 获取记录序列化后大小
std::size_t WalRecord::size() const {
    // Payload size = op_type + key_with_len + value_with_len + sequence_number
    std::size_t payload_size = sizeof(std::uint8_t) + (sizeof(std::uint32_t) + key_.size()) +
                               (sizeof(std::uint32_t) + value_.size()) + sizeof(std::uint64_t);

    // Total size = total_length_field + crc_field + payload_size
    return sizeof(std::uint32_t) + sizeof(std::uint32_t) + payload_size;
}

// 辅助函数：将 WalOpType 转换为字符串
static std::string op_type_to_string(WalOpType op_type) {
    switch (op_type) {
        case WalOpType::PUT:
            return "PUT";
        case WalOpType::REMOVE:
            return "REMOVE";
        case WalOpType::CLEAR:
            return "CLEAR";
        default:
            return "UNKNOWN";
    }
}

// 获取记录的字符串表示
std::string WalRecord::toString() const {
    return std::format("WalRecord(序列号={}, 操作类型={}, key='{}', value='{}')", sequence_number_,
                       op_type_to_string(op_type_), key_, value_);
}

// 计算记录的CRC32校验和
std::uint32_t WalRecord::calculateChecksum() const {
    BytesBuffer payload_buf;
    kvdb::core::binary::write_uint8(payload_buf, static_cast<std::uint8_t>(op_type_));
    kvdb::core::binary::write_string(payload_buf, key_);
    kvdb::core::binary::write_string(payload_buf, value_);
    kvdb::core::binary::write_uint64(payload_buf, sequence_number_);
    return kvdb::core::binary::calculate_crc32(payload_buf.get_span());
}

// 验证记录的校验和
bool WalRecord::validateChecksum() const {
    return calculateChecksum() == checksum_;
}

}  // namespace kvdb::storage