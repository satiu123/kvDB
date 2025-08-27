export module kvdb.core.database.async_manifest;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.io.file;
import kvdb.core.coro.task;
import kvdb.core.database.manifest;
import kvdb.core.types;

using kvdb::core::coro::Task;
using kvdb::core::io::File;
using kvdb::core::io::IOUring;
using kvdb::core::types::Result;

export namespace kvdb::core::database {

/**
 * @brief 异步模式的 Manifest 文件处理器
 */
class AsyncManifestFile {
  public:
    explicit AsyncManifestFile(IOUring& ring, const std::filesystem::path& db_path);

    // 异步加载 Manifest
    auto async_load() -> Task<Result<Manifest>>;

    // 异步保存 Manifest
    auto async_store(const Manifest& manifest) -> Task<Result<void>>;
    std::string get_new_manifest_filename();

  private:
    IOUring* ring_;
    std::filesystem::path manifest_path_;
    std::filesystem::path current_path_;
    File manifest_file_;
};

}  // namespace kvdb::core::database
