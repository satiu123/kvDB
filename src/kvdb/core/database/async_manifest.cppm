export module kvdb.core.database.async_manifest;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.io.file;
import kvdb.core.coro.task;
import kvdb.core.database.manifest;

using kvdb::core::coro::Task;
using kvdb::core::io::File;
using kvdb::core::io::IOUring;

export namespace kvdb::core::database {

/**
 * @brief 异步模式的 Manifest 文件处理器
 */
class AsyncManifestFile {
  public:
    explicit AsyncManifestFile(IOUring& ring, const std::filesystem::path& db_path);

    // 异步加载 Manifest
    auto async_load() -> Task<std::expected<Manifest, std::string>>;

    // 异步保存 Manifest
    auto async_store(const Manifest& manifest)
        -> kvdb::core::coro::Task<std::expected<void, std::string>>;
    std::string get_new_manifest_filename();

  private:
    IOUring* ring_;
    std::filesystem::path manifest_path_;
    std::filesystem::path current_path_;
    File manifest_file_;
};

}  // namespace kvdb::core::database
