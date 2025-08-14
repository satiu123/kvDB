module kvdb.storage.sstable;

import std;

import kvdb.logging.log;
import kvdb.core.binary;
import kvdb.storage.bloom_filter;
import kvdb.core.cache;
import kvdb.core.coro.task;
import kvdb.core.io.io_uring;
import kvdb.core.types;

using kvdb::core::binary::BytesBufferView;
using kvdb::core::coro::Task;
using kvdb::core::io::FileMode;
using kvdb::core::io::IOUring;
using kvdb::core::types::KeyView;
using kvdb::core::types::OrderedKVMap;
using kvdb::core::types::ValueView;
using kvdb::logging::LOG_DEBUG, kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO,
    kvdb::logging::LOG_WARNING;

namespace kvdb::storage {

SSTable::SSTable(IOUring& ring) : ring_(ring), in_file_(ring, "", FileMode::Read) {}

SSTable::Builder::Builder(IOUring& ring, std::string_view path, std::size_t block_size_threshold)
    : ring_(ring),
      out_file_(ring, path, FileMode::Write),
      path_(path),
      block_size_threshold_(block_size_threshold) {
    LOG_DEBUG()("SSTable Builder为'{}'创建", path_);
}

Task<void> SSTable::Builder::add(KeyView key, ValueView value) {
    std::string entry_buffer;
    entry_buffer.resize(sizeof(std::uint32_t) + key.size() + sizeof(std::uint32_t) + value.size());
    BytesBufferView view(std::as_writable_bytes(std::span{entry_buffer}));
    view.write_string(key);
    view.write_string(value);

    current_block_data_.append(entry_buffer);
    last_key_in_block_ = key;

    if (current_block_data_.size() >= block_size_threshold_) {
        LOG_DEBUG()("SSTable块达到阈值，写入磁盘: {}", path_);
        co_await writeBlock();
    }
}

Task<void> SSTable::Builder::writeBlock() {
    if (current_block_data_.empty())
        co_return;

    index_.emplace_back(last_key_in_block_, offset_, current_block_data_.size());

    co_await out_file_.write(std::as_bytes(std::span{current_block_data_}), offset_);
    offset_ += current_block_data_.size();
    current_block_data_.clear();
}

Task<bool> SSTable::Builder::finish(const OrderedKVMap& data) {
    if (!current_block_data_.empty()) {
        co_await writeBlock();
    }
    LOG_DEBUG()("完成SSTable所有数据块的写入: {}", path_);

    std::uint64_t index_block_offset = offset_;
    std::string index_buffer_str;
    for (const auto& record : index_) {
        std::string temp_record_buffer;
        temp_record_buffer.resize(sizeof(std::uint32_t) + record.last_key.size() +
                                  sizeof(std::uint64_t) * 2);
        BytesBufferView temp_view(std::as_writable_bytes(std::span{temp_record_buffer}));
        temp_view.write_string(record.last_key);
        temp_view.write_uint64(record.offset);
        temp_view.write_uint64(record.size);
        index_buffer_str.append(temp_record_buffer);
    }
    co_await out_file_.write(std::as_bytes(std::span{index_buffer_str}), offset_);
    std::uint64_t index_block_size = index_buffer_str.size();
    offset_ += index_block_size;
    LOG_DEBUG()("索引块已写入SSTable: {}", path_);

    BloomFilter bloom_filter(data.size(), 0.01);
    for (const auto& [key, value] : data) {
        bloom_filter.add(key);
    }
    std::uint64_t bloom_filter_offset = offset_;
    std::stringstream bloom_ss;
    bloom_filter.serialize(bloom_ss);
    std::string bloom_buffer = bloom_ss.str();
    co_await out_file_.write(std::as_bytes(std::span{bloom_buffer}), offset_);
    std::uint64_t bloom_filter_size = bloom_buffer.size();
    offset_ += bloom_filter_size;
    LOG_DEBUG()("布隆过滤器已写入SSTable: {}", path_);

    Footer footer;
    footer.index_block_offset = index_block_offset;
    footer.index_block_size = index_block_size;
    footer.bloom_filter_offset = bloom_filter_offset;
    footer.bloom_filter_size = bloom_filter_size;

    co_await out_file_.write(
        std::as_bytes(std::span{std::bit_cast<char*>(&footer), sizeof(footer)}), offset_);
    LOG_DEBUG()("Footer已写入SSTable: {}", path_);

    LOG_INFO()("SSTable '{}' 创建成功完成。", path_);
    co_return true;
}

Task<bool> SSTable::buildFrom(IOUring& ring, std::string_view path, const OrderedKVMap& data) {
    if (data.empty()) {
        LOG_WARNING()("尝试从空数据构建SSTable，已跳过: {}", path);
        co_return false;
    }

    Builder builder(ring, path);
    for (const auto& [key, value] : data) {
        co_await builder.add(key, value);
    }

    co_return co_await builder.finish(data);
}

Task<bool> SSTable::open(std::string_view path) {
    path_ = path;
    in_file_ = File(ring_, path, FileMode::Read);
    LOG_DEBUG()("正在打开SSTable: {}", path);
    if (!co_await loadIndex()) {
        LOG_ERROR()("加载SSTable索引失败: {}", path);
        co_return false;
    }
    if (!co_await loadBloomFilter()) {
        LOG_ERROR()("加载SSTable布隆过滤器失败: {}", path);
        co_return false;
    }
    LOG_INFO()("SSTable '{}' 已成功打开", path);
    co_return true;
}

Task<bool> SSTable::loadBloomFilter() {
    if (footer_.bloom_filter_size == 0) {
        LOG_DEBUG()("SSTable中没有布隆过滤器: {}", path_);
        co_return true;
    }
    std::string bloom_data(footer_.bloom_filter_size, '\0');
    auto bytes_read = co_await in_file_.read(std::as_writable_bytes(std::span{bloom_data}),
                                             footer_.bloom_filter_offset);
    if (static_cast<std::uint64_t>(bytes_read) != footer_.bloom_filter_size) {
        LOG_ERROR()("读取布隆过滤器数据不完整: {}", path_);
        co_return false;
    }

    std::istringstream bloom_stream(bloom_data);
    auto bloom_filter = BloomFilter::deserialize(bloom_stream);
    if (!bloom_filter) {
        LOG_ERROR()("反序列化布隆过滤器失败: {}", path_);
        co_return false;
    }
    bloom_filter_ = std::make_unique<BloomFilter>(std::move(*bloom_filter));
    LOG_DEBUG()("SSTable的布隆过滤器已加载: {}", path_);
    co_return true;
}

Task<bool> SSTable::loadIndex() {
    auto file_size = in_file_.get_size();
    if (file_size < sizeof(Footer)) {
        LOG_ERROR()("SSTable文件太小，无法包含Footer: {}", path_);
        co_return false;
    }
    auto footer_offset = file_size - sizeof(Footer);
    auto bytes_read = co_await in_file_.read(
        std::as_writable_bytes(std::span{std::bit_cast<char*>(&footer_), sizeof(Footer)}),
        footer_offset);

    if (static_cast<std::uint64_t>(bytes_read) != sizeof(Footer)) {
        LOG_ERROR()("读取SSTable Footer不完整: {}", path_);
        co_return false;
    }

    if (footer_.magic != SSTABLE_MAGIC) {
        LOG_ERROR()("SSTable魔数不匹配，文件可能已损坏: {}", path_);
        co_return false;
    }
    std::string index_data(footer_.index_block_size, '\0');
    bytes_read = co_await in_file_.read(std::as_writable_bytes(std::span{index_data}),
                                        footer_.index_block_offset);

    if (static_cast<std::uint64_t>(bytes_read) != footer_.index_block_size) {
        LOG_ERROR()("读取SSTable索引块不完整: {}", path_);
        co_return false;
    }

    BytesBufferView view(std::as_bytes(std::span{index_data}));
    while (view.get_offset() < index_data.size()) {
        auto key_res = view.read_string_view();
        if (!key_res)
            break;
        auto offset_res = view.read_uint64();
        if (!offset_res)
            break;
        auto size_res = view.read_uint64();
        if (!size_res)
            break;

        index_.emplace_back(std::string(*key_res), *offset_res, *size_res);
    }
    LOG_DEBUG()("SSTable的索引已加载: {}", path_);
    co_return true;
}

Task<std::optional<std::string>> SSTable::find(KeyView key) {
    if (bloom_filter_ && !bloom_filter_->contains(key)) {
        LOG_DEBUG()("布隆过滤器未命中，跳过SSTable '{}' 的搜索，键: {}", path_, key);
        co_return std::nullopt;
    }

    auto it = std::lower_bound(
        index_.begin(), index_.end(), key,
        [](const auto& record, std::string_view k) { return record.last_key < k; });

    if (it == index_.end()) {
        co_return std::nullopt;
    }

    auto cached_block = kvdb::core::g_block_cache.get(path_, it->offset);
    kvdb::core::ParsedBlock block_map_ptr;

    if (cached_block) {
        LOG_DEBUG()("块缓存命中: SSTable='{}', offset={}", path_, it->offset);
        block_map_ptr = *cached_block;
    } else {
        LOG_DEBUG()("块缓存未命中，从磁盘读取: SSTable='{}', offset={}", path_, it->offset);
        std::string block_data(it->size, '\0');
        auto bytes_read =
            co_await in_file_.read(std::as_writable_bytes(std::span{block_data}), it->offset);
        if (static_cast<std::uint64_t>(bytes_read) != it->size) {
            LOG_ERROR()("读取数据块失败: SSTable='{}', offset={}", path_, it->offset);
            co_return std::nullopt;
        }

        auto new_block_map = std::make_shared<OrderedKVMap>();
        BytesBufferView view(std::as_bytes(std::span{block_data}));
        while (view.get_offset() < block_data.size()) {
            auto key_res = view.read_string_view();
            if (!key_res)
                break;
            auto value_res = view.read_string_view();
            if (!value_res)
                break;
            (*new_block_map)[std::string(*key_res)] = std::string(*value_res);
        }

        kvdb::core::g_block_cache.put(path_, it->offset, new_block_map);
        block_map_ptr = new_block_map;
    }

    auto map_it = block_map_ptr->find(std::string(key));
    if (map_it != block_map_ptr->end()) {
        co_return map_it->second;
    }

    co_return std::nullopt;
}

Task<std::map<std::string, std::string>> SSTable::readAll() {
    std::map<std::string, std::string> data;
    for (const auto& index_record : index_) {
        std::string block_data(index_record.size, '\0');
        auto bytes_read = co_await in_file_.read(std::as_writable_bytes(std::span{block_data}),
                                                 index_record.offset);

        if (static_cast<std::uint64_t>(bytes_read) != index_record.size) {
            LOG_ERROR()("读取SSTable '{}' 的数据块时出错", path_);
            continue;  // 跳过这个块
        }

        BytesBufferView view(std::as_bytes(std::span{block_data}));
        while (view.get_offset() < block_data.size()) {
            auto key_res = view.read_string_view();
            if (!key_res) {
                break;
            }
            auto value_res = view.read_string_view();
            if (!value_res) {
                break;
            }
            data[std::string(*key_res)] = std::string(*value_res);
        }
    }
    co_return data;
}
}  // namespace kvdb::storage
