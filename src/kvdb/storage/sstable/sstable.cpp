module kvdb.storage.sstable;

import std;
import kvdb.logging.log;

import kvdb.core.binary;

using kvdb::logging::LOG_INFO, kvdb::logging::LOG_ERROR;
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

    index_.emplace_back(last_key_in_block_, offset_);

    file_.write(current_block_data_.data(), current_block_data_.size());
    offset_ += current_block_data_.size();
    current_block_data_.clear();
}

bool SSTable::Builder::finish() {
    if (!file_.is_open())
        return false;

    // 如果有剩余数据，则写入最后一个块
    if (!current_block_data_.empty()) {
        writeBlock();
    }

    // 写入索引块
    std::uint64_t index_block_offset = offset_;
    for (const auto& [key, block_offset] : index_) {
        kvdb::core::binary::write_string(file_, key);
        kvdb::core::binary::write_uint64(file_, block_offset);
    }
    std::uint64_t index_block_size = static_cast<std::uint64_t>(file_.tellp()) - index_block_offset;

    // 写入尾注
    Footer footer;
    footer.index_block_offset = index_block_offset;
    footer.index_block_size = index_block_size;
    file_.write(reinterpret_cast<const char*>(&footer), sizeof(footer));

    file_.close();
    LOG_INFO()("SSTable '{}' 成功完成。", path_);
    return true;
}

bool SSTable::buildFrom(std::string_view path, const std::map<std::string, std::string>& data) {
    if (data.empty()) {
        return false;
    }

    Builder builder(path);
    for (const auto& [key, value] : data) {
        builder.add(key, value);
    }

    return builder.finish();
}

bool SSTable::open(std::string_view path) {
    path_ = path;
    file_.open(path_, std::ios::binary);
    if (!file_.is_open()) {
        return false;
    }
    return loadIndex();
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
    file_.read(&index_data[0], footer_.index_block_size);
    // 解析索引数据
    std::istringstream index_stream(index_data);
    while (index_stream.peek() != std::ios::traits_type::eof()) {
        auto key_res = kvdb::core::binary::read_string(index_stream);
        if (!key_res)
            break;
        auto offset_res = kvdb::core::binary::read_uint64(index_stream);
        if (!offset_res)
            break;

        index_.emplace_back(*key_res, *offset_res);
    }
    return true;
}

std::optional<std::string> SSTable::find(std::string_view key) {
    auto it = std::lower_bound(index_.begin(), index_.end(), key,
                               [](const auto& a, std::string_view b) { return a.first < b; });
    if (it == index_.end()) {
        return std::nullopt;
    }

    file_.seekg(it->second);
    // 这是一个简化实现。真正的实现会读取整个块。
    std::string file_key, file_value;
    while (true) {
        auto key_res = kvdb::core::binary::read_string(file_);
        if (!key_res)
            break;
        auto value_res = kvdb::core::binary::read_string(file_);
        if (!value_res)
            break;

        if (*key_res == key) {
            return *value_res;
        }
        if (static_cast<std::uint64_t>(file_.tellg()) >= it->second + 4096) {  // 块大小
            break;
        }
    }

    return std::nullopt;
}

std::map<std::string, std::string> SSTable::readAll() {
    std::map<std::string, std::string> data;
    file_.clear();  // 重置流状态（例如，来自先前读取的EOF）
    file_.seekg(0);
    std::string key, value;
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