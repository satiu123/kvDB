module kvdb.storage.sstable;

import std;
import kvdb.logging.log;

using kvdb::logging::LOG_INFO, kvdb::logging::LOG_ERROR;
namespace kvdb::storage {
// Helper function to write a string with its length
static bool writeString(std::ofstream& file, std::string_view str) {
    std::uint32_t len = str.length();
    file.write(reinterpret_cast<const char*>(&len), sizeof(len));
    file.write(str.data(), len);
    return file.good();
}

// Helper function to read a string with its length
static bool readString(std::ifstream& file, std::string& str) {
    std::uint32_t len;
    file.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!file)
        return false;
    str.resize(len);
    file.read(&str[0], len);
    return file.good();
}

SSTable::Builder::Builder(std::string_view path, std::size_t block_size_threshold)
    : path_(path), block_size_threshold_(block_size_threshold) {
    file_.open(path_, std::ios::binary);
}

void SSTable::Builder::add(std::string_view key, std::string_view value) {
    if (!file_.is_open())
        return;

    // Simple serialization for key and value
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

    // Write the last remaining block if any
    if (!current_block_data_.empty()) {
        writeBlock();
    }

    // Write the index block
    std::uint64_t index_block_offset = offset_;
    for (const auto& [key, block_offset] : index_) {
        writeString(file_, key);
        file_.write(reinterpret_cast<const char*>(&block_offset), sizeof(block_offset));
    }
    std::uint64_t index_block_size = static_cast<std::uint64_t>(file_.tellp()) - index_block_offset;

    // Write the footer
    Footer footer;
    footer.index_block_offset = index_block_offset;
    footer.index_block_size = index_block_size;
    file_.write(reinterpret_cast<const char*>(&footer), sizeof(footer));

    file_.close();
    LOG_INFO()("SSTable '{}' finished successfully.", path_);
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
    file_.seekg(-static_cast<std::streamoff>(sizeof(Footer)), std::ios::end);
    file_.read(reinterpret_cast<char*>(&footer_), sizeof(Footer));

    if (footer_.magic != SSTABLE_MAGIC) {
        return false;
    }

    file_.seekg(footer_.index_block_offset);
    std::string index_data(footer_.index_block_size, '\0');
    file_.read(&index_data[0], footer_.index_block_size);

    std::istringstream index_stream(index_data);
    while (index_stream.peek() != std::ios::traits_type::eof()) {
        std::string key;
        std::uint64_t offset;
        std::uint32_t key_len;
        index_stream.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        key.resize(key_len);
        index_stream.read(&key[0], key_len);
        index_stream.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        index_.emplace_back(key, offset);
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
    // This is a simplification. A real implementation would read the whole block.
    std::string file_key, file_value;
    while (readString(file_, file_key) && readString(file_, file_value)) {
        if (file_key == key) {
            return file_value;
        }
        if (file_.tellg() >= it->second + 4096) {  // block size
            break;
        }
    }

    return std::nullopt;
}

std::map<std::string, std::string> SSTable::readAll() {
    std::map<std::string, std::string> data;
    file_.clear();  // Reset stream state (e.g., EOF from previous reads)
    file_.seekg(0);
    std::string key, value;
    while (static_cast<std::uint64_t>(file_.tellg()) < footer_.index_block_offset &&
           readString(file_, key) && readString(file_, value)) {
        data[key] = value;
    }
    return data;
}
}  // namespace kvdb::storage