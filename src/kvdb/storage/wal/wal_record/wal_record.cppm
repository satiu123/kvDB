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
 * @brief WAL记录类，表示一个预写日志记录
 *
 * 该类表示一个WAL记录，用于持久化数据库操作
 * 支持序列化到二进制格式和从二进制格式反序列化
 */
class WalRecord {
  public:
    // 构造函数
    WalRecord(WalOpType op_type, std::string_view key = "", std::string_view value = "");

    // 从二进制数据反序列化一条记录
    static std::unique_ptr<WalRecord> deserialize(const std::vector<std::uint8_t>& data);
    static std::unique_ptr<WalRecord> deserialize(const std::uint8_t* data, std::size_t size);

    // 序列化记录为二进制数据
    std::vector<std::uint8_t> serialize() const;

    // 获取记录大小（序列化后）
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

    /*
     * @brief 获取记录的字符串表示
     * @return 格式化的字符串表示
     */
    std::string toString() const;

    // 获取记录头部大小
    static constexpr std::size_t getHeaderSize() {
        return HEADER_SIZE;
    }

    // 计算记录校验和
    std::uint32_t calculateChecksum() const;
    bool validateChecksum() const;

  private:
    WalOpType op_type_;       // 操作类型
    std::string key_;         // 键
    std::string value_;       // 值（仅用于PUT操作）
    std::uint32_t checksum_;  // 校验和，用于验证记录完整性

    // 记录头部格式常量
    static constexpr std::size_t HEADER_SIZE =
        17;  // 1(op_type) + 4(key_size) + 4(value_size) + 4(checksum) + 4(total_size)
};

}  // namespace kvdb::storage