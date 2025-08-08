export module kvdb.storage.wal.async_wal;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.io.file;
import kvdb.core.coro.task;
import kvdb.storage.wal.wal_record;

using kvdb::core::coro::Task;
using kvdb::core::io::File;
using kvdb::core::io::IOUring;

export namespace kvdb::storage {

/**
 * @brief 异步模式的预写式日志 (Async Write-Ahead Log)
 */
class AsyncWal {
  public:
    /**
     * @brief 构造函数 (异步模式)
     * @param ring IOUring实例的引用
     * @param path WAL文件路径
     */
    explicit AsyncWal(IOUring& ring, const std::filesystem::path& path);

    /**
     * @brief 析构函数
     */
    ~AsyncWal() = default;

    // --- 异步 API ---
    Task<bool> async_append_put(std::string_view key, std::string_view value);
    Task<bool> async_append_remove(std::string_view key);
    Task<bool> async_append_clear();
    Task<bool> async_append_record(const WalRecord& record);
    Task<bool> async_replay(const std::function<bool(const WalRecord&)>& handler);
    Task<std::expected<WalRecord, std::string>> async_read_next_record();

    // --- 通用 API ---
    std::uint64_t getLastSequenceNumber() const;
    void setCurrentSequenceNumber(std::uint64_t seq);
    auto getFormattedContent() -> Task<std::expected<std::vector<std::string>, std::string>>;

  private:
    IOUring* ring_{nullptr};
    File wal_file_;
    std::atomic<std::uint64_t> sequence_number_{0};  // 序列号，用于记录操作顺序
    std::uint64_t read_offset_{0};                   // 用于追踪读取位置
};

}  // namespace kvdb::storage
