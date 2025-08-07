module kvdb.core.database.manifest;

import std;
import kvdb.core.binary;
import kvdb.core.coro.task;
import kvdb.core.io.file;
import kvdb.core.io.io_uring;
import kvdb.logging.log;

using kvdb::core::coro::Task;
using kvdb::core::io::File;
using kvdb::core::io::FileMode;
using kvdb::core::io::IOUring;
using kvdb::logging::LOG_ERROR;

namespace kvdb::core::database {

// --- Manifest 实现 ---

auto Manifest::serialize(std::ostream& os) const -> std::expected<void, std::string> {
    std::stringstream buffer;

    if (auto res = binary::write_uint64(buffer, last_wal_sequence_number); !res)
        return std::unexpected(res.error());
    if (auto res = binary::write_uint32(buffer, static_cast<std::uint32_t>(sstables.size())); !res)
        return std::unexpected(res.error());

    for (const auto& [level, files] : sstables) {
        if (auto res = binary::write_uint32(buffer, static_cast<std::uint32_t>(level)); !res)
            return std::unexpected(res.error());
        if (auto res = binary::write_uint32(buffer, static_cast<std::uint32_t>(files.size())); !res)
            return std::unexpected(res.error());
        for (const auto& file : files) {
            if (auto res = binary::write_string(buffer, file); !res)
                return std::unexpected(res.error());
        }
    }

    std::string content = buffer.str();
    std::vector<std::uint8_t> data_vec(content.begin(), content.end());
    std::uint32_t crc = binary::calculate_crc32(data_vec);
    if (auto res = binary::write_uint32(os, crc); !res)
        return std::unexpected(res.error());

    os.write(content.c_str(), content.size());
    if (!os)
        return std::unexpected("Failed to write manifest content to output stream");

    return {};
}

auto Manifest::deserialize(std::istream& is) -> std::expected<void, std::string>{
    auto crc_res = binary::read_uint32(is);
    if (!crc_res)
        return std::unexpected(crc_res.error());
    std::uint32_t stored_crc = *crc_res;

    std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    if (is.bad()) {
        return std::unexpected("Failed to read manifest content");
    }

    std::vector<std::uint8_t> data_vec(content.begin(), content.end());
    std::uint32_t calculated_crc = binary::calculate_crc32(data_vec);

    if (stored_crc != calculated_crc) {
        return std::unexpected("Manifest checksum mismatch. The file may be corrupted.");
    }

    std::stringstream buffer(content);
    auto seq_num_res = binary::read_uint64(buffer);
    if (!seq_num_res)
        return std::unexpected(seq_num_res.error());
    last_wal_sequence_number = *seq_num_res;

    auto levels_res = binary::read_uint32(buffer);
    if (!levels_res)
        return std::unexpected(levels_res.error());
    std::uint32_t sstable_levels = *levels_res;

    sstables.clear();
    for (std::uint32_t i = 0; i < sstable_levels; ++i) {
        auto level_res = binary::read_uint32(buffer);
        if (!level_res)
            return std::unexpected(level_res.error());
        std::uint32_t level = *level_res;

        auto num_files_res = binary::read_uint32(buffer);
        if (!num_files_res)
            return std::unexpected(num_files_res.error());
        std::uint32_t num_files = *num_files_res;

        std::vector<std::string> files;
        files.reserve(num_files);
        for (std::uint32_t j = 0; j < num_files; ++j) {
            auto file_res = binary::read_string(buffer);
            if (!file_res)
                return std::unexpected(file_res.error());
            files.push_back(*file_res);
        }
        sstables[static_cast<int>(level)] = std::move(files);
    }
    return {};
}

// --- ManifestFile 实现 ---

ManifestFile::ManifestFile(std::string_view path)
    : path_(path), current_path_(std::string(path) + "/CURRENT") {}

ManifestFile::ManifestFile(IOUring& ring, std::string_view path)
    : ring_(&ring), path_(path), current_path_(std::string(path) + "/CURRENT") {}

auto ManifestFile::load() -> std::expected<Manifest, std::string> {
    std::ifstream current_file(current_path_);
    if (!current_file.is_open()) {
        return Manifest{};  // 首次启动，没有CURRENT文件，是正常情况
    }

    std::string manifest_filename;
    current_file >> manifest_filename;
    current_file.close();

    if (manifest_filename.empty()) {
        return Manifest{};  // CURRENT文件为空，也算正常
    }

    std::ifstream manifest_file(std::string(path_) + "/" + manifest_filename, std::ios::binary);
    if (!manifest_file.is_open()) {
        return std::unexpected("Failed to open MANIFEST file: " + manifest_filename);
    }

    Manifest manifest;
    if (auto res = manifest.deserialize(manifest_file); !res) {
        LOG_ERROR()("Failed to load and parse MANIFEST file '{}': {}", manifest_filename,
                    res.error());
        return std::unexpected("Failed to parse MANIFEST file: " + res.error());
    }
    return manifest;
}

auto ManifestFile::store(const Manifest& manifest) -> std::expected<void, std::string> {
    std::string new_manifest_filename = get_new_manifest_filename();
    std::string new_manifest_path = std::string(path_) + "/" + new_manifest_filename;
    std::string temp_path = new_manifest_path + ".tmp";

    // 1. 写入到临时文件
    std::ofstream temp_file(temp_path, std::ios::binary);
    if (!temp_file.is_open()) {
        return std::unexpected("Failed to open temporary MANIFEST file for writing.");
    }
    if (auto res = manifest.serialize(temp_file); !res) {
        temp_file.close();
        return std::unexpected("Failed to serialize manifest: " + res.error());
    }
    temp_file.close();

    // 2. 原子性地重命名临时文件
    if (std::rename(temp_path.c_str(), new_manifest_path.c_str()) != 0) {
        return std::unexpected("Failed to rename temporary MANIFEST file.");
    }

    // 3. 更新CURRENT文件指向新的Manifest文件
    std::ofstream current_file(current_path_, std::ios::trunc);
    if (!current_file.is_open()) {
        // 这是一个严重问题，可能导致数据库状态不一致
        // 此时新Manifest已生成，但CURRENT未更新
        return std::unexpected("CRITICAL: Failed to open CURRENT file for writing.");
    }
    current_file << new_manifest_filename;
    current_file.close();

    return {};
}

auto ManifestFile::async_load() -> kvdb::core::coro::Task<std::expected<Manifest, std::string>> {
    if (!ring_) {
        co_return std::unexpected("IOUring not initialized for async_load");
    }
    if (!std::filesystem::exists(current_path_)) {
        co_return Manifest{};
    }
    // 1. 异步读取CURRENT文件
    std::vector<std::byte> current_buffer(128);
    File current_file(*ring_, current_path_, FileMode::Read);
    auto read_res = co_await current_file.read(current_buffer, 0);

    if (read_res <= 0) {
        co_return Manifest{};
    }
    std::string manifest_filename(reinterpret_cast<char*>(current_buffer.data()), read_res);

    // 2. 异步读取MANIFEST文件
    std::string manifest_path = std::string(path_) + "/" + manifest_filename;
    File manifest_file(*ring_, manifest_path, FileMode::Read);
    std::vector<std::byte> manifest_buffer(1024 * 1024);  // 1MB buffer
    read_res = co_await manifest_file.read(manifest_buffer, 0);
    if (read_res <= 0) {
        co_return std::unexpected("Failed to read MANIFEST file");
    }
    // 3. 反序列化
    std::string content(reinterpret_cast<char*>(manifest_buffer.data()), read_res);
    std::stringstream ss(content);
    Manifest manifest;
    if (auto res = manifest.deserialize(ss); !res) {
        co_return std::unexpected("Failed to deserialize MANIFEST: " + res.error());
    }
    co_return manifest;
}

auto ManifestFile::async_store(const Manifest& manifest)
    -> kvdb::core::coro::Task<std::expected<void, std::string>> {
    if (!ring_) {
        co_return std::unexpected("IOUring not initialized for async_store");
    }
    if (!std::filesystem::exists(path_)) {
        std::filesystem::create_directories(path_);
    }
    // 1. 序列化到内存缓冲区
    std::stringstream ss;
    if (auto res = manifest.serialize(ss); !res) {
        co_return std::unexpected("Failed to serialize manifest: " + res.error());
    }
    std::string content = ss.str();
    std::vector<std::byte> buffer(content.size());
    std::memcpy(buffer.data(), content.data(), content.size());

    // 2. 异步写入临时文件
    std::string new_manifest_filename = get_new_manifest_filename();
    std::string new_manifest_path = std::string(path_) + "/" + new_manifest_filename;
    std::string temp_path = new_manifest_path + ".tmp";

    File temp_file(*ring_, temp_path, FileMode::Write);
    auto write_res = co_await temp_file.write(buffer, 0);
    if (write_res < 0) {
        co_return std::unexpected("Failed to write to temporary MANIFEST file");
    }
    // 3. 重命名
    if (std::rename(temp_path.c_str(), new_manifest_path.c_str()) != 0) {
        co_return std::unexpected("Failed to rename temporary MANIFEST file.");
    }

    // 4. 异步更新CURRENT文件
    std::vector<std::byte> current_buffer(new_manifest_filename.size());
    std::memcpy(current_buffer.data(), new_manifest_filename.data(), new_manifest_filename.size());
    File current_file(*ring_, current_path_, FileMode::Write);
    write_res = co_await current_file.write(current_buffer, 0);
    if (write_res < 0) {
        co_return std::unexpected("Failed to write to CURRENT file");
    }
    co_return {};
}

std::string ManifestFile::get_new_manifest_filename() {
    int max_num = 0;
    if (std::filesystem::exists(path_)) {
        for (const auto& entry : std::filesystem::directory_iterator(path_)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.starts_with("MANIFEST-")) {
                    int num = std::stoi(filename.substr(9));
                    max_num = std::max(num, max_num);
                }
            }
        }
    }
    return std::format("MANIFEST-{:06d}", max_num + 1);
}

}  // namespace kvdb::core::database
