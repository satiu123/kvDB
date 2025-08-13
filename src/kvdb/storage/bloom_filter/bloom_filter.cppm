export module kvdb.storage.bloom_filter;

import std;
import kvdb.core.types;

export namespace kvdb::storage {
using kvdb::core::types::KeyView;

class BloomFilter {
  public:
    // 根据预期元素数量和期望的假阳性率构造
    BloomFilter(std::uint64_t num_items, double false_positive_rate);

    // 从序列化数据中构造
    BloomFilter(std::vector<bool> bit_set, std::uint32_t num_hash_functions);

    void add(KeyView key);
    bool contains(KeyView key) const;

    const std::vector<bool>& getBitSet() const;
    std::uint32_t getNumHashFunctions() const;
    std::size_t getBitSetSizeInBytes() const;

    // 序列化布隆过滤器
    void serialize(std::ostream& os) const;
    // 反序列化布隆过滤器
    static auto deserialize(std::istream& is) -> std::optional<BloomFilter>;

  private:
    auto hash(KeyView key) const -> std::array<std::uint64_t, 2>;

    std::uint32_t num_hash_functions_;
    std::vector<bool> bit_set_;
};

}  // namespace kvdb::storage
