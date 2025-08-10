export module kvdb.storage.sstable;

import std;

import kvdb.storage.bloom_filter;
import kvdb.core.coro.task;
import kvdb.core.io.file;
import kvdb.core.io.io_uring;


using kvdb::core::coro::Task;
using kvdb::core::io::File;
using kvdb::core::io::IOUring;
export namespace kvdb::storage {

// 用于文件验证的魔数
constexpr std::uint64_t SSTABLE_MAGIC = 0x4B56444253535441;  // "KVDB_SSTA"

struct Footer {
    std::uint64_t index_block_offset;
    std::uint64_t index_block_size;
    std::uint64_t bloom_filter_offset;
    std::uint64_t bloom_filter_size;
    std::uint64_t magic = SSTABLE_MAGIC;
};

class SSTable {
  public:
    explicit SSTable(IOUring& ring);
    class Builder {
      public:
        explicit Builder(IOUring& ring, std::string_view path,
                         std::size_t block_size_threshold = 4096);

        Task<void> add(std::string_view key, std::string_view value);
        Task<bool> finish(const std::map<std::string, std::string, std::less<>>& data);

      private:
        Task<void> writeBlock();

        // std::ofstream file_;
        IOUring& ring_;
        File out_file_;
        std::string path_;
        std::size_t block_size_threshold_;

        std::string current_block_data_;
        struct IndexRecord {
            std::string last_key;
            std::uint64_t offset;
            std::uint64_t size;
        };
        std::vector<IndexRecord> index_;
        std::uint64_t offset_ = 0;
        std::string last_key_in_block_;
    };

    // 从map构建SSTable的静态函数
    static Task<bool> buildFrom(IOUring& ring, std::string_view path,
                                const std::map<std::string, std::string, std::less<>>& data);

    // 用于读取的成员函数
    Task<bool> open(std::string_view path);
    Task<std::optional<std::string>> find(std::string_view key);
    Task<std::map<std::string, std::string>> readAll();
    const std::string& getPath() const {
        return path_;
    }

  private:
    struct IndexRecord {
        std::string last_key;
        std::uint64_t offset;
        std::uint64_t size;
    };
    Task<bool> loadIndex();
    Task<bool> loadBloomFilter();

    // std::ifstream file_;
    IOUring& ring_;
    File in_file_;
    std::string path_;
    Footer footer_;
    std::vector<IndexRecord> index_;
    std::unique_ptr<BloomFilter> bloom_filter_;
};

}  // namespace kvdb::storage