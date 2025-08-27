export module kvdb.core.cache;

import std;
import kvdb.core.types;

export namespace kvdb::core {

using kvdb::core::types::OrderedKVMap;
// 定义缓存值的类型：一个指向常量 map 的共享指针
using ParsedBlock = std::shared_ptr<const OrderedKVMap>;

// 为 std::pair 提供自定义哈希函数，使其能用于 unordered_map
struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        // 简单地组合两个哈希值
        return h1 ^ (h2 << 1);
    }
};

class BlockCache {
  public:
    explicit BlockCache(std::size_t max_size) : max_size_(max_size) {}

    // 尝试从缓存中获取一个已解析的块
    auto get(const std::string& file_path, std::uint64_t offset) -> std::optional<ParsedBlock> {
        std::lock_guard lock(mutex_);

        auto it = cache_map_.find({file_path, offset});
        if (it == cache_map_.end()) {
            return std::nullopt;  // 缓存未命中
        }

        // 缓存命中，将被访问的元素移动到LRU列表的前端
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.second);
        return it->second.first;
    }

    // 将一个已解析的块放入缓存
    void put(const std::string& file_path, std::uint64_t offset, ParsedBlock block) {
        std::lock_guard lock(mutex_);

        // 如果缓存已满，则淘汰最末尾的元素
        if (cache_map_.size() >= max_size_) {
            auto last = lru_list_.back();
            cache_map_.erase(last);
            lru_list_.pop_back();
        }

        // 将新元素插入到LRU列表的前端，并在map中记录其位置
        lru_list_.emplace_front(file_path, offset);
        cache_map_[{file_path, offset}] = {std::move(block), lru_list_.begin()};
    }

  private:
    using CacheKey = std::pair<std::string, std::uint64_t>;
    using CacheIterator = std::list<CacheKey>::iterator;

    std::size_t max_size_;
    std::list<CacheKey> lru_list_;
    std::unordered_map<CacheKey, std::pair<ParsedBlock, CacheIterator>, PairHash> cache_map_;
    std::mutex mutex_;
};

// 创建一个全局的块缓存实例，容量为 256 个块 (每个块约4KB，总计约1MB)
inline BlockCache g_block_cache(256);

}  // namespace kvdb::core
