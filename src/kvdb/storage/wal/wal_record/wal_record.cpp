module kvdb.storage.wal.wal_record;

import std;
import kvdb.core.binary;

using kvdb::core::binary::BytesBufferView;

namespace kvdb::storage {

// --- 辅助函数 ---

static std::uint32_t calculate_payload_checksum(WalOpType op_type, std::string_view key,
                                                std::string_view value, std::uint64_t seq) {
    std::size_t payload_size = sizeof(std::uint8_t) + (sizeof(std::uint32_t) + key.size()) +
                               (sizeof(std::uint32_t) + value.size()) + sizeof(std::uint64_t);
    std::vector<std::byte> temp_buffer(payload_size);
    // 显式创建可写 span 以消除构造函数歧义
    BytesBufferView temp_buf_view(std::span<std::byte>{temp_buffer});

    temp_buf_view.write_uint8(static_cast<std::uint8_t>(op_type));
    temp_buf_view.write_string(key);
    temp_buf_view.write_string(value);
    temp_buf_view.write_uint64(seq);

    return kvdb::core::binary::calculate_crc32(temp_buf_view.get_written_span());
}

// --- 构造函数 ---

WalRecord::WalRecord(WalOpType op_type, std::string_view key, std::string_view value,
                     std::uint64_t sequence_number)
    : op_type_(op_type),
      key_(key),
      value_(value),
      sequence_number_(sequence_number),
      checksum_(calculate_payload_checksum(op_type, key, value, sequence_number)) {}

WalRecord::WalRecord(WalOpType op_type, std::string_view key, std::string_view value,
                     std::uint64_t sequence_number, std::uint32_t checksum)
    : op_type_(op_type),
      key_(key),
      value_(value),
      sequence_number_(sequence_number),
      checksum_(checksum) {}

// --- 核心 API ---

auto WalRecord::deserialize(std::span<const std::byte> data)
    -> std::expected<WalRecord, std::string> {
    if (data.empty()) {
        return std::unexpected("Cannot deserialize from empty data");
    }

    BytesBufferView buf(data);

    auto total_size_res = buf.read_uint32();
    if (!total_size_res)
        return std::unexpected(total_size_res.error());
    if (*total_size_res != data.size())
        return std::unexpected("Record size mismatch");

    auto stored_crc_res = buf.read_uint32();
    if (!stored_crc_res)
        return std::unexpected(stored_crc_res.error());

    auto payload_span = data.subspan(sizeof(std::uint32_t) * 2);
    if (kvdb::core::binary::calculate_crc32(payload_span) != *stored_crc_res) {
        return std::unexpected("Checksum mismatch, record may be corrupted");
    }

    auto op_type_res = buf.read_uint8();
    if (!op_type_res)
        return std::unexpected(op_type_res.error());

    auto key_res = buf.read_string_view();
    if (!key_res)
        return std::unexpected(key_res.error());

    auto value_res = buf.read_string_view();
    if (!value_res)
        return std::unexpected(value_res.error());

    auto seq_num_res = buf.read_uint64();
    if (!seq_num_res)
        return std::unexpected(seq_num_res.error());

    return WalRecord(static_cast<WalOpType>(*op_type_res), *key_res, *value_res, *seq_num_res,
                     *stored_crc_res);
}

auto WalRecord::serialize_to(std::span<std::byte> target_buffer) const
    -> std::expected<std::size_t, std::string> {
    const auto required_size = size();
    if (target_buffer.size() < required_size) {
        return std::unexpected("Target buffer is too small");
    }

    BytesBufferView buf(target_buffer);

    buf.write_uint32(static_cast<std::uint32_t>(required_size));
    buf.write_uint32(checksum_);
    buf.write_uint8(static_cast<std::uint8_t>(op_type_));
    buf.write_string(key_);
    buf.write_string(value_);
    buf.write_uint64(sequence_number_);

    return required_size;
}

std::size_t WalRecord::size() const {
    std::size_t payload_size = sizeof(std::uint8_t) + (sizeof(std::uint32_t) + key_.size()) +
                               (sizeof(std::uint32_t) + value_.size()) + sizeof(std::uint64_t);
    return sizeof(std::uint32_t) * 2 + payload_size;
}

// --- 其他辅助函数 ---

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

std::string WalRecord::toString() const {
    return std::format("WalRecord(序列号={}, 操作类型={}, key='{}', value='{}', CRC={:x})",
                       sequence_number_, op_type_to_string(op_type_), key_, value_, checksum_);
}

}  // namespace kvdb::storage