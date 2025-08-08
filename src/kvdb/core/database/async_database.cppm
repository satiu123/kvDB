export module kvdb.core:async_database;

import std;
import kvdb.core.coro.task;
import kvdb.core.database.async_manifest;
import kvdb.storage.wal.async_wal;
import kvdb.core.database.manifest;
import kvdb.core.coro.task;

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
    auto put(std::string_view key, std::string_view value) -> kvdb::core::coro::Task<bool>;
    auto get(std::string_view key) -> kvdb::core::coro::Task<std::optional<std::string>>;
    auto remove(std::string_view key) -> kvdb::core::coro::Task<bool>;
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
            return task.get_result();
        }
    }

    // 针对 void 任务的特化
    void run(kvdb::core::coro::Task<void>&& task) {
        // 启动任务
        task.resume();

        // 只要顶层任务没完成，就一直处理IO事件
        while (!task.done()) {
            ring_->wait_for_completion();
        }
    }

    // Debug用
    void printManifest() const;
    Task<void> printWALRecords() const;
    void printSSTables() const;
    // 获取内部的ring，供内部组件使用
    auto get_ring() -> kvdb::core::io::IOUring& {
        return *ring_;
    }

  private:
    std::unique_ptr<kvdb::core::io::IOUring> ring_;  // 拥有所有权
    // MemTable
    std::map<std::string, std::string, std::less<>> memtable_;
    std::unique_ptr<std::map<std::string, std::string, std::less<>>> immutable_memtable_;

    std::string sstables_path_;
    std::unique_ptr<storage::AsyncWal> wal_;
    std::unique_ptr<database::AsyncManifestFile> manifest_;
    database::Manifest manifest_data_;
};

}  // namespace kvdb::core
