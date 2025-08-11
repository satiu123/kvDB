export module kvdb.core:async_database;

import std;
import kvdb.core.coro.task;
import kvdb.core.database.async_manifest;
import kvdb.storage.wal.async_wal;
import kvdb.core.database.manifest;
import kvdb.core.coro.task;
import kvdb.storage.sstable;

import kvdb.core.io.io_uring;
using kvdb::core::coro::Task;
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
    auto async_put(std::string_view key, std::string_view value) -> kvdb::core::coro::Task<bool>;
    auto async_get(std::string_view key) -> kvdb::core::coro::Task<std::optional<std::string>>;
    auto async_remove(std::string_view key) -> kvdb::core::coro::Task<bool>;
    auto init() -> kvdb::core::coro::Task<void>;

    // 运行一个异步任务直到完成
    template <typename T>
    auto run(kvdb::core::coro::Task<T>&& task) -> T {
        // 启动任务
        task.resume();

        // 只要顶层任务没完成，就一直处理IO事件
        while (!task.done()) {
            ring_->wait_for_completion();
        }

        // 如果任务有返回值，则返回
        if constexpr (!std::is_void_v<T>) {
            return task.get();
        }
    }

    // Debug用
    void printManifest() const;
    Task<void> printWALRecords() const;
    Task<void> printSSTables() const;
    // 获取内部的ring，供内部组件使用
    auto get_ring() -> kvdb::core::io::IOUring& {
        return *ring_;
    }
    auto set_flush_threshold(std::uint64_t threshold) -> void {
        flush_threshold_ = threshold;
    }

  private:
    Task<void> flush_memtable_to_sstable();
    std::unique_ptr<kvdb::core::io::IOUring> ring_;  // 拥有所有权
    // 可变内存表
    std::map<std::string, std::string, std::less<>> memtable_;
    // 不可变内存表
    std::unique_ptr<std::map<std::string, std::string, std::less<>>> immutable_memtable_;

    std::string sstables_path_;
    std::vector<std::unique_ptr<storage::SSTable>> sstables_;
    std::uint64_t max_sstable_num_ = 0;
    std::uint64_t flush_threshold_ = 1024;  // 1MB
    std::unique_ptr<storage::AsyncWal> wal_;
    std::unique_ptr<database::AsyncManifestFile> manifest_;
    database::Manifest manifest_data_;
};

}  // namespace kvdb::core