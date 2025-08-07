module kvdb.storage.wal.wal_record;

import std;
import kvdb.logging.log;
import kvdb.core.binary;

namespace kvdb::storage {

// 构造函数
WalRecord::WalRecord(WalOpType op_type, std::string_view key, std::string_view value,
                     std::uint64_t sequence_number)
    : op_type_(op_type),
      key_(key),
      value_(value),
      checksum_(calculateChecksum()),
      sequence_number_(sequence_number) {
    // 计算并存储校验和
}

// 序列化记录为二进制数据
std::vector<std::uint8_t> WalRecord::serialize() const {
    std::stringstream buffer;
    // 1. 操作类型
    kvdb::core::binary::write_uint8(buffer, static_cast<std::uint8_t>(op_type_));
    // 2. 键
    kvdb::core::binary::write_string(buffer, key_);
    // 3. 值
    kvdb::core::binary::write_string(buffer, value_);
    // 4. 序列号
    kvdb::core::binary::write_uint64(buffer, sequence_number_);

    std::string content = buffer.str();
    std::vector<std::uint8_t> data(content.begin(), content.end());

    // 计算并前置校验和
    std::uint32_t crc = kvdb::core::binary::calculate_crc32(data);
    std::vector<std::uint8_t> crc_bytes(sizeof(crc));
    std::memcpy(crc_bytes.data(), &crc, sizeof(crc));
    data.insert(data.begin(), crc_bytes.begin(), crc_bytes.end());

    // 前置总长度
    auto total_size = static_cast<std::uint32_t>(data.size() + sizeof(std::uint32_t));
    std::vector<std::uint8_t> size_bytes(sizeof(total_size));
    std::memcpy(size_bytes.data(), &total_size, sizeof(total_size));
    data.insert(data.begin(), size_bytes.begin(), size_bytes.end());

    return data;
}

// 从二进制数据反序列化记录
auto WalRecord::deserialize(const std::vector<std::uint8_t>& data)
    -> std::expected<std::unique_ptr<WalRecord>, std::string> {
    if (data.empty()) {
        return std::unexpected("Cannot deserialize from empty data");
    }
    return deserialize(data.data(), data.size());
}

auto WalRecord::deserialize(const std::uint8_t* data, std::size_t size)
    -> std::expected<std::unique_ptr<WalRecord>, std::string> {
    std::stringstream stream(std::string(reinterpret_cast<const char*>(data), size));

    // 1. 读取总长度
    auto total_size_res = kvdb::core::binary::read_uint32(stream);
    if (!total_size_res)
        return std::unexpected("Failed to read total size: " + total_size_res.error());
    if (*total_size_res != size)
        return std::unexpected("Record size mismatch");

    // 2. 读取并校验CRC
    auto stored_crc_res = kvdb::core::binary::read_uint32(stream);
    if (!stored_crc_res)
        return std::unexpected("Failed to read checksum: " + stored_crc_res.error());

    // 剩下的部分是真实数据
    std::string payload_str = stream.str().substr(sizeof(std::uint32_t) + sizeof(std::uint32_t));
    std::vector<std::uint8_t> payload_vec(payload_str.begin(), payload_str.end());

    if (kvdb::core::binary::calculate_crc32(payload_vec) != *stored_crc_res) {
        return std::unexpected("Checksum mismatch, record may be corrupted");
    }

    // 3. 从真实数据中解析字段
    std::stringstream payload_stream(payload_str);
    auto op_type_res = kvdb::core::binary::read_uint8(payload_stream);
    if (!op_type_res)
        return std::unexpected(op_type_res.error());

    auto key_res = kvdb::core::binary::read_string(payload_stream);
    if (!key_res)
        return std::unexpected(key_res.error());

    auto value_res = kvdb::core::binary::read_string(payload_stream);
    if (!value_res)
        return std::unexpected(value_res.error());

    auto seq_num_res = kvdb::core::binary::read_uint64(payload_stream);
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
    std::stringstream buffer;
    kvdb::core::binary::write_uint8(buffer, static_cast<std::uint8_t>(op_type_));
    kvdb::core::binary::write_string(buffer, key_);
    kvdb::core::binary::write_string(buffer, value_);
    kvdb::core::binary::write_uint64(buffer, sequence_number_);

    std::string content = buffer.str();
    std::vector<std::uint8_t> data(content.begin(), content.end());
    return kvdb::core::binary::calculate_crc32(data);
}


// 验证记录的校验和
bool WalRecord::validateChecksum() const {
    return calculateChecksum() == checksum_;
}

}  // namespace kvdb::storage
