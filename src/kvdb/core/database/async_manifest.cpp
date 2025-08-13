module kvdb.core.database.async_manifest;

import std;
import kvdb.core.io.file;
import kvdb.core.database.manifest;
import kvdb.logging.log;
import kvdb.core.types;

using kvdb::core::coro::Task;
using kvdb::core::io::FileMode;
using kvdb::core::types::Result;
using kvdb::logging::LOG_DEBUG, kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO,
    kvdb::logging::LOG_WARNING;

namespace kvdb::core::database {

AsyncManifestFile::AsyncManifestFile(IOUring& ring, const std::filesystem::path& db_path)
    : ring_(&ring),
      manifest_path_(db_path / "manifest"),
      current_path_(manifest_path_ / "CURRENT"),
      manifest_file_(ring, manifest_path_.string(), FileMode::ReadWrite) {
    if (!std::filesystem::exists(manifest_path_)) {
        std::filesystem::create_directories(manifest_path_);
    }
    LOG_INFO()("异步Manifest文件处理器已为路径'{}'初始化", manifest_path_.string());
}

auto AsyncManifestFile::async_load() -> Task<Result<Manifest>> {
    if (!ring_) {
        LOG_ERROR()("IOUring尚未初始化，无法进行异步加载");
        co_return std::unexpected("IOUring尚未初始化");
    }
    if (!std::filesystem::exists(current_path_)) {
        LOG_WARNING()("CURRENT文件不存在，将返回一个空的Manifest");
        co_return Manifest{};
    }
    // 1. 异步读取CURRENT文件
    LOG_DEBUG()("正在异步读取CURRENT文件: {}", current_path_.string());
    File current_file(*ring_, current_path_.string(), FileMode::Read);
    std::vector<std::byte> current_buffer(current_file.get_size());
    auto read_res = co_await current_file.read(current_buffer, 0);

    if (read_res <= 0) {
        LOG_WARNING()("读取CURRENT文件失败或文件为空");
        co_return Manifest{};
    }
    std::string manifest_filename(reinterpret_cast<char*>(current_buffer.data()), read_res);
    LOG_DEBUG()("从CURRENT文件读到Manifest文件名: {}", manifest_filename);

    // 2. 异步读取MANIFEST文件
    std::string manifest_path_str = (manifest_path_ / manifest_filename).string();
    LOG_DEBUG()("正在异步读取MANIFEST文件: {}", manifest_path_str);
    File manifest_file(*ring_, manifest_path_str, FileMode::Read);
    std::vector<std::byte> manifest_buffer(manifest_file.get_size());
    read_res = co_await manifest_file.read(manifest_buffer, 0);
    if (read_res <= 0) {
        LOG_ERROR()("读取MANIFEST文件 '{}' 失败", manifest_path_str);
        co_return std::unexpected("读取MANIFEST文件失败");
    }
    // 3. 反序列化
    LOG_DEBUG()("正在反序列化MANIFEST内容");
    std::string content(reinterpret_cast<char*>(manifest_buffer.data()), read_res);
    std::stringstream ss(content);
    Manifest manifest;
    if (auto res = manifest.deserialize(ss); !res) {
        LOG_ERROR()("反序列化MANIFEST失败: {}", res.error());
        co_return std::unexpected("反序列化MANIFEST失败: " + res.error());
    }
    LOG_INFO()("成功加载并反序列化MANIFEST文件: {}", manifest_filename);
    co_return manifest;
}

auto AsyncManifestFile::async_store(const Manifest& manifest) -> Task<Result<void>> {
    if (!ring_) {
        LOG_ERROR()("IOUring尚未初始化，无法进行异步存储");
        co_return std::unexpected("IOUring尚未初始化");
    }
    if (!std::filesystem::exists(manifest_path_)) {
        std::filesystem::create_directories(manifest_path_);
    }
    // 1. 序列化到内存缓冲区
    LOG_DEBUG()("正在序列化Manifest到内存缓冲区");
    std::stringstream ss;
    if (auto res = manifest.serialize(ss); !res) {
        LOG_ERROR()("序列化Manifest失败: {}", res.error());
        co_return std::unexpected("序列化Manifest失败: " + res.error());
    }
    std::string content = ss.str();
    std::vector<std::byte> buffer(content.size());
    std::memcpy(buffer.data(), content.data(), content.size());

    // 2. 异步写入临时文件
    std::string new_manifest_filename = get_new_manifest_filename();
    std::string new_manifest_path = manifest_path_ / new_manifest_filename;
    std::string temp_path = new_manifest_path + ".tmp";
    LOG_DEBUG()("正在异步写入临时Manifest文件: {}", temp_path);

    File temp_file(*ring_, temp_path, FileMode::Write);
    auto write_res = co_await temp_file.write(buffer, 0);
    if (write_res < 0) {
        LOG_ERROR()("写入临时Manifest文件 '{}' 失败", temp_path);
        co_return std::unexpected("写入临时MANIFEST文件失败");
    }
    // 3. 重命名
    LOG_DEBUG()("正在将'{}'重命名为'{}'", temp_path, new_manifest_path);
    if (std::rename(temp_path.c_str(), new_manifest_path.c_str()) != 0) {
        LOG_ERROR()("重命名临时Manifest文件失败");
        co_return std::unexpected("重命名临时MANIFEST文件失败");
    }

    // 4. 异步更新CURRENT文件
    LOG_DEBUG()("正在异步更新CURRENT文件，指向: {}", new_manifest_filename);
    std::vector<std::byte> current_buffer(new_manifest_filename.size());
    std::memcpy(current_buffer.data(), new_manifest_filename.data(), new_manifest_filename.size());
    File current_file(*ring_, current_path_.string(), FileMode::Write);
    write_res = co_await current_file.write(current_buffer, 0);
    if (write_res < 0) {
        LOG_ERROR()("写入CURRENT文件失败");
        co_return std::unexpected("写入CURRENT文件失败");
    }
    LOG_INFO()("成功存储Manifest '{}' 并更新CURRENT文件", new_manifest_filename);
    co_return {};
}
std::string AsyncManifestFile::get_new_manifest_filename() {
    int max_num = 0;
    if (std::filesystem::exists(manifest_path_)) {
        for (const auto& entry : std::filesystem::directory_iterator(manifest_path_)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.starts_with("MANIFEST-")) {
                    try {
                        int num = std::stoi(filename.substr(9));
                        max_num = std::max(num, max_num);
                    } catch (const std::invalid_argument& e) {
                        LOG_WARNING()("无法解析Manifest文件名中的数字: {}", filename);
                    } catch (const std::out_of_range& e) {
                        LOG_WARNING()("Manifest文件名中的数字超出范围: {}", filename);
                    }
                }
            }
        }
    }
    return std::format("MANIFEST-{:06d}", max_num + 1);
}
}  // namespace kvdb::core::database