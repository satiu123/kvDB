export module kvdb.core.database.manifest;

import std;

export namespace kvdb::core::database {

/**
 * @brief Manifest 文件的内容
 * @details 存储了数据库的元数据，如SSTable文件列表和最后的WAL序列号
 */
struct Manifest {
    std::uint64_t last_wal_sequence_number = 0;
    std::map<int, std::vector<std::string>> sstables;

    // 序列化到字符串
    auto serialize(std::ostream& os) const -> std::expected<void, std::string>;

    // 从字符串反序列化
    auto deserialize(std::istream& is) -> std::expected<void, std::string>;
};
}  // namespace kvdb::core::database