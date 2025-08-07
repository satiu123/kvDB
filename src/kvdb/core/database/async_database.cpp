module kvdb.core;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.database.manifest;
import kvdb.core.coro.task;

using kvdb::core::coro::Task;
using kvdb::core::database::ManifestFile;

namespace kvdb::core {

AsyncDatabase::AsyncDatabase(std::string_view base_path) {
    // 初始化IOUring
    static kvdb::core::io::IOUring ring(1024);

    // 初始化manifest
    manifest_ = std::make_unique<database::ManifestFile>(ring, base_path);
}

kvdb::core::coro::Task<void> AsyncDatabase::open() {
    auto manifest_data = co_await manifest_->async_load();
    if (manifest_data) {
        manifest_data_ = std::move(manifest_data.value());
    }
}

kvdb::core::coro::Task<bool> AsyncDatabase::put(std::string_view, std::string_view) {
    co_return true;
}

kvdb::core::coro::Task<std::optional<std::string>> AsyncDatabase::get(std::string_view) {
    co_return std::nullopt;
}

kvdb::core::coro::Task<bool> AsyncDatabase::remove(std::string_view) {
    co_return true;
}

}  // namespace kvdb::core
