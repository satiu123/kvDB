export module kvdb.storage.wal.wal_record;

import std;

export namespace kvdb::storage {

// WAL记录操作类型
enum class WalOpType : std::uint8_t {
    PUT,     // 插入或更新键值对
    REMOVE,  // 删除键
    CLEAR    // 清空数据库
};

/**
 * @brief WAL记录类，表示一个预写日志记录 (零拷贝视图版本)
 *
 * 该类是一个轻量级视图，它不拥有key和value的数据，而是引用外部缓冲区。
 * 调用者必须保证WalRecord的生命周期短于其引用的数据缓冲区。
 */
class WalRecord {
  public:
    // 构造函数 - 用于创建准备序列化的记录
    explicit WalRecord(WalOpType op_type, std::string_view key, std::string_view value,
                       std::uint64_t sequence_number);

    // 从二进制数据反序列化一条记录，返回一个视图化的WalRecord
    static auto deserialize(std::span<const std::byte> data)
        -> std::expected<WalRecord, std::string>;

    // 序列化记录到给定的缓冲区
    auto serialize_to(std::span<std::byte> target_buffer) const
        -> std::expected<std::size_t, std::string>;

    // 获取记录序列化后所需的总大小
    std::size_t size() const;

    // 访问器
    WalOpType getOpType() const {
        return op_type_;
    }
    std::string_view getKey() const {
        return key_;
    }
    std::string_view getValue() const {
        return value_;
    }
    std::uint64_t getSequenceNumber() const {
        return sequence_number_;
    }
    std::uint32_t getChecksum() const {
        return checksum_;
    }

    // 获取记录的字符串表示
    std::string toString() const;

  private:
    // 私有构造函数，用于反序列化
    explicit WalRecord(WalOpType op_type, std::string_view key, std::string_view value,
                       std::uint64_t sequence_number, std::uint32_t checksum);

    WalOpType op_type_;
    std::string_view key_;
    std::string_view value_;
    std::uint64_t sequence_number_;
    std::uint32_t checksum_;
};

}  // namespace kvdb::storage
