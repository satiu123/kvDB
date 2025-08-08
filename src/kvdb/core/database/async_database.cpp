module kvdb.core;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.database.manifest;
import kvdb.core.coro.task;
import kvdb.core.database.async_manifest;
import kvdb.storage.wal.async_wal;
import kvdb.storage.wal.wal_record;
import kvdb.logging.log;

using kvdb::core::coro::Task;
using kvdb::logging::LOG_ERROR;
using kvdb::storage::WalOpType;

namespace kvdb::core {

AsyncDatabase::AsyncDatabase(std::string_view base_path)
    : ring_(std::make_unique<io::IOUring>(1024)),
      wal_(std::make_unique<storage::AsyncWal>(*ring_, base_path)),
      manifest_(std::make_unique<database::AsyncManifestFile>(*ring_, base_path)) {}

auto AsyncDatabase::init() -> kvdb::core::coro::Task<void> {
    // 1. 异步加载 Manifest
    auto manifest_data_opt = co_await manifest_->async_load();
    if (manifest_data_opt) {
        manifest_data_ = std::move(manifest_data_opt.value());
    }
    std::uint64_t max_seq{manifest_data_.last_wal_sequence_number};
    // 2. 定义 WAL 重放逻辑
    auto replay_handler = [this, &max_seq](const storage::WalRecord& record) {
        switch (record.getOpType()) {
            case WalOpType::PUT:
                memtable_[std::string(record.getKey())] = std::string(record.getValue());
                break;
            case WalOpType::REMOVE:
                memtable_.erase(std::string(record.getKey()));
                break;
            case WalOpType::CLEAR:
                memtable_.clear();
                break;
        }
        // 更新序列号
        max_seq = std::max(max_seq, record.getSequenceNumber());
        return true;  // 表示处理成功，继续重放
    };

    // 3. 异步重放 WAL
    bool replay_ok = co_await wal_->async_replay(replay_handler);
    if (!replay_ok) {
        // 如果重放失败，可能需要处理错误，例如抛出异常或标记数据库为只读
        LOG_ERROR()("WAL replay failed. Database might be in an inconsistent state.");
        // 此处简单地抛出异常
        throw std::runtime_error("Failed to initialize database from WAL.");
    }

    // 4. 更新数据库的序列号
    wal_->setCurrentSequenceNumber(max_seq);
}

auto AsyncDatabase::put(std::string_view key, std::string_view value)
    -> kvdb::core::coro::Task<bool> {
    // 1. 先异步写入 WAL
    bool wal_ok = co_await wal_->async_append_put(key, value);
    if (!wal_ok) {
        LOG_ERROR()("Failed to write PUT operation to WAL for key: {}", key);
        co_return false;  // WAL 写入失败，操作失败
    }

    // 2. WAL 写入成功后，再更新内存中的 MemTable
    memtable_[std::string(key)] = value;

    co_return true;
}

auto AsyncDatabase::get(std::string_view key)
    -> kvdb::core::coro::Task<std::optional<std::string>> {
    // 目前只从 memtable 查找
    if (auto it = memtable_.find(std::string(key)); it != memtable_.end()) {
        co_return it->second;
    }
    // TODO: 后续需要从 SSTable 中查找
    co_return std::nullopt;
}

auto AsyncDatabase::remove(std::string_view key) -> kvdb::core::coro::Task<bool> {
    // 1. 先异步写入 WAL
    bool wal_ok = co_await wal_->async_append_remove(key);
    if (!wal_ok) {
        LOG_ERROR()("Failed to write REMOVE operation to WAL for key: {}", key);
        co_return false;  // WAL 写入失败，操作失败
    }

    // 2. WAL 写入成功后，再从内存中的 MemTable 中删除
    auto num_erased = memtable_.erase(std::string(key));

    co_return num_erased > 0;
}
Task<void> AsyncDatabase::printWALRecords() const {
    auto records = co_await wal_->getFormattedContent();
    if (records) {
        std::cout << "--- WAL Records ---" << std::endl;
        for (const auto& record : *records) {
            std::cout << record << std::endl;
        }
    } else {
        std::cerr << "获取WAL记录失败: " << records.error() << std::endl;
    }
    co_return;
}

// void AsyncDatabase::printSSTables() const {
//     std::cout << "--- SSTables Content ---" << std::endl;
//     for (const auto& sstable : sstables_) {
//         std::cout << "SSTable: " << sstable->getPath() << std::endl;
//         auto all_data = sstable->readAll();
//         for (const auto& [key, value] : all_data) {
//             std::cout << "  " << key << ": " << value << std::endl;
//         }
//     }
// }
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