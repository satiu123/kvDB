export module kvdb.core:async_database;

import std;
import kvdb.core.coro.task;
import kvdb.core.database.manifest;
import kvdb.storage;

export namespace kvdb::core {

class AsyncDatabase {
  public:
    explicit AsyncDatabase(std::string_view base_path = ".");

    // 禁用拷贝和移动
    AsyncDatabase(const AsyncDatabase&) = delete;
    AsyncDatabase& operator=(const AsyncDatabase&) = delete;
    AsyncDatabase(AsyncDatabase&&) = delete;
    AsyncDatabase& operator=(AsyncDatabase&&) = delete;

    // 异步操作
    auto put(std::string_view key, std::string_view value) -> kvdb::core::coro::Task<bool>;
    auto get(std::string_view key) -> kvdb::core::coro::Task<std::optional<std::string>>;
    auto remove(std::string_view key) -> kvdb::core::coro::Task<bool>;
    auto open() -> kvdb::core::coro::Task<void>;

  private:
    // MemTable
    std::map<std::string, std::string, std::less<>> memtable_;
    std::unique_ptr<std::map<std::string, std::string, std::less<>>> immutable_memtable_;

    std::string sstables_path_;
    std::unique_ptr<storage::Wal> wal_;
    std::unique_ptr<database::ManifestFile> manifest_;
    database::Manifest manifest_data_;
};

}  // namespace kvdb::core
