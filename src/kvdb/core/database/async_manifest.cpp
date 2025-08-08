module kvdb.core.database.async_manifest;

import std;
import kvdb.core.io.file;
import kvdb.core.database.manifest;
import kvdb.logging.log;

using kvdb::core::coro::Task;
using kvdb::core::io::FileMode;
using kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO;

namespace kvdb::core::database {

AsyncManifestFile::AsyncManifestFile(IOUring& ring, const std::filesystem::path& db_path)
    : ring_(&ring),
      manifest_path_(db_path / "manifest"),
      current_path_(manifest_path_ / "CURRENT"),
      manifest_file_(ring, manifest_path_.string(), FileMode::ReadWrite) {
    if (!std::filesystem::exists(manifest_path_)) {
        std::filesystem::create_directories(manifest_path_);
    }
    LOG_INFO()("AsyncManifestFile initialized for path: {}", manifest_path_.string());
}

auto AsyncManifestFile::async_load() -> Task<std::expected<Manifest, std::string>> {
    if (!ring_) {
        co_return std::unexpected("IOUring not initialized for async_load");
    }
    if (!std::filesystem::exists(current_path_)) {
        co_return Manifest{};
    }
    // 1. 异步读取CURRENT文件
    File current_file(*ring_, current_path_, FileMode::Read);
    std::vector<std::byte> current_buffer(current_file.get_size());
    auto read_res = co_await current_file.read(current_buffer, 0);

    if (read_res <= 0) {
        co_return Manifest{};
    }
    std::string manifest_filename(reinterpret_cast<char*>(current_buffer.data()), read_res);

    // 2. 异步读取MANIFEST文件
    std::string manifest_path = manifest_path_ / manifest_filename;
    File manifest_file(*ring_, manifest_path, FileMode::Read);
    std::vector<std::byte> manifest_buffer(manifest_file.get_size());
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

auto AsyncManifestFile::async_store(const Manifest& manifest)
    -> kvdb::core::coro::Task<std::expected<void, std::string>> {
    if (!ring_) {
        co_return std::unexpected("IOUring not initialized for async_store");
    }
    if (!std::filesystem::exists(manifest_path_)) {
        std::filesystem::create_directories(manifest_path_);
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
    std::string new_manifest_path = manifest_path_ / new_manifest_filename;
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
std::string AsyncManifestFile::get_new_manifest_filename() {
    int max_num = 0;
    if (std::filesystem::exists(manifest_path_)) {
        for (const auto& entry : std::filesystem::directory_iterator(manifest_path_)) {
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
