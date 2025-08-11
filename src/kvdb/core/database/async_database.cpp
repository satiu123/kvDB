module kvdb.core;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.database.manifest;
import kvdb.core.coro.task;
import kvdb.core.database.async_manifest;
import kvdb.storage.wal.async_wal;
import kvdb.storage.wal.wal_record;
import kvdb.logging.log;
import kvdb.storage.sstable;

using kvdb::core::coro::Task;
using kvdb::logging::LOG_DEBUG;
using kvdb::logging::LOG_ERROR;
using kvdb::logging::LOG_INFO;
using kvdb::storage::WalOpType;

namespace kvdb::core {

// Helper to extract number from sstable filename like "sstable-000001.sst"
int get_sstable_number(const std::string& filename) {
    auto first = filename.find_first_of("-");
    auto last = filename.find_last_of(".");
    if (first == std::string::npos || last == std::string::npos) {
        return 0;
    }
    std::string number_str = filename.substr(first + 1, last - first - 1);
    return std::stoi(number_str);
}

AsyncDatabase::AsyncDatabase(std::string_view base_path)
    : ring_(std::make_unique<io::IOUring>(1024)),
      wal_(std::make_unique<storage::AsyncWal>(*ring_, base_path)),
      manifest_(std::make_unique<database::AsyncManifestFile>(*ring_, base_path)) {
    std::filesystem::path path(base_path);
    sstables_path_ = path / "sstables";
    std::filesystem::create_directories(sstables_path_);
}

auto AsyncDatabase::init() -> kvdb::core::coro::Task<void> {
    // 1. 异步加载 Manifest
    auto manifest_data_opt = co_await manifest_->async_load();
    if (manifest_data_opt) {
        manifest_data_ = std::move(manifest_data_opt.value());
    }

    // 2. 加载SSTables
    for (const auto& [level, files] : manifest_data_.sstables) {
        for (const auto& file_path : files) {
            auto sstable = std::make_unique<storage::SSTable>(*ring_);
            if (co_await sstable->open(file_path)) {
                sstables_.push_back(std::move(sstable));
            } else {
                LOG_ERROR()("Failed to open SSTable: {}", file_path);
            }
        }
    }
    // Sort sstables by number to ensure correct search order (newest first)
    std::ranges::sort(sstables_, [](const auto& a, const auto& b) {
        return get_sstable_number(a->getPath()) > get_sstable_number(b->getPath());
    });

    std::uint64_t max_seq{manifest_data_.last_wal_sequence_number};
    // 3. 定义 WAL 重放逻辑
    auto replay_handler = [this, &max_seq](const storage::WalRecord& record) {
        switch (record.getOpType()) {
            case WalOpType::PUT:
                memtable_[std::string(record.getKey())] = record.getValue();
                break;
            case WalOpType::REMOVE:
                memtable_[std::string(record.getKey())] = "";
                break;
            case WalOpType::CLEAR:
                memtable_.clear();
                break;
        }
        max_seq = std::max(max_seq, record.getSequenceNumber());
        return true;
    };

    // 4. 异步重放 WAL
    bool replay_ok = co_await wal_->async_replay(replay_handler);
    if (!replay_ok) {
        LOG_ERROR()("WAL replay failed. Database might be in an inconsistent state.");
        throw std::runtime_error("Failed to initialize database from WAL.");
    }

    // 5. 更新数据库的序列号
    wal_->setCurrentSequenceNumber(max_seq);
}

auto AsyncDatabase::async_put(std::string_view key, std::string_view value)
    -> kvdb::core::coro::Task<bool> {
    bool wal_ok = co_await wal_->async_append_put(key, value);
    if (!wal_ok) {
        LOG_ERROR()("Failed to write PUT operation to WAL for key: {}", key);
        co_return false;
    }

    memtable_[std::string(key)] = value;
    LOG_DEBUG()("Put key: {}, value: {}", key, value);
    if (memtable_.size() >= flush_threshold_) {  // 当memtable大小达到阈值时
        co_await flush_memtable_to_sstable();
    }

    co_return true;
}

auto AsyncDatabase::async_get(std::string_view key)
    -> kvdb::core::coro::Task<std::optional<std::string>> {
    if (auto it = memtable_.find(std::string(key)); it != memtable_.end()) {
        LOG_DEBUG()("Found in memtable: key: {}, value: {}", key, it->second);
        co_return it->second;
    }

    if (immutable_memtable_) {
        if (auto it = immutable_memtable_->find(std::string(key));
            it != immutable_memtable_->end()) {
            LOG_DEBUG()("Found in immutable memtable: key: {}, value: {}", key, it->second);
            co_return it->second;
        }
    }

    for (const auto& sstable : sstables_) {
        auto val = co_await sstable->find(key);
        if (val) {
            LOG_DEBUG()("Found in SSTable: key: {}, value: {}", key, *val);
            co_return val;
        }
    }
    LOG_DEBUG()("Key not found: {}", key);
    co_return std::nullopt;
}


auto AsyncDatabase::async_remove(std::string_view key) -> kvdb::core::coro::Task<bool> {
    auto exists = co_await async_get(key);
    if (!exists) {
        LOG_DEBUG()("Key not found for removal: {}", key);
        co_return false;  // 如果键不存在，直接返回false
    }
    bool wal_ok = co_await wal_->async_append_remove(key);
    if (!wal_ok) {
        LOG_ERROR()("Failed to write REMOVE operation to WAL for key: {}", key);
        co_return false;
    }
    memtable_[std::string(key)] = "";  // 标记为删除
    LOG_DEBUG()("Removed key: {}", key);
    if (memtable_.size() >= flush_threshold_) {  // 当memtable大小达到阈值时
        co_await flush_memtable_to_sstable();
    }
    co_return true;
}

Task<void> AsyncDatabase::flush_memtable_to_sstable() {
    immutable_memtable_ = std::make_unique<std::map<std::string, std::string, std::less<>>>();
    std::swap(memtable_, *immutable_memtable_);

    // 1. Generate new sstable filename
    int max_sstable_num = 0;
    if (!sstables_.empty()) {
        max_sstable_num = get_sstable_number(sstables_.front()->getPath());
    }
    std::filesystem::path sstable_path = sstables_path_;
    sstable_path /= std::format("sstable-{:06d}.sst", max_sstable_num + 1);

    // 2. Build SSTable from immutable memtable
    bool build_ok =
        co_await storage::SSTable::buildFrom(*ring_, sstable_path.string(), *immutable_memtable_);

    if (build_ok) {
        // 3. Add new sstable to manifest and sstable list
        auto new_sstable = std::make_unique<storage::SSTable>(*ring_);
        co_await new_sstable->open(sstable_path.string());
        sstables_.insert(sstables_.begin(), std::move(new_sstable));  // Add to front (newest)

        manifest_data_.sstables[0].push_back(sstable_path.string());
        manifest_data_.last_wal_sequence_number = wal_->getLastSequenceNumber();
        auto store_result = co_await manifest_->async_store(manifest_data_);
        if (!store_result) {
            LOG_ERROR()("Failed to save manifest: {}", store_result.error());
        }
    } else {
        LOG_ERROR()("Failed to build SSTable: {}", sstable_path.string());
    }

    immutable_memtable_.reset();
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

Task<void> AsyncDatabase::printSSTables() const {
    std::cout << "--- SSTables Content ---" << std::endl;
    for (const auto& sstable : sstables_) {
        auto sstable_map = co_await sstable->readAll();
        std::println("SSTable：{}", sstable->getPath());
        for (const auto& [k, v] : sstable_map) {
            std::println("  {}：{}", k, v);
        }
    }
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
