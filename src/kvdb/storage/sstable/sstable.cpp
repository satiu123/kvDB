module kvdb.storage.sstable;

import std;

import kvdb.logging.log;
import kvdb.core.binary;
import kvdb.storage.bloom_filter;
import kvdb.core.cache;
import kvdb.core.coro.task;
import kvdb.core.io.io_uring;

using kvdb::core::binary::BytesBufferView;
using kvdb::core::coro::Task;
using kvdb::core::io::FileMode;
using kvdb::core::io::IOUring;
using kvdb::logging::LOG_INFO;
namespace kvdb::storage {

SSTable::SSTable(IOUring& ring) : ring_(ring), in_file_(ring, "", FileMode::Read) {}

SSTable::Builder::Builder(IOUring& ring, std::string_view path, std::size_t block_size_threshold)
    : ring_(ring),
      out_file_(ring, path, FileMode::Write),
      path_(path),
      block_size_threshold_(block_size_threshold) {}

Task<void> SSTable::Builder::add(std::string_view key, std::string_view value) {
    std::string entry_buffer;
    entry_buffer.resize(sizeof(std::uint32_t) + key.size() + sizeof(std::uint32_t) + value.size());
    BytesBufferView view(std::as_writable_bytes(std::span{entry_buffer}));
    view.write_string(key);
    view.write_string(value);

    current_block_data_.append(entry_buffer);
    last_key_in_block_ = key;

    if (current_block_data_.size() >= block_size_threshold_) {
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

Task<bool> SSTable::Builder::finish(const std::map<std::string, std::string, std::less<>>& data) {
    if (!current_block_data_.empty()) {
        co_await writeBlock();
    }

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

    Footer footer;
    footer.index_block_offset = index_block_offset;
    footer.index_block_size = index_block_size;
    footer.bloom_filter_offset = bloom_filter_offset;
    footer.bloom_filter_size = bloom_filter_size;

    co_await out_file_.write(
        std::as_bytes(std::span{reinterpret_cast<char*>(&footer), sizeof(footer)}), offset_);

    LOG_INFO()("SSTable '{}' 创建成功完成。", path_);
    co_return true;
}

Task<bool> SSTable::buildFrom(IOUring& ring, std::string_view path,
                              const std::map<std::string, std::string, std::less<>>& data) {
    if (data.empty()) {
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
    if (!co_await loadIndex()) {
        co_return false;
    }
    co_return co_await loadBloomFilter();
}

Task<bool> SSTable::loadBloomFilter() {
    if (footer_.bloom_filter_size == 0) {
        co_return true;
    }
    std::string bloom_data(footer_.bloom_filter_size, '\0');
    auto bytes_read = co_await in_file_.read(std::as_writable_bytes(std::span{bloom_data}),
                                             footer_.bloom_filter_offset);
    if (static_cast<std::uint64_t>(bytes_read) != footer_.bloom_filter_size) {
        co_return false;
    }

    std::istringstream bloom_stream(bloom_data);
    auto bloom_filter = BloomFilter::deserialize(bloom_stream);
    if (!bloom_filter) {
        co_return false;
    }
    bloom_filter_ = std::make_unique<BloomFilter>(std::move(*bloom_filter));
    co_return true;
}

Task<bool> SSTable::loadIndex() {
    auto file_size = in_file_.get_size();
    if (file_size < sizeof(Footer)) {
        co_return false;
    }
    auto footer_offset = file_size - sizeof(Footer);
    auto bytes_read = co_await in_file_.read(
        std::as_writable_bytes(std::span{reinterpret_cast<char*>(&footer_), sizeof(Footer)}),
        footer_offset);

    if (static_cast<std::uint64_t>(bytes_read) != sizeof(Footer)) {
        co_return false;
    }

    if (footer_.magic != SSTABLE_MAGIC) {
        co_return false;
    }
    std::string index_data(footer_.index_block_size, '\0');
    bytes_read = co_await in_file_.read(std::as_writable_bytes(std::span{index_data}),
                                        footer_.index_block_offset);

    if (static_cast<std::uint64_t>(bytes_read) != footer_.index_block_size) {
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
    co_return true;
}

Task<std::optional<std::string>> SSTable::find(std::string_view key) {
    if (bloom_filter_ && !bloom_filter_->contains(key)) {
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
        block_map_ptr = *cached_block;
    } else {
        std::string block_data(it->size, '\0');
        auto bytes_read =
            co_await in_file_.read(std::as_writable_bytes(std::span{block_data}), it->offset);
        if (static_cast<std::uint64_t>(bytes_read) != it->size) {
            co_return std::nullopt;
        }

        auto new_block_map = std::make_shared<std::map<std::string, std::string, std::less<>>>();
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
            // Handle error: maybe log and continue, or throw an exception
            LOG_INFO()("Error reading block for SSTable {}", path_);
            continue;  // Skip this block
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