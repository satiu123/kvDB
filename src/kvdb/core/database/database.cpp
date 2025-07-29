module kvdb.core;

import std;
import kvdb.logging.log;
import kvdb.storage;

using kvdb::logging::LOG_DEBUG, kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO,
    kvdb::logging::LOG_WARNING;

namespace kvdb::core {
Database::Database(std::string_view wal_path, std::string_view snapshot_path)
    : wal_(wal_path),
      snapshot_(snapshot_path),
      recovery_manager_(wal_, snapshot_),
      snapshot_manager_(wal_, snapshot_) {
    // 使用恢复管理器恢复数据
    if (!recovery_manager_.recover(data_)) {
        LOG_ERROR()("数据恢复失败");
    }

    // Load existing SSTables
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.is_regular_file() && entry.path().extension() == ".db" &&
            entry.path().filename().string().starts_with("sstable_")) {
            auto sstable = std::make_unique<storage::SSTable>();
            if (sstable->open(entry.path().string())) {
                sstables_.push_back(std::move(sstable));
            }
        }
    }
    // Sort SSTables by name (newest first)
    std::ranges::sort(sstables_,
                      [](const auto& a, const auto& b) { return a->getPath() > b->getPath(); });
}

Database::~Database() {
    wal_.sync();   // 确保所有数据都已同步到磁盘
    wal_.close();  // 确保在析构时关闭WAL文件
}

bool Database::put(std::string_view key, std::string_view value) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. 先写WAL
    if (!wal_.appendPut(key, value)) {
        LOG_ERROR()("写入WAL失败: PUT key={}", key);
        return false;
    }

    // 2. 再修改内存数据
    data_[key.data()] = value.data();
    LOG_DEBUG()("PUT操作成功: key={}, value={}", key, value);

    // 3. 记录操作并检查是否需要自动快照
    snapshot_manager_.recordOperation();
    snapshot_manager_.checkAutoSnapshot(data_);

    if (data_.size() >= memtable_flush_threshold_) {
        LOG_INFO()("MemTable is full. Freezing and creating a new one.");
        immutable_memtable_ =
            std::make_unique<std::map<std::string, std::string>>(std::move(data_));
        data_.clear();

        // Flush the immutable memtable to a new SSTable
        std::string sstable_path = "sstable_" + std::to_string(sstable_counter_++) + ".db";
        if (storage::SSTable::buildFrom(sstable_path, *immutable_memtable_)) {
            LOG_INFO()("Successfully flushed memtable to {}", sstable_path);
            immutable_memtable_.reset();  // Clear the immutable memtable after successful flush
            // Add the new SSTable to the list
            auto sstable = std::make_unique<storage::SSTable>();
            if (sstable->open(sstable_path)) {
                sstables_.insert(sstables_.begin(),
                                 std::move(sstable));  // Insert at the beginning (newest)
            }
        } else {
            LOG_ERROR()("Failed to flush memtable to {}", sstable_path);
            // Here you might want to handle the failure, e.g., retry or halt.
        }
    }

    return true;
}

std::optional<std::string> Database::get(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key.data());
    if (it != data_.end()) {
        // Check for tombstone
        return it->second.empty() ? std::nullopt : std::optional(it->second);
    }

    if (immutable_memtable_) {
        auto im_it = immutable_memtable_->find(key.data());
        if (im_it != immutable_memtable_->end()) {
            return im_it->second.empty() ? std::nullopt : std::optional(im_it->second);
        }
    }

    // Search in SSTables (from newest to oldest)
    for (const auto& sstable : sstables_) {
        auto value = sstable->find(key);
        if (value) {
            return value->empty() ? std::nullopt : value;
        }
    }

    return std::nullopt;
}

bool Database::remove(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);

    // In a real LSM-Tree, remove should add a tombstone record.
    if (!exists(key)) {
        return false;
    }

    // 1. 先写WAL
    if (!wal_.appendRemove(key)) {
        LOG_ERROR()("写入WAL失败: REMOVE key={}", key);
        return false;
    }

    // 2. 再修改内存数据 (add tombstone)
    data_[std::string(key)] = "";  // Empty value as tombstone
    LOG_DEBUG()("REMOVE操作成功 (tombstone): key={}", key);

    return true;
}

std::size_t Database::size() const {
    return keys().size();
}

void Database::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. 先写WAL
    if (!wal_.appendClear()) {
        LOG_ERROR()("写入WAL失败: CLEAR");
        return;
    }

    // 2. 再修改内存数据
    data_.clear();
    immutable_memtable_.reset();
    sstables_.clear();
    // Also remove all sstable files
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.is_regular_file() && entry.path().extension() == ".db" &&
            entry.path().filename().string().starts_with("sstable_")) {
            std::filesystem::remove(entry.path());
        }
    }
    LOG_DEBUG()("CLEAR操作成功");
}

bool Database::exists(std::string_view key) const {
    auto value = get(key);
    return value.has_value();
}

std::vector<std::string> Database::keys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, std::string> all_data;

    // Oldest to newest
    for (const auto& sstable : std::ranges::reverse_view(sstables_)) {
        auto sstable_data = sstable->readAll();
        for (const auto& [key, value] : sstable_data) {
            all_data[key] = value;
        }
    }
    if (immutable_memtable_) {
        for (const auto& [key, value] : *immutable_memtable_) {
            all_data[key] = value;
        }
    }
    for (const auto& [key, value] : data_) {
        all_data[key] = value;
    }

    std::vector<std::string> keys;
    for (const auto& [key, value] : all_data) {
        if (!value.empty()) {  // Exclude tombstones
            keys.push_back(key);
        }
    }
    return keys;
}

// 创建快照
bool Database::createSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_manager_.createSnapshot(data_);
}

// 设置快照配置
void Database::setSnapshotConfig(const storage::SnapshotConfig&& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_manager_.setConfig(config);
}

// 获取快照配置
const storage::SnapshotConfig& Database::getSnapshotConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_manager_.getConfig();
}

// 检查是否存在快照文件
bool Database::hasSnapshot() const {
    return snapshot_manager_.hasSnapshot();
}

void Database::setMemtableFlushThreshold(std::size_t threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    memtable_flush_threshold_ = threshold;
}

void Database::compact() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO()("Starting compaction...");

    if (sstables_.size() <= 1) {
        LOG_INFO()("Not enough SSTables to compact.");
        return;
    }

    std::map<std::string, std::string> all_data;
    // Iterate from oldest to newest to ensure newer values overwrite older ones
    for (const auto& sstable : std::ranges::reverse_view(sstables_)) {
        auto sstable_data = sstable->readAll();
        for (const auto& [key, value] : sstable_data) {
            all_data[key] = value;  // Overwrite with newer value
        }
    }

    std::string new_sstable_path =
        "sstable_compacted_" + std::to_string(sstable_counter_++) + ".db";
    if (storage::SSTable::buildFrom(new_sstable_path, all_data)) {
        LOG_INFO()("Compaction successful. New SSTable: {}", new_sstable_path);

        std::vector<std::string> old_paths;
        for (const auto& sstable : sstables_) {
            old_paths.push_back(sstable->getPath());
        }

        sstables_.clear();

        for (const auto& path : old_paths) {
            std::filesystem::remove(path);
        }

        // Load the new compacted SSTable
        auto new_sstable = std::make_unique<storage::SSTable>();
        if (new_sstable->open(new_sstable_path)) {
            sstables_.push_back(std::move(new_sstable));
        }

    } else {
        LOG_ERROR()("Compaction failed.");
    }
}

}  // namespace kvdb::core
