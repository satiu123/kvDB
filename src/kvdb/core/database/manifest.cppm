export module kvdb.core.database.manifest;

import std;
import kvdb.core.types;

export namespace kvdb::core::database {
using kvdb::core::types::Result;

/**
 * @brief Manifest 文件的内容
 * @details 存储了数据库的元数据，如SSTable文件列表和最后的WAL序列号
 */
struct Manifest {
    std::uint64_t last_wal_sequence_number = 0;
    std::map<int, std::vector<std::string>> sstables;

    // 序列化到字符串
    auto serialize(std::ostream& os) const -> Result<void>;

    // 从字符串反序列化
    auto deserialize(std::istream& is) -> Result<void>;
};
}  // namespace kvdb::core::database