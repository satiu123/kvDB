module kvdb.storage.sstable;

import std;
import kvdb.logging.log;

import kvdb.core.binary;
import kvdb.storage.bloom_filter;
import kvdb.core.cache;

using kvdb::logging::LOG_INFO;
namespace kvdb::storage {

SSTable::Builder::Builder(std::string_view path, std::size_t block_size_threshold)
    : path_(path), block_size_threshold_(block_size_threshold) {
    file_.open(path_, std::ios::binary);
}

void SSTable::Builder::add(std::string_view key, std::string_view value) {
    if (!file_.is_open())
        return;

    // 简单的键值序列化
    std::uint32_t key_len = key.length();
    std::uint32_t val_len = value.length();

    current_block_data_.append(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    current_block_data_.append(key.data(), key.length());
    current_block_data_.append(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
    current_block_data_.append(value.data(), value.length());

    last_key_in_block_ = key;

    if (current_block_data_.size() >= block_size_threshold_) {
        writeBlock();
    }
}

void SSTable::Builder::writeBlock() {
    if (current_block_data_.empty())
        return;

    // 记录索引，包含块大小
    index_.emplace_back(last_key_in_block_, offset_, current_block_data_.size());

    file_.write(current_block_data_.data(), current_block_data_.size());
    offset_ += current_block_data_.size();
    current_block_data_.clear();
}

bool SSTable::Builder::finish(const std::map<std::string, std::string, std::less<>>& data) {
    if (!file_.is_open())
        return false;

    // 如果有剩余数据，则写入最后一个块
    if (!current_block_data_.empty()) {
        writeBlock();
    }

    // 写入索引块
    std::uint64_t index_block_offset = offset_;
    for (const auto& record : index_) {
        kvdb::core::binary::write_string(file_, record.last_key);
        kvdb::core::binary::write_uint64(file_, record.offset);
        kvdb::core::binary::write_uint64(file_, record.size);
    }
    std::uint64_t index_block_size = static_cast<std::uint64_t>(file_.tellp()) - index_block_offset;

    // 创建并写入布隆过滤器
    BloomFilter bloom_filter(data.size(), 0.01);
    for (const auto& [key, value] : data) {
        bloom_filter.add(key);
    }
    std::uint64_t bloom_filter_offset = file_.tellp();
    bloom_filter.serialize(file_);
    std::uint64_t bloom_filter_size =
        static_cast<std::uint64_t>(file_.tellp()) - bloom_filter_offset;

    // 写入尾注
    Footer footer;
    footer.index_block_offset = index_block_offset;
    footer.index_block_size = index_block_size;
    footer.bloom_filter_offset = bloom_filter_offset;
    footer.bloom_filter_size = bloom_filter_size;
    file_.write(reinterpret_cast<const char*>(&footer), sizeof(footer));

    file_.close();
    LOG_INFO()("SSTable '{}' 成功完成。", path_);
    return true;
}

bool SSTable::buildFrom(std::string_view path,
                        const std::map<std::string, std::string, std::less<>>& data) {
    if (data.empty()) {
        return false;
    }

    Builder builder(path);
    for (const auto& [key, value] : data) {
        builder.add(key, value);
    }

    return builder.finish(data);
}

bool SSTable::open(std::string_view path) {
    path_ = path;
    file_.open(path_, std::ios::binary);
    if (!file_.is_open()) {
        return false;
    }
    if (!loadIndex()) {
        return false;
    }
    return loadBloomFilter();
}

bool SSTable::loadBloomFilter() {
    if (footer_.bloom_filter_size == 0) {  // Handle case where there's no bloom filter
        return true;
    }
    file_.seekg(footer_.bloom_filter_offset);
    auto bloom_filter = BloomFilter::deserialize(file_);
    if (!bloom_filter) {
        return false;
    }
    bloom_filter_ = std::make_unique<BloomFilter>(std::move(*bloom_filter));
    return true;
}

bool SSTable::loadIndex() {
    // 读取尾注
    file_.seekg(-static_cast<std::streamoff>(sizeof(Footer)), std::ios::end);
    file_.read(reinterpret_cast<char*>(&footer_), sizeof(Footer));

    if (footer_.magic != SSTABLE_MAGIC) {
        return false;
    }
    // 读取索引块
    file_.seekg(footer_.index_block_offset);
    std::string index_data(footer_.index_block_size, '\0');
    file_.read(index_data.data(), footer_.index_block_size);

    // 解析索引数据
    std::istringstream index_stream(index_data);
    while (index_stream.peek() != std::ios::traits_type::eof()) {
        auto key_res = kvdb::core::binary::read_string(index_stream);
        if (!key_res)
            break;
        auto offset_res = kvdb::core::binary::read_uint64(index_stream);
        if (!offset_res)
            break;
        auto size_res = kvdb::core::binary::read_uint64(index_stream);
        if (!size_res)
            break;

        index_.emplace_back(*key_res, *offset_res, *size_res);
    }
    return true;
}

std::optional<std::string> SSTable::find(std::string_view key) {
    if (bloom_filter_ && !bloom_filter_->contains(key)) {
        return std::nullopt;
    }

    auto it = std::lower_bound(
        index_.begin(), index_.end(), key,
        [](const auto& record, std::string_view k) { return record.last_key < k; });

    if (it == index_.end()) {
        return std::nullopt;
    }

    // 1. 尝试从缓存获取已解析的块
    auto cached_block = kvdb::core::g_block_cache.get(path_, it->offset);
    kvdb::core::ParsedBlock block_map_ptr;

    if (cached_block) {
        // 缓存命中，直接使用已解析的map
        block_map_ptr = *cached_block;
    } else {
        // 缓存未命中
        // 2. 从磁盘读取原始数据块
        std::string block_data(it->size, '\0');
        file_.seekg(it->offset);
        file_.read(block_data.data(), it->size);

        // 3. 解析块并创建一个新的map
        auto new_block_map = std::make_shared<std::map<std::string, std::string, std::less<>>>();
        std::istringstream block_stream(block_data);
        while (block_stream.peek() != std::ios::traits_type::eof()) {
            auto key_res = kvdb::core::binary::read_string(block_stream);
            if (!key_res)
                break;
            auto value_res = kvdb::core::binary::read_string(block_stream);
            if (!value_res)
                break;
            (*new_block_map)[std::move(*key_res)] = std::move(*value_res);
        }

        // 4. 将新解析的块放入缓存
        kvdb::core::g_block_cache.put(path_, it->offset, new_block_map);
        block_map_ptr = new_block_map;
    }

    // 5. 在已解析的map中进行最终查找
    auto map_it = block_map_ptr->find(key);
    if (map_it != block_map_ptr->end()) {
        return map_it->second;
    }

    return std::nullopt;
}

std::map<std::string, std::string> SSTable::readAll() {
    std::map<std::string, std::string> data;
    file_.clear();  // 重置流状态（例如，来自先前读取的EOF）
    file_.seekg(0);
    std::string key;
    std::string value;
    while (static_cast<std::uint64_t>(file_.tellg()) < footer_.index_block_offset) {
        auto key_res = kvdb::core::binary::read_string(file_);
        if (!key_res)
            break;
        auto value_res = kvdb::core::binary::read_string(file_);
        if (!value_res)
            break;
        data.emplace(std::move(*key_res), std::move(*value_res));
    }
    return data;
}
}  // namespace kvdb::storage