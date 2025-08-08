module kvdb.storage.sstable;

import std;
import kvdb.logging.log;

import kvdb.core.binary;
import kvdb.storage.bloom_filter;
import kvdb.core.cache;

using kvdb::logging::LOG_INFO;
using kvdb::core::binary::BytesBuffer;
using kvdb::core::binary::BytesBufferView;

namespace kvdb::storage {

SSTable::Builder::Builder(std::string_view path, std::size_t block_size_threshold)
    : path_(path), block_size_threshold_(block_size_threshold) {
    file_.open(path_, std::ios::binary);
}

void SSTable::Builder::add(std::string_view key, std::string_view value) {
    if (!file_.is_open()) return;

    BytesBuffer temp_buf;
    temp_buf.push_string(key);
    temp_buf.push_string(value);
    auto data_to_write = temp_buf.get_span();

    current_block_data_.insert(current_block_data_.end(), data_to_write.begin(), data_to_write.end());
    last_key_in_block_ = key;

    if (current_block_data_.size() >= block_size_threshold_) {
        writeBlock();
    }
}

void SSTable::Builder::writeBlock() {
    if (current_block_data_.empty()) return;

    index_.emplace_back(last_key_in_block_, offset_, current_block_data_.size());
    file_.write(reinterpret_cast<const char*>(current_block_data_.data()), current_block_data_.size());
    offset_ += current_block_data_.size();
    current_block_data_.clear();
}

bool SSTable::Builder::finish(const std::map<std::string, std::string, std::less<>>& data) {
    if (!file_.is_open()) return false;

    if (!current_block_data_.empty()) {
        writeBlock();
    }

    std::uint64_t index_block_offset = offset_;
    BytesBuffer index_buffer;
    for (const auto& record : index_) {
        index_buffer.push_string(record.last_key);
        index_buffer.push(reinterpret_cast<const std::byte*>(&record.offset), sizeof(record.offset));
        index_buffer.push(reinterpret_cast<const std::byte*>(&record.size), sizeof(record.size));
    }
    auto index_data = index_buffer.get_span();
    file_.write(reinterpret_cast<const char*>(index_data.data()), index_data.size());
    std::uint64_t index_block_size = index_data.size();

    BloomFilter bloom_filter(data.size(), 0.01);
    for (const auto& [key, value] : data) {
        bloom_filter.add(key);
    }
    std::uint64_t bloom_filter_offset = static_cast<std::uint64_t>(file_.tellp());
    bloom_filter.serialize(file_);
    std::uint64_t bloom_filter_size = static_cast<std::uint64_t>(file_.tellp()) - bloom_filter_offset;

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
    if (data.empty()) return false;
    Builder builder(path);
    for (const auto& [key, value] : data) {
        builder.add(key, value);
    }
    return builder.finish(data);
}

bool SSTable::open(std::string_view path) {
    path_ = path;
    file_.open(path_, std::ios::binary);
    if (!file_.is_open()) return false;
    if (!loadIndex()) return false;
    return loadBloomFilter();
}

bool SSTable::loadBloomFilter() {
    if (footer_.bloom_filter_size == 0) return true;
    file_.seekg(footer_.bloom_filter_offset);
    auto bloom_filter = BloomFilter::deserialize(file_);
    if (!bloom_filter) return false;
    bloom_filter_ = std::make_unique<BloomFilter>(std::move(*bloom_filter));
    return true;
}

bool SSTable::loadIndex() {
    file_.seekg(-static_cast<std::streamoff>(sizeof(Footer)), std::ios::end);
    file_.read(reinterpret_cast<char*>(&footer_), sizeof(Footer));

    if (footer_.magic != SSTABLE_MAGIC) return false;

    std::vector<std::byte> index_data(footer_.index_block_size);
    file_.seekg(footer_.index_block_offset);
    file_.read(reinterpret_cast<char*>(index_data.data()), footer_.index_block_size);

    BytesBufferView index_view(index_data);
    while (index_view.get_offset() < index_data.size()) {
        auto key_res = index_view.read_string_view();
        if (!key_res) break;
        auto offset_res = index_view.read_uint64();
        if (!offset_res) break;
        auto size_res = index_view.read_uint64();
        if (!size_res) break;
        index_.emplace_back(std::string(*key_res), *offset_res, *size_res);
    }
    return true;
}

auto SSTable::find(std::string_view key) -> std::optional<std::string> {
    if (bloom_filter_ && !bloom_filter_->contains(key)) {
        return std::nullopt;
    }

    auto it = std::lower_bound(index_.begin(), index_.end(), key,
        [](const auto& record, std::string_view k) { return record.last_key < k; });

    if (it == index_.end()) return std::nullopt;

    auto cached_block = kvdb::core::g_block_cache.get(path_, it->offset);
    kvdb::core::ParsedBlock block_map_ptr;

    if (cached_block) {
        block_map_ptr = *cached_block;
    } else {
        std::vector<std::byte> block_data(it->size);
        file_.seekg(it->offset);
        file_.read(reinterpret_cast<char*>(block_data.data()), it->size);

        auto new_block_map = std::make_shared<std::map<std::string, std::string, std::less<>>>();
        BytesBufferView block_view(block_data);
        while (block_view.get_offset() < block_data.size()) {
            auto key_res = block_view.read_string_view();
            if (!key_res) break;
            auto value_res = block_view.read_string_view();
            if (!value_res) break;
            (*new_block_map)[std::string(*key_res)] = std::string(*value_res);
        }

        kvdb::core::g_block_cache.put(path_, it->offset, new_block_map);
        block_map_ptr = new_block_map;
    }

    auto map_it = block_map_ptr->find(std::string(key));
    if (map_it != block_map_ptr->end()) {
        return map_it->second;
    }

    return std::nullopt;
}

auto SSTable::readAll() -> std::map<std::string, std::string> {
    std::map<std::string, std::string> data;
    file_.clear();
    file_.seekg(0);
    
    std::vector<std::byte> all_data(footer_.index_block_offset);
    file_.read(reinterpret_cast<char*>(all_data.data()), all_data.size());
    BytesBufferView data_view(all_data);

    while(data_view.get_offset() < all_data.size()) {
        auto key_res = data_view.read_string_view();
        if (!key_res) break;
        auto value_res = data_view.read_string_view();
        if (!value_res) break;
        data.emplace(std::string(*key_res), std::string(*value_res));
    }
    return data;
}

}  // namespace kvdb::storage
