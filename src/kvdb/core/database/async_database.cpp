module kvdb.core;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.database.manifest;
import kvdb.core.coro.task;

using kvdb::core::coro::Task;
using kvdb::core::database::ManifestFile;


namespace kvdb::core {

AsyncDatabase::AsyncDatabase(std::string_view base_path)
    : ring_(std::make_unique<io::IOUring>(1024)) {  // 内部创建 IOUring
    // 初始化manifest
    manifest_ = std::make_unique<database::ManifestFile>(*ring_, base_path);
}

auto AsyncDatabase::open() -> kvdb::core::coro::Task<void> {
    auto manifest_data = co_await manifest_->async_load();
    if (manifest_data) {
        manifest_data_ = std::move(manifest_data.value());
    }
}

auto AsyncDatabase::put(std::string_view, std::string_view) -> kvdb::core::coro::Task<bool> {
    co_return true;
}

auto AsyncDatabase::get(std::string_view) -> kvdb::core::coro::Task<std::optional<std::string>> {
    co_return std::nullopt;
}

auto AsyncDatabase::remove(std::string_view) -> kvdb::core::coro::Task<bool> {
    co_return true;
}
void AsyncDatabase::printManifest() const {
    std::cout << "--- Manifest Content ---" << std::endl;
    std::cout << "Last WAL Sequence Number: " << manifest_data_.last_wal_sequence_number
              << std::endl;
    std::cout << "SSTables:" << std::endl;
    for (const auto& [level, files] : manifest_data_.sstables) {
        std::cout << "  Level " << level << ":" << std::endl;
        for (const auto& file : files) {
            std::cout << "    " << file << std::endl;
        }
    }
}
}  // namespace kvdb::core
