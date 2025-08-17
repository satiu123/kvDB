export module kvdb.core:async_database;

import std;
import kvdb.core.coro.task;
import kvdb.core.types;
import kvdb.core.database.async_manifest;
import kvdb.storage.wal.async_wal;
import kvdb.core.database.manifest;
import kvdb.core.coro.task;
import kvdb.storage.sstable;
import kvdb.core.types;

import kvdb.core.io.io_uring;
using kvdb::core::coro::Task;
export namespace kvdb::core {
// 引入常用别名，避免到处写全限定名
using kvdb::core::types::KeyView;
using kvdb::core::types::OrderedKVMap;
using kvdb::core::types::ValueView;

class AsyncDatabase {
  public:
    explicit AsyncDatabase(std::string_view base_path = ".");

    // 禁用拷贝和移动
    AsyncDatabase(const AsyncDatabase&) = delete;
    AsyncDatabase& operator=(const AsyncDatabase&) = delete;
    AsyncDatabase(AsyncDatabase&&) = delete;
    AsyncDatabase& operator=(AsyncDatabase&&) = delete;

    // 异步操作
    auto async_put(KeyView key, ValueView value) -> Task<bool>;
    auto async_get(KeyView key) -> Task<std::optional<std::string>>;
    auto async_remove(KeyView key) -> Task<bool>;
    auto init() -> Task<void>;

    // 运行一个异步任务直到完成
    template <typename T>
    auto run(Task<T>&& task) -> T {
        // 启动任务
        task.resume();

        // 只要顶层任务没完成，就一直处理IO事件
        while (!task.done()) {
            ring_->wait_for_completion();
        }

        // 始终调用 get() 以获取结果并传播异常（void 任务也需要调用以抛出异常）
        if constexpr (std::is_void_v<T>) {
            task.get();
        } else {
            return task.get();
        }
    }

    // 并发运行一批任务（非 void 版本）：先全部启动，然后驱动 ring 直到全部完成，最后收集结果
    template <typename T>
    auto run_all(std::vector<Task<T>>&& tasks) -> std::vector<T> {
        for (auto& t : tasks)
            t.resume();
        auto any_not_done = [&]() {
            for (auto& t : tasks) {
                if (!t.done())
                    return true;
            }
            return false;
        };
        while (any_not_done()) {
            ring_->wait_for_completion();
        }
        std::vector<T> results;
        results.reserve(tasks.size());
        for (auto& t : tasks) {
            results.emplace_back(t.get());
        }
        return results;
    }

    // 并发运行一批任务（void 版本）
    auto run_all(std::vector<Task<void>>&& tasks) -> void {
        for (auto& t : tasks)
            t.resume();
        auto any_not_done = [&]() {
            for (auto& t : tasks) {
                if (!t.done())
                    return true;
            }
            return false;
        };
        while (any_not_done()) {
            ring_->wait_for_completion();
        }
        for (auto& t : tasks) {
            t.get();
        }
    }

    // Debug用
    void printManifest() const;
    Task<void> printWALRecords() const;
    Task<void> printSSTables() const;
    // 触发对当前所有SSTable的压缩合并
    Task<void> compact_sstables();
    // 获取内部的ring，供内部组件使用
    auto get_ring() -> kvdb::core::io::IOUring& {
        return *ring_;
    }
    auto set_flush_threshold(std::uint64_t threshold) -> void {
        flush_threshold_ = threshold;
    }

    // WAL 批量写策略配置
    struct WalBatchPolicy {
        bool enabled = false;             // 是否启用批量写策略
        std::size_t min_batch_count = 8;  // 触发批量写的最小条数
        std::size_t min_total_bytes =
            static_cast<std::size_t>(16) * 1024U;  // 触发批量写的最小总字节数
    };

    // 设置 WAL 批量写策略
    auto set_wal_batch_policy(const WalBatchPolicy& policy) -> void {
        wal_batch_policy_ = policy;
    }

  private:
    Task<void> flush_memtable_to_sstable();
    // 刷新 WAL 批量缓冲（达到阈值或需要一致性时调用）
    Task<bool> flush_wal_batch();

    std::unique_ptr<kvdb::core::io::IOUring> ring_;  // 拥有所有权
    // 归一化后的数据库根路径（绝对路径）
    std::string base_path_;
    // 可变内存表
    OrderedKVMap memtable_;
    // 不可变内存表
    std::unique_ptr<OrderedKVMap> immutable_memtable_;

    std::string sstables_path_;
    std::vector<std::unique_ptr<storage::SSTable>> sstables_;
    std::uint64_t max_sstable_num_ = 10;    // 最大SSTable数量
    std::uint64_t flush_threshold_ = 1024;  // 1MB
    std::unique_ptr<storage::AsyncWal> wal_;
    std::unique_ptr<database::AsyncManifestFile> manifest_;
    database::Manifest manifest_data_;
    WalBatchPolicy wal_batch_policy_{};  // WAL 批量写策略
    // WAL 批量缓冲
    std::vector<std::string> wal_batch_keys_;
    std::vector<std::string> wal_batch_values_;
    std::size_t wal_batch_total_bytes_ = 0;
    bool wal_batch_flush_in_progress_ = false;
};

}  // namespace kvdb::core