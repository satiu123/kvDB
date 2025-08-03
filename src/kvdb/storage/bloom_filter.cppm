export module kvdb.storage.bloom_filter;

import std;

export namespace kvdb::storage {

class BloomFilter {
  public:
    // 根据预期元素数量和期望的假阳性率构造
    BloomFilter(std::uint64_t num_items, double false_positive_rate);

    // 从序列化数据中构造
    BloomFilter(std::vector<bool> bit_set, std::uint32_t num_hash_functions);

    void add(std::string_view key);
    bool contains(std::string_view key) const;

    const std::vector<bool>& getBitSet() const;
    std::uint32_t getNumHashFunctions() const;
    std::size_t getBitSetSizeInBytes() const;

    // 序列化布隆过滤器
    void serialize(std::ostream& os) const;
    // 反序列化布隆过滤器
    static std::optional<BloomFilter> deserialize(std::istream& is);

  private:
    std::array<std::uint64_t, 2> hash(std::string_view key) const;

    std::uint32_t num_hash_functions_;
    std::vector<bool> bit_set_;
};

}  // namespace kvdb::storage
