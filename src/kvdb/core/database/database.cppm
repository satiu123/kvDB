export module kvdb.core:database;

import std;
import kvdb.storage;
export import kvdb.core.database.manifest;


export namespace kvdb::core {

class Database {
  public:
    ~Database();
    explicit Database(std::string_view base_path = ".");

    // 禁用拷贝和移动
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    // 基本操作
    bool put(std::string_view key, std::string_view value);
    auto get(std::string_view key) const -> std::optional<std::string>;
    bool remove(std::string_view key);

    // 高级操作
    std::size_t size() const;
    void clear();
    bool exists(std::string_view key) const;
    std::vector<std::string> keys() const;
    void compact();

    // 配置
    void setMemtableFlushThreshold(std::size_t threshold);
    void printWALRecords() const;
    void printSSTables() const;
    void printManifest() const;

  private:
    void recover();
    void flushMemtableIfNeeded();
    auto get_all_data() const -> std::map<std::string, std::string, std::less<>>;
    auto get_locked(std::string_view key) const -> std::optional<std::string>;

    std::string sstables_path_;
    std::map<std::string, std::string, std::less<>> data_;  // 可变 MemTable
    std::unique_ptr<std::map<std::string, std::string, std::less<>>>
        immutable_memtable_;  // 不可变 MemTable
    mutable std::mutex mutex_;

    std::unique_ptr<storage::Wal> wal_;                        // WAL实例
    std::vector<std::unique_ptr<storage::SSTable>> sstables_;  // SSTable读取器
    std::unique_ptr<database::ManifestFile> manifest_;         // MANIFEST文件
    database::Manifest manifest_data_;                         // MANIFEST数据

    std::size_t memtable_flush_threshold_ = 1000;   // MemTable刷写阈值
    std::atomic<std::size_t> sstable_counter_ = 0;  // SSTable文件计数器
};

}  // namespace kvdb::core
