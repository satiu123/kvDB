export module kvdb.storage.wal;

import std;
export import kvdb.storage.wal.wal_record;

export namespace kvdb::storage {

/**
 * @brief (同步)预写式日志(Write-Ahead Log)类
 */
class Wal {
  public:
    explicit Wal(const std::filesystem::path& path);
    ~Wal();

    // 禁止拷贝和移动
    Wal(const Wal&) = delete;
    Wal& operator=(const Wal&) = delete;
    Wal(Wal&&) = delete;
    Wal& operator=(Wal&&) = delete;

    bool appendPut(std::string_view key, std::string_view value);
    bool appendRemove(std::string_view key);
    bool appendClear();
    bool appendRecord(const WalRecord& record);
    bool replay(const std::function<bool(const WalRecord&)>& handler);
    bool sync();
    bool truncate();
    bool isEmpty();
    void close();
    auto getFormattedContent() -> std::expected<std::vector<std::string>, std::string>;
    std::uint64_t getLastSequenceNumber() const;
    void setCurrentSequenceNumber(std::uint64_t seq);

  private:
    std::string path_;
    std::fstream file_;
    mutable std::mutex mutex_;
    bool is_open_ = false;
    std::atomic<std::uint64_t> sequence_number_{0};

    std::jthread sync_thread_;
    std::condition_variable cv_;
    std::mutex sync_mutex_;
    std::atomic<bool> stop_sync_ = false;
    std::atomic<bool> has_new_data_ = false;

    void syncLoop();
    bool isEmpty_locked();
    void sync_locked();
    bool open(bool truncate = false);
    auto readNextRecord() -> std::expected<std::unique_ptr<WalRecord>, std::string>;
};

}  // namespace kvdb::storage