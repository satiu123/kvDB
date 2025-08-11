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
    explicit AsyncWal(IOUring& ring, const std::filesystem::path& path);
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
    // 填充读取缓冲区
    Task<std::expected<std::size_t, std::string>> fill_read_buffer();

    IOUring* ring_{nullptr};
    File wal_file_;
    std::atomic<std::uint64_t> sequence_number_{0};

    // --- 读取状态 ---
    static constexpr std::size_t READ_BUFFER_SIZE = 8 * 1024 * 1024;  // 8MB
    std::vector<std::byte> read_buffer_;
    std::uint64_t file_read_offset_ = 0;
    std::size_t buffer_pos_ = 0;
    std::size_t buffer_valid_size_ = 0;
};

}  // namespace kvdb::storage
