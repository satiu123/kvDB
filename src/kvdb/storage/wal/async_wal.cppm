export module kvdb.storage.wal.async_wal;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.io.file;
import kvdb.core.coro.task;
import kvdb.storage.wal.wal_record;
import kvdb.core.types;

using kvdb::core::coro::Task;
using kvdb::core::io::File;
using kvdb::core::io::IOUring;
using kvdb::core::types::KeyView;
using kvdb::core::types::Result;
using kvdb::core::types::ValueView;

export namespace kvdb::storage {

/**
 * @brief 异步模式的预写式日志 (Async Write-Ahead Log)
 */
class AsyncWal {
  public:
    explicit AsyncWal(IOUring& ring, const std::filesystem::path& path);
    ~AsyncWal() = default;

    // WAL 组提交/刷盘选项
    enum class SyncMode : std::uint8_t { None, FDataSync, FSync };
    struct Options {
        bool group_commit = true;  // 是否启用组提交
        std::size_t max_buffer_bytes =
            static_cast<std::size_t>(256) * 1024;  // 达到该字节阈值触发写入
        std::size_t max_records = 1024;            // 达到该记录数阈值触发写入
        std::uint32_t max_interval_ms = 10;        // 距上次写入超过该间隔则写入
        SyncMode sync_mode = SyncMode::FDataSync;  // 同步策略
    };
    void set_options(const Options& opt) {
        options_ = opt;
    }

    // --- 异步 API ---
    Task<bool> async_append_put(KeyView key, ValueView value);
    Task<bool> async_append_remove(KeyView key);
    Task<bool> async_append_clear();
    Task<bool> async_append_record(const WalRecord& record);
    // 批量追加多条 PUT 记录：序列化到一个缓冲区，一次写入，降低写放大
    Task<bool> async_append_batch_put(std::span<const std::string> keys,
                                      std::span<const std::string> values);
    Task<bool> async_replay(const std::function<bool(const WalRecord&)>& handler);
    Task<Result<WalRecord>> async_read_next_record();

    // 刷新与同步（用于在生成 SSTable 或显式要求持久化前调用）
    Task<bool> async_flush(bool force = false);
    Task<bool> async_sync();  // flush 后根据 sync_mode 执行 (f)data sync

    // --- 通用 API ---
    std::uint64_t getLastSequenceNumber() const;
    void setCurrentSequenceNumber(std::uint64_t seq);
    // 截断WAL文件（清空），用于在数据安全落盘后回收WAL
    void truncate();
    auto getFormattedContent() -> Task<Result<std::vector<std::string>>>;

  private:
    // 填充读取缓冲区
    Task<Result<std::size_t>> fill_read_buffer();

    IOUring* ring_{nullptr};
    std::string wal_path_;
    File wal_file_;
    std::atomic<std::uint64_t> sequence_number_{0};

    // --- 写入缓冲（组提交）---
    Options options_{};
    std::vector<std::byte> write_buffer_;
    std::size_t pending_records_{0};
    std::chrono::steady_clock::time_point last_flush_tp_{std::chrono::steady_clock::now()};

    // --- 读取状态 ---
    static constexpr std::size_t READ_BUFFER_SIZE =
        static_cast<std::size_t>(8) * 1024 * 1024;  // 8MB
    std::vector<std::byte> read_buffer_;
    std::uint64_t file_read_offset_ = 0;
    std::size_t buffer_pos_ = 0;
    std::size_t buffer_valid_size_ = 0;
};

}  // namespace kvdb::storage
