export module kvdb.storage.sstable;

import std;

import kvdb.storage.bloom_filter;

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
    class Builder {
      public:
        explicit Builder(std::string_view path, std::size_t block_size_threshold = 4096);

        void add(std::string_view key, std::string_view value);
        bool finish(const std::map<std::string, std::string>& data);

      private:
        void writeBlock();

        std::ofstream file_;
        std::string path_;
        std::size_t block_size_threshold_;

        std::string current_block_data_;
        std::vector<std::pair<std::string, std::uint64_t>> index_;
        std::uint64_t offset_ = 0;
        std::string last_key_in_block_;
    };

    // 从map构建SSTable的静态函数
    static bool buildFrom(std::string_view path, const std::map<std::string, std::string>& data);

    // 用于读取的成员函数
    bool open(std::string_view path);
    std::optional<std::string> find(std::string_view key);
    std::map<std::string, std::string> readAll();
    const std::string& getPath() const {
        return path_;
    }

  private:
    bool loadIndex();
    bool loadBloomFilter();

    std::ifstream file_;
    std::string path_;
    Footer footer_;
    std::vector<std::pair<std::string, std::uint64_t>> index_;
    std::unique_ptr<BloomFilter> bloom_filter_;
};

}  // namespace kvdb::storage
